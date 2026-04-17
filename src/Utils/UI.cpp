#include "Utils/UI.h"

#include "Define/Enjoyment.h"
#include "Instance/Interact.h"
#include "Instance/SceneInstance.h"
#include "Registry/Stat.h"

#include "API/PrismaUI_API.h"

#include "magic_enum/magic_enum.hpp"
#include "nlohmann/json.hpp"

static PRISMA_UI_API::IVPrismaUI2* prisma = nullptr;
static PrismaView view                    = 0;

namespace
{
nlohmann::json BuildInteractionJson(Define::ActorBody::PartName partName,
                                    const Instance::PartRuntime& partState)
{
  const auto& interaction = partState.interaction;
  nlohmann::json ij;
  ij["part"]     = std::string(magic_enum::enum_name(partName));
  ij["type"]     = std::string(magic_enum::enum_name(interaction.type));
  ij["partner"]  = interaction.partner ? interaction.partner->GetDisplayFullName() : "";
  ij["velocity"] = interaction.approachSpeed;
  return ij;
}

nlohmann::json BuildSceneSelectionJson(const Instance::SceneInstance& scene)
{
  nlohmann::json selection;
  selection["options"]     = nlohmann::json::array();
  selection["selectedPtr"] = "";

  const auto& availableScenes = scene.GetAvailableScenes();
  Define::Scene* current      = scene.GetCurrentScene();
  std::vector<Define::Scene*> orderedScenes;
  orderedScenes.reserve(availableScenes.size());
  for (const auto& [ptr, _] : availableScenes) {
    if (ptr)
      orderedScenes.push_back(ptr);
  }

  std::sort(orderedScenes.begin(), orderedScenes.end(),
            [](const Define::Scene* lhs, const Define::Scene* rhs) {
              if (lhs->GetName() != rhs->GetName())
                return lhs->GetName() < rhs->GetName();
              return lhs < rhs;
            });

  for (auto* ptr : orderedScenes) {
    if (!ptr)
      continue;

    nlohmann::json item;
    item["ptr"]  = std::to_string(reinterpret_cast<std::uintptr_t>(ptr));
    item["name"] = ptr->GetName();
    selection["options"].push_back(std::move(item));

    if (ptr == current)
      selection["selectedPtr"] = std::to_string(reinterpret_cast<std::uintptr_t>(ptr));
  }

  selection["currentName"] = current ? current->GetName() : "";
  return selection;
}
}  // namespace

// ════════════════════════════════════════════════════════════
// UI
// ════════════════════════════════════════════════════════════

UI::UI()
{
  prisma = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
  if (!prisma) {
    logger::error("[SexLab NG] Failed to acquire Prisma UI API.");
    return;
  }
  logger::info("[SexLab NG] Prisma UI API v2 acquired.");

  CreateView();
}

UI::~UI()
{
  if (prisma) {
    DestroyView();
    prisma = nullptr;
  }
}

void UI::CreateView()
{
  if (!prisma)
    return;

  view = prisma->CreateView("SexLabNG/hud.html", [](PrismaView v) {
    logger::info("[SexLab NG] HUD DOM ready.");
  });

  // 注册 JS → C++ 监听器: 用户拖拽 HUD 后 JS 回传位置
  prisma->RegisterJSListener(view, "onHudPositionChanged", [](const char* arg) {
    if (!arg || !*arg)
      return;

    const char* comma = std::strchr(arg, ',');
    if (!comma)
      return;

    char* end     = nullptr;
    const float x = std::strtof(arg, &end);
    if (end != comma)
      return;

    const float y = std::strtof(comma + 1, &end);
    if (end == comma + 1 || *end != '\0')
      return;

    UI::GetSingleton().OnPositionChanged(x, y);
  });

  prisma->RegisterJSListener(view, "onHudSceneSelected", [](const char* arg) {
    if (!arg || !*arg)
      return;

    char* end                         = nullptr;
    const unsigned long long selected = std::strtoull(arg, &end, 10);
    if (end == arg)
      return;
    UI::GetSingleton().OnSceneSelected(static_cast<std::uintptr_t>(selected));
  });

  prisma->Hide(view);
}

void UI::DestroyView()
{
  if (prisma && prisma->IsValid(view)) {
    prisma->Unfocus(view);
    prisma->Destroy(view);
  }
  view     = 0;
  isActive = false;
}

// ── 场景开始 ──────────────────────────────────────────────

void UI::Show(Instance::SceneInstance* scene)
{
  if (!prisma || !scene)
    return;

  if (currentScene) {
    logger::warn("[SexLab NG] HUD is already shown for another scene.");
    return;
  }

  currentScene = scene;

  isActive = true;
  prisma->Show(view);
  SendInitData(scene);
}

// ── 帧更新 ────────────────────────────────────────────────

void UI::Update(Instance::SceneInstance* scene)
{
  if (!isActive || !prisma || scene != currentScene)
    return;
  SendUpdateData(scene);
}

// ── 场景结束 ──────────────────────────────────────────────

void UI::Hide(Instance::SceneInstance* scene)
{
  if (!isActive || scene != currentScene)
    return;
  isActive     = false;
  currentScene = nullptr;
  if (prisma && prisma->IsValid(view)) {
    prisma->Unfocus(view);
    prisma->Hide(view);
  }
}

// ── Alt 键焦点控制 ────────────────────────────────────────

void UI::SetFocus(bool focused)
{
  if (!isActive || !prisma)
    return;
  if (focused && !isFocused) {
    prisma->Focus(view, false, false);
    isFocused = true;
  } else if (!focused && isFocused) {
    prisma->Unfocus(view);
    isFocused = false;
  }
}

bool UI::HasFocus() const
{
  return isFocused;
}

void UI::OnPositionChanged(float x, float y)
{
  posX = x;
  posY = y;
}

void UI::OnSceneSelected(std::uintptr_t scenePtr)
{
  if (!currentScene)
    return;

  auto* scene = reinterpret_cast<Define::Scene*>(scenePtr);
  if (!currentScene->SetScene(scene)) {
    logger::warn("[SexLab NG] UI: failed to switch scene ptr={}", scenePtr);
    return;
  }

  SendInitData(currentScene);
}

// ── JSON 构建 ─────────────────────────────────────────────

void UI::SendInitData(Instance::SceneInstance* scene)
{
  nlohmann::json j;
  j["actors"]         = nlohmann::json::array();
  j["sceneSelection"] = BuildSceneSelectionJson(*scene);

  const auto& actors   = scene->GetActors();
  const auto& interact = scene->GetInteract();

  for (std::size_t i = 0; i < actors.size(); ++i) {
    auto* actor = actors[i];
    if (!actor)
      continue;

    const auto& stat = Registry::ActorStat::GetSingleton().GetStat(actor);

    nlohmann::json aj;
    aj["index"]  = i;
    aj["name"]   = actor->GetDisplayFullName();
    aj["enjoy"]  = stat.enjoy.GetValue();
    aj["degree"] = std::string(magic_enum::enum_name(stat.enjoy.GetDegree()));

    // 初始交互列表（可能全为空）
    aj["interactions"]    = nlohmann::json::array();
    const auto* actorData = interact.GetActorState(actor);
    if (!actorData)
      continue;

    for (const auto& [partName, partState] : actorData->parts) {
      if (!partState.interaction.active)
        continue;
      aj["interactions"].push_back(BuildInteractionJson(partName, partState));
    }

    j["actors"].push_back(aj);
  }

  nlohmann::json pos;
  pos["x"]      = posX;
  pos["y"]      = posY;
  j["position"] = pos;

  prisma->InteropCall(view, "initHud", j.dump().c_str());
}

void UI::SendUpdateData(Instance::SceneInstance* scene)
{
  nlohmann::json j;
  j["actors"]         = nlohmann::json::array();
  j["sceneSelection"] = BuildSceneSelectionJson(*scene);

  const auto& actors   = scene->GetActors();
  const auto& interact = scene->GetInteract();

  for (std::size_t i = 0; i < actors.size(); ++i) {
    auto* actor = actors[i];
    if (!actor)
      continue;

    const auto& stat = Registry::ActorStat::GetSingleton().GetStat(actor);

    nlohmann::json aj;
    aj["index"]  = i;
    aj["enjoy"]  = stat.enjoy.GetValue();
    aj["degree"] = std::string(magic_enum::enum_name(stat.enjoy.GetDegree()));

    aj["interactions"]    = nlohmann::json::array();
    const auto* actorData = interact.GetActorState(actor);
    if (!actorData)
      continue;

    for (const auto& [partName, partState] : actorData->parts) {
      if (!partState.interaction.active)
        continue;
      aj["interactions"].push_back(BuildInteractionJson(partName, partState));
    }

    j["actors"].push_back(aj);
  }

  prisma->InteropCall(view, "updateHud", j.dump().c_str());
}
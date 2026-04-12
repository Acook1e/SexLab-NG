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
    float x = 0, y = 0;
    if (std::sscanf(arg, "%f,%f", &x, &y) == 2)
      UI::GetSingleton().OnPositionChanged(x, y);
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
    prisma->Focus(view, false, true);
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

// ── JSON 构建 ─────────────────────────────────────────────

void UI::SendInitData(Instance::SceneInstance* scene)
{
  nlohmann::json j;
  j["actors"] = nlohmann::json::array();

  const auto& actors   = scene->GetActors();
  const auto& interact = scene->GetInteract();

  for (std::size_t i = 0; i < actors.size(); ++i) {
    auto* actor = actors[i];
    if (!actor)
      continue;

    const auto& info = scene->GetActorInfo(actor);
    const auto& stat = Registry::ActorStat::GetSingleton().GetStat(actor);

    nlohmann::json aj;
    aj["index"]  = i;
    aj["name"]   = actor->GetDisplayFullName();
    aj["enjoy"]  = stat.enjoy.GetValue();
    aj["degree"] = std::string(magic_enum::enum_name(stat.enjoy.GetDegree()));

    // 初始交互列表（可能全为空）
    aj["interactions"]    = nlohmann::json::array();
    const auto& actorData = interact.GetData(actor);
    for (const auto& [partName, bpInfo] : actorData.infos) {
      if (bpInfo.type == Instance::Interact::Type::None)
        continue;
      nlohmann::json ij;
      ij["type"]     = std::string(magic_enum::enum_name(bpInfo.type));
      ij["partner"]  = bpInfo.actor ? bpInfo.actor->GetDisplayFullName() : "";
      ij["velocity"] = bpInfo.velocity;
      aj["interactions"].push_back(ij);
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
  j["actors"] = nlohmann::json::array();

  const auto& actors   = scene->GetActors();
  const auto& interact = scene->GetInteract();

  for (std::size_t i = 0; i < actors.size(); ++i) {
    auto* actor = actors[i];
    if (!actor)
      continue;

    const auto& info = scene->GetActorInfo(actor);
    const auto& stat = Registry::ActorStat::GetSingleton().GetStat(actor);

    nlohmann::json aj;
    aj["index"]  = i;
    aj["enjoy"]  = stat.enjoy.GetValue();
    aj["degree"] = std::string(magic_enum::enum_name(stat.enjoy.GetDegree()));

    aj["interactions"]    = nlohmann::json::array();
    const auto& actorData = interact.GetData(actor);
    for (const auto& [partName, bpInfo] : actorData.infos) {
      if (bpInfo.type == Instance::Interact::Type::None)
        continue;
      nlohmann::json ij;
      ij["type"]     = std::string(magic_enum::enum_name(bpInfo.type));
      ij["partner"]  = bpInfo.actor ? bpInfo.actor->GetDisplayFullName() : "";
      ij["velocity"] = bpInfo.velocity;
      aj["interactions"].push_back(ij);
    }

    j["actors"].push_back(aj);
  }

  prisma->InteropCall(view, "updateHud", j.dump().c_str());
}
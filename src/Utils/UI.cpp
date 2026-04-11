#include "Utils/UI.h"

#include "Define/Enjoyment.h"
#include "Instance/Interact.h"
#include "Instance/SceneInstance.h"

#include "API/PrismaUI_API.h"

#include "magic_enum/magic_enum.hpp"
#include "nlohmann/json.hpp"

namespace UI
{

static PRISMA_UI_API::IVPrismaUI2* prisma = nullptr;
static PrismaView view                    = 0;

void Initialize()
{
  prisma = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
  if (!prisma) {
    logger::error("[SexLab NG] Failed to acquire Prisma UI API.");
    return;
  }
  logger::info("[SexLab NG] Prisma UI API v2 acquired.");
}

// ════════════════════════════════════════════════════════════
// SceneHUD
// ════════════════════════════════════════════════════════════

void SceneHUD::CreateView()
{
  if (!prisma)
    return;

  view = prisma->CreateView("SexLabNG/hud.html", [](PrismaView v) {
    logger::info("[SexLab NG] HUD DOM ready.");
  });

  // 注册 JS → C++ 监听器: 用户拖拽 HUD 后 JS 回传位置
  prisma->RegisterJSListener(view, "onHudPositionChanged", [](const char* arg) {
    // arg = "x,y"
    float x = 0, y = 0;
    if (std::sscanf(arg, "%f,%f", &x, &y) == 2)
      SceneHUD::GetSingleton().OnPositionChanged(x, y);
  });

  prisma->Hide(view);
}

void SceneHUD::DestroyView()
{
  if (prisma && prisma->IsValid(view)) {
    prisma->Unfocus(view);
    prisma->Destroy(view);
  }
  view     = 0;
  isActive = false;
}

// ── 场景开始 ──────────────────────────────────────────────

void SceneHUD::Show(Instance::SceneInstance* scene)
{
  if (!prisma || !scene)
    return;

  if (currentScene) {
    logger::warn("[SexLab NG] HUD is already shown for another scene.");
    return;  // 已有场景在显示 HUD，避免重复 Show
  }

  // 检测 Player 是否参与
  // SceneInstance::actors 第一个是 central, 遍历检查 IsPlayerRef
  currentScene = scene;

  bool hasPlayer = false;
  // 需要 SceneInstance 暴露 actors 的查询接口, 见下方建议
  // 这里假设有 scene->HasPlayer()
  // hasPlayer = scene->HasPlayer();

  if (!hasPlayer)
    return;

  if (!prisma->IsValid(view))
    CreateView();

  isActive = true;
  prisma->Show(view);
  SendInitData(scene);
}

// ── 帧更新 ────────────────────────────────────────────────

void SceneHUD::Update(Instance::SceneInstance* scene)
{
  if (!isActive || !prisma || scene != currentScene)
    return;
  SendUpdateData(scene);
}

// ── 场景结束 ──────────────────────────────────────────────

void SceneHUD::Hide(Instance::SceneInstance* scene)
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

void SceneHUD::SetFocus(bool focused)
{
  if (!isActive || !prisma)
    return;
  if (focused && !isFocused) {
    // pauseGame=false: 不暂停游戏; disableFocusMenu=true: 不弹出 ESC 菜单
    prisma->Focus(view, false, true);
    isFocused = true;
  } else if (!focused && isFocused) {
    prisma->Unfocus(view);
    isFocused = false;
  }
}

bool SceneHUD::HasFocus() const
{
  return isFocused;
}

void SceneHUD::OnPositionChanged(float x, float y)
{
  posX = x;
  posY = y;
  // TODO: 持久化到 ini 或 cosave
}

// ── JSON 构建 ─────────────────────────────────────────────

/*
  InitData JSON 格式:
  {
    "actors": [
      {
        "index": 0,
        "name": "Lydia",
        "enjoy": 0.0,
        "degree": "Neutral",
        "interactions": []
      },
      ...
    ],
    "position": { "x": 100, "y": 200 }   // -1 表示使用默认
  }
*/

void SceneHUD::SendInitData(Instance::SceneInstance* scene)
{
  nlohmann::json j;
  j["actors"] = nlohmann::json::array();

  // 需要 SceneInstance 暴露查询接口（见下方建议）
  // 遍历 scene 的所有 actor + actorInfoMap
  // 伪代码:
  //   for (int i = 0; i < scene->GetActorCount(); ++i) {
  //     auto* actor = scene->GetActor(i);
  //     auto& info  = scene->GetActorInfo(actor);
  //     nlohmann::json aj;
  //     aj["index"]  = i;
  //     aj["name"]   = actor->GetDisplayFullName();
  //     aj["enjoy"]  = info.enjoy.GetValue();
  //     aj["degree"] = magic_enum::enum_name(info.enjoy.GetDegree());
  //     aj["interactions"] = nlohmann::json::array();
  //     j["actors"].push_back(aj);
  //   }

  nlohmann::json pos;
  pos["x"]      = posX;
  pos["y"]      = posY;
  j["position"] = pos;

  prisma->InteropCall(view, "initHud", j.dump().c_str());
}

/*
  UpdateData JSON 格式:
  {
    "actors": [
      {
        "index": 0,
        "enjoy": 35.5,
        "degree": "Pleasure",
        "interactions": [
          { "type": "Vaginal",  "partner": "Dragonborn", "velocity": -0.5 },
          { "type": "Kiss",     "partner": "Dragonborn", "velocity": 0.0 }
        ]
      },
      ...
    ]
  }
*/

void SceneHUD::SendUpdateData(Instance::SceneInstance* scene)
{
  nlohmann::json j;
  j["actors"] = nlohmann::json::array();

  // 伪代码 — 遍历每个 actor:
  //   for (int i = 0; i < scene->GetActorCount(); ++i) {
  //     auto* actor = scene->GetActor(i);
  //     auto& info  = scene->GetActorInfo(actor);
  //     nlohmann::json aj;
  //     aj["index"]  = i;
  //     aj["enjoy"]  = info.enjoy.GetValue();
  //     aj["degree"] = magic_enum::enum_name(info.enjoy.GetDegree());
  //
  //     aj["interactions"] = nlohmann::json::array();
  //     // 从 Interact 获取该 actor 所有活跃交互
  //     auto& interact = scene->GetInteract();
  //     for (uint8_t bp = 0; bp <= static_cast<uint8_t>(BodyPart::Name::Penis); ++bp) {
  //       auto& bpInfo = interact.GetInfo(actor, static_cast<BodyPart::Name>(bp));
  //       if (bpInfo.type == Interact::Type::None) continue;
  //       nlohmann::json ij;
  //       ij["type"]     = magic_enum::enum_name(bpInfo.type);
  //       ij["partner"]  = bpInfo.actor ? bpInfo.actor->GetDisplayFullName() : "";
  //       ij["velocity"] = bpInfo.velocity;
  //       aj["interactions"].push_back(ij);
  //     }
  //     j["actors"].push_back(aj);
  //   }

  prisma->InteropCall(view, "updateHud", j.dump().c_str());
}

}  // namespace UI
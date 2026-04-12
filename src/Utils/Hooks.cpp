#include "Utils/Hooks.h"

#include "Instance/Collision.h"
#include "Instance/Manager.h"
#include "Registry/Stat.h"
#include "Utils/UI.h"

namespace Hooks
{
void MainUpdate::Update()
{
  _Update();

  // 场景更新
  try {
    Instance::SceneManager::GetSingleton().UpdateScenes();
  } catch (const std::exception& e) {
    logger::error("Error in MainUpdate: {}", e.what());
  }

  // ── 游戏时间 delta 计算 ──────────────────────────────────────────
  // gameDaysPassed 单位为天
  // 以下皆为根据时间更新
  static const auto* gameDaysPassed = []() -> const RE::TESGlobal* {
    const auto cal = RE::Calendar::GetSingleton();
    return cal ? cal->gameDaysPassed : nullptr;
  }();

  if (!gameDaysPassed)
    return;

  // 以游戏小时为单位
  const float currentGameHours      = gameDaysPassed->value * 24.f;
  static float lastGameHours        = currentGameHours;
  static float accumulatedGameHours = 0.f;

  const float delta = currentGameHours - lastGameHours;
  lastGameHours     = currentGameHours;

  // 忽略时间倒流（读档、RevertToSavedGame 等）
  if (delta <= 0.f)
    return;

  // 按天更新的部分

  accumulatedGameHours += delta;

  // 按小时更新的部分
  // 每积累 1 游戏小时触发一次（约现实 1 分钟）
  if (accumulatedGameHours < 1.f)
    return;

  const float tickHours = std::floor(accumulatedGameHours);
  accumulatedGameHours -= tickHours;

  // stat 更新
  try {
    // 不在场景的 actor 的 arouse/enjoy 时间衰减
    Registry::ActorStat::GetSingleton().Update(tickHours);
  } catch (const std::exception& e) {
    logger::error("Error in MainUpdate::ActorStat::Update: {}", e.what());
  }
}

void PlayerUpdate::Update(RE::PlayerCharacter* a_this, float a_delta)
{
  _Update(a_this, a_delta);
  // Placeholder for player update logic
}

void NPCUpdate::Update(RE::Actor* a_this, float a_delta)
{
  _Update(a_this, a_delta);
  // Placeholder for NPC update logic
}

bool* CollisionEnable::IsCollisionEnabled(RE::hkpCollidableCollidableFilter* a_this, bool* a_result,
                                          const RE::hkpCollidable* a_collidableA,
                                          const RE::hkpCollidable* a_collidableB)
{
  a_result = _IsCollisionEnabled(a_this, a_result, a_collidableA, a_collidableB);

  if (!*a_result) {
    return a_result;
  }

  static auto IsBipedCollisionLayer = [](RE::COL_LAYER layer) -> bool {
    return layer == RE::COL_LAYER::kBiped || layer == RE::COL_LAYER::kBipedNoCC ||
           layer == RE::COL_LAYER::kCharController || layer == RE::COL_LAYER::kDeadBip;
  };

  static auto GetTESObjectREFR = [](const RE::hkpCollidable* collidable) -> RE::TESObjectREFR* {
    if (!collidable || collidable->ownerOffset >= 0)
      return nullptr;

    auto type = static_cast<RE::hkpWorldObject::BroadPhaseType>(collidable->broadPhaseHandle.type);
    if (type == RE::hkpWorldObject::BroadPhaseType::kEntity)
      return collidable->GetOwner<RE::hkpRigidBody>()->GetUserData();
    else if (type == RE::hkpWorldObject::BroadPhaseType::kPhantom)
      return collidable->GetOwner<RE::hkpPhantom>()->GetUserData();
    else
      return nullptr;
  };

  if (!IsBipedCollisionLayer(a_collidableA->GetCollisionLayer()) ||
      !IsBipedCollisionLayer(a_collidableB->GetCollisionLayer())) {
    return a_result;
  }

  auto* refA = GetTESObjectREFR(a_collidableA);
  auto* refB = GetTESObjectREFR(a_collidableB);

  if (!refA || !refB || refA == refB) {
    return a_result;
  }

  auto* actorA = refA->As<RE::Actor>();
  auto* actorB = refB->As<RE::Actor>();

  if (actorA && actorB)
    if (Instance::Collision::GetSingleton().HasActor(actorA) ||
        Instance::Collision::GetSingleton().HasActor(actorB))
      *a_result = false;

  return a_result;
}

void ApplyMovement::ApplyMovementDelta(RE::Actor* a_actor, float a_delta)
{
  if (a_actor && Instance::Collision::GetSingleton().HasActor(a_actor))
    if (auto* controller = a_actor->GetCharController(); controller) {
      Instance::Collision::ConfigureControllerForScene(controller);
      return;
    }

  _ApplyMovementDelta(a_actor, a_delta);
}

void InputEvent::ProcessEvent(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher,
                              RE::InputEvent* const* a_events)
{
  if (a_events && *a_events && UI::GetSingleton().IsVisible()) {
    auto event = *a_events;
    if (event->eventType == RE::INPUT_EVENT_TYPE::kButton) {
      auto buttonEvent = event->AsButtonEvent();
      // Left Alt key code
      if (buttonEvent && buttonEvent->GetIDCode() == 56) {
        if (buttonEvent->IsDown())
          UI::GetSingleton().SetFocus(true);
        else if (buttonEvent->IsUp())
          UI::GetSingleton().SetFocus(false);
      }
    }
    // constexpr RE::InputEvent* const dummy[] = {nullptr};
    // return _ProcessEvent(a_dispatcher, dummy);
  }
  _ProcessEvent(a_dispatcher, a_events);
}
}  // namespace Hooks
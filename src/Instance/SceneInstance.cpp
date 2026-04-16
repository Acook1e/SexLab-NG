#include "Instance/SceneInstance.h"

#include "Instance/Collision.h"
#include "Instance/Scale.h"
#include "Registry/Stat.h"
#include "Utils/UI.h"

namespace Instance
{

namespace
{
  constexpr std::size_t kPackedActorOrderCapacity = sizeof(std::uint64_t) / sizeof(std::uint8_t);
  constexpr std::uint8_t kInvalidPackedActorIndex = 0xFF;
  constexpr float kEquivalentScaleDelta           = 0.01f;

  std::uint8_t UnpackActorIndex(std::uint64_t packedOrder, std::size_t positionIndex)
  {
    if (positionIndex >= kPackedActorOrderCapacity)
      return kInvalidPackedActorIndex;

    const std::uint64_t shift = static_cast<std::uint64_t>(positionIndex) * 8ULL;
    return static_cast<std::uint8_t>((packedOrder >> shift) & 0xFFULL);
  }

  struct PendingAssignment
  {
    std::size_t actorIndex;
    RE::Actor* actor;
    Define::Position* position;
    Define::Race race;
    Define::Gender gender;
    float scale;
  };

  void ShuffleEquivalentAssignments(std::vector<PendingAssignment>& assignments)
  {
    static std::mt19937 rng(std::random_device{}());

    std::vector<std::size_t> order(assignments.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t lhsIndex, std::size_t rhsIndex) {
      const auto& lhs = assignments[lhsIndex];
      const auto& rhs = assignments[rhsIndex];
      if (lhs.race.Get() != rhs.race.Get())
        return lhs.race.Get() < rhs.race.Get();
      if (lhs.gender.Get() != rhs.gender.Get())
        return lhs.gender.Get() < rhs.gender.Get();
      if (lhs.scale != rhs.scale)
        return lhs.scale < rhs.scale;
      return lhs.actorIndex < rhs.actorIndex;
    });

    for (std::size_t begin = 0; begin < order.size();) {
      std::size_t end              = begin + 1;
      const auto& anchorAssignment = assignments[order[begin]];
      const auto anchorRace        = anchorAssignment.race.Get();
      const auto anchorGender      = anchorAssignment.gender.Get();
      const float anchorScale      = anchorAssignment.scale;

      while (end < order.size()) {
        const auto& candidate = assignments[order[end]];
        if (candidate.race.Get() != anchorRace || candidate.gender.Get() != anchorGender ||
            std::abs(candidate.scale - anchorScale) >= kEquivalentScaleDelta) {
          break;
        }
        ++end;
      }

      if (end - begin > 1) {
        std::vector<std::size_t> shuffledActorIndices;
        shuffledActorIndices.reserve(end - begin);
        for (std::size_t index = begin; index < end; ++index)
          shuffledActorIndices.push_back(assignments[order[index]].actorIndex);

        std::shuffle(shuffledActorIndices.begin(), shuffledActorIndices.end(), rng);

        for (std::size_t index = begin; index < end; ++index) {
          auto& assignment       = assignments[order[index]];
          const std::size_t slot = index - begin;
          assignment.actorIndex  = shuffledActorIndices[slot];
        }
      }

      begin = end;
    }
  }

}  // namespace

SceneInstance::SceneInstance(RE::Actor* central, std::vector<RE::Actor*> participants,
                             SceneSearchResult scenes)
{
  availableScenes = std::move(scenes);

  actors.reserve(participants.size() + 1);
  actors.push_back(central);
  for (auto* participant : participants)
    actors.push_back(participant);

  interact = Interact(actors);

  if (availableScenes.empty()) {
    currentScene = nullptr;
    currentStage = 0;
    return;
  }

  state = InstanceState::CreateInstance;

  currentScene = RandomScene();
  currentStage = 0;

  logger::info("[SexLab NG] Current scene: '{}'", currentScene ? currentScene->GetName() : "null");

  if (!SetPositions()) {
    currentScene = nullptr;
    currentStage = 0;
    return;
  }

  for (auto* actor : actors) {
    if (!actor)
      continue;
    Collision::GetSingleton().AddActor(actor);
  }

  if (Settings::bEnableActorWalkToCenter) {
    state = InstanceState::ActorApproach;
  } else {
    state = InstanceState::SceneReady;
  }
}

SceneInstance::~SceneInstance()
{
  state = InstanceState::DestroyInstance;

  UI::GetSingleton().Hide(this);

  // TODO: 在交互识别稳定后，基于 interact.GetObservedInteractTags(actor) 做场景/位置 tag 纠正。
  // ── 场景结束: 收集 enjoyment 记录并更新 stat ──────────────
  Registry::ActorStat::GetSingleton().UpdateStat(currentScene, actorInfoMap);

  ResetActors();
  DressActors();
  UnlockActors();

  for (auto* actor : actors) {
    if (!actor)
      continue;
    Collision::GetSingleton().RemoveActor(actor);
  }
}

bool SceneInstance::Update()
{
  constexpr std::uint64_t UPDATE_INTERVAL = 100;  // ScenePlay阶段每100ms更新一次
  constexpr std::uint64_t BASE_STAGE_LENGTH =
      10 * 1000;  // 每个阶段基础长度为10秒，实际长度会根据场景定义和当前阶段进行调整
  constexpr std::uint64_t MAX_STAGE_LENGTH =
      40 * 1000;  // 每个阶段的最大长度为40秒，防止某些阶段过长导致游戏体验变差
  constexpr std::uint64_t SOS_READY =
      1000;  // 因为TNG行为图性能较差，给SOS准备阶段额外增加1秒的时间

  const auto Now =
      static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count());

  auto now = Now;

  if (state == InstanceState::ActorApproach) {
    // TODO: 允许在 ActorApproach 阶段启用 Actor Walk to Center 功能，暂时先直接跳过此阶段
    state = InstanceState::SceneReady;
    return true;
  }

  // 场景准备阶段，锁定、脱衣、准备，并显示 UI
  if (state == InstanceState::SceneReady) {

    if (actorInfoMap.empty() || !currentScene)
      return false;

    LockActors();
    StripActors();
    ReadyActors();

    for (auto* actor : actors)
      if (actor->IsPlayerRef()) {
        UI::GetSingleton().Show(this);
        break;
      }

    currentStage        = 1;
    lastStageUpdateTime = now - BASE_STAGE_LENGTH + SOS_READY;  // Schedule the first stage update
    lastUpdateTime      = now;
    state               = InstanceState::ScenePlay;
    interact.FlashNodeData();

    return true;
  }

  // 处于场景切换阶段时，暂停更新
  // 设置上次推进阶段的时间为最大，确保切换完成后能立即推进到下一阶段
  if (state == InstanceState::SceneChange) {
    lastUpdateTime      = now;
    lastStageUpdateTime = now - MAX_STAGE_LENGTH;
    return true;
  }

  if (state == InstanceState::ScenePlay) {

    // 对于其他阶段不需要更新间隔
    if (now - lastUpdateTime < UPDATE_INTERVAL)
      return true;

    const float deltaMs = static_cast<float>(now - lastUpdateTime);
    lastUpdateTime      = now;

    interact.Update(deltaMs);
    Registry::ActorStat::GetSingleton().UpdateEnjoyment(currentScene, actorInfoMap, interact);

    // 总是最后更新 UI，确保数据同步
    UI::GetSingleton().Update(this);

    auto res = true;
    if (now - lastStageUpdateTime > BASE_STAGE_LENGTH) {
      lastStageUpdateTime = now;
      res                 = SetStage(currentStage);
      if (res)
        ++currentStage;
      if (currentStage > currentScene->GetTotalStages())
        state = InstanceState::SceneEnd;
    }

    // 当 res 为 false 时，表示场景遭遇某些错误必须提前结束
    if (!res) {
      logger::warn("[SexLab NG] Scene '{}' ended prematurely at stage {} due to an error.",
                   currentScene ? currentScene->GetName() : "null", currentStage);
      return false;
    }

    return true;
  }

  // 当进入场景结束阶段，进行一次事件分发
  // 假如此时收到新的场景列表或者设置为新场景
  // 那保留此实例并play新场景，否则销毁此实例
  if (state == InstanceState::SceneEnd) {
    // TODO: 允许在SceneEnd阶段注入新场景并重置状态而不销毁实例
    return false;
  }

  // 你为什么会执行到这
  logger::error("[SexLab NG] SceneInstance in unexpected state: {}",
                static_cast<std::uint8_t>(state));
  return true;
}

void SceneInstance::LockActors()
{
  for (auto* actor : actors) {
    if (!actor)
      continue;

    if (actor->IsPlayerRef()) {
      RE::PlayerCharacter::GetSingleton()->SetAIDriven(true);
      actor->SetLifeState(RE::ACTOR_LIFE_STATE::kAlive);
    } else {
      actor->SetLifeState(RE::ACTOR_LIFE_STATE::kRestrained);
      actor->SetGraphVariableInt("IsNPC", 0);
    }

    actor->NotifyAnimationGraph("IdleFurnitureExit");
    actor->NotifyAnimationGraph("IdleStop");
    actor->SetGraphVariableBool("bHumanoidFootIKDisable", true);

    if (actor->AsActorState()->IsWeaponDrawn()) {
      const auto factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::Script>();
      if (const auto script = factory ? factory->Create() : nullptr) {
        script->SetCommand("rae weaponsheathe");
        script->CompileAndRun(actor);
        if (!actor->IsPlayerRef() && actor->IsSneaking()) {
          script->SetCommand("setforcesneak 0");
          script->CompileAndRun(actor);
        }
        delete script;
      }
    }

    actor->StopCombat();
    actor->EndDialogue();
    actor->InterruptCast(false);
    actor->StopInteractingQuick(true);
    if (auto process = actor->GetActorRuntimeData().currentProcess; process)
      process->ClearMuzzleFlashes();
    actor->StopMoving(1.0f);
  }
}
void SceneInstance::UnlockActors()
{
  for (auto* actor : actors) {
    if (!actor)
      continue;

    if (actor->IsPlayerRef()) {
      RE::PlayerCharacter::GetSingleton()->SetAIDriven(false);
    } else {
      actor->SetGraphVariableInt("IsNPC", 1);
    }

    actor->SetLifeState(RE::ACTOR_LIFE_STATE::kAlive);
    actor->SetGraphVariableBool("bHumanoidFootIKDisable", false);
  }
}

void SceneInstance::StripActors()
{

  const auto manager = RE::ActorEquipManager::GetSingleton();
  if (!manager)
    return;

  for (auto& [actor, info] : actorInfoMap) {
    if (!actor)
      continue;

    for (const auto& [object, entry] : actor->GetInventory()) {
      if (!entry.second->IsWorn())
        continue;

      info.strippedItems.push_back(object);
      manager->UnequipObject(actor, object);
    }
    actor->Update3DModel();
  }

  interact.FlashNodeData();
}

void SceneInstance::DressActors()
{
  const auto manager = RE::ActorEquipManager::GetSingleton();
  if (!manager)
    return;

  for (const auto& [actor, info] : actorInfoMap) {
    if (!actor || info.strippedItems.empty())
      continue;

    for (auto* object : info.strippedItems) {
      if (!object)
        continue;
      manager->EquipObject(actor, object);
    }
    actor->Update3DModel();
  }

  interact.FlashNodeData();
}

void SceneInstance::ReadyActors()
{
  if (!actors.empty() && actors.front()) {
    auto* central = actors.front();
    for (const auto& [actor, info] : actorInfoMap) {
      if (!actor)
        continue;
      Scale::GetSingleton().ApplyScale(actor, info.position->GetScale());

      if (actor == central)
        continue;
      actor->SetPosition(central->GetPosition(), true);
      actor->SetAngle(central->GetAngle());
      actor->Update3DPosition(true);
      if (info.position->GetGender().HasPenis())
        actor->NotifyAnimationGraph("SOSBend0");
    }
  }
}

void SceneInstance::ResetActors()
{
  for (const auto& [actor, info] : actorInfoMap) {
    if (!actor)
      continue;
    Scale::GetSingleton().RemoveScale(actor);
    actor->NotifyAnimationGraph("AnimObjectUnequip");
    // Reset human
    actor->NotifyAnimationGraph("IdleForceDefaultState");
    // actor->NotifyAnimationGraph("ReturnDefaultState");
    // actor->NotifyAnimationGraph("ReturnToDefault");
    // actor->NotifyAnimationGraph("FNISDefault");
    //  actor->NotifyAnimationGraph("IdleReturnToDefault");
    //  actor->NotifyAnimationGraph("ForceFurnExit");
    // actor->NotifyAnimationGraph("Reset");
  }
}

bool SceneInstance::SetScene(Define::Scene* scene)
{
  if (!scene)
    return false;

  if (scene == currentScene)
    return true;

  if (availableScenes.find(scene) == availableScenes.end())
    return false;

  Define::Scene* previousScene      = currentScene;
  const std::uint32_t previousStage = currentStage;
  const InstanceState previousState = state;

  state = InstanceState::SceneChange;

  currentScene = scene;
  currentStage = 1;

  if (!SetPositions()) {
    currentScene = previousScene;
    currentStage = previousStage;
    state        = previousState;
    return false;
  }

  ReadyActors();
  interact.FlashNodeData();

  logger::info("[SexLab NG] Switched to scene: '{}'",
               currentScene ? currentScene->GetName() : "null");
  state = InstanceState::ScenePlay;
  return true;
}

Define::Scene* SceneInstance::RandomScene()
{
  if (availableScenes.empty())
    return nullptr;

  static std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<std::size_t> dist(0, availableScenes.size() - 1);
  auto it = availableScenes.begin();
  std::advance(it, static_cast<std::ptrdiff_t>(dist(rng)));
  return it != availableScenes.end() ? it->first : nullptr;
}

bool SceneInstance::SetPositions()
{
  if (!currentScene)
    return false;

  auto& positions = currentScene->GetPositions();

  if (auto orderIt = availableScenes.find(currentScene); orderIt != availableScenes.end()) {
    const std::uint64_t packedOrder = orderIt->second;
    if (positions.size() <= kPackedActorOrderCapacity) {
      bool validRecordedOrder = true;
      std::vector<bool> seen(actors.size(), false);
      std::vector<PendingAssignment> assignments;
      assignments.reserve(positions.size());

      for (std::size_t p = 0; p < positions.size(); ++p) {
        const std::size_t actorIndex = UnpackActorIndex(packedOrder, p);
        if (actorIndex >= actors.size() || seen[actorIndex] || !actors[actorIndex]) {
          validRecordedOrder = false;
          break;
        }
        seen[actorIndex] = true;

        RE::Actor* actor = actors[actorIndex];
        assignments.push_back(
            PendingAssignment{actorIndex, actor, &positions[p], Define::Race::GetRace(actor),
                              Define::Gender::GetGender(actor), Scale::CalculateScale(actor)});
      }

      if (validRecordedOrder) {
        ShuffleEquivalentAssignments(assignments);

        for (auto& assignment : assignments) {
          assignment.actor = actors[assignment.actorIndex];
          if (!assignment.actor) {
            validRecordedOrder = false;
            break;
          }
        }

        if (!validRecordedOrder)
          return false;

        for (const auto& assignment : assignments) {
          auto* actor                = assignment.actor;
          Define::Position* position = assignment.position;
          if (auto it = actorInfoMap.find(actor); it != actorInfoMap.end())
            it->second.position = position;
          else
            actorInfoMap.emplace(actor, SceneActorInfo(position));
        }
        return true;
      }
    }
  }

  logger::warn("[SexLab NG] SetPositions: missing or invalid packed order for scene '{}'",
               currentScene ? currentScene->GetName() : "null");
  return false;
}

bool SceneInstance::SetStage(std::uint32_t stage)
{
  for (const auto& [actor, info] : actorInfoMap) {
    if (!actor)
      return false;

    const auto& events = info.position->GetEvents();
    if (stage > events.size())
      return false;

    if (!actor->NotifyAnimationGraph(events[stage - 1].data()))
      return false;

    if (info.position->GetGender().HasPenis()) {
      // For SOS or TNG, value from -9 to 9
      std::int8_t angle = info.position->GetSchlongAngles()[stage - 1];
      actor->NotifyAnimationGraph("SOSBend" + std::to_string(angle));
    }
  }

  interact.FlashNodeData();
  return true;
}
}  // namespace Instance
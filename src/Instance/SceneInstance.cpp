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
}

SceneInstance::~SceneInstance()
{
  state = InstanceState::DestroyInstance;

  UI::GetSingleton().Hide(this);

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
  constexpr std::uint64_t UPDATE_INTERVAL = 100;        // Update every 100 milliseconds
  constexpr std::uint64_t STAGE_LENGTH    = 10 * 1000;  // Update every 10 seconds
  constexpr std::uint64_t SOS_READY       = 1000;       // 1 second to prepare for SOSBend

  const auto Now =
      static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count());

  auto now = Now;
  if (now - lastUpdateTime < UPDATE_INTERVAL)
    return true;

  // If the scene hasn't started yet, start it
  // Process only once
  if (!currentStage) {
    if (actorInfoMap.empty() || !currentScene)
      return false;

    state = InstanceState::SceneReady;

    LockActors();
    StripActors();
    ReadyActors();

    for (auto* actor : actors)
      if (actor->IsPlayerRef()) {
        UI::GetSingleton().Show(this);
        break;
      }

    currentStage        = 1;
    lastStageUpdateTime = now - STAGE_LENGTH + SOS_READY;  // Schedule the first stage update
    state               = InstanceState::ScenePlay;
    interact.FlashNodeData();
  }

  // Real Update start from here
  lastUpdateTime = now;

  interact.Update();
  Registry::ActorStat::GetSingleton().UpdateEnjoyment(currentScene, actorInfoMap, interact);

  // 总是最后更新 UI，确保数据同步
  UI::GetSingleton().Update(this);

  if (now - lastStageUpdateTime > STAGE_LENGTH) {
    lastStageUpdateTime = now;
    if (SetStage(currentStage))
      return currentStage++;

    state = InstanceState::SceneEnd;
    return false;
  }

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
#include "Instance/SceneInstance.h"

#include "Instance/Collision.h"

namespace Instance
{
SceneInstance::SceneInstance(RE::Actor* central, std::vector<RE::Actor*> participants,
                             std::vector<const Define::Scene*> scenes)
{
  actors.reserve(participants.size() + 1);
  actors.push_back(central);
  for (auto* participant : participants)
    actors.push_back(participant);

  availableScenes = std::move(scenes);

  if (availableScenes.empty()) {
    currentScene = nullptr;
    currentStage = 0;
    return;
  }

  currentScene = availableScenes[rand() % availableScenes.size()];
  currentStage = 0;

  const auto& positions = currentScene->GetPositions();
  std::vector<bool> actorAssigned(actors.size(), false);
  for (auto& position : positions) {
    for (std::size_t j = 0; j < actors.size(); ++j) {
      auto* actor = actors[j];
      if (!actor || actorAssigned[j])
        continue;

      if (position.GetRace() == Define::Race::GetRace(actor) &&
          position.GetGender() == Define::Gender::GetGender(actor)) {
        actorAssigned[j] = true;
        actorInfoMap.emplace(actor, SceneActorInfo(Define::EnjoyDegree::NoFeeling, 0.0f, position));
        break;
      }
    }
  }

  for (auto* actor : actors) {
    if (!actor)
      continue;
    if (actorInfoMap.find(actor) == actorInfoMap.end())
      continue;
    Collision::GetSingleton().AddActor(actor);
  }
}

SceneInstance::~SceneInstance()
{
  for (auto* actor : actors) {
    if (!actor)
      continue;
    if (actorInfoMap.find(actor) == actorInfoMap.end())
      continue;
    Collision::GetSingleton().RemoveActor(actor);
  }
}

bool SceneInstance::Update()
{
  constexpr std::uint64_t STAGE_LENGTH = 30 * 1000;  // Update every 30 seconds

  const auto now = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
          .count());

  auto cleanupAndFinish = [this]() {
    DressActors();
    UnlockActors();
    return false;
  };

  if (!currentStage) {
    if (actorInfoMap.empty() || !currentScene)
      return false;

    LockActors();

    if (!actors.empty() && actors.front()) {
      auto* central = actors.front();
      for (const auto& [actor, _] : actorInfoMap) {
        if (!actor || actor == central)
          continue;
        actor->SetPosition(central->GetPosition(), true);
        actor->SetAngle(central->GetAngle());
        actor->Update3DPosition(true);
      }
    }

    StripActors();
    lastStageUpdateTime = now;
    if (!NextStage())
      return cleanupAndFinish();
  }

  if (now <= lastStageUpdateTime || now - lastStageUpdateTime < STAGE_LENGTH) {
    return true;
  }

  if (!NextStage())
    return cleanupAndFinish();

  lastStageUpdateTime = now;
  return true;
}

void SceneInstance::LockActors()
{
  for (auto* actor : actors) {
    if (!actor || actorInfoMap.find(actor) == actorInfoMap.end())
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
    if (const auto process = actor->GetActorRuntimeData().currentProcess; process)
      process->ClearMuzzleFlashes();
    actor->StopMoving(1.0f);
  }
}
void SceneInstance::UnlockActors()
{
  for (auto* actor : actors) {
    if (!actor || actorInfoMap.find(actor) == actorInfoMap.end())
      continue;

    if (actor->IsPlayerRef()) {
      RE::PlayerCharacter::GetSingleton()->SetAIDriven(false);
    } else {
      actor->SetGraphVariableInt("IsNPC", 1);
    }

    actor->SetLifeState(RE::ACTOR_LIFE_STATE::kAlive);
    actor->NotifyAnimationGraph("IdleStop");
    actor->SetGraphVariableBool("bHumanoidFootIKDisable", false);
  }
}

void SceneInstance::StripActors()
{

  const auto manager = RE::ActorEquipManager::GetSingleton();
  if (!manager)
    return;

  for (auto* actor : actors) {
    if (!actor)
      continue;

    auto it = actorInfoMap.find(actor);
    if (it == actorInfoMap.end())
      continue;

    for (auto& [object, entry] : actor->GetInventory()) {
      if (!entry.second->IsWorn())
        continue;

      it->second.strippedItems.push_back(object);
      manager->UnequipObject(actor, object);
    }
    actor->Update3DModel();
  }
}
void SceneInstance::DressActors()
{
  const auto manager = RE::ActorEquipManager::GetSingleton();
  if (!manager)
    return;

  for (auto* actor : actors) {
    if (!actor)
      continue;

    auto it = actorInfoMap.find(actor);
    if (it == actorInfoMap.end())
      continue;

    for (auto* object : it->second.strippedItems) {
      if (!object)
        continue;
      manager->EquipObject(actor, object);
    }
    actor->Update3DModel();
  }
}

bool SceneInstance::NextStage()
{
  currentStage++;
  for (auto& [actor, info] : actorInfoMap) {
    if (!actor) {
      return false;
    }

    const auto& events = info.position->GetEvents();
    if (currentStage > events.size() + 1) {
      return false;
    }

    if (!actor->NotifyAnimationGraph(events[currentStage - 1].data()))
      return false;
  }
  return true;
}

bool SceneInstance::PrevStage()
{
  if (currentStage == 0)
    return false;

  currentStage--;
  for (auto& [actor, info] : actorInfoMap) {
    if (!actor) {
      return false;
    }

    const auto& events = info.position->GetEvents();
    if (currentStage > events.size() + 1) {
      return false;
    }

    if (!actor->NotifyAnimationGraph(events[currentStage - 1].data()))
      return false;
  }
  return true;
}
}  // namespace Instance
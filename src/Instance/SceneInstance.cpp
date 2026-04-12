#include "Instance/SceneInstance.h"

#include "Instance/Collision.h"
#include "Instance/Scale.h"
#include "Registry/Stat.h"
#include "Utils/UI.h"

namespace Instance
{
SceneInstance::SceneInstance(RE::Actor* central, std::vector<RE::Actor*> participants,
                             std::vector<Define::Scene*> scenes, Define::Scene* leadIn)
    : availableScenes(std::move(scenes))
{
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

  if (leadIn)
    currentScene = leadIn;
  else
    currentScene = RandomScene();
  currentStage = 0;

  logger::info("[SexLab NG] Current scene: '{}'", currentScene ? currentScene->GetName() : "null");

  SetPositions();

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
    if (std::find(availableScenes.begin(), availableScenes.end(), currentScene) ==
        availableScenes.end())
      state = InstanceState::LeadIn;
    else
      state = InstanceState::ScenePlay;
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
    if (state == InstanceState::LeadIn) {
      if (SetStage(currentStage))
        return currentStage++;
      else {
        state        = InstanceState::ScenePlay;
        currentScene = RandomScene();
        currentStage = 1;
        for (auto& [actor, info] : actorInfoMap)
          info.position = nullptr;  // Clear previous position info
        SetPositions();
      }
    }
    return SetStage(currentStage) ? currentStage++ : false;
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
  return availableScenes.at(dist(rng));
}

// ════════════════════════════════════════════════════════════
// O(N³) 匈牙利算法: 最小代价完美匹配
// cost[i][j] = 将第 i 行分配给第 j 列的代价 (N×N 方阵)
// 返回 assignment[i] = j (0-indexed)
// ════════════════════════════════════════════════════════════
static std::vector<int> HungarianAssign(const std::vector<std::vector<float>>& cost)
{
  const int n           = static_cast<int>(cost.size());
  constexpr float kHInf = 2e9f;

  std::vector<float> u(n + 1, 0.f), v(n + 1, 0.f);
  std::vector<int> p(n + 1, 0), way(n + 1, 0);

  for (int i = 1; i <= n; ++i) {
    p[0]   = i;
    int j0 = 0;
    std::vector<float> minv(n + 1, kHInf);
    std::vector<bool> used(n + 1, false);
    do {
      used[j0]     = true;
      const int i0 = p[j0];
      float delta  = kHInf;
      int j1       = -1;
      for (int j = 1; j <= n; ++j) {
        if (!used[j]) {
          const float cur = cost[i0 - 1][j - 1] - u[i0] - v[j];
          if (cur < minv[j]) {
            minv[j] = cur;
            way[j]  = j0;
          }
          if (minv[j] < delta) {
            delta = minv[j];
            j1    = j;
          }
        }
      }
      for (int j = 0; j <= n; ++j) {
        if (used[j]) {
          u[p[j]] += delta;
          v[j] -= delta;
        } else {
          minv[j] -= delta;
        }
      }
      j0 = j1;
    } while (p[j0] != 0);
    do {
      const int j1 = way[j0];
      p[j0]        = p[j1];
      j0           = j1;
    } while (j0 != 0);
  }

  std::vector<int> assignment(n, -1);
  for (int j = 1; j <= n; ++j)
    if (p[j] != 0)
      assignment[p[j] - 1] = j - 1;
  return assignment;
}

void SceneInstance::SetPositions()
{
  auto& positions      = currentScene->GetPositions();
  const int n          = static_cast<int>(actors.size());
  constexpr float kInf = 1e9f;

  // 构建代价矩阵: race/gender 不匹配 → kInf, 否则 |scaleDiff|
  std::vector<std::vector<float>> cost(n, std::vector<float>(n, kInf));
  for (int a = 0; a < n; ++a) {
    auto* actor = actors[a];
    if (!actor)
      continue;
    const auto race   = Define::Race::GetRace(actor);
    const auto gender = Define::Gender::GetGender(actor);
    const float scale = Scale::CalculateScale(actor);
    for (int p = 0; p < n; ++p) {
      if (positions[p].GetRace() == race && positions[p].GetGender() == gender)
        cost[a][p] = std::abs(scale - positions[p].GetScale());
    }
  }

  const auto assignment = HungarianAssign(cost);

  for (int a = 0; a < n; ++a) {
    const int pa = assignment[a];
    if (pa < 0 || cost[a][pa] >= kInf * 0.5f)
      continue;
    auto* actor = actors[a];
    if (!actor)
      continue;
    if (auto it = actorInfoMap.find(actor); it != actorInfoMap.end())
      it->second.position = &positions[pa];
    else
      actorInfoMap.emplace(actor, SceneActorInfo(&positions[pa]));
  }
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
#include "Instance/Manager.h"

#include "Instance/Scale.h"

namespace Instance
{

// ════════════════════════════════════════════════════════════
// 辅助结构: 缓存 actor 属性, 避免重复计算
// ════════════════════════════════════════════════════════════
struct ActorInfo
{
  Define::Race race;
  Define::Gender gender;
  float scale;
};

struct SceneCandidate
{
  Define::Scene* scene         = nullptr;
  std::uint64_t actorOrderCode = 0;
  float totalScaleDeviation    = 0.f;
  std::uint32_t highCostScales = 0;
};

constexpr std::size_t kPackedActorOrderCapacity = sizeof(std::uint64_t) / sizeof(std::uint8_t);
constexpr std::uint8_t kInvalidPackedActorIndex = 0xFF;

static std::uint64_t PackActorOrder(const std::vector<std::size_t>& actorOrder)
{
  std::uint64_t packed = (std::numeric_limits<std::uint64_t>::max)();
  for (std::size_t positionIndex = 0; positionIndex < actorOrder.size(); ++positionIndex) {
    const std::uint64_t shift = static_cast<std::uint64_t>(positionIndex) * 8ULL;
    packed &= ~(0xFFULL << shift);
    packed |= (static_cast<std::uint64_t>(actorOrder[positionIndex]) & 0xFFULL) << shift;
  }
  return packed;
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

// ════════════════════════════════════════════════════════════
// Stage 0: Race + Gender 完全匹配 + 最优 scale 分配 (匈牙利)
// 返回最小总 scaleDev、高代价 scale 次数，以及 position 顺序下的 actor 分配
// ════════════════════════════════════════════════════════════
struct ScaleMatchResult
{
  bool valid                   = false;
  float totalScaleDeviation    = 0.f;
  std::uint32_t highCostScales = 0;
  std::uint64_t actorOrderCode = 0;
};

static ScaleMatchResult
MatchActorsToPositions(const std::vector<RE::Actor*>& actors,
                       const std::unordered_map<RE::Actor*, ActorInfo>& infos,
                       const std::vector<Define::Position>& positions)
{
  constexpr float kInf                    = 1e9f;
  constexpr float kHighCostScaleDeviation = 0.10f;
  const int n                             = static_cast<int>(actors.size());
  ScaleMatchResult result;
  if (actors.size() > kPackedActorOrderCapacity)
    return result;

  std::vector<std::size_t> actorOrder(n, static_cast<std::size_t>(kInvalidPackedActorIndex));

  // 构建代价矩阵: race/gender 不匹配 → kInf, 否则 |scaleDiff|
  std::vector<std::vector<float>> cost(n, std::vector<float>(n, kInf));
  for (int a = 0; a < n; ++a) {
    const auto& info = infos.at(actors[a]);
    for (int p = 0; p < n; ++p) {
      if (positions[p].GetRace() == info.race && positions[p].GetGender() == info.gender)
        cost[a][p] = std::abs(info.scale - positions[p].GetScale());
    }
  }

  const auto assignment = HungarianAssign(cost);

  for (int a = 0; a < n; ++a) {
    const int pa = assignment[a];
    if (pa < 0 || cost[a][pa] >= kInf * 0.5f)
      return result;

    result.totalScaleDeviation += cost[a][pa];
    if (cost[a][pa] > kHighCostScaleDeviation)
      ++result.highCostScales;
    actorOrder[pa] = static_cast<std::size_t>(a);
  }

  result.actorOrderCode = PackActorOrder(actorOrder);
  result.valid          = true;
  return result;
}

// ════════════════════════════════════════════════════════════
// Stage 1: SceneTags 匹配
// 场景的 SceneTags 必须包含 required 中设置的所有位
// ════════════════════════════════════════════════════════════
static bool MatchSceneTags(const Define::SceneTags& sceneTags, const Define::SceneTags& required)
{
  return (sceneTags.Get() & required.Get()) == required.Get();
}

// ════════════════════════════════════════════════════════════
// Stage 2: InteractTags 匹配
//
// actorInteractTags: 每个 actor 要求的交互类型
// 对于每个有要求的 actor, 场景中必须存在至少一个
// race+gender 匹配的 position, 其 InteractTags 包含该 actor 的所有要求位
// ════════════════════════════════════════════════════════════
static bool
MatchInteractTags(const std::vector<RE::Actor*>& actors,
                  const std::unordered_map<RE::Actor*, ActorInfo>& infos,
                  const std::unordered_map<RE::Actor*, Define::InteractTags>& actorInteractTags,
                  std::vector<Define::Position>& positions)
{
  for (const auto& [actor, required] : actorInteractTags) {
    if (required.Get() == 0)
      continue;

    const auto it = infos.find(actor);
    if (it == infos.end())
      continue;

    const auto& info = it->second;
    bool found       = false;
    for (const auto& pos : positions) {
      if (pos.GetRace() == info.race && pos.GetGender() == info.gender) {
        if ((pos.GetTags().Get() & required.Get()) == required.Get()) {
          found = true;
          break;
        }
      }
    }
    if (!found)
      return false;
  }
  return true;
}

// ════════════════════════════════════════════════════════════
// SearchScenes — 多阶段渐进筛选
//
// Pipeline:
//   Stage 0: Race + Gender 完全匹配           → basePool
//   Stage 1: SceneTags                        → sceneTagPool
//   Stage 2: InteractTags (per-actor)         → interactPool
//   Stage 3: Scale (totalScaleDev ≤ 阈值)     → 排序返回 / fallback
// ════════════════════════════════════════════════════════════

SceneSearchResult SceneManager::SearchScenes(std::vector<RE::Actor*> actors, SearchOptions options)
{
  ScopeTimer timer("SceneManager::SearchScenes");

  for (auto* actor : actors)
    if (!actor)
      return {};

  if (actors.size() > kPackedActorOrderCapacity) {
    logger::warn("[SexLab NG] SearchScenes: {} actors exceed packed order capacity {}",
                 actors.size(), kPackedActorOrderCapacity);
    return {};
  }

  // ── 预计算 actor 信息 ─────────────────────────────────────
  std::unordered_map<RE::Actor*, ActorInfo> infos;
  for (auto* actor : actors)
    infos.emplace(actor, ActorInfo{Define::Race::GetRace(actor), Define::Gender::GetGender(actor),
                                   Scale::CalculateScale(actor)});

  std::uint64_t racesMask = 0;
  for (auto& [_, info] : infos)
    racesMask |= info.race.Get();

  // ── 是否有 SceneTags / InteractTags 要求 ──────────────────
  const bool hasSceneTagReq = options.sceneTags.Get() != 0;

  bool hasInteractTagReq = false;
  for (const auto& [_, tags] : options.actorInteractTags)
    if (tags.Get() != 0) {
      hasInteractTagReq = true;
      break;
    }

  // ══════════════════════════════════════════════════════════
  // Stage 0: Race + Gender + 人数过滤
  // ══════════════════════════════════════════════════════════
  std::vector<SceneCandidate> basePool;

  for (auto& animPack : animPacks) {
    for (auto& scene : animPack.GetScenes()) {
      if (!Settings::bUseLeadIn && scene.GetTags().Has(Define::SceneTags::Type::LeadIn))
        continue;

      // Races 掩码快速排除
      if (!(scene.GetRaces() >= Define::Race(racesMask)))
        continue;

      // 人数匹配
      auto& positions = scene.GetPositions();
      if (positions.size() != actors.size())
        continue;

      // Race + Gender 一一对应 (匈牙利最优匹配, 同时算出最小 scaleDev)
      const ScaleMatchResult scaleMatch = MatchActorsToPositions(actors, infos, positions);
      if (!scaleMatch.valid)
        continue;

      basePool.push_back(SceneCandidate{&scene, scaleMatch.actorOrderCode,
                                        scaleMatch.totalScaleDeviation, scaleMatch.highCostScales});
    }
  }

  if (basePool.empty()) {
    logger::warn("[SexLab NG] SearchScenes: No scenes match race/gender for {} actors",
                 actors.size());
    return {};
  }

  logger::info("[SexLab NG] SearchScenes: Stage 0 (base) — {} scenes", basePool.size());

  // ══════════════════════════════════════════════════════════
  // Stage 1: SceneTags
  // ══════════════════════════════════════════════════════════
  auto sceneTagPool = [&]() -> std::vector<SceneCandidate> {
    if (!hasSceneTagReq)
      return basePool;

    std::vector<SceneCandidate> pool;
    for (auto& entry : basePool)
      if (MatchSceneTags(entry.scene->GetTags(), options.sceneTags))
        pool.push_back(entry);

    if (pool.empty()) {
      if (options.strictMatchSceneTags) {
        logger::info("[SexLab NG] SearchScenes: Strict SceneTags — empty, returning empty");
        return {};  // 严格模式: 返回空, 外层检测后直接返回
      }
      logger::info("[SexLab NG] SearchScenes: SceneTags fallback to base ({})", basePool.size());
      return basePool;
    }

    logger::info("[SexLab NG] SearchScenes: Stage 1 (SceneTags) — {} scenes", pool.size());
    return pool;
  }();

  // 严格 SceneTags 未找到 → 直接返回空
  if (hasSceneTagReq && options.strictMatchSceneTags && sceneTagPool.empty())
    return {};

  // ══════════════════════════════════════════════════════════
  // Stage 2: InteractTags (per-actor)
  // ══════════════════════════════════════════════════════════
  auto interactPool = [&]() -> std::vector<SceneCandidate> {
    if (!hasInteractTagReq)
      return sceneTagPool;

    std::vector<SceneCandidate> pool;
    for (auto& entry : sceneTagPool) {
      auto& positions = entry.scene->GetPositions();
      if (MatchInteractTags(actors, infos, options.actorInteractTags, positions))
        pool.push_back(entry);
    }

    if (pool.empty()) {
      if (options.strictMatchInteractTags) {
        logger::info("[SexLab NG] SearchScenes: Strict InteractTags — empty, returning empty");
        return {};
      }
      logger::info("[SexLab NG] SearchScenes: InteractTags fallback to previous ({})",
                   sceneTagPool.size());
      return sceneTagPool;
    }

    logger::info("[SexLab NG] SearchScenes: Stage 2 (InteractTags) — {} scenes", pool.size());
    return pool;
  }();

  // 严格 InteractTags 未找到 → 直接返回空
  if (hasInteractTagReq && options.strictMatchInteractTags && interactPool.empty())
    return {};

  // ══════════════════════════════════════════════════════════
  // Stage 3: Scale (totalScaleDev ≤ 阈值, 否则 fallback)
  // ══════════════════════════════════════════════════════════
  const auto minHighCostIt =
      std::min_element(interactPool.begin(), interactPool.end(),
                       [](const SceneCandidate& lhs, const SceneCandidate& rhs) {
                         return lhs.highCostScales < rhs.highCostScales;
                       });
  const std::uint32_t minHighCostScales =
      minHighCostIt != interactPool.end() ? minHighCostIt->highCostScales : 0;

  std::vector<SceneCandidate> minHighCostPool;
  for (const auto& entry : interactPool)
    if (entry.highCostScales == minHighCostScales)
      minHighCostPool.push_back(entry);

  logger::info("[SexLab NG] SearchScenes: Stage 3 (ScaleCost) — minHighCost={} => {} / {} scenes",
               minHighCostScales, minHighCostPool.size(), interactPool.size());

  std::vector<SceneCandidate> scaleStrict;
  for (const auto& entry : minHighCostPool)
    if (entry.totalScaleDeviation <= Settings::fMaxScaleDeviation)
      scaleStrict.push_back(entry);

  std::vector<SceneCandidate> finalPool =
      scaleStrict.empty() ? std::move(minHighCostPool) : std::move(scaleStrict);

  if (!scaleStrict.empty()) {
    logger::info("[SexLab NG] SearchScenes: Stage 3 (ScaleDeviation) — {} scenes within {:.3f}",
                 finalPool.size(), Settings::fMaxScaleDeviation);
  } else {
    logger::info("[SexLab NG] SearchScenes: Scale deviation fallback — keeping {} scenes",
                 finalPool.size());
  }

  std::sort(
      finalPool.begin(), finalPool.end(), [](const SceneCandidate& a, const SceneCandidate& b) {
        if (a.highCostScales != b.highCostScales)
          return a.highCostScales < b.highCostScales;
        if (a.totalScaleDeviation != b.totalScaleDeviation)
          return a.totalScaleDeviation < b.totalScaleDeviation;
        return a.scene && b.scene ? a.scene->GetName() < b.scene->GetName() : a.scene < b.scene;
      });

  SceneSearchResult result;
  result.reserve(finalPool.size());
  for (const auto& entry : finalPool) {
    if (!entry.scene)
      continue;
    result.emplace(entry.scene, entry.actorOrderCode);
  }

  return result;
}

std::uint64_t SceneManager::CreateInstance(std::vector<RE::Actor*> actors, SceneSearchResult scenes)
{
  std::lock_guard<std::mutex> lock(mapMutex);
  static std::mt19937_64 rng{std::random_device{}()};
  std::uint64_t id;
  do
    id = rng();
  while (id == 0 || sceneInstances.contains(id));
  sceneInstances.try_emplace(id, actors.front(),
                             std::vector<RE::Actor*>(actors.begin() + 1, actors.end()),
                             std::move(scenes));
  return id;
}

void SceneManager::DestroyInstance(std::uint64_t id)
{
  std::lock_guard<std::mutex> lock(mapMutex);
  sceneInstances.erase(id);
}

const SceneInstance* SceneManager::GetInstance(std::uint64_t id)
{
  std::lock_guard<std::mutex> lock(mapMutex);
  auto it = sceneInstances.find(id);
  return it == sceneInstances.end() ? nullptr : &it->second;
}

const SceneInstance* SceneManager::GetInstanceForActor(RE::Actor* actor)
{
  std::lock_guard<std::mutex> lock(mapMutex);
  for (const auto& [id, instance] : sceneInstances) {
    (void)id;
    const auto actors = instance.GetActors();
    if (std::find(actors.begin(), actors.end(), actor) != actors.end())
      return &instance;
  }
  return nullptr;
}

bool SceneManager::IsActorInScene(RE::Actor* actor)
{
  for (const auto& [id, instance] : sceneInstances) {
    auto actors = instance.GetActors();
    if (std::find(actors.begin(), actors.end(), actor) != actors.end())
      return true;
  }
  return false;
}

void SceneManager::UpdateScenes()
{
  std::lock_guard<std::mutex> lock(mapMutex);
  for (auto it = sceneInstances.begin(); it != sceneInstances.end();) {
    if (!it->second.Update()) {
      it = sceneInstances.erase(it);
    } else
      ++it;
  }
}
}  // namespace Instance
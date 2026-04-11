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

// ════════════════════════════════════════════════════════════
// Stage 0: Race + Gender 贪心匹配
//
// 在匹配的同时计算总 scale 偏差, 供后续 Stage 4 使用。
// 贪心策略: 按 scaleDev 升序分配, 使总偏差最小化。
// 返回: {是否全部匹配成功, 总 scale 偏差}
// ════════════════════════════════════════════════════════════
struct MatchResult
{
  bool matched        = false;
  float totalScaleDev = 0.f;
};

static MatchResult MatchActorsToPositions(const std::vector<RE::Actor*>& actors,
                                          const std::unordered_map<RE::Actor*, ActorInfo>& infos,
                                          std::vector<Define::Position>& positions)
{
  struct Candidate
  {
    std::size_t actorIdx;
    std::size_t posIdx;
    float scaleDev;
  };

  std::vector<Candidate> candidates;
  candidates.reserve(actors.size() * positions.size());

  for (std::size_t a = 0; a < actors.size(); ++a) {
    const auto& info = infos.at(actors[a]);
    for (std::size_t p = 0; p < positions.size(); ++p) {
      if (positions[p].GetRace() == info.race && positions[p].GetGender() == info.gender)
        candidates.push_back({a, p, std::abs(info.scale - positions[p].GetScale())});
    }
  }

  std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
    return a.scaleDev < b.scaleDev;
  });

  std::vector<bool> actorUsed(actors.size(), false);
  std::vector<bool> posUsed(positions.size(), false);
  float totalDev    = 0.f;
  std::size_t count = 0;

  for (const auto& c : candidates) {
    if (actorUsed[c.actorIdx] || posUsed[c.posIdx])
      continue;
    actorUsed[c.actorIdx] = true;
    posUsed[c.posIdx]     = true;
    totalDev += c.scaleDev;
    if (++count == actors.size())
      break;
  }

  return {count == actors.size(), totalDev};
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

std::vector<Define::Scene*> SceneManager::SearchScenes(std::vector<RE::Actor*> actors,
                                                       SearchOptions options)
{
  ScopeTimer timer("SceneManager::SearchScenes");

  for (auto* actor : actors)
    if (!actor)
      return {};

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
  // Stage 0: Race + Gender + 人数 + LeadIn 过滤
  // ══════════════════════════════════════════════════════════
  struct BaseEntry
  {
    Define::Scene* scene;
    float totalScaleDev;
  };

  std::vector<BaseEntry> basePool;

  for (auto& animPack : animPacks) {
    for (auto& scene : animPack.GetScenes()) {
      // LeadIn 过滤: SceneTags 中的 LeadIn 位
      bool isLeadIn = scene.GetTags().Has(Define::SceneTags::Type::LeadIn);
      if (options.useLeadIn != isLeadIn)
        continue;

      // Races 掩码快速排除
      if (!(scene.GetRaces() >= Define::Race(racesMask)))
        continue;

      // 人数匹配
      auto& positions = scene.GetPositions();
      if (positions.size() != actors.size())
        continue;

      // Race + Gender 一一对应 (贪心匹配, 同时算出 scaleDev)
      auto match = MatchActorsToPositions(actors, infos, positions);
      if (!match.matched)
        continue;

      basePool.push_back({&scene, match.totalScaleDev});
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
  auto sceneTagPool = [&]() -> std::vector<BaseEntry> {
    if (!hasSceneTagReq)
      return basePool;

    std::vector<BaseEntry> pool;
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
  auto interactPool = [&]() -> std::vector<BaseEntry> {
    if (!hasInteractTagReq)
      return sceneTagPool;

    std::vector<BaseEntry> pool;
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
  std::vector<BaseEntry> scaleStrict;
  for (auto& entry : interactPool)
    if (entry.totalScaleDev <= Settings::fMaxScaleDeviation)
      scaleStrict.push_back(entry);

  auto& finalPool = scaleStrict.empty() ? interactPool : scaleStrict;

  if (!scaleStrict.empty()) {
    logger::info("[SexLab NG] SearchScenes: Stage 3 (Scale) — {} / {} scenes pass",
                 scaleStrict.size(), interactPool.size());
  } else {
    logger::info("[SexLab NG] SearchScenes: Scale fallback — returning all {} scenes",
                 interactPool.size());
  }

  // 按 totalScaleDev 升序排序, scale 最契合的排前面
  std::sort(finalPool.begin(), finalPool.end(), [](const BaseEntry& a, const BaseEntry& b) {
    return a.totalScaleDev < b.totalScaleDev;
  });

  // 转换为结果
  std::vector<Define::Scene*> res;
  res.reserve(finalPool.size());
  for (auto& entry : finalPool)
    res.push_back(entry.scene);

  return res;
}

std::uint64_t SceneManager::CreateInstance(std::vector<RE::Actor*> actors,
                                           std::vector<Define::Scene*> scenes)
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

void SceneManager::UpdateScenes()
{
  std::lock_guard<std::mutex> lock(mapMutex);
  for (auto it = sceneInstances.begin(); it != sceneInstances.end();) {
    if (!it->second.Update())
      it = sceneInstances.erase(it);
    else
      ++it;
  }
}
}  // namespace Instance
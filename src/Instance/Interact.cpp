#include "Instance/Interact.h"

#include "magic_enum/magic_enum.hpp"

namespace Instance
{

using Name = Define::BodyPart::Name;
using BP   = Define::BodyPart;

// ═══════════════════════════════════════════════════════════════════════════
// §1  Rule 表
// ═══════════════════════════════════════════════════════════════════════════
// Key = {min(NameA, NameB), max(NameA, NameB)}，按枚举值升序拼接，保证唯一。
// 同一部位自交（Tribbing）用 {Vagina, Vagina}。
//
// Rule 字段：
//   type       — 映射到的 Interact::Type
//   radius     — 最大允许距离（world units）
//   maxAngle   — 方向约束（两部位 direction 之间的允许夹角绝对值，度）
//                360.f = 无约束（Point 类型或纯距离交互）
//   needFront  — 是否要求目标在本部位的 IsInFront() 一侧
//                （Belly 用于防止从身后触发 Naveljob）
//   needHoriz  — 是否要求 Penis 接近水平（Naveljob 防止竖直插入误判）

struct Rule
{
  Interact::Type type = Interact::Type::None;
  float radius        = 8.f;
  float maxAngle      = 360.f;
  bool needFront      = false;
  bool needHoriz      = false;
};

std::uint16_t operator|(Define::BodyPart::Name a, Define::BodyPart::Name b)
{
  const auto va = static_cast<std::uint16_t>(a);
  const auto vb = static_cast<std::uint16_t>(b);
  // 按枚举值升序排列
  if (va < vb)
    return vb | (va << 8);
  return va | (vb << 8);
}

// ── 规则表 ────────────────────────────────────────────────────────────────
// radius 单位与骨骼世界坐标一致（Skyrim ≈ 1 unit = 1 cm 量级）。
// maxAngle：
//   Kiss       两 Mouth 方向需近似反向（面对面）→ angle 接近 180°，但这里存的是
//              "与反向的允许偏差"，即我们检测 |angle| >= 180 - maxAngle。
//              为了统一语义，Kiss 的 maxAngle 存 30.f，检测时用 IsAntiAligned。
//   穿插类     penis 方向与 canal 方向需近似反向（penis 插入）→ IsAntiAligned + maxAngle。
//   摩擦类     纯距离，360.f。

static const std::unordered_map<std::uint16_t, Rule> rules{
    // ── Mouth ───────────────────────────────────────────────────────────────
    // Mouth(0) <-> Mouth(0)  →  Tribbing key {0,0}: Name::Mouth=0
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::Mouth,
     {Interact::Type::Kiss, 6.f, 30.f, false, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::FootLeft,
     {Interact::Type::ToeSucking, 8.f, 40.f, false, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::FootRight,
     {Interact::Type::ToeSucking, 8.f, 40.f, false, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::Vagina,
     {Interact::Type::Cunnilingus, 8.f, 50.f, false, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::Anus,
     {Interact::Type::Anilingus, 8.f, 50.f, false, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::Penis,
     {Interact::Type::Fellatio, 8.f, 45.f, false, false}},
    // ── Breast ──────────────────────────────────────────────────────────────
    {Define::BodyPart::Name::BreastLeft | Define::BodyPart::Name::HandLeft,
     {Interact::Type::GropeBreast, 7.f, 360.f, false, false}},
    {Define::BodyPart::Name::BreastLeft | Define::BodyPart::Name::Penis,
     {Interact::Type::Titfuck, 8.f, 60.f, false, false}},
    // ── Finger / Hand ───────────────────────────────────────────────────────
    {Define::BodyPart::Name::FingerLeft | Define::BodyPart::Name::Vagina,
     {Interact::Type::FingerVagina, 6.f, 50.f, false, false}},
    {Define::BodyPart::Name::FingerLeft | Define::BodyPart::Name::Anus,
     {Interact::Type::FingerAnus, 6.f, 50.f, false, false}},
    {Define::BodyPart::Name::HandLeft | Define::BodyPart::Name::Penis,
     {Interact::Type::Handjob, 7.f, 50.f, false, false}},
    // ── Belly ───────────────────────────────────────────────────────────────
    // needFront=true: penis tip must be in front of actor (IsInFront on Belly)
    // needHoriz=true: penis must be roughly horizontal
    {Define::BodyPart::Name::Belly | Define::BodyPart::Name::Penis,
     {Interact::Type::Naveljob, 10.f, 60.f, true, true}},
    // ── Thigh ───────────────────────────────────────────────────────────────
    {Define::BodyPart::Name::ThighLeft | Define::BodyPart::Name::Penis,
     {Interact::Type::Thighjob, 9.f, 50.f, false, false}},
    // ── Butt ────────────────────────────────────────────────────────────────
    {Define::BodyPart::Name::ButtLeft | Define::BodyPart::Name::Penis,
     {Interact::Type::Frottage, 8.f, 360.f, false, false}},
    // ── Foot ────────────────────────────────────────────────────────────────
    {Define::BodyPart::Name::FootLeft | Define::BodyPart::Name::Penis,
     {Interact::Type::Footjob, 8.f, 360.f, false, false}},
    // ── Vagina ──────────────────────────────────────────────────────────────
    {Define::BodyPart::Name::Vagina | Define::BodyPart::Name::Vagina,
     {Interact::Type::Tribbing, 8.f, 30.f, false, false}},
    // Vaginal: penis 方向与 vagina 方向近似反向（插入）
    {Define::BodyPart::Name::Vagina | Define::BodyPart::Name::Penis,
     {Interact::Type::Vaginal, 8.f, 35.f, false, false}},
    // ── Anus ────────────────────────────────────────────────────────────────
    {Define::BodyPart::Name::Anus | Define::BodyPart::Name::Penis,
     {Interact::Type::Anal, 6.f, 35.f, false, false}},
};

const Rule& GetRule(Define::BodyPart::Name a, Define::BodyPart::Name b)
{
  static Rule empty = {Interact::Type::None, 0.f, 360.f, false, false};

  auto normalize = [](Define::BodyPart::Name name) {
    switch (name) {
    case Define::BodyPart::Name::BreastRight:
      return Define::BodyPart::Name::BreastLeft;
    case Define::BodyPart::Name::HandRight:
      return Define::BodyPart::Name::HandLeft;
    case Define::BodyPart::Name::FingerRight:
      return Define::BodyPart::Name::FingerLeft;
    case Define::BodyPart::Name::ThighRight:
      return Define::BodyPart::Name::ThighLeft;
    case Define::BodyPart::Name::ButtRight:
      return Define::BodyPart::Name::ButtLeft;
    case Define::BodyPart::Name::FootRight:
      return Define::BodyPart::Name::FootLeft;
    default:
      return name;
    }
  };
  auto na = normalize(a);
  auto nb = normalize(b);
  if (auto it = rules.find(na | nb); it != rules.end())
    return it->second;
  return empty;  // default rule
}

// ── 角度检测语义说明 ──────────────────────────────────────────────────────
// 不同交互的 maxAngle 语义不同，由 CheckAngle() 根据类型选择：
//
//   Kiss / Tribbing          → IsAntiAligned（两方向夹角 >= 180 - maxAngle）
//                              即两者面对面 / 方向相反
//   Vaginal / Anal /         → IsAntiAligned（penis 方向与 canal 方向近似反向）
//     Fellatio / Cunnilingus    penis.direction ≈ -canal.direction 时为插入
//   Anilingus                → IsAntiAligned（同 Cunnilingus）
//   ToeSucking               → IsAligned（mouth 朝向 toe 方向）
//   GropeBreast / Footjob /  → 无角度约束（360.f）
//     Frottage / Handjob
//   Naveljob                 → IsAligned（penis 方向与 belly 朝外方向近似对齐，
//                              即 penis 尖端朝向腹部）+ needFront + needHoriz
//   Thighjob / Titfuck       → IsAligned（penis 大致平行于对应部位方向）

static const std::unordered_map<Interact::Type, bool> kUseAntiAligned{
    {Interact::Type::Kiss, true},        {Interact::Type::Tribbing, true},
    {Interact::Type::Cunnilingus, true}, {Interact::Type::Anilingus, true},
    {Interact::Type::Fellatio, true},    {Interact::Type::DeepThroat, true},
    {Interact::Type::Vaginal, true},     {Interact::Type::Anal, true},
};

namespace
{

  struct BPPair
  {
    const BP* a;
    const BP* b;
    bool operator==(const BPPair& o) const { return a == o.a && b == o.b; }
  };
  struct BPPairHash
  {
    std::size_t operator()(const BPPair& k) const
    {
      return std::hash<const BP*>{}(k.a) ^ (std::hash<const BP*>{}(k.b) << 16);
    }
  };

  using DistCache = std::unordered_map<BPPair, float, BPPairHash>;

  BPPair MakeBPKey(const BP& x, const BP& y)
  {
    return (&x <= &y) ? BPPair{&x, &y} : BPPair{&y, &x};
  }

  float CachedDistance(DistCache& cache, const BP& a, const BP& b)
  {
    auto key = MakeBPKey(a, b);
    if (auto it = cache.find(key); it != cache.end())
      return it->second;
    const float d = a.Distance(b);
    cache[key]    = d;
    return d;
  }

}  // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════
// §4  角度判断辅助
// ═══════════════════════════════════════════════════════════════════════════

namespace
{

  // 判断两个 BodyPart 的方向是否满足规则的角度约束。
  // antiAligned=true : 两方向夹角绝对值 >= (180 - maxAngle)（近似反向）
  // antiAligned=false: 两方向夹角绝对值 <=  maxAngle          （近似对齐）
  // maxAngle=360 : 始终返回 true（无约束）。
  bool CheckAngle(const BP& a, const BP& b, float maxAngle, bool antiAligned)
  {
    if (maxAngle >= 359.f)
      return true;

    // Angle() 返回有符号值 (-180, 180]，取绝对值得到 [0, 180]
    const float absAngle = std::abs(a.Angle(b));

    if (antiAligned)
      return absAngle >= (180.f - maxAngle);
    else
      return absAngle <= maxAngle;
  }

  // 附加几何约束：needFront 和 needHoriz（仅 Naveljob 用）
  // partA = Belly（提供 IsInFront 语义）
  // partB = Penis（提供 IsHorizontal 语义）
  bool CheckExtra(const BP& partA, const BP& partB, bool needFront, bool needHoriz)
  {
    if (needFront && !partA.IsInFront(partB.GetEnd()))
      return false;
    if (needHoriz && !partB.IsHorizontal(30.f))
      return false;
    return true;
  }

}  // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════
// §5  候选结构（Update 内部临时数据）
// ═══════════════════════════════════════════════════════════════════════════

namespace
{

  // 一个候选交互结果：某 actor 的某部位与另一 actor 的某部位
  struct Candidate
  {
    RE::Actor* actor    = nullptr;
    Name partSelf       = Name::Mouth;  // 本 actor 的部位
    Name partOther      = Name::Mouth;  // 对方部位
    float distance      = 10000.0f;
    Interact::Type type = Interact::Type::None;
  };

}  // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════
// §6  构造 / FlashNodeData
// ═══════════════════════════════════════════════════════════════════════════

Interact::Interact(std::vector<RE::Actor*> actors)
{
  for (auto* actor : actors) {
    if (!actor)
      continue;
    ActorData data;
    data.race   = Define::Race(actor);
    data.gender = Define::Gender(actor);

    // 为该 actor 创建它拥有的所有 BodyPart 实例
    for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(Name::Penis) + 1; ++i) {
      const auto n = static_cast<Name>(i);
      if (!BP::HasBodyPart(data.gender, data.race, n))
        continue;
      data.infos.emplace(n, Info{BP{actor, data.race, n}});
    }
    datas.emplace(actor, std::move(data));
  }
}

void Interact::FlashNodeData()
{
  for (auto& [actor, data] : datas)
    for (auto& [name, info] : data.infos)
      info.bodypart.UpdateNodes();
}

// ═══════════════════════════════════════════════════════════════════════════
// §7  Update
// ═════════════════════════════════���═════════════════════════════════════════
//
// Phase 0 — 滚动历史帧（prevType ← type，prevActor ← actor），重置当前帧
// Phase 1 — 更新所有 BodyPart 位置（UpdatePosition）
// Phase 2 — 遍历所有 actor 对，为每个有效 NamePair 查找 Rule，计算距离并缓存，
//            建立 Candidate 列表（每个部位保留距离最近的候选）
// Phase 3 — 抢占仲裁：若同一 partSelf 被多个 actor 同时满足，按距离 + 上帧结果决定归属
// Phase 4 — 将候选写回 Info，计算 velocity

void Interact::Update()
{
  // ── Phase 0: 滚动历史 ────────────────────────────────────────────────────
  for (auto& [actor, data] : datas) {
    for (auto& [name, info] : data.infos) {
      info.prevType     = info.type;
      info.prevActor    = info.actor;
      info.prevDistance = info.distance;
      info.type         = Interact::Type::None;
      info.actor        = nullptr;
      info.distance     = 0.f;
    }
  }

  // ── Phase 1: 更新位置 ────────────────────────────────────────────────────
  for (auto& [actor, data] : datas)
    for (auto& [name, info] : data.infos)
      info.bodypart.UpdatePosition();

  // ── Phase 2: 遍历 actor 对，建立候选 ────────────────────────────────────
  // candidates[actor][partName] = 当前最优 Candidate
  std::unordered_map<RE::Actor*, std::unordered_map<Name, Candidate>> candidates;

  DistCache distCache;

  // 收集所有 actor 指针（有序，避免 (A,B) 和 (B,A) 重复处理）
  std::vector<RE::Actor*> actorList;
  actorList.reserve(datas.size());
  for (auto& [a, _] : datas)
    actorList.push_back(a);

  for (std::size_t i = 0; i < actorList.size(); ++i) {
    for (std::size_t j = i; j < actorList.size(); ++j) {
      if (i == j)
        continue;

      RE::Actor* actA = actorList[i];
      RE::Actor* actB = actorList[j];
      ActorData& dA   = datas[actA];
      ActorData& dB   = datas[actB];

      // 遍历 actA 的每个部位
      for (auto& [nameA, infoA] : dA.infos) {
        if (!infoA.bodypart.IsValid())
          continue;

        // 遍历 actB 的每个部位
        for (auto& [nameB, infoB] : dB.infos) {
          if (!infoB.bodypart.IsValid())
            continue;

          // 同一 actor 同一部位跳过
          if (actA == actB && nameA == nameB)
            continue;

          // 查找规则（key 按枚举值升序）
          auto rule = GetRule(nameA, nameB);

          // 距离检测（缓存）
          const float dist = CachedDistance(distCache, infoA.bodypart, infoB.bodypart);
          if (dist > rule.radius)
            continue;

          // 角度检测
          const bool antiAligned = kUseAntiAligned.count(rule.type) > 0;
          if (!CheckAngle(infoA.bodypart, infoB.bodypart, rule.maxAngle, antiAligned))
            continue;

          // 附加约束（Naveljob: front + horiz）
          // needFront / needHoriz 总是针对 Belly（infoA.bodypart 或 infoB.bodypart 中哪个是 Belly）
          // 和 Penis，确定好顺序后调用 CheckExtra
          {
            const BP* bellyPart = (nameA == Name::Belly)   ? &infoA.bodypart
                                  : (nameB == Name::Belly) ? &infoB.bodypart
                                                           : nullptr;
            const BP* penisPart = (nameA == Name::Penis)   ? &infoA.bodypart
                                  : (nameB == Name::Penis) ? &infoB.bodypart
                                                           : nullptr;
            if ((rule.needFront || rule.needHoriz) && bellyPart && penisPart) {
              if (!CheckExtra(*bellyPart, *penisPart, rule.needFront, rule.needHoriz))
                continue;
            }
          }

          // 更新 actA 侧的候选（若比当前更近）
          auto& candA = candidates[actA][nameA];
          if (dist < candA.distance)
            candA = {actB, nameA, nameB, dist, rule.type};

          // 更新 actB 侧的候选（若 actA != actB，对称写入）
          if (actA != actB) {
            auto& candB = candidates[actB][nameB];
            if (dist < candB.distance)
              candB = {actA, nameB, nameA, dist, rule.type};
          }
        }
      }
    }
  }

  // ── Phase 3: 抢占仲裁 ────────────────────────────────────────────────────
  // 如果多个 actor 争夺同一目标部位，较远的一方需要重新寻找候选（跳过胜方）。
  //
  // 做法：
  //   对每个 (actor, part)，检查对方 actor 是否也把 part 的对应部位候选指向了
  //   不同的第三方。若存在竞争（两个 actorX 同时指向 actorY.partY），
  //   则按距离选胜者，败者重新在候选列表中找次优（排除胜者），若无次优则置空。
  //
  // 注：当前场景一般 2~4 人，简单循环代价可接受。
  //
  // 竞争定义：两个不同的 actorX、actorZ 的 candidates 中，都以
  //   actorY.nameY 作为目标（即 cand.actor == actorY && cand.partOther == nameY）
  // 这种情况较少（需要两个 penis 争一个 vagina），暂时用简单的"保留距离最近者"策略。

  // 构建反向索引：target(actor, partName) -> list of (sourceActor, sourcePartName, dist)
  using TargetKey = std::pair<RE::Actor*, Name>;
  struct TargetKeyHash
  {
    std::size_t operator()(const TargetKey& k) const
    {
      return std::hash<RE::Actor*>{}(k.first) ^ (static_cast<std::size_t>(k.second) << 8);
    }
  };
  struct ClaimEntry
  {
    RE::Actor* srcActor;
    Name srcPart;
    float dist;
  };
  std::unordered_map<TargetKey, std::vector<ClaimEntry>, TargetKeyHash> claims;

  for (auto& [actor, partMap] : candidates) {
    for (auto& [partName, cand] : partMap) {
      if (!cand.actor)
        continue;
      claims[{cand.actor, cand.partOther}].push_back({actor, partName, cand.distance});
    }
  }

  // 对每个被争夺的目标，只保留距离最近的��请者
  for (auto& [target, claimList] : claims) {
    if (claimList.size() <= 1)
      continue;

    // 排序，最近的排前
    std::sort(claimList.begin(), claimList.end(), [](const ClaimEntry& x, const ClaimEntry& y) {
      return x.dist < y.dist;
    });

    // 胜者保留；败者将候选置空（暂不重新搜索，可扩展）
    for (std::size_t k = 1; k < claimList.size(); ++k) {
      auto& loser = candidates[claimList[k].srcActor][claimList[k].srcPart];
      loser       = {};  // 置空
    }
  }

  // ── Phase 4: 写回 Info，计算 velocity ───────────────────────────────────
  const float nowMs = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(
                                             std::chrono::steady_clock::now().time_since_epoch())
                                             .count()) /
                      1000.f;

  for (auto& [actor, data] : datas) {
    const float dt    = nowMs - data.lastUpdateMs;
    data.lastUpdateMs = nowMs;

    for (auto& [partName, info] : data.infos) {
      auto it = candidates.find(actor);
      if (it == candidates.end())
        continue;
      auto it2 = it->second.find(partName);
      if (it2 == it->second.end() || !it2->second.actor)
        continue;

      const Candidate& cand = it2->second;

      // DeepThroat 升级：Fellatio 满足且 penis.start 在 mouth 前方
      Interact::Type resolvedType = cand.type;
      if (cand.type == Interact::Type::Fellatio) {
        const BP& mouth = data.infos.at(Name::Mouth).bodypart;
        const BP& penis = datas.at(cand.actor).infos.at(Name::Penis).bodypart;
        // penis の root (start) が mouth の前方にある = deep throat
        if (mouth.IsInFront(penis.GetStart()))
          resolvedType = Interact::Type::DeepThroat;
      }

      info.actor    = cand.actor;
      info.distance = cand.distance;
      info.type     = resolvedType;
      info.velocity = (dt > 1e-4f) ? (cand.distance - info.prevDistance) / dt : 0.f;
    }
  }

  // Debug 输出
  for (auto& [actor, data] : datas) {
    for (auto& [partName, info] : data.infos) {
      // 输出每个部位的交互信息
      if (!info.actor || info.type == Interact::Type::None)
        continue;

      if (info.type == info.prevType && info.actor == info.prevActor)
        continue;

      logger::info(
          "Actor: {}, Part: {}, Info: {{ actor: {}, distance: {}, velocity: {}, type: {} }}",
          actor->GetName(), magic_enum::enum_name(partName),
          info.actor ? info.actor->GetName() : "None", info.distance, info.velocity,
          magic_enum::enum_name(info.type));
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// §8  查询接口
// ═══════════════════════════════════════════════════════════════════════════

const Interact::Info& Interact::GetInfo(RE::Actor* actor, Name partName) const
{
  static const Info kEmpty{};
  auto it = datas.find(actor);
  if (it == datas.end())
    return kEmpty;
  auto it2 = it->second.infos.find(partName);
  if (it2 == it->second.infos.end())
    return kEmpty;
  return it2->second;
}

}  // namespace Instance
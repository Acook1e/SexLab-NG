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
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::Mouth,
     {Interact::Type::Kiss, 6.f, 360.f, false, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::BreastLeft,
     {Interact::Type::BreastSucking, 8.f, 360.f, false, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::FootLeft,
     {Interact::Type::ToeSucking, 8.f, 360.f, false, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::FootRight,
     {Interact::Type::ToeSucking, 8.f, 360.f, false, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::Vagina,
     {Interact::Type::Cunnilingus, 8.f, 360.f, false, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::Anus,
     {Interact::Type::Anilingus, 8.f, 360.f, false, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::Penis,
     {Interact::Type::Fellatio, 8.f, 360.f, false, false}},
    // ── Breast ──────────────────────────────────────────────────────────────
    {Define::BodyPart::Name::BreastLeft | Define::BodyPart::Name::HandLeft,
     {Interact::Type::GropeBreast, 7.f, 360.f, false, false}},
    {Define::BodyPart::Name::BreastLeft | Define::BodyPart::Name::Penis,
     {Interact::Type::Titfuck, 8.f, 60.f, false, false}},
    // ── Finger / Hand ───────────────────────────────────────────────────────
    // FingerVagina/FingerAnus: 手指方向（根→尖）与管道朝外方向近似反向（插入）
    {Define::BodyPart::Name::FingerLeft | Define::BodyPart::Name::Vagina,
     {Interact::Type::FingerVagina, 6.f, 50.f, false, false}},
    {Define::BodyPart::Name::FingerLeft | Define::BodyPart::Name::Anus,
     {Interact::Type::FingerAnus, 6.f, 50.f, false, false}},
    {Define::BodyPart::Name::HandLeft | Define::BodyPart::Name::Penis,
     {Interact::Type::Handjob, 7.f, 360.f, false, false}},
    // ── Belly ───────────────────────────────────────────────────────────────
    // needFront=true: penis tip must be in front of actor (IsInFront on Belly)
    // needHoriz=true: penis must be roughly horizontal
    {Define::BodyPart::Name::Belly | Define::BodyPart::Name::Penis,
     {Interact::Type::Naveljob, 10.f, 60.f, true, true}},
    // ── Thigh ───────────────────────────────────────────────────────────────
    {Define::BodyPart::Name::ThighLeft | Define::BodyPart::Name::Penis,
     {Interact::Type::Thighjob, 4.f, 50.f, false, false}},
    // ── Butt ────────────────────────────────────────────────────────────────
    {Define::BodyPart::Name::GlutealCleft | Define::BodyPart::Name::Penis,
     {Interact::Type::Frottage, 4.f, 45.f, false, false}},
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
     {Interact::Type::Anal, 7.f, 35.f, false, false}},
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
//   Kiss / Tribbing                      → IsAntiAligned（两方向夹角 >= 180 - maxAngle）
//                                           即两者面对面 / 方向相反
//   Vaginal / Anal / Fellatio            → IsAntiAligned（penis 方向与 canal 方向近似反向）
//     Cunnilingus / Anilingus               penis.direction ≈ -canal.direction 时为插入
//   BreastSucking                        → IsAntiAligned（mouth 朝向乳头方向 vs 乳房方向）
//   FingerVagina / FingerAnus            → IsAntiAligned（手指方向与管道朝外方向近似反向）
//   ToeSucking                           → IsAligned（mouth 朝向 toe 方向）
//   GropeBreast / Footjob / Frottage /   → 无角度约束（360.f）
//     Handjob
//   Naveljob                             → IsAligned（penis 方向与 belly 朝外方向近似对齐）
//                                           + needFront + needHoriz
//   Thighjob / Titfuck                   → IsAligned（penis 大致平行于对应部位方向）

static const std::unordered_map<Interact::Type, bool> kUseAntiAligned{
    {Interact::Type::FingerVagina, true},
    {Interact::Type::FingerAnus, true},
    {Interact::Type::Frottage, true},
    {Interact::Type::Tribbing, true},
};

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
// §5  DistEntry（Phase 2 距离条目）
// ═══════════════════════════════════════════════════════════════════════════

namespace
{

  // 跨 actor 部位对的距离记录，用于 Phase 2 排序后的贪心分配
  struct DistEntry
  {
    RE::Actor* actorA;
    Name nameA;
    RE::Actor* actorB;
    Name nameB;
    float distance;
    Rule rule;  // 规则副本
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
// ═══════════════════════════════════════════════════════════════════════════
//
// Phase 0 — 滚动历史帧（prevType ← type，prevActor ← actor），重置当前帧
// Phase 1 — 更新所有 BodyPart 位置（UpdatePosition）
// Phase 2 — 计算所有 actor 对（含自交互）的有规则部位对距离，构建 DistEntry 并按距离升序排序
// Phase 3 — 贪心分配：从近到远遍历 DistEntry，对每个条目检查约束并分配；
//            每个 (actor, part) 只参与一个交互；自交互升级为 Masturbation
// Phase 4 — DeepThroat 升级：Fellatio 满足且 penis.start 在 mouth 前方
// Phase 5 — 组合交互检测：SixtyNine / Spitroast / DoublePenetration / TriplePenetration
// Phase 6 — 写回 Info，计算 velocity
// Phase 7 — Debug 输出（含 combo types）

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

  // ── Phase 2: 计算所有跨 actor 距离，按距离排序 ───────────────────────────
  // 包括自交互（同一 actor 的不同部位），用于检测 Masturbation。
  std::vector<DistEntry> entries;

  std::vector<RE::Actor*> actorList;
  actorList.reserve(datas.size());
  for (auto& [a, _] : datas)
    actorList.push_back(a);

  for (std::size_t i = 0; i < actorList.size(); ++i) {
    for (std::size_t j = i; j < actorList.size(); ++j) {
      RE::Actor* actA = actorList[i];
      RE::Actor* actB = actorList[j];
      ActorData& dA   = datas[actA];
      ActorData& dB   = datas[actB];

      for (auto& [nameA, infoA] : dA.infos) {
        if (!infoA.bodypart.IsValid())
          continue;

        for (auto& [nameB, infoB] : dB.infos) {
          if (!infoB.bodypart.IsValid())
            continue;

          // 同一 actor 同一部位跳过
          if (actA == actB && nameA == nameB)
            continue;

          // 预过滤：只处理有规则的部位对
          const Rule& rule = GetRule(nameA, nameB);
          if (rule.type == Interact::Type::None)
            continue;

          const float dist = infoA.bodypart.Distance(infoB.bodypart);
          entries.push_back({actA, nameA, actB, nameB, dist, rule});
        }
      }
    }
  }

  // 按距离升序排序，保证贪心分配优先处理最近的配对
  std::sort(entries.begin(), entries.end(), [](const DistEntry& x, const DistEntry& y) {
    return x.distance < y.distance;
  });

  // ── Phase 3: 贪心分配（从近到远） ────────────────────────────────────────
  // usedParts 记录已被分配的 (actor, partName) 对
  using PartKey = std::pair<RE::Actor*, Name>;
  struct PartKeyHash
  {
    std::size_t operator()(const PartKey& k) const
    {
      return std::hash<RE::Actor*>{}(k.first) ^ (static_cast<std::size_t>(k.second) << 8);
    }
  };
  std::unordered_set<PartKey, PartKeyHash> usedParts;

  // 分配结果：actor -> partName -> {partner, partSelf, partPartner, distance, type}
  struct AssignInfo
  {
    RE::Actor* partner;
    Name partSelf;
    Name partPartner;
    float distance;
    Interact::Type type;
  };
  std::unordered_map<RE::Actor*, std::unordered_map<Name, AssignInfo>> assigns;

  for (const DistEntry& e : entries) {
    // 距离超出规则半径则跳过（不能 break，因为不同规则半径不同）
    if (e.distance > e.rule.radius)
      continue;

    // 任意一侧已被占用则跳过
    if (usedParts.count({e.actorA, e.nameA}) || usedParts.count({e.actorB, e.nameB}))
      continue;

    // 角度约束
    const bool antiAligned = kUseAntiAligned.count(e.rule.type) > 0;
    const BP& bpA          = datas[e.actorA].infos[e.nameA].bodypart;
    const BP& bpB          = datas[e.actorB].infos[e.nameB].bodypart;
    if (!CheckAngle(bpA, bpB, e.rule.maxAngle, antiAligned))
      continue;

    // 附加约束（Naveljob: front + horiz）
    if (e.rule.needFront || e.rule.needHoriz) {
      const BP* bellyPart = (e.nameA == Name::Belly)   ? &bpA
                            : (e.nameB == Name::Belly) ? &bpB
                                                       : nullptr;
      const BP* penisPart = (e.nameA == Name::Penis)   ? &bpA
                            : (e.nameB == Name::Penis) ? &bpB
                                                       : nullptr;
      if (bellyPart && penisPart) {
        if (!CheckExtra(*bellyPart, *penisPart, e.rule.needFront, e.rule.needHoriz))
          continue;
      }
    }

    // 自交互升级为 Masturbation
    Interact::Type resolvedType = e.rule.type;
    if (e.actorA == e.actorB) {
      if (resolvedType == Interact::Type::FingerVagina ||
          resolvedType == Interact::Type::FingerAnus || resolvedType == Interact::Type::Handjob) {
        resolvedType = Interact::Type::Masturbation;
      }
    }

    // 分配双侧
    assigns[e.actorA][e.nameA] = {e.actorB, e.nameA, e.nameB, e.distance, resolvedType};
    usedParts.insert({e.actorA, e.nameA});

    assigns[e.actorB][e.nameB] = {e.actorA, e.nameB, e.nameA, e.distance, resolvedType};
    usedParts.insert({e.actorB, e.nameB});
  }

  // ── Phase 4: DeepThroat 升级 ─────────────────────────────────────────────
  // Fellatio → DeepThroat：penis.start 在 mouth 的前方（penis 插入更深）
  for (auto& [actor, partMap] : assigns) {
    auto itMouth = partMap.find(Name::Mouth);
    if (itMouth == partMap.end() || itMouth->second.type != Interact::Type::Fellatio)
      continue;

    AssignInfo& mouthAsn = itMouth->second;

    auto itMouthBP = datas[actor].infos.find(Name::Mouth);
    auto itPenisBP = datas[mouthAsn.partner].infos.find(Name::Penis);
    if (itMouthBP == datas[actor].infos.end() || itPenisBP == datas[mouthAsn.partner].infos.end())
      continue;

    const BP& mouth = itMouthBP->second.bodypart;
    const BP& penis = itPenisBP->second.bodypart;

    if (mouth.IsInFront(penis.GetStart()))
      mouthAsn.type = Interact::Type::DeepThroat;
  }

  // ── Phase 5: 组合交互检测 ────────────────────────────────────────────────
  struct ActorInteractSummary
  {
    bool hasOral              = false;  // Mouth 接受 Fellatio/DeepThroat（被口交）
    bool givesOral            = false;  // Mouth 给对方 Cunnilingus/Anilingus
    bool hasVaginal           = false;  // Vagina 参与 Vaginal
    bool hasAnal              = false;  // Anus 参与 Anal
    RE::Actor* oralPartner    = nullptr;
    RE::Actor* vaginalPartner = nullptr;
    RE::Actor* analPartner    = nullptr;
    RE::Actor* givesOralTo    = nullptr;
  };

  std::unordered_map<RE::Actor*, ActorInteractSummary> summaries;

  for (auto& [actor, partMap] : assigns) {
    ActorInteractSummary& s = summaries[actor];
    for (auto& [partName, asn] : partMap) {
      switch (asn.type) {
      case Interact::Type::Fellatio:
      case Interact::Type::DeepThroat:
        if (partName == Name::Mouth) {
          s.hasOral     = true;
          s.oralPartner = asn.partner;
        }
        break;
      case Interact::Type::Cunnilingus:
      case Interact::Type::Anilingus:
        if (partName == Name::Mouth) {
          s.givesOral   = true;
          s.givesOralTo = asn.partner;
        }
        break;
      case Interact::Type::Vaginal:
        if (partName == Name::Vagina) {
          s.hasVaginal     = true;
          s.vaginalPartner = asn.partner;
        }
        break;
      case Interact::Type::Anal:
        if (partName == Name::Anus) {
          s.hasAnal     = true;
          s.analPartner = asn.partner;
        }
        break;
      default:
        break;
      }
    }
  }

  // comboTypes 存储每个 actor 检测到的组合类型，用于 debug 输出
  std::unordered_map<RE::Actor*, std::vector<Interact::Type>> comboTypes;

  for (auto& [actor, s] : summaries) {
    // SixtyNine：自己给对方口交，且对方也给自己口（Fellatio/DeepThroat 或 Cunnilingus/Anilingus）
    if (s.givesOral && s.givesOralTo) {
      auto it = summaries.find(s.givesOralTo);
      if (it != summaries.end()) {
        const ActorInteractSummary& ps = it->second;
        if ((ps.givesOral && ps.givesOralTo == actor) || (ps.hasOral && ps.oralPartner == actor)) {
          comboTypes[actor].push_back(Interact::Type::SixtyNine);
        }
      }
    }

    // DoublePenetration：同时有 Vaginal 和 Anal，来自不同 partner
    if (s.hasVaginal && s.hasAnal && s.vaginalPartner != s.analPartner)
      comboTypes[actor].push_back(Interact::Type::DoublePenetration);

    // TriplePenetration：Oral（被插）+ Vaginal + Anal，三者 partner 各不同
    if (s.hasOral && s.hasVaginal && s.hasAnal && s.oralPartner != s.vaginalPartner &&
        s.oralPartner != s.analPartner && s.vaginalPartner != s.analPartner) {
      comboTypes[actor].push_back(Interact::Type::TriplePenetration);
    }

    // Spitroast：被口交（Fellatio/DeepThroat）同时有 Vaginal 或 Anal，来自不同 partner
    if (s.hasOral && (s.hasVaginal || s.hasAnal)) {
      const bool diffVaginal = s.hasVaginal && s.oralPartner != s.vaginalPartner;
      const bool diffAnal    = s.hasAnal && s.oralPartner != s.analPartner;
      if (diffVaginal || diffAnal)
        comboTypes[actor].push_back(Interact::Type::Spitroast);
    }
  }

  // ── Phase 6: 写回 Info，计算 velocity ───────────────────────────────────
  const float nowMs = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(
                                             std::chrono::steady_clock::now().time_since_epoch())
                                             .count()) /
                      1000.f;

  for (auto& [actor, data] : datas) {
    const float dt    = nowMs - data.lastUpdateMs;
    data.lastUpdateMs = nowMs;

    for (auto& [partName, info] : data.infos) {
      auto it = assigns.find(actor);
      if (it == assigns.end())
        continue;
      auto it2 = it->second.find(partName);
      if (it2 == it->second.end())
        continue;

      const AssignInfo& asn = it2->second;
      info.actor            = asn.partner;
      info.distance         = asn.distance;
      info.type             = asn.type;
      info.velocity         = (dt > 1e-4f) ? (asn.distance - info.prevDistance) / dt : 0.f;
    }
  }

  // ── Phase 7: Debug 输出 ──────────────────────────────────────────────────
  for (auto& [actor, data] : datas) {
    for (auto& [partName, info] : data.infos) {
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

    // Combo types
    auto itCombo = comboTypes.find(actor);
    if (itCombo != comboTypes.end()) {
      for (const auto& combo : itCombo->second)
        logger::info("Actor: {}, Combo: {}", actor->GetName(), magic_enum::enum_name(combo));
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

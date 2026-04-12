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
//   min/max    — 方向约束（两部位 direction 之间的绝对夹角区间，度）
//   needFront  — 是否要求目标在本部位的 IsInFront() 一侧
//                （Belly 用于防止从身后触发 Naveljob）
//   needHoriz  — 是否要求 Penis 接近水平（Naveljob 防止竖直插入误判）

struct Rule
{
  Interact::Type type = Interact::Type::None;
  float radius        = 8.f;
  float minAngle      = 0.f;    // 角度区间下限 [0, 180]
  float maxAngle      = 180.f;  // 角度区间上限 [0, 180]
                                // minAngle <= maxAngle: 角度必须在 [min, max] 内
  // minAngle >  maxAngle: 卷绕区间 [0, max]∪[min, 180]（不区分同向/反向）
  bool needFront      = false;  // See Belly/Naveljob
  bool needHorizontal = false;  // See Belly/Naveljob
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
// {type, radius, minAngle, maxAngle, needFront}
// 角度约定（绝对值 [0,180]）：
//   [0,  x]      — 近似同向（对齐），x 越小越严格
//   [x, 180]     — 近似反向（面对面/插入方向相反），x 越大越严格
//   [a,  b]      — 近似垂直（±偏差），如 [70,110] = 垂直 ±20°
//   [b,  a](b>a) — 换绕区间 [0,a]∪[b,180]，同/反向均可，排除近垂直
//                  如 [130,50] = ≤50° 或 ≥130°
//   [0, 180]     — 无约束

static const std::unordered_map<std::uint16_t, Rule> rules{
    // ── Mouth ───────────────────────────────────────────────────────────────
    // Kiss: Mouth00→Head 双方面对面 → 反向 [140,180]
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::Mouth,
     {Interact::Type::Kiss, 6.f, 145.f, 180.f, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::BreastLeft,
     {Interact::Type::BreastSucking, 7.f, 0.f, 180.f, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::FootLeft,
     {Interact::Type::ToeSucking, 7.f, 0.f, 180.f, false}},
    // Cunnilingus 优先走外部 vulva/clit 轴线；保留 Vagina 作为更深 oral 接触兜底
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::ThighCleft,
     {Interact::Type::Cunnilingus, 6.f, 0.f, 180.f, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::Vagina,
     {Interact::Type::Cunnilingus, 6.5f, 0.f, 180.f, false}},
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::Anus,
     {Interact::Type::Anilingus, 6.f, 0.f, 180.f, false}},
    // Fellatio: Mouth00→Head 与 Penis root→tip 同向 → 对齐 [0,50]
    {Define::BodyPart::Name::Mouth | Define::BodyPart::Name::Penis,
     {Interact::Type::Fellatio, 7.f, 0.f, 35.f, false}},
    // ── Throat ──────────────────────────────────────────────────────────────
    // DeepThroat: Head→Neck 与 Penis 同向 → 对齐 [0,40]
    {Define::BodyPart::Name::Throat | Define::BodyPart::Name::Penis,
     {Interact::Type::DeepThroat, 6.f, 0.f, 28.f, false}},
    // ── Breast ──────────────────────────────────────────────────────────────
    // 手掌向量必须与乳房向量反向
    {Define::BodyPart::Name::BreastLeft | Define::BodyPart::Name::HandLeft,
     {Interact::Type::GropeBreast, 5.f, 150.f, 180.f, false}},
    {Define::BodyPart::Name::HandLeft | Define::BodyPart::Name::ThighLeft,
     {Interact::Type::GropeThigh, 5.5f, 0.f, 180.f, false}},
    {Define::BodyPart::Name::HandLeft | Define::BodyPart::Name::ButtLeft,
     {Interact::Type::GropeButt, 4.5f, 0.f, 180.f, false}},
    {Define::BodyPart::Name::HandLeft | Define::BodyPart::Name::FootLeft,
     {Interact::Type::GropeFoot, 5.5f, 0.f, 180.f, false}},
    // Titfuck via BreastLeft: penis 平行于乳房轴向 → 对齐 [0,60]
    {Define::BodyPart::Name::BreastLeft | Define::BodyPart::Name::Penis,
     {Interact::Type::Titfuck, 7.f, 0.f, 50.f, false}},
    // Titfuck via Cleavage: Cleavage 朝外法向 vs penis 垂直滑入 → 垂直 [70,110]
    {Define::BodyPart::Name::Cleavage | Define::BodyPart::Name::Penis,
     {Interact::Type::Titfuck, 7.f, 75.f, 105.f, false}},
    // ── Finger / Hand ───────────────────────────────────────────────────────
    // FingerVagina: 外部刺激优先走 ThighCleft，插入则走 Vagina；手指方向变动大，不加角度限制
    {Define::BodyPart::Name::FingerLeft | Define::BodyPart::Name::ThighCleft,
     {Interact::Type::FingerVagina, 4.5f, 0.f, 180.f, false}},
    {Define::BodyPart::Name::FingerLeft | Define::BodyPart::Name::Vagina,
     {Interact::Type::FingerVagina, 5.5f, 0.f, 180.f, false}},
    {Define::BodyPart::Name::FingerLeft | Define::BodyPart::Name::Anus,
     {Interact::Type::FingerAnus, 5.f, 0.f, 180.f, false}},
    // 手掌向量与penis向量接近垂直
    {Define::BodyPart::Name::HandLeft | Define::BodyPart::Name::Penis,
     {Interact::Type::Handjob, 7.f, 60.f, 120.f, false}},
    // ── Belly ───────────────────────────────────────────────────────────────
    // Naveljob: penis 大致对齐 Belly 前向且保持近水平，避免把下压插入误判成脐交
    {Define::BodyPart::Name::Belly | Define::BodyPart::Name::Penis,
     {Interact::Type::Naveljob, 9.f, 0.f, 50.f, true, true}},
    // ── Thigh ───────────────────────────────────────────────────────────────
    // Thighjob 优先走裆前中心线；保留单侧 Thigh 作为外侧夹腿兜底
    {Define::BodyPart::Name::ThighCleft | Define::BodyPart::Name::Penis,
     {Interact::Type::Thighjob, 5.5f, 0.f, 25.f, false}},
    {Define::BodyPart::Name::ThighLeft | Define::BodyPart::Name::Penis,
     {Interact::Type::Thighjob, 4.5f, 0.f, 25.f, false}},
    // ── Butt ────────────────────────────────────────────────────────────────
    // Frottage: GlutealCleft Penis 不区分方向 30 度以内
    {Define::BodyPart::Name::GlutealCleft | Define::BodyPart::Name::Penis,
     {Interact::Type::Frottage, 7.f, 145.f, 35.f, false}},
    // ── Foot ────────────────────────────────────────────────────────────────
    {Define::BodyPart::Name::FootLeft | Define::BodyPart::Name::Penis,
     {Interact::Type::Footjob, 7.f, 0.f, 180.f, false}},
    // ── Vagina ──────────────────────────────────────────────────────────────
    // Tribbing: 两 Vagina 开口相对 → 反向 [150,180]
    {Define::BodyPart::Name::Vagina | Define::BodyPart::Name::Vagina,
     {Interact::Type::Tribbing, 7.f, 145.f, 180.f, false}},
    // Vaginal: penis root→tip 与 vagina 入口→深处同向 → 对齐 [0,35]
    {Define::BodyPart::Name::Vagina | Define::BodyPart::Name::Penis,
     {Interact::Type::Vaginal, 7.f, 0.f, 22.f, false}},
    // ── Anus ────────────────────────────────────────────────────────────────
    {Define::BodyPart::Name::Anus | Define::BodyPart::Name::Penis,
     {Interact::Type::Anal, 6.f, 0.f, 18.f, false}},
};

const Rule& GetRule(Define::BodyPart::Name a, Define::BodyPart::Name b)
{
  static Rule empty = {Interact::Type::None, 0.f, 0.f, 180.f, false, false};

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
// Rule.minAngle / Rule.maxAngle 统一使用绝对角度 [0, 180]，Angle() 已返回绝对值。
//
// 区间语义（minAngle <= maxAngle：正常区间；minAngle > maxAngle：换绕区间）：
//   [0,   x]       — 近似同向（对齐），x 越小越严格
//   [x,  180]      — 近似反向（面对面），x 越大越严格
//   [a,   b]       — 近似垂直，如 [70,110] = 垂直 ±20°
//   [b,   a](b>a)  — 换绕 [0,a]∪[b,180]，同/反向均可，如 [130,50]
//   [0,  180]      — 无约束
//
// kHighAnglePriority: 对轴向更敏感的交互。角度应该明显参与排序，但不能压过距离本身，
// 否则会出现 “位置其实更对，但因为角度略差就永远抢不到” 的情况。
static const float kHighAngleWeight   = 0.45f;
static const float kNormalAngleWeight = 0.12f;

static const std::unordered_set<Interact::Type> kHighAnglePriority{
    Interact::Type::Vaginal,    Interact::Type::Anal,         Interact::Type::Fellatio,
    Interact::Type::DeepThroat, Interact::Type::FingerVagina, Interact::Type::FingerAnus,
    Interact::Type::Titfuck,    Interact::Type::Thighjob,     Interact::Type::Tribbing,
    Interact::Type::Kiss,       Interact::Type::Frottage,
};

// ═══════════════════════════════════════════════════════════════════════════
// §4  角度判断辅助
// ═══════════════════════════════════════════════════════════════════════════

namespace
{

  const BP* TryGetBodyPart(const Instance::Interact::ActorData& data, Name name)
  {
    auto it = data.infos.find(name);
    if (it == data.infos.end() || !it->second.bodypart.IsValid())
      return nullptr;
    return &it->second.bodypart;
  }

  float CalculatePenetrationContextBias(
      const std::unordered_map<RE::Actor*, Instance::Interact::ActorData>& datas, RE::Actor* actorA,
      Name nameA, RE::Actor* actorB, Name nameB, Interact::Type type)
  {
    if (type != Interact::Type::Vaginal && type != Interact::Type::Anal)
      return 0.f;

    RE::Actor* receiver = nullptr;
    RE::Actor* owner    = nullptr;
    if (nameA == Name::Vagina || nameA == Name::Anus) {
      receiver = actorA;
      owner    = actorB;
    } else if (nameB == Name::Vagina || nameB == Name::Anus) {
      receiver = actorB;
      owner    = actorA;
    }

    if (!receiver || !owner)
      return 0.f;

    auto receiverIt = datas.find(receiver);
    auto ownerIt    = datas.find(owner);
    if (receiverIt == datas.end() || ownerIt == datas.end())
      return 0.f;

    const BP* penisPart   = TryGetBodyPart(ownerIt->second, Name::Penis);
    const BP* thighCleft  = TryGetBodyPart(receiverIt->second, Name::ThighCleft);
    const BP* glutealPart = TryGetBodyPart(receiverIt->second, Name::GlutealCleft);
    if (!penisPart)
      return 0.f;

    // kSupportScale:
    //   把 “Vaginal 支撑点距离” 与 “Anal 支撑点距离” 的差值映射到 score 偏置时使用的缩放尺。
    //   数值越大，前后区域差异要更明显才会产生同样强度的偏置；数值越小，系统越敏感。
    //   例如这里设为 8，表示两侧支撑距离相差约 8 个 world units 时，偏置接近饱和。
    constexpr float kSupportScale = 8.f;

    // kMaxBias:
    //   对 Vaginal/Anal 候选额外施加的最大 score 修正幅度。score 越低越优先，
    //   所以负值偏置会“扶正”当前候选，正值偏置会“压低”当前候选。
    //   它的作用是打破前后区域模糊时的僵局，而不是覆盖掉距离/角度本身。
    constexpr float kMaxBias = 0.22f;

    const float vagSupport =
        thighCleft ? thighCleft->Distance(*penisPart) : std::numeric_limits<float>::infinity();
    const float analSupport =
        glutealPart ? glutealPart->Distance(*penisPart) : std::numeric_limits<float>::infinity();

    if (std::isfinite(vagSupport) && std::isfinite(analSupport)) {
      const float delta =
          std::clamp((vagSupport - analSupport) / kSupportScale, -kMaxBias, kMaxBias);
      return type == Interact::Type::Vaginal ? delta : -delta;
    }

    if (type == Interact::Type::Vaginal && std::isfinite(vagSupport))
      return -std::clamp(1.f - vagSupport / kSupportScale, 0.f, kMaxBias);
    if (type == Interact::Type::Anal && std::isfinite(analSupport))
      return -std::clamp(1.f - analSupport / kSupportScale, 0.f, kMaxBias);

    return 0.f;
  }

  // 判断绝对角度是否落在规则区间内。
  // minAngle <= maxAngle: 正常区间 [min, max]
  // minAngle >  maxAngle: 换绕区间 [0, max] ∪ [min, 180]
  // [0, 180]: 始终通过（无约束）
  bool CheckAngle(float absAngle, float minAngle, float maxAngle)
  {
    if (minAngle <= 0.f && maxAngle >= 179.9f)
      return true;
    if (minAngle <= maxAngle)
      return absAngle >= minAngle && absAngle <= maxAngle;
    else  // wrap: [0, maxAngle] ∪ [minAngle, 180]
      return absAngle <= maxAngle || absAngle >= minAngle;
  }

  // 角度偏差归一化 [0, ∞)：0 = 恰在区间中心，1 = 恰在区间边界，>1 = 在区间外。
  // 用于 Phase 2 评分，与距离归一化加权合并。
  float AngleDeviation(float absAngle, float minAngle, float maxAngle)
  {
    if (minAngle <= 0.f && maxAngle >= 179.9f)
      return 0.f;  // 无约束，不贡献角度评分

    const bool wrap = (minAngle > maxAngle);
    if (!wrap) {
      // 正常区间：以中心为基准归一化
      const float half = (maxAngle - minAngle) * 0.5f;
      const float mid  = (minAngle + maxAngle) * 0.5f;
      return (half > 1e-4f) ? std::abs(absAngle - mid) / half : 0.f;
    } else {
      // 换绕区间 [0, maxAngle] ∪ [minAngle, 180]
      // 分别计算到两段中心的偏差，取较小值
      const float midLow   = maxAngle * 0.5f;
      const float midHigh  = (minAngle + 180.f) * 0.5f;
      const float halfLow  = (maxAngle > 1e-4f) ? maxAngle * 0.5f : 1e-4f;
      const float halfHigh = ((180.f - minAngle) > 1e-4f) ? (180.f - minAngle) * 0.5f : 1e-4f;
      const float devLow   = std::abs(absAngle - midLow) / halfLow;
      const float devHigh  = std::abs(absAngle - midHigh) / halfHigh;
      return devLow < devHigh ? devLow : devHigh;
    }
  }

  // 附加几何约束：当前仅用于 Naveljob 的前向与水平限制。
  bool CheckExtra(const Rule& rule, Name nameA, const BP& partA, Name nameB, const BP& partB)
  {
    const BP* bellyPart = (nameA == Name::Belly)   ? &partA
                          : (nameB == Name::Belly) ? &partB
                                                   : nullptr;
    const BP* penisPart = (nameA == Name::Penis)   ? &partA
                          : (nameB == Name::Penis) ? &partB
                                                   : nullptr;

    if (rule.needFront) {
      if (!bellyPart || !penisPart || !bellyPart->IsInFront(penisPart->GetEnd()))
        return false;
    }

    if (rule.needHorizontal) {
      if (!penisPart || !penisPart->IsHorizontal())
        return false;
    }

    return true;
  }

}  // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════
// §5  DistEntry（Phase 2 距离条目）
// ═══════════════════════════════════════════════════════════════════════════

namespace
{

  // 跨 actor 部位对的距离/角度记录，用于 Phase 2 排序后的贪心分配
  // score = dist_norm*(1-w) + angle_norm*w
  //   穿插类（kHighAnglePriority）无约束类 w=0
  struct DistEntry
  {
    RE::Actor* actorA;
    Name nameA;
    RE::Actor* actorB;
    Name nameB;
    float distance;  // world units
    float absAngle;  // [0, 180] 度，两部位方向夹角绝对值
    float score;     // 综合评分（越低越优先）
    Rule rule;       // 规则副本
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
// Phase 2 — 计算所有 actor 对的有规则部位对距离+角度，构建 DistEntry 并按综合评分升序排序
//            评分 = dist_norm*(1-w) + angle_norm*w
// Phase 3 — 贪心分配：遍历 DistEntry，检查约束后分配；每个 (actor, part) 只参与一个交互
//            DeepThroat 由 Throat<->Penis 规则直接检测（无需 Phase 4 升级）
// Phase 4 — 组合交互检测：SixtyNine / Spitroast / DoublePenetration / TriplePenetration
// Phase 5 — 组合类型覆写：将组合类型写回参与部位的 type（优先级 TP > DP/SR > SN）
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
      info.velocity     = 0.f;
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

          const float dist     = infoA.bodypart.Distance(infoB.bodypart);
          const float absAngle = infoA.bodypart.Angle(infoB.bodypart);

          // 归一化距离 [0,1]：0=紧贴, 1=恰好在 radius 边界
          const float distNorm = (rule.radius > 1e-4f) ? dist / rule.radius : 0.f;

          // 归一化角度偏差 [0,∞)：0=恰在区间中心，1=区间边界
          const float angleNorm = AngleDeviation(absAngle, rule.minAngle, rule.maxAngle);

          // 有约束的交互按重要性区分权重；无约束（{0,180}）纯靠距离排序
          const bool hasConstraint = !(rule.minAngle <= 0.f && rule.maxAngle >= 179.9f);
          const float angleWeight  = !hasConstraint                        ? 0.f
                                     : kHighAnglePriority.count(rule.type) ? kHighAngleWeight
                                                                           : kNormalAngleWeight;
          const float baseScore    = distNorm * (1.f - angleWeight) + angleNorm * angleWeight;
          const float score = baseScore + CalculatePenetrationContextBias(datas, actA, nameA, actB,
                                                                          nameB, rule.type);

          entries.push_back({actA, nameA, actB, nameB, dist, absAngle, score, rule});
        }
      }
    }
  }

  // 按综合评分升序排序：距离+角度契合度加权，骨骼密集区角度优先
  std::sort(entries.begin(), entries.end(), [](const DistEntry& x, const DistEntry& y) {
    return x.score < y.score;
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

    // 角度约束（使用 Phase 2 中已缓存的 absAngle）
    if (!CheckAngle(e.absAngle, e.rule.minAngle, e.rule.maxAngle))
      continue;

    // 附加约束（当前用于 Naveljob: Belly.IsInFront + penis 水平）
    if (e.rule.needFront || e.rule.needHorizontal) {
      const BP& bpA = datas[e.actorA].infos[e.nameA].bodypart;
      const BP& bpB = datas[e.actorB].infos[e.nameB].bodypart;
      if (!CheckExtra(e.rule, e.nameA, bpA, e.nameB, bpB))
        continue;
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

  // ── Phase 4: 组合交互检测 ────────────────────────────────────────────────
  struct ActorInteractSummary
  {
    bool hasOral              = false;  // Mouth(Fellatio) 或 Throat(DeepThroat)：被口交/深喉
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
        // Fellatio 由 Mouth 触发
        if (partName == Name::Mouth) {
          s.hasOral     = true;
          s.oralPartner = asn.partner;
        }
        break;
      case Interact::Type::DeepThroat:
        // DeepThroat 由 Throat 触发（Phase 2 规则直接匹配）
        if (partName == Name::Throat) {
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

  // ── Phase 5: 组合类型覆写 ────────────────────────────────────────────────
  // 将组合类型写回参与部位的 assigns type，以便 Phase 6 写入 Info.type 后
  // 外部只需读 Info.type 即可感知全部交互状态，无需额外查询 combo 集合。
  // 优先级（高→低）：TriplePenetration > DoublePenetration / Spitroast > SixtyNine。
  // SixtyNine 与 Spitroast 因 usedParts 限制天然互斥（同一 Mouth 不能同时收/发）。
  for (auto& [actor, combos] : comboTypes) {
    auto itAssign = assigns.find(actor);
    if (itAssign == assigns.end())
      continue;
    auto& partMap = itAssign->second;
    const auto& s = summaries[actor];

    auto has = [&](Interact::Type t) {
      return std::find(combos.begin(), combos.end(), t) != combos.end();
    };
    auto set = [&](Name part, Interact::Type t) {
      auto it = partMap.find(part);
      if (it != partMap.end())
        it->second.type = t;
    };

    const bool hasTP = has(Interact::Type::TriplePenetration);
    const bool hasDP = has(Interact::Type::DoublePenetration);
    const bool hasSR = has(Interact::Type::Spitroast);
    const bool hasSN = has(Interact::Type::SixtyNine);

    // TriplePenetration：覆写全部参与部位，跳过低优先级
    if (hasTP) {
      set(Name::Mouth, Interact::Type::TriplePenetration);
      set(Name::Throat, Interact::Type::TriplePenetration);
      set(Name::Vagina, Interact::Type::TriplePenetration);
      set(Name::Anus, Interact::Type::TriplePenetration);
      continue;
    }

    // DoublePenetration：覆写 Vagina + Anus
    if (hasDP) {
      set(Name::Vagina, Interact::Type::DoublePenetration);
      set(Name::Anus, Interact::Type::DoublePenetration);
    }

    // Spitroast：覆写口腔部位 + 未被 DP 覆写的穿插部位
    if (hasSR) {
      set(Name::Throat, Interact::Type::Spitroast);
      set(Name::Mouth, Interact::Type::Spitroast);
      if (!hasDP) {
        if (s.hasVaginal)
          set(Name::Vagina, Interact::Type::Spitroast);
        if (s.hasAnal)
          set(Name::Anus, Interact::Type::Spitroast);
      }
    }

    // SixtyNine：覆写发起方的 Mouth（与 Spitroast 天然互斥）
    if (hasSN)
      set(Name::Mouth, Interact::Type::SixtyNine);
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
  // for (auto& [actor, data] : datas) {
  //   for (auto& [partName, info] : data.infos) {
  //     if (!info.actor || info.type == Interact::Type::None)
  //       continue;

  //     if (info.type == info.prevType && info.actor == info.prevActor)
  //       continue;

  //     logger::info(
  //         "Actor: {}, Part: {}, Info: {{ actor: {}, distance: {}, velocity: {}, type: {} }}",
  //         actor->GetName(), magic_enum::enum_name(partName),
  //         info.actor ? info.actor->GetName() : "None", info.distance, info.velocity,
  //         magic_enum::enum_name(info.type));
  //   }

  //   // Combo types
  //   auto itCombo = comboTypes.find(actor);
  //   if (itCombo != comboTypes.end()) {
  //     for (const auto& combo : itCombo->second)
  //       logger::info("Actor: {}, Combo: {}", actor->GetName(), magic_enum::enum_name(combo));
  //   }
  // }
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

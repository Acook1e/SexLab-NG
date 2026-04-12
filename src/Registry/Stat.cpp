#include "Registry/Stat.h"

#include "Instance/Manager.h"
#include "Papyrus/SexLab.h"
#include "Utils/Serialization.h"

namespace Registry
{

// ════════════════════════════════════════════════════════════
// 升级曲线参数: XP(level) = base * exp(factor * level)
// ════════════════════════════════════════════════════════════
static const std::unordered_map<ActorStat::ExperienceType, float> kBaseXP{
    {ActorStat::ExperienceType::Main, 45.0f},       {ActorStat::ExperienceType::Oral, 40.0f},
    {ActorStat::ExperienceType::Vagina, 40.0f},     {ActorStat::ExperienceType::Anus, 40.0f},
    {ActorStat::ExperienceType::Solo, 35.0f},       {ActorStat::ExperienceType::Group, 50.0f},
    {ActorStat::ExperienceType::Aggressive, 42.0f}, {ActorStat::ExperienceType::Submissive, 42.0f},
};

static const std::unordered_map<ActorStat::ExperienceType, float> kFactor{
    {ActorStat::ExperienceType::Main, 0.0050f},
    {ActorStat::ExperienceType::Oral, 0.0048f},
    {ActorStat::ExperienceType::Vagina, 0.0048f},
    {ActorStat::ExperienceType::Anus, 0.0048f},
    {ActorStat::ExperienceType::Solo, 0.0045f},
    {ActorStat::ExperienceType::Group, 0.0052f},
    {ActorStat::ExperienceType::Aggressive, 0.0049f},
    {ActorStat::ExperienceType::Submissive, 0.0049f},
};

static float XPToNextLevel(ActorStat::ExperienceType type, std::uint16_t currentLevel)
{
  return kBaseXP.at(type) * std::exp(kFactor.at(type) * static_cast<float>(currentLevel));
}

// ════════════════════════════════════════════════════════════
// 从 experienceLevels 取 level，若无记录返回 1
// ════════════════════════════════════════════════════════════
static std::uint16_t GetLevel(const ActorStat::Stat& stat, ActorStat::ExperienceType type)
{
  const auto it = stat.experienceLevels.find(type);
  return it != stat.experienceLevels.end() ? it->second.level : 1;
}

namespace
{
  struct InteractionProfile
  {
    ActorStat::ExperienceType experienceType;
    float baseDelta;
    float neutralLevel;
    float levelCurve;
  };

  struct ActiveInteractionKey
  {
    Instance::Interact::Type type = Instance::Interact::Type::None;
    RE::Actor* partner            = nullptr;

    bool operator==(const ActiveInteractionKey&) const = default;
  };

  struct ActiveInteractionKeyHash
  {
    std::size_t operator()(const ActiveInteractionKey& key) const
    {
      const auto typeHash  = static_cast<std::size_t>(std::to_underlying(key.type));
      const auto actorHash = std::hash<RE::Actor*>{}(key.partner);
      return typeHash ^ (actorHash + 0x9E3779B97F4A7C15ull + (typeHash << 6) + (typeHash >> 2));
    }
  };

  struct InteractionTickResult
  {
    float delta                      = 0.f;
    std::uint32_t activeInteractions = 0;
  };

  constexpr float kMaxEnjoyTickDelta     = 0.80f;
  constexpr float kMinSensitivityFactor  = 0.70f;
  constexpr float kMaxSensitivityFactor  = 1.30f;
  constexpr float kPostClimaxMinEnjoy    = -20.f;
  constexpr float kPostClimaxMaxEnjoy    = 20.f;
  constexpr float kArouseMultiplierCeil  = 1.20f;
  constexpr float kVelocityReference     = 0.08f;
  constexpr float kVelocityNormalization = 2.50f;

  static const InteractionProfile& GetInteractionProfile(Instance::Interact::Type type)
  {
    using ExperienceType = ActorStat::ExperienceType;
    using Type           = Instance::Interact::Type;

    static const InteractionProfile fallback{ExperienceType::Main, 0.0f, 1.0f, 1.0f};

    switch (type) {
    case Type::Kiss: {
      static const InteractionProfile profile{ExperienceType::Main, 0.10f, 120.f, 70.f};
      return profile;
    }
    case Type::BreastSucking: {
      static const InteractionProfile profile{ExperienceType::Oral, 0.22f, 95.f, 55.f};
      return profile;
    }
    case Type::ToeSucking: {
      static const InteractionProfile profile{ExperienceType::Oral, 0.14f, 160.f, 80.f};
      return profile;
    }
    case Type::Cunnilingus: {
      static const InteractionProfile profile{ExperienceType::Oral, 0.30f, 80.f, 50.f};
      return profile;
    }
    case Type::Anilingus: {
      static const InteractionProfile profile{ExperienceType::Oral, 0.24f, 110.f, 55.f};
      return profile;
    }
    case Type::Fellatio: {
      static const InteractionProfile profile{ExperienceType::Oral, 0.28f, 85.f, 50.f};
      return profile;
    }
    case Type::DeepThroat: {
      static const InteractionProfile profile{ExperienceType::Oral, 0.34f, 140.f, 60.f};
      return profile;
    }
    case Type::MouthAnimObj: {
      static const InteractionProfile profile{ExperienceType::Oral, 0.12f, 90.f, 60.f};
      return profile;
    }
    case Type::GropeBreast: {
      static const InteractionProfile profile{ExperienceType::Main, 0.18f, 90.f, 55.f};
      return profile;
    }
    case Type::GropeThigh: {
      static const InteractionProfile profile{ExperienceType::Main, 0.14f, 95.f, 55.f};
      return profile;
    }
    case Type::GropeButt: {
      static const InteractionProfile profile{ExperienceType::Main, 0.20f, 90.f, 55.f};
      return profile;
    }
    case Type::GropeFoot: {
      static const InteractionProfile profile{ExperienceType::Main, 0.12f, 145.f, 75.f};
      return profile;
    }
    case Type::Titfuck: {
      static const InteractionProfile profile{ExperienceType::Main, 0.30f, 110.f, 60.f};
      return profile;
    }
    case Type::FingerVagina: {
      static const InteractionProfile profile{ExperienceType::Vagina, 0.24f, 70.f, 45.f};
      return profile;
    }
    case Type::FingerAnus: {
      static const InteractionProfile profile{ExperienceType::Anus, 0.22f, 105.f, 55.f};
      return profile;
    }
    case Type::Handjob: {
      static const InteractionProfile profile{ExperienceType::Main, 0.24f, 75.f, 50.f};
      return profile;
    }
    case Type::Masturbation: {
      static const InteractionProfile profile{ExperienceType::Solo, 0.20f, 65.f, 45.f};
      return profile;
    }
    case Type::HandAnimObj: {
      static const InteractionProfile profile{ExperienceType::Main, 0.10f, 85.f, 55.f};
      return profile;
    }
    case Type::Naveljob: {
      static const InteractionProfile profile{ExperienceType::Main, 0.18f, 120.f, 60.f};
      return profile;
    }
    case Type::Thighjob: {
      static const InteractionProfile profile{ExperienceType::Main, 0.18f, 120.f, 60.f};
      return profile;
    }
    case Type::Frottage: {
      static const InteractionProfile profile{ExperienceType::Main, 0.20f, 100.f, 55.f};
      return profile;
    }
    case Type::Footjob: {
      static const InteractionProfile profile{ExperienceType::Main, 0.16f, 140.f, 70.f};
      return profile;
    }
    case Type::Tribbing: {
      static const InteractionProfile profile{ExperienceType::Vagina, 0.28f, 95.f, 50.f};
      return profile;
    }
    case Type::Vaginal: {
      static const InteractionProfile profile{ExperienceType::Vagina, 0.38f, 60.f, 40.f};
      return profile;
    }
    case Type::VaginaAnimObj: {
      static const InteractionProfile profile{ExperienceType::Vagina, 0.12f, 90.f, 55.f};
      return profile;
    }
    case Type::Anal: {
      static const InteractionProfile profile{ExperienceType::Anus, 0.34f, 120.f, 55.f};
      return profile;
    }
    case Type::AnalAnimObj: {
      static const InteractionProfile profile{ExperienceType::Anus, 0.10f, 95.f, 60.f};
      return profile;
    }
    case Type::PenisAnimObj: {
      static const InteractionProfile profile{ExperienceType::Main, 0.10f, 95.f, 60.f};
      return profile;
    }
    case Type::SixtyNine: {
      static const InteractionProfile profile{ExperienceType::Oral, 0.42f, 100.f, 50.f};
      return profile;
    }
    case Type::Spitroast: {
      static const InteractionProfile profile{ExperienceType::Group, 0.48f, 130.f, 60.f};
      return profile;
    }
    case Type::DoublePenetration: {
      static const InteractionProfile profile{ExperienceType::Group, 0.54f, 150.f, 65.f};
      return profile;
    }
    case Type::TriplePenetration: {
      static const InteractionProfile profile{ExperienceType::Group, 0.62f, 180.f, 70.f};
      return profile;
    }
    case Type::None:
    case Type::Total:
    default:
      return fallback;
    }
  }

  static bool IsAnimObjectInteraction(Instance::Interact::Type type)
  {
    using Type = Instance::Interact::Type;
    return type == Type::MouthAnimObj || type == Type::HandAnimObj || type == Type::VaginaAnimObj ||
           type == Type::AnalAnimObj || type == Type::PenisAnimObj;
  }

  static float CalculateObjectFactor(const Instance::Interact::Info& info)
  {
    using Type = Instance::Interact::Type;

    if (info.type == Type::Masturbation)
      return 0.78f;
    if (IsAnimObjectInteraction(info.type))
      return 0.60f;
    if (!info.actor)
      return 0.85f;

    switch (info.type) {
    case Type::Kiss:
      return 0.95f;
    case Type::GropeBreast:
    case Type::GropeThigh:
    case Type::GropeButt:
      return 0.90f;
    case Type::GropeFoot:
    case Type::ToeSucking:
    case Type::Footjob:
      return 0.82f;
    default:
      return 1.0f;
    }
  }

  static float CalculateSpeedFactor(float velocity)
  {
    const float speed      = std::abs(velocity);
    const float normalized = std::clamp(speed / kVelocityReference, 0.f, kVelocityNormalization);
    const float shaped     = std::sqrt(normalized / kVelocityNormalization);
    return std::clamp(0.55f + shaped * 0.70f, 0.55f, 1.25f);
  }

  static float CalculateLevelResponse(std::uint16_t level, const InteractionProfile& profile)
  {
    const float normalized =
        (static_cast<float>(level) - profile.neutralLevel) / (std::max)(profile.levelCurve, 1.f);
    return std::clamp(std::tanh(normalized), -0.85f, 0.95f);
  }

  static float CalculateSensitivityFactor(const ActorStat::Stat& stat)
  {
    const float sensitive = std::clamp(stat.sensitive, -10.f, 10.f);
    return std::clamp(1.f + sensitive * 0.03f, kMinSensitivityFactor, kMaxSensitivityFactor);
  }

  static float CalculateArouseMultiplier(const ActorStat::Stat& stat)
  {
    const float arouseRatio = std::clamp(stat.arouse / 100.f, 0.f, 1.f);
    return std::lerp(1.0f, kArouseMultiplierCeil, arouseRatio);
  }

  static float CalculateClimaxArouseDrain(float currentArouse, std::uint8_t climaxCount)
  {
    const float arouse      = std::clamp(currentArouse, 0.f, 100.f);
    const float baseDrain   = 8.f + arouse * 0.22f;
    const float chainFactor = std::clamp(
        1.f + static_cast<float>(climaxCount > 0 ? climaxCount - 1 : 0) * 0.18f, 1.f, 1.6f);
    return (std::min)(arouse, baseDrain * chainFactor);
  }

  static float CalculatePostClimaxEnjoy(const ActorStat::Stat& stat)
  {
    const float mainLevelRatio =
        (static_cast<float>(GetLevel(stat, ActorStat::ExperienceType::Main)) - 1.f) / 998.f;
    const float sensitiveRatio = std::clamp(stat.sensitive, -10.f, 10.f) / 10.f;
    const float arouseRatio    = std::clamp(stat.arouse / 100.f, 0.f, 1.f);

    const float resetEnjoy =
        -12.f + mainLevelRatio * 10.f + sensitiveRatio * 8.f + arouseRatio * 6.f;
    return std::clamp(resetEnjoy, kPostClimaxMinEnjoy, kPostClimaxMaxEnjoy);
  }
}  // namespace

static InteractionTickResult CalculateInteract(ActorStat::Stat& stat,
                                               const Instance::Interact::ActorData& interactData)
{
  std::unordered_set<ActiveInteractionKey, ActiveInteractionKeyHash> activeInteractions;

  float totalDelta              = 0.f;
  const float sensitivityFactor = CalculateSensitivityFactor(stat);
  const float arouseMultiplier  = CalculateArouseMultiplier(stat);

  for (const auto& [partName, info] : interactData.infos) {
    if (info.type == Instance::Interact::Type::None)
      continue;

    const ActiveInteractionKey key{info.type, info.actor};
    if (!activeInteractions.insert(key).second)
      continue;

    const auto& profile      = GetInteractionProfile(info.type);
    const auto interactionLv = GetLevel(stat, profile.experienceType);
    const float levelFactor  = CalculateLevelResponse(interactionLv, profile);
    const float speedFactor  = CalculateSpeedFactor(info.velocity);
    const float objectFactor = CalculateObjectFactor(info);

    totalDelta += profile.baseDelta * levelFactor * speedFactor * objectFactor * sensitivityFactor *
                  arouseMultiplier;
  }

  if (activeInteractions.empty())
    return {};

  return {std::clamp(totalDelta, -kMaxEnjoyTickDelta, kMaxEnjoyTickDelta),
          static_cast<std::uint32_t>(activeInteractions.size())};
}

// ════════════════════════════════════════════════════════════
// 构造 / 获取 Stat
// ════════════════════════════════════════════════════════════
ActorStat::ActorStat()
{
  Serialization::RegisterSaveCallback(SerializationType, onSave);
  Serialization::RegisterLoadCallback(SerializationType, onLoad);
  Serialization::RegisterRevertCallback(SerializationType, onRevert);
}

ActorStat::Stat& ActorStat::GetStat(RE::Actor* actor)
{
  static Stat nullStat;

  if (!actor)
    return nullStat;

  if (actor->GetActorBase()->IsUnique()) {
    auto [it, _] = actorStats.try_emplace(actor->GetFormID());
    return it->second;
  } else {
    auto [it, _] = runtimeActorStats.try_emplace(actor);
    return it->second;
  }
}

// ════════════════════════════════════════════════════════════
// AddXP — 向指定经验类型添加 XP，处理连续升级
// ════════════════════════════════════════════════════════════
void ActorStat::AddXP(RE::Actor* actor, ExperienceType type, float amount)
{
  if (!actor || amount <= 0.f)
    return;

  auto& stat  = GetStat(actor);
  auto& entry = stat.experienceLevels[type];

  entry.XP += amount;
  while (entry.level < 999) {
    const float needed = XPToNextLevel(type, entry.level);
    if (entry.XP < needed)
      break;
    entry.XP -= needed;
    ++entry.level;
  }
  if (entry.level >= 999)
    entry.XP = 0.f;
}

// ════════════════════════════════════════════════════════════
// Update — 全局时间推进，仅对不在场景中的 actor 调用
//
// 每游戏小时触发一次（约现实 1 分钟）。
// deltaGameHours 由 Hooks::MainUpdate 根据 Calendar 计算传入。
//
// 效果:
//   - arouse 随时间缓慢自然增长（每小时 +0.5，受性别倾向影响极小幅修正）
//   - enjoy 向 0 衰减（每小时衰减 5%，不低于 0 或高于当前值）
// ════════════════════════════════════════════════════════════
void ActorStat::Update(float deltaGameHours)
{
  if (deltaGameHours <= 0.f)
    return;

  // 不在场景的 actor 的时间推进处理
  auto processActor = [&](RE::Actor* actor, Stat& stat) {
    if (!actor)
      return;
    if (Instance::SceneManager::IsActorInScene(actor))
      return;

    // arouse 自然增长: +0.5 / game hour，上限 100
    stat.arouse = min(100.f, stat.arouse + 0.5f * deltaGameHours);

    // enjoy 向 0 衰减: 每小时衰减 30%，但不越过 0
    const float current = stat.enjoy.GetValue();
    if (current > 0.f)
      stat.enjoy.SetValue(max(0.f, current - current * 0.3f * deltaGameHours));
    else if (current < 0.f)
      stat.enjoy.SetValue(min(0.f, current - current * 0.3f * deltaGameHours));
  };

  for (auto& [formID, stat] : actorStats) {
    const auto actor = RE::TESForm::LookupByID<RE::Actor>(formID);
    processActor(actor, stat);
  }
  for (auto& [actor, stat] : runtimeActorStats) {
    processActor(actor, stat);
  }
}

// ════════════════════════════════════════════════════════════
// UpdateEnjoyment — 场景内每 0.1 秒按实时交互更新 enjoy
//
// 设计:
//   1. 不再使用场景 tag 作为基础增量，只有实时交互才会产生 enjoy 变化
//   2. 每种交互按 类型 / 对象 / 速度 / 对应等级 计算单独增量
//   3. 交互等级函数在低等级阶段可为负，因此痛苦型增长会自然出现
//   4. 整体倍率只由 arouse 决定，峰值 1.2
//   5. 当 enjoy 达到 Climax 阈值时，立即触发高潮事件、扣减 arouse、重置 enjoy
// ════════════════════════════════════════════════════════════
void ActorStat::UpdateEnjoyment(
    const Define::Scene* scene,
    std::unordered_map<RE::Actor*, Instance::SceneInstance::SceneActorInfo>& actorInfoMap,
    const Instance::Interact& interact)
{
  if (actorInfoMap.empty())
    return;

  for (auto& [actor, info] : actorInfoMap) {
    if (!actor)
      continue;

    auto& stat            = GetStat(actor);
    const auto& actorData = interact.GetData(actor);
    const auto tick       = CalculateInteract(stat, actorData);

    if (tick.activeInteractions == 0)
      continue;

    const float newEnjoy = std::clamp(stat.enjoy.GetValue() + tick.delta,
                                      static_cast<float>(Define::Enjoyment::Degree::MinValue),
                                      static_cast<float>(Define::Enjoyment::Degree::MaxValue));
    stat.enjoy.SetValue(newEnjoy);

    if (newEnjoy < static_cast<float>(Define::Enjoyment::Degree::Climax))
      continue;

    const auto nextClimaxCount =
        (std::min)(static_cast<std::uint32_t>((std::numeric_limits<std::uint8_t>::max)()),
                   static_cast<std::uint32_t>(info.climaxCount) + 1u);
    info.climaxCount = static_cast<std::uint8_t>(nextClimaxCount);
    stat.arouse =
        (std::max)(0.f, stat.arouse - CalculateClimaxArouseDrain(stat.arouse, info.climaxCount));

    stat.enjoy.SetValue(CalculatePostClimaxEnjoy(stat));
  }
}

// ════════════════════════════════════════════════════════════
// UpdateStat
//
// 仅在场景销毁时调用一次。
// 根据场景标签和最终 enjoyment 更新:
//   - 各经验类型 XP (enjoyment 越高, climax 次数越多，收益越多)
//   - recentPartners 列表 (上限 20)
//   - 性别 / 种族倾向计数
//   - arouse 更新 (高潮多则场景后降低，否则残留较高)
// ════════════════════════════════════════════════════════════
void ActorStat::UpdateStat(
    const Define::Scene* scene,
    std::unordered_map<RE::Actor*, Instance::SceneInstance::SceneActorInfo> actorInfoMap)
{
  if (!scene || actorInfoMap.empty())
    return;

  const auto& tags        = scene->GetTags();
  const bool isSolo       = actorInfoMap.size() == 1;
  const bool isGroup      = actorInfoMap.size() > 2;
  const bool isAggressive = tags.Has(Define::SceneTags::Type::Aggressive);
  const bool isOral       = tags.Has(Define::SceneTags::Type::Oral);
  const bool isVaginal    = tags.Has(Define::SceneTags::Type::Vaginal);
  const bool isAnal       = tags.Has(Define::SceneTags::Type::Anal);

  // 收集所有 actor 的信息以便后续倾向计算
  struct PeerInfo
  {
    RE::FormID formID;
    Define::Gender::Type gender;
    Define::Race::Type race;
  };
  std::vector<PeerInfo> peers;
  peers.reserve(actorInfoMap.size());
  for (const auto& [actor, _] : actorInfoMap) {
    if (!actor)
      continue;
    peers.push_back(
        {actor->GetFormID(), Define::Gender(actor).Get(), Define::Race(actor).GetType()});
  }

  for (auto& [actor, info] : actorInfoMap) {
    if (!actor)
      continue;

    auto& stat = GetStat(actor);

    const float enjoyVal   = stat.enjoy.GetValue();
    const float enjoyNorm  = (enjoyVal + 100.f) / 200.f;  // [0, 1]
    const float climaxMult = 1.f + static_cast<float>(info.climaxCount) * 0.25f;

    // ── XP 结算 ──────────────────────────────────────────────────
    // Main XP: 由最终 enjoy 正规化决定基础量，climax 倍率加成
    // enjoy 越高 → XP 越多；climax 越多 → 倍率越高
    const float mainXP = enjoyNorm * 20.f * climaxMult;
    AddXP(actor, ExperienceType::Main, mainXP);

    if (isSolo)
      AddXP(actor, ExperienceType::Solo, enjoyNorm * 10.f);
    if (isGroup)
      AddXP(actor, ExperienceType::Group, enjoyNorm * 12.f * climaxMult);
    if (isOral)
      AddXP(actor, ExperienceType::Oral, enjoyNorm * 8.f * climaxMult);
    if (isVaginal)
      AddXP(actor, ExperienceType::Vagina, enjoyNorm * 10.f * climaxMult);
    if (isAnal)
      AddXP(actor, ExperienceType::Anus, enjoyNorm * 10.f * climaxMult);
    if (isAggressive) {
      // 场景内主动方加 Aggressive XP，被动方加 Submissive XP
      // 通过 interactTags 判断 actor 是否曾经是主动方
      // 暂以 Aggressive tag 场景统一处理
      AddXP(actor, ExperienceType::Aggressive, enjoyNorm * 8.f);
      AddXP(actor, ExperienceType::Submissive, enjoyNorm * 8.f);
    }

    // ── recentPartners 更新 ──────────────────────────────────────
    for (const auto& peer : peers) {
      if (peer.formID == actor->GetFormID())
        continue;
      const auto peerActor = RE::TESForm::LookupByID<RE::Actor>(peer.formID);
      if (!peerActor)
        continue;
      const std::string peerName = peerActor->GetDisplayFullName();
      if (peerName.empty())
        continue;
      // 去重后插入到头部
      auto it = std::find(stat.recentPartners.begin(), stat.recentPartners.end(), peerName);
      if (it != stat.recentPartners.end())
        stat.recentPartners.erase(it);
      stat.recentPartners.insert(stat.recentPartners.begin(), peerName);
      if (stat.recentPartners.size() > 20)
        stat.recentPartners.resize(20);
    }

    // ── 性别 / 种族倾向计数 ──────────────────────────────────────
    const Define::Gender::Type selfGender = Define::Gender(actor).Get();
    const Define::Race::Type selfRace     = Define::Race(actor).GetType();
    for (const auto& peer : peers) {
      if (peer.formID == actor->GetFormID())
        continue;
      if (peer.gender == selfGender)
        ++stat.TimesSameSex;
      else
        ++stat.TimesDiffSex;
      if (peer.race == selfRace)
        ++stat.TimesSameRace;
      else
        ++stat.TimesDiffRace;
    }

    // ── arouse 结算 ──────────────────────────────────────────────
    // 运行时高潮已即时扣减 arouse，这里只对未高潮场景做轻微收束
    if (info.climaxCount == 0) {
      if (enjoyVal >= 50.f) {
        const float settle = 4.f + enjoyNorm * 8.f;
        stat.arouse        = max(0.f, stat.arouse - settle);
      } else {
        stat.arouse = min(100.f, stat.arouse + 2.f);
      }
    }
  }
}

// ════════════════════════════════════════════════════════════
// 序列化
// ════════════════════════════════════════════════════════════
void ActorStat::onSave(SKSE::SerializationInterface* serial)
{
  // 清理无效引用
  std::erase_if(actorStats, [](const auto& pair) {
    return !RE::TESForm::LookupByID<RE::Actor>(pair.first);
  });

  const std::size_t size = actorStats.size();
  serial->WriteRecordData(&size, sizeof(size));

  for (auto& [formID, stat] : actorStats) {
    serial->WriteRecordData(&formID, sizeof(formID));

    // experienceLevels
    const std::size_t expCount = stat.experienceLevels.size();
    serial->WriteRecordData(&expCount, sizeof(expCount));
    for (auto& [type, lvl] : stat.experienceLevels) {
      serial->WriteRecordData(&type, sizeof(type));
      serial->WriteRecordData(&lvl.level, sizeof(lvl.level));
      serial->WriteRecordData(&lvl.XP, sizeof(lvl.XP));
    }

    // 倾向计数
    serial->WriteRecordData(&stat.TimesSameSex, sizeof(stat.TimesSameSex));
    serial->WriteRecordData(&stat.TimesDiffSex, sizeof(stat.TimesDiffSex));
    serial->WriteRecordData(&stat.TimesSameRace, sizeof(stat.TimesSameRace));
    serial->WriteRecordData(&stat.TimesDiffRace, sizeof(stat.TimesDiffRace));

    // 性欲
    serial->WriteRecordData(&stat.arouse, sizeof(stat.arouse));

    // enjoy (float)
    const float enjoyVal = stat.enjoy.GetValue();
    serial->WriteRecordData(&enjoyVal, sizeof(enjoyVal));

    // recentPartners
    auto partners     = join(stat.recentPartners, ';');
    auto partnersSize = partners.size();
    serial->WriteRecordData(&partnersSize, sizeof(partnersSize));
    serial->WriteRecordData(partners.data(), partnersSize);
  }
}

void ActorStat::onLoad(SKSE::SerializationInterface* serial)
{
  actorStats.clear();
  runtimeActorStats.clear();

  std::size_t size;
  if (!serial->ReadRecordData(&size, sizeof(size)))
    return;

  for (std::size_t i = 0; i < size; ++i) {
    std::uint32_t formID;
    Stat stat;

    if (!serial->ReadRecordData(&formID, sizeof(formID)))
      return;

    std::size_t expCount;
    if (!serial->ReadRecordData(&expCount, sizeof(expCount)))
      return;
    for (std::size_t j = 0; j < expCount; ++j) {
      ExperienceType type;
      Level lvl;
      if (!serial->ReadRecordData(&type, sizeof(type)))
        return;
      if (!serial->ReadRecordData(&lvl.level, sizeof(lvl.level)))
        return;
      if (!serial->ReadRecordData(&lvl.XP, sizeof(lvl.XP)))
        return;
      stat.experienceLevels[type] = lvl;
    }

    if (!serial->ReadRecordData(&stat.TimesSameSex, sizeof(stat.TimesSameSex)))
      return;
    if (!serial->ReadRecordData(&stat.TimesDiffSex, sizeof(stat.TimesDiffSex)))
      return;
    if (!serial->ReadRecordData(&stat.TimesSameRace, sizeof(stat.TimesSameRace)))
      return;
    if (!serial->ReadRecordData(&stat.TimesDiffRace, sizeof(stat.TimesDiffRace)))
      return;
    if (!serial->ReadRecordData(&stat.arouse, sizeof(stat.arouse)))
      return;

    // enjoy
    float enjoyVal = 0.f;
    if (!serial->ReadRecordData(&enjoyVal, sizeof(enjoyVal)))
      return;
    stat.enjoy.SetValue(enjoyVal);

    std::size_t partnersSize;
    if (!serial->ReadRecordData(&partnersSize, sizeof(partnersSize)))
      return;
    std::string partners(partnersSize, '\0');
    if (!serial->ReadRecordData(partners.data(), partnersSize))
      return;
    stat.recentPartners = split(partners, ';');

    actorStats[formID] = std::move(stat);
  }
}

void ActorStat::onRevert(SKSE::SerializationInterface* /*serial*/)
{
  actorStats.clear();
  runtimeActorStats.clear();
}

}  // namespace Registry
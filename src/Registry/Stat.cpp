#include "Registry/Stat.h"

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
// GetInitialEnjoyment
//
// 根据 actor 积累的 stat 计算场景开始时的 enjoy 初始值。
//
// 贡献来源 (均 clamp 后叠加):
//   A. 性欲残留:    arouse * 0.3          (上限 +30)
//   B. 综合经验:    Main level * 0.05      (上限 +15)
//   C. 专项经验:    每个匹配类型 * 0.02    (各上限 +5)
//   D. 性别倾向:    历史同性/异性比率 bias  (±10)
//   E. 种族倾向:    历史同族/异族比率 bias  (±5)
// ════════════════════════════════════════════════════════════
float ActorStat::GetInitialEnjoyment(RE::Actor* actor, const Define::SceneTags& sceneTags,
                                     const Define::Position& /*position*/,
                                     const std::vector<RE::Actor*>& others)
{
  if (!actor)
    return 0.f;

  auto& stat  = GetStat(actor);
  float enjoy = 0.f;

  // A. 性欲残留
  enjoy += (std::min)(stat.arouse * 0.3f, 30.f);

  // B. 综合经验
  {
    auto it = stat.experienceLevels.find(ExperienceType::Main);
    if (it != stat.experienceLevels.end())
      enjoy += (std::min)(static_cast<float>(it->second.level) * 0.05f, 15.f);
  }

  // C. 专项经验
  auto addSpecialty = [&](ExperienceType type) {
    auto it = stat.experienceLevels.find(type);
    if (it != stat.experienceLevels.end())
      enjoy += (std::min)(static_cast<float>(it->second.level) * 0.02f, 5.f);
  };
  if (sceneTags.Has(Define::SceneTags::Type::Oral))
    addSpecialty(ExperienceType::Oral);
  if (sceneTags.Has(Define::SceneTags::Type::Vaginal))
    addSpecialty(ExperienceType::Vagina);
  if (sceneTags.Has(Define::SceneTags::Type::Anal))
    addSpecialty(ExperienceType::Anus);

  // D/E. 倾向 bonus（基于历史比率与当前场景实际匹配度）
  if (!others.empty()) {
    const Define::Gender::Type actorGender = Define::Gender::GetGender(actor);
    const Define::Race actorRace(actor);

    float sexBias  = 0.f;
    float raceBias = 0.f;

    for (auto* other : others) {
      if (!other)
        continue;

      // 性别倾向
      const std::uint32_t sexTotal = stat.TimesSameSex + stat.TimesDiffSex;
      if (sexTotal > 0) {
        const bool sameSex   = (Define::Gender::GetGender(other) == actorGender);
        const float prefSame = static_cast<float>(stat.TimesSameSex) / sexTotal;
        const float match    = sameSex ? prefSame : (1.f - prefSame);
        sexBias += (match - 0.5f) * 20.f;  // [-10, +10] per partner
      }

      // 种族倾向
      const std::uint32_t raceTotal = stat.TimesSameRace + stat.TimesDiffRace;
      if (raceTotal > 0) {
        const bool sameRace      = (Define::Race(other) == actorRace);
        const float prefSameRace = static_cast<float>(stat.TimesSameRace) / raceTotal;
        const float raceMatch    = sameRace ? prefSameRace : (1.f - prefSameRace);
        raceBias += (raceMatch - 0.5f) * 10.f;  // [-5, +5] per partner
      }
    }

    enjoy += std::clamp(sexBias, -10.f, 10.f);
    enjoy += std::clamp(raceBias, -5.f, 5.f);
  }

  return std::clamp(enjoy, -100.f, 100.f);
}

// ════════════════════════════════════════════════════════════
// UpdateOnSceneEnd
//
// 仅在场景销毁时调用一次。
// 根据场景标签和最终 enjoyment 更新:
//   - 各经验类型 XP (enjoyment 越高, 收益越多)
//   - recentPartners 列表 (上限 20)
//   - 性别 / 种族倾向计数
//   - arouse 重置
// ════════════════════════════════════════════════════════════
void ActorStat::UpdateOnSceneEnd(const std::vector<RE::Actor*>& actors, const Define::Scene* scene,
                                 const std::unordered_map<RE::Actor*, EndSceneRecord>& records)
{
  if (!scene || actors.empty())
    return;

  const auto& tags        = scene->GetTags();
  const bool isSolo       = actors.size() == 1;
  const bool isGroup      = actors.size() > 2;
  const bool isAggressive = tags.Has(Define::SceneTags::Type::Aggressive);
  const bool isOral       = tags.Has(Define::SceneTags::Type::Oral);
  const bool isVaginal    = tags.Has(Define::SceneTags::Type::Vaginal);
  const bool isAnal       = tags.Has(Define::SceneTags::Type::Anal);

  for (auto* actor : actors) {
    if (!actor)
      continue;

    const auto recIt       = records.find(actor);
    const float enjoy      = recIt != records.end() ? recIt->second.enjoyment : 0.f;
    const bool isAggressor = recIt != records.end() ? recIt->second.isAggressor : false;

    // XP 乘数: enjoy=-100 → ×0.5, enjoy=0 → ×1.0, enjoy=+100 → ×1.5
    const float xpMult = 1.f + enjoy / 200.f;

    AddXP(actor, ExperienceType::Main, 1.f * xpMult);
    if (isSolo)
      AddXP(actor, ExperienceType::Solo, 1.f * xpMult);
    if (isGroup)
      AddXP(actor, ExperienceType::Group, 1.f * xpMult);
    if (isOral)
      AddXP(actor, ExperienceType::Oral, 1.f * xpMult);
    if (isVaginal)
      AddXP(actor, ExperienceType::Vagina, 1.f * xpMult);
    if (isAnal)
      AddXP(actor, ExperienceType::Anus, 1.f * xpMult);
    if (isAggressive) {
      if (isAggressor)
        AddXP(actor, ExperienceType::Aggressive, 1.f * xpMult);
      else
        AddXP(actor, ExperienceType::Submissive, 1.f * xpMult);
    }

    // 更新 partner 记录 + 倾向计数
    auto& stat = GetStat(actor);

    for (auto* other : actors) {
      if (!other || other == actor)
        continue;

      // recentPartners (上限 20)
      if (auto* base = other->GetActorBase()) {
        std::string name(base->GetName());
        if (name.empty())
          name = "Unknown";
        if (stat.recentPartners.size() >= 20)
          stat.recentPartners.erase(stat.recentPartners.begin());
        stat.recentPartners.push_back(std::move(name));
      }

      // 性别倾向
      if (Define::Gender::GetGender(other) == Define::Gender::GetGender(actor))
        ++stat.TimesSameSex;
      else
        ++stat.TimesDiffSex;

      // 种族倾向
      if (Define::Race(other) == Define::Race(actor))
        ++stat.TimesSameRace;
      else
        ++stat.TimesDiffRace;
    }

    // 场景结束，清零性欲残留
    stat.arouse = 0.f;
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
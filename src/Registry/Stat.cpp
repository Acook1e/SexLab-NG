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
// UpdateEnjoyment
//
// 场景更新时根据 actor 当前 stat 计算 enjoyment 值，供场景分配/动画调整等使用。
// 计算时考虑以下因素：
//   A. 性欲残留（arouse）
//   B. 综合经验等级（Main）
//   C. 专项经验等级（Oral/Vagina/Anus）
//   D. 当前的交互类型
// ════════════════════════════════════════════════════════════
void ActorStat::UpdateEnjoyment(RE::Actor* actor, const Define::Scene* scene,
                                const Define::Position& position,
                                const Instance::Interact::ActorData& interactData)
{
  if (!actor)
    return;

  auto& stat       = GetStat(actor);
  const auto& tags = scene->GetTags();

  // TODO
}

// ════════════════════════════════════════════════════════════
// UpdateStat
//
// 仅在场景销毁时调用一次。
// 根据场景标签和最终 enjoyment 更新:
//   - 各经验类型 XP (enjoyment 越高, climax 次数越多 ,收益越多)
//   - recentPartners 列表 (上限 20)
//   - 性别 / 种族倾向计数
//   - arouse 更新
// ════════════════════════════════════════════════════════════
void ActorStat::UpdateStat(
    std::unordered_map<RE::Actor*, Instance::SceneInstance::SceneActorInfo> actorInfoMap,
    const Define::Scene* scene)
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

  // TODO
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
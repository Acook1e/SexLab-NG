#include "Registry/Stat.h"

#include "Utils/Serialization.h"

namespace Registry
{
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
    if (auto it = actorStats.find(actor->GetFormID()); it != actorStats.end())
      return it->second;
    actorStats[actor->GetFormID()] = Stat();
    return actorStats[actor->GetFormID()];
  } else {
    if (auto it = runtimeActorStats.find(actor); it != runtimeActorStats.end())
      return it->second;
    runtimeActorStats[actor] = Stat();
    return runtimeActorStats[actor];
  }
}

void ActorStat::onSave(SKSE::SerializationInterface* serial)
{
  // Clear null references before saving
  std::erase_if(actorStats, [](const auto& pair) {
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(pair.first);
    return !actor;
  });

  std::size_t size = actorStats.size();
  serial->WriteRecordData(&size, sizeof(size));
  for (auto& [formID, stat] : actorStats) {
    serial->WriteRecordData(&formID, sizeof(formID));
    std::uint16_t data[12] = {
        stat.SexualExperience,           stat.SexualExperienceOral,       stat.SexualExperienceVaginal,
        stat.SexualExperienceAnal,       stat.SexualExperienceSolo,       stat.SexualExperienceGroup,
        stat.SexualExperienceAggressive, stat.SexualExperienceSubmissive, stat.SexualExperienceFemale,
        stat.SexualExperienceMale,       stat.SexualExperienceFuta,       stat.SexualExperienceCreature,
    };
    serial->WriteRecordData(data, sizeof(data));
    auto partners = join(stat.recentPartners, ';');
    auto size     = partners.size();
    serial->WriteRecordData(&size, sizeof(size));
    serial->WriteRecordData(partners.data(), size);
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
    std::uint16_t data[12];
    if (!serial->ReadRecordData(data, sizeof(data)))
      return;
    stat.SexualExperience           = data[0];
    stat.SexualExperienceOral       = data[1];
    stat.SexualExperienceVaginal    = data[2];
    stat.SexualExperienceAnal       = data[3];
    stat.SexualExperienceSolo       = data[4];
    stat.SexualExperienceGroup      = data[5];
    stat.SexualExperienceAggressive = data[6];
    stat.SexualExperienceSubmissive = data[7];
    stat.SexualExperienceFemale     = data[8];
    stat.SexualExperienceMale       = data[9];
    stat.SexualExperienceFuta       = data[10];
    stat.SexualExperienceCreature   = data[11];
    std::size_t partnersSize;
    if (!serial->ReadRecordData(&partnersSize, sizeof(partnersSize)))
      return;
    std::string partners(partnersSize, '\0');
    if (!serial->ReadRecordData(partners.data(), partnersSize))
      return;
    stat.recentPartners = std::move(split(partners, ';'));
    actorStats[formID]  = std::move(stat);
  }
}

void ActorStat::onRevert(SKSE::SerializationInterface* serial)
{
  actorStats.clear();
  runtimeActorStats.clear();
}

}  // namespace Registry
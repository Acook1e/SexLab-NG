#pragma once

namespace Registry
{

class ActorStat
{
public:
  struct Stat
  {
    std::uint16_t SexualExperience        = 0;
    std::uint16_t SexualExperienceOral    = 0;
    std::uint16_t SexualExperienceVaginal = 0;
    std::uint16_t SexualExperienceAnal    = 0;

    std::uint16_t SexualExperienceSolo  = 0;
    std::uint16_t SexualExperienceGroup = 0;

    std::uint16_t SexualExperienceAggressive = 0;
    std::uint16_t SexualExperienceSubmissive = 0;

    std::uint16_t SexualExperienceFemale   = 0;
    std::uint16_t SexualExperienceMale     = 0;
    std::uint16_t SexualExperienceFuta     = 0;
    std::uint16_t SexualExperienceCreature = 0;

    std::vector<std::string> recentPartners{};
  };

  static ActorStat& GetSingleton()
  {
    static ActorStat instance;
    return instance;
  }

  Stat& GetStat(RE::Actor* actor);

  static void onSave(SKSE::SerializationInterface* serial);
  static void onLoad(SKSE::SerializationInterface* serial);
  static void onRevert(SKSE::SerializationInterface* serial);

private:
  ActorStat();
  // For serialization actorStats
  constexpr static std::uint32_t SerializationType = 'ASTA';
  static inline std::unordered_map<RE::FormID, Stat> actorStats;
  static inline std::unordered_map<RE::Actor*, Stat> runtimeActorStats;
};
}  // namespace Registry
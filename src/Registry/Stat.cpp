#include "Registry/Stat.h"

#include <array>
#include <numeric>
#include <unordered_set>

#include "Instance/Manager.h"
#include "Utils/Serialization.h"

namespace Registry
{

namespace
{
  using ExperienceType  = ActorStat::ExperienceType;
  using InteractionType = Instance::Interact::Type;
  using BodyPartName    = Define::BodyPart::Name;

  constexpr std::size_t kInteractionProfileCount = static_cast<std::size_t>(InteractionType::Total);
  constexpr std::size_t kExperienceTypeCount     = static_cast<std::size_t>(ExperienceType::Total);
  constexpr float kEnjoyUpdateSeconds            = 0.10f;
  constexpr float kMaxEnjoyTickDelta             = 1.05f;
  constexpr float kMinSensitivityFactor          = 0.75f;
  constexpr float kMaxSensitivityFactor          = 1.30f;
  constexpr float kPostClimaxMinEnjoy            = -18.0f;
  constexpr float kPostClimaxMaxEnjoy            = 18.0f;
  constexpr float kArouseMultiplierCeil          = 1.25f;
  constexpr float kVelocityReference             = 0.08f;
  constexpr float kVelocityNormalization         = 2.50f;
  constexpr float kMainLevelSupportWeight        = 0.35f;
  constexpr float kMaxTrackXPPerScene            = 36.0f;
  constexpr float kMaxMainXPPerScene             = 32.0f;
  constexpr float kMaxSceneStyleXPPerScene       = 20.0f;
  constexpr float kTendencyPruneEpsilon          = 0.0005f;

  enum class InteractionFamily : std::uint8_t
  {
    Tender,
    Oral,
    Manual,
    External,
    Penetration,
    Multi,
    Utility
  };

  struct ExperienceCurve
  {
    float baseXP = 40.0f;
    float factor = 0.0048f;
  };

  struct InteractionProfile
  {
    ExperienceType representativeTrack = ExperienceType::Main;
    InteractionFamily family           = InteractionFamily::Utility;
    float baseEnjoy                    = 0.0f;
    float xpPerSecond                  = 0.0f;
    float tendencyGain                 = 0.0f;
    float neutralLevel                 = 1.0f;
    float levelCurve                   = 1.0f;
  };

  struct ActiveTrackKey
  {
    ExperienceType track = ExperienceType::Main;
    RE::Actor* partner   = nullptr;

    bool operator==(const ActiveTrackKey&) const = default;
  };

  struct ActiveTrackKeyHash
  {
    std::size_t operator()(const ActiveTrackKey& key) const
    {
      const auto typeHash  = static_cast<std::size_t>(std::to_underlying(key.track));
      const auto actorHash = std::hash<RE::Actor*>{}(key.partner);
      return typeHash ^ (actorHash + 0x9E3779B97F4A7C15ull + (typeHash << 6) + (typeHash >> 2));
    }
  };

  struct InteractionTickResult
  {
    float delta                      = 0.0f;
    std::uint32_t activeInteractions = 0;
    std::array<float, kExperienceTypeCount> exposureSeconds{};
  };

  struct SceneRuntimeProgress
  {
    std::array<float, kExperienceTypeCount> exposureSeconds{};
    float accumulatedEnjoyValue = 0.0f;
    float peakEnjoyValue        = static_cast<float>(Define::Enjoyment::Degree::Neutral);
    std::uint32_t sampledTicks  = 0;
  };

  static std::unordered_map<RE::Actor*, SceneRuntimeProgress> activeSceneProgress{};

  constexpr std::array<ExperienceType, 7> kBodyTracks{
      ExperienceType::Mouth,  ExperienceType::Breast, ExperienceType::Hand,  ExperienceType::Body,
      ExperienceType::Vagina, ExperienceType::Anus,   ExperienceType::Penis,
  };

  constexpr std::size_t ToInteractionIndex(InteractionType type)
  {
    return static_cast<std::size_t>(type);
  }

  constexpr std::size_t ToExperienceIndex(ExperienceType type)
  {
    return static_cast<std::size_t>(type);
  }

  constexpr bool IsTrackType(ExperienceType type)
  {
    return type != ExperienceType::Total;
  }

  constexpr bool IsBodyTrack(ExperienceType type)
  {
    switch (type) {
    case ExperienceType::Mouth:
    case ExperienceType::Breast:
    case ExperienceType::Hand:
    case ExperienceType::Body:
    case ExperienceType::Vagina:
    case ExperienceType::Anus:
    case ExperienceType::Penis:
      return true;
    default:
      return false;
    }
  }

  constexpr bool IsTrackedInteraction(InteractionType type)
  {
    return type > InteractionType::None && type < InteractionType::Total;
  }

  constexpr ExperienceType GetTrackForBodyPart(BodyPartName part)
  {
    switch (part) {
    case BodyPartName::Mouth:
    case BodyPartName::Throat:
      return ExperienceType::Mouth;
    case BodyPartName::BreastLeft:
    case BodyPartName::BreastRight:
    case BodyPartName::Cleavage:
      return ExperienceType::Breast;
    case BodyPartName::HandLeft:
    case BodyPartName::HandRight:
    case BodyPartName::FingerLeft:
    case BodyPartName::FingerRight:
      return ExperienceType::Hand;
    case BodyPartName::Vagina:
      return ExperienceType::Vagina;
    case BodyPartName::Anus:
      return ExperienceType::Anus;
    case BodyPartName::Penis:
      return ExperienceType::Penis;
    case BodyPartName::Belly:
    case BodyPartName::ThighLeft:
    case BodyPartName::ThighRight:
    case BodyPartName::ThighCleft:
    case BodyPartName::ButtLeft:
    case BodyPartName::ButtRight:
    case BodyPartName::GlutealCleft:
    case BodyPartName::FootLeft:
    case BodyPartName::FootRight:
      return ExperienceType::Body;
    default:
      return ExperienceType::Main;
    }
  }

  static const std::array<InteractionProfile, kInteractionProfileCount> kInteractionProfiles{{
      {ExperienceType::Main, InteractionFamily::Utility, 0.00f, 0.00f, 0.0000f, 1.0f, 1.0f},
      {ExperienceType::Mouth, InteractionFamily::Tender, 0.08f, 0.14f, 0.0038f, 10.0f, 8.0f},
      {ExperienceType::Breast, InteractionFamily::Oral, 0.16f, 0.22f, 0.0055f, 16.0f, 11.0f},
      {ExperienceType::Body, InteractionFamily::Oral, 0.10f, 0.16f, 0.0040f, 20.0f, 14.0f},
      {ExperienceType::Vagina, InteractionFamily::Oral, 0.24f, 0.34f, 0.0070f, 16.0f, 10.0f},
      {ExperienceType::Anus, InteractionFamily::Oral, 0.18f, 0.28f, 0.0060f, 20.0f, 12.0f},
      {ExperienceType::Penis, InteractionFamily::Oral, 0.22f, 0.32f, 0.0068f, 16.0f, 10.0f},
      {ExperienceType::Penis, InteractionFamily::Oral, 0.28f, 0.40f, 0.0080f, 24.0f, 13.0f},
      {ExperienceType::Mouth, InteractionFamily::Utility, 0.10f, 0.16f, 0.0025f, 14.0f, 11.0f},
      {ExperienceType::Breast, InteractionFamily::Manual, 0.12f, 0.18f, 0.0045f, 12.0f, 10.0f},
      {ExperienceType::Breast, InteractionFamily::External, 0.24f, 0.30f, 0.0060f, 20.0f, 13.0f},
      {ExperienceType::Body, InteractionFamily::Manual, 0.10f, 0.16f, 0.0042f, 12.0f, 10.0f},
      {ExperienceType::Body, InteractionFamily::Manual, 0.13f, 0.18f, 0.0045f, 12.0f, 10.0f},
      {ExperienceType::Body, InteractionFamily::Manual, 0.08f, 0.14f, 0.0038f, 18.0f, 13.0f},
      {ExperienceType::Vagina, InteractionFamily::Penetration, 0.18f, 0.26f, 0.0058f, 16.0f, 10.0f},
      {ExperienceType::Anus, InteractionFamily::Penetration, 0.16f, 0.24f, 0.0055f, 20.0f, 12.0f},
      {ExperienceType::Penis, InteractionFamily::Manual, 0.18f, 0.26f, 0.0058f, 14.0f, 10.0f},
      {ExperienceType::Body, InteractionFamily::Manual, 0.16f, 0.22f, 0.0055f, 12.0f, 10.0f},
      {ExperienceType::Hand, InteractionFamily::Utility, 0.08f, 0.14f, 0.0025f, 14.0f, 11.0f},
      {ExperienceType::Body, InteractionFamily::External, 0.12f, 0.18f, 0.0044f, 18.0f, 12.0f},
      {ExperienceType::Body, InteractionFamily::External, 0.14f, 0.20f, 0.0048f, 18.0f, 12.0f},
      {ExperienceType::Body, InteractionFamily::External, 0.16f, 0.22f, 0.0050f, 18.0f, 12.0f},
      {ExperienceType::Body, InteractionFamily::External, 0.12f, 0.18f, 0.0044f, 20.0f, 14.0f},
      {ExperienceType::Vagina, InteractionFamily::Penetration, 0.20f, 0.28f, 0.0062f, 16.0f, 11.0f},
      {ExperienceType::Vagina, InteractionFamily::Penetration, 0.28f, 0.40f, 0.0080f, 16.0f, 10.0f},
      {ExperienceType::Vagina, InteractionFamily::Utility, 0.09f, 0.15f, 0.0025f, 16.0f, 11.0f},
      {ExperienceType::Anus, InteractionFamily::Penetration, 0.24f, 0.36f, 0.0072f, 20.0f, 12.0f},
      {ExperienceType::Anus, InteractionFamily::Utility, 0.08f, 0.14f, 0.0025f, 18.0f, 12.0f},
      {ExperienceType::Penis, InteractionFamily::Utility, 0.08f, 0.14f, 0.0025f, 16.0f, 11.0f},
      {ExperienceType::Mouth, InteractionFamily::Multi, 0.12f, 0.20f, 0.0050f, 24.0f, 14.0f},
      {ExperienceType::Body, InteractionFamily::Multi, 0.14f, 0.22f, 0.0058f, 28.0f, 16.0f},
      {ExperienceType::Body, InteractionFamily::Multi, 0.15f, 0.24f, 0.0060f, 30.0f, 16.0f},
      {ExperienceType::Body, InteractionFamily::Multi, 0.18f, 0.28f, 0.0068f, 36.0f, 18.0f},
  }};

  static_assert(kInteractionProfiles.size() == kInteractionProfileCount);

  static const InteractionProfile& GetInteractionProfile(InteractionType type)
  {
    if (!IsTrackedInteraction(type))
      return kInteractionProfiles.front();
    return kInteractionProfiles[ToInteractionIndex(type)];
  }

  static ExperienceType GetRepresentativeTrack(InteractionType type)
  {
    return GetInteractionProfile(type).representativeTrack;
  }

  static ExperienceCurve GetExperienceCurve(ExperienceType type)
  {
    switch (type) {
    case ExperienceType::Main:
      return {48.0f, 0.0048f};
    case ExperienceType::Mouth:
      return {44.0f, 0.0047f};
    case ExperienceType::Breast:
      return {42.0f, 0.0046f};
    case ExperienceType::Hand:
      return {40.0f, 0.0045f};
    case ExperienceType::Body:
      return {42.0f, 0.0046f};
    case ExperienceType::Vagina:
      return {46.0f, 0.0048f};
    case ExperienceType::Anus:
      return {47.0f, 0.0049f};
    case ExperienceType::Penis:
      return {45.0f, 0.0048f};
    case ExperienceType::Solo:
      return {38.0f, 0.0045f};
    case ExperienceType::Group:
      return {54.0f, 0.0052f};
    case ExperienceType::Aggressive:
    case ExperienceType::Submissive:
      return {45.0f, 0.0049f};
    case ExperienceType::Total:
      return {48.0f, 0.0048f};
    default:
      return {48.0f, 0.0048f};
    }
  }

  static float XPToNextLevel(ExperienceType type, std::uint16_t currentLevel)
  {
    const auto curve = GetExperienceCurve(type);
    return curve.baseXP * std::exp(curve.factor * static_cast<float>(currentLevel));
  }

  static std::uint16_t GetStoredLevel(const ActorStat::Stat& stat, ExperienceType type)
  {
    const auto it = stat.experienceLevels.find(type);
    return it != stat.experienceLevels.end() ? it->second.level : 1;
  }

  static float GetStoredTendency(const ActorStat::Stat& stat, ExperienceType type)
  {
    const auto it = stat.interactionTendencies.find(type);
    return it != stat.interactionTendencies.end() ? it->second : 0.0f;
  }

  static float GetEffectiveTrackLevel(const ActorStat::Stat& stat, ExperienceType type)
  {
    if (!IsBodyTrack(type))
      return 1.0f;

    const float trackLevel = static_cast<float>(GetStoredLevel(stat, type));
    const float mainLevel  = static_cast<float>(GetStoredLevel(stat, ExperienceType::Main));
    const float support    = (mainLevel - 1.0f) * kMainLevelSupportWeight;
    return std::clamp(trackLevel + support, 1.0f, 999.0f);
  }

  static float CalculateSkillFactor(float effectiveLevel, const InteractionProfile& profile)
  {
    const float normalized =
        (effectiveLevel - profile.neutralLevel) / (std::max)(profile.levelCurve, 1.0f);
    const float wave = 0.5f * (std::tanh(normalized) + 1.0f);
    return std::clamp(0.35f + wave * 0.75f, 0.35f, 1.10f);
  }

  static float CalculateTendencyFactor(const ActorStat::Stat& stat, ExperienceType type)
  {
    return std::clamp(1.0f + GetStoredTendency(stat, type) * 0.25f, 0.75f, 1.25f);
  }

  static float CalculateGenderFactor(Define::Gender::Type gender, ExperienceType track,
                                     InteractionFamily family)
  {
    float factor = 1.0f;

    switch (gender) {
    case Define::Gender::Type::Female:
      factor = 0.92f;
      break;
    case Define::Gender::Type::Male:
      factor = 1.10f;
      break;
    case Define::Gender::Type::Futa:
      factor = 1.03f;
      break;
    case Define::Gender::Type::CreatureMale:
      factor = 1.08f;
      break;
    case Define::Gender::Type::CreatureFemale:
      factor = 0.95f;
      break;
    case Define::Gender::Type::Unknown:
    default:
      factor = 1.0f;
      break;
    }

    switch (track) {
    case ExperienceType::Breast:
      factor += 0.01f;
      break;
    case ExperienceType::Hand:
      factor += 0.01f;
      break;
    case ExperienceType::Body:
      factor += 0.02f;
      break;
    case ExperienceType::Vagina:
      factor += 0.03f;
      break;
    case ExperienceType::Anus:
      factor += 0.04f;
      break;
    case ExperienceType::Penis:
      factor += 0.05f;
      break;
    case ExperienceType::Mouth:
    case ExperienceType::Main:
    case ExperienceType::Solo:
    case ExperienceType::Group:
    case ExperienceType::Aggressive:
    case ExperienceType::Submissive:
    case ExperienceType::Total:
    default:
      break;
    }

    switch (family) {
    case InteractionFamily::Tender:
      factor -= 0.02f;
      break;
    case InteractionFamily::Manual:
      factor += 0.01f;
      break;
    case InteractionFamily::External:
      factor += 0.02f;
      break;
    case InteractionFamily::Penetration:
      factor += 0.04f;
      break;
    case InteractionFamily::Multi:
      factor += 0.06f;
      break;
    case InteractionFamily::Oral:
    case InteractionFamily::Utility:
    default:
      break;
    }

    return std::clamp(factor, 0.84f, 1.22f);
  }

  static float CalculatePartnerPreferenceFactor(const ActorStat::Stat& stat, RE::Actor* actor,
                                                RE::Actor* partner)
  {
    if (!actor || !partner || actor == partner)
      return 1.0f;

    const auto selfGender    = Define::Gender(actor).Get();
    const auto partnerGender = Define::Gender(partner).Get();
    const auto selfRace      = Define::Race(actor).GetType();
    const auto partnerRace   = Define::Race(partner).GetType();

    const float sameSexRatio = (static_cast<float>(stat.TimesSameSex) + 1.0f) /
                               (static_cast<float>(stat.TimesSameSex + stat.TimesDiffSex) + 2.0f);
    const float sameRaceRatio =
        (static_cast<float>(stat.TimesSameRace) + 1.0f) /
        (static_cast<float>(stat.TimesSameRace + stat.TimesDiffRace) + 2.0f);

    const bool isSameSex  = selfGender == partnerGender;
    const bool isSameRace = selfRace == partnerRace;

    const float sexFactor =
        isSameSex ? std::lerp(0.93f, 1.07f, sameSexRatio) : std::lerp(1.07f, 0.93f, sameSexRatio);
    const float raceFactor = isSameRace ? std::lerp(0.96f, 1.04f, sameRaceRatio)
                                        : std::lerp(1.04f, 0.96f, sameRaceRatio);

    return std::clamp(sexFactor * raceFactor, 0.88f, 1.12f);
  }

  static bool IsAnimObjectInteraction(InteractionType type)
  {
    return type == InteractionType::MouthAnimObj || type == InteractionType::HandAnimObj ||
           type == InteractionType::VaginaAnimObj || type == InteractionType::AnalAnimObj ||
           type == InteractionType::PenisAnimObj;
  }

  static float CalculateObjectFactor(const Instance::Interact::InteractionState& interaction)
  {
    if (interaction.type == InteractionType::Masturbation)
      return 0.82f;
    if (IsAnimObjectInteraction(interaction.type))
      return 0.68f;
    if (!interaction.partner)
      return 0.86f;

    switch (interaction.type) {
    case InteractionType::Kiss:
      return 0.97f;
    case InteractionType::GropeBreast:
    case InteractionType::GropeThigh:
    case InteractionType::GropeButt:
      return 0.92f;
    case InteractionType::GropeFoot:
    case InteractionType::ToeSucking:
    case InteractionType::Footjob:
      return 0.84f;
    default:
      return 1.0f;
    }
  }

  static float CalculateSpeedFactor(float velocity)
  {
    const float speed      = std::abs(velocity);
    const float normalized = std::clamp(speed / kVelocityReference, 0.0f, kVelocityNormalization);
    const float shaped     = std::sqrt(normalized / kVelocityNormalization);
    return std::clamp(0.55f + shaped * 0.70f, 0.55f, 1.25f);
  }

  static float CalculateSensitivityFactor(const ActorStat::Stat& stat)
  {
    const float sensitive = std::clamp(stat.sensitive, -10.0f, 10.0f);
    return std::clamp(1.0f + sensitive * 0.03f, kMinSensitivityFactor, kMaxSensitivityFactor);
  }

  static float CalculateArouseMultiplier(const ActorStat::Stat& stat)
  {
    const float arouseRatio = std::clamp(stat.arouse / 100.0f, 0.0f, 1.0f);
    return std::lerp(1.0f, kArouseMultiplierCeil, arouseRatio);
  }

  static float CalculateClimaxArouseDrain(float currentArouse, std::uint8_t climaxCount)
  {
    const float arouse      = std::clamp(currentArouse, 0.0f, 100.0f);
    const float baseDrain   = 8.0f + arouse * 0.22f;
    const float chainFactor = std::clamp(
        1.0f + static_cast<float>(climaxCount > 0 ? climaxCount - 1 : 0) * 0.18f, 1.0f, 1.6f);
    return (std::min)(arouse, baseDrain * chainFactor);
  }

  static float CalculatePostClimaxEnjoy(const ActorStat::Stat& stat)
  {
    const float mainLevelRatio =
        (static_cast<float>(GetStoredLevel(stat, ExperienceType::Main)) - 1.0f) / 998.0f;
    const float sensitiveRatio = std::clamp(stat.sensitive, -10.0f, 10.0f) / 10.0f;
    const float arouseRatio    = std::clamp(stat.arouse / 100.0f, 0.0f, 1.0f);

    const float resetEnjoy =
        -10.0f + mainLevelRatio * 10.0f + sensitiveRatio * 8.0f + arouseRatio * 6.0f;
    return std::clamp(resetEnjoy, kPostClimaxMinEnjoy, kPostClimaxMaxEnjoy);
  }

  static void TrackSceneProgress(RE::Actor* actor, const InteractionTickResult& tick,
                                 float enjoyValue)
  {
    auto& progress = activeSceneProgress[actor];
    for (std::size_t index = 0; index < kExperienceTypeCount; ++index)
      progress.exposureSeconds[index] += tick.exposureSeconds[index];

    progress.accumulatedEnjoyValue += enjoyValue;
    progress.peakEnjoyValue = (std::max)(progress.peakEnjoyValue, enjoyValue);
    ++progress.sampledTicks;
  }

  static float CalculateAverageEnjoy(const SceneRuntimeProgress* progress, float fallbackValue)
  {
    if (!progress || progress->sampledTicks == 0)
      return fallbackValue;
    return progress->accumulatedEnjoyValue / static_cast<float>(progress->sampledTicks);
  }

  static float CalculatePeakEnjoy(const SceneRuntimeProgress* progress, float fallbackValue)
  {
    return progress ? progress->peakEnjoyValue : fallbackValue;
  }

  static float CalculateSceneOutcomeFactor(float averageEnjoy, float peakEnjoy)
  {
    const float averageRatio = std::clamp((averageEnjoy + 100.0f) / 200.0f, 0.0f, 1.0f);
    const float peakRatio    = std::clamp((peakEnjoy + 100.0f) / 200.0f, 0.0f, 1.0f);
    return std::lerp(0.75f, 1.18f, averageRatio) * std::lerp(0.95f, 1.10f, peakRatio);
  }

  static float CalculateClimaxExperienceFactor(std::uint8_t climaxCount)
  {
    return std::clamp(1.0f + static_cast<float>(climaxCount) * 0.18f, 1.0f, 1.54f);
  }

  static void AdjustInteractionTendency(ActorStat::Stat& stat, ExperienceType type, float exposure,
                                        const InteractionProfile& profile, float averageEnjoy,
                                        float peakEnjoy, std::uint8_t climaxCount)
  {
    if (type == ExperienceType::Total || exposure <= 0.0f)
      return;

    const float averageRatio = std::clamp((averageEnjoy + 100.0f) / 200.0f, 0.0f, 1.0f);
    const float peakRatio    = std::clamp((peakEnjoy + 100.0f) / 200.0f, 0.0f, 1.0f);
    const float climaxBonus  = (std::min)(0.20f, static_cast<float>(climaxCount) * 0.06f);
    const float disposition  = std::clamp(
        (averageRatio - 0.45f) + (peakRatio - 0.50f) * 0.50f + climaxBonus, -0.35f, 0.55f);

    float& tendency = stat.interactionTendencies[type];
    tendency = std::clamp(tendency + exposure * profile.tendencyGain * disposition, -1.0f, 1.0f);
  }

  static float GetTotalBodyExposure(const SceneRuntimeProgress* progress)
  {
    if (!progress)
      return 0.0f;

    float total = 0.0f;
    for (ExperienceType track : kBodyTracks)
      total += progress->exposureSeconds[ToExperienceIndex(track)];
    return total;
  }

  static void RememberRecentPartners(
      ActorStat::Stat& stat, RE::Actor* actor,
      const std::vector<std::tuple<RE::FormID, Define::Gender::Type, Define::Race::Type>>& peers)
  {
    if (!actor)
      return;

    for (const auto& [formID, _, __] : peers) {
      (void)_;
      (void)__;
      if (formID == actor->GetFormID())
        continue;

      const auto peerActor = RE::TESForm::LookupByID<RE::Actor>(formID);
      if (!peerActor)
        continue;

      const std::string peerName = peerActor->GetDisplayFullName();
      if (peerName.empty())
        continue;

      auto it = std::find(stat.recentPartners.begin(), stat.recentPartners.end(), peerName);
      if (it != stat.recentPartners.end())
        stat.recentPartners.erase(it);

      stat.recentPartners.insert(stat.recentPartners.begin(), peerName);
      if (stat.recentPartners.size() > 20)
        stat.recentPartners.resize(20);
    }
  }

  static void UpdatePartnerTendencyCounters(
      ActorStat::Stat& stat, RE::Actor* actor,
      const std::vector<std::tuple<RE::FormID, Define::Gender::Type, Define::Race::Type>>& peers)
  {
    if (!actor)
      return;

    const Define::Gender::Type selfGender = Define::Gender(actor).Get();
    const Define::Race::Type selfRace     = Define::Race(actor).GetType();

    for (const auto& [formID, peerGender, peerRace] : peers) {
      if (formID == actor->GetFormID())
        continue;

      if (peerGender == selfGender)
        ++stat.TimesSameSex;
      else
        ++stat.TimesDiffSex;

      if (peerRace == selfRace)
        ++stat.TimesSameRace;
      else
        ++stat.TimesDiffRace;
    }
  }

  static InteractionTickResult CalculateInteract(RE::Actor* actor, ActorStat::Stat& stat,
                                                 const Instance::Interact::ActorState& actorState)
  {
    InteractionTickResult result;
    std::unordered_map<ActiveTrackKey, float, ActiveTrackKeyHash> trackContributions;

    const float sensitivityFactor = CalculateSensitivityFactor(stat);
    const float arouseMultiplier  = CalculateArouseMultiplier(stat);
    const auto selfGender         = Define::Gender(actor).Get();

    for (const auto& [partName, partState] : actorState.parts) {
      const auto& interaction = partState.current;
      if (!IsTrackedInteraction(interaction.type))
        continue;

      const auto track = GetTrackForBodyPart(partName);
      if (!IsBodyTrack(track))
        continue;

      const auto& profile        = GetInteractionProfile(interaction.type);
      const float effectiveLv    = GetEffectiveTrackLevel(stat, track);
      const float skillFactor    = CalculateSkillFactor(effectiveLv, profile);
      const float tendencyFactor = CalculateTendencyFactor(stat, track);
      const float genderFactor   = CalculateGenderFactor(selfGender, track, profile.family);
      const float partnerFactor =
          CalculatePartnerPreferenceFactor(stat, actor, interaction.partner);
      const float speedFactor  = CalculateSpeedFactor(interaction.approachSpeed);
      const float objectFactor = CalculateObjectFactor(interaction);

      const float contribution = profile.baseEnjoy * skillFactor * tendencyFactor * genderFactor *
                                 partnerFactor * speedFactor * objectFactor * sensitivityFactor *
                                 arouseMultiplier;

      const ActiveTrackKey key{track, interaction.partner};
      auto [it, inserted] = trackContributions.try_emplace(key, contribution);
      if (!inserted)
        it->second = (std::max)(it->second, contribution);
    }

    for (const auto& [key, contribution] : trackContributions) {
      result.delta += contribution;
      result.exposureSeconds[ToExperienceIndex(key.track)] += kEnjoyUpdateSeconds;
    }

    result.delta              = std::clamp(result.delta, -kMaxEnjoyTickDelta, kMaxEnjoyTickDelta);
    result.activeInteractions = static_cast<std::uint32_t>(trackContributions.size());
    return result;
  }

  static double TotalXPForLevelProgress(ExperienceType type, const ActorStat::Level& level)
  {
    double total = level.XP;
    for (std::uint16_t current = 1; current < level.level; ++current)
      total += XPToNextLevel(type, current);
    return total;
  }

  static ActorStat::Level BuildLevelFromTotalXP(ExperienceType type, double totalXP)
  {
    ActorStat::Level level;

    while (level.level < 999) {
      const double needed = XPToNextLevel(type, level.level);
      if (totalXP < needed)
        break;
      totalXP -= needed;
      ++level.level;
    }

    level.XP = level.level >= 999 ? 0.0f : static_cast<float>(totalXP);
    return level;
  }

  static void MergeSerializedLevel(ActorStat::Stat& stat, ExperienceType target,
                                   const ActorStat::Level& sourceLevel)
  {
    if (!IsTrackType(target) || target == ExperienceType::Total)
      return;

    const double existing         = TotalXPForLevelProgress(target, stat.experienceLevels[target]);
    const double incoming         = TotalXPForLevelProgress(target, sourceLevel);
    stat.experienceLevels[target] = BuildLevelFromTotalXP(target, existing + incoming);
  }

  static ExperienceType MapLegacySerializedTrack(std::uint8_t raw)
  {
    switch (raw) {
    case 0:
      return ExperienceType::Main;
    case 1:
      return ExperienceType::Mouth;
    case 2:
      return ExperienceType::Vagina;
    case 3:
      return ExperienceType::Anus;
    case 4:
      return ExperienceType::Solo;
    case 5:
      return ExperienceType::Group;
    case 6:
      return ExperienceType::Aggressive;
    case 7:
      return ExperienceType::Submissive;
    case 8:
      return ExperienceType::Mouth;
    case 9:
      return ExperienceType::Breast;
    case 10:
      return ExperienceType::Body;
    case 11:
      return ExperienceType::Vagina;
    case 12:
      return ExperienceType::Anus;
    case 13:
    case 14:
      return ExperienceType::Penis;
    case 15:
      return ExperienceType::Mouth;
    case 16:
    case 17:
      return ExperienceType::Breast;
    case 18:
    case 19:
    case 20:
    case 24:
    case 26:
    case 27:
    case 28:
    case 29:
    case 37:
    case 38:
    case 39:
      return ExperienceType::Body;
    case 21:
    case 31:
    case 32:
      return ExperienceType::Vagina;
    case 22:
    case 33:
    case 34:
      return ExperienceType::Anus;
    case 23:
    case 35:
      return ExperienceType::Penis;
    case 25:
      return ExperienceType::Hand;
    case 30:
      return ExperienceType::Vagina;
    case 36:
      return ExperienceType::Mouth;
    case 41:
      return ExperienceType::Breast;
    case 42:
      return ExperienceType::Hand;
    case 43:
      return ExperienceType::Body;
    case 44:
      return ExperienceType::Penis;
    default:
      return ExperienceType::Total;
    }
  }

  static void PruneStatForSave(ActorStat::Stat& stat)
  {
    std::erase_if(stat.experienceLevels, [](const auto& entry) {
      return entry.second.level <= 1 && entry.second.XP <= 0.0f;
    });

    std::erase_if(stat.interactionTendencies, [](const auto& entry) {
      return std::abs(entry.second) < kTendencyPruneEpsilon;
    });
  }
}  // namespace

ActorStat::ActorStat()
{
  Serialization::RegisterSaveCallback(SerializationType, onSave);
  Serialization::RegisterLoadCallback(SerializationType, onLoad);
  Serialization::RegisterSaveCallback(ProfileSerializationType, onProfileSave);
  Serialization::RegisterLoadCallback(ProfileSerializationType, onProfileLoad);
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
  }

  auto [it, _] = runtimeActorStats.try_emplace(actor);
  return it->second;
}

const ActorStat::Stat& ActorStat::GetStat(RE::Actor* actor) const
{
  static const Stat nullStat;

  if (!actor)
    return nullStat;

  if (actor->GetActorBase()->IsUnique()) {
    const auto it = actorStats.find(actor->GetFormID());
    return it != actorStats.end() ? it->second : nullStat;
  }

  const auto it = runtimeActorStats.find(actor);
  return it != runtimeActorStats.end() ? it->second : nullStat;
}

void ActorStat::AddXP(RE::Actor* actor, ExperienceType type, float amount)
{
  if (!actor || type == ExperienceType::Total || amount <= 0.0f)
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
    entry.XP = 0.0f;
}

void ActorStat::AddInteractionXP(RE::Actor* actor, InteractionType type, float amount)
{
  if (!actor || !IsTrackedInteraction(type) || amount <= 0.0f)
    return;

  AddXP(actor, GetRepresentativeTrack(type), amount);
}

std::uint16_t ActorStat::GetTrackLevel(RE::Actor* actor, ExperienceType type) const
{
  if (!actor || !IsTrackType(type) || type == ExperienceType::Total)
    return 1;

  if (IsBodyTrack(type))
    return static_cast<std::uint16_t>(std::round(GetEffectiveTrackLevel(GetStat(actor), type)));

  return GetStoredLevel(GetStat(actor), type);
}

float ActorStat::GetTrackTendency(RE::Actor* actor, ExperienceType type) const
{
  if (!actor || !IsTrackType(type) || type == ExperienceType::Total)
    return 0.0f;

  return GetStoredTendency(GetStat(actor), type);
}

std::uint16_t ActorStat::GetInteractionLevel(RE::Actor* actor, InteractionType type) const
{
  if (!actor || !IsTrackedInteraction(type))
    return 1;

  return GetTrackLevel(actor, GetRepresentativeTrack(type));
}

float ActorStat::GetInteractionTendency(RE::Actor* actor, InteractionType type) const
{
  if (!actor || !IsTrackedInteraction(type))
    return 0.0f;

  return GetTrackTendency(actor, GetRepresentativeTrack(type));
}

void ActorStat::Update(float deltaGameHours)
{
  if (deltaGameHours <= 0.0f)
    return;

  auto processActor = [&](RE::Actor* actor, Stat& stat) {
    if (!actor)
      return;
    if (Instance::SceneManager::IsActorInScene(actor))
      return;

    activeSceneProgress.erase(actor);

    stat.arouse = (std::min)(100.0f, stat.arouse + 0.5f * deltaGameHours);

    float current = stat.enjoy.GetValue();
    current -= current * 0.6f * deltaGameHours;
    if (std::abs(current) < 0.1f)
      current = 0.0f;
    stat.enjoy.SetValue(current);
  };

  for (auto& [formID, stat] : actorStats) {
    const auto actor = RE::TESForm::LookupByID<RE::Actor>(formID);
    processActor(actor, stat);
  }

  for (auto& [actor, stat] : runtimeActorStats)
    processActor(actor, stat);
}

void ActorStat::UpdateEnjoyment(
    const Define::Scene* /*scene*/,
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
    const auto tick       = CalculateInteract(actor, stat, actorData);

    if (tick.activeInteractions == 0)
      continue;

    const float preClimaxEnjoy = std::clamp(
        stat.enjoy.GetValue() + tick.delta, static_cast<float>(Define::Enjoyment::Degree::MinValue),
        static_cast<float>(Define::Enjoyment::Degree::MaxValue));

    TrackSceneProgress(actor, tick, preClimaxEnjoy);
    stat.enjoy.SetValue(preClimaxEnjoy);

    if (preClimaxEnjoy < static_cast<float>(Define::Enjoyment::Degree::Climax))
      continue;

    const auto nextClimaxCount =
        (std::min)(static_cast<std::uint32_t>((std::numeric_limits<std::uint8_t>::max)()),
                   static_cast<std::uint32_t>(info.climaxCount) + 1u);
    info.climaxCount = static_cast<std::uint8_t>(nextClimaxCount);
    stat.arouse =
        (std::max)(0.0f, stat.arouse - CalculateClimaxArouseDrain(stat.arouse, info.climaxCount));
    stat.enjoy.SetValue(CalculatePostClimaxEnjoy(stat));
  }
}

void ActorStat::UpdateStat(
    const Define::Scene* scene,
    const std::unordered_map<RE::Actor*, Instance::SceneInstance::SceneActorInfo>& actorInfoMap)
{
  if (!scene || actorInfoMap.empty())
    return;

  const auto& tags        = scene->GetTags();
  const bool isSolo       = actorInfoMap.size() == 1;
  const bool isGroup      = actorInfoMap.size() > 2;
  const bool isAggressive = tags.Has(Define::SceneTags::Type::Aggressive);

  std::vector<std::tuple<RE::FormID, Define::Gender::Type, Define::Race::Type>> peers;
  peers.reserve(actorInfoMap.size());
  for (const auto& [actor, _] : actorInfoMap) {
    if (!actor)
      continue;
    peers.emplace_back(actor->GetFormID(), Define::Gender(actor).Get(),
                       Define::Race(actor).GetType());
  }

  for (const auto& [actor, info] : actorInfoMap) {
    if (!actor)
      continue;

    auto& stat             = GetStat(actor);
    const auto runtimeIt   = activeSceneProgress.find(actor);
    const auto* sceneTrace = runtimeIt != activeSceneProgress.end() ? &runtimeIt->second : nullptr;
    const float averageEnjoy  = CalculateAverageEnjoy(sceneTrace, stat.enjoy.GetValue());
    const float peakEnjoy     = CalculatePeakEnjoy(sceneTrace, averageEnjoy);
    const float outcome       = CalculateSceneOutcomeFactor(averageEnjoy, peakEnjoy);
    const float climaxMult    = CalculateClimaxExperienceFactor(info.climaxCount);
    const float totalExposure = GetTotalBodyExposure(sceneTrace);

    if (sceneTrace) {
      for (ExperienceType track : kBodyTracks) {
        const float exposure = sceneTrace->exposureSeconds[ToExperienceIndex(track)];
        if (exposure <= 0.0f)
          continue;

        const auto& profile = [&]() -> const InteractionProfile& {
          switch (track) {
          case ExperienceType::Mouth:
            return GetInteractionProfile(InteractionType::Kiss);
          case ExperienceType::Breast:
            return GetInteractionProfile(InteractionType::GropeBreast);
          case ExperienceType::Hand:
            return GetInteractionProfile(InteractionType::Handjob);
          case ExperienceType::Body:
            return GetInteractionProfile(InteractionType::Thighjob);
          case ExperienceType::Vagina:
            return GetInteractionProfile(InteractionType::Vaginal);
          case ExperienceType::Anus:
            return GetInteractionProfile(InteractionType::Anal);
          case ExperienceType::Penis:
            return GetInteractionProfile(InteractionType::Fellatio);
          case ExperienceType::Main:
          case ExperienceType::Solo:
          case ExperienceType::Group:
          case ExperienceType::Aggressive:
          case ExperienceType::Submissive:
          case ExperienceType::Total:
          default:
            return GetInteractionProfile(InteractionType::Kiss);
          }
        }();
        const float gainedXP =
            (std::min)(kMaxTrackXPPerScene, exposure * profile.xpPerSecond * outcome * climaxMult);

        AddXP(actor, track, gainedXP);
        AdjustInteractionTendency(stat, track, exposure, profile, averageEnjoy, peakEnjoy,
                                  info.climaxCount);
      }
    }

    if (totalExposure > 0.0f) {
      AddXP(actor, ExperienceType::Main,
            (std::min)(kMaxMainXPPerScene, totalExposure * 0.18f * outcome * climaxMult));

      if (isSolo) {
        AddXP(actor, ExperienceType::Solo,
              (std::min)(kMaxSceneStyleXPPerScene, totalExposure * 0.10f * outcome));
      }
      if (isGroup) {
        AddXP(actor, ExperienceType::Group,
              (std::min)(kMaxSceneStyleXPPerScene, totalExposure * 0.12f * outcome * climaxMult));
      }
      if (isAggressive) {
        const float roleXP = (std::min)(kMaxSceneStyleXPPerScene, totalExposure * 0.08f * outcome);
        AddXP(actor, ExperienceType::Aggressive, roleXP * 0.5f);
        AddXP(actor, ExperienceType::Submissive, roleXP * 0.5f);
      }
    }

    RememberRecentPartners(stat, actor, peers);
    UpdatePartnerTendencyCounters(stat, actor, peers);

    if (info.climaxCount == 0) {
      if (averageEnjoy >= 40.0f) {
        const float settle = 4.0f + std::clamp((averageEnjoy + 100.0f) / 200.0f, 0.0f, 1.0f) * 8.0f;
        stat.arouse        = (std::max)(0.0f, stat.arouse - settle);
      } else {
        stat.arouse = (std::min)(100.0f, stat.arouse + 2.0f);
      }
    }

    activeSceneProgress.erase(actor);
  }
}

void ActorStat::onSave(SKSE::SerializationInterface* serial)
{
  std::erase_if(actorStats, [](const auto& pair) {
    return !RE::TESForm::LookupByID<RE::Actor>(pair.first);
  });

  for (auto& [_, stat] : actorStats)
    PruneStatForSave(stat);

  const std::size_t size = actorStats.size();
  serial->WriteRecordData(&size, sizeof(size));

  for (auto& [formID, stat] : actorStats) {
    serial->WriteRecordData(&formID, sizeof(formID));

    const std::size_t expCount = stat.experienceLevels.size();
    serial->WriteRecordData(&expCount, sizeof(expCount));
    for (auto& [type, lvl] : stat.experienceLevels) {
      const auto rawType = static_cast<std::uint8_t>(type);
      serial->WriteRecordData(&rawType, sizeof(rawType));
      serial->WriteRecordData(&lvl.level, sizeof(lvl.level));
      serial->WriteRecordData(&lvl.XP, sizeof(lvl.XP));
    }

    serial->WriteRecordData(&stat.TimesSameSex, sizeof(stat.TimesSameSex));
    serial->WriteRecordData(&stat.TimesDiffSex, sizeof(stat.TimesDiffSex));
    serial->WriteRecordData(&stat.TimesSameRace, sizeof(stat.TimesSameRace));
    serial->WriteRecordData(&stat.TimesDiffRace, sizeof(stat.TimesDiffRace));
    serial->WriteRecordData(&stat.arouse, sizeof(stat.arouse));

    const float enjoyVal = stat.enjoy.GetValue();
    serial->WriteRecordData(&enjoyVal, sizeof(enjoyVal));

    auto partners     = join(stat.recentPartners, ';');
    auto partnersSize = partners.size();
    serial->WriteRecordData(&partnersSize, sizeof(partnersSize));
    serial->WriteRecordData(partners.data(), partnersSize);
  }
}

void ActorStat::onLoad(SKSE::SerializationInterface* serial)
{
  if (!loadStatePrepared) {
    actorStats.clear();
    runtimeActorStats.clear();
    activeSceneProgress.clear();
    loadStatePrepared = true;
  }

  std::size_t size = 0;
  if (!serial->ReadRecordData(&size, sizeof(size)))
    return;

  for (std::size_t i = 0; i < size; ++i) {
    std::uint32_t formID = 0;
    if (!serial->ReadRecordData(&formID, sizeof(formID)))
      return;

    Stat loadedStat;
    if (const auto existing = actorStats.find(formID); existing != actorStats.end()) {
      loadedStat.interactionTendencies = existing->second.interactionTendencies;
      loadedStat.sensitive             = existing->second.sensitive;
    }

    std::size_t expCount = 0;
    if (!serial->ReadRecordData(&expCount, sizeof(expCount)))
      return;

    for (std::size_t j = 0; j < expCount; ++j) {
      std::uint8_t rawType = 0;
      Level lvl;
      if (!serial->ReadRecordData(&rawType, sizeof(rawType)))
        return;
      if (!serial->ReadRecordData(&lvl.level, sizeof(lvl.level)))
        return;
      if (!serial->ReadRecordData(&lvl.XP, sizeof(lvl.XP)))
        return;

      const auto mappedType = MapLegacySerializedTrack(rawType);
      if (mappedType != ExperienceType::Total)
        MergeSerializedLevel(loadedStat, mappedType, lvl);
    }

    if (!serial->ReadRecordData(&loadedStat.TimesSameSex, sizeof(loadedStat.TimesSameSex)))
      return;
    if (!serial->ReadRecordData(&loadedStat.TimesDiffSex, sizeof(loadedStat.TimesDiffSex)))
      return;
    if (!serial->ReadRecordData(&loadedStat.TimesSameRace, sizeof(loadedStat.TimesSameRace)))
      return;
    if (!serial->ReadRecordData(&loadedStat.TimesDiffRace, sizeof(loadedStat.TimesDiffRace)))
      return;
    if (!serial->ReadRecordData(&loadedStat.arouse, sizeof(loadedStat.arouse)))
      return;

    float enjoyVal = 0.0f;
    if (!serial->ReadRecordData(&enjoyVal, sizeof(enjoyVal)))
      return;
    loadedStat.enjoy.SetValue(enjoyVal);

    std::size_t partnersSize = 0;
    if (!serial->ReadRecordData(&partnersSize, sizeof(partnersSize)))
      return;
    std::string partners(partnersSize, '\0');
    if (!serial->ReadRecordData(partners.data(), partnersSize))
      return;
    loadedStat.recentPartners = split(partners, ';');

    actorStats[formID] = std::move(loadedStat);
  }
}

void ActorStat::onProfileSave(SKSE::SerializationInterface* serial)
{
  std::erase_if(actorStats, [](const auto& pair) {
    return !RE::TESForm::LookupByID<RE::Actor>(pair.first);
  });

  for (auto& [_, stat] : actorStats)
    PruneStatForSave(stat);

  const std::size_t size = actorStats.size();
  serial->WriteRecordData(&size, sizeof(size));

  for (auto& [formID, stat] : actorStats) {
    serial->WriteRecordData(&formID, sizeof(formID));
    serial->WriteRecordData(&stat.sensitive, sizeof(stat.sensitive));

    const std::size_t tendencyCount = stat.interactionTendencies.size();
    serial->WriteRecordData(&tendencyCount, sizeof(tendencyCount));
    for (auto& [type, tendency] : stat.interactionTendencies) {
      const auto rawType = static_cast<std::uint8_t>(type);
      serial->WriteRecordData(&rawType, sizeof(rawType));
      serial->WriteRecordData(&tendency, sizeof(tendency));
    }
  }
}

void ActorStat::onProfileLoad(SKSE::SerializationInterface* serial)
{
  if (!loadStatePrepared) {
    actorStats.clear();
    runtimeActorStats.clear();
    activeSceneProgress.clear();
    loadStatePrepared = true;
  }

  std::size_t size = 0;
  if (!serial->ReadRecordData(&size, sizeof(size)))
    return;

  for (std::size_t i = 0; i < size; ++i) {
    std::uint32_t formID = 0;
    if (!serial->ReadRecordData(&formID, sizeof(formID)))
      return;

    float sensitive = 1.0f;
    if (!serial->ReadRecordData(&sensitive, sizeof(sensitive)))
      return;

    auto& stat     = actorStats[formID];
    stat.sensitive = sensitive;
    stat.interactionTendencies.clear();

    std::array<float, kExperienceTypeCount> tendencySums{};
    std::array<float, kExperienceTypeCount> tendencyWeights{};

    std::size_t tendencyCount = 0;
    if (!serial->ReadRecordData(&tendencyCount, sizeof(tendencyCount)))
      return;

    for (std::size_t j = 0; j < tendencyCount; ++j) {
      std::uint8_t rawType = 0;
      float tendency       = 0.0f;
      if (!serial->ReadRecordData(&rawType, sizeof(rawType)))
        return;
      if (!serial->ReadRecordData(&tendency, sizeof(tendency)))
        return;

      const auto mappedType = MapLegacySerializedTrack(rawType);
      if (mappedType == ExperienceType::Total)
        continue;

      tendencySums[ToExperienceIndex(mappedType)] += std::clamp(tendency, -1.0f, 1.0f);
      tendencyWeights[ToExperienceIndex(mappedType)] += 1.0f;
    }

    for (std::size_t index = 0; index < kExperienceTypeCount; ++index) {
      if (tendencyWeights[index] <= 0.0f)
        continue;

      const auto type = static_cast<ExperienceType>(index);
      stat.interactionTendencies[type] =
          std::clamp(tendencySums[index] / tendencyWeights[index], -1.0f, 1.0f);
    }
  }
}

void ActorStat::onRevert(SKSE::SerializationInterface* /*serial*/)
{
  actorStats.clear();
  runtimeActorStats.clear();
  activeSceneProgress.clear();
  loadStatePrepared = false;
}

}  // namespace Registry
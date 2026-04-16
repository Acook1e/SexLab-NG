#include "Instance/Interact.h"
#include "Instance/InteractAllocator.h"
#include "Instance/InteractGeometry.h"
#include "Instance/InteractTemporal.h"

#include "Utils/Settings.h"

#include "magic_enum/magic_enum.hpp"

namespace Instance::Detail
{

#pragma region RuntimeInternals

using Name                 = Define::BodyPart::Name;
using BP                   = Define::BodyPart;
using Type                 = Interact::Type;
using PartState            = Interact::PartState;
using ActorState           = Interact::ActorState;
using MotionSnapshot       = Interact::MotionSnapshot;
using MotionHistory        = Interact::MotionHistory;
using GenitalSlotKind      = Temporal::GenitalSlotKind;
using CandidateFamily      = Allocator::CandidateFamily;
using Candidate            = Allocator::Candidate;
using AssignInfo           = Allocator::AssignInfo;
using ActorInteractSummary = Allocator::ActorInteractSummary;
using FamilyLogStats       = Allocator::FamilyLogStats;
using AssignedCandidateLog = Allocator::AssignedCandidateLog;
using AssignMap            = Allocator::AssignMap;
using UsedPartSet          = Allocator::UsedPartSet;

namespace
{
  enum class AngleModel : std::uint8_t
  {
    Absolute = 0,
    Wrap,
  };

  struct AngleRange
  {
    float min       = 0.f;
    float max       = 180.f;
    AngleModel mode = AngleModel::Absolute;
  };

  struct PairContext
  {
    RE::Actor* actorA = nullptr;
    RE::Actor* actorB = nullptr;
    const ActorState& dataA;
    const ActorState& dataB;
    bool self = false;
  };

  struct DirectedContext
  {
    RE::Actor* sourceActor = nullptr;
    RE::Actor* targetActor = nullptr;
    const ActorState& sourceData;
    const ActorState& targetData;
    bool self = false;
  };

  struct SimpleDirectedSpec
  {
    const Name* sourceParts = nullptr;
    std::size_t sourceCount = 0;
    const Name* targetParts = nullptr;
    std::size_t targetCount = 0;
    Type type               = Type::None;
    CandidateFamily family  = CandidateFamily::Contact;
    float maxDistance       = 0.f;
    AngleRange angleRange{};
    float angleWeight       = 0.f;
    float scoreOffset       = 0.f;
    AnchorMode anchorMode   = AnchorMode::None;
    float anchorWeight      = 0.f;
    float anchorMaxDistance = 0.f;
  };

  struct PenetrationSpec
  {
    Name receiverPart = Name::Mouth;
    Type type         = Type::None;
    float maxDistance = 0.f;
    float maxAngle    = 0.f;
    float maxAxis     = 0.f;
    float minDepth    = 0.f;
    float maxDepth    = 0.f;
  };

  struct ProximitySpec
  {
    Name targetPart   = Name::Mouth;
    Type type         = Type::None;
    float maxDistance = 0.f;
    AngleRange angleRange{};
    float angleWeight       = 0.f;
    float scoreOffset       = 0.f;
    AnchorMode anchorMode   = AnchorMode::None;
    float anchorWeight      = 0.f;
    float anchorMaxDistance = 0.f;
  };

  struct SymmetricSpec
  {
    Name partA             = Name::Mouth;
    Name partB             = Name::Mouth;
    Type type              = Type::None;
    CandidateFamily family = CandidateFamily::Contact;
    float maxDistance      = 0.f;
    AngleRange angleRange{};
    float angleWeight = 0.f;
    float scoreOffset = 0.f;
  };

  struct PenisContextMetrics
  {
    float vaginalSupport       = std::numeric_limits<float>::infinity();
    float analSupport          = std::numeric_limits<float>::infinity();
    float vaginalEntryDistance = std::numeric_limits<float>::infinity();
    float analEntryDistance    = std::numeric_limits<float>::infinity();
  };

  static constexpr std::array<Name, 1> kMouthParts{Name::Mouth};
  static constexpr std::array<Name, 1> kThroatParts{Name::Throat};
  static constexpr std::array<Name, 1> kPenisParts{Name::Penis};
  static constexpr std::array<Name, 1> kVaginaParts{Name::Vagina};
  static constexpr std::array<Name, 1> kAnusParts{Name::Anus};
  static constexpr std::array<Name, 1> kThighCleftParts{Name::ThighCleft};
  static constexpr std::array<Name, 1> kGlutealCleftParts{Name::GlutealCleft};
  static constexpr std::array<Name, 2> kHandParts{Name::HandLeft, Name::HandRight};
  static constexpr std::array<Name, 2> kFingerParts{Name::FingerLeft, Name::FingerRight};
  static constexpr std::array<Name, 2> kBreastParts{Name::BreastLeft, Name::BreastRight};
  static constexpr std::array<Name, 2> kFootParts{Name::FootLeft, Name::FootRight};
  static constexpr std::array<Name, 2> kThighParts{Name::ThighLeft, Name::ThighRight};
  static constexpr std::array<Name, 2> kButtParts{Name::ButtLeft, Name::ButtRight};
  std::string_view NameText(Name value)
  {
    return magic_enum::enum_name<Define::BodyPart::Name>(value);
  }

  std::string_view TypeText(Type value)
  {
    return magic_enum::enum_name<Interact::Type>(value);
  }

  std::string_view FamilyText(CandidateFamily family)
  {
    switch (family) {
    case CandidateFamily::Penetration:
      return "Penetration";
    case CandidateFamily::Proximity:
      return "Proximity";
    case CandidateFamily::Contact:
      return "Contact";
    default:
      return "Unknown";
    }
  }

  bool AccumulateObservedType(Define::InteractTags& tags, Type type)
  {
    using TagType = Define::InteractTags::Type;

    switch (type) {
    case Type::Kiss:
      tags.Set(TagType::Kiss);
      return true;
    case Type::BreastSucking:
      tags.Set(TagType::BreastSucking);
      return true;
    case Type::ToeSucking:
      tags.Set(TagType::ToeSucking);
      return true;
    case Type::Cunnilingus:
      tags.Set(TagType::Cunnilingus);
      return true;
    case Type::Anilingus:
      tags.Set(TagType::Anilingus);
      return true;
    case Type::Fellatio:
      tags.Set(TagType::Fellatio);
      return true;
    case Type::DeepThroat:
      tags.Set(TagType::DeepThroat);
      return true;
    case Type::MouthAnimObj:
      tags.Set(TagType::MouthAnimObj);
      return true;
    case Type::GropeBreast:
      tags.Set(TagType::GropeBreast);
      return true;
    case Type::Titfuck:
      tags.Set(TagType::Titfuck);
      return true;
    case Type::GropeThigh:
      tags.Set(TagType::GropeThigh);
      return true;
    case Type::GropeButt:
      tags.Set(TagType::GropeButt);
      return true;
    case Type::GropeFoot:
      tags.Set(TagType::GropeFoot);
      return true;
    case Type::FingerVagina:
      tags.Set(TagType::FingerVagina);
      return true;
    case Type::FingerAnus:
      tags.Set(TagType::FingerAnus);
      return true;
    case Type::Handjob:
      tags.Set(TagType::Handjob);
      return true;
    case Type::Masturbation:
      tags.Set(TagType::Masturbation);
      return true;
    case Type::HandAnimObj:
      tags.Set(TagType::HandAnimObj);
      return true;
    case Type::Naveljob:
      tags.Set(TagType::Naveljob);
      return true;
    case Type::Thighjob:
      tags.Set(TagType::Thighjob);
      return true;
    case Type::Frottage:
      tags.Set(TagType::Frottage);
      return true;
    case Type::Footjob:
      tags.Set(TagType::Footjob);
      return true;
    case Type::Tribbing:
      tags.Set(TagType::Tribbing);
      return true;
    case Type::Vaginal:
      tags.Set(TagType::Vaginal);
      return true;
    case Type::VaginaAnimObj:
      tags.Set(TagType::VaginaAnimObj);
      return true;
    case Type::Anal:
      tags.Set(TagType::Anal);
      return true;
    case Type::AnalAnimObj:
      tags.Set(TagType::AnalAnimObj);
      return true;
    case Type::PenisAnimObj:
      tags.Set(TagType::PenisAnimObj);
      return true;
    case Type::SixtyNine:
      tags.Set(TagType::SixtyNine);
      return true;
    case Type::Spitroast:
      tags.Set(TagType::Spitroast);
      return true;
    case Type::DoublePenetration:
      tags.Set(TagType::DoublePenetration);
      return true;
    case Type::TriplePenetration:
      tags.Set(TagType::TriplePenetration);
      return true;
    case Type::None:
    case Type::Total:
      return false;
    }

    return false;
  }

  bool IsDebugEnabled()
  {
    return Settings::bDebugMode;
  }

  void LogFamilyOutcome(CandidateFamily family, const FamilyLogStats& stats,
                        const std::vector<AssignedCandidateLog>& assignedCandidates)
  {
    if (!IsDebugEnabled())
      return;

    logger::info("[SexLab NG] Interact {} total={} considered={} suppressed={} blocked={} "
                 "assigned={}",
                 FamilyText(family), stats.total, stats.considered, stats.suppressed, stats.blocked,
                 assignedCandidates.size());

    for (const auto& entry : assignedCandidates) {
      const Candidate& candidate = entry.candidate;
      logger::info("[SexLab NG] Interact {} assigned type={} resolved={} src={}:{} "
                   "dst={}:{} score={:.3f} distance={:.2f}",
                   FamilyText(family), TypeText(candidate.type), TypeText(entry.resolvedType),
                   candidate.sourceActor->GetDisplayFullName(), NameText(candidate.sourcePart),
                   candidate.targetActor->GetDisplayFullName(), NameText(candidate.targetPart),
                   candidate.score, candidate.distance);
    }
  }

  const PartState* TryGetPartState(const ActorState& data, Name name)
  {
    auto it = data.parts.find(name);
    return it == data.parts.end() ? nullptr : &it->second;
  }

  const MotionHistory* TryGetMotionHistory(const ActorState& data, Name name)
  {
    const PartState* state = TryGetPartState(data, name);
    return state ? &state->motion : nullptr;
  }

  const MotionSnapshot* TryGetCurrentSnapshot(const ActorState& data, Name name)
  {
    const MotionHistory* history = TryGetMotionHistory(data, name);
    if (!history || !history->current.valid)
      return nullptr;
    return &history->current;
  }

  template <std::size_t N>
  bool HasAnyCurrentPart(const ActorState& data, const std::array<Name, N>& names)
  {
    for (Name name : names) {
      if (TryGetCurrentSnapshot(data, name))
        return true;
    }
    return false;
  }

  MotionSnapshot BuildSnapshot(const BP& bodypart, float nowMs)
  {
    MotionSnapshot snapshot;
    if (!bodypart.IsValid())
      return snapshot;

    const auto& axisInfo = bodypart.GetAxisInfo();
    snapshot.start       = axisInfo.start;
    snapshot.end         = axisInfo.end;
    snapshot.direction   = axisInfo.direction;
    snapshot.length      = axisInfo.length;
    snapshot.timeMs      = nowMs;
    snapshot.valid       = true;
    snapshot.directional = bodypart.IsDirectional() && snapshot.length >= 1e-6f;
    if (const auto* collisionInfo = bodypart.GetCollisionInfo())
      snapshot.collisions = *collisionInfo;
    return snapshot;
  }

  Define::Vector3f ClampVectorLength(const Define::Vector3f& value, float maxLength)
  {
    const float length = value.norm();
    if (length <= maxLength || length < 1e-6f)
      return value;
    return value * (maxLength / length);
  }

  MotionSnapshot PredictSnapshot(const MotionHistory& history)
  {
    if (!history.current.valid)
      return {};
    if (!history.previous.valid)
      return history.current;

    const float dt = history.current.timeMs - history.previous.timeMs;
    if (dt <= 1e-4f)
      return history.current;

    Define::Vector3f startVelocity = (history.current.start - history.previous.start) / dt;
    Define::Vector3f endVelocity   = (history.current.end - history.previous.end) / dt;

    if (history.older.valid) {
      const float prevDt = history.previous.timeMs - history.older.timeMs;
      if (prevDt > 1e-4f) {
        const Define::Vector3f olderStartVelocity =
            (history.previous.start - history.older.start) / prevDt;
        const Define::Vector3f olderEndVelocity =
            (history.previous.end - history.older.end) / prevDt;
        startVelocity = startVelocity * 0.68f + olderStartVelocity * 0.32f;
        endVelocity   = endVelocity * 0.68f + olderEndVelocity * 0.32f;
      }
    }

    const float horizonMs     = std::clamp(dt, 8.f, 24.f);
    constexpr float kMaxShift = 14.f;
    MotionSnapshot predicted  = history.current;
    predicted.timeMs          = history.current.timeMs + horizonMs;
    predicted.start += ClampVectorLength(startVelocity * horizonMs, kMaxShift);
    predicted.end += ClampVectorLength(endVelocity * horizonMs, kMaxShift);

    const Define::Vector3f axis = predicted.end - predicted.start;
    predicted.length            = axis.norm();
    if (predicted.directional && predicted.length >= 1e-6f) {
      predicted.direction = axis / predicted.length;
    } else {
      predicted.directional = false;
      predicted.direction   = history.current.direction;
      predicted.end         = predicted.start;
      predicted.length      = 0.f;
    }

    return predicted;
  }

  bool IsAngleUnconstrained(AngleRange range)
  {
    return range.min <= 0.f && range.max >= 179.9f;
  }

  bool CheckAngle(float absAngle, AngleRange range)
  {
    if (IsAngleUnconstrained(range))
      return true;
    if (range.mode == AngleModel::Wrap || range.min > range.max)
      return absAngle <= range.max || absAngle >= range.min;
    return absAngle >= range.min && absAngle <= range.max;
  }

  float AngleDeviation(float absAngle, AngleRange range)
  {
    if (IsAngleUnconstrained(range))
      return 0.f;

    const bool wrap = range.mode == AngleModel::Wrap || range.min > range.max;
    if (!wrap) {
      const float half = (range.max - range.min) * 0.5f;
      const float mid  = (range.min + range.max) * 0.5f;
      return half > 1e-4f ? std::abs(absAngle - mid) / half : 0.f;
    }

    const float midLow   = range.max * 0.5f;
    const float midHigh  = (range.min + 180.f) * 0.5f;
    const float halfLow  = range.max > 1e-4f ? range.max * 0.5f : 1e-4f;
    const float halfHigh = (180.f - range.min) > 1e-4f ? (180.f - range.min) * 0.5f : 1e-4f;
    const float devLow   = std::abs(absAngle - midLow) / halfLow;
    const float devHigh  = std::abs(absAngle - midHigh) / halfHigh;
    return devLow < devHigh ? devLow : devHigh;
  }

  float NormalizeDistance(float distance, float maxDistance)
  {
    if (maxDistance <= 1e-4f)
      return 0.f;
    return std::clamp(distance / maxDistance, 0.f, 2.f);
  }

  float TemporalBonus(const ActorState& data, Name part, RE::Actor* partner, Type type)
  {
    return Temporal::InteractionBonus(data, part, partner, type);
  }

  bool HadPreviousInteraction(const ActorState& data, Name part, RE::Actor* partner, Type type)
  {
    return Temporal::HadPreviousInteraction(data, part, partner, type);
  }

  bool HasStickyInteractionPair(const DirectedContext& ctx, Name sourcePart, Name targetPart,
                                Type type)
  {
    return Temporal::HasStickyPair(ctx.sourceData, sourcePart, ctx.targetActor, ctx.targetData,
                                   targetPart, ctx.sourceActor, type);
  }

  bool IncludesVaginal(Type type)
  {
    return Temporal::IncludesVaginal(type);
  }

  bool IncludesAnal(Type type)
  {
    return Temporal::IncludesAnal(type);
  }

  const Interact::SlotMemory* TryGetRecentReceiverSlotMemory(const ActorState& receiverData,
                                                             Type type)
  {
    return Temporal::TryGetRecentReceiverSlotMemory(receiverData, type);
  }

  bool HasStrongRearPreference(float vaginalMetric, float analMetric, float tolerance = 0.55f)
  {
    if (!std::isfinite(analMetric))
      return false;
    if (!std::isfinite(vaginalMetric))
      return true;
    return analMetric + tolerance < vaginalMetric;
  }

  bool HasStrongFrontPreference(float vaginalMetric, float analMetric, float tolerance = 0.55f)
  {
    if (!std::isfinite(vaginalMetric))
      return false;
    if (!std::isfinite(analMetric))
      return true;
    return vaginalMetric + tolerance < analMetric;
  }

  float MeasureSpatialDistanceByName(const ActorState& data, Name lhs, const MotionSnapshot& rhs)
  {
    const MotionSnapshot* lhsPart = TryGetCurrentSnapshot(data, lhs);
    return lhsPart ? Geometry::MeasureSpatialDistance(*lhsPart, rhs)
                   : std::numeric_limits<float>::infinity();
  }

  PenisContextMetrics BuildPenisContextMetrics(const ActorState& receiverData,
                                               const MotionSnapshot& penisSnapshot)
  {
    PenisContextMetrics metrics;
    metrics.vaginalSupport =
        MeasureSpatialDistanceByName(receiverData, Name::ThighCleft, penisSnapshot);
    metrics.analSupport =
        MeasureSpatialDistanceByName(receiverData, Name::GlutealCleft, penisSnapshot);

    if (const MotionSnapshot* vaginaPart = TryGetCurrentSnapshot(receiverData, Name::Vagina)) {
      metrics.vaginalEntryDistance = Geometry::SampledShaftEntryDistance(
          Geometry::MeasureSampledMetrics(penisSnapshot, *vaginaPart));
    }
    if (const MotionSnapshot* anusPart = TryGetCurrentSnapshot(receiverData, Name::Anus)) {
      metrics.analEntryDistance = Geometry::SampledShaftEntryDistance(
          Geometry::MeasureSampledMetrics(penisSnapshot, *anusPart));
    }

    return metrics;
  }

  float ComputeFrontRearBias(const PenisContextMetrics& metrics, Type type)
  {
    if (type != Type::Vaginal && type != Type::Anal)
      return 0.f;

    constexpr float kSupportScale           = 6.5f;
    constexpr float kSupportMaxBias         = 0.34f;
    constexpr float kSupportVaginalTieBreak = 0.10f;
    constexpr float kEntryScale             = 5.2f;
    constexpr float kEntryMaxBias           = 0.24f;
    constexpr float kEntryVaginalTieBreak   = 0.06f;
    constexpr float kTotalMaxBias           = 0.52f;

    float bias = 0.f;

    if (std::isfinite(metrics.vaginalSupport) && std::isfinite(metrics.analSupport)) {
      const float delta = std::clamp((metrics.vaginalSupport - metrics.analSupport) / kSupportScale,
                                     -kSupportMaxBias, kSupportMaxBias);
      const float tieBreak =
          metrics.vaginalSupport <= metrics.analSupport + 1.0f ? kSupportVaginalTieBreak : 0.f;
      bias += type == Type::Vaginal ? delta - tieBreak : -delta + tieBreak;
    } else if (type == Type::Vaginal && std::isfinite(metrics.vaginalSupport)) {
      bias +=
          -std::clamp(1.f - metrics.vaginalSupport / kSupportScale, 0.f, kSupportMaxBias) - 0.05f;
    } else if (type == Type::Anal && std::isfinite(metrics.analSupport)) {
      bias += -std::clamp(1.f - metrics.analSupport / kSupportScale, 0.f, kSupportMaxBias);
    }

    if (std::isfinite(metrics.vaginalEntryDistance) && std::isfinite(metrics.analEntryDistance)) {
      const float delta =
          std::clamp((metrics.vaginalEntryDistance - metrics.analEntryDistance) / kEntryScale,
                     -kEntryMaxBias, kEntryMaxBias);
      const float tieBreak = metrics.vaginalEntryDistance <= metrics.analEntryDistance + 0.75f
                                 ? kEntryVaginalTieBreak
                                 : 0.f;
      bias += type == Type::Vaginal ? delta - tieBreak : -delta + tieBreak;
    } else if (type == Type::Vaginal && std::isfinite(metrics.vaginalEntryDistance)) {
      bias +=
          -std::clamp(1.f - metrics.vaginalEntryDistance / kEntryScale, 0.f, kEntryMaxBias) - 0.03f;
    } else if (type == Type::Anal && std::isfinite(metrics.analEntryDistance)) {
      bias += -std::clamp(1.f - metrics.analEntryDistance / kEntryScale, 0.f, kEntryMaxBias);
    }

    return std::clamp(bias, -kTotalMaxBias, kTotalMaxBias);
  }

  float ComputeAnalAngleAllowance(const PenisContextMetrics& metrics, float rootEntryDistance,
                                  float shaftDistance)
  {
    float allowance = 0.f;

    if (std::isfinite(metrics.analSupport)) {
      if (metrics.analSupport <= 6.0f)
        allowance += 3.0f;
      if (metrics.analSupport <= 5.0f)
        allowance += 3.0f;
      if (metrics.analSupport <= 4.0f)
        allowance += 2.0f;
    }

    if (std::isfinite(metrics.analEntryDistance)) {
      if (metrics.analEntryDistance <= 5.0f)
        allowance += 1.5f;
      if (metrics.analEntryDistance <= 4.0f)
        allowance += 1.5f;
    }

    if (std::isfinite(metrics.vaginalSupport) && std::isfinite(metrics.analSupport) &&
        metrics.analSupport + 0.75f < metrics.vaginalSupport) {
      allowance += 1.5f;
    }

    if (std::isfinite(metrics.vaginalEntryDistance) && std::isfinite(metrics.analEntryDistance) &&
        metrics.analEntryDistance + 0.75f < metrics.vaginalEntryDistance) {
      allowance += 1.5f;
    }

    // Close rear contacts are often animated more horizontally than the anus channel axis.
    if (rootEntryDistance <= 4.5f)
      allowance += 1.0f;
    if (rootEntryDistance <= 3.0f)
      allowance += 1.5f;
    if (rootEntryDistance <= 2.0f)
      allowance += 2.5f;
    if (rootEntryDistance <= 1.0f)
      allowance += 3.5f;

    if (shaftDistance <= 5.0f)
      allowance += 1.0f;
    if (shaftDistance <= 3.5f)
      allowance += 1.5f;
    if (shaftDistance <= 2.5f)
      allowance += 2.0f;
    if (shaftDistance <= 1.5f)
      allowance += 2.5f;

    if (rootEntryDistance <= 1.25f && shaftDistance <= 2.5f)
      allowance += 3.0f;

    return std::clamp(allowance, 0.f, 22.f);
  }

  bool IsNearThreshold(float currentValue, float predictedValue, float limit, float margin)
  {
    return currentValue <= limit + margin || predictedValue <= limit + margin;
  }

  bool ResolvePenetrationContact(const SampledMetrics& metrics, float maxAxis, float minDepth,
                                 float maxDepth, bool stickyPair, float& axisDistance, float& depth)
  {
    const bool midDepthInRange = metrics.midDepth >= minDepth && metrics.midDepth <= maxDepth;
    const bool tipDepthInRange = metrics.tipDepth >= minDepth && metrics.tipDepth <= maxDepth;

    bool useMid = metrics.midAxisDistance + 0.12f < metrics.tipAxisDistance;
    if (midDepthInRange != tipDepthInRange)
      useMid = midDepthInRange;

    axisDistance = useMid ? metrics.midAxisDistance : metrics.tipAxisDistance;
    if (axisDistance > maxAxis) {
      const float alternateAxisDistance =
          useMid ? metrics.tipAxisDistance : metrics.midAxisDistance;
      if (alternateAxisDistance <= maxAxis) {
        useMid       = !useMid;
        axisDistance = alternateAxisDistance;
      }
    }
    if (axisDistance > maxAxis)
      return false;

    depth = useMid ? metrics.midDepth : metrics.tipDepth;

    const float shaftMinDepth = (std::min)({metrics.rootDepth, metrics.midDepth, metrics.tipDepth});
    const float shaftMaxDepth = (std::max)({metrics.rootDepth, metrics.midDepth, metrics.tipDepth});
    const bool shaftInsideReceiver = metrics.overlapDepth > 0.05f;
    const bool shaftInsideDepthWindow =
        shaftMaxDepth >= minDepth + 0.05f && shaftMinDepth <= maxDepth - 0.05f;
    const bool nearEntryHold = stickyPair && Geometry::SampledShaftEntryDistance(metrics) <= 2.0f &&
                               axisDistance <= maxAxis + 0.6f;
    const bool deepInsertionHold =
        stickyPair && shaftInsideDepthWindow && axisDistance <= maxAxis + 0.6f;
    if (!shaftInsideReceiver && !nearEntryHold && !deepInsertionHold)
      return false;

    if (depth < minDepth || depth > maxDepth) {
      if (midDepthInRange && metrics.midAxisDistance <= maxAxis) {
        axisDistance = metrics.midAxisDistance;
        depth        = metrics.midDepth;
      } else if (tipDepthInRange && metrics.tipAxisDistance <= maxAxis) {
        axisDistance = metrics.tipAxisDistance;
        depth        = metrics.tipDepth;
      } else if (nearEntryHold || deepInsertionHold) {
        depth = std::clamp(depth, minDepth + 0.05f, maxDepth - 0.05f);
      } else {
        return false;
      }
    }

    return depth >= minDepth && depth <= maxDepth;
  }

  MotionSnapshot BuildOralChannelSnapshot(const ActorState& data, Name receiverPart,
                                          bool predicted = false, bool previous = false)
  {
    const MotionHistory* mouthHistory  = TryGetMotionHistory(data, Name::Mouth);
    const MotionHistory* throatHistory = TryGetMotionHistory(data, Name::Throat);

    auto select = [&](const MotionHistory* history) -> MotionSnapshot {
      if (!history)
        return {};
      if (predicted)
        return PredictSnapshot(*history);
      if (previous)
        return history->previous;
      return history->current;
    };

    const MotionSnapshot mouth  = select(mouthHistory);
    const MotionSnapshot throat = select(throatHistory);

    if (receiverPart == Name::Mouth) {
      if (!mouth.valid)
        return {};
      MotionSnapshot channel = mouth;
      if (throat.valid) {
        channel.end                 = throat.end;
        const Define::Vector3f axis = channel.end - channel.start;
        channel.length              = axis.norm();
        channel.directional         = channel.length >= 1e-6f;
        if (channel.directional)
          channel.direction = axis / channel.length;
      }
      return channel;
    }

    if (receiverPart == Name::Throat)
      return throat;

    return {};
  }

  void AddCandidate(std::vector<Candidate>& out, const DirectedContext& ctx, Name sourcePart,
                    Name targetPart, Type type, CandidateFamily family, float distance, float score)
  {
    out.push_back(Candidate{ctx.sourceActor, sourcePart, ctx.targetActor, targetPart, type,
                            distance, score, family});
  }

  void AddSimpleDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx,
                                   const Name* sourceParts, std::size_t sourceCount,
                                   const Name* targetParts, std::size_t targetCount, Type type,
                                   CandidateFamily family, float maxDistance, AngleRange angleRange,
                                   float angleWeight, float scoreOffset = 0.f,
                                   AnchorMode anchorMode = AnchorMode::None,
                                   float anchorWeight = 0.f, float anchorMaxDistance = 0.f)
  {
    for (std::size_t sourceIndex = 0; sourceIndex < sourceCount; ++sourceIndex) {
      const Name sourceName            = sourceParts[sourceIndex];
      const MotionSnapshot* sourcePart = TryGetCurrentSnapshot(ctx.sourceData, sourceName);
      if (!sourcePart)
        continue;

      for (std::size_t targetIndex = 0; targetIndex < targetCount; ++targetIndex) {
        const Name targetName = targetParts[targetIndex];
        if (ctx.self && sourceName == targetName)
          continue;

        const MotionSnapshot* targetPart = TryGetCurrentSnapshot(ctx.targetData, targetName);
        if (!targetPart)
          continue;

        const float distance = Geometry::MeasureSpatialDistance(*sourcePart, *targetPart);
        if (distance > maxDistance)
          continue;

        const float angle = Geometry::MeasurePartAngle(*sourcePart, *targetPart);
        if (!CheckAngle(angle, angleRange))
          continue;

        const float distNorm = NormalizeDistance(distance, maxDistance);
        float score          = distNorm;
        if (!IsAngleUnconstrained(angleRange)) {
          const float angleNorm = AngleDeviation(angle, angleRange);
          score                 = distNorm * (1.f - angleWeight) + angleNorm * angleWeight;
        }

        if (anchorMode != AnchorMode::None && anchorWeight > 1e-4f) {
          const float auxMaxDistance = anchorMaxDistance > 1e-4f ? anchorMaxDistance : maxDistance;
          const float anchorDistance =
              Geometry::AnchorDistance(*sourcePart, *targetPart, anchorMode);
          const float anchorNorm = NormalizeDistance(anchorDistance, auxMaxDistance);
          score                  = score * (1.f - anchorWeight) + anchorNorm * anchorWeight;
        }

        const float bonus = TemporalBonus(ctx.sourceData, sourceName, ctx.targetActor, type) +
                            TemporalBonus(ctx.targetData, targetName, ctx.sourceActor, type);
        score             = (std::max)(0.f, score + scoreOffset - bonus);

        AddCandidate(out, ctx, sourceName, targetName, type, family, distance, score);
      }
    }
  }

  template <std::size_t NS, std::size_t NT>
  void AddSimpleDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx,
                                   const std::array<Name, NS>& sourceParts,
                                   const std::array<Name, NT>& targetParts, Type type,
                                   CandidateFamily family, float maxDistance, AngleRange angleRange,
                                   float angleWeight, float scoreOffset = 0.f,
                                   AnchorMode anchorMode = AnchorMode::None,
                                   float anchorWeight = 0.f, float anchorMaxDistance = 0.f)
  {
    AddSimpleDirectedCandidates(out, ctx, sourceParts.data(), sourceParts.size(),
                                targetParts.data(), targetParts.size(), type, family, maxDistance,
                                angleRange, angleWeight, scoreOffset, anchorMode, anchorWeight,
                                anchorMaxDistance);
  }

  void AddSimpleDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx,
                                   const SimpleDirectedSpec& spec)
  {
    AddSimpleDirectedCandidates(out, ctx, spec.sourceParts, spec.sourceCount, spec.targetParts,
                                spec.targetCount, spec.type, spec.family, spec.maxDistance,
                                spec.angleRange, spec.angleWeight, spec.scoreOffset,
                                spec.anchorMode, spec.anchorWeight, spec.anchorMaxDistance);
  }

  void TryAddGropeBreastCandidate(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    constexpr float kMaxDistance       = 4.9f;
    constexpr float kMaxNippleDistance = 4.5f;
    constexpr AngleRange kAngleRange{160.f, 180.f};

    for (Name handName : kHandParts) {
      const MotionSnapshot* handPart = TryGetCurrentSnapshot(ctx.sourceData, handName);
      if (!handPart)
        continue;

      for (Name breastName : kBreastParts) {
        const MotionSnapshot* breastPart = TryGetCurrentSnapshot(ctx.targetData, breastName);
        if (!breastPart)
          continue;

        const float distance = Geometry::MeasureSpatialDistance(*handPart, *breastPart);
        if (distance > kMaxDistance)
          continue;

        const float angle = Geometry::MeasurePartAngle(*handPart, *breastPart);
        if (!CheckAngle(angle, kAngleRange))
          continue;

        const float nippleDistance = (handPart->start - breastPart->end).norm();
        if (nippleDistance > kMaxNippleDistance)
          continue;

        const float distNorm   = NormalizeDistance(distance, kMaxDistance);
        const float angleNorm  = AngleDeviation(angle, kAngleRange);
        const float nippleNorm = NormalizeDistance(nippleDistance, kMaxNippleDistance);
        const float bonus =
            TemporalBonus(ctx.sourceData, handName, ctx.targetActor, Type::GropeBreast) +
            TemporalBonus(ctx.targetData, breastName, ctx.sourceActor, Type::GropeBreast);
        const float score =
            (std::max)(0.f, distNorm * 0.44f + angleNorm * 0.24f + nippleNorm * 0.32f - bonus);

        AddCandidate(out, ctx, handName, breastName, Type::GropeBreast, CandidateFamily::Contact,
                     distance, score);
      }
    }
  }

  void TryAddHandjobCandidate(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    const MotionSnapshot* penisCurrent = TryGetCurrentSnapshot(ctx.targetData, Name::Penis);
    if (!penisCurrent)
      return;

    const MotionHistory* penisHistory = TryGetMotionHistory(ctx.targetData, Name::Penis);
    if (!penisHistory)
      return;

    constexpr float kMaxDistance = 7.2f;
    constexpr AngleRange kAngleRange{58.f, 122.f};
    constexpr float kAngleWeight             = 0.20f;
    constexpr float kCoverageWeight          = 0.16f;
    constexpr float kSelfFaceGuardDistance   = 6.0f;
    constexpr float kOralInterferencePenalty = 0.45f;
    constexpr float kPartnerOralDistance     = 7.8f;

    for (Name handName : kHandParts) {
      const MotionSnapshot* handCurrent = TryGetCurrentSnapshot(ctx.sourceData, handName);
      if (!handCurrent)
        continue;

      const SampledMetrics currentMetrics =
          Geometry::MeasureSampledMetrics(*penisCurrent, *handCurrent);
      const float distance =
          (std::min)(Geometry::SampledShaftDistance(currentMetrics),
                     Geometry::MeasureSpatialDistance(*handCurrent, *penisCurrent));
      if (distance > kMaxDistance)
        continue;

      const float angle = Geometry::MeasurePartAngle(*handCurrent, *penisCurrent);
      if (!CheckAngle(angle, kAngleRange))
        continue;

      float handToOral  = std::numeric_limits<float>::infinity();
      float penisToOral = std::numeric_limits<float>::infinity();
      if (const MotionSnapshot* mouthPart = TryGetCurrentSnapshot(ctx.sourceData, Name::Mouth))
        handToOral =
            (std::min)(handToOral, Geometry::MeasureSpatialDistance(*handCurrent, *mouthPart));
      if (const MotionSnapshot* throatPart = TryGetCurrentSnapshot(ctx.sourceData, Name::Throat))
        handToOral =
            (std::min)(handToOral, Geometry::MeasureSpatialDistance(*handCurrent, *throatPart));
      if (const MotionSnapshot* mouthPart = TryGetCurrentSnapshot(ctx.sourceData, Name::Mouth))
        penisToOral =
            (std::min)(penisToOral, Geometry::MeasureSpatialDistance(*penisCurrent, *mouthPart));
      if (const MotionSnapshot* throatPart = TryGetCurrentSnapshot(ctx.sourceData, Name::Throat))
        penisToOral =
            (std::min)(penisToOral, Geometry::MeasureSpatialDistance(*penisCurrent, *throatPart));

      if (ctx.self && std::isfinite(handToOral) && handToOral < kSelfFaceGuardDistance)
        continue;

      const bool oralInterference =
          !ctx.self && std::isfinite(handToOral) && handToOral < kSelfFaceGuardDistance &&
          std::isfinite(penisToOral) && penisToOral < kPartnerOralDistance;

      const MotionSnapshot predictedPenis = PredictSnapshot(*penisHistory);
      float predictionBonus               = 0.f;
      if (predictedPenis.valid) {
        const SampledMetrics predictedMetrics =
            Geometry::MeasureSampledMetrics(predictedPenis, *handCurrent);
        predictionBonus = std::clamp(
            (distance - Geometry::SampledShaftDistance(predictedMetrics)) / 8.f, 0.f, 0.08f);
      }

      const float distNorm     = NormalizeDistance(distance, kMaxDistance);
      const float coverageNorm = NormalizeDistance(
          (currentMetrics.rootDistance + currentMetrics.midDistance + currentMetrics.tipDistance) /
              3.f,
          kMaxDistance);
      const float angleNorm = AngleDeviation(angle, kAngleRange);
      float score = distNorm * (1.f - kAngleWeight - kCoverageWeight) + angleNorm * kAngleWeight +
                    coverageNorm * kCoverageWeight;
      if (oralInterference)
        score += kOralInterferencePenalty;

      const float bonus =
          TemporalBonus(ctx.sourceData, handName, ctx.targetActor, Type::Handjob) +
          TemporalBonus(ctx.targetData, Name::Penis, ctx.sourceActor, Type::Handjob);
      score = (std::max)(0.f, score - bonus - predictionBonus);

      AddCandidate(out, ctx, handName, Name::Penis, Type::Handjob, CandidateFamily::Contact,
                   distance, score);
    }
  }

  void TryAddGenitalPenetrationCandidate(std::vector<Candidate>& out, const DirectedContext& ctx,
                                         Name receiverPartName, Type type, float maxDistance,
                                         float maxAngle, float maxAxis, float minDepth,
                                         float maxDepth)
  {
    const MotionHistory* penisHistory    = TryGetMotionHistory(ctx.sourceData, Name::Penis);
    const MotionHistory* receiverHistory = TryGetMotionHistory(ctx.targetData, receiverPartName);
    if (!penisHistory || !receiverHistory || !penisHistory->current.valid ||
        !receiverHistory->current.valid) {
      return;
    }

    const SampledMetrics currentMetrics =
        Geometry::MeasureSampledMetrics(penisHistory->current, receiverHistory->current);
    const float distance          = Geometry::SampledShaftDistance(currentMetrics);
    const float rootEntryDistance = currentMetrics.rootEntryDistance;

    const MotionSnapshot predictedPenis    = PredictSnapshot(*penisHistory);
    const MotionSnapshot predictedReceiver = PredictSnapshot(*receiverHistory);
    const bool hasPredicted                = predictedPenis.valid && predictedReceiver.valid;
    const SampledMetrics predictedMetrics =
        hasPredicted ? Geometry::MeasureSampledMetrics(predictedPenis, predictedReceiver)
                     : SampledMetrics{};
    const float predictedDistance = hasPredicted ? Geometry::SampledShaftDistance(predictedMetrics)
                                                 : std::numeric_limits<float>::infinity();
    const float predictedRootEntryDistance =
        hasPredicted ? predictedMetrics.rootEntryDistance : std::numeric_limits<float>::infinity();
    const bool hasPreviousSamples = penisHistory->previous.valid && receiverHistory->previous.valid;
    const SampledMetrics previousMetrics =
        hasPreviousSamples
            ? Geometry::MeasureSampledMetrics(penisHistory->previous, receiverHistory->previous)
            : SampledMetrics{};
    const float previousDistance = hasPreviousSamples
                                       ? Geometry::SampledShaftDistance(previousMetrics)
                                       : std::numeric_limits<float>::infinity();

    const auto temporalState = Temporal::BuildGenitalPenetrationContext(
        ctx.sourceData, ctx.sourceActor, Name::Penis, ctx.targetData, ctx.targetActor,
        receiverPartName, type);
    const bool stickyPair               = temporalState.stickyPair;
    const bool previousSameType         = temporalState.previousSameType;
    const bool previousOppositeType     = temporalState.previousOppositeType;
    const bool recentDifferentSlotOwner = temporalState.recentDifferentSlotOwner;

    const PenisContextMetrics contextMetrics =
        BuildPenisContextMetrics(ctx.targetData, penisHistory->current);
    const PenisContextMetrics predictedContextMetrics =
        hasPredicted ? BuildPenisContextMetrics(ctx.targetData, predictedPenis)
                     : PenisContextMetrics{};
    const auto hasStrongTypeContext = [&](const PenisContextMetrics& metrics) {
      return type == Type::Anal
                 ? HasStrongRearPreference(metrics.vaginalSupport, metrics.analSupport) &&
                       HasStrongRearPreference(metrics.vaginalEntryDistance,
                                               metrics.analEntryDistance)
                 : HasStrongFrontPreference(metrics.vaginalSupport, metrics.analSupport) &&
                       HasStrongFrontPreference(metrics.vaginalEntryDistance,
                                                metrics.analEntryDistance);
    };
    const bool strongContext = hasStrongTypeContext(contextMetrics) ||
                               (hasPredicted && hasStrongTypeContext(predictedContextMetrics));
    const float contextBias  = ComputeFrontRearBias(contextMetrics, type);
    const float analAngleAllowance =
        type == Type::Anal
            ? (std::max)(ComputeAnalAngleAllowance(contextMetrics, rootEntryDistance, distance),
                         hasPredicted ? ComputeAnalAngleAllowance(predictedContextMetrics,
                                                                  predictedRootEntryDistance,
                                                                  predictedDistance)
                                      : 0.f)
            : 0.f;

    float effectiveMaxDistance = maxDistance;
    float effectiveMaxAngle    = maxAngle;
    float effectiveMaxAxis     = maxAxis;
    float effectiveMinDepth    = minDepth;
    float effectiveMaxDepth    = maxDepth;
    float temporalBonus        = 0.f;
    float switchPenalty        = 0.f;

    if (type == Type::Anal)
      effectiveMaxAngle += analAngleAllowance;

    if (previousSameType) {
      effectiveMaxDistance += type == Type::Vaginal ? 1.0f : 0.8f;
      effectiveMaxAngle += type == Type::Vaginal ? 10.f : 8.f;
      effectiveMaxAxis += type == Type::Vaginal ? 0.9f : 0.6f;
      effectiveMinDepth -= type == Type::Vaginal ? 1.2f : 0.9f;
      effectiveMaxDepth += type == Type::Vaginal ? 1.5f : 1.0f;
      temporalBonus += type == Type::Vaginal ? 0.20f : 0.18f;
    }

    if (hasPredicted && predictedDistance + 0.35f < distance) {
      effectiveMaxDistance += 0.45f;
      effectiveMaxAngle += 5.f;
      effectiveMaxAxis += 0.45f;
      temporalBonus += 0.06f;
    }

    if (strongContext) {
      effectiveMaxDistance += 0.25f;
      effectiveMaxAngle += 3.f;
      temporalBonus += 0.04f;
    }

    if (rootEntryDistance <= 3.4f) {
      effectiveMaxDistance += 0.25f;
      effectiveMaxAxis += 0.25f;
    }

    const bool predictedDistanceHold =
        hasPredicted &&
        ((type == Type::Vaginal && strongContext &&
          predictedDistance <= effectiveMaxDistance + 0.25f &&
          predictedDistance + 0.35f < distance && distance <= effectiveMaxDistance + 1.8f) ||
         (type == Type::Anal && (stickyPair || previousSameType || strongContext) &&
          predictedDistance <= effectiveMaxDistance + 0.35f &&
          predictedDistance + 0.25f < distance && distance <= effectiveMaxDistance + 1.35f &&
          predictedMetrics.minAxisDistance <= effectiveMaxAxis + 0.45f &&
          predictedRootEntryDistance <= 3.75f));

    bool acceptedByPredictedApproach = false;
    if (distance > effectiveMaxDistance) {
      if (predictedDistanceHold) {
        acceptedByPredictedApproach = true;
        temporalBonus += 0.05f;
      } else {
        return;
      }
    }

    const float angle = Geometry::MeasurePartAngle(receiverHistory->current, penisHistory->current);
    const float predictedAngle =
        hasPredicted ? Geometry::MeasurePartAngle(predictedReceiver, predictedPenis) : angle;
    const float previousAngle =
        hasPreviousSamples
            ? Geometry::MeasurePartAngle(receiverHistory->previous, penisHistory->previous)
            : std::numeric_limits<float>::infinity();
    const float angleHoldMargin = Temporal::ComputeGenitalAngleHoldMargin(
        type, stickyPair, previousSameType, strongContext, rootEntryDistance, predictedAngle,
        effectiveMaxAngle);
    const bool heldByAngleHysteresis =
        angle > effectiveMaxAngle && angle <= effectiveMaxAngle + angleHoldMargin &&
        Temporal::HasStableAngleTrend(previousAngle, angle, predictedAngle, effectiveMaxAngle);
    const bool heldByPredictedAngle = acceptedByPredictedApproach && angle > effectiveMaxAngle &&
                                      predictedAngle <= effectiveMaxAngle + 2.5f;
    float angleGateForScore         = effectiveMaxAngle;
    if (angle > effectiveMaxAngle && !heldByAngleHysteresis && !heldByPredictedAngle)
      return;

    if (heldByAngleHysteresis) {
      angleGateForScore += angleHoldMargin;
      temporalBonus += type == Type::Vaginal ? 0.07f : 0.05f;
    } else if (heldByPredictedAngle) {
      angleGateForScore += 2.5f;
      temporalBonus += 0.03f;
    }

    float axisDistance        = 0.f;
    float depth               = 0.f;
    bool usedPredictedContact = false;
    if (!ResolvePenetrationContact(currentMetrics, effectiveMaxAxis, effectiveMinDepth,
                                   effectiveMaxDepth, stickyPair || previousSameType, axisDistance,
                                   depth)) {
      if (acceptedByPredictedApproach &&
          ResolvePenetrationContact(predictedMetrics, effectiveMaxAxis + 0.45f,
                                    effectiveMinDepth - 0.45f, effectiveMaxDepth + 0.90f, true,
                                    axisDistance, depth)) {
        usedPredictedContact = true;
      } else {
        return;
      }
    }

    const float predictedAxisDistance =
        hasPredicted ? predictedMetrics.minAxisDistance : std::numeric_limits<float>::infinity();
    const float predictedDepth = hasPredicted ? predictedMetrics.tipDepth : depth;
    const bool switchTrendConfirmed =
        strongContext ||
        Temporal::HasConfirmedPenetrationSwitchTrend(previousDistance, distance, previousAngle,
                                                     angle, predictedDistance, predictedAngle);
    const bool comfortableSwitchGeometry = distance <= effectiveMaxDistance - 0.45f &&
                                           angle <= effectiveMaxAngle - 4.5f &&
                                           axisDistance <= effectiveMaxAxis - 0.35f;
    if (previousOppositeType && !previousSameType && !switchTrendConfirmed &&
        !comfortableSwitchGeometry) {
      return;
    }

    if (recentDifferentSlotOwner && previousOppositeType && !previousSameType && !strongContext &&
        !(switchTrendConfirmed && comfortableSwitchGeometry)) {
      return;
    }

    if (previousOppositeType && !strongContext)
      switchPenalty = type == Type::Anal ? 0.30f : 0.24f;
    if (previousOppositeType && !switchTrendConfirmed)
      switchPenalty += type == Type::Anal ? 0.08f : 0.06f;
    if (recentDifferentSlotOwner && previousOppositeType && !previousSameType)
      switchPenalty += type == Type::Anal ? 0.16f : 0.12f;

    const float distNorm  = NormalizeDistance(distance, effectiveMaxDistance);
    const float angleNorm = AngleDeviation(angle, {0.f, angleGateForScore});
    const float axisNorm  = NormalizeDistance(axisDistance, effectiveMaxAxis);

    float depthPenalty = 0.f;
    if (depth < 0.f) {
      depthPenalty = std::abs(depth) / (std::max)(std::abs(effectiveMinDepth), 1e-4f);
    } else if (depth > receiverHistory->current.length) {
      depthPenalty = (depth - receiverHistory->current.length) /
                     (std::max)(effectiveMaxDepth - receiverHistory->current.length, 1e-4f);
    }

    const float bonus = TemporalBonus(ctx.sourceData, Name::Penis, ctx.targetActor, type) +
                        TemporalBonus(ctx.targetData, receiverPartName, ctx.sourceActor, type);
    const float predictionBonus =
        hasPredicted ? std::clamp((distance - predictedDistance) / 8.f, 0.f, 0.08f) : 0.f;
    const float predictedApproachPenalty = usedPredictedContact ? 0.18f : 0.f;

    const float score =
        std::clamp(distNorm * 0.42f + angleNorm * 0.22f + axisNorm * 0.22f +
                       std::clamp(depthPenalty, 0.f, 1.f) * 0.14f + contextBias + switchPenalty +
                       predictedApproachPenalty - bonus - temporalBonus - predictionBonus - 0.10f,
                   -1.20f, 2.50f);

    if (type == Type::Anal && score > 0.20f && !stickyPair && !previousSameType && !strongContext &&
        rootEntryDistance > 2.5f && distance > 2.25f)
      return;

    AddCandidate(out, ctx, Name::Penis, receiverPartName, type, CandidateFamily::Penetration,
                 distance, score);
  }

  void TryAddOralPenetrationCandidate(std::vector<Candidate>& out, const DirectedContext& ctx,
                                      Name receiverPartName, Type type, float maxDistance,
                                      float maxAngle, float maxAxis, float minDepth, float maxDepth)
  {
    const MotionHistory* penisHistory = TryGetMotionHistory(ctx.targetData, Name::Penis);
    if (!penisHistory || !penisHistory->current.valid)
      return;

    const MotionSnapshot receiverCurrent =
        BuildOralChannelSnapshot(ctx.sourceData, receiverPartName);
    if (!receiverCurrent.valid)
      return;

    const MotionSnapshot predictedPenis = PredictSnapshot(*penisHistory);
    const MotionSnapshot receiverPredicted =
        BuildOralChannelSnapshot(ctx.sourceData, receiverPartName, true, false);
    const bool hasPredicted = predictedPenis.valid && receiverPredicted.valid;

    const SampledMetrics currentMetrics =
        Geometry::MeasureSampledMetrics(penisHistory->current, receiverCurrent);
    const float distance = Geometry::SampledShaftDistance(currentMetrics);
    const float predictedDistance =
        hasPredicted ? Geometry::SampledShaftDistance(
                           Geometry::MeasureSampledMetrics(predictedPenis, receiverPredicted))
                     : std::numeric_limits<float>::infinity();

    const bool stickyPair = HasStickyInteractionPair(ctx, receiverPartName, Name::Penis, type);
    const bool previousRelated =
        HadPreviousInteraction(ctx.sourceData, receiverPartName, ctx.targetActor, type) ||
        HadPreviousInteraction(ctx.targetData, Name::Penis, ctx.sourceActor, type);

    float effectiveMaxDistance = maxDistance;
    float effectiveMaxAngle    = maxAngle;
    float effectiveMaxAxis     = maxAxis;
    float effectiveMinDepth    = minDepth;
    float effectiveMaxDepth    = maxDepth;
    float temporalBonus        = 0.f;

    if (stickyPair || previousRelated) {
      effectiveMaxDistance += type == Type::DeepThroat ? 0.8f : 1.0f;
      effectiveMaxAngle += type == Type::DeepThroat ? 8.f : 10.f;
      effectiveMaxAxis += type == Type::DeepThroat ? 0.6f : 0.9f;
      effectiveMinDepth -= type == Type::DeepThroat ? 0.4f : 0.8f;
      effectiveMaxDepth += type == Type::DeepThroat ? 1.6f : 1.2f;
      temporalBonus += type == Type::DeepThroat ? 0.18f : 0.16f;
    }

    if (hasPredicted && predictedDistance + 0.35f < distance) {
      effectiveMaxDistance += 0.35f;
      effectiveMaxAngle += 4.f;
      effectiveMaxAxis += 0.35f;
      temporalBonus += 0.05f;
    }

    if (distance > effectiveMaxDistance)
      return;

    const float angle = Geometry::MeasurePartAngle(receiverCurrent, penisHistory->current);
    const float predictedAngle =
        hasPredicted ? Geometry::MeasurePartAngle(receiverPredicted, predictedPenis) : angle;
    if (angle > effectiveMaxAngle)
      return;

    float axisDistance = 0.f;
    float depth        = 0.f;
    if (!ResolvePenetrationContact(currentMetrics, effectiveMaxAxis, effectiveMinDepth,
                                   effectiveMaxDepth, stickyPair || previousRelated, axisDistance,
                                   depth)) {
      return;
    }

    const float bonus = TemporalBonus(ctx.sourceData, receiverPartName, ctx.targetActor, type) +
                        TemporalBonus(ctx.targetData, Name::Penis, ctx.sourceActor, type);
    const float predictionBonus =
        hasPredicted ? std::clamp((distance - predictedDistance) / 8.f, 0.f, 0.08f) : 0.f;
    const float distNorm  = NormalizeDistance(distance, effectiveMaxDistance);
    const float axisNorm  = NormalizeDistance(axisDistance, effectiveMaxAxis);
    const float angleNorm = AngleDeviation(angle, {0.f, effectiveMaxAngle});

    float depthPenalty = 0.f;
    if (depth < 0.f)
      depthPenalty = std::abs(depth) / (std::max)(std::abs(effectiveMinDepth), 1e-4f);
    else if (depth > receiverCurrent.length)
      depthPenalty = (depth - receiverCurrent.length) /
                     (std::max)(effectiveMaxDepth - receiverCurrent.length, 1e-4f);

    const float score =
        (std::max)(0.f, distNorm * 0.34f + axisNorm * 0.34f + angleNorm * 0.18f +
                            std::clamp(depthPenalty, 0.f, 1.f) * 0.14f - bonus - temporalBonus -
                            predictionBonus - (type == Type::DeepThroat ? 0.08f : 0.12f));

    AddCandidate(out, ctx, receiverPartName, Name::Penis, type, CandidateFamily::Penetration,
                 distance, score);
  }

  void TryAddSampledPenisProximityCandidate(std::vector<Candidate>& out, const DirectedContext& ctx,
                                            Name targetName, Type type, float maxDistance,
                                            AngleRange angleRange, float angleWeight,
                                            float scoreOffset     = 0.f,
                                            AnchorMode anchorMode = AnchorMode::None,
                                            float anchorWeight = 0.f, float anchorMaxDistance = 0.f)
  {
    const MotionHistory* penisHistory = TryGetMotionHistory(ctx.sourceData, Name::Penis);
    const MotionSnapshot* targetPart  = TryGetCurrentSnapshot(ctx.targetData, targetName);
    if (!penisHistory || !penisHistory->current.valid || !targetPart)
      return;

    const SampledMetrics currentMetrics =
        Geometry::MeasureSampledMetrics(penisHistory->current, *targetPart);
    const float distance =
        (std::min)(Geometry::SampledShaftDistance(currentMetrics),
                   Geometry::MeasureSpatialDistance(penisHistory->current, *targetPart));
    if (distance > maxDistance)
      return;

    const float angle = Geometry::MeasurePartAngle(penisHistory->current, *targetPart);
    if (!CheckAngle(angle, angleRange))
      return;

    const MotionSnapshot predictedPenis = PredictSnapshot(*penisHistory);
    float predictionBonus               = 0.f;
    if (predictedPenis.valid) {
      const SampledMetrics predictedMetrics =
          Geometry::MeasureSampledMetrics(predictedPenis, *targetPart);
      predictionBonus = std::clamp(
          (distance - Geometry::SampledShaftDistance(predictedMetrics)) / 8.f, 0.f, 0.07f);
    }

    const bool stickyPair    = HasStickyInteractionPair(ctx, Name::Penis, targetName, type);
    const float distNorm     = NormalizeDistance(distance, maxDistance);
    const float coverageNorm = NormalizeDistance(
        (currentMetrics.rootDistance + currentMetrics.midDistance + currentMetrics.tipDistance) /
            3.f,
        maxDistance);
    float score = distNorm * (1.f - angleWeight - 0.16f - anchorWeight) +
                  AngleDeviation(angle, angleRange) * angleWeight + coverageNorm * 0.16f;

    if (anchorMode != AnchorMode::None && anchorWeight > 1e-4f) {
      const float auxMaxDistance = anchorMaxDistance > 1e-4f ? anchorMaxDistance : maxDistance;
      const float anchorDistance =
          Geometry::AnchorDistance(penisHistory->current, *targetPart, anchorMode);
      const float anchorNorm = NormalizeDistance(anchorDistance, auxMaxDistance);
      score += anchorNorm * anchorWeight;
    }

    const float bonus = TemporalBonus(ctx.sourceData, Name::Penis, ctx.targetActor, type) +
                        TemporalBonus(ctx.targetData, targetName, ctx.sourceActor, type) +
                        (stickyPair ? 0.04f : 0.f);
    score             = (std::max)(0.f, score + scoreOffset - bonus - predictionBonus);

    AddCandidate(out, ctx, Name::Penis, targetName, type, CandidateFamily::Proximity, distance,
                 score);
  }

  void TryAddNaveljobCandidate(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    const MotionHistory* penisHistory = TryGetMotionHistory(ctx.sourceData, Name::Penis);
    const MotionSnapshot* belly       = TryGetCurrentSnapshot(ctx.targetData, Name::Belly);
    if (!penisHistory || !penisHistory->current.valid || !belly)
      return;

    const SampledMetrics currentMetrics =
        Geometry::MeasureSampledMetrics(penisHistory->current, *belly);
    const float distance =
        (std::min)(Geometry::SampledShaftDistance(currentMetrics),
                   Geometry::MeasureSpatialDistance(penisHistory->current, *belly));
    if (distance > 9.4f)
      return;

    const float angle = Geometry::MeasurePartAngle(*belly, penisHistory->current);
    if (angle > 48.f)
      return;

    if (!Geometry::IsPointInFront(*belly, penisHistory->current.end) ||
        !Geometry::IsHorizontal(penisHistory->current)) {
      return;
    }

    const MotionSnapshot predictedPenis = PredictSnapshot(*penisHistory);
    const float predictionBonus =
        predictedPenis.valid && Geometry::IsHorizontal(predictedPenis)
            ? std::clamp((distance - Geometry::SampledShaftDistance(
                                         Geometry::MeasureSampledMetrics(predictedPenis, *belly))) /
                             9.f,
                         0.f, 0.05f)
            : 0.f;

    const float distNorm     = NormalizeDistance(distance, 9.4f);
    const float angleNorm    = AngleDeviation(angle, {0.f, 48.f});
    const float coverageNorm = NormalizeDistance(
        (currentMetrics.rootDistance + currentMetrics.midDistance + currentMetrics.tipDistance) /
            3.f,
        9.4f);
    const float bonus =
        TemporalBonus(ctx.sourceData, Name::Penis, ctx.targetActor, Type::Naveljob) +
        TemporalBonus(ctx.targetData, Name::Belly, ctx.sourceActor, Type::Naveljob);
    const float score = (std::max)(0.f, distNorm * 0.44f + angleNorm * 0.28f +
                                            coverageNorm * 0.28f - bonus - predictionBonus);

    AddCandidate(out, ctx, Name::Penis, Name::Belly, Type::Naveljob, CandidateFamily::Proximity,
                 distance, score);
  }

  void TryAddSymmetricCandidate(std::vector<Candidate>& out, const PairContext& pair, Name partA,
                                Name partB, Type type, CandidateFamily family, float maxDistance,
                                AngleRange angleRange, float angleWeight, float scoreOffset = 0.f)
  {
    if (pair.self)
      return;

    const MotionSnapshot* lhs = TryGetCurrentSnapshot(pair.dataA, partA);
    const MotionSnapshot* rhs = TryGetCurrentSnapshot(pair.dataB, partB);
    if (!lhs || !rhs)
      return;

    const float distance = Geometry::MeasureSpatialDistance(*lhs, *rhs);
    if (distance > maxDistance)
      return;

    const float angle = Geometry::MeasurePartAngle(*lhs, *rhs);
    if (!CheckAngle(angle, angleRange))
      return;

    const float distNorm = NormalizeDistance(distance, maxDistance);
    float score          = distNorm;
    if (!IsAngleUnconstrained(angleRange)) {
      const float angleNorm = AngleDeviation(angle, angleRange);
      score                 = distNorm * (1.f - angleWeight) + angleNorm * angleWeight;
    }

    const float bonus = TemporalBonus(pair.dataA, partA, pair.actorB, type) +
                        TemporalBonus(pair.dataB, partB, pair.actorA, type);
    score             = (std::max)(0.f, score + scoreOffset - bonus);

    const DirectedContext ctx{pair.actorA, pair.actorB, pair.dataA, pair.dataB, false};
    AddCandidate(out, ctx, partA, partB, type, family, distance, score);
  }

  void GenerateOralContactCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (ctx.self)
      return;
    if (!HasAnyCurrentPart(ctx.sourceData, kMouthParts) &&
        !HasAnyCurrentPart(ctx.sourceData, kThroatParts))
      return;

    static constexpr SimpleDirectedSpec kContactSpecs[]{
        {kMouthParts.data(),
         kMouthParts.size(),
         kBreastParts.data(),
         kBreastParts.size(),
         Type::BreastSucking,
         CandidateFamily::Contact,
         7.2f,
         {0.f, 180.f},
         0.f},
        {kMouthParts.data(),
         kMouthParts.size(),
         kFootParts.data(),
         kFootParts.size(),
         Type::ToeSucking,
         CandidateFamily::Contact,
         7.2f,
         {0.f, 180.f},
         0.f},
        {kMouthParts.data(),
         kMouthParts.size(),
         kThighCleftParts.data(),
         kThighCleftParts.size(),
         Type::Cunnilingus,
         CandidateFamily::Contact,
         6.4f,
         {0.f, 180.f},
         0.f,
         -0.05f,
         AnchorMode::StartToStart,
         0.18f,
         6.4f},
        {kMouthParts.data(),
         kMouthParts.size(),
         kVaginaParts.data(),
         kVaginaParts.size(),
         Type::Cunnilingus,
         CandidateFamily::Contact,
         6.8f,
         {0.f, 180.f},
         0.f,
         0.01f,
         AnchorMode::StartToStart,
         0.22f,
         6.6f},
        {kMouthParts.data(),
         kMouthParts.size(),
         kAnusParts.data(),
         kAnusParts.size(),
         Type::Anilingus,
         CandidateFamily::Contact,
         6.3f,
         {0.f, 180.f},
         0.f,
         0.f,
         AnchorMode::StartToStart,
         0.20f,
         6.0f},
    };
    for (const auto& spec : kContactSpecs)
      AddSimpleDirectedCandidates(out, ctx, spec);

    static constexpr PenetrationSpec kPenetrationSpecs[]{
        {Name::Mouth, Type::Fellatio, 8.6f, 68.f, 5.8f, -2.2f, 9.6f},
        {Name::Throat, Type::DeepThroat, 6.8f, 40.f, 4.2f, 1.4f, 12.2f},
    };
    for (const auto& spec : kPenetrationSpecs) {
      TryAddOralPenetrationCandidate(out, ctx, spec.receiverPart, spec.type, spec.maxDistance,
                                     spec.maxAngle, spec.maxAxis, spec.minDepth, spec.maxDepth);
    }
  }

  void GenerateManualCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (!HasAnyCurrentPart(ctx.sourceData, kHandParts) &&
        !HasAnyCurrentPart(ctx.sourceData, kFingerParts))
      return;

    TryAddGropeBreastCandidate(out, ctx);

    static constexpr SimpleDirectedSpec kManualSpecs[]{
        {kHandParts.data(),
         kHandParts.size(),
         kThighParts.data(),
         kThighParts.size(),
         Type::GropeThigh,
         CandidateFamily::Contact,
         6.4f,
         {0.f, 180.f},
         0.f,
         0.f,
         AnchorMode::StartToStart,
         0.14f,
         5.8f},
        {kHandParts.data(),
         kHandParts.size(),
         kButtParts.data(),
         kButtParts.size(),
         Type::GropeButt,
         CandidateFamily::Contact,
         4.7f,
         {0.f, 180.f},
         0.f,
         0.f,
         AnchorMode::StartToStart,
         0.16f,
         4.3f},
        {kHandParts.data(),
         kHandParts.size(),
         kFootParts.data(),
         kFootParts.size(),
         Type::GropeFoot,
         CandidateFamily::Contact,
         6.4f,
         {0.f, 180.f},
         0.f,
         0.f,
         AnchorMode::StartToStart,
         0.12f,
         5.8f},
        {kFingerParts.data(),
         kFingerParts.size(),
         kThighCleftParts.data(),
         kThighCleftParts.size(),
         Type::FingerVagina,
         CandidateFamily::Contact,
         5.8f,
         {0.f, 180.f},
         0.f,
         -0.04f,
         AnchorMode::StartToStart,
         0.18f,
         5.6f},
        {kFingerParts.data(),
         kFingerParts.size(),
         kVaginaParts.data(),
         kVaginaParts.size(),
         Type::FingerVagina,
         CandidateFamily::Contact,
         6.4f,
         {0.f, 180.f},
         0.f,
         0.01f,
         AnchorMode::StartToStart,
         0.22f,
         6.0f},
        {kFingerParts.data(),
         kFingerParts.size(),
         kAnusParts.data(),
         kAnusParts.size(),
         Type::FingerAnus,
         CandidateFamily::Contact,
         6.0f,
         {0.f, 180.f},
         0.f,
         0.f,
         AnchorMode::StartToStart,
         0.20f,
         5.6f},
    };
    for (const auto& spec : kManualSpecs)
      AddSimpleDirectedCandidates(out, ctx, spec);

    TryAddHandjobCandidate(out, ctx);
  }

  void GenerateProximityCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (!HasAnyCurrentPart(ctx.sourceData, kPenisParts))
      return;

    static constexpr ProximitySpec kProximitySpecs[]{
        {Name::BreastLeft, Type::Titfuck, 7.2f, {0.f, 50.f}, 0.32f},
        {Name::BreastRight, Type::Titfuck, 7.2f, {0.f, 50.f}, 0.32f},
        {Name::Cleavage, Type::Titfuck, 7.2f, {78.f, 102.f}, 0.40f, -0.03f},
        {Name::ThighCleft, Type::Thighjob, 5.8f, {0.f, 28.f}, 0.24f, 0.02f},
        {Name::ThighLeft, Type::Thighjob, 4.8f, {0.f, 28.f}, 0.22f, 0.01f},
        {Name::ThighRight, Type::Thighjob, 4.8f, {0.f, 28.f}, 0.22f, 0.01f},
        {Name::GlutealCleft, Type::Frottage, 7.2f, {148.f, 32.f, AngleModel::Wrap}, 0.28f},
        {Name::FootLeft, Type::Footjob, 7.4f, {0.f, 180.f}, 0.f},
        {Name::FootRight, Type::Footjob, 7.4f, {0.f, 180.f}, 0.f},
    };
    for (const auto& spec : kProximitySpecs) {
      TryAddSampledPenisProximityCandidate(
          out, ctx, spec.targetPart, spec.type, spec.maxDistance, spec.angleRange, spec.angleWeight,
          spec.scoreOffset, spec.anchorMode, spec.anchorWeight, spec.anchorMaxDistance);
    }

    TryAddNaveljobCandidate(out, ctx);
  }

  void GeneratePenetrationCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (ctx.self || !HasAnyCurrentPart(ctx.sourceData, kPenisParts))
      return;

    static constexpr PenetrationSpec kPenetrationSpecs[]{
        {Name::Vagina, Type::Vaginal, 7.4f, 24.f, 4.8f, -3.0f, 10.5f},
        {Name::Anus, Type::Anal, 5.8f, 16.f, 3.9f, -2.0f, 8.8f},
    };
    for (const auto& spec : kPenetrationSpecs) {
      TryAddGenitalPenetrationCandidate(out, ctx, spec.receiverPart, spec.type, spec.maxDistance,
                                        spec.maxAngle, spec.maxAxis, spec.minDepth, spec.maxDepth);
    }
  }

  void GenerateSymmetricCandidates(std::vector<Candidate>& out, const PairContext& pair)
  {
    static constexpr SymmetricSpec kSymmetricSpecs[]{
        {Name::Mouth,
         Name::Mouth,
         Type::Kiss,
         CandidateFamily::Contact,
         6.0f,
         {145.f, 180.f},
         0.48f},
        {Name::Vagina,
         Name::Vagina,
         Type::Tribbing,
         CandidateFamily::Contact,
         7.2f,
         {150.f, 180.f},
         0.32f},
    };
    for (const auto& spec : kSymmetricSpecs) {
      TryAddSymmetricCandidate(out, pair, spec.partA, spec.partB, spec.type, spec.family,
                               spec.maxDistance, spec.angleRange, spec.angleWeight,
                               spec.scoreOffset);
    }
  }

  void GenerateDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    GenerateOralContactCandidates(out, ctx);
    GenerateManualCandidates(out, ctx);
    GenerateProximityCandidates(out, ctx);
    GeneratePenetrationCandidates(out, ctx);
  }

  void GeneratePairCandidates(std::vector<Candidate>& out, const PairContext& pair)
  {
    if (pair.self) {
      const DirectedContext selfCtx{pair.actorA, pair.actorA, pair.dataA, pair.dataA, true};
      GenerateManualCandidates(out, selfCtx);
      return;
    }

    GenerateSymmetricCandidates(out, pair);
    GenerateDirectedCandidates(
        out, DirectedContext{pair.actorA, pair.actorB, pair.dataA, pair.dataB, false});
    GenerateDirectedCandidates(
        out, DirectedContext{pair.actorB, pair.actorA, pair.dataB, pair.dataA, false});
  }

  Type ResolveSelfType(const Candidate& candidate)
  {
    if (candidate.sourceActor != candidate.targetActor)
      return candidate.type;

    if (candidate.type == Type::FingerVagina || candidate.type == Type::FingerAnus ||
        candidate.type == Type::Handjob)
      return Type::Masturbation;
    return candidate.type;
  }

  void BuildComboTypes(const std::unordered_map<RE::Actor*, ActorInteractSummary>& summaries,
                       std::unordered_map<RE::Actor*, std::vector<Type>>& comboTypes)
  {
    comboTypes.reserve(summaries.size());
    for (const auto& [actor, summary] : summaries) {
      if (summary.givesOral && summary.givesOralTo) {
        auto it = summaries.find(summary.givesOralTo);
        if (it != summaries.end()) {
          const ActorInteractSummary& partnerSummary = it->second;
          if ((partnerSummary.givesOral && partnerSummary.givesOralTo == actor) ||
              (partnerSummary.hasOral && partnerSummary.oralPartner == actor)) {
            comboTypes[actor].push_back(Type::SixtyNine);
          }
        }
      }

      if (summary.hasVaginal && summary.hasAnal && summary.vaginalPartner != summary.analPartner)
        comboTypes[actor].push_back(Type::DoublePenetration);

      if (summary.hasOral && summary.hasVaginal && summary.hasAnal &&
          summary.oralPartner != summary.vaginalPartner &&
          summary.oralPartner != summary.analPartner &&
          summary.vaginalPartner != summary.analPartner) {
        comboTypes[actor].push_back(Type::TriplePenetration);
      }

      if (summary.hasOral && (summary.hasVaginal || summary.hasAnal)) {
        const bool diffVaginal =
            summary.hasVaginal && summary.oralPartner != summary.vaginalPartner;
        const bool diffAnal = summary.hasAnal && summary.oralPartner != summary.analPartner;
        if (diffVaginal || diffAnal)
          comboTypes[actor].push_back(Type::Spitroast);
      }
    }
  }

  void ApplyComboOverrides(AssignMap& assigns,
                           const std::unordered_map<RE::Actor*, ActorInteractSummary>& summaries,
                           const std::unordered_map<RE::Actor*, std::vector<Type>>& comboTypes)
  {
    for (const auto& [actor, combos] : comboTypes) {
      auto assignIt = assigns.find(actor);
      if (assignIt == assigns.end())
        continue;

      auto summaryIt = summaries.find(actor);
      if (summaryIt == summaries.end())
        continue;

      auto& partMap                     = assignIt->second;
      const ActorInteractSummary& state = summaryIt->second;

      auto has = [&](Type type) {
        return std::find(combos.begin(), combos.end(), type) != combos.end();
      };
      auto set = [&](Name part, Type type) {
        auto it = partMap.find(part);
        if (it != partMap.end())
          it->second.type = type;
      };

      const bool hasTP = has(Type::TriplePenetration);
      const bool hasDP = has(Type::DoublePenetration);
      const bool hasSR = has(Type::Spitroast);
      const bool hasSN = has(Type::SixtyNine);

      if (hasTP) {
        set(Name::Mouth, Type::TriplePenetration);
        set(Name::Throat, Type::TriplePenetration);
        set(Name::Vagina, Type::TriplePenetration);
        set(Name::Anus, Type::TriplePenetration);
        continue;
      }

      if (hasDP) {
        set(Name::Vagina, Type::DoublePenetration);
        set(Name::Anus, Type::DoublePenetration);
      }

      if (hasSR) {
        set(Name::Mouth, Type::Spitroast);
        set(Name::Throat, Type::Spitroast);
        if (!hasDP) {
          if (state.hasVaginal)
            set(Name::Vagina, Type::Spitroast);
          if (state.hasAnal)
            set(Name::Anus, Type::Spitroast);
        }
      }

      if (hasSN)
        set(Name::Mouth, Type::SixtyNine);
    }
  }

  void AccumulateObservedInteractTags(
      const AssignMap& assigns, const std::unordered_map<RE::Actor*, std::vector<Type>>& comboTypes,
      std::unordered_map<RE::Actor*, Define::InteractTags>& observedInteractTags)
  {
    for (const auto& [actor, partMap] : assigns) {
      Define::InteractTags& tags = observedInteractTags[actor];
      for (const auto& [_, assign] : partMap)
        AccumulateObservedType(tags, assign.type);
    }

    for (const auto& [actor, types] : comboTypes) {
      Define::InteractTags& tags = observedInteractTags[actor];
      for (Type type : types)
        AccumulateObservedType(tags, type);
    }
  }

  void UpdateGenitalSlotMemory(std::unordered_map<RE::Actor*, ActorState>& actorStates,
                               const AssignMap& assigns, float deltaMs)
  {
    auto resolvePartner = [](const AssignInfo* assign, bool (*includeType)(Type)) -> RE::Actor* {
      return assign && includeType(assign->type) ? assign->partner : nullptr;
    };

    for (auto& [actor, actorState] : actorStates) {
      Temporal::UpdateSlotMemory(
          Temporal::GetSlotMemory(actorState, GenitalSlotKind::Vaginal),
          resolvePartner(Allocator::FindAssignInfo(assigns, actor, Name::Vagina), IncludesVaginal),
          deltaMs);
      Temporal::UpdateSlotMemory(
          Temporal::GetSlotMemory(actorState, GenitalSlotKind::Anal),
          resolvePartner(Allocator::FindAssignInfo(assigns, actor, Name::Anus), IncludesAnal),
          deltaMs);
    }
  }

  void ResetInteractionInfos(std::unordered_map<RE::Actor*, ActorState>& actorStates)
  {
    for (auto& [actor, actorState] : actorStates) {
      for (auto& [name, partState] : actorState.parts) {
        partState.previous = partState.current;
        partState.current  = {};
      }
    }
  }

  void UpdateBodyPartPositions(std::unordered_map<RE::Actor*, ActorState>& actorStates)
  {
    for (auto& [actor, actorState] : actorStates) {
      for (auto& [name, partState] : actorState.parts)
        partState.bodyPart.UpdatePosition();
    }
  }

  void CaptureMotionHistory(std::unordered_map<RE::Actor*, ActorState>& actorStates, float nowMs)
  {
    for (auto& [actor, actorState] : actorStates) {
      for (auto& [name, partState] : actorState.parts) {
        MotionHistory& history = partState.motion;
        history.older          = history.previous;
        history.previous       = history.current;
        history.current        = BuildSnapshot(partState.bodyPart, nowMs);
      }
    }
  }

  void ApplyAssignments(std::unordered_map<RE::Actor*, ActorState>& actorStates,
                        const AssignMap& assigns, float deltaMs)
  {
    for (auto& [actor, actorState] : actorStates) {
      auto assignIt = assigns.find(actor);
      if (assignIt == assigns.end())
        continue;

      for (auto& [partName, partState] : actorState.parts) {
        auto partIt = assignIt->second.find(partName);
        if (partIt == assignIt->second.end())
          continue;

        const AssignInfo& assign   = partIt->second;
        partState.current.partner  = assign.partner;
        partState.current.distance = assign.distance;
        partState.current.type     = assign.type;
        partState.current.approachSpeed =
            deltaMs > 1e-4f ? (assign.distance - partState.previous.distance) / deltaMs : 0.f;
      }
    }
  }

}  // namespace

#pragma endregion

#pragma region RuntimeEntryPoints

void InitializeInteractActors(std::unordered_map<RE::Actor*, Interact::ActorState>& actorStates,
                              const std::vector<RE::Actor*>& actors)
{
  actorStates.clear();

  for (auto* actor : actors) {
    if (!actor)
      continue;

    ActorState actorState;
    actorState.race   = Define::Race(actor);
    actorState.gender = Define::Gender(actor);
    actorState.parts.reserve(static_cast<std::size_t>(Name::Penis) + 1);

    for (std::uint8_t index = 0; index < static_cast<std::uint8_t>(Name::Penis) + 1; ++index) {
      const auto name = static_cast<Name>(index);
      if (!BP::HasBodyPart(actorState.gender, actorState.race, name))
        continue;
      actorState.parts.emplace(name, PartState{BP{actor, actorState.race, name}});
    }

    actorStates.emplace(actor, std::move(actorState));
  }
}

void FlashInteractNodeData(std::unordered_map<RE::Actor*, Interact::ActorState>& actorStates)
{
  for (auto& [actor, actorState] : actorStates)
    for (auto& [name, partState] : actorState.parts)
      partState.bodyPart.UpdateNodes();
}

void UpdateInteractState(std::unordered_map<RE::Actor*, Interact::ActorState>& actorStates,
                         std::unordered_map<RE::Actor*, Define::InteractTags>& observedInteractTags,
                         float nowMs, float deltaMs)
{
  ResetInteractionInfos(actorStates);
  UpdateBodyPartPositions(actorStates);
  CaptureMotionHistory(actorStates, nowMs);

  std::vector<Candidate> candidates;
  std::vector<RE::Actor*> actors;
  actors.reserve(actorStates.size());
  for (const auto& [actor, _] : actorStates)
    actors.push_back(actor);

  for (std::size_t i = 0; i < actors.size(); ++i) {
    for (std::size_t j = i; j < actors.size(); ++j) {
      RE::Actor* actorA = actors[i];
      RE::Actor* actorB = actors[j];
      GeneratePairCandidates(candidates, PairContext{actorA, actorB, actorStates.at(actorA),
                                                     actorStates.at(actorB), actorA == actorB});
    }
  }

  UsedPartSet usedParts;
  AssignMap assigns;

  const auto penetrationResult =
      Allocator::AssignPenetrationCandidates(candidates, actorStates, usedParts, assigns);
  LogFamilyOutcome(CandidateFamily::Penetration, penetrationResult.stats,
                   penetrationResult.assignedLogs);

  const auto proximityResult = Allocator::AssignFamilyCandidates(
      candidates, CandidateFamily::Proximity, actorStates, usedParts, assigns);
  LogFamilyOutcome(CandidateFamily::Proximity, proximityResult.stats, proximityResult.assignedLogs);

  const auto contactResult = Allocator::AssignFamilyCandidates(candidates, CandidateFamily::Contact,
                                                               actorStates, usedParts, assigns);
  LogFamilyOutcome(CandidateFamily::Contact, contactResult.stats, contactResult.assignedLogs);

  std::unordered_map<RE::Actor*, ActorInteractSummary> summaries;
  Allocator::BuildSummaries(assigns, summaries);

  std::unordered_map<RE::Actor*, std::vector<Type>> comboTypes;
  BuildComboTypes(summaries, comboTypes);
  AccumulateObservedInteractTags(assigns, comboTypes, observedInteractTags);
  ApplyComboOverrides(assigns, summaries, comboTypes);
  ApplyAssignments(actorStates, assigns, deltaMs);
  UpdateGenitalSlotMemory(actorStates, assigns, deltaMs);
}

#pragma endregion

}  // namespace Instance::Detail

#pragma region PublicAPI

namespace Instance
{

Interact::Interact(std::vector<RE::Actor*> actors)
{
  Detail::InitializeInteractActors(actorStates, actors);
}

void Interact::FlashNodeData()
{
  Detail::FlashInteractNodeData(actorStates);
}

void Interact::Update(float deltaMs)
{
  if (observedInteractTags.empty()) {
    for (const auto& [actor, _] : actorStates) {
      observedInteractTags.try_emplace(actor, Define::InteractTags{});
    }
  }

  const float clampedDeltaMs = (std::max)(deltaMs, 0.f);
  timelineMs += clampedDeltaMs;
  Detail::UpdateInteractState(actorStates, observedInteractTags, timelineMs, clampedDeltaMs);
}

const Interact::PartState& Interact::GetPartState(RE::Actor* actor,
                                                  Define::BodyPart::Name partName) const
{
  static const Interact::PartState kEmpty{};
  auto it = actorStates.find(actor);
  if (it == actorStates.end())
    return kEmpty;
  auto partIt = it->second.parts.find(partName);
  if (partIt == it->second.parts.end())
    return kEmpty;
  return partIt->second;
}

const Define::InteractTags& Interact::GetObservedInteractTags(RE::Actor* actor) const
{
  static const Define::InteractTags kEmpty{};
  auto it = observedInteractTags.find(actor);
  return it == observedInteractTags.end() ? kEmpty : it->second;
}

}  // namespace Instance

#pragma endregion

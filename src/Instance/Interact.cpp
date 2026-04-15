#include "Instance/Interact.h"

#include "Utils/Settings.h"

#include "magic_enum/magic_enum.hpp"

namespace Instance::Detail
{

#pragma region RuntimeInternals

using Name              = Define::BodyPart::Name;
using BP                = Define::BodyPart;
using Type              = Interact::Type;
using PartState         = Interact::PartState;
using ActorState        = Interact::ActorState;
using PenetrationMemory = Interact::PenetrationMemory;
using MotionSnapshot    = Interact::MotionSnapshot;
using MotionHistory     = Interact::MotionHistory;

namespace
{

  enum class CandidateFamily : std::uint8_t
  {
    Penetration = 0,
    Proximity   = 1,
    Contact     = 2,
  };

  enum class AngleModel : std::uint8_t
  {
    Absolute = 0,
    Wrap,
  };

  enum class AnchorMode : std::uint8_t
  {
    None = 0,
    StartToStart,
    EndToStart,
    StartToEnd,
    EndToEnd,
  };

  enum class GenitalSlotKind : std::uint8_t
  {
    Vaginal = 0,
    Anal,
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

  struct Candidate
  {
    RE::Actor* sourceActor = nullptr;
    Name sourcePart        = Name::Mouth;
    RE::Actor* targetActor = nullptr;
    Name targetPart        = Name::Mouth;
    Type type              = Type::None;
    float distance         = 0.f;
    float score            = 0.f;
    CandidateFamily family = CandidateFamily::Contact;
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

  struct AssignInfo
  {
    RE::Actor* partner = nullptr;
    Name partSelf      = Name::Mouth;
    Name partPartner   = Name::Mouth;
    float distance     = 0.f;
    Type type          = Type::None;
  };

  struct ActorInteractSummary
  {
    bool hasOral              = false;
    bool givesOral            = false;
    bool hasVaginal           = false;
    bool hasAnal              = false;
    RE::Actor* oralPartner    = nullptr;
    RE::Actor* vaginalPartner = nullptr;
    RE::Actor* analPartner    = nullptr;
    RE::Actor* givesOralTo    = nullptr;
  };

  struct GenitalPlan
  {
    RE::Actor* receiver      = nullptr;
    const Candidate* vaginal = nullptr;
    const Candidate* anal    = nullptr;
    float score              = 0.f;
    std::uint8_t filledSlots = 0;
  };

  struct SampledMetrics
  {
    Define::Point3f rootPoint = Define::Point3f::Zero();
    Define::Point3f midPoint  = Define::Point3f::Zero();
    Define::Point3f tipPoint  = Define::Point3f::Zero();
    float rootDistance        = std::numeric_limits<float>::infinity();
    float midDistance         = std::numeric_limits<float>::infinity();
    float tipDistance         = std::numeric_limits<float>::infinity();
    float minDistance         = std::numeric_limits<float>::infinity();
    float rootEntryDistance   = std::numeric_limits<float>::infinity();
    float midEntryDistance    = std::numeric_limits<float>::infinity();
    float tipEntryDistance    = std::numeric_limits<float>::infinity();
    float minEntryDistance    = std::numeric_limits<float>::infinity();
    float rootAxisDistance    = std::numeric_limits<float>::infinity();
    float midAxisDistance     = std::numeric_limits<float>::infinity();
    float tipAxisDistance     = std::numeric_limits<float>::infinity();
    float minAxisDistance     = std::numeric_limits<float>::infinity();
    float rootDepth           = 0.f;
    float midDepth            = 0.f;
    float tipDepth            = 0.f;
    float overlapDepth        = 0.f;
  };

  struct PenisContextMetrics
  {
    float vaginalSupport       = std::numeric_limits<float>::infinity();
    float analSupport          = std::numeric_limits<float>::infinity();
    float vaginalEntryDistance = std::numeric_limits<float>::infinity();
    float analEntryDistance    = std::numeric_limits<float>::infinity();
  };

  struct FamilyLogStats
  {
    std::size_t total      = 0;
    std::size_t considered = 0;
    std::size_t suppressed = 0;
    std::size_t blocked    = 0;
  };

  struct AssignedCandidateLog
  {
    Candidate candidate{};
    Type resolvedType = Type::None;
  };

  struct PartKey
  {
    RE::Actor* actor = nullptr;
    Name part        = Name::Mouth;

    bool operator==(const PartKey&) const = default;
  };

  struct PartKeyHash
  {
    std::size_t operator()(const PartKey& key) const
    {
      return std::hash<RE::Actor*>{}(key.actor) ^ (static_cast<std::size_t>(key.part) << 8);
    }
  };

  using AssignMap   = std::unordered_map<RE::Actor*, std::unordered_map<Name, AssignInfo>>;
  using UsedPartSet = std::unordered_set<PartKey, PartKeyHash>;

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

  bool IsGenitalPenetrationType(Type type)
  {
    return type == Type::Vaginal || type == Type::Anal;
  }

  float GetSteadyNowMs()
  {
    return static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now().time_since_epoch())
                                  .count()) /
           1000.f;
  }

  std::uint32_t GetActorFormID(RE::Actor* actor)
  {
    return actor ? actor->GetFormID() : 0u;
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
      logger::info("[SexLab NG] Interact {} assigned type={} resolved={} src={:08X}:{} "
                   "dst={:08X}:{} score={:.3f} distance={:.2f}",
                   FamilyText(family), TypeText(candidate.type), TypeText(entry.resolvedType),
                   GetActorFormID(candidate.sourceActor), NameText(candidate.sourcePart),
                   GetActorFormID(candidate.targetActor), NameText(candidate.targetPart),
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

  namespace Geometry
  {

    bool HasDirectionalAxis(const MotionSnapshot& part)
    {
      return part.valid && part.directional && part.length >= 1e-6f;
    }

    bool IsPointInFront(const MotionSnapshot& part, const Define::Point3f& point)
    {
      if (!HasDirectionalAxis(part))
        return false;
      return (point - part.start).dot(part.direction) > 0.f;
    }

    bool IsHorizontal(const MotionSnapshot& part, float toleranceDeg = 20.f)
    {
      if (!HasDirectionalAxis(part))
        return false;

      const float sinTol = std::sin(toleranceDeg * std::numbers::pi_v<float> / 180.f);
      return std::abs(part.direction.z()) <= sinTol;
    }

    float DistancePointToSegment(const Define::Point3f& point, const Define::Point3f& segStart,
                                 const Define::Point3f& segEnd)
    {
      const Define::Vector3f segment = segEnd - segStart;
      const float length             = segment.norm();
      if (length < 1e-6f)
        return (point - segStart).norm();

      const Define::Vector3f dir = segment / length;
      const float t              = std::clamp((point - segStart).dot(dir), 0.f, length);
      return (point - (segStart + dir * t)).norm();
    }

    float ProjectionOnSegmentAxis(const Define::Point3f& point, const Define::Point3f& segStart,
                                  const Define::Point3f& segEnd)
    {
      const Define::Vector3f segment = segEnd - segStart;
      const float length             = segment.norm();
      if (length < 1e-6f)
        return 0.f;

      return (point - segStart).dot(segment / length);
    }

    float MeasurePartAngle(const MotionSnapshot& lhs, const MotionSnapshot& rhs)
    {
      if (!HasDirectionalAxis(lhs) || !HasDirectionalAxis(rhs))
        return 0.f;

      const float dot = std::clamp(lhs.direction.dot(rhs.direction), -1.f, 1.f);
      return std::acos(dot) * (180.f / std::numbers::pi_v<float>);
    }

    float DistancePointToPart(const Define::Point3f& point, const MotionSnapshot& part)
    {
      if (!HasDirectionalAxis(part))
        return (point - part.start).norm();
      return DistancePointToSegment(point, part.start, part.end);
    }

    float MeasurePartDistance(const MotionSnapshot& lhs, const MotionSnapshot& rhs)
    {
      if (!HasDirectionalAxis(lhs) && !HasDirectionalAxis(rhs))
        return (lhs.start - rhs.start).norm();
      if (!HasDirectionalAxis(lhs))
        return DistancePointToPart(lhs.start, rhs);
      if (!HasDirectionalAxis(rhs))
        return DistancePointToPart(rhs.start, lhs);

      const Define::Vector3f dA = lhs.direction;
      const Define::Vector3f dB = rhs.direction;
      const float lA            = lhs.length;
      const float lB            = rhs.length;
      const Define::Vector3f w  = rhs.start - lhs.start;
      const float b             = dA.dot(dB);
      const float d             = dA.dot(w);
      const float e             = dB.dot(w);
      const float denom         = 1.f - b * b;

      if (denom < 1e-6f) {
        float distance = DistancePointToPart(lhs.start, rhs);
        distance       = (std::min)(distance, DistancePointToPart(rhs.start, lhs));
        distance       = (std::min)(distance, DistancePointToPart(lhs.end, rhs));
        distance       = (std::min)(distance, DistancePointToPart(rhs.end, lhs));
        return distance;
      }

      float s = (b * e - d) / denom;
      float t = (e - b * d) / denom;

      if (s < 0.f) {
        s = 0.f;
        t = std::clamp(e, 0.f, lB);
      } else if (s > lA) {
        s = lA;
        t = std::clamp(e + b * lA, 0.f, lB);
      }

      if (t < 0.f) {
        t = 0.f;
        s = std::clamp(-d, 0.f, lA);
      } else if (t > lB) {
        t = lB;
        s = std::clamp(b * lB - d, 0.f, lA);
      }

      const Define::Point3f p = lhs.start + dA * s;
      const Define::Point3f q = rhs.start + dB * t;
      return (p - q).norm();
    }

    Define::Point3f SamplePoint(const MotionSnapshot& part, float fraction)
    {
      if (!HasDirectionalAxis(part))
        return part.start;
      return part.start + (part.end - part.start) * std::clamp(fraction, 0.f, 1.f);
    }

    SampledMetrics MeasureSampledMetrics(const MotionSnapshot& sampledPart,
                                         const MotionSnapshot& targetPart)
    {
      SampledMetrics metrics;
      metrics.rootPoint = SamplePoint(sampledPart, 0.f);
      metrics.midPoint  = SamplePoint(sampledPart, 0.5f);
      metrics.tipPoint  = SamplePoint(sampledPart, 1.f);

      const auto measurePoint = [&](const Define::Point3f& point, float& distance,
                                    float& entryDistance, float& axisDistance, float& depth) {
        distance      = DistancePointToPart(point, targetPart);
        entryDistance = (point - targetPart.start).norm();

        if (!HasDirectionalAxis(targetPart)) {
          axisDistance = distance;
          depth        = 0.f;
          return;
        }

        axisDistance = DistancePointToSegment(point, targetPart.start, targetPart.end);
        depth        = ProjectionOnSegmentAxis(point, targetPart.start, targetPart.end);
      };

      measurePoint(metrics.rootPoint, metrics.rootDistance, metrics.rootEntryDistance,
                   metrics.rootAxisDistance, metrics.rootDepth);
      measurePoint(metrics.midPoint, metrics.midDistance, metrics.midEntryDistance,
                   metrics.midAxisDistance, metrics.midDepth);
      measurePoint(metrics.tipPoint, metrics.tipDistance, metrics.tipEntryDistance,
                   metrics.tipAxisDistance, metrics.tipDepth);

      metrics.minDistance =
          (std::min)(metrics.rootDistance, (std::min)(metrics.midDistance, metrics.tipDistance));
      metrics.minEntryDistance =
          (std::min)(metrics.rootEntryDistance,
                     (std::min)(metrics.midEntryDistance, metrics.tipEntryDistance));
      metrics.minAxisDistance =
          (std::min)(metrics.rootAxisDistance,
                     (std::min)(metrics.midAxisDistance, metrics.tipAxisDistance));

      if (HasDirectionalAxis(targetPart)) {
        const float projectedMin = (std::min)(metrics.rootDepth, metrics.tipDepth);
        const float projectedMax = (std::max)(metrics.rootDepth, metrics.tipDepth);
        metrics.overlapDepth     = (std::max)(0.f, (std::min)(projectedMax, targetPart.length) -
                                                       (std::max)(projectedMin, 0.f));
      }

      return metrics;
    }

    float SampledShaftDistance(const SampledMetrics& metrics)
    {
      return (std::min)(metrics.midDistance, metrics.tipDistance);
    }

    float SampledShaftEntryDistance(const SampledMetrics& metrics)
    {
      return (std::min)(metrics.midEntryDistance, metrics.tipEntryDistance);
    }

    Define::Point3f SelectAnchorPoint(const MotionSnapshot& part, bool useEnd)
    {
      return useEnd ? part.end : part.start;
    }

    float AnchorDistance(const MotionSnapshot& sourcePart, const MotionSnapshot& targetPart,
                         AnchorMode mode)
    {
      switch (mode) {
      case AnchorMode::StartToStart:
        return (SelectAnchorPoint(sourcePart, false) - SelectAnchorPoint(targetPart, false)).norm();
      case AnchorMode::EndToStart:
        return (SelectAnchorPoint(sourcePart, true) - SelectAnchorPoint(targetPart, false)).norm();
      case AnchorMode::StartToEnd:
        return (SelectAnchorPoint(sourcePart, false) - SelectAnchorPoint(targetPart, true)).norm();
      case AnchorMode::EndToEnd:
        return (SelectAnchorPoint(sourcePart, true) - SelectAnchorPoint(targetPart, true)).norm();
      case AnchorMode::None:
      default:
        return 0.f;
      }
    }

    // ── Collider geometry primitives ──────────────────────────────────────

    float DistancePointToCapsule(const Define::Point3f& point,
                                 const Define::CapsuleCollider& capsule)
    {
      const float segDist = DistancePointToSegment(point, capsule.start, capsule.end);
      return (std::max)(0.f, segDist - capsule.radius);
    }

    float DistancePointToBox(const Define::Point3f& point, const Define::BoxCollider& box)
    {
      const Define::Vector3f local = point - box.center;
      float sqDist                 = 0.f;
      for (int axis = 0; axis < 3; ++axis) {
        const float proj   = local.dot(box.basis.col(axis));
        const float half   = box.halfExtents[axis];
        const float excess = std::abs(proj) - half;
        if (excess > 0.f)
          sqDist += excess * excess;
      }
      return std::sqrt(sqDist);
    }

    float DistanceCapsuleToCapsule(const Define::CapsuleCollider& a,
                                   const Define::CapsuleCollider& b)
    {
      // Reuse segment-segment closest distance via MeasurePartDistance logic
      MotionSnapshot segA;
      segA.start       = a.start;
      segA.end         = a.end;
      segA.direction   = (a.end - a.start);
      segA.length      = segA.direction.norm();
      segA.valid       = true;
      segA.directional = segA.length >= 1e-6f;
      if (segA.directional)
        segA.direction /= segA.length;

      MotionSnapshot segB;
      segB.start       = b.start;
      segB.end         = b.end;
      segB.direction   = (b.end - b.start);
      segB.length      = segB.direction.norm();
      segB.valid       = true;
      segB.directional = segB.length >= 1e-6f;
      if (segB.directional)
        segB.direction /= segB.length;

      const float axisDist = MeasurePartDistance(segA, segB);
      return (std::max)(0.f, axisDist - a.radius - b.radius);
    }

    float DistanceCapsuleToBox(const Define::CapsuleCollider& capsule,
                               const Define::BoxCollider& box)
    {
      // Sample capsule axis at start, mid, end and take best box distance minus radius
      const Define::Point3f mid = (capsule.start + capsule.end) * 0.5f;
      float best                = DistancePointToBox(capsule.start, box);
      best                      = (std::min)(best, DistancePointToBox(mid, box));
      best                      = (std::min)(best, DistancePointToBox(capsule.end, box));
      return (std::max)(0.f, best - capsule.radius);
    }

    float DistanceColliderToCollider(const MotionSnapshot& a, const MotionSnapshot& b)
    {
      float best = std::numeric_limits<float>::infinity();

      for (const auto& ca : a.collisions.capsules) {
        for (const auto& cb : b.collisions.capsules)
          best = (std::min)(best, DistanceCapsuleToCapsule(ca, cb));
        for (const auto& bb : b.collisions.boxes)
          best = (std::min)(best, DistanceCapsuleToBox(ca, bb));
      }

      for (const auto& ba : a.collisions.boxes) {
        for (const auto& cb : b.collisions.capsules)
          best = (std::min)(best, DistanceCapsuleToBox(cb, ba));
        // Box-box: sample centers cross-checked
        for (const auto& bb : b.collisions.boxes) {
          best = (std::min)(best, DistancePointToBox(ba.center, bb));
          best = (std::min)(best, DistancePointToBox(bb.center, ba));
        }
      }

      return best;
    }

    float DistancePointToCollider(const Define::Point3f& point, const MotionSnapshot& collider)
    {
      float best = std::numeric_limits<float>::infinity();
      for (const auto& capsule : collider.collisions.capsules)
        best = (std::min)(best, DistancePointToCapsule(point, capsule));
      for (const auto& box : collider.collisions.boxes)
        best = (std::min)(best, DistancePointToBox(point, box));
      return best;
    }

    float DistanceSegmentToCollider(const Define::Point3f& segStart, const Define::Point3f& segEnd,
                                    const MotionSnapshot& collider)
    {
      // Sample segment at start, mid, end for practical approximation
      const Define::Point3f mid = (segStart + segEnd) * 0.5f;
      float best                = DistancePointToCollider(segStart, collider);
      best                      = (std::min)(best, DistancePointToCollider(mid, collider));
      best                      = (std::min)(best, DistancePointToCollider(segEnd, collider));
      return best;
    }

    float MeasureSpatialDistance(const MotionSnapshot& lhs, const MotionSnapshot& rhs)
    {
      const bool lhsHas = lhs.HasCollider();
      const bool rhsHas = rhs.HasCollider();

      if (lhsHas && rhsHas)
        return DistanceColliderToCollider(lhs, rhs);

      if (lhsHas && !rhsHas) {
        if (HasDirectionalAxis(rhs))
          return DistanceSegmentToCollider(rhs.start, rhs.end, lhs);
        return DistancePointToCollider(rhs.start, lhs);
      }

      if (!lhsHas && rhsHas) {
        if (HasDirectionalAxis(lhs))
          return DistanceSegmentToCollider(lhs.start, lhs.end, rhs);
        return DistancePointToCollider(lhs.start, rhs);
      }

      return MeasurePartDistance(lhs, rhs);
    }

  }  // namespace Geometry

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
    const PartState* state = TryGetPartState(data, part);
    if (!state)
      return 0.f;
    if (state->previous.partner == partner && state->previous.type == type)
      return 0.12f;
    if (state->previous.partner == partner)
      return 0.06f;
    if (state->previous.type == type)
      return 0.03f;
    return 0.f;
  }

  bool HadPreviousInteraction(const ActorState& data, Name part, RE::Actor* partner, Type type)
  {
    const PartState* state = TryGetPartState(data, part);
    return state && state->previous.partner == partner && state->previous.type == type;
  }

  bool HasStickyInteractionPair(const DirectedContext& ctx, Name sourcePart, Name targetPart,
                                Type type)
  {
    return HadPreviousInteraction(ctx.sourceData, sourcePart, ctx.targetActor, type) ||
           HadPreviousInteraction(ctx.targetData, targetPart, ctx.sourceActor, type);
  }

  bool IncludesVaginal(Type type)
  {
    switch (type) {
    case Type::Vaginal:
    case Type::DoublePenetration:
    case Type::TriplePenetration:
    case Type::Spitroast:
      return true;
    default:
      return false;
    }
  }

  bool IncludesAnal(Type type)
  {
    switch (type) {
    case Type::Anal:
    case Type::DoublePenetration:
    case Type::TriplePenetration:
    case Type::Spitroast:
      return true;
    default:
      return false;
    }
  }

  bool HasPreviousPartContext(const ActorState& data, Name part, bool (*predicate)(Type))
  {
    const PartState* state = TryGetPartState(data, part);
    return state && predicate(state->previous.type);
  }

  bool HadPreviousPenetrationWithPartner(const ActorState& data, Name part, RE::Actor* partner)
  {
    const PartState* state = TryGetPartState(data, part);
    if (!state || state->previous.partner != partner)
      return false;

    return IncludesVaginal(state->previous.type) || IncludesAnal(state->previous.type);
  }

  RE::Actor* GetPreviousPenetrationPartner(const ActorState& data, Name part)
  {
    const PartState* state = TryGetPartState(data, part);
    if (!state)
      return nullptr;

    return IncludesVaginal(state->previous.type) || IncludesAnal(state->previous.type)
               ? state->previous.partner
               : nullptr;
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

  float ComputeGenitalAngleHoldMargin(Type type, bool stickyPair, bool previousSameType,
                                      bool strongContext, float rootEntryDistance,
                                      float predictedAngle, float effectiveMaxAngle)
  {
    if (!stickyPair && !previousSameType)
      return 0.f;

    float margin = 0.f;
    if (previousSameType)
      margin += type == Type::Vaginal ? 4.5f : 3.5f;

    if (stickyPair && rootEntryDistance <= 3.0f)
      margin += type == Type::Vaginal ? 4.0f : 3.0f;
    else if (stickyPair && rootEntryDistance <= 4.0f)
      margin += type == Type::Vaginal ? 2.5f : 2.0f;

    if (strongContext)
      margin += 1.5f;

    if (std::isfinite(predictedAngle) && predictedAngle <= effectiveMaxAngle + 2.0f)
      margin += 1.75f;

    return margin;
  }

  bool HasStableAngleTrend(float previousAngle, float currentAngle, float predictedAngle,
                           float effectiveMaxAngle)
  {
    if (std::isfinite(previousAngle) && previousAngle <= effectiveMaxAngle + 2.5f)
      return true;

    if (!std::isfinite(predictedAngle))
      return false;

    return predictedAngle <= effectiveMaxAngle + 2.0f || predictedAngle + 1.5f < currentAngle;
  }

  bool HasConfirmedPenetrationSwitchTrend(float previousDistance, float distance,
                                          float previousAngle, float angle, float predictedDistance,
                                          float predictedAngle)
  {
    const bool distanceImproving =
        std::isfinite(previousDistance) && previousDistance - distance >= 0.35f;
    const bool angleImproving = std::isfinite(previousAngle) && previousAngle - angle >= 3.0f;
    const bool predictedImproving =
        (std::isfinite(predictedDistance) && predictedDistance + 0.25f < distance) ||
        (std::isfinite(predictedAngle) && predictedAngle + 2.0f < angle);

    return distanceImproving || angleImproving || predictedImproving;
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

    const bool stickyPair = HasStickyInteractionPair(ctx, Name::Penis, receiverPartName, type);
    const bool previousSameType =
        HadPreviousInteraction(ctx.sourceData, Name::Penis, ctx.targetActor, type) ||
        HadPreviousInteraction(ctx.targetData, receiverPartName, ctx.sourceActor, type);
    const bool previousOppositeType =
        type == Type::Anal
            ? (HadPreviousInteraction(ctx.sourceData, Name::Penis, ctx.targetActor,
                                      Type::Vaginal) ||
               HadPreviousInteraction(ctx.targetData, Name::Vagina, ctx.sourceActor, Type::Vaginal))
        : type == Type::Vaginal
            ? (HadPreviousInteraction(ctx.sourceData, Name::Penis, ctx.targetActor, Type::Anal) ||
               HadPreviousInteraction(ctx.targetData, Name::Anus, ctx.sourceActor, Type::Anal))
            : false;

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
    const float angleHoldMargin =
        ComputeGenitalAngleHoldMargin(type, stickyPair, previousSameType, strongContext,
                                      rootEntryDistance, predictedAngle, effectiveMaxAngle);
    const bool heldByAngleHysteresis =
        angle > effectiveMaxAngle && angle <= effectiveMaxAngle + angleHoldMargin &&
        HasStableAngleTrend(previousAngle, angle, predictedAngle, effectiveMaxAngle);
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
        HasConfirmedPenetrationSwitchTrend(previousDistance, distance, previousAngle, angle,
                                           predictedDistance, predictedAngle);
    const bool comfortableSwitchGeometry = distance <= effectiveMaxDistance - 0.45f &&
                                           angle <= effectiveMaxAngle - 4.5f &&
                                           axisDistance <= effectiveMaxAxis - 0.35f;
    if (previousOppositeType && !previousSameType && !switchTrendConfirmed &&
        !comfortableSwitchGeometry) {
      return;
    }

    if (previousOppositeType && !strongContext)
      switchPenalty = type == Type::Anal ? 0.30f : 0.24f;
    if (previousOppositeType && !switchTrendConfirmed)
      switchPenalty += type == Type::Anal ? 0.08f : 0.06f;

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

    if (type == Type::Anal && score > 0.f)
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

  const AssignInfo* FindAssignInfo(const AssignMap& assigns, RE::Actor* actor, Name partName)
  {
    auto actorIt = assigns.find(actor);
    if (actorIt == assigns.end())
      return nullptr;

    auto partIt = actorIt->second.find(partName);
    return partIt == actorIt->second.end() ? nullptr : &partIt->second;
  }

  void BuildSummaries(const AssignMap& assigns,
                      std::unordered_map<RE::Actor*, ActorInteractSummary>& summaries)
  {
    summaries.reserve(assigns.size());
    for (const auto& [actor, partMap] : assigns) {
      ActorInteractSummary& summary = summaries[actor];
      for (const auto& [partName, assign] : partMap) {
        switch (assign.type) {
        case Type::Fellatio:
          if (partName == Name::Mouth) {
            summary.hasOral     = true;
            summary.oralPartner = assign.partner;
          }
          break;
        case Type::DeepThroat:
          if (partName == Name::Throat) {
            summary.hasOral     = true;
            summary.oralPartner = assign.partner;
          }
          break;
        case Type::Cunnilingus:
        case Type::Anilingus:
          if (partName == Name::Mouth) {
            summary.givesOral   = true;
            summary.givesOralTo = assign.partner;
          }
          break;
        case Type::Vaginal:
          if (partName == Name::Vagina) {
            summary.hasVaginal     = true;
            summary.vaginalPartner = assign.partner;
          }
          break;
        case Type::Anal:
          if (partName == Name::Anus) {
            summary.hasAnal     = true;
            summary.analPartner = assign.partner;
          }
          break;
        default:
          break;
        }
      }
    }
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

  bool ShouldSuppressProximityCandidate(
      const Candidate& candidate,
      const std::unordered_map<RE::Actor*, ActorInteractSummary>& summaries)
  {
    auto it = summaries.find(candidate.targetActor);
    if (it == summaries.end())
      return false;

    return (it->second.hasVaginal && it->second.vaginalPartner == candidate.sourceActor) ||
           (it->second.hasAnal && it->second.analPartner == candidate.sourceActor);
  }

  bool ShouldSuppressRecentPenetrationFallback(
      const Candidate& candidate, const std::unordered_map<RE::Actor*, ActorState>& actorStates)
  {
    const auto sourceIt = actorStates.find(candidate.sourceActor);
    const auto targetIt = actorStates.find(candidate.targetActor);
    if (sourceIt == actorStates.end() || targetIt == actorStates.end())
      return false;

    const RE::Actor* sourceRecentPartner =
        GetPreviousPenetrationPartner(sourceIt->second, Name::Penis);
    const bool sourceRecent = sourceRecentPartner != nullptr;
    const bool targetRecent =
        HadPreviousPenetrationWithPartner(targetIt->second, Name::Vagina, candidate.sourceActor) ||
        HadPreviousPenetrationWithPartner(targetIt->second, Name::Anus, candidate.sourceActor);
    if (!sourceRecent && !targetRecent)
      return false;

    if (sourceRecentPartner && sourceRecentPartner != candidate.targetActor)
      return true;

    return TryGetCurrentSnapshot(targetIt->second, Name::Vagina) ||
           TryGetCurrentSnapshot(targetIt->second, Name::Anus);
  }

  float GetProximityPenalty(const Candidate& candidate,
                            const std::unordered_map<RE::Actor*, ActorInteractSummary>& summaries,
                            const std::unordered_map<RE::Actor*, ActorState>& actorStates)
  {
    const auto summaryIt      = summaries.find(candidate.targetActor);
    const bool currentVaginal = summaryIt != summaries.end() && summaryIt->second.hasVaginal &&
                                summaryIt->second.vaginalPartner == candidate.sourceActor;
    const bool currentAnal    = summaryIt != summaries.end() && summaryIt->second.hasAnal &&
                                summaryIt->second.analPartner == candidate.sourceActor;

    const auto dataIt = actorStates.find(candidate.targetActor);
    const bool prevVaginal =
        dataIt != actorStates.end() &&
        HadPreviousPenetrationWithPartner(dataIt->second, Name::Vagina, candidate.sourceActor);
    const bool prevAnal =
        dataIt != actorStates.end() &&
        HadPreviousPenetrationWithPartner(dataIt->second, Name::Anus, candidate.sourceActor);
    const bool anyCurrent  = currentVaginal || currentAnal;
    const bool anyPrevious = prevVaginal || prevAnal;
    float penalty          = 0.f;

    if (anyCurrent)
      penalty += 0.22f;
    if (anyPrevious)
      penalty += 0.10f;

    if (candidate.type == Type::Titfuck && (currentVaginal || prevVaginal))
      penalty += 0.12f;
    else if (candidate.type == Type::Thighjob && (anyCurrent || anyPrevious))
      penalty += 0.10f;
    else if (candidate.type == Type::Naveljob && (anyCurrent || anyPrevious))
      penalty += 0.14f;
    else if (candidate.type == Type::Frottage && (currentAnal || prevAnal))
      penalty += 0.16f;

    return penalty;
  }

  bool TryAssignCandidate(const Candidate& candidate, UsedPartSet& usedParts, AssignMap& assigns,
                          std::vector<AssignedCandidateLog>* assignedLogs = nullptr);

  bool CandidateLess(const Candidate* lhs, const Candidate* rhs)
  {
    if (lhs->score != rhs->score)
      return lhs->score < rhs->score;
    if (lhs->distance != rhs->distance)
      return lhs->distance < rhs->distance;
    return static_cast<std::uint8_t>(lhs->type) < static_cast<std::uint8_t>(rhs->type);
  }

  float GetGenitalSlotContinuityAdjustment(
      RE::Actor* receiver, GenitalSlotKind slot, RE::Actor* challenger,
      const std::unordered_map<RE::Actor*, PenetrationMemory>& penetrationMemory)
  {
    auto it = penetrationMemory.find(receiver);
    if (it == penetrationMemory.end())
      return 0.f;

    const PenetrationMemory& memory = it->second;
    const auto& slotMemory        = slot == GenitalSlotKind::Vaginal ? memory.vaginal : memory.anal;
    const RE::Actor* incumbent    = slotMemory.partner;
    const std::uint8_t continuity = slotMemory.continuity;

    if (!incumbent)
      return 0.f;
    if (incumbent == challenger)
      return -0.18f - 0.04f * static_cast<float>((std::min)(continuity, std::uint8_t{4}));

    return 0.10f + 0.03f * static_cast<float>((std::min)(continuity, std::uint8_t{4}));
  }

  float ComputeGenitalPlanScore(
      RE::Actor* receiver, const Candidate* vaginal, const Candidate* anal,
      const std::unordered_map<RE::Actor*, PenetrationMemory>& penetrationMemory)
  {
    float score = 0.f;

    if (vaginal) {
      score += vaginal->score;
      score += GetGenitalSlotContinuityAdjustment(receiver, GenitalSlotKind::Vaginal,
                                                  vaginal->sourceActor, penetrationMemory);
    }

    if (anal) {
      score += anal->score;
      score += GetGenitalSlotContinuityAdjustment(receiver, GenitalSlotKind::Anal,
                                                  anal->sourceActor, penetrationMemory);
    }

    return score;
  }

  std::vector<GenitalPlan> BuildReceiverGenitalPlans(
      RE::Actor* receiver, const std::vector<const Candidate*>& vaginalCandidates,
      const std::vector<const Candidate*>& analCandidates,
      const std::unordered_map<RE::Actor*, PenetrationMemory>& penetrationMemory)
  {
    constexpr std::size_t kMaxCandidatesPerSlot = 4;

    std::vector<const Candidate*> sortedVaginal = vaginalCandidates;
    std::vector<const Candidate*> sortedAnal    = analCandidates;
    std::sort(sortedVaginal.begin(), sortedVaginal.end(), CandidateLess);
    std::sort(sortedAnal.begin(), sortedAnal.end(), CandidateLess);

    if (sortedVaginal.size() > kMaxCandidatesPerSlot)
      sortedVaginal.resize(kMaxCandidatesPerSlot);
    if (sortedAnal.size() > kMaxCandidatesPerSlot)
      sortedAnal.resize(kMaxCandidatesPerSlot);

    std::vector<GenitalPlan> plans;
    plans.reserve(1 + sortedVaginal.size() + sortedAnal.size() +
                  sortedVaginal.size() * sortedAnal.size());
    plans.push_back(GenitalPlan{receiver, nullptr, nullptr, 0.f, 0});

    for (const Candidate* vaginal : sortedVaginal) {
      plans.push_back(
          GenitalPlan{receiver, vaginal, nullptr,
                      ComputeGenitalPlanScore(receiver, vaginal, nullptr, penetrationMemory), 1});
    }

    for (const Candidate* anal : sortedAnal) {
      plans.push_back(
          GenitalPlan{receiver, nullptr, anal,
                      ComputeGenitalPlanScore(receiver, nullptr, anal, penetrationMemory), 1});
    }

    for (const Candidate* vaginal : sortedVaginal) {
      for (const Candidate* anal : sortedAnal) {
        if (vaginal->sourceActor == anal->sourceActor && vaginal->sourcePart == anal->sourcePart)
          continue;

        plans.push_back(
            GenitalPlan{receiver, vaginal, anal,
                        ComputeGenitalPlanScore(receiver, vaginal, anal, penetrationMemory), 2});
      }
    }

    std::sort(plans.begin(), plans.end(), [](const GenitalPlan& lhs, const GenitalPlan& rhs) {
      if (lhs.filledSlots != rhs.filledSlots)
        return lhs.filledSlots > rhs.filledSlots;
      return lhs.score < rhs.score;
    });

    return plans;
  }

  bool IsPlanCompatible(const GenitalPlan& plan, const UsedPartSet& usedParts)
  {
    auto available = [&](const Candidate* candidate) {
      return !candidate || (usedParts.count({candidate->sourceActor, candidate->sourcePart}) == 0 &&
                            usedParts.count({candidate->targetActor, candidate->targetPart}) == 0);
    };

    return available(plan.vaginal) && available(plan.anal);
  }

  void ApplyPlanUsage(const GenitalPlan& plan, UsedPartSet& usedParts)
  {
    auto mark = [&](const Candidate* candidate) {
      if (!candidate)
        return;
      usedParts.insert({candidate->sourceActor, candidate->sourcePart});
      usedParts.insert({candidate->targetActor, candidate->targetPart});
    };

    mark(plan.vaginal);
    mark(plan.anal);
  }

  void RemovePlanUsage(const GenitalPlan& plan, UsedPartSet& usedParts)
  {
    auto unmark = [&](const Candidate* candidate) {
      if (!candidate)
        return;
      usedParts.erase({candidate->sourceActor, candidate->sourcePart});
      usedParts.erase({candidate->targetActor, candidate->targetPart});
    };

    unmark(plan.vaginal);
    unmark(plan.anal);
  }

  struct GenitalSearchResult
  {
    int filledSlots = -1;
    float score     = std::numeric_limits<float>::infinity();
    std::vector<GenitalPlan> plans{};
  };

  void SearchReceiverGenitalPlans(
      std::size_t index,
      const std::vector<std::pair<RE::Actor*, std::vector<GenitalPlan>>>& receiverPlans,
      const std::vector<int>& remainingCapacity, UsedPartSet& usedParts,
      std::vector<GenitalPlan>& currentPlans, int currentFilledSlots, float currentScore,
      GenitalSearchResult& best)
  {
    if (index >= receiverPlans.size()) {
      if (currentFilledSlots > best.filledSlots ||
          (currentFilledSlots == best.filledSlots && currentScore < best.score)) {
        best.filledSlots = currentFilledSlots;
        best.score       = currentScore;
        best.plans       = currentPlans;
      }
      return;
    }

    if (currentFilledSlots + remainingCapacity[index] < best.filledSlots)
      return;

    for (const GenitalPlan& plan : receiverPlans[index].second) {
      if (!IsPlanCompatible(plan, usedParts))
        continue;

      ApplyPlanUsage(plan, usedParts);
      currentPlans.push_back(plan);
      SearchReceiverGenitalPlans(index + 1, receiverPlans, remainingCapacity, usedParts,
                                 currentPlans, currentFilledSlots + plan.filledSlots,
                                 currentScore + plan.score, best);
      currentPlans.pop_back();
      RemovePlanUsage(plan, usedParts);
    }
  }

  void AssignSortedPenetrationCandidates(const std::vector<const Candidate*>& sortedCandidates,
                                         UsedPartSet& usedParts, AssignMap& assigns,
                                         std::vector<AssignedCandidateLog>& assignedLogs,
                                         FamilyLogStats& stats)
  {
    for (const Candidate* candidate : sortedCandidates) {
      if (!candidate)
        continue;
      if (!TryAssignCandidate(*candidate, usedParts, assigns, &assignedLogs))
        ++stats.blocked;
    }
  }

  void AssignPenetrationCandidates(
      const std::vector<Candidate>& allCandidates,
      const std::unordered_map<RE::Actor*, PenetrationMemory>& penetrationMemory,
      UsedPartSet& usedParts, AssignMap& assigns)
  {
    FamilyLogStats stats;
    std::vector<AssignedCandidateLog> assignedLogs;
    std::vector<const Candidate*> nonGenitalCandidates;
    std::unordered_map<RE::Actor*, std::vector<const Candidate*>> vaginalByReceiver;
    std::unordered_map<RE::Actor*, std::vector<const Candidate*>> analByReceiver;

    for (const Candidate& candidate : allCandidates) {
      if (candidate.family != CandidateFamily::Penetration)
        continue;

      ++stats.total;

      if (!IsGenitalPenetrationType(candidate.type)) {
        nonGenitalCandidates.push_back(&candidate);
        continue;
      }

      if (candidate.type == Type::Vaginal)
        vaginalByReceiver[candidate.targetActor].push_back(&candidate);
      else if (candidate.type == Type::Anal)
        analByReceiver[candidate.targetActor].push_back(&candidate);
    }

    std::sort(nonGenitalCandidates.begin(), nonGenitalCandidates.end(), CandidateLess);
    stats.considered = stats.total;
    AssignSortedPenetrationCandidates(nonGenitalCandidates, usedParts, assigns, assignedLogs,
                                      stats);

    std::unordered_set<RE::Actor*> receivers;
    receivers.reserve(vaginalByReceiver.size() + analByReceiver.size());
    for (const auto& [receiver, _] : vaginalByReceiver)
      receivers.insert(receiver);
    for (const auto& [receiver, _] : analByReceiver)
      receivers.insert(receiver);

    std::vector<std::pair<RE::Actor*, std::vector<GenitalPlan>>> receiverPlans;
    receiverPlans.reserve(receivers.size());
    for (RE::Actor* receiver : receivers) {
      const auto vaginalIt = vaginalByReceiver.find(receiver);
      const auto analIt    = analByReceiver.find(receiver);
      const std::vector<const Candidate*> emptyCandidates;
      const auto& vaginalCandidates =
          vaginalIt != vaginalByReceiver.end() ? vaginalIt->second : emptyCandidates;
      const auto& analCandidates =
          analIt != analByReceiver.end() ? analIt->second : emptyCandidates;

      auto plans =
          BuildReceiverGenitalPlans(receiver, vaginalCandidates, analCandidates, penetrationMemory);
      if (!plans.empty())
        receiverPlans.emplace_back(receiver, std::move(plans));
    }

    std::sort(receiverPlans.begin(), receiverPlans.end(), [](const auto& lhs, const auto& rhs) {
      const std::uint8_t lhsBest = lhs.second.empty() ? 0 : lhs.second.front().filledSlots;
      const std::uint8_t rhsBest = rhs.second.empty() ? 0 : rhs.second.front().filledSlots;
      if (lhsBest != rhsBest)
        return lhsBest > rhsBest;
      return lhs.first < rhs.first;
    });

    std::vector<int> remainingCapacity(receiverPlans.size() + 1, 0);
    for (std::size_t index = receiverPlans.size(); index-- > 0;) {
      const int bestReceiverSlots =
          receiverPlans[index].second.empty()
              ? 0
              : static_cast<int>(receiverPlans[index].second.front().filledSlots);
      remainingCapacity[index] = remainingCapacity[index + 1] + bestReceiverSlots;
    }

    GenitalSearchResult best;
    std::vector<GenitalPlan> currentPlans;
    SearchReceiverGenitalPlans(0, receiverPlans, remainingCapacity, usedParts, currentPlans, 0, 0.f,
                               best);

    for (const GenitalPlan& plan : best.plans) {
      if (plan.vaginal && !TryAssignCandidate(*plan.vaginal, usedParts, assigns, &assignedLogs))
        ++stats.blocked;
      if (plan.anal && !TryAssignCandidate(*plan.anal, usedParts, assigns, &assignedLogs))
        ++stats.blocked;
    }

    LogFamilyOutcome(CandidateFamily::Penetration, stats, assignedLogs);
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

  void UpdateGenitalSlotMemory(const std::unordered_map<RE::Actor*, ActorState>& actorStates,
                               const AssignMap& assigns,
                               std::unordered_map<RE::Actor*, PenetrationMemory>& penetrationMemory)
  {
    auto updateSlot = [](RE::Actor*& partnerOut, std::uint8_t& continuityOut,
                         const AssignInfo* assign, bool (*includeType)(Type)) {
      if (assign && includeType(assign->type)) {
        if (partnerOut == assign->partner)
          continuityOut =
              static_cast<std::uint8_t>((std::min)(std::uint32_t{8}, continuityOut + 1u));
        else {
          partnerOut    = assign->partner;
          continuityOut = 1;
        }
      } else {
        partnerOut    = nullptr;
        continuityOut = 0;
      }
    };

    for (const auto& [actor, _] : actorStates) {
      PenetrationMemory& memory = penetrationMemory[actor];
      updateSlot(memory.vaginal.partner, memory.vaginal.continuity,
                 FindAssignInfo(assigns, actor, Name::Vagina), IncludesVaginal);
      updateSlot(memory.anal.partner, memory.anal.continuity,
                 FindAssignInfo(assigns, actor, Name::Anus), IncludesAnal);
    }
  }

  bool TryAssignCandidate(const Candidate& candidate, UsedPartSet& usedParts, AssignMap& assigns,
                          std::vector<AssignedCandidateLog>* assignedLogs)
  {
    const bool sourceUsed = usedParts.count({candidate.sourceActor, candidate.sourcePart}) > 0;
    const bool targetUsed = usedParts.count({candidate.targetActor, candidate.targetPart}) > 0;
    if (sourceUsed || targetUsed)
      return false;

    const Type resolvedType = ResolveSelfType(candidate);

    assigns[candidate.sourceActor][candidate.sourcePart] = {
        candidate.targetActor, candidate.sourcePart, candidate.targetPart, candidate.distance,
        resolvedType};
    usedParts.insert({candidate.sourceActor, candidate.sourcePart});

    assigns[candidate.targetActor][candidate.targetPart] = {
        candidate.sourceActor, candidate.targetPart, candidate.sourcePart, candidate.distance,
        resolvedType};
    usedParts.insert({candidate.targetActor, candidate.targetPart});

    if (assignedLogs)
      assignedLogs->push_back(AssignedCandidateLog{candidate, resolvedType});

    return true;
  }

  void AssignFamilyCandidates(const std::vector<Candidate>& allCandidates, CandidateFamily family,
                              const std::unordered_map<RE::Actor*, ActorState>& actorStates,
                              UsedPartSet& usedParts, AssignMap& assigns)
  {
    FamilyLogStats stats;
    std::vector<AssignedCandidateLog> assignedLogs;
    std::unordered_map<RE::Actor*, ActorInteractSummary> summaries;
    if (family == CandidateFamily::Proximity)
      BuildSummaries(assigns, summaries);

    std::vector<Candidate> familyCandidates;
    familyCandidates.reserve(allCandidates.size());
    for (const Candidate& candidate : allCandidates) {
      if (candidate.family != family)
        continue;

      ++stats.total;

      Candidate adjusted = candidate;
      if (family == CandidateFamily::Proximity) {
        if (ShouldSuppressProximityCandidate(adjusted, summaries)) {
          ++stats.suppressed;
          continue;
        }

        if (ShouldSuppressRecentPenetrationFallback(adjusted, actorStates)) {
          ++stats.suppressed;
          continue;
        }

        adjusted.score += GetProximityPenalty(adjusted, summaries, actorStates);
      }

      familyCandidates.push_back(adjusted);
    }

    stats.considered = familyCandidates.size();

    std::sort(familyCandidates.begin(), familyCandidates.end(),
              [](const Candidate& lhs, const Candidate& rhs) {
                if (lhs.score != rhs.score)
                  return lhs.score < rhs.score;
                if (lhs.distance != rhs.distance)
                  return lhs.distance < rhs.distance;
                return static_cast<std::uint8_t>(lhs.type) < static_cast<std::uint8_t>(rhs.type);
              });

    for (const Candidate& candidate : familyCandidates) {
      if (!TryAssignCandidate(candidate, usedParts, assigns, &assignedLogs))
        ++stats.blocked;
    }

    LogFamilyOutcome(family, stats, assignedLogs);
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
                        const AssignMap& assigns, float nowMs)
  {
    for (auto& [actor, actorState] : actorStates) {
      const float dt              = nowMs - actorState.lastEvaluationMs;
      actorState.lastEvaluationMs = nowMs;

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
            dt > 1e-4f ? (assign.distance - partState.previous.distance) / dt : 0.f;
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

void UpdateInteractState(
    std::unordered_map<RE::Actor*, Interact::ActorState>& actorStates,
    std::unordered_map<RE::Actor*, Interact::PenetrationMemory>& penetrationMemory,
    std::unordered_map<RE::Actor*, Define::InteractTags>& observedInteractTags)
{
  ResetInteractionInfos(actorStates);
  UpdateBodyPartPositions(actorStates);

  const float nowMs = GetSteadyNowMs();
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

  AssignPenetrationCandidates(candidates, penetrationMemory, usedParts, assigns);
  AssignFamilyCandidates(candidates, CandidateFamily::Proximity, actorStates, usedParts, assigns);
  AssignFamilyCandidates(candidates, CandidateFamily::Contact, actorStates, usedParts, assigns);

  std::unordered_map<RE::Actor*, ActorInteractSummary> summaries;
  BuildSummaries(assigns, summaries);

  std::unordered_map<RE::Actor*, std::vector<Type>> comboTypes;
  BuildComboTypes(summaries, comboTypes);
  AccumulateObservedInteractTags(assigns, comboTypes, observedInteractTags);
  ApplyComboOverrides(assigns, summaries, comboTypes);
  ApplyAssignments(actorStates, assigns, nowMs);
  UpdateGenitalSlotMemory(actorStates, assigns, penetrationMemory);
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

void Interact::Update()
{
  if (penetrationMemory.empty()) {
    for (const auto& [actor, _] : actorStates) {
      penetrationMemory.emplace(actor, PenetrationMemory{});
      observedInteractTags.try_emplace(actor, Define::InteractTags{});
    }
  }

  Detail::UpdateInteractState(actorStates, penetrationMemory, observedInteractTags);
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

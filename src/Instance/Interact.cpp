#include "Instance/Interact.h"

#include "Utils/Settings.h"

#include "magic_enum/magic_enum.hpp"

#include <numbers>

namespace Instance::Detail
{

#pragma region RuntimeInternals

using Name              = Define::BodyPart::Name;
using BP                = Define::BodyPart;
using Type              = Interact::Type;
using Info              = Interact::Info;
using ActorData         = Interact::ActorData;
using GenitalSlotMemory = Interact::GenitalSlotMemory;
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

  enum class DebugEvent : std::uint8_t
  {
    Candidate = 0,
    Reject,
    Suppressed,
    AssignSkipped,
    Assigned,
    FinalState,
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
    const ActorData& dataA;
    const ActorData& dataB;
    bool self = false;
  };

  struct DirectedContext
  {
    RE::Actor* sourceActor = nullptr;
    RE::Actor* targetActor = nullptr;
    const ActorData& sourceData;
    const ActorData& targetData;
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

  struct DebugKey
  {
    RE::Actor* sourceActor = nullptr;
    RE::Actor* targetActor = nullptr;
    Type type              = Type::None;
    DebugEvent event       = DebugEvent::Candidate;

    bool operator==(const DebugKey&) const = default;
  };

  struct DebugKeyHash
  {
    std::size_t operator()(const DebugKey& key) const
    {
      std::size_t value = std::hash<RE::Actor*>{}(key.sourceActor);
      value ^= std::hash<RE::Actor*>{}(key.targetActor) + 0x9E3779B97F4A7C15ull + (value << 6) +
               (value >> 2);
      value ^= static_cast<std::size_t>(key.type) << 8;
      value ^= static_cast<std::size_t>(key.event) << 16;
      return value;
    }
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
  static constexpr std::array<Name, 1> kCleavageParts{Name::Cleavage};
  static constexpr std::array<Name, 1> kBellyParts{Name::Belly};
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

  bool ShouldEmitDebug(RE::Actor* sourceActor, RE::Actor* targetActor, Type type, DebugEvent event,
                       float minIntervalMs = 350.f)
  {
    static std::unordered_map<DebugKey, float, DebugKeyHash> s_lastLogTimes;

    if (!IsDebugEnabled())
      return false;

    const float nowMs = GetSteadyNowMs();
    const DebugKey key{sourceActor, targetActor, type, event};
    auto [it, inserted] = s_lastLogTimes.try_emplace(key, nowMs);
    if (inserted || nowMs - it->second >= minIntervalMs) {
      it->second = nowMs;
      return true;
    }

    return false;
  }

  const BP* TryGetBodyPart(const ActorData& data, Name name)
  {
    auto it = data.infos.find(name);
    if (it == data.infos.end() || !it->second.bodypart.IsValid())
      return nullptr;
    return &it->second.bodypart;
  }

  const Info* TryGetInfo(const ActorData& data, Name name)
  {
    auto it = data.infos.find(name);
    return it == data.infos.end() ? nullptr : &it->second;
  }

  const MotionHistory* TryGetMotionHistory(const ActorData& data, Name name)
  {
    auto it = data.motion.find(name);
    return it == data.motion.end() ? nullptr : &it->second;
  }

  const MotionSnapshot* TryGetCurrentSnapshot(const ActorData& data, Name name)
  {
    const MotionHistory* history = TryGetMotionHistory(data, name);
    if (!history || !history->current.valid)
      return nullptr;
    return &history->current;
  }

  template <std::size_t N>
  bool HasAnyCurrentPart(const ActorData& data, const std::array<Name, N>& names)
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
    snapshot.start       = bodypart.GetStart();
    snapshot.end         = bodypart.GetEnd();
    snapshot.direction   = bodypart.GetDirection();
    snapshot.length      = bodypart.GetLength();
    snapshot.timeMs      = nowMs;
    snapshot.valid       = bodypart.IsValid();
    snapshot.directional = bodypart.GetShape() != BP::Shape::Point && snapshot.length >= 1e-6f;
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

  float TemporalBonus(const ActorData& data, Name part, RE::Actor* partner, Type type)
  {
    const Info* info = TryGetInfo(data, part);
    if (!info)
      return 0.f;
    if (info->prevActor == partner && info->prevType == type)
      return 0.12f;
    if (info->prevActor == partner)
      return 0.06f;
    if (info->prevType == type)
      return 0.03f;
    return 0.f;
  }

  bool HadPreviousInteraction(const ActorData& data, Name part, RE::Actor* partner, Type type)
  {
    const Info* info = TryGetInfo(data, part);
    return info && info->prevActor == partner && info->prevType == type;
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

  bool HasPreviousPartContext(const ActorData& data, Name part, bool (*predicate)(Type))
  {
    const Info* info = TryGetInfo(data, part);
    return info && predicate(info->prevType);
  }

  bool HasPreviousVaginalContext(const ActorData& data)
  {
    return HasPreviousPartContext(data, Name::Vagina, IncludesVaginal);
  }

  bool HasPreviousAnalContext(const ActorData& data)
  {
    return HasPreviousPartContext(data, Name::Anus, IncludesAnal);
  }

  bool HadPreviousPenetrationWithPartner(const ActorData& data, Name part, RE::Actor* partner)
  {
    const Info* info = TryGetInfo(data, part);
    if (!info || info->prevActor != partner)
      return false;

    return IncludesVaginal(info->prevType) || IncludesAnal(info->prevType);
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

  float MeasurePartDistanceByName(const ActorData& data, Name lhs, const MotionSnapshot& rhs)
  {
    const MotionSnapshot* lhsPart = TryGetCurrentSnapshot(data, lhs);
    return lhsPart ? Geometry::MeasurePartDistance(*lhsPart, rhs)
                   : std::numeric_limits<float>::infinity();
  }

  PenisContextMetrics BuildPenisContextMetrics(const ActorData& receiverData,
                                               const MotionSnapshot& penisSnapshot)
  {
    PenisContextMetrics metrics;
    metrics.vaginalSupport =
        MeasurePartDistanceByName(receiverData, Name::ThighCleft, penisSnapshot);
    metrics.analSupport =
        MeasurePartDistanceByName(receiverData, Name::GlutealCleft, penisSnapshot);

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

    depth                          = useMid ? metrics.midDepth : metrics.tipDepth;
    const bool shaftInsideReceiver = metrics.overlapDepth > 0.05f;
    const bool nearEntryHold = stickyPair && Geometry::SampledShaftEntryDistance(metrics) <= 2.0f &&
                               axisDistance <= maxAxis + 0.6f;
    if (!shaftInsideReceiver && !nearEntryHold)
      return false;

    if (depth < minDepth || depth > maxDepth) {
      if (midDepthInRange && metrics.midAxisDistance <= maxAxis) {
        axisDistance = metrics.midAxisDistance;
        depth        = metrics.midDepth;
      } else if (tipDepthInRange && metrics.tipAxisDistance <= maxAxis) {
        axisDistance = metrics.tipAxisDistance;
        depth        = metrics.tipDepth;
      } else if (nearEntryHold) {
        depth = std::clamp(depth, minDepth + 0.05f, maxDepth - 0.05f);
      } else {
        return false;
      }
    }

    return depth >= minDepth && depth <= maxDepth;
  }

  MotionSnapshot BuildOralChannelSnapshot(const ActorData& data, Name receiverPart,
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

  template <std::size_t NS, std::size_t NT>
  void AddSimpleDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx,
                                   const std::array<Name, NS>& sourceParts,
                                   const std::array<Name, NT>& targetParts, Type type,
                                   CandidateFamily family, float maxDistance, AngleRange angleRange,
                                   float angleWeight, float scoreOffset = 0.f,
                                   AnchorMode anchorMode = AnchorMode::None,
                                   float anchorWeight = 0.f, float anchorMaxDistance = 0.f)
  {
    for (Name sourceName : sourceParts) {
      const MotionSnapshot* sourcePart = TryGetCurrentSnapshot(ctx.sourceData, sourceName);
      if (!sourcePart)
        continue;

      for (Name targetName : targetParts) {
        if (ctx.self && sourceName == targetName)
          continue;

        const MotionSnapshot* targetPart = TryGetCurrentSnapshot(ctx.targetData, targetName);
        if (!targetPart)
          continue;

        const float distance = Geometry::MeasurePartDistance(*sourcePart, *targetPart);
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

  void TryAddGropeBreastCandidate(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    constexpr float kMaxDistance       = 5.4f;
    constexpr float kMaxNippleDistance = 4.9f;
    constexpr AngleRange kAngleRange{156.f, 180.f};

    for (Name handName : kHandParts) {
      const MotionSnapshot* handPart = TryGetCurrentSnapshot(ctx.sourceData, handName);
      if (!handPart)
        continue;

      for (Name breastName : kBreastParts) {
        const MotionSnapshot* breastPart = TryGetCurrentSnapshot(ctx.targetData, breastName);
        if (!breastPart)
          continue;

        const float distance = Geometry::MeasurePartDistance(*handPart, *breastPart);
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

  MotionSnapshot BuildPredictedOrCurrent(const ActorData& data, Name partName)
  {
    const MotionHistory* history = TryGetMotionHistory(data, partName);
    if (!history)
      return {};
    return PredictSnapshot(*history);
  }

  void TryAddHandjobCandidate(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    const MotionSnapshot* penisCurrent = TryGetCurrentSnapshot(ctx.targetData, Name::Penis);
    if (!penisCurrent)
      return;

    const MotionHistory* penisHistory = TryGetMotionHistory(ctx.targetData, Name::Penis);
    if (!penisHistory)
      return;

    constexpr float kMaxDistance = 8.f;
    constexpr AngleRange kAngleRange{50.f, 130.f};
    constexpr float kAngleWeight             = 0.20f;
    constexpr float kCoverageWeight          = 0.16f;
    constexpr float kSelfFaceGuardDistance   = 6.6f;
    constexpr float kOralInterferencePenalty = 0.45f;
    constexpr float kPartnerOralDistance     = 8.6f;

    for (Name handName : kHandParts) {
      const MotionSnapshot* handCurrent = TryGetCurrentSnapshot(ctx.sourceData, handName);
      if (!handCurrent)
        continue;

      const SampledMetrics currentMetrics =
          Geometry::MeasureSampledMetrics(*penisCurrent, *handCurrent);
      const float distance = Geometry::SampledShaftDistance(currentMetrics);
      if (distance > kMaxDistance)
        continue;

      const float angle = Geometry::MeasurePartAngle(*handCurrent, *penisCurrent);
      if (!CheckAngle(angle, kAngleRange))
        continue;

      float handToOral  = std::numeric_limits<float>::infinity();
      float penisToOral = std::numeric_limits<float>::infinity();
      if (const MotionSnapshot* mouthPart = TryGetCurrentSnapshot(ctx.sourceData, Name::Mouth))
        handToOral =
            (std::min)(handToOral, Geometry::MeasurePartDistance(*handCurrent, *mouthPart));
      if (const MotionSnapshot* throatPart = TryGetCurrentSnapshot(ctx.sourceData, Name::Throat))
        handToOral =
            (std::min)(handToOral, Geometry::MeasurePartDistance(*handCurrent, *throatPart));
      if (const MotionSnapshot* mouthPart = TryGetCurrentSnapshot(ctx.sourceData, Name::Mouth))
        penisToOral =
            (std::min)(penisToOral, Geometry::MeasurePartDistance(*penisCurrent, *mouthPart));
      if (const MotionSnapshot* throatPart = TryGetCurrentSnapshot(ctx.sourceData, Name::Throat))
        penisToOral =
            (std::min)(penisToOral, Geometry::MeasurePartDistance(*penisCurrent, *throatPart));

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

  bool ShouldLogPenetrationReject(float currentValue, float predictedValue, float limit,
                                  float allowance)
  {
    return IsNearThreshold(currentValue, predictedValue, limit, allowance);
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

  void LogPenetrationEvent(std::string_view stage, const DirectedContext& ctx, Name receiverPart,
                           Type type, float distance, float predictedDistance, float angle,
                           float predictedAngle, float axisDistance, float predictedAxisDistance,
                           float depth, float predictedDepth, float score, std::string_view detail,
                           DebugEvent event, float intervalMs = 350.f)
  {
    if (!ShouldEmitDebug(ctx.sourceActor, ctx.targetActor, type, event, intervalMs))
      return;

    logger::info("[SexLab NG] InteractDebug Penetration {} src={:08X}:{} dst={:08X}:{} receiver={} "
                 "dist={:.2f}/{:.2f} angle={:.2f}/{:.2f} axis={:.2f}/{:.2f} depth={:.2f}/{:.2f} "
                 "score={:.3f} detail={}",
                 stage, GetActorFormID(ctx.sourceActor), NameText(Name::Penis),
                 GetActorFormID(ctx.targetActor), NameText(receiverPart), NameText(receiverPart),
                 distance, predictedDistance, angle, predictedAngle, axisDistance,
                 predictedAxisDistance, depth, predictedDepth, score, detail);
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

    float effectiveMaxDistance = maxDistance;
    float effectiveMaxAngle    = maxAngle;
    float effectiveMaxAxis     = maxAxis;
    float effectiveMinDepth    = minDepth;
    float effectiveMaxDepth    = maxDepth;
    float temporalBonus        = 0.f;
    float switchPenalty        = 0.f;

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

    const bool predictedDistanceHold = type == Type::Vaginal && hasPredicted && strongContext &&
                                       predictedDistance <= effectiveMaxDistance + 0.25f &&
                                       predictedDistance + 0.35f < distance &&
                                       distance <= effectiveMaxDistance + 1.8f;

    bool acceptedByPredictedApproach = false;
    if (distance > effectiveMaxDistance) {
      if (predictedDistanceHold) {
        acceptedByPredictedApproach = true;
        temporalBonus += 0.05f;
      } else {
        if (ShouldLogPenetrationReject(distance, predictedDistance, effectiveMaxDistance, 1.2f)) {
          LogPenetrationEvent("reject-distance", ctx, receiverPartName, type, distance,
                              predictedDistance, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -1.f,
                              "distance above gate", DebugEvent::Reject, 900.f);
        }
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
    if (angle > effectiveMaxAngle && !heldByAngleHysteresis && !heldByPredictedAngle) {
      if (ShouldLogPenetrationReject(angle, predictedAngle, effectiveMaxAngle, 10.f)) {
        LogPenetrationEvent("reject-angle", ctx, receiverPartName, type, distance,
                            predictedDistance, angle, predictedAngle, 0.f, 0.f, 0.f, 0.f, -1.f,
                            "angle above gate", DebugEvent::Reject, 900.f);
      }
      return;
    }

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
        const float predictedAxis  = hasPredicted ? predictedMetrics.minAxisDistance : 0.f;
        const float predictedDepth = hasPredicted ? predictedMetrics.tipDepth : 0.f;
        if (ShouldLogPenetrationReject(currentMetrics.minAxisDistance, predictedAxis,
                                       effectiveMaxAxis, 1.2f)) {
          LogPenetrationEvent("reject-shaft", ctx, receiverPartName, type, distance,
                              predictedDistance, angle, predictedAngle,
                              currentMetrics.minAxisDistance, predictedAxis,
                              currentMetrics.tipDepth, predictedDepth, -1.f,
                              "axis/depth outside gate", DebugEvent::Reject, 900.f);
        }
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
      LogPenetrationEvent("reject-switch", ctx, receiverPartName, type, distance, predictedDistance,
                          angle, predictedAngle, axisDistance, predictedAxisDistance, depth,
                          predictedDepth, -1.f, "waiting for sustained opposite-type trend",
                          DebugEvent::Reject, 900.f);
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
    const char* candidateDetail          = "accepted";
    if (usedPredictedContact)
      candidateDetail = "accepted predicted-approach";
    else if (heldByAngleHysteresis && previousOppositeType && !strongContext)
      candidateDetail = "accepted angle-hold opposite-switch";
    else if (heldByAngleHysteresis)
      candidateDetail = "accepted angle-hold";
    else if (previousOppositeType && !strongContext)
      candidateDetail = "opposite-type-switch";

    const float score =
        std::clamp(distNorm * 0.42f + angleNorm * 0.22f + axisNorm * 0.22f +
                       std::clamp(depthPenalty, 0.f, 1.f) * 0.14f + contextBias + switchPenalty +
                       predictedApproachPenalty - bonus - temporalBonus - predictionBonus - 0.10f,
                   -1.20f, 2.50f);

    LogPenetrationEvent("candidate", ctx, receiverPartName, type, distance, predictedDistance,
                        angle, predictedAngle, axisDistance, predictedAxisDistance, depth,
                        predictedDepth, score, candidateDetail, DebugEvent::Candidate, 320.f);

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

    if (distance > effectiveMaxDistance) {
      if (ShouldLogPenetrationReject(distance, predictedDistance, effectiveMaxDistance, 1.0f)) {
        LogPenetrationEvent("reject-distance", ctx, receiverPartName, type, distance,
                            predictedDistance, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -1.f,
                            "oral distance above gate", DebugEvent::Reject, 900.f);
      }
      return;
    }

    const float angle = Geometry::MeasurePartAngle(receiverCurrent, penisHistory->current);
    const float predictedAngle =
        hasPredicted ? Geometry::MeasurePartAngle(receiverPredicted, predictedPenis) : angle;
    if (angle > effectiveMaxAngle) {
      if (ShouldLogPenetrationReject(angle, predictedAngle, effectiveMaxAngle, 8.f)) {
        LogPenetrationEvent("reject-angle", ctx, receiverPartName, type, distance,
                            predictedDistance, angle, predictedAngle, 0.f, 0.f, 0.f, 0.f, -1.f,
                            "oral angle above gate", DebugEvent::Reject, 900.f);
      }
      return;
    }

    float axisDistance = 0.f;
    float depth        = 0.f;
    if (!ResolvePenetrationContact(currentMetrics, effectiveMaxAxis, effectiveMinDepth,
                                   effectiveMaxDepth, stickyPair || previousRelated, axisDistance,
                                   depth)) {
      const float predictedAxis =
          hasPredicted
              ? Geometry::MeasureSampledMetrics(predictedPenis, receiverPredicted).minAxisDistance
              : 0.f;
      if (ShouldLogPenetrationReject(currentMetrics.minAxisDistance, predictedAxis,
                                     effectiveMaxAxis, 1.0f)) {
        LogPenetrationEvent("reject-shaft", ctx, receiverPartName, type, distance,
                            predictedDistance, angle, predictedAngle,
                            currentMetrics.minAxisDistance, predictedAxis, currentMetrics.tipDepth,
                            depth, -1.f, "oral axis/depth outside gate", DebugEvent::Reject, 900.f);
      }
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

    LogPenetrationEvent("candidate", ctx, receiverPartName, type, distance, predictedDistance,
                        angle, predictedAngle, axisDistance, axisDistance, depth, depth, score,
                        "accepted", DebugEvent::Candidate, 320.f);

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
    const float distance = Geometry::SampledShaftDistance(currentMetrics);
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

    if (ShouldEmitDebug(ctx.sourceActor, ctx.targetActor, type, DebugEvent::Candidate, 380.f)) {
      logger::info(
          "[SexLab NG] InteractDebug Proximity candidate type={} src={:08X}:{} dst={:08X}:{} "
          "dist={:.2f} root/mid/tip=({:.2f},{:.2f},{:.2f}) angle={:.2f} score={:.3f}",
          TypeText(type), GetActorFormID(ctx.sourceActor), NameText(Name::Penis),
          GetActorFormID(ctx.targetActor), NameText(targetName), distance,
          currentMetrics.rootDistance, currentMetrics.midDistance, currentMetrics.tipDistance,
          angle, score);
    }

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
    const float distance = Geometry::SampledShaftDistance(currentMetrics);
    if (distance > 10.5f)
      return;

    const float angle = Geometry::MeasurePartAngle(*belly, penisHistory->current);
    if (angle > 60.f)
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
                             10.f,
                         0.f, 0.05f)
            : 0.f;

    const float distNorm     = NormalizeDistance(distance, 10.5f);
    const float angleNorm    = AngleDeviation(angle, {0.f, 60.f});
    const float coverageNorm = NormalizeDistance(
        (currentMetrics.rootDistance + currentMetrics.midDistance + currentMetrics.tipDistance) /
            3.f,
        10.5f);
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

    const float distance = Geometry::MeasurePartDistance(*lhs, *rhs);
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

    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kBreastParts, Type::BreastSucking,
                                CandidateFamily::Contact, 8.f, {0.f, 180.f}, 0.f);
    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kFootParts, Type::ToeSucking,
                                CandidateFamily::Contact, 8.f, {0.f, 180.f}, 0.f);
    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kThighCleftParts, Type::Cunnilingus,
                                CandidateFamily::Contact, 7.2f, {0.f, 180.f}, 0.f, -0.05f,
                                AnchorMode::StartToStart, 0.18f, 7.2f);
    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kVaginaParts, Type::Cunnilingus,
                                CandidateFamily::Contact, 7.7f, {0.f, 180.f}, 0.f, 0.01f,
                                AnchorMode::StartToStart, 0.22f, 7.4f);
    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kAnusParts, Type::Anilingus,
                                CandidateFamily::Contact, 7.1f, {0.f, 180.f}, 0.f, 0.f,
                                AnchorMode::StartToStart, 0.20f, 6.8f);

    TryAddOralPenetrationCandidate(out, ctx, Name::Mouth, Type::Fellatio, 9.4f, 76.f, 6.6f, -2.5f,
                                   10.5f);
    TryAddOralPenetrationCandidate(out, ctx, Name::Throat, Type::DeepThroat, 7.4f, 46.f, 4.8f, 1.0f,
                                   13.2f);
  }

  void GenerateManualCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (!HasAnyCurrentPart(ctx.sourceData, kHandParts) &&
        !HasAnyCurrentPart(ctx.sourceData, kFingerParts))
      return;

    TryAddGropeBreastCandidate(out, ctx);
    AddSimpleDirectedCandidates(out, ctx, kHandParts, kThighParts, Type::GropeThigh,
                                CandidateFamily::Contact, 7.1f, {0.f, 180.f}, 0.f, 0.f,
                                AnchorMode::StartToStart, 0.14f, 6.4f);
    AddSimpleDirectedCandidates(out, ctx, kHandParts, kButtParts, Type::GropeButt,
                                CandidateFamily::Contact, 5.2f, {0.f, 180.f}, 0.f, 0.f,
                                AnchorMode::StartToStart, 0.16f, 4.8f);
    AddSimpleDirectedCandidates(out, ctx, kHandParts, kFootParts, Type::GropeFoot,
                                CandidateFamily::Contact, 7.0f, {0.f, 180.f}, 0.f, 0.f,
                                AnchorMode::StartToStart, 0.12f, 6.4f);

    AddSimpleDirectedCandidates(out, ctx, kFingerParts, kThighCleftParts, Type::FingerVagina,
                                CandidateFamily::Contact, 6.4f, {0.f, 180.f}, 0.f, -0.04f,
                                AnchorMode::StartToStart, 0.18f, 6.0f);
    AddSimpleDirectedCandidates(out, ctx, kFingerParts, kVaginaParts, Type::FingerVagina,
                                CandidateFamily::Contact, 7.2f, {0.f, 180.f}, 0.f, 0.01f,
                                AnchorMode::StartToStart, 0.22f, 6.6f);
    AddSimpleDirectedCandidates(out, ctx, kFingerParts, kAnusParts, Type::FingerAnus,
                                CandidateFamily::Contact, 6.8f, {0.f, 180.f}, 0.f, 0.f,
                                AnchorMode::StartToStart, 0.20f, 6.2f);

    TryAddHandjobCandidate(out, ctx);
  }

  void GenerateProximityCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (!HasAnyCurrentPart(ctx.sourceData, kPenisParts))
      return;

    TryAddSampledPenisProximityCandidate(out, ctx, Name::BreastLeft, Type::Titfuck, 8.f,
                                         {0.f, 58.f}, 0.32f);
    TryAddSampledPenisProximityCandidate(out, ctx, Name::BreastRight, Type::Titfuck, 8.f,
                                         {0.f, 58.f}, 0.32f);
    TryAddSampledPenisProximityCandidate(out, ctx, Name::Cleavage, Type::Titfuck, 8.f,
                                         {70.f, 110.f}, 0.40f, -0.03f);
    TryAddNaveljobCandidate(out, ctx);
    TryAddSampledPenisProximityCandidate(out, ctx, Name::ThighCleft, Type::Thighjob, 6.5f,
                                         {0.f, 32.f}, 0.24f, 0.02f);
    TryAddSampledPenisProximityCandidate(out, ctx, Name::ThighLeft, Type::Thighjob, 5.4f,
                                         {0.f, 32.f}, 0.22f, 0.01f);
    TryAddSampledPenisProximityCandidate(out, ctx, Name::ThighRight, Type::Thighjob, 5.4f,
                                         {0.f, 32.f}, 0.22f, 0.01f);
    TryAddSampledPenisProximityCandidate(out, ctx, Name::GlutealCleft, Type::Frottage, 8.f,
                                         {140.f, 40.f, AngleModel::Wrap}, 0.28f);
    TryAddSampledPenisProximityCandidate(out, ctx, Name::FootLeft, Type::Footjob, 8.2f,
                                         {0.f, 180.f}, 0.f);
    TryAddSampledPenisProximityCandidate(out, ctx, Name::FootRight, Type::Footjob, 8.2f,
                                         {0.f, 180.f}, 0.f);
  }

  void GeneratePenetrationCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (ctx.self || !HasAnyCurrentPart(ctx.sourceData, kPenisParts))
      return;

    TryAddGenitalPenetrationCandidate(out, ctx, Name::Vagina, Type::Vaginal, 8.4f, 30.f, 5.5f,
                                      -3.5f, 12.f);
    TryAddGenitalPenetrationCandidate(out, ctx, Name::Anus, Type::Anal, 6.6f, 20.f, 4.5f, -2.5f,
                                      9.8f);
  }

  void GenerateSymmetricCandidates(std::vector<Candidate>& out, const PairContext& pair)
  {
    TryAddSymmetricCandidate(out, pair, Name::Mouth, Name::Mouth, Type::Kiss,
                             CandidateFamily::Contact, 6.8f, {140.f, 180.f}, 0.48f);
    TryAddSymmetricCandidate(out, pair, Name::Vagina, Name::Vagina, Type::Tribbing,
                             CandidateFamily::Contact, 8.f, {140.f, 180.f}, 0.32f);
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
      const Candidate& candidate, const std::unordered_map<RE::Actor*, ActorData>& datas,
      bool* sourceRecentOut = nullptr, bool* targetRecentOut = nullptr)
  {
    if (sourceRecentOut)
      *sourceRecentOut = false;
    if (targetRecentOut)
      *targetRecentOut = false;

    const auto sourceIt = datas.find(candidate.sourceActor);
    const auto targetIt = datas.find(candidate.targetActor);
    if (sourceIt == datas.end() || targetIt == datas.end())
      return false;

    const bool sourceRecent =
        HadPreviousPenetrationWithPartner(sourceIt->second, Name::Penis, candidate.targetActor);
    const bool targetRecent =
        HadPreviousPenetrationWithPartner(targetIt->second, Name::Vagina, candidate.sourceActor) ||
        HadPreviousPenetrationWithPartner(targetIt->second, Name::Anus, candidate.sourceActor);
    if (sourceRecentOut)
      *sourceRecentOut = sourceRecent;
    if (targetRecentOut)
      *targetRecentOut = targetRecent;
    if (!sourceRecent && !targetRecent)
      return false;

    return TryGetCurrentSnapshot(targetIt->second, Name::Vagina) ||
           TryGetCurrentSnapshot(targetIt->second, Name::Anus);
  }

  float GetProximityPenalty(const Candidate& candidate,
                            const std::unordered_map<RE::Actor*, ActorInteractSummary>& summaries,
                            const std::unordered_map<RE::Actor*, ActorData>& datas)
  {
    const auto summaryIt      = summaries.find(candidate.targetActor);
    const bool currentVaginal = summaryIt != summaries.end() && summaryIt->second.hasVaginal &&
                                summaryIt->second.vaginalPartner == candidate.sourceActor;
    const bool currentAnal    = summaryIt != summaries.end() && summaryIt->second.hasAnal &&
                                summaryIt->second.analPartner == candidate.sourceActor;

    const auto dataIt = datas.find(candidate.targetActor);
    const bool prevVaginal =
        dataIt != datas.end() &&
        HadPreviousPenetrationWithPartner(dataIt->second, Name::Vagina, candidate.sourceActor);
    const bool prevAnal =
        dataIt != datas.end() &&
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

  bool TryAssignCandidate(const Candidate& candidate, UsedPartSet& usedParts, AssignMap& assigns);

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
      const std::unordered_map<RE::Actor*, GenitalSlotMemory>& genitalSlotMemory)
  {
    auto it = genitalSlotMemory.find(receiver);
    if (it == genitalSlotMemory.end())
      return 0.f;

    const GenitalSlotMemory& memory = it->second;
    const RE::Actor* incumbent =
        slot == GenitalSlotKind::Vaginal ? memory.vaginalPartner : memory.analPartner;
    const std::uint8_t continuity =
        slot == GenitalSlotKind::Vaginal ? memory.vaginalContinuity : memory.analContinuity;

    if (!incumbent)
      return 0.f;
    if (incumbent == challenger)
      return -0.18f - 0.04f * static_cast<float>((std::min)(continuity, std::uint8_t{4}));

    return 0.10f + 0.03f * static_cast<float>((std::min)(continuity, std::uint8_t{4}));
  }

  float ComputeGenitalPlanScore(
      RE::Actor* receiver, const Candidate* vaginal, const Candidate* anal,
      const std::unordered_map<RE::Actor*, GenitalSlotMemory>& genitalSlotMemory)
  {
    float score = 0.f;

    if (vaginal) {
      score += vaginal->score;
      score += GetGenitalSlotContinuityAdjustment(receiver, GenitalSlotKind::Vaginal,
                                                  vaginal->sourceActor, genitalSlotMemory);
    }

    if (anal) {
      score += anal->score;
      score += GetGenitalSlotContinuityAdjustment(receiver, GenitalSlotKind::Anal,
                                                  anal->sourceActor, genitalSlotMemory);
    }

    return score;
  }

  std::vector<GenitalPlan> BuildReceiverGenitalPlans(
      RE::Actor* receiver, const std::vector<const Candidate*>& vaginalCandidates,
      const std::vector<const Candidate*>& analCandidates,
      const std::unordered_map<RE::Actor*, GenitalSlotMemory>& genitalSlotMemory)
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
                      ComputeGenitalPlanScore(receiver, vaginal, nullptr, genitalSlotMemory), 1});
    }

    for (const Candidate* anal : sortedAnal) {
      plans.push_back(
          GenitalPlan{receiver, nullptr, anal,
                      ComputeGenitalPlanScore(receiver, nullptr, anal, genitalSlotMemory), 1});
    }

    for (const Candidate* vaginal : sortedVaginal) {
      for (const Candidate* anal : sortedAnal) {
        if (vaginal->sourceActor == anal->sourceActor && vaginal->sourcePart == anal->sourcePart)
          continue;

        plans.push_back(
            GenitalPlan{receiver, vaginal, anal,
                        ComputeGenitalPlanScore(receiver, vaginal, anal, genitalSlotMemory), 2});
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
                                         UsedPartSet& usedParts, AssignMap& assigns)
  {
    for (const Candidate* candidate : sortedCandidates) {
      if (!candidate)
        continue;
      TryAssignCandidate(*candidate, usedParts, assigns);
    }
  }

  void AssignPenetrationCandidates(
      const std::vector<Candidate>& allCandidates,
      const std::unordered_map<RE::Actor*, GenitalSlotMemory>& genitalSlotMemory,
      UsedPartSet& usedParts, AssignMap& assigns)
  {
    std::vector<const Candidate*> nonGenitalCandidates;
    std::unordered_map<RE::Actor*, std::vector<const Candidate*>> vaginalByReceiver;
    std::unordered_map<RE::Actor*, std::vector<const Candidate*>> analByReceiver;

    for (const Candidate& candidate : allCandidates) {
      if (candidate.family != CandidateFamily::Penetration)
        continue;

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
    AssignSortedPenetrationCandidates(nonGenitalCandidates, usedParts, assigns);

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
          BuildReceiverGenitalPlans(receiver, vaginalCandidates, analCandidates, genitalSlotMemory);
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
      if (plan.vaginal)
        TryAssignCandidate(*plan.vaginal, usedParts, assigns);
      if (plan.anal)
        TryAssignCandidate(*plan.anal, usedParts, assigns);
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

  void UpdateGenitalSlotMemory(const std::unordered_map<RE::Actor*, ActorData>& datas,
                               const AssignMap& assigns,
                               std::unordered_map<RE::Actor*, GenitalSlotMemory>& genitalSlotMemory)
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

    for (const auto& [actor, _] : datas) {
      GenitalSlotMemory& memory = genitalSlotMemory[actor];
      updateSlot(memory.vaginalPartner, memory.vaginalContinuity,
                 FindAssignInfo(assigns, actor, Name::Vagina), IncludesVaginal);
      updateSlot(memory.analPartner, memory.analContinuity,
                 FindAssignInfo(assigns, actor, Name::Anus), IncludesAnal);
    }
  }

  bool TryAssignCandidate(const Candidate& candidate, UsedPartSet& usedParts, AssignMap& assigns)
  {
    const bool sourceUsed = usedParts.count({candidate.sourceActor, candidate.sourcePart}) > 0;
    const bool targetUsed = usedParts.count({candidate.targetActor, candidate.targetPart}) > 0;
    if (sourceUsed || targetUsed) {
      const AssignInfo* sourceAssign =
          FindAssignInfo(assigns, candidate.sourceActor, candidate.sourcePart);
      const AssignInfo* targetAssign =
          FindAssignInfo(assigns, candidate.targetActor, candidate.targetPart);

      if ((candidate.family == CandidateFamily::Penetration ||
           candidate.family == CandidateFamily::Proximity) &&
          ShouldEmitDebug(candidate.sourceActor, candidate.targetActor, candidate.type,
                          DebugEvent::AssignSkipped, 260.f)) {
        logger::info(
            "[SexLab NG] InteractDebug Assign skipped family={} type={} src={:08X}:{} "
            "dst={:08X}:{} "
            "score={:.3f} distance={:.2f} sourceUsed={} sourceBy={} targetUsed={} targetBy={}",
            FamilyText(candidate.family), TypeText(candidate.type),
            GetActorFormID(candidate.sourceActor), NameText(candidate.sourcePart),
            GetActorFormID(candidate.targetActor), NameText(candidate.targetPart), candidate.score,
            candidate.distance, sourceUsed,
            TypeText(sourceAssign ? sourceAssign->type : Type::None), targetUsed,
            TypeText(targetAssign ? targetAssign->type : Type::None));
      }
      return false;
    }

    const Type resolvedType = ResolveSelfType(candidate);

    assigns[candidate.sourceActor][candidate.sourcePart] = {
        candidate.targetActor, candidate.sourcePart, candidate.targetPart, candidate.distance,
        resolvedType};
    usedParts.insert({candidate.sourceActor, candidate.sourcePart});

    assigns[candidate.targetActor][candidate.targetPart] = {
        candidate.sourceActor, candidate.targetPart, candidate.sourcePart, candidate.distance,
        resolvedType};
    usedParts.insert({candidate.targetActor, candidate.targetPart});

    if ((candidate.family == CandidateFamily::Penetration ||
         candidate.family == CandidateFamily::Proximity) &&
        ShouldEmitDebug(candidate.sourceActor, candidate.targetActor, candidate.type,
                        DebugEvent::Assigned, 260.f)) {
      logger::info(
          "[SexLab NG] InteractDebug Assign applied family={} type={} src={:08X}:{} dst={:08X}:{} "
          "resolvedType={} score={:.3f} distance={:.2f}",
          FamilyText(candidate.family), TypeText(candidate.type),
          GetActorFormID(candidate.sourceActor), NameText(candidate.sourcePart),
          GetActorFormID(candidate.targetActor), NameText(candidate.targetPart),
          TypeText(resolvedType), candidate.score, candidate.distance);
    }

    return true;
  }

  void AssignFamilyCandidates(const std::vector<Candidate>& allCandidates, CandidateFamily family,
                              const std::unordered_map<RE::Actor*, ActorData>& datas,
                              UsedPartSet& usedParts, AssignMap& assigns)
  {
    std::unordered_map<RE::Actor*, ActorInteractSummary> summaries;
    if (family == CandidateFamily::Proximity)
      BuildSummaries(assigns, summaries);

    std::vector<Candidate> familyCandidates;
    familyCandidates.reserve(allCandidates.size());
    for (const Candidate& candidate : allCandidates) {
      if (candidate.family != family)
        continue;

      Candidate adjusted = candidate;
      if (family == CandidateFamily::Proximity) {
        if (ShouldSuppressProximityCandidate(adjusted, summaries)) {
          if (ShouldEmitDebug(adjusted.sourceActor, adjusted.targetActor, adjusted.type,
                              DebugEvent::Suppressed, 700.f)) {
            logger::info("[SexLab NG] InteractDebug Proximity suppressed type={} src={:08X}:{} "
                         "dst={:08X}:{} "
                         "detail=current penetration keeps fallback muted",
                         TypeText(adjusted.type), GetActorFormID(adjusted.sourceActor),
                         NameText(adjusted.sourcePart), GetActorFormID(adjusted.targetActor),
                         NameText(adjusted.targetPart));
          }
          continue;
        }

        bool sourceRecent = false;
        bool targetRecent = false;
        if (ShouldSuppressRecentPenetrationFallback(adjusted, datas, &sourceRecent,
                                                    &targetRecent)) {
          if (ShouldEmitDebug(adjusted.sourceActor, adjusted.targetActor, adjusted.type,
                              DebugEvent::Suppressed, 700.f)) {
            logger::info("[SexLab NG] InteractDebug Proximity suppressed type={} src={:08X}:{} "
                         "dst={:08X}:{} "
                         "detail=recent penetration hold sourceRecent={} targetRecent={}",
                         TypeText(adjusted.type), GetActorFormID(adjusted.sourceActor),
                         NameText(adjusted.sourcePart), GetActorFormID(adjusted.targetActor),
                         NameText(adjusted.targetPart), sourceRecent, targetRecent);
          }
          continue;
        }

        adjusted.score += GetProximityPenalty(adjusted, summaries, datas);
      }

      familyCandidates.push_back(adjusted);
    }

    std::sort(familyCandidates.begin(), familyCandidates.end(),
              [](const Candidate& lhs, const Candidate& rhs) {
                if (lhs.score != rhs.score)
                  return lhs.score < rhs.score;
                if (lhs.distance != rhs.distance)
                  return lhs.distance < rhs.distance;
                return static_cast<std::uint8_t>(lhs.type) < static_cast<std::uint8_t>(rhs.type);
              });

    for (const Candidate& candidate : familyCandidates)
      TryAssignCandidate(candidate, usedParts, assigns);
  }

  void LogPenetrationFinalStates(const std::unordered_map<RE::Actor*, ActorData>& datas,
                                 const AssignMap& assigns)
  {
    if (!IsDebugEnabled())
      return;

    for (const auto& [actor, data] : datas) {
      const bool hasVagina = TryGetCurrentSnapshot(data, Name::Vagina) != nullptr;
      const bool hasAnus   = TryGetCurrentSnapshot(data, Name::Anus) != nullptr;
      if (!hasVagina && !hasAnus)
        continue;

      const AssignInfo* vaginalAssign = FindAssignInfo(assigns, actor, Name::Vagina);
      const AssignInfo* analAssign    = FindAssignInfo(assigns, actor, Name::Anus);
      if (!vaginalAssign && !analAssign)
        continue;

      if (!ShouldEmitDebug(actor, actor, Type::None, DebugEvent::FinalState, 600.f))
        continue;

      logger::info("[SexLab NG] InteractDebug Final actor={:08X} has(vag={}, anus={}) "
                   "vaginal={} partner={:08X} anal={} partner={:08X}",
                   GetActorFormID(actor), hasVagina, hasAnus,
                   TypeText(vaginalAssign ? vaginalAssign->type : Type::None),
                   GetActorFormID(vaginalAssign ? vaginalAssign->partner : nullptr),
                   TypeText(analAssign ? analAssign->type : Type::None),
                   GetActorFormID(analAssign ? analAssign->partner : nullptr));
    }
  }

  void ResetInteractionInfos(std::unordered_map<RE::Actor*, ActorData>& datas)
  {
    for (auto& [actor, data] : datas) {
      for (auto& [name, info] : data.infos) {
        info.prevType     = info.type;
        info.prevActor    = info.actor;
        info.prevDistance = info.distance;
        info.type         = Type::None;
        info.actor        = nullptr;
        info.distance     = 0.f;
        info.velocity     = 0.f;
      }
    }
  }

  void UpdateBodyPartPositions(std::unordered_map<RE::Actor*, ActorData>& datas)
  {
    for (auto& [actor, data] : datas) {
      for (auto& [name, info] : data.infos)
        info.bodypart.UpdatePosition();
    }
  }

  void CaptureMotionHistory(std::unordered_map<RE::Actor*, ActorData>& datas, float nowMs)
  {
    for (auto& [actor, data] : datas) {
      for (auto& [name, info] : data.infos) {
        MotionHistory& history = data.motion[name];
        history.older          = history.previous;
        history.previous       = history.current;
        history.current        = BuildSnapshot(info.bodypart, nowMs);
      }
    }
  }

  void ApplyAssignments(std::unordered_map<RE::Actor*, ActorData>& datas, const AssignMap& assigns,
                        float nowMs)
  {
    for (auto& [actor, data] : datas) {
      const float dt    = nowMs - data.lastUpdateMs;
      data.lastUpdateMs = nowMs;

      auto assignIt = assigns.find(actor);
      if (assignIt == assigns.end())
        continue;

      for (auto& [partName, info] : data.infos) {
        auto partIt = assignIt->second.find(partName);
        if (partIt == assignIt->second.end())
          continue;

        const AssignInfo& assign = partIt->second;
        info.actor               = assign.partner;
        info.distance            = assign.distance;
        info.type                = assign.type;
        info.velocity            = dt > 1e-4f ? (assign.distance - info.prevDistance) / dt : 0.f;
      }
    }
  }

}  // namespace

#pragma endregion

#pragma region RuntimeEntryPoints

void InitializeInteractActors(std::unordered_map<RE::Actor*, Interact::ActorData>& datas,
                              const std::vector<RE::Actor*>& actors)
{
  datas.clear();

  for (auto* actor : actors) {
    if (!actor)
      continue;

    ActorData data;
    data.race   = Define::Race(actor);
    data.gender = Define::Gender(actor);

    for (std::uint8_t index = 0; index < static_cast<std::uint8_t>(Name::Penis) + 1; ++index) {
      const auto name = static_cast<Name>(index);
      if (!BP::HasBodyPart(data.gender, data.race, name))
        continue;
      data.infos.emplace(name, Info{BP{actor, data.race, name}});
      data.motion.emplace(name, MotionHistory{});
    }

    datas.emplace(actor, std::move(data));
  }
}

void FlashInteractNodeData(std::unordered_map<RE::Actor*, Interact::ActorData>& datas)
{
  for (auto& [actor, data] : datas)
    for (auto& [name, info] : data.infos)
      info.bodypart.UpdateNodes();
}

void UpdateInteractState(
    std::unordered_map<RE::Actor*, Interact::ActorData>& datas,
    std::unordered_map<RE::Actor*, Interact::GenitalSlotMemory>& genitalSlotMemory,
    std::unordered_map<RE::Actor*, Define::InteractTags>& observedInteractTags)
{
  ResetInteractionInfos(datas);
  UpdateBodyPartPositions(datas);

  const float nowMs = GetSteadyNowMs();
  CaptureMotionHistory(datas, nowMs);

  std::vector<Candidate> candidates;
  std::vector<RE::Actor*> actors;
  actors.reserve(datas.size());
  for (const auto& [actor, _] : datas)
    actors.push_back(actor);

  for (std::size_t i = 0; i < actors.size(); ++i) {
    for (std::size_t j = i; j < actors.size(); ++j) {
      RE::Actor* actorA = actors[i];
      RE::Actor* actorB = actors[j];
      GeneratePairCandidates(candidates, PairContext{actorA, actorB, datas.at(actorA),
                                                     datas.at(actorB), actorA == actorB});
    }
  }

  UsedPartSet usedParts;
  AssignMap assigns;

  AssignPenetrationCandidates(candidates, genitalSlotMemory, usedParts, assigns);
  AssignFamilyCandidates(candidates, CandidateFamily::Proximity, datas, usedParts, assigns);
  AssignFamilyCandidates(candidates, CandidateFamily::Contact, datas, usedParts, assigns);

  std::unordered_map<RE::Actor*, ActorInteractSummary> summaries;
  BuildSummaries(assigns, summaries);

  std::unordered_map<RE::Actor*, std::vector<Type>> comboTypes;
  BuildComboTypes(summaries, comboTypes);
  AccumulateObservedInteractTags(assigns, comboTypes, observedInteractTags);
  ApplyComboOverrides(assigns, summaries, comboTypes);
  LogPenetrationFinalStates(datas, assigns);
  ApplyAssignments(datas, assigns, nowMs);
  UpdateGenitalSlotMemory(datas, assigns, genitalSlotMemory);
}

#pragma endregion

}  // namespace Instance::Detail

#pragma region PublicAPI

namespace Instance
{

Interact::Interact(std::vector<RE::Actor*> actors)
{
  Detail::InitializeInteractActors(datas, actors);
}

void Interact::FlashNodeData()
{
  Detail::FlashInteractNodeData(datas);
}

void Interact::Update()
{
  if (genitalSlotMemory.empty()) {
    for (const auto& [actor, _] : datas) {
      genitalSlotMemory.emplace(actor, GenitalSlotMemory{});
      observedInteractTags.try_emplace(actor, Define::InteractTags{});
    }
  }

  Detail::UpdateInteractState(datas, genitalSlotMemory, observedInteractTags);
}

const Interact::Info& Interact::GetInfo(RE::Actor* actor, Define::BodyPart::Name partName) const
{
  static const Interact::Info kEmpty{};
  auto it = datas.find(actor);
  if (it == datas.end())
    return kEmpty;
  auto partIt = it->second.infos.find(partName);
  if (partIt == it->second.infos.end())
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

#include "Instance/Interact.h"

#include "magic_enum/magic_enum.hpp"

namespace Instance
{

using Name      = Define::BodyPart::Name;
using BP        = Define::BodyPart;
using Type      = Interact::Type;
using ActorData = Interact::ActorData;
using Info      = Interact::Info;

namespace
{

  enum class CandidateFamily : std::uint8_t
  {
    Penetration = 0,
    Oral        = 1,
    Proximity   = 2,
    Contact     = 3,
  };

  struct AngleRange
  {
    float min = 0.f;
    float max = 180.f;
  };

  enum class AnchorMode : std::uint8_t
  {
    None = 0,
    StartToStart,
    EndToStart,
    StartToEnd,
    EndToEnd,
  };

  struct PairContext
  {
    RE::Actor* actorA;
    RE::Actor* actorB;
    const ActorData& dataA;
    const ActorData& dataB;
    bool self;
  };

  struct DirectedContext
  {
    RE::Actor* sourceActor;
    RE::Actor* targetActor;
    const ActorData& sourceData;
    const ActorData& targetData;
    bool self;
  };

  struct Candidate
  {
    RE::Actor* sourceActor;
    Name sourcePart;
    RE::Actor* targetActor;
    Name targetPart;
    Type type;
    float distance;
    float score;
    CandidateFamily family;
  };

  struct AssignInfo
  {
    RE::Actor* partner;
    Name partSelf;
    Name partPartner;
    float distance;
    Type type;
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

  enum class PenetrationDiagEvent : std::uint8_t
  {
    MissingReceiverPart = 0,
    RejectedDistance,
    RejectedAngle,
    RejectedTipAxis,
    RejectedDepth,
    RejectedContextSwitch,
    CandidateAccepted,
    AssignSkipped,
    Assigned,
    FinalState,
  };

  struct PenetrationDiagKey
  {
    RE::Actor* sourceActor     = nullptr;
    RE::Actor* targetActor     = nullptr;
    Type type                  = Type::None;
    PenetrationDiagEvent event = PenetrationDiagEvent::CandidateAccepted;

    bool operator==(const PenetrationDiagKey&) const = default;
  };

  struct PenetrationDiagKeyHash
  {
    std::size_t operator()(const PenetrationDiagKey& key) const
    {
      std::size_t value = std::hash<RE::Actor*>{}(key.sourceActor);
      value ^= std::hash<RE::Actor*>{}(key.targetActor) + 0x9E3779B97F4A7C15ull + (value << 6) +
               (value >> 2);
      value ^= static_cast<std::size_t>(key.type) << 8;
      value ^= static_cast<std::size_t>(key.event) << 16;
      return value;
    }
  };

  struct PenetrationDiagMetrics
  {
    float distance             = std::numeric_limits<float>::infinity();
    float maxDistance          = 0.f;
    float rootEntryDistance    = std::numeric_limits<float>::infinity();
    float angle                = 0.f;
    float maxAngle             = 0.f;
    float tipAxisDistance      = std::numeric_limits<float>::infinity();
    float maxTipAxis           = 0.f;
    float depth                = 0.f;
    float minDepth             = 0.f;
    float maxDepth             = 0.f;
    float score                = -1.f;
    float contextBias          = 0.f;
    float bonus                = 0.f;
    float vaginalSupport       = std::numeric_limits<float>::infinity();
    float analSupport          = std::numeric_limits<float>::infinity();
    float vaginalEntryDistance = std::numeric_limits<float>::infinity();
    float analEntryDistance    = std::numeric_limits<float>::infinity();
    float hysteresisBonus      = 0.f;
    float switchPenalty        = 0.f;
    bool hasVagina             = false;
    bool hasAnus               = false;
    bool stickyPair            = false;
    bool complementaryContext  = false;
    bool relaxVaginalGate      = false;
    bool relaxAnalGate         = false;
    bool previousSameType      = false;
    bool previousOppositeType  = false;
  };

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

  template <std::size_t N>
  bool HasAnyPart(const ActorData& data, const std::array<Name, N>& names)
  {
    for (Name name : names) {
      if (TryGetBodyPart(data, name))
        return true;
    }
    return false;
  }

  std::uint32_t GetActorFormID(RE::Actor* actor)
  {
    return actor ? actor->GetFormID() : 0u;
  }

  float GetSteadyNowMs()
  {
    return static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now().time_since_epoch())
                                  .count()) /
           1000.f;
  }

  bool HasPenetrationReceiverPart(const ActorData& data)
  {
    return TryGetBodyPart(data, Name::Vagina) || TryGetBodyPart(data, Name::Anus);
  }

  bool ShouldLogPenetrationDiagnostics(const DirectedContext& ctx)
  {
    return !ctx.self && TryGetBodyPart(ctx.sourceData, Name::Penis) &&
           HasPenetrationReceiverPart(ctx.targetData);
  }

  bool ShouldEmitPenetrationDiag(RE::Actor* sourceActor, RE::Actor* targetActor, Type type,
                                 PenetrationDiagEvent event, float minIntervalMs = 400.f)
  {
    static std::unordered_map<PenetrationDiagKey, float, PenetrationDiagKeyHash> s_lastLogTimes;

    const float nowMs = GetSteadyNowMs();
    const PenetrationDiagKey key{sourceActor, targetActor, type, event};
    auto [it, inserted] = s_lastLogTimes.try_emplace(key, nowMs);
    if (inserted || nowMs - it->second >= minIntervalMs) {
      it->second = nowMs;
      return true;
    }

    return false;
  }

  void LogPenetrationDiagnostic(const DirectedContext& ctx, Name receiverPartName, Type type,
                                PenetrationDiagEvent event, std::string_view stage,
                                std::string_view detail, const PenetrationDiagMetrics& metrics)
  {
    if (!ShouldLogPenetrationDiagnostics(ctx))
      return;
    if (!ShouldEmitPenetrationDiag(ctx.sourceActor, ctx.targetActor, type, event))
      return;

    logger::info("[SexLab NG] Interact PenetrationDiag {} src={:08X} dst={:08X} receiver={} "
                 "target(vag={}, anus={}) sticky={} complement={} relax(vag={}, anal={}) "
                 "hold(same={}, opposite={}) hyst={:.3f} switchPenalty={:.3f} "
                 "dist={:.2f}/{:.2f} rootEntry={:.2f} angle={:.2f}/{:.2f} tipAxis={:.2f}/{:.2f} "
                 "depth={:.2f}[{:.2f},{:.2f}] support(vag={:.2f}, anal={:.2f}) entry(vag={:.2f}, "
                 "anal={:.2f}) score={:.3f} bias={:.3f} bonus={:.3f} detail={}",
                 stage, GetActorFormID(ctx.sourceActor), GetActorFormID(ctx.targetActor),
                 magic_enum::enum_name(receiverPartName), metrics.hasVagina, metrics.hasAnus,
                 metrics.stickyPair, metrics.complementaryContext, metrics.relaxVaginalGate,
                 metrics.relaxAnalGate, metrics.previousSameType, metrics.previousOppositeType,
                 metrics.hysteresisBonus, metrics.switchPenalty, metrics.distance,
                 metrics.maxDistance, metrics.rootEntryDistance, metrics.angle, metrics.maxAngle,
                 metrics.tipAxisDistance, metrics.maxTipAxis, metrics.depth, metrics.minDepth,
                 metrics.maxDepth, metrics.vaginalSupport, metrics.analSupport,
                 metrics.vaginalEntryDistance, metrics.analEntryDistance, metrics.score,
                 metrics.contextBias, metrics.bonus, detail);
  }

  bool IsAngleUnconstrained(AngleRange range)
  {
    return range.min <= 0.f && range.max >= 179.9f;
  }

  bool CheckAngle(float absAngle, AngleRange range)
  {
    if (IsAngleUnconstrained(range))
      return true;
    if (range.min <= range.max)
      return absAngle >= range.min && absAngle <= range.max;
    return absAngle <= range.max || absAngle >= range.min;
  }

  float AngleDeviation(float absAngle, AngleRange range)
  {
    if (IsAngleUnconstrained(range))
      return 0.f;

    const bool wrap = range.min > range.max;
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

  namespace Geometry
  {

    bool HasDirectionalAxis(const BP& part)
    {
      return part.GetType() != BP::Type::Point && part.GetLength() >= 1e-6f;
    }

    bool IsPointInFront(const BP& part, const Define::Point3f& point)
    {
      if (!HasDirectionalAxis(part))
        return false;
      return (point - part.GetStart()).dot(part.GetDirection()) > 0.f;
    }

    bool IsHorizontal(const BP& part, float toleranceDeg = 20.f)
    {
      if (!HasDirectionalAxis(part))
        return false;

      const float sinTol = std::sin(toleranceDeg * std::numbers::pi_v<float> / 180.f);
      return std::abs(part.GetDirection().z()) <= sinTol;
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

    float MeasurePartAngle(const BP& lhs, const BP& rhs)
    {
      if (!HasDirectionalAxis(lhs) || !HasDirectionalAxis(rhs))
        return 0.f;

      const float dot = std::clamp(lhs.GetDirection().dot(rhs.GetDirection()), -1.f, 1.f);
      return std::acos(dot) * (180.f / std::numbers::pi_v<float>);
    }

    float DistancePointToBodyPart(const Define::Point3f& point, const BP& part)
    {
      if (!HasDirectionalAxis(part))
        return (point - part.GetStart()).norm();
      return DistancePointToSegment(point, part.GetStart(), part.GetEnd());
    }

    float MeasurePartDistance(const BP& lhs, const BP& rhs)
    {
      if (lhs.GetType() == BP::Type::Point && rhs.GetType() == BP::Type::Point)
        return (lhs.GetStart() - rhs.GetStart()).norm();

      const bool lhsSegment = HasDirectionalAxis(lhs);
      const bool rhsSegment = HasDirectionalAxis(rhs);

      if (!lhsSegment)
        return DistancePointToBodyPart(lhs.GetStart(), rhs);
      if (!rhsSegment)
        return DistancePointToBodyPart(rhs.GetStart(), lhs);

      const Define::Vector3f dA = lhs.GetDirection();
      const Define::Vector3f dB = rhs.GetDirection();
      const float lA            = lhs.GetLength();
      const float lB            = rhs.GetLength();
      const Define::Vector3f w  = rhs.GetStart() - lhs.GetStart();
      const float b             = dA.dot(dB);
      const float d             = dA.dot(w);
      const float e             = dB.dot(w);
      const float denom         = 1.f - b * b;

      if (denom < 1e-6f) {
        float distance = DistancePointToBodyPart(lhs.GetStart(), rhs);
        distance       = (std::min)(distance, DistancePointToBodyPart(rhs.GetStart(), lhs));
        distance       = (std::min)(distance, DistancePointToBodyPart(lhs.GetEnd(), rhs));
        distance       = (std::min)(distance, DistancePointToBodyPart(rhs.GetEnd(), lhs));
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

      const Define::Point3f p = lhs.GetStart() + dA * s;
      const Define::Point3f q = rhs.GetStart() + dB * t;
      return (p - q).norm();
    }

    Define::Point3f SamplePartPoint(const BP& part, float fraction)
    {
      if (!HasDirectionalAxis(part))
        return part.GetStart();
      return part.GetStart() + (part.GetEnd() - part.GetStart()) * std::clamp(fraction, 0.f, 1.f);
    }

    struct SampledPartMetrics
    {
      Define::Point3f startPoint = Define::Point3f::Zero();
      Define::Point3f midPoint   = Define::Point3f::Zero();
      Define::Point3f endPoint   = Define::Point3f::Zero();
      float startDistance        = std::numeric_limits<float>::infinity();
      float midDistance          = std::numeric_limits<float>::infinity();
      float endDistance          = std::numeric_limits<float>::infinity();
      float minDistance          = std::numeric_limits<float>::infinity();
      float startEntryDistance   = std::numeric_limits<float>::infinity();
      float midEntryDistance     = std::numeric_limits<float>::infinity();
      float endEntryDistance     = std::numeric_limits<float>::infinity();
      float minEntryDistance     = std::numeric_limits<float>::infinity();
      float startAxisDistance    = std::numeric_limits<float>::infinity();
      float midAxisDistance      = std::numeric_limits<float>::infinity();
      float endAxisDistance      = std::numeric_limits<float>::infinity();
      float minAxisDistance      = std::numeric_limits<float>::infinity();
      float startDepth           = 0.f;
      float midDepth             = 0.f;
      float endDepth             = 0.f;
      float overlapDepth         = 0.f;
    };

    SampledPartMetrics MeasureSampledPartMetrics(const BP& sampledPart, const BP& targetPart)
    {
      SampledPartMetrics metrics;
      metrics.startPoint = SamplePartPoint(sampledPart, 0.f);
      metrics.midPoint   = SamplePartPoint(sampledPart, 0.5f);
      metrics.endPoint   = SamplePartPoint(sampledPart, 1.f);

      const auto measurePoint = [&](const Define::Point3f& point, float& distance,
                                    float& entryDistance, float& axisDistance, float& depth) {
        distance      = DistancePointToBodyPart(point, targetPart);
        entryDistance = (point - targetPart.GetStart()).norm();

        if (!HasDirectionalAxis(targetPart)) {
          axisDistance = distance;
          depth        = 0.f;
          return;
        }

        axisDistance = DistancePointToSegment(point, targetPart.GetStart(), targetPart.GetEnd());
        depth        = ProjectionOnSegmentAxis(point, targetPart.GetStart(), targetPart.GetEnd());
      };

      measurePoint(metrics.startPoint, metrics.startDistance, metrics.startEntryDistance,
                   metrics.startAxisDistance, metrics.startDepth);
      measurePoint(metrics.midPoint, metrics.midDistance, metrics.midEntryDistance,
                   metrics.midAxisDistance, metrics.midDepth);
      measurePoint(metrics.endPoint, metrics.endDistance, metrics.endEntryDistance,
                   metrics.endAxisDistance, metrics.endDepth);

      metrics.minDistance =
          (std::min)(metrics.startDistance, (std::min)(metrics.midDistance, metrics.endDistance));
      metrics.minEntryDistance =
          (std::min)(metrics.startEntryDistance,
                     (std::min)(metrics.midEntryDistance, metrics.endEntryDistance));
      metrics.minAxisDistance =
          (std::min)(metrics.startAxisDistance,
                     (std::min)(metrics.midAxisDistance, metrics.endAxisDistance));

      if (HasDirectionalAxis(targetPart)) {
        const float projectedMin = (std::min)(metrics.startDepth, metrics.endDepth);
        const float projectedMax = (std::max)(metrics.startDepth, metrics.endDepth);
        metrics.overlapDepth = (std::max)(0.f, (std::min)(projectedMax, targetPart.GetLength()) -
                                                   (std::max)(projectedMin, 0.f));
      }

      return metrics;
    }

    float SampledShaftDistance(const SampledPartMetrics& metrics)
    {
      return (std::min)(metrics.midDistance, metrics.endDistance);
    }

    float SampledShaftEntryDistance(const SampledPartMetrics& metrics)
    {
      return (std::min)(metrics.midEntryDistance, metrics.endEntryDistance);
    }

    Define::Point3f SelectAnchorPoint(const BP& part, bool useEnd)
    {
      return useEnd ? part.GetEnd() : part.GetStart();
    }

    float AnchorDistance(const BP& sourcePart, const BP& targetPart, AnchorMode mode)
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

  using Geometry::AnchorDistance;
  using Geometry::MeasurePartAngle;
  using Geometry::MeasurePartDistance;
  using Geometry::MeasureSampledPartMetrics;
  using Geometry::SampledShaftDistance;
  using Geometry::SampledShaftEntryDistance;
  using Geometry::SamplePartPoint;
  using SampledPartMetrics = Geometry::SampledPartMetrics;

  struct OralContactMetrics
  {
    Define::Point3f point  = Define::Point3f::Zero();
    float axisDistance     = std::numeric_limits<float>::infinity();
    float depth            = 0.f;
    float selectionPenalty = 0.f;
  };

  OralContactMetrics MeasureOralContactPoint(const Define::Point3f& point,
                                             const Define::Point3f& oralEntry,
                                             const Define::Point3f& oralAxisEnd,
                                             float selectionPenalty = 0.f)
  {
    return OralContactMetrics{
        point, Geometry::DistancePointToSegment(point, oralEntry, oralAxisEnd),
        Geometry::ProjectionOnSegmentAxis(point, oralEntry, oralAxisEnd), selectionPenalty};
  }

  bool IsBetterOralContact(const OralContactMetrics& candidate, const OralContactMetrics& current)
  {
    const float candidateScore = candidate.axisDistance + candidate.selectionPenalty;
    const float currentScore   = current.axisDistance + current.selectionPenalty;

    if (candidateScore + 0.12f < currentScore)
      return true;

    return std::abs(candidateScore - currentScore) <= 0.12f && candidate.depth > current.depth;
  }

  OralContactMetrics SelectOralContactMetrics(const BP& penisPart, const Define::Point3f& oralEntry,
                                              const Define::Point3f& oralAxisEnd)
  {
    OralContactMetrics best =
        MeasureOralContactPoint(penisPart.GetEnd(), oralEntry, oralAxisEnd, 0.f);

    if (penisPart.GetLength() < 1e-6f)
      return best;

    const float sampleBackDistance = std::clamp(penisPart.GetLength() * 0.30f, 1.2f, 3.5f);
    const Define::Point3f nearTipPoint =
        penisPart.GetEnd() - penisPart.GetDirection() * sampleBackDistance;
    const OralContactMetrics nearTip =
        MeasureOralContactPoint(nearTipPoint, oralEntry, oralAxisEnd, 0.08f);
    const OralContactMetrics midPoint =
        MeasureOralContactPoint(SamplePartPoint(penisPart, 0.5f), oralEntry, oralAxisEnd, 0.28f);
    const OralContactMetrics rootPoint =
        MeasureOralContactPoint(penisPart.GetStart(), oralEntry, oralAxisEnd, 0.62f);

    if (IsBetterOralContact(nearTip, best))
      best = nearTip;
    if (IsBetterOralContact(midPoint, best))
      best = midPoint;
    if (IsBetterOralContact(rootPoint, best))
      best = rootPoint;

    return best;
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

  bool HasStrongRearPreference(const PenetrationDiagMetrics& metrics)
  {
    return HasStrongRearPreference(metrics.vaginalSupport, metrics.analSupport) &&
           HasStrongRearPreference(metrics.vaginalEntryDistance, metrics.analEntryDistance);
  }

  bool HasStrongFrontPreference(float vaginalMetric, float analMetric, float tolerance = 0.55f)
  {
    if (!std::isfinite(vaginalMetric))
      return false;
    if (!std::isfinite(analMetric))
      return true;
    return vaginalMetric + tolerance < analMetric;
  }

  bool HasStrongFrontPreference(const PenetrationDiagMetrics& metrics)
  {
    return HasStrongFrontPreference(metrics.vaginalSupport, metrics.analSupport) &&
           HasStrongFrontPreference(metrics.vaginalEntryDistance, metrics.analEntryDistance);
  }

  float MinDistanceToOralRegion(const ActorData& data, const BP& part)
  {
    float result = std::numeric_limits<float>::infinity();
    if (const BP* mouthPart = TryGetBodyPart(data, Name::Mouth))
      result = (std::min)(result, MeasurePartDistance(part, *mouthPart));
    if (const BP* throatPart = TryGetBodyPart(data, Name::Throat))
      result = (std::min)(result, MeasurePartDistance(part, *throatPart));
    return result;
  }

  float CalculatePenetrationContextBias(const ActorData& receiverData, const BP& penisPart,
                                        Type type)
  {
    if (type != Type::Vaginal && type != Type::Anal)
      return 0.f;

    const BP* thighCleft  = TryGetBodyPart(receiverData, Name::ThighCleft);
    const BP* glutealPart = TryGetBodyPart(receiverData, Name::GlutealCleft);

    // kSupportScale:
    //   把前后区域支撑点的距离差映射到 score 偏置时使用的缩放尺。
    //   数值越大，前后差异必须更明显才能改变排序；数值越小，系统更敏感。
    constexpr float kSupportScale = 6.5f;

    // kMaxBias:
    //   前后区域上下文最多能对 Vaginal / Anal 候选施加多大的 score 修正。
    //   它只能用来打破僵局，不应覆盖距离与角度本身，因此保持在较小范围内。
    constexpr float kMaxBias = 0.34f;

    // 当裆前与裆后的支撑距离接近时，默认轻微偏向 vaginal，
    // 避免 pelvis 中心附近的模糊姿态长期被 anal 抢占。
    constexpr float kVaginalTieBreakBias = 0.10f;

    // 入口点与 penis root 的相对距离，用来区分前后两个 entry 很近的姿态。
    constexpr float kEntryScale               = 5.2f;
    constexpr float kEntryMaxBias             = 0.24f;
    constexpr float kVaginalEntryTieBreakBias = 0.06f;
    constexpr float kTotalMaxBias             = 0.52f;

    const BP* vaginaPart = TryGetBodyPart(receiverData, Name::Vagina);
    const BP* anusPart   = TryGetBodyPart(receiverData, Name::Anus);

    const float vagSupport  = thighCleft ? MeasurePartDistance(*thighCleft, penisPart)
                                         : std::numeric_limits<float>::infinity();
    const float analSupport = glutealPart ? MeasurePartDistance(*glutealPart, penisPart)
                                          : std::numeric_limits<float>::infinity();
    const float vagEntryDistance =
        vaginaPart ? SampledShaftEntryDistance(MeasureSampledPartMetrics(penisPart, *vaginaPart))
                   : std::numeric_limits<float>::infinity();
    const float analEntryDistance =
        anusPart ? SampledShaftEntryDistance(MeasureSampledPartMetrics(penisPart, *anusPart))
                 : std::numeric_limits<float>::infinity();

    float bias = 0.f;

    if (std::isfinite(vagSupport) && std::isfinite(analSupport)) {
      const float delta =
          std::clamp((vagSupport - analSupport) / kSupportScale, -kMaxBias, kMaxBias);
      const float tieBreak = vagSupport <= analSupport + 1.0f ? kVaginalTieBreakBias : 0.f;
      bias += type == Type::Vaginal ? delta - tieBreak : -delta + tieBreak;
    } else if (type == Type::Vaginal && std::isfinite(vagSupport)) {
      bias += -std::clamp(1.f - vagSupport / kSupportScale, 0.f, kMaxBias) - 0.05f;
    } else if (type == Type::Anal && std::isfinite(analSupport)) {
      bias += -std::clamp(1.f - analSupport / kSupportScale, 0.f, kMaxBias);
    }

    if (std::isfinite(vagEntryDistance) && std::isfinite(analEntryDistance)) {
      const float delta = std::clamp((vagEntryDistance - analEntryDistance) / kEntryScale,
                                     -kEntryMaxBias, kEntryMaxBias);
      const float tieBreak =
          vagEntryDistance <= analEntryDistance + 0.75f ? kVaginalEntryTieBreakBias : 0.f;
      bias += type == Type::Vaginal ? delta - tieBreak : -delta + tieBreak;
    } else if (type == Type::Vaginal && std::isfinite(vagEntryDistance)) {
      bias += -std::clamp(1.f - vagEntryDistance / kEntryScale, 0.f, kEntryMaxBias) - 0.03f;
    } else if (type == Type::Anal && std::isfinite(analEntryDistance)) {
      bias += -std::clamp(1.f - analEntryDistance / kEntryScale, 0.f, kEntryMaxBias);
    }

    return std::clamp(bias, -kTotalMaxBias, kTotalMaxBias);
  }

  bool ShouldRelaxVaginalGate(const ActorData& receiverData, const BP& penisPart,
                              float rootEntryDistance, float partDistance, bool stickyPair)
  {
    const BP* thighCleft    = TryGetBodyPart(receiverData, Name::ThighCleft);
    const BP* glutealPart   = TryGetBodyPart(receiverData, Name::GlutealCleft);
    const float vagSupport  = thighCleft ? MeasurePartDistance(*thighCleft, penisPart)
                                         : std::numeric_limits<float>::infinity();
    const float analSupport = glutealPart ? MeasurePartDistance(*glutealPart, penisPart)
                                          : std::numeric_limits<float>::infinity();

    if (!std::isfinite(vagSupport))
      return false;
    if (rootEntryDistance <= 3.6f)
      return !std::isfinite(analSupport) || vagSupport <= analSupport + 1.25f;

    const bool frontContext = !std::isfinite(analSupport) || vagSupport <= analSupport + 1.25f;
    const bool strongFrontContext =
        !std::isfinite(analSupport) || vagSupport + 0.75f <= analSupport;

    if (stickyPair && rootEntryDistance <= 9.1f && partDistance <= 10.4f && frontContext)
      return true;
    if (stickyPair && rootEntryDistance <= 9.8f && partDistance <= 10.8f && strongFrontContext)
      return true;
    if (rootEntryDistance <= 7.4f && partDistance <= 9.2f && strongFrontContext)
      return true;
    return false;
  }

  bool ShouldRelaxAnalGate(const ActorData& receiverData, const BP& penisPart,
                           float rootEntryDistance, float partDistance, bool stickyPair,
                           bool complementaryVaginalContext)
  {
    const BP* thighCleft    = TryGetBodyPart(receiverData, Name::ThighCleft);
    const BP* glutealPart   = TryGetBodyPart(receiverData, Name::GlutealCleft);
    const float vagSupport  = thighCleft ? MeasurePartDistance(*thighCleft, penisPart)
                                         : std::numeric_limits<float>::infinity();
    const float analSupport = glutealPart ? MeasurePartDistance(*glutealPart, penisPart)
                                          : std::numeric_limits<float>::infinity();

    if (!std::isfinite(analSupport))
      return false;
    if (rootEntryDistance <= 3.4f)
      return !std::isfinite(vagSupport) || analSupport <= vagSupport + 1.1f;

    const bool rearContext       = !std::isfinite(vagSupport) || analSupport <= vagSupport + 1.1f;
    const bool strongRearContext = !std::isfinite(vagSupport) || analSupport + 0.75f <= vagSupport;

    if (stickyPair && rootEntryDistance <= 7.8f && partDistance <= 8.8f && rearContext)
      return true;
    if (complementaryVaginalContext && rootEntryDistance <= 8.0f && partDistance <= 8.8f &&
        rearContext)
      return true;
    if (rootEntryDistance <= 6.8f && partDistance <= 8.0f && strongRearContext)
      return true;
    return false;
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
      const BP* sourcePart = TryGetBodyPart(ctx.sourceData, sourceName);
      if (!sourcePart)
        continue;

      for (Name targetName : targetParts) {
        if (ctx.self && sourceName == targetName)
          continue;

        const BP* targetPart = TryGetBodyPart(ctx.targetData, targetName);
        if (!targetPart)
          continue;

        const float distance = MeasurePartDistance(*sourcePart, *targetPart);
        if (distance > maxDistance)
          continue;

        const float angle = MeasurePartAngle(*sourcePart, *targetPart);
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
          const float anchorDistance = AnchorDistance(*sourcePart, *targetPart, anchorMode);
          const float anchorNorm     = NormalizeDistance(anchorDistance, auxMaxDistance);
          score                      = score * (1.f - anchorWeight) + anchorNorm * anchorWeight;
        }

        const float bonus = TemporalBonus(ctx.sourceData, sourceName, ctx.targetActor, type) +
                            TemporalBonus(ctx.targetData, targetName, ctx.sourceActor, type);
        score             = (std::max)(0.f, score + scoreOffset - bonus);

        AddCandidate(out, ctx, sourceName, targetName, type, family, distance, score);
      }
    }
  }

  template <std::size_t NT>
  void AddSampledPenisDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx,
                                         const std::array<Name, NT>& targetParts, Type type,
                                         CandidateFamily family, float maxDistance,
                                         AngleRange angleRange, float angleWeight,
                                         float scoreOffset     = 0.f,
                                         AnchorMode anchorMode = AnchorMode::None,
                                         float anchorWeight = 0.f, float anchorMaxDistance = 0.f)
  {
    const BP* penisPart = TryGetBodyPart(ctx.sourceData, Name::Penis);
    if (!penisPart)
      return;

    for (Name targetName : targetParts) {
      const BP* targetPart = TryGetBodyPart(ctx.targetData, targetName);
      if (!targetPart)
        continue;

      const SampledPartMetrics penisMetrics = MeasureSampledPartMetrics(*penisPart, *targetPart);
      const float distance                  = SampledShaftDistance(penisMetrics);
      if (distance > maxDistance)
        continue;

      const float angle = MeasurePartAngle(*penisPart, *targetPart);
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
        const float anchorDistance = AnchorDistance(*penisPart, *targetPart, anchorMode);
        const float anchorNorm     = NormalizeDistance(anchorDistance, auxMaxDistance);
        score                      = score * (1.f - anchorWeight) + anchorNorm * anchorWeight;
      }

      const float bonus = TemporalBonus(ctx.sourceData, Name::Penis, ctx.targetActor, type) +
                          TemporalBonus(ctx.targetData, targetName, ctx.sourceActor, type);
      score             = (std::max)(0.f, score + scoreOffset - bonus);

      AddCandidate(out, ctx, Name::Penis, targetName, type, family, distance, score);
    }
  }

  void TryAddSymmetricCandidate(std::vector<Candidate>& out, const PairContext& pair, Name partA,
                                Name partB, Type type, CandidateFamily family, float maxDistance,
                                AngleRange angleRange, float angleWeight, float scoreOffset = 0.f)
  {
    if (pair.self)
      return;

    const BP* lhs = TryGetBodyPart(pair.dataA, partA);
    const BP* rhs = TryGetBodyPart(pair.dataB, partB);
    if (!lhs || !rhs)
      return;

    const float distance = MeasurePartDistance(*lhs, *rhs);
    if (distance > maxDistance)
      return;

    const float angle = MeasurePartAngle(*lhs, *rhs);
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

    DirectedContext ctx{pair.actorA, pair.actorB, pair.dataA, pair.dataB, false};
    AddCandidate(out, ctx, partA, partB, type, family, distance, score);
  }

  void TryAddNaveljobCandidate(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (!TryGetBodyPart(ctx.sourceData, Name::Penis) ||
        !TryGetBodyPart(ctx.targetData, Name::Belly))
      return;

    const BP& penis                       = *TryGetBodyPart(ctx.sourceData, Name::Penis);
    const BP& belly                       = *TryGetBodyPart(ctx.targetData, Name::Belly);
    const SampledPartMetrics penisMetrics = MeasureSampledPartMetrics(penis, belly);

    const float distance = SampledShaftDistance(penisMetrics);
    if (distance > 10.5f)
      return;

    const float angle = MeasurePartAngle(belly, penis);
    if (angle > 60.f)
      return;

    if (!Geometry::IsPointInFront(belly, penis.GetEnd()) || !Geometry::IsHorizontal(penis))
      return;

    const float distNorm  = NormalizeDistance(distance, 10.5f);
    const float angleNorm = AngleDeviation(angle, {0.f, 60.f});
    const float bonus =
        TemporalBonus(ctx.sourceData, Name::Penis, ctx.targetActor, Type::Naveljob) +
        TemporalBonus(ctx.targetData, Name::Belly, ctx.sourceActor, Type::Naveljob);
    const float score = (std::max)(0.f, distNorm * 0.55f + angleNorm * 0.45f - bonus);

    AddCandidate(out, ctx, Name::Penis, Name::Belly, Type::Naveljob, CandidateFamily::Proximity,
                 distance, score);
  }

  void TryAddFellatioCandidate(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    const BP* mouthPart  = TryGetBodyPart(ctx.sourceData, Name::Mouth);
    const BP* penisPart  = TryGetBodyPart(ctx.targetData, Name::Penis);
    const BP* throatPart = TryGetBodyPart(ctx.sourceData, Name::Throat);
    if (!mouthPart || !penisPart)
      return;

    constexpr float kMaxDistance       = 9.4f;
    constexpr float kStickyMaxDistance = 10.4f;
    constexpr float kMaxAngle          = 76.f;
    constexpr float kStickyMaxAngle    = 84.f;
    constexpr float kMaxTipAxis        = 6.6f;
    constexpr float kStickyMaxTipAxis  = 7.8f;
    constexpr float kMinDepth          = -2.5f;
    constexpr float kStickyMinDepth    = -3.6f;
    constexpr float kMaxDepth          = 10.5f;
    constexpr float kStickyMaxDepth    = 12.4f;

    const Define::Point3f oralEntry   = mouthPart->GetStart();
    const Define::Point3f oralAxisEnd = throatPart ? throatPart->GetEnd() : mouthPart->GetEnd();
    if ((oralAxisEnd - oralEntry).norm() < 1e-6f)
      return;

    const bool stickyPair = HasStickyInteractionPair(ctx, Name::Mouth, Name::Penis, Type::Fellatio);
    const float effectiveMaxDistance = stickyPair ? kStickyMaxDistance : kMaxDistance;
    const float effectiveMaxAngle    = stickyPair ? kStickyMaxAngle : kMaxAngle;
    const float effectiveMaxTipAxis  = stickyPair ? kStickyMaxTipAxis : kMaxTipAxis;
    const float effectiveMinDepth    = stickyPair ? kStickyMinDepth : kMinDepth;
    const float effectiveMaxDepth    = stickyPair ? kStickyMaxDepth : kMaxDepth;

    const OralContactMetrics oralContact =
        SelectOralContactMetrics(*penisPart, oralEntry, oralAxisEnd);
    const float mouthDistance = MeasurePartDistance(*mouthPart, *penisPart);
    const float entryDistance = (oralContact.point - oralEntry).norm();
    const float distance      = (std::min)(mouthDistance, entryDistance);
    if (distance > effectiveMaxDistance)
      return;

    const float angle = MeasurePartAngle(*mouthPart, *penisPart);
    if (angle > effectiveMaxAngle)
      return;

    const float tipAxisDistance = oralContact.axisDistance;
    if (tipAxisDistance > effectiveMaxTipAxis)
      return;

    const float depth = oralContact.depth;
    if (depth < effectiveMinDepth || depth > effectiveMaxDepth)
      return;

    const float distNorm  = NormalizeDistance(distance, effectiveMaxDistance);
    const float axisNorm  = NormalizeDistance(tipAxisDistance, effectiveMaxTipAxis);
    const float angleNorm = AngleDeviation(angle, {0.f, effectiveMaxAngle});

    float depthPenalty         = 0.f;
    const float oralAxisLength = (oralAxisEnd - oralEntry).norm();
    if (depth < 0.f)
      depthPenalty = std::abs(depth) / (std::max)(std::abs(effectiveMinDepth), 1e-4f);
    else if (depth > oralAxisLength)
      depthPenalty =
          (depth - oralAxisLength) / (std::max)(effectiveMaxDepth - oralAxisLength, 1e-4f);

    const float bonus =
        TemporalBonus(ctx.sourceData, Name::Mouth, ctx.targetActor, Type::Fellatio) +
        TemporalBonus(ctx.targetData, Name::Penis, ctx.sourceActor, Type::Fellatio);
    const float score = (std::max)(0.f, distNorm * 0.34f + axisNorm * 0.34f + angleNorm * 0.18f +
                                            std::clamp(depthPenalty, 0.f, 1.f) * 0.14f - bonus -
                                            (stickyPair ? 0.16f : 0.12f));

    AddCandidate(out, ctx, Name::Mouth, Name::Penis, Type::Fellatio, CandidateFamily::Oral,
                 distance, score);
  }

  void TryAddHandjobCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    const BP* penisPart = TryGetBodyPart(ctx.targetData, Name::Penis);
    if (!penisPart)
      return;

    constexpr float kMaxDistance = 8.f;
    constexpr AngleRange kAngleRange{50.f, 130.f};
    constexpr float kAngleWeight             = 0.20f;
    constexpr float kSelfFaceGuardDistance   = 6.6f;
    constexpr float kOralInterferencePenalty = 0.45f;
    constexpr float kPartnerOralDistance     = 8.6f;

    for (Name handName : kHandParts) {
      const BP* handPart = TryGetBodyPart(ctx.sourceData, handName);
      if (!handPart)
        continue;

      const SampledPartMetrics penisMetrics = MeasureSampledPartMetrics(*penisPart, *handPart);

      const float distance = SampledShaftDistance(penisMetrics);
      if (distance > kMaxDistance)
        continue;

      const float angle = MeasurePartAngle(*handPart, *penisPart);
      if (!CheckAngle(angle, kAngleRange))
        continue;

      const float handToOral  = MinDistanceToOralRegion(ctx.sourceData, *handPart);
      const float penisToOral = MinDistanceToOralRegion(ctx.sourceData, *penisPart);
      if (ctx.self && std::isfinite(handToOral) && handToOral < kSelfFaceGuardDistance)
        continue;

      const bool oralInterference =
          !ctx.self && std::isfinite(handToOral) && handToOral < kSelfFaceGuardDistance &&
          std::isfinite(penisToOral) && penisToOral < kPartnerOralDistance;

      const float distNorm  = NormalizeDistance(distance, kMaxDistance);
      const float angleNorm = AngleDeviation(angle, kAngleRange);
      float score           = distNorm * (1.f - kAngleWeight) + angleNorm * kAngleWeight;
      if (oralInterference)
        score += kOralInterferencePenalty;

      const float bonus =
          TemporalBonus(ctx.sourceData, handName, ctx.targetActor, Type::Handjob) +
          TemporalBonus(ctx.targetData, Name::Penis, ctx.sourceActor, Type::Handjob);
      score = (std::max)(0.f, score - bonus);

      AddCandidate(out, ctx, handName, Name::Penis, Type::Handjob, CandidateFamily::Contact,
                   distance, score);
    }
  }

  void TryAddGropeBreastCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    constexpr float kMaxDistance       = 5.4f;
    constexpr float kMaxNippleDistance = 4.9f;
    constexpr AngleRange kAngleRange{156.f, 180.f};

    for (Name handName : kHandParts) {
      const BP* handPart = TryGetBodyPart(ctx.sourceData, handName);
      if (!handPart)
        continue;

      for (Name breastName : kBreastParts) {
        const BP* breastPart = TryGetBodyPart(ctx.targetData, breastName);
        if (!breastPart)
          continue;

        const float distance = MeasurePartDistance(*handPart, *breastPart);
        if (distance > kMaxDistance)
          continue;

        const float angle = MeasurePartAngle(*handPart, *breastPart);
        if (!CheckAngle(angle, kAngleRange))
          continue;

        const float nippleDistance = (handPart->GetStart() - breastPart->GetEnd()).norm();
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

  void TryAddPenetrationCandidate(std::vector<Candidate>& out, const DirectedContext& ctx,
                                  Name receiverPartName, Type type, float maxDistance,
                                  float maxAngle, float maxTipAxis, float minDepth, float maxDepth)
  {
    const BP* penisPart    = TryGetBodyPart(ctx.sourceData, Name::Penis);
    const BP* receiverPart = TryGetBodyPart(ctx.targetData, receiverPartName);
    const BP* vaginaPart   = TryGetBodyPart(ctx.targetData, Name::Vagina);
    const BP* anusPart     = TryGetBodyPart(ctx.targetData, Name::Anus);
    const BP* thighCleft   = TryGetBodyPart(ctx.targetData, Name::ThighCleft);
    const BP* glutealCleft = TryGetBodyPart(ctx.targetData, Name::GlutealCleft);

    PenetrationDiagMetrics metrics;
    metrics.hasVagina = vaginaPart != nullptr;
    metrics.hasAnus   = anusPart != nullptr;

    if (!penisPart || !receiverPart) {
      if (penisPart && HasPenetrationReceiverPart(ctx.targetData)) {
        LogPenetrationDiagnostic(ctx, receiverPartName, type,
                                 PenetrationDiagEvent::MissingReceiverPart, "missing-receiver",
                                 "receiver bodypart missing or invalid", metrics);
      }
      return;
    }

    const SampledPartMetrics penetrationMetrics =
        MeasureSampledPartMetrics(*penisPart, *receiverPart);
    const float distance           = SampledShaftDistance(penetrationMetrics);
    const float rootEntryDistance  = penetrationMetrics.startEntryDistance;
    const float shaftEntryDistance = SampledShaftEntryDistance(penetrationMetrics);
    const bool stickyPair = HasStickyInteractionPair(ctx, Name::Penis, receiverPartName, type);
    const bool previousSameTypePair =
        HadPreviousInteraction(ctx.sourceData, Name::Penis, ctx.targetActor, type) ||
        HadPreviousInteraction(ctx.targetData, receiverPartName, ctx.sourceActor, type);
    const bool previousOppositeTypePair =
        type == Type::Anal
            ? (HadPreviousInteraction(ctx.sourceData, Name::Penis, ctx.targetActor,
                                      Type::Vaginal) ||
               HadPreviousInteraction(ctx.targetData, Name::Vagina, ctx.sourceActor, Type::Vaginal))
        : type == Type::Vaginal
            ? (HadPreviousInteraction(ctx.sourceData, Name::Penis, ctx.targetActor, Type::Anal) ||
               HadPreviousInteraction(ctx.targetData, Name::Anus, ctx.sourceActor, Type::Anal))
            : false;
    const bool complementaryPenetrationContext =
        type == Type::Anal      ? HasPreviousVaginalContext(ctx.targetData)
        : type == Type::Vaginal ? HasPreviousAnalContext(ctx.targetData)
                                : false;
    const bool relaxVaginalGate =
        type == Type::Vaginal &&
        ShouldRelaxVaginalGate(ctx.targetData, *penisPart, rootEntryDistance, distance, stickyPair);
    const bool relaxAnalGate =
        type == Type::Anal &&
        ShouldRelaxAnalGate(ctx.targetData, *penisPart, rootEntryDistance, distance, stickyPair,
                            complementaryPenetrationContext);

    metrics.distance             = distance;
    metrics.rootEntryDistance    = rootEntryDistance;
    metrics.stickyPair           = stickyPair;
    metrics.previousSameType     = previousSameTypePair;
    metrics.previousOppositeType = previousOppositeTypePair;
    metrics.complementaryContext = complementaryPenetrationContext;
    metrics.relaxVaginalGate     = relaxVaginalGate;
    metrics.relaxAnalGate        = relaxAnalGate;
    metrics.vaginalSupport       = thighCleft ? MeasurePartDistance(*thighCleft, *penisPart)
                                              : std::numeric_limits<float>::infinity();
    metrics.analSupport          = glutealCleft ? MeasurePartDistance(*glutealCleft, *penisPart)
                                                : std::numeric_limits<float>::infinity();
    metrics.vaginalEntryDistance =
        vaginaPart ? SampledShaftEntryDistance(MeasureSampledPartMetrics(*penisPart, *vaginaPart))
                   : std::numeric_limits<float>::infinity();
    metrics.analEntryDistance =
        anusPart ? SampledShaftEntryDistance(MeasureSampledPartMetrics(*penisPart, *anusPart))
                 : std::numeric_limits<float>::infinity();

    float effectiveMaxDistance = maxDistance;
    float effectiveMaxAngle    = maxAngle;
    float effectiveMaxTipAxis  = maxTipAxis;
    float effectiveMinDepth    = minDepth;
    float effectiveMaxDepth    = maxDepth;

    if (relaxVaginalGate) {
      effectiveMaxDistance = (std::max)(effectiveMaxDistance, stickyPair ? 9.6f : 8.8f);
      effectiveMaxAngle    = (std::max)(effectiveMaxAngle, stickyPair ? 72.f : 68.f);
      effectiveMaxTipAxis  = (std::max)(effectiveMaxTipAxis, 10.f);
      effectiveMinDepth    = (std::min)(effectiveMinDepth, -5.5f);
      effectiveMaxDepth    = (std::max)(effectiveMaxDepth, 14.f);
    }

    if (type == Type::Vaginal && complementaryPenetrationContext) {
      effectiveMaxDistance = (std::max)(effectiveMaxDistance, stickyPair ? 9.8f : 9.1f);
      effectiveMaxAngle    = (std::max)(effectiveMaxAngle, stickyPair ? 72.f : 68.f);
      effectiveMaxTipAxis  = (std::max)(effectiveMaxTipAxis, 10.4f);
      effectiveMinDepth    = (std::min)(effectiveMinDepth, -5.8f);
      effectiveMaxDepth    = (std::max)(effectiveMaxDepth, 14.5f);
    }

    if (relaxAnalGate) {
      effectiveMaxDistance = (std::max)(effectiveMaxDistance, stickyPair ? 8.4f : 8.0f);
      effectiveMaxAngle    = (std::max)(effectiveMaxAngle, stickyPair ? 48.f : 44.f);
      effectiveMaxTipAxis  = (std::max)(effectiveMaxTipAxis, stickyPair ? 7.2f : 6.6f);
      effectiveMinDepth    = (std::min)(effectiveMinDepth, -3.6f);
      effectiveMaxDepth    = (std::max)(effectiveMaxDepth, 11.8f);
    }

    float hysteresisBonus = 0.f;
    float switchPenalty   = 0.f;

    if (previousSameTypePair) {
      if (type == Type::Vaginal) {
        effectiveMaxDistance = (std::max)(effectiveMaxDistance, 10.2f);
        effectiveMaxAngle    = (std::max)(effectiveMaxAngle, 60.f);
        effectiveMaxTipAxis  = (std::max)(effectiveMaxTipAxis, 8.4f);
        effectiveMinDepth    = (std::min)(effectiveMinDepth, -4.8f);
        effectiveMaxDepth    = (std::max)(effectiveMaxDepth, 13.8f);
        hysteresisBonus += 0.26f;
      } else if (type == Type::Anal) {
        effectiveMaxDistance = (std::max)(effectiveMaxDistance, 8.8f);
        effectiveMaxAngle    = (std::max)(effectiveMaxAngle, 42.f);
        effectiveMaxTipAxis  = (std::max)(effectiveMaxTipAxis, 6.9f);
        effectiveMinDepth    = (std::min)(effectiveMinDepth, -3.8f);
        effectiveMaxDepth    = (std::max)(effectiveMaxDepth, 11.8f);
        hysteresisBonus += 0.24f;
      }
    }

    if (previousOppositeTypePair) {
      const bool strongContext = type == Type::Anal      ? HasStrongRearPreference(metrics)
                                 : type == Type::Vaginal ? HasStrongFrontPreference(metrics)
                                                         : false;
      if (!strongContext)
        switchPenalty = type == Type::Anal ? 0.32f : 0.26f;
    }

    metrics.maxDistance     = effectiveMaxDistance;
    metrics.maxAngle        = effectiveMaxAngle;
    metrics.maxTipAxis      = effectiveMaxTipAxis;
    metrics.minDepth        = effectiveMinDepth;
    metrics.maxDepth        = effectiveMaxDepth;
    metrics.hysteresisBonus = hysteresisBonus;
    metrics.switchPenalty   = switchPenalty;
    metrics.contextBias     = CalculatePenetrationContextBias(ctx.targetData, *penisPart, type);
    metrics.bonus = TemporalBonus(ctx.sourceData, Name::Penis, ctx.targetActor, type) +
                    TemporalBonus(ctx.targetData, receiverPartName, ctx.sourceActor, type);

    if (distance > effectiveMaxDistance) {
      LogPenetrationDiagnostic(ctx, receiverPartName, type, PenetrationDiagEvent::RejectedDistance,
                               "reject-distance", "distance above gate", metrics);
      return;
    }

    const float angle = MeasurePartAngle(*receiverPart, *penisPart);
    metrics.angle     = angle;
    if (angle > effectiveMaxAngle) {
      LogPenetrationDiagnostic(ctx, receiverPartName, type, PenetrationDiagEvent::RejectedAngle,
                               "reject-angle", "angle above gate", metrics);
      return;
    }

    const bool midDepthInRange = penetrationMetrics.midDepth >= effectiveMinDepth &&
                                 penetrationMetrics.midDepth <= effectiveMaxDepth;
    const bool tipDepthInRange = penetrationMetrics.endDepth >= effectiveMinDepth &&
                                 penetrationMetrics.endDepth <= effectiveMaxDepth;

    bool useMidSample =
        penetrationMetrics.midAxisDistance + 0.12f < penetrationMetrics.endAxisDistance;
    if (midDepthInRange != tipDepthInRange)
      useMidSample = midDepthInRange;

    float axisDistance =
        useMidSample ? penetrationMetrics.midAxisDistance : penetrationMetrics.endAxisDistance;
    if (axisDistance > effectiveMaxTipAxis) {
      const float alternateAxisDistance =
          useMidSample ? penetrationMetrics.endAxisDistance : penetrationMetrics.midAxisDistance;
      if (alternateAxisDistance <= effectiveMaxTipAxis) {
        useMidSample = !useMidSample;
        axisDistance = alternateAxisDistance;
      }
    }

    metrics.tipAxisDistance = axisDistance;
    if (axisDistance > effectiveMaxTipAxis) {
      LogPenetrationDiagnostic(ctx, receiverPartName, type, PenetrationDiagEvent::RejectedTipAxis,
                               "reject-tip-axis", "shaft axis distance above gate", metrics);
      return;
    }

    float depth = useMidSample ? penetrationMetrics.midDepth : penetrationMetrics.endDepth;
    const bool shaftInsideReceiver = penetrationMetrics.overlapDepth > 0.05f;
    const bool nearEntryHold       = previousSameTypePair && shaftEntryDistance <= 2.0f &&
                                     axisDistance <= effectiveMaxTipAxis + 0.6f;

    if (!shaftInsideReceiver && !nearEntryHold) {
      metrics.depth = depth;
      LogPenetrationDiagnostic(ctx, receiverPartName, type, PenetrationDiagEvent::RejectedDepth,
                               "reject-depth", "shaft fully outside receiver interval", metrics);
      return;
    }

    if (depth < effectiveMinDepth || depth > effectiveMaxDepth) {
      if (midDepthInRange && penetrationMetrics.midAxisDistance <= effectiveMaxTipAxis) {
        useMidSample = true;
        axisDistance = penetrationMetrics.midAxisDistance;
        depth        = penetrationMetrics.midDepth;
      } else if (tipDepthInRange && penetrationMetrics.endAxisDistance <= effectiveMaxTipAxis) {
        useMidSample = false;
        axisDistance = penetrationMetrics.endAxisDistance;
        depth        = penetrationMetrics.endDepth;
      } else if (nearEntryHold) {
        depth = std::clamp(depth, effectiveMinDepth + 0.05f, effectiveMaxDepth - 0.05f);
      } else if (!nearEntryHold) {
        metrics.depth           = depth;
        metrics.tipAxisDistance = axisDistance;
        LogPenetrationDiagnostic(ctx, receiverPartName, type, PenetrationDiagEvent::RejectedDepth,
                                 "reject-depth", "depth outside gate", metrics);
        return;
      }
    }

    metrics.tipAxisDistance = axisDistance;
    metrics.depth           = depth;
    if (depth < effectiveMinDepth || depth > effectiveMaxDepth) {
      LogPenetrationDiagnostic(ctx, receiverPartName, type, PenetrationDiagEvent::RejectedDepth,
                               "reject-depth", "depth outside gate", metrics);
      return;
    }

    const float distNorm  = NormalizeDistance(distance, effectiveMaxDistance);
    const float angleNorm = AngleDeviation(angle, {0.f, effectiveMaxAngle});
    const float axisNorm  = NormalizeDistance(axisDistance, effectiveMaxTipAxis);

    float depthPenalty = 0.f;
    if (depth < 0.f)
      depthPenalty = std::abs(depth) / (std::max)(std::abs(effectiveMinDepth), 1e-4f);
    else if (depth > receiverPart->GetLength())
      depthPenalty = (depth - receiverPart->GetLength()) /
                     (std::max)(effectiveMaxDepth - receiverPart->GetLength(), 1e-4f);

    constexpr float kPenetrationPriorityBias = 0.10f;
    const float complementaryBonus =
        complementaryPenetrationContext ? (type == Type::Anal ? 0.12f : 0.08f) : 0.f;

    const float score =
        (std::max)(0.f, distNorm * 0.42f + angleNorm * 0.22f + axisNorm * 0.22f +
                            std::clamp(depthPenalty, 0.f, 1.f) * 0.14f + metrics.contextBias -
                            metrics.bonus - hysteresisBonus + switchPenalty -
                            kPenetrationPriorityBias - complementaryBonus);

    metrics.score = score;
    LogPenetrationDiagnostic(ctx, receiverPartName, type, PenetrationDiagEvent::CandidateAccepted,
                             "candidate-accepted", "penetration candidate added", metrics);

    AddCandidate(out, ctx, Name::Penis, receiverPartName, type, CandidateFamily::Penetration,
                 distance, score);
  }

  void GenerateOralDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (ctx.self)
      return;
    if (!HasAnyPart(ctx.sourceData, kMouthParts) && !HasAnyPart(ctx.sourceData, kThroatParts))
      return;

    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kBreastParts, Type::BreastSucking,
                                CandidateFamily::Oral, 8.f, {0.f, 180.f}, 0.f);
    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kFootParts, Type::ToeSucking,
                                CandidateFamily::Oral, 8.f, {0.f, 180.f}, 0.f);
    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kThighCleftParts, Type::Cunnilingus,
                                CandidateFamily::Oral, 7.2f, {0.f, 180.f}, 0.f, -0.05f,
                                AnchorMode::StartToStart, 0.18f, 7.2f);
    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kVaginaParts, Type::Cunnilingus,
                                CandidateFamily::Oral, 7.7f, {0.f, 180.f}, 0.f, 0.01f,
                                AnchorMode::StartToStart, 0.22f, 7.4f);
    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kAnusParts, Type::Anilingus,
                                CandidateFamily::Oral, 7.1f, {0.f, 180.f}, 0.f, 0.f,
                                AnchorMode::StartToStart, 0.20f, 6.8f);
    TryAddFellatioCandidate(out, ctx);
    AddSimpleDirectedCandidates(out, ctx, kThroatParts, kPenisParts, Type::DeepThroat,
                                CandidateFamily::Oral, 7.f, {0.f, 34.f}, 0.42f, -0.02f);
  }

  void GenerateManualDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (!HasAnyPart(ctx.sourceData, kHandParts) && !HasAnyPart(ctx.sourceData, kFingerParts))
      return;

    TryAddGropeBreastCandidates(out, ctx);
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
    TryAddHandjobCandidates(out, ctx);
  }

  void GenerateContactDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (!HasAnyPart(ctx.sourceData, kPenisParts))
      return;

    AddSampledPenisDirectedCandidates(out, ctx, kBreastParts, Type::Titfuck,
                                      CandidateFamily::Proximity, 8.f, {0.f, 58.f}, 0.32f);
    AddSampledPenisDirectedCandidates(out, ctx, kCleavageParts, Type::Titfuck,
                                      CandidateFamily::Proximity, 8.f, {70.f, 110.f}, 0.40f,
                                      -0.03f);
    TryAddNaveljobCandidate(out, ctx);
    AddSampledPenisDirectedCandidates(out, ctx, kThighCleftParts, Type::Thighjob,
                                      CandidateFamily::Proximity, 6.5f, {0.f, 32.f}, 0.24f, 0.02f);
    AddSampledPenisDirectedCandidates(out, ctx, kThighParts, Type::Thighjob,
                                      CandidateFamily::Proximity, 5.4f, {0.f, 32.f}, 0.22f, 0.01f);
    AddSampledPenisDirectedCandidates(out, ctx, kGlutealCleftParts, Type::Frottage,
                                      CandidateFamily::Proximity, 8.f, {140.f, 40.f}, 0.28f);
    AddSampledPenisDirectedCandidates(out, ctx, kFootParts, Type::Footjob,
                                      CandidateFamily::Proximity, 8.2f, {0.f, 180.f}, 0.f);
  }

  void GeneratePenetrationDirectedCandidates(std::vector<Candidate>& out,
                                             const DirectedContext& ctx)
  {
    if (ctx.self || !HasAnyPart(ctx.sourceData, kPenisParts))
      return;

    TryAddPenetrationCandidate(out, ctx, Name::Vagina, Type::Vaginal, 8.4f, 30.f, 5.5f, -3.5f,
                               12.f);
    TryAddPenetrationCandidate(out, ctx, Name::Anus, Type::Anal, 6.6f, 20.f, 4.5f, -2.5f, 9.8f);
  }

  void GenerateSymmetricCandidates(std::vector<Candidate>& out, const PairContext& pair)
  {
    TryAddSymmetricCandidate(out, pair, Name::Mouth, Name::Mouth, Type::Kiss, CandidateFamily::Oral,
                             6.8f, {140.f, 180.f}, 0.48f);
    TryAddSymmetricCandidate(out, pair, Name::Vagina, Name::Vagina, Type::Tribbing,
                             CandidateFamily::Proximity, 8.f, {140.f, 180.f}, 0.32f);
  }

  void GenerateDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    GenerateOralDirectedCandidates(out, ctx);
    GenerateManualDirectedCandidates(out, ctx);
    GenerateContactDirectedCandidates(out, ctx);
    GeneratePenetrationDirectedCandidates(out, ctx);
  }

  void GeneratePairCandidates(std::vector<Candidate>& out, const PairContext& pair)
  {
    if (pair.self) {
      const DirectedContext selfCtx{pair.actorA, pair.actorA, pair.dataA, pair.dataA, true};
      GenerateManualDirectedCandidates(out, selfCtx);
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

  using AssignMap = std::unordered_map<RE::Actor*, std::unordered_map<Name, AssignInfo>>;

  const AssignInfo* FindAssignInfo(const AssignMap& assigns, RE::Actor* actor, Name partName)
  {
    auto actorIt = assigns.find(actor);
    if (actorIt == assigns.end())
      return nullptr;

    auto partIt = actorIt->second.find(partName);
    return partIt == actorIt->second.end() ? nullptr : &partIt->second;
  }

  void LogPenetrationFinalStates(const std::unordered_map<RE::Actor*, ActorData>& datas,
                                 const AssignMap& assigns)
  {
    for (const auto& [actor, data] : datas) {
      const bool hasVagina = TryGetBodyPart(data, Name::Vagina) != nullptr;
      const bool hasAnus   = TryGetBodyPart(data, Name::Anus) != nullptr;
      if (!hasVagina && !hasAnus)
        continue;

      const AssignInfo* vaginalAssign = FindAssignInfo(assigns, actor, Name::Vagina);
      const AssignInfo* analAssign    = FindAssignInfo(assigns, actor, Name::Anus);
      if (!vaginalAssign && !analAssign)
        continue;

      if (!ShouldEmitPenetrationDiag(actor, actor, Type::None, PenetrationDiagEvent::FinalState,
                                     250.f)) {
        continue;
      }

      logger::info("[SexLab NG] Interact PenetrationFinal actor={:08X} has(vag={}, anus={}) "
                   "vaginal={} partner={:08X} anal={} partner={:08X}",
                   GetActorFormID(actor), hasVagina, hasAnus,
                   magic_enum::enum_name(vaginalAssign ? vaginalAssign->type : Type::None),
                   GetActorFormID(vaginalAssign ? vaginalAssign->partner : nullptr),
                   magic_enum::enum_name(analAssign ? analAssign->type : Type::None),
                   GetActorFormID(analAssign ? analAssign->partner : nullptr));
    }
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

  using PartKey = std::pair<RE::Actor*, Name>;

  struct PartKeyHash
  {
    std::size_t operator()(const PartKey& key) const
    {
      return std::hash<RE::Actor*>{}(key.first) ^ (static_cast<std::size_t>(key.second) << 8);
    }
  };

  using UsedPartSet = std::unordered_set<PartKey, PartKeyHash>;

  bool IsPenetrationSensitiveProximity(Type type)
  {
    switch (type) {
    case Type::Frottage:
    case Type::Thighjob:
    case Type::Naveljob:
      return true;
    default:
      return false;
    }
  }

  bool ShouldSuppressProximityCandidate(
      const Candidate& candidate,
      const std::unordered_map<RE::Actor*, ActorInteractSummary>& summaries)
  {
    if (!IsPenetrationSensitiveProximity(candidate.type))
      return false;

    auto it = summaries.find(candidate.targetActor);
    if (it == summaries.end())
      return false;

    return it->second.hasVaginal || it->second.hasAnal;
  }

  float GetProximityPenalty(const Candidate& candidate,
                            const std::unordered_map<RE::Actor*, ActorInteractSummary>& summaries,
                            const std::unordered_map<RE::Actor*, ActorData>& datas)
  {
    if (!IsPenetrationSensitiveProximity(candidate.type))
      return 0.f;

    const auto summaryIt      = summaries.find(candidate.targetActor);
    const bool currentVaginal = summaryIt != summaries.end() && summaryIt->second.hasVaginal;
    const bool currentAnal    = summaryIt != summaries.end() && summaryIt->second.hasAnal;

    const auto dataIt      = datas.find(candidate.targetActor);
    const bool prevVaginal = dataIt != datas.end() && HasPreviousVaginalContext(dataIt->second);
    const bool prevAnal    = dataIt != datas.end() && HasPreviousAnalContext(dataIt->second);
    const bool anyCurrent  = currentVaginal || currentAnal;
    const bool anyPrevious = prevVaginal || prevAnal;
    float penalty          = 0.f;

    if (anyCurrent)
      penalty += 0.22f;
    if (anyPrevious)
      penalty += 0.10f;

    if (candidate.type == Type::Frottage && (currentVaginal || prevVaginal))
      penalty += 0.16f;
    else if (candidate.type == Type::Thighjob && (anyCurrent || anyPrevious))
      penalty += 0.10f;
    else if (candidate.type == Type::Naveljob && (anyCurrent || anyPrevious))
      penalty += 0.14f;

    return penalty;
  }

  bool
  ShouldSuppressRecentPenetrationFallback(const Candidate& candidate,
                                          const std::unordered_map<RE::Actor*, ActorData>& datas)
  {
    if (!IsPenetrationSensitiveProximity(candidate.type) || candidate.sourcePart != Name::Penis)
      return false;

    const auto sourceIt = datas.find(candidate.sourceActor);
    const auto targetIt = datas.find(candidate.targetActor);
    if (sourceIt == datas.end() || targetIt == datas.end())
      return false;

    const bool sourceRecent =
        HadPreviousPenetrationWithPartner(sourceIt->second, Name::Penis, candidate.targetActor);
    const bool targetRecent =
        HadPreviousPenetrationWithPartner(targetIt->second, Name::Vagina, candidate.sourceActor) ||
        HadPreviousPenetrationWithPartner(targetIt->second, Name::Anus, candidate.sourceActor);
    if (!sourceRecent && !targetRecent)
      return false;

    return TryGetBodyPart(targetIt->second, Name::Vagina) ||
           TryGetBodyPart(targetIt->second, Name::Anus);
  }

  bool TryAssignCandidate(const Candidate& candidate, UsedPartSet& usedParts, AssignMap& assigns)
  {
    const bool sourceUsed = usedParts.count({candidate.sourceActor, candidate.sourcePart}) > 0;
    const bool targetUsed = usedParts.count({candidate.targetActor, candidate.targetPart}) > 0;
    if (sourceUsed || targetUsed) {
      if (candidate.family == CandidateFamily::Penetration &&
          ShouldEmitPenetrationDiag(candidate.sourceActor, candidate.targetActor, candidate.type,
                                    PenetrationDiagEvent::AssignSkipped, 200.f)) {
        const AssignInfo* sourceAssign =
            FindAssignInfo(assigns, candidate.sourceActor, candidate.sourcePart);
        const AssignInfo* targetAssign =
            FindAssignInfo(assigns, candidate.targetActor, candidate.targetPart);
        logger::info(
            "[SexLab NG] Interact PenetrationAssign skipped type={} src={:08X}:{} dst={:08X}:{} "
            "score={:.3f} distance={:.2f} sourceUsed={} sourceBy={} sourcePartner={:08X} "
            "targetUsed={} targetBy={} targetPartner={:08X}",
            magic_enum::enum_name(candidate.type), GetActorFormID(candidate.sourceActor),
            magic_enum::enum_name(candidate.sourcePart), GetActorFormID(candidate.targetActor),
            magic_enum::enum_name(candidate.targetPart), candidate.score, candidate.distance,
            sourceUsed, magic_enum::enum_name(sourceAssign ? sourceAssign->type : Type::None),
            GetActorFormID(sourceAssign ? sourceAssign->partner : nullptr), targetUsed,
            magic_enum::enum_name(targetAssign ? targetAssign->type : Type::None),
            GetActorFormID(targetAssign ? targetAssign->partner : nullptr));
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

    if (candidate.family == CandidateFamily::Penetration &&
        ShouldEmitPenetrationDiag(candidate.sourceActor, candidate.targetActor, candidate.type,
                                  PenetrationDiagEvent::Assigned, 200.f)) {
      logger::info("[SexLab NG] Interact PenetrationAssign applied type={} src={:08X}:{} "
                   "dst={:08X}:{} resolvedType={} score={:.3f} distance={:.2f}",
                   magic_enum::enum_name(candidate.type), GetActorFormID(candidate.sourceActor),
                   magic_enum::enum_name(candidate.sourcePart),
                   GetActorFormID(candidate.targetActor),
                   magic_enum::enum_name(candidate.targetPart), magic_enum::enum_name(resolvedType),
                   candidate.score, candidate.distance);
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
        if (ShouldSuppressProximityCandidate(adjusted, summaries))
          continue;
        if (ShouldSuppressRecentPenetrationFallback(adjusted, datas))
          continue;
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

}  // namespace

Interact::Interact(std::vector<RE::Actor*> actors)
{
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

void Interact::Update()
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

  for (auto& [actor, data] : datas)
    for (auto& [name, info] : data.infos)
      info.bodypart.UpdatePosition();

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

  AssignFamilyCandidates(candidates, CandidateFamily::Penetration, datas, usedParts, assigns);
  AssignFamilyCandidates(candidates, CandidateFamily::Oral, datas, usedParts, assigns);
  AssignFamilyCandidates(candidates, CandidateFamily::Proximity, datas, usedParts, assigns);
  AssignFamilyCandidates(candidates, CandidateFamily::Contact, datas, usedParts, assigns);

  std::unordered_map<RE::Actor*, ActorInteractSummary> summaries;
  BuildSummaries(assigns, summaries);

  std::unordered_map<RE::Actor*, std::vector<Type>> comboTypes;
  BuildComboTypes(summaries, comboTypes);
  ApplyComboOverrides(assigns, summaries, comboTypes);
  LogPenetrationFinalStates(datas, assigns);

  const float nowMs = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(
                                             std::chrono::steady_clock::now().time_since_epoch())
                                             .count()) /
                      1000.f;

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

const Interact::Info& Interact::GetInfo(RE::Actor* actor, Name partName) const
{
  static const Info kEmpty{};
  auto it = datas.find(actor);
  if (it == datas.end())
    return kEmpty;
  auto partIt = it->second.infos.find(partName);
  if (partIt == it->second.infos.end())
    return kEmpty;
  return partIt->second;
}

}  // namespace Instance

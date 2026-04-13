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
    Manual      = 2,
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

  struct OralContactMetrics
  {
    Define::Point3f point = Define::Point3f::Zero();
    float axisDistance    = std::numeric_limits<float>::infinity();
    float depth           = 0.f;
  };

  OralContactMetrics MeasureOralContactPoint(const Define::Point3f& point,
                                             const Define::Point3f& oralEntry,
                                             const Define::Point3f& oralAxisEnd)
  {
    return OralContactMetrics{point, DistancePointToSegment(point, oralEntry, oralAxisEnd),
                              ProjectionOnSegmentAxis(point, oralEntry, oralAxisEnd)};
  }

  OralContactMetrics SelectOralContactMetrics(const BP& penisPart, const Define::Point3f& oralEntry,
                                              const Define::Point3f& oralAxisEnd)
  {
    OralContactMetrics best = MeasureOralContactPoint(penisPart.GetEnd(), oralEntry, oralAxisEnd);

    if (penisPart.GetLength() < 1e-6f)
      return best;

    const float sampleBackDistance = std::clamp(penisPart.GetLength() * 0.30f, 1.2f, 3.5f);
    const Define::Point3f nearTipPoint =
        penisPart.GetEnd() - penisPart.GetDirection() * sampleBackDistance;
    const OralContactMetrics nearTip =
        MeasureOralContactPoint(nearTipPoint, oralEntry, oralAxisEnd);

    if (nearTip.axisDistance + 0.15f < best.axisDistance ||
        (std::abs(nearTip.axisDistance - best.axisDistance) <= 0.15f &&
         nearTip.depth > best.depth)) {
      best = nearTip;
    }

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

  float MinDistanceToOralRegion(const ActorData& data, const BP& part)
  {
    float result = std::numeric_limits<float>::infinity();
    if (const BP* mouthPart = TryGetBodyPart(data, Name::Mouth))
      result = (std::min)(result, part.Distance(*mouthPart));
    if (const BP* throatPart = TryGetBodyPart(data, Name::Throat))
      result = (std::min)(result, part.Distance(*throatPart));
    return result;
  }

  float ProjectionOnAxis(const Define::Point3f& point, const BP& axisPart)
  {
    if (axisPart.GetType() == BP::Type::Point || axisPart.GetLength() < 1e-6f)
      return 0.f;
    return (point - axisPart.GetStart()).dot(axisPart.GetDirection());
  }

  float DistanceToAxisSegment(const Define::Point3f& point, const BP& axisPart)
  {
    if (axisPart.GetType() == BP::Type::Point || axisPart.GetLength() < 1e-6f)
      return (point - axisPart.GetStart()).norm();

    const float t      = std::clamp((point - axisPart.GetStart()).dot(axisPart.GetDirection()), 0.f,
                                    axisPart.GetLength());
    const auto closest = axisPart.GetStart() + axisPart.GetDirection() * t;
    return (point - closest).norm();
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

    const float vagSupport =
        thighCleft ? thighCleft->Distance(penisPart) : std::numeric_limits<float>::infinity();
    const float analSupport =
        glutealPart ? glutealPart->Distance(penisPart) : std::numeric_limits<float>::infinity();
    const float vagEntryDistance =
        vaginaPart ? AnchorDistance(penisPart, *vaginaPart, AnchorMode::StartToStart)
                   : std::numeric_limits<float>::infinity();
    const float analEntryDistance =
        anusPart ? AnchorDistance(penisPart, *anusPart, AnchorMode::StartToStart)
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
    const BP* thighCleft  = TryGetBodyPart(receiverData, Name::ThighCleft);
    const BP* glutealPart = TryGetBodyPart(receiverData, Name::GlutealCleft);
    const float vagSupport =
        thighCleft ? thighCleft->Distance(penisPart) : std::numeric_limits<float>::infinity();
    const float analSupport =
        glutealPart ? glutealPart->Distance(penisPart) : std::numeric_limits<float>::infinity();

    if (!std::isfinite(vagSupport))
      return false;
    if (rootEntryDistance <= 3.6f)
      return !std::isfinite(analSupport) || vagSupport <= analSupport + 1.25f;

    const bool frontContext = !std::isfinite(analSupport) || vagSupport <= analSupport + 1.25f;
    const bool strongFrontContext =
        !std::isfinite(analSupport) || vagSupport + 0.75f <= analSupport;

    if (stickyPair && rootEntryDistance <= 8.2f && partDistance <= 9.6f && frontContext)
      return true;
    if (rootEntryDistance <= 6.8f && partDistance <= 8.8f && strongFrontContext)
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

        const float distance = sourcePart->Distance(*targetPart);
        if (distance > maxDistance)
          continue;

        const float angle = sourcePart->Angle(*targetPart);
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

    const float distance = lhs->Distance(*rhs);
    if (distance > maxDistance)
      return;

    const float angle = lhs->Angle(*rhs);
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

    const BP& penis = *TryGetBodyPart(ctx.sourceData, Name::Penis);
    const BP& belly = *TryGetBodyPart(ctx.targetData, Name::Belly);

    const float distance = belly.Distance(penis);
    if (distance > 10.5f)
      return;

    const float angle = belly.Angle(penis);
    if (angle > 60.f)
      return;

    if (!belly.IsInFront(penis.GetEnd()) || !penis.IsHorizontal())
      return;

    const float distNorm  = NormalizeDistance(distance, 10.5f);
    const float angleNorm = AngleDeviation(angle, {0.f, 60.f});
    const float bonus =
        TemporalBonus(ctx.sourceData, Name::Penis, ctx.targetActor, Type::Naveljob) +
        TemporalBonus(ctx.targetData, Name::Belly, ctx.sourceActor, Type::Naveljob);
    const float score = (std::max)(0.f, distNorm * 0.55f + angleNorm * 0.45f - bonus);

    AddCandidate(out, ctx, Name::Penis, Name::Belly, Type::Naveljob, CandidateFamily::Contact,
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
    constexpr float kMaxAngle          = 70.f;
    constexpr float kStickyMaxAngle    = 78.f;
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
    const float mouthDistance = mouthPart->Distance(*penisPart);
    const float entryDistance = (oralContact.point - oralEntry).norm();
    const float distance      = (std::min)(mouthDistance, entryDistance);
    if (distance > effectiveMaxDistance)
      return;

    const float angle = mouthPart->Angle(*penisPart);
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

      const float distance = handPart->Distance(*penisPart);
      if (distance > kMaxDistance)
        continue;

      const float angle = handPart->Angle(*penisPart);
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

      AddCandidate(out, ctx, handName, Name::Penis, Type::Handjob, CandidateFamily::Manual,
                   distance, score);
    }
  }

  void TryAddPenetrationCandidate(std::vector<Candidate>& out, const DirectedContext& ctx,
                                  Name receiverPartName, Type type, float maxDistance,
                                  float maxAngle, float maxTipAxis, float minDepth, float maxDepth)
  {
    const BP* penisPart    = TryGetBodyPart(ctx.sourceData, Name::Penis);
    const BP* receiverPart = TryGetBodyPart(ctx.targetData, receiverPartName);
    if (!penisPart || !receiverPart)
      return;

    const float distance = receiverPart->Distance(*penisPart);
    const float rootEntryDistance =
        AnchorDistance(*penisPart, *receiverPart, AnchorMode::StartToStart);
    const bool stickyPair = HasStickyInteractionPair(ctx, Name::Penis, receiverPartName, type);
    const bool relaxVaginalGate =
        type == Type::Vaginal &&
        ShouldRelaxVaginalGate(ctx.targetData, *penisPart, rootEntryDistance, distance, stickyPair);
    const float effectiveMaxDistance =
        relaxVaginalGate ? (std::max)(maxDistance, stickyPair ? 9.6f : 8.8f) : maxDistance;
    const float effectiveMaxAngle =
        relaxVaginalGate ? (std::max)(maxAngle, stickyPair ? 68.f : 64.f) : maxAngle;
    const float effectiveMaxTipAxis = relaxVaginalGate ? (std::max)(maxTipAxis, 10.f) : maxTipAxis;
    const float effectiveMinDepth   = relaxVaginalGate ? (std::min)(minDepth, -5.5f) : minDepth;
    const float effectiveMaxDepth   = relaxVaginalGate ? (std::max)(maxDepth, 14.f) : maxDepth;

    if (distance > effectiveMaxDistance)
      return;

    const float angle = receiverPart->Angle(*penisPart);
    if (angle > effectiveMaxAngle)
      return;

    const float tipAxisDistance = DistanceToAxisSegment(penisPart->GetEnd(), *receiverPart);
    if (tipAxisDistance > effectiveMaxTipAxis)
      return;

    const float depth = ProjectionOnAxis(penisPart->GetEnd(), *receiverPart);
    if (depth < effectiveMinDepth || depth > effectiveMaxDepth)
      return;

    const float distNorm  = NormalizeDistance(distance, effectiveMaxDistance);
    const float angleNorm = AngleDeviation(angle, {0.f, effectiveMaxAngle});
    const float axisNorm  = NormalizeDistance(tipAxisDistance, effectiveMaxTipAxis);

    float depthPenalty = 0.f;
    if (depth < 0.f)
      depthPenalty = std::abs(depth) / (std::max)(std::abs(effectiveMinDepth), 1e-4f);
    else if (depth > receiverPart->GetLength())
      depthPenalty = (depth - receiverPart->GetLength()) /
                     (std::max)(effectiveMaxDepth - receiverPart->GetLength(), 1e-4f);

    const float contextBias = CalculatePenetrationContextBias(ctx.targetData, *penisPart, type);
    const float bonus = TemporalBonus(ctx.sourceData, Name::Penis, ctx.targetActor, type) +
                        TemporalBonus(ctx.targetData, receiverPartName, ctx.sourceActor, type);
    constexpr float kPenetrationPriorityBias = 0.10f;

    const float score = (std::max)(0.f, distNorm * 0.42f + angleNorm * 0.22f + axisNorm * 0.22f +
                                            std::clamp(depthPenalty, 0.f, 1.f) * 0.14f +
                                            contextBias - bonus - kPenetrationPriorityBias);

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

    AddSimpleDirectedCandidates(out, ctx, kHandParts, kBreastParts, Type::GropeBreast,
                                CandidateFamily::Manual, 6.8f, {140.f, 180.f}, 0.24f, 0.f,
                                AnchorMode::StartToStart, 0.16f, 6.2f);
    AddSimpleDirectedCandidates(out, ctx, kHandParts, kThighParts, Type::GropeThigh,
                                CandidateFamily::Manual, 7.1f, {0.f, 180.f}, 0.f, 0.f,
                                AnchorMode::StartToStart, 0.14f, 6.4f);
    AddSimpleDirectedCandidates(out, ctx, kHandParts, kButtParts, Type::GropeButt,
                                CandidateFamily::Manual, 6.0f, {0.f, 180.f}, 0.f, 0.f,
                                AnchorMode::StartToStart, 0.16f, 5.5f);
    AddSimpleDirectedCandidates(out, ctx, kHandParts, kFootParts, Type::GropeFoot,
                                CandidateFamily::Manual, 7.0f, {0.f, 180.f}, 0.f, 0.f,
                                AnchorMode::StartToStart, 0.12f, 6.4f);

    AddSimpleDirectedCandidates(out, ctx, kFingerParts, kThighCleftParts, Type::FingerVagina,
                                CandidateFamily::Manual, 5.9f, {0.f, 180.f}, 0.f, -0.04f,
                                AnchorMode::StartToStart, 0.18f, 5.6f);
    AddSimpleDirectedCandidates(out, ctx, kFingerParts, kVaginaParts, Type::FingerVagina,
                                CandidateFamily::Manual, 6.8f, {0.f, 180.f}, 0.f, 0.01f,
                                AnchorMode::StartToStart, 0.22f, 6.2f);
    AddSimpleDirectedCandidates(out, ctx, kFingerParts, kAnusParts, Type::FingerAnus,
                                CandidateFamily::Manual, 6.3f, {0.f, 180.f}, 0.f, 0.f,
                                AnchorMode::StartToStart, 0.20f, 5.8f);
    TryAddHandjobCandidates(out, ctx);
  }

  void GenerateContactDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (!HasAnyPart(ctx.sourceData, kPenisParts))
      return;

    AddSimpleDirectedCandidates(out, ctx, kPenisParts, kBreastParts, Type::Titfuck,
                                CandidateFamily::Contact, 8.f, {0.f, 58.f}, 0.32f);
    AddSimpleDirectedCandidates(out, ctx, kPenisParts, kCleavageParts, Type::Titfuck,
                                CandidateFamily::Contact, 8.f, {70.f, 110.f}, 0.40f, -0.03f);
    TryAddNaveljobCandidate(out, ctx);
    AddSimpleDirectedCandidates(out, ctx, kPenisParts, kThighCleftParts, Type::Thighjob,
                                CandidateFamily::Contact, 6.5f, {0.f, 32.f}, 0.24f, 0.02f);
    AddSimpleDirectedCandidates(out, ctx, kPenisParts, kThighParts, Type::Thighjob,
                                CandidateFamily::Contact, 5.4f, {0.f, 32.f}, 0.22f, 0.01f);
    AddSimpleDirectedCandidates(out, ctx, kPenisParts, kGlutealCleftParts, Type::Frottage,
                                CandidateFamily::Contact, 8.f, {140.f, 40.f}, 0.28f);
    AddSimpleDirectedCandidates(out, ctx, kPenisParts, kFootParts, Type::Footjob,
                                CandidateFamily::Contact, 8.2f, {0.f, 180.f}, 0.f);
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
                             CandidateFamily::Contact, 8.f, {140.f, 180.f}, 0.32f);
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

  std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
    if (lhs.score != rhs.score)
      return lhs.score < rhs.score;
    if (lhs.family != rhs.family)
      return static_cast<std::uint8_t>(lhs.family) < static_cast<std::uint8_t>(rhs.family);
    if (lhs.distance != rhs.distance)
      return lhs.distance < rhs.distance;
    return static_cast<std::uint8_t>(lhs.type) < static_cast<std::uint8_t>(rhs.type);
  });

  using PartKey = std::pair<RE::Actor*, Name>;
  struct PartKeyHash
  {
    std::size_t operator()(const PartKey& key) const
    {
      return std::hash<RE::Actor*>{}(key.first) ^ (static_cast<std::size_t>(key.second) << 8);
    }
  };

  std::unordered_set<PartKey, PartKeyHash> usedParts;
  AssignMap assigns;

  for (const Candidate& candidate : candidates) {
    if (usedParts.count({candidate.sourceActor, candidate.sourcePart}) ||
        usedParts.count({candidate.targetActor, candidate.targetPart})) {
      continue;
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
  }

  std::unordered_map<RE::Actor*, ActorInteractSummary> summaries;
  BuildSummaries(assigns, summaries);

  std::unordered_map<RE::Actor*, std::vector<Type>> comboTypes;
  BuildComboTypes(summaries, comboTypes);
  ApplyComboOverrides(assigns, summaries, comboTypes);

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

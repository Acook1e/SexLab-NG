#include "Instance/Interact.h"

#include "Utils/Settings.h"

namespace Instance
{

namespace
{
  using PartName   = Define::ActorBody::PartName;
  using Point3f    = Define::Point3f;
  using Vector3f   = Define::Vector3f;
  using VolumeData = Define::VolumeData;
  using Capsule    = Define::Capsule;
  using Funnel     = Define::Funnel;
  using HalfBall   = Define::HalfBall;
  using EnvelopeHb = Define::Envelope<Define::HalfBall>;
  using EnvelopeCp = Define::Envelope<Define::Capsule>;
  using Spline     = Define::SplineCapsule;
  using Surface    = Define::SurfaceVolume;

  namespace InteractTuning = Settings::Interact;

  struct SourceProbe
  {
    Point3f point      = Point3f::Zero();
    Vector3f direction = Vector3f::Zero();
  };

  constexpr std::array<PartName, 21> kAllParts{
      PartName::Mouth,       PartName::Throat,    PartName::BreastLeft, PartName::BreastRight,
      PartName::Cleavage,    PartName::HandLeft,  PartName::HandRight,  PartName::FingerLeft,
      PartName::FingerRight, PartName::Belly,     PartName::ThighLeft,  PartName::ThighRight,
      PartName::ThighCleft,  PartName::ButtLeft,  PartName::ButtRight,  PartName::GlutealCleft,
      PartName::FootLeft,    PartName::FootRight, PartName::Vagina,     PartName::Anus,
      PartName::Penis,
  };

  float Clamp01(float value)
  {
    return std::clamp(value, 0.f, 1.f);
  }

  Vector3f SafeNormalized(const Vector3f& value, const Vector3f& fallback = Vector3f::UnitZ())
  {
    const float length = value.norm();
    if (length <= Define::kEpsilon) {
      return fallback;
    }
    return value / length;
  }

  float PositiveAlignment(const Vector3f& lhs, const Vector3f& rhs)
  {
    if (lhs.squaredNorm() <= Define::kEpsilon || rhs.squaredNorm() <= Define::kEpsilon) {
      return 0.f;
    }
    return Clamp01(SafeNormalized(lhs).dot(SafeNormalized(rhs)));
  }

  float AbsoluteAlignment(const Vector3f& lhs, const Vector3f& rhs)
  {
    if (lhs.squaredNorm() <= Define::kEpsilon || rhs.squaredNorm() <= Define::kEpsilon) {
      return 0.f;
    }
    return Clamp01(std::abs(SafeNormalized(lhs).dot(SafeNormalized(rhs))));
  }

  float ContactFactor(float signedDistance, float scale)
  {
    const float positiveDistance = (std::max)(0.f, signedDistance);
    return Clamp01(1.f - positiveDistance / (std::max)(scale, 0.001f));
  }

  PartName SlotToPart(SlotType slot)
  {
    switch (slot) {
    case SlotType::Mouth:
      return PartName::Mouth;
    case SlotType::Throat:
      return PartName::Throat;
    case SlotType::Vagina:
      return PartName::Vagina;
    case SlotType::Anus:
      return PartName::Anus;
    case SlotType::BreastLeft:
      return PartName::BreastLeft;
    case SlotType::BreastRight:
      return PartName::BreastRight;
    case SlotType::Cleavage:
      return PartName::Cleavage;
    case SlotType::Belly:
      return PartName::Belly;
    case SlotType::ThighCleft:
      return PartName::ThighCleft;
    case SlotType::GlutealCleft:
      return PartName::GlutealCleft;
    case SlotType::FootLeft:
      return PartName::FootLeft;
    case SlotType::FootRight:
      return PartName::FootRight;
    }

    return PartName::Mouth;
  }

  bool TryPartToSlot(PartName part, SlotType& outSlot)
  {
    switch (part) {
    case PartName::Mouth:
      outSlot = SlotType::Mouth;
      return true;
    case PartName::Throat:
      outSlot = SlotType::Throat;
      return true;
    case PartName::BreastLeft:
      outSlot = SlotType::BreastLeft;
      return true;
    case PartName::BreastRight:
      outSlot = SlotType::BreastRight;
      return true;
    case PartName::Cleavage:
      outSlot = SlotType::Cleavage;
      return true;
    case PartName::Belly:
      outSlot = SlotType::Belly;
      return true;
    case PartName::ThighCleft:
      outSlot = SlotType::ThighCleft;
      return true;
    case PartName::GlutealCleft:
      outSlot = SlotType::GlutealCleft;
      return true;
    case PartName::FootLeft:
      outSlot = SlotType::FootLeft;
      return true;
    case PartName::FootRight:
      outSlot = SlotType::FootRight;
      return true;
    case PartName::Vagina:
      outSlot = SlotType::Vagina;
      return true;
    case PartName::Anus:
      outSlot = SlotType::Anus;
      return true;
    default:
      return false;
    }
  }

  PartName SourcePartForResource(ResourceType resource, InteractionType type)
  {
    switch (resource) {
    case ResourceType::Mouth:
      return PartName::Mouth;
    case ResourceType::LeftHand:
      return type == InteractionType::FingerVagina || type == InteractionType::FingerAnus
                 ? PartName::FingerLeft
                 : PartName::HandLeft;
    case ResourceType::RightHand:
      return type == InteractionType::FingerVagina || type == InteractionType::FingerAnus
                 ? PartName::FingerRight
                 : PartName::HandRight;
    case ResourceType::Penis:
      return PartName::Penis;
    }

    return PartName::Mouth;
  }

  bool AccumulateObservedType(Define::InteractTags& tags, InteractionType type)
  {
    using TagType = Define::InteractTags::Type;

    switch (type) {
    case InteractionType::Kiss:
      tags.Set(TagType::Kiss);
      return true;
    case InteractionType::BreastSucking:
      tags.Set(TagType::BreastSucking);
      return true;
    case InteractionType::ToeSucking:
      tags.Set(TagType::ToeSucking);
      return true;
    case InteractionType::Cunnilingus:
      tags.Set(TagType::Cunnilingus);
      return true;
    case InteractionType::Anilingus:
      tags.Set(TagType::Anilingus);
      return true;
    case InteractionType::Fellatio:
      tags.Set(TagType::Fellatio);
      return true;
    case InteractionType::DeepThroat:
      tags.Set(TagType::DeepThroat);
      return true;
    case InteractionType::GropeBreast:
      tags.Set(TagType::GropeBreast);
      return true;
    case InteractionType::GropeThigh:
      tags.Set(TagType::GropeThigh);
      return true;
    case InteractionType::GropeButt:
      tags.Set(TagType::GropeButt);
      return true;
    case InteractionType::GropeFoot:
      tags.Set(TagType::GropeFoot);
      return true;
    case InteractionType::FingerVagina:
      tags.Set(TagType::FingerVagina);
      return true;
    case InteractionType::FingerAnus:
      tags.Set(TagType::FingerAnus);
      return true;
    case InteractionType::Handjob:
      tags.Set(TagType::Handjob);
      return true;
    case InteractionType::Masturbation:
      tags.Set(TagType::Masturbation);
      return true;
    case InteractionType::Titfuck:
      tags.Set(TagType::Titfuck);
      return true;
    case InteractionType::Naveljob:
      tags.Set(TagType::Naveljob);
      return true;
    case InteractionType::Thighjob:
      tags.Set(TagType::Thighjob);
      return true;
    case InteractionType::Frottage:
      tags.Set(TagType::Frottage);
      return true;
    case InteractionType::Footjob:
      tags.Set(TagType::Footjob);
      return true;
    case InteractionType::Vaginal:
      tags.Set(TagType::Vaginal);
      return true;
    case InteractionType::Anal:
      tags.Set(TagType::Anal);
      return true;
    case InteractionType::Tribbing:
      tags.Set(TagType::Tribbing);
      return true;
    case InteractionType::SixtyNine:
      tags.Set(TagType::SixtyNine);
      return true;
    case InteractionType::Spitroast:
      tags.Set(TagType::Spitroast);
      return true;
    case InteractionType::DoublePenetration:
      tags.Set(TagType::DoublePenetration);
      return true;
    case InteractionType::TriplePenetration:
      tags.Set(TagType::TriplePenetration);
      return true;
    case InteractionType::None:
    case InteractionType::Count:
      return false;
    }

    return false;
  }

  float ConfidenceThresholdFor(InteractionType type)
  {
    switch (type) {
    case InteractionType::Kiss:
      return InteractTuning::fProposalThresholdKiss;
    case InteractionType::GropeBreast:
    case InteractionType::GropeThigh:
    case InteractionType::GropeButt:
    case InteractionType::GropeFoot:
      return InteractTuning::fProposalThresholdManual;
    case InteractionType::BreastSucking:
    case InteractionType::ToeSucking:
    case InteractionType::Cunnilingus:
    case InteractionType::Anilingus:
    case InteractionType::FingerVagina:
    case InteractionType::FingerAnus:
    case InteractionType::Titfuck:
    case InteractionType::Naveljob:
    case InteractionType::Thighjob:
    case InteractionType::Frottage:
    case InteractionType::Footjob:
      return InteractTuning::fProposalThresholdContact;
    case InteractionType::Fellatio:
    case InteractionType::DeepThroat:
    case InteractionType::Vaginal:
    case InteractionType::Anal:
      return InteractTuning::fProposalThresholdPenetration;
    default:
      return InteractTuning::fProposalThresholdDefault;
    }
  }

  const PartRuntime* TryGetRuntime(const ActorInteractState& state, PartName part)
  {
    auto it = state.parts.find(part);
    return it == state.parts.end() ? nullptr : &it->second;
  }

  PartRuntime* TryGetRuntime(ActorInteractState& state, PartName part)
  {
    auto it = state.parts.find(part);
    return it == state.parts.end() ? nullptr : &it->second;
  }

  IntentPrior BuildPrior(const ActorInteractState& sourceState, const PartRuntime& sourcePart,
                         const ActorInteractState& targetState, const PartRuntime& targetPart,
                         InteractionType type)
  {
    IntentPrior prior;

    if (sourcePart.interaction.active && sourcePart.interaction.partner == targetState.actor &&
        sourcePart.interaction.type == type) {
      prior.historyBonus += InteractTuning::fPriorHistoryBonusBase;
    }

    if (targetPart.interaction.active && targetPart.interaction.partner == sourceState.actor &&
        targetPart.interaction.type == type) {
      prior.historyBonus += InteractTuning::fPriorHistoryBonusBase;
    }

    if (sourcePart.motion.directional && targetPart.motion.valid) {
      const Vector3f toTarget = targetPart.motion.start - sourcePart.motion.end;
      if (toTarget.squaredNorm() > Define::kEpsilon) {
        prior.trajectoryBonus = InteractTuning::fPriorTrajectoryBonusWeight *
                                PositiveAlignment(sourcePart.motion.direction, toTarget);
      }
    }

    return prior;
  }

  SourceProbe MakeProbe(const Capsule& source, float t)
  {
    t = Clamp01(t);
    return {source.worldStart + (source.worldEnd - source.worldStart) * t, source.worldDirection};
  }

  SourceProbe MakeProbe(const Spline& source, float t)
  {
    t = Clamp01(t);
    return {source.SamplePoint(t), source.SampleTangent(t)};
  }

  template <typename T>
  std::array<SourceProbe, 3> MakeProbes(const T& source)
  {
    return {MakeProbe(source, 0.2f), MakeProbe(source, 0.65f), MakeProbe(source, 1.0f)};
  }

  template <typename T>
  GeometricEvidence EvaluateAgainstHalfBall(const T& source, const HalfBall& target)
  {
    GeometricEvidence best{};
    float bestContact = -1.f;

    for (const auto& probe : MakeProbes(source)) {
      const auto projection = Define::ProjectPointOntoVolume(target, probe.point);
      const float contact   = ContactFactor(projection.signedDistance,
                                            target.radius + InteractTuning::fHalfBallContactSlack);
      if (contact <= bestContact) {
        continue;
      }

      bestContact           = contact;
      best.surfaceContact   = contact;
      best.enclosureFactor  = projection.inside ? 1.f : contact;
      best.alignmentQuality = AbsoluteAlignment(probe.direction, target.worldDirection);
      best.radialDeviation  = projection.radialDistance / (target.radius + 0.001f);
      best.entryDistance    = projection.signedDistance;
      best.depth            = projection.inside ? target.radius - projection.radialDistance : 0.f;
      best.maxDepth         = target.radius;
      best.corePenetration  = Clamp01(best.depth / (target.radius + 0.001f));
    }

    return best;
  }

  template <typename TEnvelope, typename TSource>
  GeometricEvidence EvaluateAgainstEnvelope(const TSource& source, const TEnvelope& target)
  {
    GeometricEvidence best{};
    float bestContact = -1.f;

    for (const auto& probe : MakeProbes(source)) {
      const auto projection = Define::ProjectPointOntoEnvelope(target, probe.point);
      const float contact =
          ContactFactor(projection.projection.signedDistance,
                        target.runtime.typicalWidth + InteractTuning::fEnvelopeContactSlack);
      if (contact <= bestContact) {
        continue;
      }

      bestContact           = contact;
      best.surfaceContact   = contact;
      best.enclosureFactor  = projection.enclosureFactor;
      best.alignmentQuality = AbsoluteAlignment(probe.direction, target.runtime.direction);
      best.radialDeviation  = projection.localWidth > Define::kEpsilon
                                  ? projection.projection.radialDistance / projection.localWidth
                                  : projection.projection.radialDistance;
      best.entryDistance    = projection.projection.signedDistance;
      best.depth            = projection.projection.t * target.runtime.totalLength;
      best.maxDepth         = target.runtime.totalLength;
      best.corePenetration  = Clamp01(best.surfaceContact * 0.5f + best.enclosureFactor * 0.5f);
    }

    return best;
  }

  template <typename T>
  GeometricEvidence EvaluateAgainstSurface(const T& source, const Surface& target)
  {
    GeometricEvidence best{};
    float bestContact = -1.f;

    for (const auto& probe : MakeProbes(source)) {
      const auto projection = Define::ProjectPointOntoSurface(target, probe.point);
      const float distance  = projection.planeDistance + projection.planarDistance;
      const float contact =
          projection.insideSurface
              ? 1.f
              : ContactFactor(distance, InteractTuning::fSurfaceContactDistanceScale);
      if (contact <= bestContact) {
        continue;
      }

      bestContact           = contact;
      best.surfaceContact   = contact;
      best.enclosureFactor  = projection.insideSurface ? 1.f : 0.f;
      best.alignmentQuality = AbsoluteAlignment(probe.direction, target.worldDirection);
      best.radialDeviation  = projection.planarDistance;
      best.entryDistance    = projection.planeDistance;
      best.depth = projection.insideSurface ? (std::max)(0.f, 1.f - projection.planeDistance) : 0.f;
      best.maxDepth        = 1.f;
      best.corePenetration = projection.insideSurface ? contact : 0.f;
    }

    return best;
  }

  template <typename T>
  GeometricEvidence EvaluateAgainstFunnel(const T& source, const Funnel& target)
  {
    GeometricEvidence best{};
    float bestScore = -1.f;

    for (const auto& probe : MakeProbes(source)) {
      const auto projection = Define::ProjectPointOntoFunnel(target, probe.point);
      const float localRadius =
          (std::max)(projection.localRadius, InteractTuning::fFunnelMinimumLocalRadius);
      const float contact = ContactFactor(projection.projection.signedDistance,
                                          localRadius + InteractTuning::fFunnelContactSlack);
      const float score =
          contact + Clamp01(projection.normalizedDepth) * InteractTuning::fFunnelDepthScoreBonus;
      if (score <= bestScore) {
        continue;
      }

      bestScore             = score;
      best.corePenetration  = projection.insideCore ? 1.f : Clamp01(projection.normalizedDepth);
      best.surfaceContact   = contact;
      best.enclosureFactor  = projection.insideCapture ? contact : 0.f;
      best.alignmentQuality = PositiveAlignment(probe.direction, target.worldDirection);
      best.radialDeviation  = projection.projection.radialDistance / localRadius;
      best.entryDistance    = projection.depth;
      best.depth            = projection.depth;
      best.maxDepth         = target.worldLength;
    }

    return best;
  }

  template <typename T>
  GeometricEvidence EvaluateGeneric(const T& source, const VolumeData& target)
  {
    GeometricEvidence best{};
    float bestContact = -1.f;

    for (const auto& probe : MakeProbes(source)) {
      const auto projection = Define::ProjectPointOntoVolume(target, probe.point);
      const float contact =
          ContactFactor(projection.signedDistance, InteractTuning::fGenericContactDistanceScale);
      if (contact <= bestContact) {
        continue;
      }

      bestContact           = contact;
      best.surfaceContact   = contact;
      best.enclosureFactor  = projection.inside ? 1.f : 0.f;
      best.alignmentQuality = AbsoluteAlignment(probe.direction, projection.direction);
      best.radialDeviation  = projection.radialDistance;
      best.entryDistance    = projection.signedDistance;
      best.depth            = projection.inside ? contact : 0.f;
      best.maxDepth         = 1.f;
      best.corePenetration  = projection.inside ? contact : 0.f;
    }

    return best;
  }

  InteractionSnapshot MakeInteraction(RE::Actor* partner, InteractionType type, float distance,
                                      float approachSpeed)
  {
    InteractionSnapshot snapshot;
    snapshot.partner       = partner;
    snapshot.type          = type;
    snapshot.distance      = distance;
    snapshot.approachSpeed = approachSpeed;
    snapshot.active        = type != InteractionType::None;
    return snapshot;
  }

  float EstimateDistance(const GeometricEvidence& evidence)
  {
    if (evidence.maxDepth > Define::kEpsilon && evidence.depth > 0.f) {
      return (std::max)(0.f, evidence.maxDepth - evidence.depth);
    }
    if (evidence.entryDistance != 0.f) {
      return std::abs(evidence.entryDistance);
    }
    return evidence.radialDeviation;
  }

  void BindActorParts(ActorInteractState& state)
  {
    for (PartName part : kAllParts) {
      const auto* volume = state.body.GetPart(part);
      if (!volume) {
        state.parts.erase(part);
        continue;
      }

      auto [it, inserted] = state.parts.try_emplace(part);
      (void)inserted;
      it->second.volume = volume;

      SlotType slotType{};
      if (TryPartToSlot(part, slotType)) {
        if (!it->second.slotFSM) {
          it->second.slotFSM = std::make_unique<SlotStateMachine>(slotType, state.actor);
        }
      } else {
        it->second.slotFSM.reset();
      }
    }
  }

  void CaptureMotionFromVolume(const VolumeData& volume, MotionSnapshot& motion, float nowMs)
  {
    const Define::SampleResult begin = Define::SampleVolume(volume, 0.f);
    const Define::SampleResult end   = Define::SampleVolume(volume, 1.f);

    motion               = MotionSnapshot{};
    motion.start         = begin.point;
    motion.end           = end.valid ? end.point : begin.point;
    motion.direction     = begin.direction;
    motion.length        = (motion.end - motion.start).norm();
    motion.timeMs        = nowMs;
    motion.valid         = begin.valid || end.valid;
    motion.directional   = motion.length > Define::kEpsilon;
    motion.sampledPoints = {};

    if (motion.directional) {
      motion.direction = SafeNormalized(motion.end - motion.start, begin.direction);
    }

    std::visit(
        [&motion](const auto& shape) {
          using T = std::decay_t<decltype(shape)>;
          if constexpr (std::is_same_v<T, Spline>) {
            motion.sampledPoints = shape.worldSamples;
          }
        },
        volume);
  }

}  // namespace

float Proposal::ComputeConfidence(bool isCurrentPartner, bool isSustaining) const
{
  const float penetration      = (std::max)(evidence.corePenetration, evidence.surfaceContact);
  const float depthFactor      = evidence.maxDepth > Define::kEpsilon
                                     ? Clamp01(evidence.depth / evidence.maxDepth)
                                     : Clamp01(evidence.depth);
  const float deviationPenalty = Clamp01(evidence.radialDeviation);

  float confidence = penetration * InteractTuning::fConfidencePenetrationWeight;
  confidence += evidence.surfaceContact * InteractTuning::fConfidenceSurfaceWeight;
  confidence += evidence.enclosureFactor * InteractTuning::fConfidenceEnclosureWeight;
  confidence += evidence.alignmentQuality * InteractTuning::fConfidenceAlignmentWeight;
  confidence += (1.f - deviationPenalty) * InteractTuning::fConfidenceDeviationWeight;
  confidence += depthFactor * InteractTuning::fConfidenceDepthWeight;
  confidence += prior.animationBonus + prior.trajectoryBonus + prior.historyBonus;

  if (isCurrentPartner) {
    confidence += InteractTuning::fConfidenceCurrentBonus;
  }
  if (isSustaining) {
    confidence += InteractTuning::fConfidenceSustainBonus;
  }

  return Clamp01(confidence);
}

SlotStateMachine::SlotStateMachine(SlotType type, RE::Actor* owner) : m_type(type), m_owner(owner)
{}

void SlotStateMachine::Update(float deltaMs, const std::vector<Proposal>& proposals)
{
  m_timeInState += deltaMs;
  m_lockoutTimer      = (std::max)(0.f, m_lockoutTimer - deltaMs);
  m_sustainGraceTimer = (std::max)(0.f, m_sustainGraceTimer - deltaMs);

  const Proposal* currentProp = nullptr;
  const Proposal* bestProp    = nullptr;
  float bestConfidence        = 0.f;

  for (const Proposal& proposal : proposals) {
    if (proposal.targetActor != m_owner || proposal.targetSlot != m_type) {
      continue;
    }

    const bool isCurrent   = proposal.sourceActor == m_currentPartner &&
                             proposal.sourceResource == m_currentResource &&
                             proposal.inferredType == m_currentType;
    const float confidence = proposal.ComputeConfidence(isCurrent, m_state == State::Sustaining);
    if (isCurrent) {
      currentProp = &proposal;
    }
    if (confidence > bestConfidence) {
      bestConfidence = confidence;
      bestProp       = &proposal;
    }
  }

  if (!bestProp) {
    if (!ShouldMaintain(currentProp)) {
      TransitionTo(State::Idle, nullptr, m_currentResource, InteractionType::None, 0.f);
    }
    return;
  }

  if (currentProp && ShouldMaintain(currentProp)) {
    m_currentConfidence = bestConfidence;
    if (m_state == State::Approaching && bestConfidence >= InteractTuning::fEngageThreshold) {
      m_state       = State::Engaging;
      m_timeInState = 0.f;
    } else if ((m_state == State::Engaging || m_state == State::Sustaining) &&
               bestConfidence >= InteractTuning::fSustainThreshold) {
      m_state       = State::Sustaining;
      m_timeInState = 0.f;
    }
    return;
  }

  if (ShouldSwitchTo(*bestProp) || m_state == State::Idle) {
    const float confidence = bestProp->ComputeConfidence(false, false);
    const State nextState =
        confidence >= InteractTuning::fEngageThreshold ? State::Engaging : State::Approaching;
    TransitionTo(nextState, bestProp->sourceActor, bestProp->sourceResource, bestProp->inferredType,
                 confidence);
  }
}

SlotStateMachine::Desire SlotStateMachine::GetDesire() const
{
  return Desire{m_type,
                m_owner,
                m_currentPartner,
                m_currentResource,
                m_currentType,
                m_currentConfidence,
                m_state == State::Sustaining};
}

void SlotStateMachine::AcceptAssignment(RE::Actor* partner, ResourceType resource,
                                        InteractionType type)
{
  const State nextState = m_state == State::Approaching ? State::Engaging : State::Sustaining;
  TransitionTo(nextState, partner, resource, type,
               (std::max)(m_currentConfidence, InteractTuning::fAssignmentAcceptFloorConfidence));
  m_sustainGraceTimer = InteractTuning::fGraceMs;
}

void SlotStateMachine::RejectAssignment()
{
  if (m_state == State::Idle) {
    return;
  }

  if (m_state == State::Sustaining && m_sustainGraceTimer > 0.f) {
    m_currentConfidence =
        (std::max)(0.f, m_currentConfidence - InteractTuning::fRejectDecayPerFrame);
    return;
  }

  m_lockoutTimer = InteractTuning::fLockoutMs;
  TransitionTo(State::Idle, nullptr, m_currentResource, InteractionType::None, 0.f);
}

bool SlotStateMachine::ShouldMaintain(const Proposal* currentProp) const
{
  if (!currentProp) {
    return m_state == State::Sustaining && m_sustainGraceTimer > 0.f;
  }

  const float confidence = currentProp->ComputeConfidence(true, m_state == State::Sustaining ||
                                                                    m_state == State::Engaging);
  return confidence >= (m_state == State::Sustaining ? InteractTuning::fSustainThreshold
                                                     : InteractTuning::fApproachThreshold);
}

bool SlotStateMachine::ShouldSwitchTo(const Proposal& newProp) const
{
  if (m_lockoutTimer > 0.f && newProp.sourceActor != m_currentPartner) {
    return false;
  }

  const float newConfidence = newProp.ComputeConfidence(false, false);
  return m_currentPartner == nullptr ||
         newConfidence > m_currentConfidence + InteractTuning::fSwitchMargin;
}

void SlotStateMachine::TransitionTo(State newState, RE::Actor* partner, ResourceType resource,
                                    InteractionType type, float confidence)
{
  m_state             = newState;
  m_currentPartner    = partner;
  m_currentResource   = resource;
  m_currentType       = type;
  m_currentConfidence = confidence;
  m_timeInState       = 0.f;

  if (newState == State::Idle) {
    m_currentPartner    = nullptr;
    m_currentType       = InteractionType::None;
    m_currentConfidence = 0.f;
  }
}

void ResourceArbiter::Update(float deltaMs)
{
  for (auto& [owner, locks] : m_resourceLocks) {
    for (auto& [resource, lock] : locks) {
      (void)resource;
      lock.lockoutTimer = (std::max)(0.f, lock.lockoutTimer - deltaMs);
    }
  }
}

void ResourceArbiter::Resolve(const std::vector<SlotStateMachine::Desire>& desires)
{
  m_lastAssignments.clear();
  m_resourceLocks.clear();

  std::vector<SlotStateMachine::Desire> sorted = desires;
  std::sort(sorted.begin(), sorted.end(), [](const auto& lhs, const auto& rhs) {
    if (lhs.isSustaining != rhs.isSustaining) {
      return lhs.isSustaining > rhs.isSustaining;
    }
    return lhs.confidence > rhs.confidence;
  });

  for (const auto& desire : sorted) {
    Assignment assignment{
        desire.slot,        desire.owner, desire.preferredSource, desire.desiredResource,
        desire.desiredType, false};
    if (desire.owner && desire.preferredSource && desire.confidence > 0.f) {
      assignment.accepted = TryAssign(desire, assignment);
    }
    m_lastAssignments.push_back(assignment);
  }
}

bool ResourceArbiter::IsResourceAvailable(ResourceType res, RE::Actor* owner, RE::Actor* requester,
                                          SlotType slot) const
{
  (void)requester;
  (void)slot;

  auto ownerIt = m_resourceLocks.find(owner);
  if (ownerIt == m_resourceLocks.end()) {
    return true;
  }

  return ownerIt->second.find(res) == ownerIt->second.end();
}

bool ResourceArbiter::TryAssign(const SlotStateMachine::Desire& desire, Assignment& outAssign)
{
  auto& ownerLocks = m_resourceLocks[desire.preferredSource];
  auto lockIt      = ownerLocks.find(desire.desiredResource);
  if (lockIt == ownerLocks.end()) {
    ownerLocks[desire.desiredResource] =
        ResourceLock{desire.desiredResource,
                     desire.preferredSource,
                     desire.slot,
                     desire.owner,
                     desire.desiredType,
                     desire.confidence,
                     desire.isSustaining ? 0.f : InteractTuning::fLockoutMs,
                     desire.isSustaining};
    outAssign.accepted = true;
    return true;
  }

  if (!CanPreempt(lockIt->second, desire)) {
    return false;
  }

  lockIt->second     = ResourceLock{desire.desiredResource,
                                    desire.preferredSource,
                                    desire.slot,
                                    desire.owner,
                                    desire.desiredType,
                                    desire.confidence,
                                    desire.isSustaining ? 0.f : InteractTuning::fLockoutMs,
                                    desire.isSustaining};
  outAssign.accepted = true;
  return true;
}

bool ResourceArbiter::CanPreempt(const ResourceLock& current,
                                 const SlotStateMachine::Desire& challenger) const
{
  if (current.partner == challenger.owner && current.interactionType == challenger.desiredType) {
    return true;
  }
  if (current.isSustaining && !challenger.isSustaining) {
    return challenger.confidence > current.confidence + InteractTuning::fPreemptMarginSustaining;
  }
  if (current.lockoutTimer > 0.f) {
    return challenger.confidence > current.confidence + InteractTuning::fPreemptMarginLockout;
  }
  return challenger.confidence > current.confidence + InteractTuning::fPreemptMarginDefault;
}

void ResourceArbiter::ReleaseResource(RE::Actor* owner, ResourceType resource)
{
  auto ownerIt = m_resourceLocks.find(owner);
  if (ownerIt == m_resourceLocks.end()) {
    return;
  }

  ownerIt->second.erase(resource);
  if (ownerIt->second.empty()) {
    m_resourceLocks.erase(ownerIt);
  }
}

void ActorInteractState::UpdateBody(float deltaMs)
{
  (void)deltaMs;
  body.UpdateWorld();
  BindActorParts(*this);
}

void ActorInteractState::CaptureMotion(float nowMs)
{
  for (auto& [part, runtime] : parts) {
    (void)part;
    if (!runtime.volume) {
      runtime.motion = MotionSnapshot{};
      continue;
    }
    CaptureMotionFromVolume(*runtime.volume, runtime.motion, nowMs);
  }
}

namespace EvidenceEvaluator
{

  GeometricEvidence Evaluate(const VolumeData& source, const VolumeData& target,
                             SlotType targetSlotType)
  {
    (void)targetSlotType;

    return std::visit(
        [&](const auto& srcValue) -> GeometricEvidence {
          using TSource = std::decay_t<decltype(srcValue)>;
          if constexpr (!std::is_same_v<TSource, Capsule> && !std::is_same_v<TSource, Spline>) {
            GeometricEvidence evidence{};
            return evidence;
          } else {
            return std::visit(
                [&](const auto& dstValue) -> GeometricEvidence {
                  using TTarget = std::decay_t<decltype(dstValue)>;
                  if constexpr (std::is_same_v<TTarget, Funnel>) {
                    return EvaluateAgainstFunnel(srcValue, dstValue);
                  } else if constexpr (std::is_same_v<TTarget, EnvelopeCp>) {
                    return EvaluateAgainstEnvelope(srcValue, dstValue);
                  } else if constexpr (std::is_same_v<TTarget, EnvelopeHb>) {
                    return EvaluateAgainstEnvelope(srcValue, dstValue);
                  } else if constexpr (std::is_same_v<TTarget, HalfBall>) {
                    return EvaluateAgainstHalfBall(srcValue, dstValue);
                  } else if constexpr (std::is_same_v<TTarget, Surface>) {
                    return EvaluateAgainstSurface(srcValue, dstValue);
                  } else {
                    return EvaluateGeneric(srcValue, target);
                  }
                },
                target);
          }
        },
        source);
  }

  GeometricEvidence EvaluateCapsuleToFunnel(const Capsule& cap, const Funnel& funnel)
  {
    return EvaluateAgainstFunnel(cap, funnel);
  }

  GeometricEvidence EvaluateSplineCapsuleToFunnel(const Spline& spline, const Funnel& funnel)
  {
    return EvaluateAgainstFunnel(spline, funnel);
  }

  GeometricEvidence EvaluateCapsuleToEnvelope(const Capsule& cap, const EnvelopeCp& env)
  {
    return EvaluateAgainstEnvelope(cap, env);
  }

  GeometricEvidence EvaluateSplineCapsuleToEnvelope(const Spline& spline, const EnvelopeCp& env)
  {
    return EvaluateAgainstEnvelope(spline, env);
  }

  GeometricEvidence EvaluateCapsuleToHalfBall(const Capsule& cap, const HalfBall& hb)
  {
    return EvaluateAgainstHalfBall(cap, hb);
  }

}  // namespace EvidenceEvaluator

void ProposalBuilder::BuildProposals(const std::vector<ActorInteractState*>& actors,
                                     std::vector<Proposal>& outProposals, float nowMs)
{
  outProposals.clear();
  for (std::size_t i = 0; i < actors.size(); ++i) {
    for (std::size_t j = 0; j < actors.size(); ++j) {
      if (i == j || !actors[i] || !actors[j]) {
        continue;
      }
      GenerateForPair(*actors[i], *actors[j], outProposals, nowMs);
    }
  }
}

void ProposalBuilder::GenerateForPair(ActorInteractState& sourceState,
                                      ActorInteractState& targetState, std::vector<Proposal>& out,
                                      float nowMs)
{
  auto addProposal = [&](ResourceType resource, PartName sourcePartName, SlotType slot,
                         InteractionType type) {
    const PartName targetPartName = SlotToPart(slot);
    const PartRuntime* sourcePart = TryGetRuntime(sourceState, sourcePartName);
    const PartRuntime* targetPart = TryGetRuntime(targetState, targetPartName);
    if (!sourcePart || !targetPart || !sourcePart->volume || !targetPart->volume) {
      return;
    }

    Proposal proposal;
    proposal.sourceActor    = sourceState.actor;
    proposal.targetActor    = targetState.actor;
    proposal.sourceResource = resource;
    proposal.targetSlot     = slot;
    proposal.inferredType   = type;
    proposal.evidence = EvidenceEvaluator::Evaluate(*sourcePart->volume, *targetPart->volume, slot);
    proposal.prior    = BuildPrior(sourceState, *sourcePart, targetState, *targetPart, type);
    proposal.timestamp = nowMs;
    AddIfValid(proposal, out);
  };

  addProposal(ResourceType::Mouth, PartName::Mouth, SlotType::Mouth, InteractionType::Kiss);
  addProposal(ResourceType::Mouth, PartName::Mouth, SlotType::BreastLeft,
              InteractionType::BreastSucking);
  addProposal(ResourceType::Mouth, PartName::Mouth, SlotType::BreastRight,
              InteractionType::BreastSucking);
  addProposal(ResourceType::Mouth, PartName::Mouth, SlotType::FootLeft,
              InteractionType::ToeSucking);
  addProposal(ResourceType::Mouth, PartName::Mouth, SlotType::FootRight,
              InteractionType::ToeSucking);
  addProposal(ResourceType::Mouth, PartName::Mouth, SlotType::Vagina, InteractionType::Cunnilingus);
  addProposal(ResourceType::Mouth, PartName::Mouth, SlotType::Anus, InteractionType::Anilingus);

  addProposal(ResourceType::LeftHand, PartName::HandLeft, SlotType::BreastLeft,
              InteractionType::GropeBreast);
  addProposal(ResourceType::LeftHand, PartName::HandLeft, SlotType::BreastRight,
              InteractionType::GropeBreast);
  addProposal(ResourceType::RightHand, PartName::HandRight, SlotType::BreastLeft,
              InteractionType::GropeBreast);
  addProposal(ResourceType::RightHand, PartName::HandRight, SlotType::BreastRight,
              InteractionType::GropeBreast);
  addProposal(ResourceType::LeftHand, PartName::HandLeft, SlotType::ThighCleft,
              InteractionType::GropeThigh);
  addProposal(ResourceType::RightHand, PartName::HandRight, SlotType::ThighCleft,
              InteractionType::GropeThigh);
  addProposal(ResourceType::LeftHand, PartName::HandLeft, SlotType::GlutealCleft,
              InteractionType::GropeButt);
  addProposal(ResourceType::RightHand, PartName::HandRight, SlotType::GlutealCleft,
              InteractionType::GropeButt);
  addProposal(ResourceType::LeftHand, PartName::HandLeft, SlotType::FootLeft,
              InteractionType::GropeFoot);
  addProposal(ResourceType::LeftHand, PartName::HandLeft, SlotType::FootRight,
              InteractionType::GropeFoot);
  addProposal(ResourceType::RightHand, PartName::HandRight, SlotType::FootLeft,
              InteractionType::GropeFoot);
  addProposal(ResourceType::RightHand, PartName::HandRight, SlotType::FootRight,
              InteractionType::GropeFoot);
  addProposal(ResourceType::LeftHand, PartName::FingerLeft, SlotType::Vagina,
              InteractionType::FingerVagina);
  addProposal(ResourceType::RightHand, PartName::FingerRight, SlotType::Vagina,
              InteractionType::FingerVagina);
  addProposal(ResourceType::LeftHand, PartName::FingerLeft, SlotType::Anus,
              InteractionType::FingerAnus);
  addProposal(ResourceType::RightHand, PartName::FingerRight, SlotType::Anus,
              InteractionType::FingerAnus);

  addProposal(ResourceType::Penis, PartName::Penis, SlotType::Mouth, InteractionType::Fellatio);
  addProposal(ResourceType::Penis, PartName::Penis, SlotType::Throat, InteractionType::DeepThroat);
  addProposal(ResourceType::Penis, PartName::Penis, SlotType::Cleavage, InteractionType::Titfuck);
  addProposal(ResourceType::Penis, PartName::Penis, SlotType::Belly, InteractionType::Naveljob);
  addProposal(ResourceType::Penis, PartName::Penis, SlotType::ThighCleft,
              InteractionType::Thighjob);
  addProposal(ResourceType::Penis, PartName::Penis, SlotType::GlutealCleft,
              InteractionType::Frottage);
  addProposal(ResourceType::Penis, PartName::Penis, SlotType::FootLeft, InteractionType::Footjob);
  addProposal(ResourceType::Penis, PartName::Penis, SlotType::FootRight, InteractionType::Footjob);
  addProposal(ResourceType::Penis, PartName::Penis, SlotType::Vagina, InteractionType::Vaginal);
  addProposal(ResourceType::Penis, PartName::Penis, SlotType::Anus, InteractionType::Anal);
}

void ProposalBuilder::AddIfValid(const Proposal& prop, std::vector<Proposal>& out)
{
  const float confidence = prop.ComputeConfidence(false, false);
  if (confidence < ConfidenceThresholdFor(prop.inferredType)) {
    return;
  }
  out.push_back(prop);
}

Interact::Interact(std::vector<RE::Actor*> actors)
{
  for (RE::Actor* actor : actors) {
    if (!actor) {
      continue;
    }

    auto state   = std::make_unique<ActorInteractState>();
    state->actor = actor;
    state->body =
        Define::ActorBody(actor, Define::Race::GetRace(actor), Define::Gender::GetGender(actor));
    BindActorParts(*state);
    state->CaptureMotion(0.f);
    m_actorStates.emplace(actor, std::move(state));
  }
}

void Interact::FlashNodeData()
{
  for (auto& [actor, state] : m_actorStates) {
    (void)actor;
    if (!state) {
      continue;
    }
    state->body.ResolveNodes();
    state->body.UpdateWorld();
    BindActorParts(*state);
    state->CaptureMotion(m_timelineMs);
  }
}

void Interact::Update(float deltaMs)
{
  m_timelineMs += deltaMs;

  std::vector<ActorInteractState*> actorStates;
  actorStates.reserve(m_actorStates.size());
  for (auto& [actor, state] : m_actorStates) {
    (void)actor;
    if (!state) {
      continue;
    }
    state->UpdateBody(deltaMs);
    state->CaptureMotion(m_timelineMs);
    actorStates.push_back(state.get());
  }

  std::vector<Proposal> proposals;
  m_builder.BuildProposals(actorStates, proposals, m_timelineMs);

  for (auto& [actor, state] : m_actorStates) {
    (void)actor;
    if (!state) {
      continue;
    }
    for (auto& [partName, runtime] : state->parts) {
      (void)partName;
      if (runtime.slotFSM) {
        runtime.slotFSM->Update(deltaMs, proposals);
      }
    }
  }

  std::vector<SlotStateMachine::Desire> desires;
  for (auto& [actor, state] : m_actorStates) {
    (void)actor;
    if (!state) {
      continue;
    }
    state->observedTags = Define::InteractTags{};
    state->resourceBusy.clear();
    for (auto& [partName, runtime] : state->parts) {
      (void)partName;
      runtime.interaction = InteractionSnapshot{};
      if (runtime.slotFSM) {
        const auto desire = runtime.slotFSM->GetDesire();
        if (desire.owner && desire.preferredSource && desire.desiredType != InteractionType::None &&
            desire.confidence > 0.f) {
          desires.push_back(desire);
        }
      }
    }
  }

  m_arbiter.Update(deltaMs);
  m_arbiter.Resolve(desires);

  for (auto& [actor, state] : m_actorStates) {
    (void)actor;
    if (!state) {
      continue;
    }
    for (auto& [partName, runtime] : state->parts) {
      if (!runtime.slotFSM) {
        continue;
      }

      SlotType slotType{};
      if (!TryPartToSlot(partName, slotType)) {
        continue;
      }

      const auto& assignments = m_arbiter.GetLastAssignments();
      const auto it =
          std::find_if(assignments.begin(), assignments.end(), [&](const auto& assignment) {
            return assignment.targetActor == state->actor && assignment.slot == slotType;
          });

      if (it != assignments.end() && it->accepted) {
        runtime.slotFSM->AcceptAssignment(it->sourceActor, it->resource, it->type);
      } else {
        runtime.slotFSM->RejectAssignment();
      }
    }
  }

  for (const auto& assignment : m_arbiter.GetLastAssignments()) {
    if (!assignment.accepted || !assignment.targetActor || !assignment.sourceActor) {
      continue;
    }

    auto sourceIt = m_actorStates.find(assignment.sourceActor);
    auto targetIt = m_actorStates.find(assignment.targetActor);
    if (sourceIt == m_actorStates.end() || targetIt == m_actorStates.end()) {
      continue;
    }

    ActorInteractState& sourceState = *sourceIt->second;
    ActorInteractState& targetState = *targetIt->second;

    const PartName sourcePartName = SourcePartForResource(assignment.resource, assignment.type);
    const PartName targetPartName = SlotToPart(assignment.slot);
    PartRuntime* sourceRuntime    = TryGetRuntime(sourceState, sourcePartName);
    PartRuntime* targetRuntime    = TryGetRuntime(targetState, targetPartName);
    if (!sourceRuntime || !targetRuntime || !sourceRuntime->volume || !targetRuntime->volume) {
      continue;
    }

    const GeometricEvidence evidence = EvidenceEvaluator::Evaluate(
        *sourceRuntime->volume, *targetRuntime->volume, assignment.slot);
    const float distance      = EstimateDistance(evidence);
    const float approachSpeed = 0.f;

    sourceRuntime->interaction =
        MakeInteraction(targetState.actor, assignment.type, distance, approachSpeed);
    targetRuntime->interaction =
        MakeInteraction(sourceState.actor, assignment.type, distance, approachSpeed);

    sourceState.resourceBusy[assignment.resource] = true;
    AccumulateObservedType(sourceState.observedTags, assignment.type);
    AccumulateObservedType(targetState.observedTags, assignment.type);
  }
}

const ActorInteractState* Interact::GetActorState(RE::Actor* actor) const
{
  auto it = m_actorStates.find(actor);
  return it == m_actorStates.end() ? nullptr : it->second.get();
}

const PartRuntime* Interact::GetPartRuntime(RE::Actor* actor, PartName part) const
{
  const ActorInteractState* state = GetActorState(actor);
  if (!state) {
    return nullptr;
  }

  auto it = state->parts.find(part);
  return it == state->parts.end() ? nullptr : &it->second;
}

const Define::InteractTags& Interact::GetObservedTags(RE::Actor* actor) const
{
  static const Define::InteractTags kEmpty{};
  const ActorInteractState* state = GetActorState(actor);
  return state ? state->observedTags : kEmpty;
}

void Interact::DebugDraw() {}

}  // namespace Instance

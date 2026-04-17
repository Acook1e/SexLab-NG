#pragma once

namespace Settings
{
#pragma region General
inline constexpr bool kDefaultDebugMode              = false;
inline constexpr bool kDefaultDebugShowInactiveParts = false;
inline constexpr bool kDefaultDebugShowMotionData    = false;

inline bool bDebugMode              = kDefaultDebugMode;
inline bool bDebugShowInactiveParts = kDefaultDebugShowInactiveParts;
inline bool bDebugShowMotionData    = kDefaultDebugShowMotionData;
#pragma endregion

#pragma region Scene Search
inline constexpr float kDefaultMaxScaleDeviation = 0.3f;
inline constexpr bool kDefaultUseLeadIn          = false;
inline constexpr bool kDefaultStrictSceneTags    = true;
inline constexpr bool kDefaultStrictInteractTags = true;

inline float fMaxScaleDeviation = kDefaultMaxScaleDeviation;
inline bool bUseLeadIn          = kDefaultUseLeadIn;
inline bool bStrictSceneTags    = kDefaultStrictSceneTags;
inline bool bStrictInteractTags = kDefaultStrictInteractTags;
#pragma endregion

#pragma region Scene Play
inline bool bEnableActorWalkToCenter = true;
#pragma endregion

#pragma region Interact
namespace Interact
{
  inline constexpr float kDefaultApproachThreshold = 0.28f;
  inline constexpr float kDefaultEngageThreshold   = 0.44f;
  inline constexpr float kDefaultSustainThreshold  = 0.38f;
  inline constexpr float kDefaultSwitchMargin      = 0.12f;
  inline constexpr float kDefaultLockoutMs         = 180.f;
  inline constexpr float kDefaultGraceMs           = 220.f;

  inline constexpr float kDefaultProposalThresholdKiss        = 0.24f;
  inline constexpr float kDefaultProposalThresholdManual      = 0.22f;
  inline constexpr float kDefaultProposalThresholdContact     = 0.32f;
  inline constexpr float kDefaultProposalThresholdPenetration = 0.42f;
  inline constexpr float kDefaultProposalThresholdDefault     = 0.28f;

  inline constexpr float kDefaultPriorHistoryBonusBase      = 0.12f;
  inline constexpr float kDefaultPriorTrajectoryBonusWeight = 0.08f;

  inline constexpr float kDefaultConfidencePenetrationWeight = 0.42f;
  inline constexpr float kDefaultConfidenceSurfaceWeight     = 0.18f;
  inline constexpr float kDefaultConfidenceEnclosureWeight   = 0.12f;
  inline constexpr float kDefaultConfidenceAlignmentWeight   = 0.14f;
  inline constexpr float kDefaultConfidenceDeviationWeight   = 0.08f;
  inline constexpr float kDefaultConfidenceDepthWeight       = 0.06f;
  inline constexpr float kDefaultConfidenceCurrentBonus      = 0.08f;
  inline constexpr float kDefaultConfidenceSustainBonus      = 0.08f;

  inline constexpr float kDefaultHalfBallContactSlack        = 1.5f;
  inline constexpr float kDefaultEnvelopeContactSlack        = 1.5f;
  inline constexpr float kDefaultSurfaceContactDistanceScale = 5.0f;
  inline constexpr float kDefaultFunnelMinimumLocalRadius    = 0.2f;
  inline constexpr float kDefaultFunnelContactSlack          = 1.5f;
  inline constexpr float kDefaultFunnelDepthScoreBonus       = 0.4f;
  inline constexpr float kDefaultGenericContactDistanceScale = 3.5f;

  inline constexpr float kDefaultAssignmentAcceptFloorConfidence = 0.5f;
  inline constexpr float kDefaultRejectDecayPerFrame             = 0.04f;
  inline constexpr float kDefaultPreemptMarginSustaining         = 0.16f;
  inline constexpr float kDefaultPreemptMarginLockout            = 0.20f;
  inline constexpr float kDefaultPreemptMarginDefault            = 0.08f;

  inline float fApproachThreshold = kDefaultApproachThreshold;
  inline float fEngageThreshold   = kDefaultEngageThreshold;
  inline float fSustainThreshold  = kDefaultSustainThreshold;
  inline float fSwitchMargin      = kDefaultSwitchMargin;
  inline float fLockoutMs         = kDefaultLockoutMs;
  inline float fGraceMs           = kDefaultGraceMs;

  inline float fProposalThresholdKiss        = kDefaultProposalThresholdKiss;
  inline float fProposalThresholdManual      = kDefaultProposalThresholdManual;
  inline float fProposalThresholdContact     = kDefaultProposalThresholdContact;
  inline float fProposalThresholdPenetration = kDefaultProposalThresholdPenetration;
  inline float fProposalThresholdDefault     = kDefaultProposalThresholdDefault;

  inline float fPriorHistoryBonusBase      = kDefaultPriorHistoryBonusBase;
  inline float fPriorTrajectoryBonusWeight = kDefaultPriorTrajectoryBonusWeight;

  inline float fConfidencePenetrationWeight = kDefaultConfidencePenetrationWeight;
  inline float fConfidenceSurfaceWeight     = kDefaultConfidenceSurfaceWeight;
  inline float fConfidenceEnclosureWeight   = kDefaultConfidenceEnclosureWeight;
  inline float fConfidenceAlignmentWeight   = kDefaultConfidenceAlignmentWeight;
  inline float fConfidenceDeviationWeight   = kDefaultConfidenceDeviationWeight;
  inline float fConfidenceDepthWeight       = kDefaultConfidenceDepthWeight;
  inline float fConfidenceCurrentBonus      = kDefaultConfidenceCurrentBonus;
  inline float fConfidenceSustainBonus      = kDefaultConfidenceSustainBonus;

  inline float fHalfBallContactSlack        = kDefaultHalfBallContactSlack;
  inline float fEnvelopeContactSlack        = kDefaultEnvelopeContactSlack;
  inline float fSurfaceContactDistanceScale = kDefaultSurfaceContactDistanceScale;
  inline float fFunnelMinimumLocalRadius    = kDefaultFunnelMinimumLocalRadius;
  inline float fFunnelContactSlack          = kDefaultFunnelContactSlack;
  inline float fFunnelDepthScoreBonus       = kDefaultFunnelDepthScoreBonus;
  inline float fGenericContactDistanceScale = kDefaultGenericContactDistanceScale;

  inline float fAssignmentAcceptFloorConfidence = kDefaultAssignmentAcceptFloorConfidence;
  inline float fRejectDecayPerFrame             = kDefaultRejectDecayPerFrame;
  inline float fPreemptMarginSustaining         = kDefaultPreemptMarginSustaining;
  inline float fPreemptMarginLockout            = kDefaultPreemptMarginLockout;
  inline float fPreemptMarginDefault            = kDefaultPreemptMarginDefault;

  inline void Reset()
  {
    fApproachThreshold = kDefaultApproachThreshold;
    fEngageThreshold   = kDefaultEngageThreshold;
    fSustainThreshold  = kDefaultSustainThreshold;
    fSwitchMargin      = kDefaultSwitchMargin;
    fLockoutMs         = kDefaultLockoutMs;
    fGraceMs           = kDefaultGraceMs;

    fProposalThresholdKiss        = kDefaultProposalThresholdKiss;
    fProposalThresholdManual      = kDefaultProposalThresholdManual;
    fProposalThresholdContact     = kDefaultProposalThresholdContact;
    fProposalThresholdPenetration = kDefaultProposalThresholdPenetration;
    fProposalThresholdDefault     = kDefaultProposalThresholdDefault;

    fPriorHistoryBonusBase      = kDefaultPriorHistoryBonusBase;
    fPriorTrajectoryBonusWeight = kDefaultPriorTrajectoryBonusWeight;

    fConfidencePenetrationWeight = kDefaultConfidencePenetrationWeight;
    fConfidenceSurfaceWeight     = kDefaultConfidenceSurfaceWeight;
    fConfidenceEnclosureWeight   = kDefaultConfidenceEnclosureWeight;
    fConfidenceAlignmentWeight   = kDefaultConfidenceAlignmentWeight;
    fConfidenceDeviationWeight   = kDefaultConfidenceDeviationWeight;
    fConfidenceDepthWeight       = kDefaultConfidenceDepthWeight;
    fConfidenceCurrentBonus      = kDefaultConfidenceCurrentBonus;
    fConfidenceSustainBonus      = kDefaultConfidenceSustainBonus;

    fHalfBallContactSlack        = kDefaultHalfBallContactSlack;
    fEnvelopeContactSlack        = kDefaultEnvelopeContactSlack;
    fSurfaceContactDistanceScale = kDefaultSurfaceContactDistanceScale;
    fFunnelMinimumLocalRadius    = kDefaultFunnelMinimumLocalRadius;
    fFunnelContactSlack          = kDefaultFunnelContactSlack;
    fFunnelDepthScoreBonus       = kDefaultFunnelDepthScoreBonus;
    fGenericContactDistanceScale = kDefaultGenericContactDistanceScale;

    fAssignmentAcceptFloorConfidence = kDefaultAssignmentAcceptFloorConfidence;
    fRejectDecayPerFrame             = kDefaultRejectDecayPerFrame;
    fPreemptMarginSustaining         = kDefaultPreemptMarginSustaining;
    fPreemptMarginLockout            = kDefaultPreemptMarginLockout;
    fPreemptMarginDefault            = kDefaultPreemptMarginDefault;
  }
}  // namespace Interact
#pragma endregion

}  // namespace Settings
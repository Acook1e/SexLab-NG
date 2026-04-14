#pragma once

namespace Settings
{
#pragma region General
inline bool bDebugMode = false;
#pragma endregion

#pragma region Scene Search
inline float fMaxScaleDeviation = 0.3f;
inline bool bUseLeadIn          = false;
inline bool bStrictSceneTags    = true;
inline bool bStrictInteractTags = true;
#pragma endregion

#pragma region Scene Play
inline bool bEnableActorWalkToCenter = true;
#pragma endregion

}  // namespace Settings
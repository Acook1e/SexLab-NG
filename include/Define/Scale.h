#pragma once

namespace Define
{
class Scale
{
public:
  Scale(float scale) : scale(scale) {}

  float GetScale() const { return scale; }

  static float CalculateScale(RE::Actor* actor)
  {
    if (!actor)
      return 1.0f;

    auto node = actor->GetNodeByName(baseNode);
    return actor->GetScale() * (node ? node->local.scale : 1.0f);
  }

  static void ApplyScale(RE::Actor* actor, float scale)
  {
    if (!actor)
      return;
  }

private:
  constexpr static inline std::string_view baseNode = "NPC";

  float scale;
};
}  // namespace Define
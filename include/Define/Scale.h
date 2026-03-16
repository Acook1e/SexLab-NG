#pragma once

namespace Define
{
class Scale
{
public:
  Scale(float scale) : scale(scale) {}

  float GetScale() const { return scale; }

  static void ApplyScale(RE::Actor* actor, float scale)
  {
    // Placeholder for applying scale to the actor
    // This would involve modifying the actor's properties based on the scale
  }

  static float CalculateScale(RE::Actor* actor)
  {
    // Placeholder for scale calculation logic based on position
    return 1.0f;  // Default scale value
  }

private:
  float scale;
};
}  // namespace Define
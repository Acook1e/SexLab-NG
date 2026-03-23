#pragma once

namespace Define
{
// Define enjoyment levels for interactions
// from -100 to +100
class Enjoyment
{
public:
  enum class Degree : int8_t
  {
    MinValue      = -100,
    Agony         = -80,
    Pain          = -50,
    Uncomfortable = -20,
    Neutral       = 0,
    Pleasure      = 20,
    Aroused       = 50,
    Climax        = 80,
    MaxValue      = 100
  };

  Enjoyment() : value(0.0f) {}
  Enjoyment(float a_value)
      : value(std::clamp(a_value, static_cast<float>(Degree::MinValue), static_cast<float>(Degree::MaxValue)))
  {}

  const float& GetValue() const { return value; }
  Degree SetValue(float a_value)
  {
    value = std::clamp(a_value, static_cast<float>(Degree::MinValue), static_cast<float>(Degree::MaxValue));
    return GetDegree();
  }

  const Degree GetDegree() const
  {
    if (value <= static_cast<float>(Degree::Agony))
      return Degree::Agony;
    else if (value <= static_cast<float>(Degree::Pain))
      return Degree::Pain;
    else if (value <= static_cast<float>(Degree::Uncomfortable))
      return Degree::Uncomfortable;
    else if (value < static_cast<float>(Degree::Pleasure))
      return Degree::Neutral;
    else if (value < static_cast<float>(Degree::Aroused))
      return Degree::Pleasure;
    else if (value < static_cast<float>(Degree::Climax))
      return Degree::Aroused;
    else if (value <= static_cast<float>(Degree::MaxValue))
      return Degree::Climax;
    else
      return Degree::Neutral;  // Should never happen due to clamping, but just in case
  }

private:
  float value = 0.0f;
};
}  // namespace Define
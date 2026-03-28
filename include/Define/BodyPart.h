#pragma once

#include "Define/Gender.h"
#include "Define/Race.h"

#include "eigen3/Eigen/Dense"

namespace Define
{
using Point3f  = Eigen::Vector3f;
using Vector3f = Eigen::Vector3f;

class BodyPart
{
public:
  enum class Name : std::uint8_t
  {
    Mouth,        // All Creature
    BreastLeft,   // Female or Futa
    BreastRight,  // Female or Futa
    HandLeft,     // All Creature
    HandRight,    // All Creature
    FingerLeft,   // Human only
    FingerRight,  // Human only
    Belly,        // Human only
    ThighLeft,    // Human only
    ThighRight,   // Human only
    ButtLeft,     // Human only
    ButtRight,    // Human only
    FootLeft,     // All Creature
    FootRight,    // All Creature
    Vagina,       // Female or Futa
    Anus,         // Human only
    Penis,        // Male, Futa or CreatureMale
  };

  static bool HasBodyPart(Gender gender, Race race, Name name);

  enum class Type : std::uint8_t
  {
    Point,
    Vector,             // Vector based on 2 points
    FitVector,          // Vector fitted based on 3 points
    NormalVectorStart,  // Start Point on the Plane
    NormalVectorEnd,    // End Point on the Plane
  };

  BodyPart() = default;
  BodyPart(Race race, Name name, Type type);

  void UpdateNodes();
  void UpdatePosition();

private:
  std::vector<std::variant<RE::NiNode*, std::array<RE::NiNode*, 2>>> nodes;
  Point3f start;
  Point3f end;
  Vector3f direction;
  float squaredLength;
  Name name;
  Type type;
};
}  // namespace Define
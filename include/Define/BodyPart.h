#pragma once

#include "Define/Gender.h"
#include "Define/Race.h"

#include "eigen3/Eigen/Dense"

namespace Define
{
using Point3f  = Eigen::Vector3f;
using Vector3f = Eigen::Vector3f;

using NodeName    = std::string_view;
using MidNodeName = std::array<std::string_view, 2>;
using PointName   = std::variant<NodeName, MidNodeName>;

using Node    = RE::NiNode*;
using MidNode = std::array<RE::NiNode*, 2>;
using Point   = std::variant<Node, MidNode>;

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
  BodyPart(RE::Actor* actor, Race race, Name name);

  Name GetName() const { return name; }

  const Point3f& GetStart() const { return start; }
  const Point3f& GetEnd() const { return end; }
  const Vector3f& GetDirection() const { return direction; }
  float GetLength() const { return length; }

  void UpdateNodes();
  void UpdatePosition();
  Vector3f FitVector();

  float Angle(const BodyPart& other);
  float Distance(const BodyPart& other);

private:
  std::vector<PointName*> nodeNames{};
  std::vector<Point> nodes{};
  Point3f start{};
  Point3f end{};
  Vector3f direction{};
  float length     = 0.0f;
  RE::Actor* actor = nullptr;
  Name name;
  Type type;
};
}  // namespace Define
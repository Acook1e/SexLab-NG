#pragma once

#include "Define/Gender.h"
#include "Define/Race.h"

#include "eigen3/Eigen/Dense"

namespace Define
{
using Point3f  = Eigen::Vector3f;
using Vector3f = Eigen::Vector3f;
using Matrix3f = Eigen::Matrix3f;

using NodeName        = std::string_view;
using MidNodeName     = std::array<std::string_view, 2>;
using CentralNodeName = std::array<std::string_view, 3>;

struct OffsetNodeName
{
  std::string_view baseNode;
  Vector3f eulerRot;  // Euler angles in radians, XYZ order
  Vector3f localTrans;
};

using PointName = std::variant<NodeName, MidNodeName, CentralNodeName, OffsetNodeName>;

using Node        = RE::NiPointer<RE::NiNode>;
using MidNode     = std::array<RE::NiPointer<RE::NiNode>, 2>;
using CentralNode = std::array<RE::NiPointer<RE::NiNode>, 3>;

struct OffsetNode
{
  RE::NiPointer<RE::NiNode> baseNode;
  Matrix3f localRot;
  Vector3f localTrans;
};

using Point = std::variant<Node, MidNode, CentralNode, OffsetNode>;

class BodyPart
{
public:
  enum class Name : std::uint8_t
  {
    Mouth,         // All Creature
    Throat,        // Human only
    BreastLeft,    // Female or Futa
    BreastRight,   // Female or Futa
    Cleavage,      // Female or Futa
    HandLeft,      // All Creature
    HandRight,     // All Creature
    FingerLeft,    // Human only
    FingerRight,   // Human only
    Belly,         // Human only
    ThighLeft,     // Human only
    ThighRight,    // Human only
    ThighCleft,    // Female or Futa
    ButtLeft,      // Human only
    ButtRight,     // Human only
    GlutealCleft,  // Female or Futa
    FootLeft,      // All Creature
    FootRight,     // All Creature
    Vagina,        // Female or Futa
    Anus,          // Human only
    Penis,         // Male, Futa or CreatureMale
  };

  static bool HasBodyPart(Gender gender, Race race, Name name);

  enum class Type : std::uint8_t
  {
    Point,              // Single node; no direction
    Vector,             // start -> end, direction = unit(end - start)
    FitVector,          // SVD principal axis through 2-3 nodes
    NormalVectorStart,  // Vector where start is the surface point
    NormalVectorEnd,    // Vector where end is the surface point (e.g. Belly: Spine1->Belly)
  };

  BodyPart() = default;
  BodyPart(RE::Actor* actor, Race race, Name name);

  [[nodiscard]] Name GetName() const noexcept { return name; }
  [[nodiscard]] Type GetType() const noexcept { return type; }
  [[nodiscard]] bool IsValid() const;

  [[nodiscard]] const Point3f& GetStart() const noexcept { return start; }
  [[nodiscard]] const Point3f& GetEnd() const noexcept { return end; }
  [[nodiscard]] const Vector3f& GetDirection() const noexcept { return direction; }
  [[nodiscard]] float GetLength() const noexcept { return length; }

  void UpdateNodes();
  void UpdatePosition();

private:
  Vector3f FitVector();

  std::vector<PointName*> nodeNames{};
  std::vector<Point> nodes{};
  Point3f start{};
  Point3f end{};
  Vector3f direction{};  // unit vector after UpdatePosition
  float length     = 0.f;
  RE::Actor* actor = nullptr;
  Name name        = Name::Mouth;
  Type type        = Type::Point;
};

}  // namespace Define
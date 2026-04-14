#pragma once

#include "Define/Gender.h"
#include "Define/Race.h"

#include "eigen3/Eigen/Dense"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Define
{
using Point3f  = Eigen::Vector3f;
using Vector3f = Eigen::Vector3f;
using Matrix3f = Eigen::Matrix3f;

struct Node
{
  std::vector<std::string_view> nodeNames{};
  std::vector<RE::NiPointer<RE::NiNode>> nodes{};
  Point3f position    = Point3f::Zero();
  Matrix3f localRot   = Matrix3f::Identity();
  Vector3f localTrans = Vector3f::Zero();
  bool hasLocalOffset = false;
};

struct Vector
{
  std::vector<Node> nodes{};
  Point3f start      = Point3f::Zero();
  Point3f end        = Point3f::Zero();
  Vector3f direction = Vector3f::Zero();
  float length       = 0.f;
};

struct CapsuleCollider
{
  Point3f start = Point3f::Zero();
  Point3f end   = Point3f::Zero();
  float radius  = 0.f;
};

struct BoxCollider
{
  Point3f center       = Point3f::Zero();
  Matrix3f basis       = Matrix3f::Identity();
  Vector3f halfExtents = Vector3f::Zero();
};

struct Collider
{
  std::vector<CapsuleCollider> capsules{};
  std::vector<BoxCollider> boxes{};
  Point3f center     = Point3f::Zero();
  Vector3f direction = Vector3f::Zero();
  float length       = 0.f;
};

struct ShapeInfo
{
  std::optional<Vector> vector{};
  std::optional<Collider> collider{};
};

struct ResolvedNodeInfo
{
  std::string description{};
  std::vector<std::string> requestedNodes{};
  std::vector<std::string> resolvedNodes{};
  std::vector<std::string> parentNodes{};
  bool valid = false;
};

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

  enum class Shape : std::uint8_t
  {
    Point,              // Single node; no direction
    Vector,             // start -> end, direction = unit(end - start)
    FitVector,          // Principal axis through multiple nodes
    NormalVectorStart,  // Vector where start is the surface point
    NormalVectorEnd,    // Vector where end is the surface point (e.g. Belly: Spine1->Belly)
  };

  using Type = Shape;

  BodyPart() = default;
  BodyPart(RE::Actor* actor, Race race, Name name);

  [[nodiscard]] bool IsValid() const;
  [[nodiscard]] Name GetName() const noexcept { return name; }
  [[nodiscard]] Shape GetShape() const noexcept { return shape; }
  [[nodiscard]] Type GetType() const noexcept { return shape; }
  [[nodiscard]] const ShapeInfo& GetShapeInfo() const noexcept { return info; }
  [[nodiscard]] const Vector* GetVectorInfo() const noexcept
  {
    return info.vector ? &info.vector.value() : nullptr;
  }
  [[nodiscard]] const Collider* GetColliderInfo() const noexcept
  {
    return info.collider ? &info.collider.value() : nullptr;
  }
  [[nodiscard]] const Point3f& GetStart() const noexcept
  {
    static const Point3f zero = Point3f::Zero();
    return info.vector ? info.vector->start : zero;
  }
  [[nodiscard]] const Point3f& GetEnd() const noexcept
  {
    static const Point3f zero = Point3f::Zero();
    return info.vector ? info.vector->end : zero;
  }
  [[nodiscard]] const Vector3f& GetDirection() const noexcept
  {
    static const Vector3f zero = Vector3f::Zero();
    return info.vector ? info.vector->direction : zero;
  }
  [[nodiscard]] float GetLength() const noexcept { return info.vector ? info.vector->length : 0.f; }

  [[nodiscard]] const std::vector<ResolvedNodeInfo>& GetNodeInfos() const noexcept
  {
    return nodeInfos;
  }

  void UpdateNodes();
  void UpdatePosition();

private:
  Vector3f FitVector(const Vector& vectorInfo) const;
  bool IsNodeResolved(const Node& node) const;
  std::vector<Point3f> CollectResolvedNodePositions(const Node& node) const;
  std::vector<Point3f> CollectValidNodePositions(const Vector& vectorInfo) const;
  void UpdateNodePosition(Node& node);
  void UpdateVectorInfo();
  void UpdateColliderInfo();
  Vector3f BuildNodeSpanAxis(const Node& node) const;
  Matrix3f BuildBasis(const Vector3f& primaryAxis, const Node* referenceNode) const;

  ShapeInfo info{};
  std::vector<ResolvedNodeInfo> nodeInfos{};
  RE::Actor* actor = nullptr;
  Name name        = Name::Mouth;
  Shape shape      = Shape::Point;
};

}  // namespace Define
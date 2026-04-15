#pragma once

#include "Define/Gender.h"
#include "Define/Race.h"

#include "eigen3/Eigen/Dense"

namespace Define
{
using Point3f  = Eigen::Vector3f;
using Vector3f = Eigen::Vector3f;
using Matrix3f = Eigen::Matrix3f;

struct Node
{
  std::vector<std::string_view> requestedNodeNames{};
  std::vector<RE::NiPointer<RE::NiNode>> resolvedNodes{};
  Point3f position = Point3f::Zero();
  // 偏移节点视为没有孙节点的虚拟子节点，只需要局部位移。
  // 世界坐标计算公式:
  // world_pos = parent.world_rot * (parent.world_scale * child.local_pos) + parent.world_pos
  RE::NiPoint3 localTranslate = RE::NiPoint3::Zero();

  Node() = default;

  Node(std::string_view nodeName, const RE::NiPoint3& localTranslate = RE::NiPoint3::Zero())
  {
    requestedNodeNames.push_back(nodeName);
    this->localTranslate = localTranslate;
  }
  Node(std::initializer_list<std::string_view> nodeNames,
       const RE::NiPoint3& localTranslate = RE::NiPoint3::Zero())
  {
    requestedNodeNames.assign(nodeNames.begin(), nodeNames.end());
    this->localTranslate = localTranslate;
  }

  [[nodiscard]] bool HasOffset() const noexcept { return localTranslate.SqrLength() > 1e-6f; }
};

struct Vector
{
  Point3f start      = Point3f::Zero();
  Point3f end        = Point3f::Zero();
  Vector3f direction = Vector3f::Zero();
  float length       = 0.f;
};

struct CapsuleCollider
{
  Point3f start      = Point3f::Zero();
  Point3f end        = Point3f::Zero();
  Vector3f direction = Vector3f::Zero();
  float length       = 0.f;
  float radius       = 0.f;
};

struct BoxCollider
{
  Point3f start        = Point3f::Zero();
  Point3f end          = Point3f::Zero();
  Vector3f direction   = Vector3f::Zero();
  float length         = 0.f;
  Point3f center       = Point3f::Zero();
  Matrix3f basis       = Matrix3f::Identity();
  Vector3f halfExtents = Vector3f::Zero();
};

struct CollisionSet
{
  std::vector<CapsuleCollider> capsules{};
  std::vector<BoxCollider> boxes{};
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

  enum class Shape : std::uint8_t
  {
    Point,    // Single anchor; no axis direction
    Segment,  // 2 or more points but not a collider
    CapsuleCollider,
    BoxCollider
  };

  static bool HasBodyPart(Gender gender, Race race, Name name);

  BodyPart() = default;
  BodyPart(RE::Actor* actor, Race race, Name name);

  [[nodiscard]] bool IsValid() const;
  [[nodiscard]] Name GetName() const noexcept { return name; }
  [[nodiscard]] bool IsDirectional() const noexcept { return shape != Shape::Point; }
  [[nodiscard]] Shape GetShape() const noexcept { return shape; }
  [[nodiscard]] const Vector& GetAxisInfo() const noexcept { return vectorInfo; }
  [[nodiscard]] const CollisionSet* GetCollisionInfo() const noexcept
  {
    return collisionInfo ? &(*collisionInfo) : nullptr;
  }

  void UpdateNodes();
  void UpdatePosition();

private:
  std::vector<Node> nodes{};
  Vector vectorInfo{};
  std::optional<CollisionSet> collisionInfo{};
  RE::Actor* actor = nullptr;
  Name name        = Name::Mouth;
  Shape shape      = Shape::Point;
};

}  // namespace Define
#pragma once

#include "Define/Gender.h"
#include "Define/Race.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "eigen3/Eigen/Dense"

namespace Define
{
using Point3f  = Eigen::Vector3f;
using Vector3f = Eigen::Vector3f;
using Matrix3f = Eigen::Matrix3f;

struct Node
{
  std::string_view nodeName{};
  RE::NiPoint3 localTranslate = RE::NiPoint3::Zero();
  RE::NiPointer<RE::NiNode> node{};

  Node() = default;

  Node(std::string_view nodeName, const RE::NiPoint3& localTranslate = RE::NiPoint3::Zero())
      : nodeName(nodeName), localTranslate(localTranslate)
  {}

  [[nodiscard]] bool UpdateNode(RE::Actor* actor)
  {
    node = nullptr;
    if (!actor || nodeName.empty())
      return false;

    auto* object   = actor->GetNodeByName(nodeName);
    auto* resolved = object ? object->AsNode() : nullptr;
    if (!resolved) {
      logger::warn("Node not found for {}: {}", actor->GetDisplayFullName(), nodeName);
      return false;
    }

    node = RE::NiPointer<RE::NiNode>(resolved);
    return true;
  }

  [[nodiscard]] bool IsResolved() const noexcept { return node != nullptr; }

  [[nodiscard]] Point3f GetWorldPosition() const
  {
    if (!node)
      return Point3f::Zero();

    const auto worldPosition =
        node->world.rotate * (node->world.scale * localTranslate) + node->world.translate;
    return Point3f(worldPosition.x, worldPosition.y, worldPosition.z);
  }
};

template <std::size_t N>
using Polygon = std::array<Node, N>;

using Point = std::variant<Node, Polygon<2>, Polygon<3>, Polygon<4>>;
using Face  = std::variant<Polygon<3>, Polygon<4>>;

enum class VolumeType : std::uint8_t
{
  Ball = 0,
  HalfBall,
  Capsule,
  Funnel,
  Surface,
  Envelope,
};

struct Ball
{
  Point center{};
  float radius = 0.f;
};

struct HalfBall
{
  Point center{};
  Point pole{};
  float radius = 0.f;
};

struct Capsule
{
  Point start{};
  Point end{};
  float radius = 0.f;
};

struct Funnel
{
  Face entrance{};
  Point deep{};
  float entranceRadius = 0.f;
  float innerRadius    = 0.f;
  float depth          = 0.f;
};

struct Surface
{
  Face face{};
  float halfThickness = 0.f;
};

struct Envelope
{
  Capsule left{};
  Capsule right{};
  float depthPadding = 0.f;
};

using VolumeData = std::variant<Ball, HalfBall, Capsule, Funnel, Surface, Envelope>;

enum class MotionClass : std::uint8_t
{
  Static = 0,
  Fine,
  Large,
};

struct VolumeProfile
{
  VolumeType topology     = VolumeType::Ball;
  MotionClass motionClass = MotionClass::Static;
  bool canInitiate        = false;
  bool ownsStateSlot      = false;
  bool providesSupport    = false;
  bool exclusiveSource    = false;
  bool exclusiveTarget    = false;
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

struct FunnelCollider
{
  Point3f entranceCenter  = Point3f::Zero();
  Vector3f entranceNormal = Vector3f::Zero();
  float entranceRadius    = 0.f;
  float innerRadius       = 0.f;
  float depth             = 0.f;
  std::vector<Point3f> channelSpline{};
};

struct SurfaceCollider
{
  Point3f center       = Point3f::Zero();
  Matrix3f basis       = Matrix3f::Identity();
  Vector3f halfExtents = Vector3f::Zero();
  Vector3f normal      = Vector3f::Zero();
};

struct EnvelopeCollider
{
  Point3f center       = Point3f::Zero();
  Matrix3f basis       = Matrix3f::Identity();
  Vector3f halfExtents = Vector3f::Zero();
  Vector3f clampAxis   = Vector3f::Zero();
};

struct CollisionSet
{
  std::vector<CapsuleCollider> capsules{};
  std::vector<BoxCollider> boxes{};
  std::vector<FunnelCollider> funnels{};
  std::vector<SurfaceCollider> surfaces{};
  std::vector<EnvelopeCollider> envelopes{};
};

class BodyPart
{
public:
  enum class Name : std::uint8_t
  {
    Mouth,
    Throat,
    BreastLeft,
    BreastRight,
    Cleavage,
    HandLeft,
    HandRight,
    FingerLeft,
    FingerRight,
    Belly,
    ThighLeft,
    ThighRight,
    ThighCleft,
    ButtLeft,
    ButtRight,
    GlutealCleft,
    FootLeft,
    FootRight,
    Vagina,
    Anus,
    Penis,
  };

  static bool HasBodyPart(Gender gender, Race race, Name name);
  [[nodiscard]] static const VolumeProfile& GetVolumeProfile(Name name);

  BodyPart() = default;
  BodyPart(RE::Actor* actor, Race race, Name name);

  [[nodiscard]] bool IsValid() const noexcept { return valid; }
  [[nodiscard]] Name GetName() const noexcept { return name; }
  [[nodiscard]] bool IsDirectional() const noexcept;
  [[nodiscard]] VolumeType GetTopology() const noexcept { return GetVolumeProfile().topology; }
  [[nodiscard]] const VolumeProfile& GetVolumeProfile() const noexcept;
  [[nodiscard]] const Vector& GetAxisInfo() const noexcept { return vectorInfo; }
  [[nodiscard]] const CollisionSet* GetCollisionInfo() const noexcept
  {
    return collisionInfo ? &(*collisionInfo) : nullptr;
  }
  [[nodiscard]] const std::vector<VolumeData>& GetVolumes() const noexcept { return volumes; }

  void UpdateNodes();
  void UpdatePosition();

private:
  std::vector<VolumeData> volumes{};
  Vector vectorInfo{};
  std::optional<CollisionSet> collisionInfo{};
  RE::Actor* actor = nullptr;
  Name name        = Name::Mouth;
  bool valid       = false;
};

}  // namespace Define
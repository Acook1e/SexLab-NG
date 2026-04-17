#pragma once

#include "Define/Gender.h"
#include "Define/Race.h"

#include "eigen3/Eigen/Dense"

namespace Define
{

using Point3f  = Eigen::Vector3f;
using Vector3f = Eigen::Vector3f;
using Matrix3f = Eigen::Matrix3f;

static constexpr float kEpsilon = 1e-6f;

// -----------------------------------------------------------------------------
// 骨骼节点与偏移
// -----------------------------------------------------------------------------

struct Node
{
  std::string nodeName;
  RE::NiPointer<RE::NiNode> nodePtr;
  RE::NiPoint3 localTranslate = RE::NiPoint3::Zero();

  Node() = default;
  Node(std::string_view name, const RE::NiPoint3& offset = RE::NiPoint3::Zero());
  bool Resolve(RE::Actor* actor);
  [[nodiscard]] Point3f GetWorldPosition() const;
};

// Point 可以是单个节点，也可以是两个节点的中点
using Point = std::variant<Node, std::array<Node, 2>>;

bool ResolvePoint(Point& point, RE::Actor* actor);
Point3f GetPointWorld(const Point& point);

// -----------------------------------------------------------------------------
// 面类型（用于漏斗入口定义）
// -----------------------------------------------------------------------------

struct Triangle
{
  std::array<Node, 3> vertices;
  bool Resolve(RE::Actor* actor);
  std::array<Point3f, 3> GetWorldVertices() const;
};

struct Polygon
{
  std::vector<Node> vertices;  // 至少3个
  bool Resolve(RE::Actor* actor);
  std::vector<Point3f> GetWorldVertices() const;
};

using SurfaceDef = std::variant<Triangle, Polygon>;

// -----------------------------------------------------------------------------
// 体积类型定义
// -----------------------------------------------------------------------------

enum class VolumeType : std::uint8_t
{
  HalfBall,
  Capsule,
  SplineCapsule,
  Funnel,
  Surface,
  EnvelopeHalfBall,
  EnvelopeCapsule
};

// 前向声明
struct HalfBall;
struct Capsule;
struct SplineCapsule;
struct Funnel;
struct SurfaceVolume;
template <typename T>
struct Envelope;

struct HalfBall
{
  Point center;  // 半球底面中心
  Point pole;    // 半球顶点（方向）
  float radius = 0.f;

  // 世界缓存
  Point3f worldCenter     = Point3f::Zero();
  Point3f worldPole       = Point3f::Zero();
  Vector3f worldDirection = Vector3f::Zero();

  void UpdateWorld();
  [[nodiscard]] std::pair<Point3f, Point3f> GetAxis() const { return {worldCenter, worldPole}; }
  static constexpr VolumeType kType = VolumeType::HalfBall;
};

struct Capsule
{
  Point start;
  Point end;
  float radius = 0.f;

  // 世界缓存
  Point3f worldStart      = Point3f::Zero();
  Point3f worldEnd        = Point3f::Zero();
  Vector3f worldDirection = Vector3f::Zero();
  float worldLength       = 0.f;

  void UpdateWorld();
  [[nodiscard]] std::pair<Point3f, Point3f> GetAxis() const { return {worldStart, worldEnd}; }
  static constexpr VolumeType kType = VolumeType::Capsule;
};

struct SplineCapsule
{
  std::vector<Point> controlPoints;  // 至少2个
  float radius = 0.f;

  // 世界缓存：等距重采样后的点（用于高效查询）
  std::vector<Point3f> worldSamples;
  float worldLength = 0.f;

  void UpdateWorld();
  [[nodiscard]] Point3f SamplePoint(float t) const;  // t∈[0,1]
  [[nodiscard]] Vector3f SampleTangent(float t) const;
  static constexpr VolumeType kType = VolumeType::SplineCapsule;
};

struct Funnel
{
  SurfaceDef entrance;         // 入口多边形（三角形或多边形）
  Point deep;                  // 最深点
  float entranceRadius = 0.f;  // 若为0则自动从入口拟合
  float deepRadius     = 0.f;  // 深处半径（0表示尖点）
  float depth          = 0.f;  // 轴向深度（可自动计算）

  // 三段区域参数
  float coreDepthRatio = 0.3f;
  float captureExtend  = 5.0f;
  float exitTolerance  = 1.8f;

  // 世界缓存
  Point3f worldEntranceCenter = Point3f::Zero();
  Point3f worldDeep           = Point3f::Zero();
  Vector3f worldDirection     = Vector3f::Zero();  // 入口指向深处
  float worldLength           = 0.f;
  float worldEntranceRadius   = 0.f;
  float worldDeepRadius       = 0.f;

  void UpdateWorld();
  static constexpr VolumeType kType = VolumeType::Funnel;
};

struct SurfaceVolume
{
  SurfaceDef shape;

  // 世界缓存
  std::vector<Point3f> worldVertices;
  Point3f worldCenter     = Point3f::Zero();
  Vector3f worldDirection = Vector3f::Zero();  // 表面法向

  void UpdateWorld();
  static constexpr VolumeType kType = VolumeType::Surface;
};

template <typename T>
struct Envelope
{
  static_assert(std::is_same_v<T, HalfBall> || std::is_same_v<T, Capsule>);
  T* left  = nullptr;
  T* right = nullptr;

  // 运行时推导的夹缝几何
  struct Runtime
  {
    Vector3f direction     = Vector3f::Zero();
    Point3f referencePoint = Point3f::Zero();
    float totalLength      = 0.f;
    float typicalWidth     = 0.f;
    float startT           = 0.f;
    float endT             = 1.f;
  } runtime;

  void UpdateRuntime();
  static constexpr VolumeType kType =
      std::is_same_v<T, HalfBall> ? VolumeType::EnvelopeHalfBall : VolumeType::EnvelopeCapsule;
};

using VolumeData = std::variant<HalfBall, Capsule, SplineCapsule, Funnel, SurfaceVolume,
                                Envelope<HalfBall>, Envelope<Capsule>>;

struct SampleResult
{
  Point3f point      = Point3f::Zero();
  Vector3f direction = Vector3f::Zero();
  float t            = 0.f;
  float radius       = 0.f;
  bool valid         = false;
};

struct ProjectionResult
{
  Point3f closestPoint = Point3f::Zero();
  Vector3f direction   = Vector3f::Zero();
  float t              = 0.f;
  float radialDistance = 0.f;
  float signedDistance = 0.f;
  bool inside          = false;
  bool valid           = false;
};

struct SurfaceProjection
{
  ProjectionResult projection;
  Point3f planePoint          = Point3f::Zero();
  float planeDistance         = 0.f;
  float planarDistance        = 0.f;
  bool insideSurface          = false;
  std::uint32_t triangleIndex = 0;
};

struct FunnelProjection
{
  ProjectionResult projection;
  float depth           = 0.f;
  float normalizedDepth = 0.f;
  float localRadius     = 0.f;
  bool insideCore       = false;
  bool insideCapture    = false;
};

struct EnvelopeProjection
{
  ProjectionResult projection;
  float leftDistance    = 0.f;
  float rightDistance   = 0.f;
  float localWidth      = 0.f;
  float enclosureFactor = 0.f;
};

[[nodiscard]] SampleResult SampleVolume(const VolumeData& volume, float t);
[[nodiscard]] ProjectionResult ProjectPointOntoVolume(const VolumeData& volume,
                                                      const Point3f& point);
[[nodiscard]] SurfaceProjection ProjectPointOntoSurface(const SurfaceVolume& volume,
                                                        const Point3f& point);
[[nodiscard]] FunnelProjection ProjectPointOntoFunnel(const Funnel& volume, const Point3f& point);
[[nodiscard]] EnvelopeProjection ProjectPointOntoEnvelope(const Envelope<HalfBall>& volume,
                                                          const Point3f& point);
[[nodiscard]] EnvelopeProjection ProjectPointOntoEnvelope(const Envelope<Capsule>& volume,
                                                          const Point3f& point);

// -----------------------------------------------------------------------------
// ActorBody 类
// -----------------------------------------------------------------------------

class ActorBody
{
public:
  enum class PartName : std::uint8_t
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
    Penis
  };

  static bool HasBodyPart(Race race, Gender gender, PartName name);

  ActorBody() = default;
  ActorBody(RE::Actor* actor, Race race, Gender gender);

  void ResolveNodes();
  void UpdateWorld();

  [[nodiscard]] const VolumeData* GetPart(PartName name) const;
  [[nodiscard]] bool HasPart(PartName name) const { return volumes.count(name) > 0; }

private:
  RE::Actor* m_actor = nullptr;
  std::unordered_map<PartName, VolumeData> volumes;

  void BuildVolumes(Race race, Gender gender);
};

}  // namespace Define
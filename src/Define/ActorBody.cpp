#include "Define/ActorBody.h"

#include "magic_enum/magic_enum.hpp"

namespace Define
{

namespace
{

  using SchlongNodes = std::array<std::string_view, 3>;

  // -----------------------------------------------------------------------------
  // 辅助宏与静态常量
  // -----------------------------------------------------------------------------

  static const std::unordered_map<Define::Race::Type, SchlongNodes> kSchlongMap{
      // 人类单独定义
      // {Define::Race::Type::Human,
      // {"NPC Genitals01 [Gen01]", "NPC Genitals04 [Gen04]", "NPC Genitals06 [Gen06]"}},
      {Define::Race::Type::Bear, {"BearD 5", "BearD 7", "BearD 9"}},
      {Define::Race::Type::Boar, {"BoarDick04", "BoarDick05", "BoarDick06"}},
      {Define::Race::Type::BoarMounted, {"BoarDick04", "BoarDick05", "BoarDick06"}},
      {Define::Race::Type::Chaurus, {"CO 4", "CO 7", "CO 9"}},
      {Define::Race::Type::ChaurusHunter, {"CO 3", "CO 5", "CO 8"}},
      {Define::Race::Type::ChaurusReaper, {"CO 4", "CO 7", "CO 9"}},
      {Define::Race::Type::Deer, {"ElkD03", "ElkD05", "ElkD06"}},
      {Define::Race::Type::Dog, {"CDPenis 3", "CDPenis 5", "CDPenis 7"}},
      {Define::Race::Type::DragonPriest, {"DD 2", "DD 4", "DD 6"}},
      {Define::Race::Type::Draugr, {"DD 2", "DD 4", "DD 6"}},
      {Define::Race::Type::DwarvenCenturion,
       {"DwarvenBattery01", "DwarvenBattery02", "DwarvenInjectorLid"}},
      {Define::Race::Type::DwarvenSpider, {"Dildo01", "Dildo02", "Dildo03"}},
      {Define::Race::Type::Falmer, {"FD 3", "FD 5", "FD 7"}},
      {Define::Race::Type::Fox, {"CDPenis 3", "CDPenis 5", "CDPenis 7"}},
      {Define::Race::Type::FrostAtronach, {"NPC IceGenital03", "NPC IcePenis01", "NPC IcePenis02"}},
      {Define::Race::Type::Gargoyle, {"GD 3", "GD 5", "GD 7"}},
      {Define::Race::Type::Giant, {"GS 3", "GS 5", "GS 7"}},
      {Define::Race::Type::GiantSpider, {"CO 5", "CO 7", "CO tip"}},
      {Define::Race::Type::Horse, {"HS 5", "HS 6", "HorsePenisFlareTop2"}},
      {Define::Race::Type::LargeSpider, {"CO 5", "CO 7", "CO tip"}},
      {Define::Race::Type::Lurker, {"GS 3", "GS 5", "GS 7"}},
      {Define::Race::Type::Riekling, {"RD 2", "RD 4", "RD 5"}},
      {Define::Race::Type::Sabrecat, {"SCD 3", "SCD 5", "SCD 7"}},
      {Define::Race::Type::Skeever, {"SkeeverD 03", "SkeeverD 05", "SkeeverD 07"}},
      {Define::Race::Type::Spider, {"CO 5", "CO 7", "CO tip"}},
      {Define::Race::Type::StormAtronach, {"", "Torso Rock 2", ""}},
      {Define::Race::Type::Troll, {"TD 3", "TD 5", "TD 7"}},
      {Define::Race::Type::VampireLord, {"VLDick03", "VLDick05", "VLDick06"}},
      {Define::Race::Type::Werebear, {"WWD 5", "WWD 7", "WWD 9"}},
      {Define::Race::Type::Werewolf, {"WWD 5", "WWD 7", "WWD 9"}},
      {Define::Race::Type::Wolf, {"CDPenis 3", "CDPenis 5", "CDPenis 7"}},
  };

  [[nodiscard]] Vector3f SafeNormalized(const Vector3f& value,
                                        const Vector3f& fallback = Vector3f::UnitZ())
  {
    const float length = value.norm();
    if (length <= kEpsilon) {
      return fallback;
    }
    return value / length;
  }

  [[nodiscard]] std::pair<std::string_view, std::string_view>
  SelectCapsuleEndpoints(const SchlongNodes& nodes)
  {
    const std::string_view start = !nodes[0].empty() ? nodes[0] : nodes[1];
    const std::string_view end   = !nodes[2].empty() ? nodes[2] : nodes[1];
    return {start, end};
  }

  [[nodiscard]] float Clamp01(float value)
  {
    return std::clamp(value, 0.f, 1.f);
  }

  [[nodiscard]] Point3f LerpPoint(const Point3f& a, const Point3f& b, float t)
  {
    return a + (b - a) * t;
  }

  [[nodiscard]] float LerpScalar(float a, float b, float t)
  {
    return a + (b - a) * t;
  }

  [[nodiscard]] Point3f ClosestPointOnSegment(const Point3f& point, const Point3f& start,
                                              const Point3f& end, float* outT = nullptr)
  {
    const Vector3f segment = end - start;
    const float lengthSq   = segment.squaredNorm();
    float t                = 0.f;
    if (lengthSq > kEpsilon) {
      t = Clamp01((point - start).dot(segment) / lengthSq);
    }
    if (outT) {
      *outT = t;
    }
    return start + segment * t;
  }

  [[nodiscard]] Point3f ClosestPointOnTriangle(const Point3f& point, const Point3f& a,
                                               const Point3f& b, const Point3f& c)
  {
    const Vector3f ab = b - a;
    const Vector3f ac = c - a;
    const Vector3f ap = point - a;

    const float d1 = ab.dot(ap);
    const float d2 = ac.dot(ap);
    if (d1 <= 0.f && d2 <= 0.f)
      return a;

    const Vector3f bp = point - b;
    const float d3    = ab.dot(bp);
    const float d4    = ac.dot(bp);
    if (d3 >= 0.f && d4 <= d3)
      return b;

    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) {
      const float v = d1 / (d1 - d3);
      return a + ab * v;
    }

    const Vector3f cp = point - c;
    const float d5    = ab.dot(cp);
    const float d6    = ac.dot(cp);
    if (d6 >= 0.f && d5 <= d6)
      return c;

    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) {
      const float w = d2 / (d2 - d6);
      return a + ac * w;
    }

    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) {
      const Vector3f bc = c - b;
      const float w     = (d4 - d3) / ((d4 - d3) + (d5 - d6));
      return b + bc * w;
    }

    const float denom = 1.f / (va + vb + vc);
    const float v     = vb * denom;
    const float w     = vc * denom;
    return a + ab * v + ac * w;
  }

  [[nodiscard]] Point3f ClosestPointOnPolygon(const Point3f& point,
                                              const std::vector<Point3f>& vertices)
  {
    if (vertices.empty())
      return Point3f::Zero();
    if (vertices.size() == 1)
      return vertices.front();
    if (vertices.size() == 2)
      return ClosestPointOnSegment(point, vertices[0], vertices[1]);

    Point3f bestPoint = ClosestPointOnTriangle(point, vertices[0], vertices[1], vertices[2]);
    float bestDistSq  = (point - bestPoint).squaredNorm();

    for (std::size_t index = 1; index + 1 < vertices.size(); ++index) {
      Point3f candidate =
          ClosestPointOnTriangle(point, vertices[0], vertices[index], vertices[index + 1]);
      const float distSq = (point - candidate).squaredNorm();
      if (distSq < bestDistSq) {
        bestDistSq = distSq;
        bestPoint  = candidate;
      }
    }

    return bestPoint;
  }

  [[nodiscard]] float DistanceToAxis(const Point3f& point, const Point3f& start, const Point3f& end,
                                     float* outT = nullptr)
  {
    const Point3f closest = ClosestPointOnSegment(point, start, end, outT);
    return (point - closest).norm();
  }

  [[nodiscard]] bool IsPointInTriangleFan(const Point3f& point, const Point3f& a, const Point3f& b,
                                          const Point3f& c)
  {
    return (ClosestPointOnTriangle(point, a, b, c) - point).squaredNorm() <= kEpsilon * kEpsilon;
  }

  template <typename T>
  [[nodiscard]] float DistanceToBoundarySurface(const Point3f& point, const T& boundary)
  {
    if constexpr (std::is_same_v<T, Capsule>) {
      float axisT = 0.f;
      const Point3f closest =
          ClosestPointOnSegment(point, boundary.worldStart, boundary.worldEnd, &axisT);
      return (point - closest).norm() - boundary.radius;
    } else {
      return (point - boundary.worldCenter).norm() - boundary.radius;
    }
  }

}  // namespace

// -----------------------------------------------------------------------------
// Node 实现
// -----------------------------------------------------------------------------

Node::Node(std::string_view name, const RE::NiPoint3& offset)
    : nodeName(name), localTranslate(offset)
{}

bool Node::Resolve(RE::Actor* actor)
{
  if (!actor)
    return false;
  auto* obj = actor->GetNodeByName(nodeName);
  auto* res = obj ? obj->AsNode() : nullptr;
  if (!res) {
    logger::warn("Node not found: {}", nodeName);
    nodePtr = nullptr;
    return false;
  }
  nodePtr = RE::NiPointer<RE::NiNode>(res);
  return true;
}

Point3f Node::GetWorldPosition() const
{
  if (!nodePtr)
    return Point3f::Zero();
  const auto& t      = nodePtr->world.translate;
  const auto& r      = nodePtr->world.rotate;
  const auto& s      = nodePtr->world.scale;
  RE::NiPoint3 world = r * (s * localTranslate) + t;
  return Point3f(world.x, world.y, world.z);
}

// -----------------------------------------------------------------------------
// Point 辅助函数
// -----------------------------------------------------------------------------

bool ResolvePoint(Point& point, RE::Actor* actor)
{
  return std::visit(
      [actor](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Node>) {
          return arg.Resolve(actor);
        } else {
          bool ok = true;
          for (auto& n : arg)
            ok &= n.Resolve(actor);
          return ok;
        }
      },
      point);
}

Point3f GetPointWorld(const Point& point)
{
  return std::visit(
      [](auto&& arg) -> Point3f {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Node>) {
          return arg.GetWorldPosition();
        } else {
          return (arg[0].GetWorldPosition() + arg[1].GetWorldPosition()) * 0.5f;
        }
      },
      point);
}

// -----------------------------------------------------------------------------
// Triangle / Polygon 实现
// -----------------------------------------------------------------------------

bool Triangle::Resolve(RE::Actor* actor)
{
  bool ok = true;
  for (auto& v : vertices)
    ok &= v.Resolve(actor);
  return ok;
}

std::array<Point3f, 3> Triangle::GetWorldVertices() const
{
  return {vertices[0].GetWorldPosition(), vertices[1].GetWorldPosition(),
          vertices[2].GetWorldPosition()};
}

bool Polygon::Resolve(RE::Actor* actor)
{
  bool ok = true;
  for (auto& v : vertices)
    ok &= v.Resolve(actor);
  return ok;
}

std::vector<Point3f> Polygon::GetWorldVertices() const
{
  std::vector<Point3f> world;
  world.reserve(vertices.size());
  for (const auto& v : vertices)
    world.push_back(v.GetWorldPosition());
  return world;
}

// -----------------------------------------------------------------------------
// 体积类型 UpdateWorld 实现
// -----------------------------------------------------------------------------

void HalfBall::UpdateWorld()
{
  worldCenter    = GetPointWorld(center);
  worldPole      = GetPointWorld(pole);
  Vector3f diff  = worldPole - worldCenter;
  worldDirection = SafeNormalized(diff);
}

void Capsule::UpdateWorld()
{
  worldStart     = GetPointWorld(start);
  worldEnd       = GetPointWorld(end);
  Vector3f diff  = worldEnd - worldStart;
  worldLength    = diff.norm();
  worldDirection = SafeNormalized(diff);
}

void SplineCapsule::UpdateWorld()
{
  if (controlPoints.size() < 2) {
    worldSamples.clear();
    worldLength = 0.f;
    return;
  }

  // 1. 获取所有控制点的世界坐标
  std::vector<Point3f> ctrl;
  ctrl.reserve(controlPoints.size());
  for (const auto& pt : controlPoints) {
    ctrl.push_back(GetPointWorld(pt));
  }

  // 2. 计算累积弧长（线性插值）
  std::vector<float> arcLengths = {0.f};
  for (size_t i = 1; i < ctrl.size(); ++i) {
    arcLengths.push_back(arcLengths.back() + (ctrl[i] - ctrl[i - 1]).norm());
  }
  worldLength = arcLengths.back();
  if (worldLength < kEpsilon) {
    worldSamples = ctrl;
    return;
  }

  // 3. 等距重采样（目标段长 = radius * 1.5，但至少5段）
  float targetStep = (std::max)(radius * 1.5f, worldLength / 20.0f);
  int numSegments  = (std::max)(5, static_cast<int>(std::ceil(worldLength / targetStep)));
  float step       = worldLength / numSegments;

  worldSamples.clear();
  worldSamples.reserve(numSegments + 1);
  for (int i = 0; i <= numSegments; ++i) {
    float s = i * step;
    // 找到s所在的段
    auto it = std::upper_bound(arcLengths.begin(), arcLengths.end(), s);
    if (it == arcLengths.end()) {
      worldSamples.push_back(ctrl.back());
    } else {
      size_t idx = it - arcLengths.begin();
      if (idx == 0) {
        worldSamples.push_back(ctrl[0]);
      } else {
        float t   = (s - arcLengths[idx - 1]) / (arcLengths[idx] - arcLengths[idx - 1]);
        Point3f p = ctrl[idx - 1] * (1.f - t) + ctrl[idx] * t;
        worldSamples.push_back(p);
      }
    }
  }
}

Point3f SplineCapsule::SamplePoint(float t) const
{
  if (worldSamples.empty())
    return Point3f::Zero();
  t          = std::clamp(t, 0.f, 1.f);
  float pos  = t * (worldSamples.size() - 1);
  size_t idx = static_cast<size_t>(pos);
  float frac = pos - idx;
  if (idx >= worldSamples.size() - 1)
    return worldSamples.back();
  return worldSamples[idx] * (1.f - frac) + worldSamples[idx + 1] * frac;
}

Vector3f SplineCapsule::SampleTangent(float t) const
{
  if (worldSamples.size() < 2)
    return Vector3f::UnitZ();
  t          = std::clamp(t, 0.f, 1.f);
  float pos  = t * (worldSamples.size() - 1);
  size_t idx = static_cast<size_t>(pos);
  if (idx >= worldSamples.size() - 1)
    idx = worldSamples.size() - 2;
  Vector3f dir = worldSamples[idx + 1] - worldSamples[idx];
  return SafeNormalized(dir);
}

// 辅助：从点集拟合圆（最小二乘）
static std::pair<Point3f, float> FitCircleFromPoints(const std::vector<Point3f>& points,
                                                     const Vector3f& normal)
{
  // 简化实现：计算中心点，然后取平均半径
  (void)normal;

  Point3f center = Point3f::Zero();
  for (const auto& point : points) {
    center += point;
  }
  center /= static_cast<float>(points.size());

  float radius = 0.f;
  for (const auto& p : points) {
    radius += (p - center).norm();
  }
  radius /= static_cast<float>(points.size());
  return {center, radius};
}

void Funnel::UpdateWorld()
{
  // 1. 获取入口多边形顶点
  std::vector<Point3f> polyVerts;
  std::visit(
      [&polyVerts](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Triangle>) {
          auto arr = arg.GetWorldVertices();
          polyVerts.assign(arr.begin(), arr.end());
        } else if constexpr (std::is_same_v<T, Polygon>) {
          polyVerts = arg.GetWorldVertices();
        }
      },
      entrance);

  if (polyVerts.size() < 3) {
    // 退化处理
    worldEntranceCenter = Point3f::Zero();
    worldDirection      = Vector3f::UnitZ();
    worldLength         = 0.f;
    return;
  }

  worldDeep = GetPointWorld(deep);

  // 2. 计算入口中心
  worldEntranceCenter = Point3f::Zero();
  for (const auto& vertex : polyVerts) {
    worldEntranceCenter += vertex;
  }
  worldEntranceCenter /= static_cast<float>(polyVerts.size());

  // 3. 计算入口平面法向：PCA
  Matrix3f cov = Matrix3f::Zero();
  for (const auto& v : polyVerts) {
    Vector3f d = v - worldEntranceCenter;
    cov += d * d.transpose();
  }
  Eigen::SelfAdjointEigenSolver<Matrix3f> solver(cov);
  Vector3f entranceNormal = solver.eigenvectors().col(0);  // 最小特征值对应

  // 确保指向深处
  if (entranceNormal.dot(worldDeep - worldEntranceCenter) < 0.f) {
    entranceNormal = -entranceNormal;
  }

  // 4. 投影顶点到平面并拟合圆
  std::vector<Point3f> projVerts;
  for (const auto& v : polyVerts) {
    projVerts.push_back(v - entranceNormal * (entranceNormal.dot(v - worldEntranceCenter)));
  }
  auto [center, radius] = FitCircleFromPoints(projVerts, entranceNormal);
  worldEntranceCenter   = center;
  worldEntranceRadius   = (entranceRadius > kEpsilon) ? entranceRadius : radius;

  // 5. 轴向长度
  Vector3f diff  = worldDeep - worldEntranceCenter;
  worldLength    = diff.norm();
  worldDirection = SafeNormalized(diff, entranceNormal);

  worldDeepRadius = (deepRadius > kEpsilon) ? deepRadius : worldEntranceRadius * 0.5f;
}

void SurfaceVolume::UpdateWorld()
{
  std::visit(
      [this](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, Triangle>) {
          const auto vertices = arg.GetWorldVertices();
          worldVertices.assign(vertices.begin(), vertices.end());
        } else if constexpr (std::is_same_v<T, Polygon>) {
          worldVertices = arg.GetWorldVertices();
        }
      },
      shape);

  if (worldVertices.empty()) {
    worldCenter    = Point3f::Zero();
    worldDirection = Vector3f::Zero();
    return;
  }

  worldCenter = Point3f::Zero();
  for (const auto& vertex : worldVertices) {
    worldCenter += vertex;
  }
  worldCenter /= static_cast<float>(worldVertices.size());

  Vector3f normal = Vector3f::Zero();
  if (worldVertices.size() >= 3) {
    for (std::size_t index = 1; index + 1 < worldVertices.size(); ++index) {
      normal += (worldVertices[index] - worldVertices[0])
                    .cross(worldVertices[index + 1] - worldVertices[0]);
    }
  }
  worldDirection = SafeNormalized(normal);
}

template <typename T>
void Envelope<T>::UpdateRuntime()
{
  if (!left || !right)
    return;

  auto [lStart, lEnd] = left->GetAxis();
  auto [rStart, rEnd] = right->GetAxis();

  Vector3f leftDir  = SafeNormalized(lEnd - lStart);
  Vector3f rightDir = SafeNormalized(rEnd - rStart);
  runtime.direction =
      SafeNormalized(leftDir + rightDir, SafeNormalized((lEnd + rEnd) - (lStart + rStart)));

  Point3f midStart       = (lStart + rStart) * 0.5f;
  Point3f midEnd         = (lEnd + rEnd) * 0.5f;
  runtime.referencePoint = midStart;
  runtime.totalLength    = (midEnd - midStart).norm();

  // 典型宽度简化计算
  auto sampleWidth = [&](float t) {
    Point3f p          = midStart + runtime.direction * (t * runtime.totalLength);
    auto distToSurface = [&p](const auto& cap) {
      if constexpr (std::is_same_v<T, Capsule>) {
        Vector3f seg = cap.worldEnd - cap.worldStart;
        float len    = seg.norm();
        if (len < kEpsilon)
          return (p - cap.worldStart).norm() - cap.radius;
        Vector3f dir    = seg / len;
        float proj      = std::clamp((p - cap.worldStart).dot(dir), 0.f, len);
        Point3f closest = cap.worldStart + dir * proj;
        return (p - closest).norm() - cap.radius;
      } else {
        return (p - cap.worldCenter).norm() - cap.radius;
      }
    };
    float dL = distToSurface(*left);
    float dR = distToSurface(*right);
    return (std::max)(0.f, dL + dR);
  };

  runtime.typicalWidth = sampleWidth(0.5f);
  if (runtime.typicalWidth < kEpsilon)
    runtime.typicalWidth = 1.f;
}

// 显式实例化
template struct Envelope<HalfBall>;
template struct Envelope<Capsule>;

SurfaceProjection ProjectPointOntoSurface(const SurfaceVolume& volume, const Point3f& point)
{
  SurfaceProjection result;
  if (volume.worldVertices.size() < 3)
    return result;

  const float planeDistance = (point - volume.worldCenter).dot(volume.worldDirection);
  result.planeDistance      = planeDistance;
  result.planePoint         = point - volume.worldDirection * planeDistance;

  bool insideSurface       = false;
  std::uint32_t triangleId = 0;
  Point3f closestPoint = ClosestPointOnTriangle(result.planePoint, volume.worldVertices[0],
                                                volume.worldVertices[1], volume.worldVertices[2]);
  float bestPlanarSq   = (result.planePoint - closestPoint).squaredNorm();

  for (std::size_t index = 1; index + 1 < volume.worldVertices.size(); ++index) {
    const Point3f& a = volume.worldVertices[0];
    const Point3f& b = volume.worldVertices[index];
    const Point3f& c = volume.worldVertices[index + 1];

    if (!insideSurface && IsPointInTriangleFan(result.planePoint, a, b, c)) {
      insideSurface = true;
      triangleId    = static_cast<std::uint32_t>(index - 1);
      closestPoint  = result.planePoint;
      bestPlanarSq  = 0.f;
      break;
    }

    const Point3f candidate = ClosestPointOnTriangle(result.planePoint, a, b, c);
    const float candidateSq = (result.planePoint - candidate).squaredNorm();
    if (candidateSq < bestPlanarSq) {
      bestPlanarSq = candidateSq;
      closestPoint = candidate;
      triangleId   = static_cast<std::uint32_t>(index - 1);
    }
  }

  result.insideSurface             = insideSurface;
  result.triangleIndex             = triangleId;
  result.planarDistance            = std::sqrt(bestPlanarSq);
  result.projection.closestPoint   = closestPoint;
  result.projection.direction      = volume.worldDirection;
  result.projection.t              = 0.f;
  result.projection.radialDistance = result.planarDistance;
  result.projection.signedDistance = planeDistance;
  result.projection.inside         = insideSurface;
  result.projection.valid          = true;
  return result;
}

FunnelProjection ProjectPointOntoFunnel(const Funnel& volume, const Point3f& point)
{
  FunnelProjection result;
  if (volume.worldLength <= kEpsilon) {
    result.projection.closestPoint   = volume.worldEntranceCenter;
    result.projection.direction      = volume.worldDirection;
    result.projection.radialDistance = (point - volume.worldEntranceCenter).norm();
    result.projection.signedDistance =
        result.projection.radialDistance - volume.worldEntranceRadius;
    result.projection.inside = result.projection.signedDistance <= 0.f;
    result.projection.valid  = true;
  } else {
    float axisT = 0.f;
    result.projection.closestPoint =
        ClosestPointOnSegment(point, volume.worldEntranceCenter, volume.worldDeep, &axisT);
    result.projection.direction      = volume.worldDirection;
    result.projection.t              = axisT;
    result.projection.radialDistance = (point - result.projection.closestPoint).norm();
    result.localRadius = LerpScalar(volume.worldEntranceRadius, volume.worldDeepRadius, axisT);
    result.projection.signedDistance = result.projection.radialDistance - result.localRadius;
    result.projection.inside         = result.projection.signedDistance <= 0.f;
    result.projection.valid          = true;
  }

  if (!result.projection.valid)
    return result;

  result.depth           = result.projection.t * volume.worldLength;
  result.normalizedDepth = result.projection.t;
  result.localRadius =
      LerpScalar(volume.worldEntranceRadius, volume.worldDeepRadius, result.normalizedDepth);
  result.insideCore = result.projection.inside && result.normalizedDepth >= volume.coreDepthRatio;
  result.insideCapture = result.projection.signedDistance <= volume.captureExtend;
  return result;
}

template <typename T>
EnvelopeProjection ProjectPointOntoEnvelopeImpl(const Envelope<T>& volume, const Point3f& point)
{
  EnvelopeProjection result;
  const Point3f axisEnd =
      volume.runtime.referencePoint + volume.runtime.direction * volume.runtime.totalLength;
  float axisT = 0.f;
  result.projection.closestPoint =
      ClosestPointOnSegment(point, volume.runtime.referencePoint, axisEnd, &axisT);
  result.projection.direction      = volume.runtime.direction;
  result.projection.t              = Clamp01(axisT);
  result.projection.radialDistance = (point - result.projection.closestPoint).norm();
  result.projection.signedDistance =
      result.projection.radialDistance - volume.runtime.typicalWidth * 0.5f;
  result.projection.inside = result.projection.signedDistance <= 0.f;
  result.projection.valid  = volume.runtime.totalLength > kEpsilon;

  if (!result.projection.valid || !volume.left || !volume.right)
    return result;

  const float axisScale  = result.projection.t * volume.runtime.totalLength;
  const Point3f midPoint = volume.runtime.referencePoint + volume.runtime.direction * axisScale;
  result.leftDistance    = DistanceToBoundarySurface(midPoint, *volume.left);
  result.rightDistance   = DistanceToBoundarySurface(midPoint, *volume.right);
  result.localWidth = (std::max)(0.f, result.leftDistance) + (std::max)(0.f, result.rightDistance);

  const float widthDenom = (std::max)(result.localWidth, kEpsilon);
  const float symmetry   = 1.f - std::abs(result.leftDistance - result.rightDistance) / widthDenom;
  const float centered   = 1.f - std::abs(result.projection.signedDistance) /
                                     (std::max)(volume.runtime.typicalWidth * 0.5f, kEpsilon);
  result.enclosureFactor = Clamp01(symmetry) * Clamp01(centered);
  return result;
}

EnvelopeProjection ProjectPointOntoEnvelope(const Envelope<HalfBall>& volume, const Point3f& point)
{
  return ProjectPointOntoEnvelopeImpl(volume, point);
}

EnvelopeProjection ProjectPointOntoEnvelope(const Envelope<Capsule>& volume, const Point3f& point)
{
  return ProjectPointOntoEnvelopeImpl(volume, point);
}

namespace
{

  template <typename T>
  SampleResult MakeAxialSample(const T& volume, const Point3f& start, const Point3f& end,
                               const Vector3f& direction, float radius, float t)
  {
    SampleResult result;
    result.t         = Clamp01(t);
    result.point     = LerpPoint(start, end, result.t);
    result.direction = direction;
    result.radius    = radius;
    result.valid     = true;
    return result;
  }

  template <typename T>
  ProjectionResult MakeAxialProjection(const T& volume, const Point3f& point, const Point3f& start,
                                       const Point3f& end, const Vector3f& direction, float radius)
  {
    ProjectionResult result;
    float axisT           = 0.f;
    result.closestPoint   = ClosestPointOnSegment(point, start, end, &axisT);
    result.direction      = direction;
    result.t              = axisT;
    result.radialDistance = (point - result.closestPoint).norm();
    result.signedDistance = result.radialDistance - radius;
    result.inside         = result.signedDistance <= 0.f;
    result.valid          = true;
    return result;
  }

  [[nodiscard]] SampleResult SampleVolumeImpl(const HalfBall& volume, float t)
  {
    SampleResult result    = MakeAxialSample(volume, volume.worldCenter, volume.worldPole,
                                             volume.worldDirection, volume.radius, t);
    const float axialRatio = result.t;
    result.radius = volume.radius * std::sqrt((std::max)(0.f, 1.f - axialRatio * axialRatio));
    return result;
  }

  [[nodiscard]] ProjectionResult ProjectPointImpl(const HalfBall& volume, const Point3f& point)
  {
    ProjectionResult result = MakeAxialProjection(
        volume, point, volume.worldCenter, volume.worldPole, volume.worldDirection, volume.radius);
    const float axialRatio = result.t;
    const float localRadius =
        volume.radius * std::sqrt((std::max)(0.f, 1.f - axialRatio * axialRatio));
    result.signedDistance = result.radialDistance - localRadius;
    result.inside         = result.signedDistance <= 0.f;
    return result;
  }

  [[nodiscard]] SampleResult SampleVolumeImpl(const Capsule& volume, float t)
  {
    return MakeAxialSample(volume, volume.worldStart, volume.worldEnd, volume.worldDirection,
                           volume.radius, t);
  }

  [[nodiscard]] ProjectionResult ProjectPointImpl(const Capsule& volume, const Point3f& point)
  {
    return MakeAxialProjection(volume, point, volume.worldStart, volume.worldEnd,
                               volume.worldDirection, volume.radius);
  }

  [[nodiscard]] SampleResult SampleVolumeImpl(const SplineCapsule& volume, float t)
  {
    SampleResult result;
    result.t         = Clamp01(t);
    result.point     = volume.SamplePoint(result.t);
    result.direction = volume.SampleTangent(result.t);
    result.radius    = volume.radius;
    result.valid     = !volume.worldSamples.empty();
    return result;
  }

  [[nodiscard]] ProjectionResult ProjectPointImpl(const SplineCapsule& volume, const Point3f& point)
  {
    ProjectionResult result;
    if (volume.worldSamples.size() < 2)
      return result;

    float traversedLength  = 0.f;
    float bestAxisDistance = std::numeric_limits<float>::infinity();
    Point3f bestPoint      = Point3f::Zero();
    Vector3f bestDirection = Vector3f::UnitZ();
    float bestT            = 0.f;

    for (std::size_t index = 0; index + 1 < volume.worldSamples.size(); ++index) {
      const Point3f& start      = volume.worldSamples[index];
      const Point3f& end        = volume.worldSamples[index + 1];
      float segmentT            = 0.f;
      const Point3f closest     = ClosestPointOnSegment(point, start, end, &segmentT);
      const float distance      = (point - closest).squaredNorm();
      const float segmentLength = (end - start).norm();
      if (distance < bestAxisDistance) {
        bestAxisDistance = distance;
        bestPoint        = closest;
        bestDirection    = SafeNormalized(end - start);
        bestT            = volume.worldLength > kEpsilon
                               ? (traversedLength + segmentLength * segmentT) / volume.worldLength
                               : 0.f;
      }
      traversedLength += segmentLength;
    }

    result.closestPoint   = bestPoint;
    result.direction      = bestDirection;
    result.t              = Clamp01(bestT);
    result.radialDistance = std::sqrt(bestAxisDistance);
    result.signedDistance = result.radialDistance - volume.radius;
    result.inside         = result.signedDistance <= 0.f;
    result.valid          = true;
    return result;
  }

  [[nodiscard]] SampleResult SampleVolumeImpl(const Funnel& volume, float t)
  {
    SampleResult result;
    result.t = Clamp01(t);
    result.point =
        volume.worldEntranceCenter + volume.worldDirection * (result.t * volume.worldLength);
    result.direction = volume.worldDirection;
    result.radius    = LerpScalar(volume.worldEntranceRadius, volume.worldDeepRadius, result.t);
    result.valid     = true;
    return result;
  }

  [[nodiscard]] ProjectionResult ProjectPointImpl(const Funnel& volume, const Point3f& point)
  {
    return ProjectPointOntoFunnel(volume, point).projection;
  }

  [[nodiscard]] SampleResult SampleVolumeImpl(const SurfaceVolume& volume, float t)
  {
    SampleResult result;
    result.t         = Clamp01(t);
    result.point     = volume.worldCenter;
    result.direction = volume.worldDirection;
    result.radius    = 0.f;
    result.valid     = !volume.worldVertices.empty();
    return result;
  }

  [[nodiscard]] ProjectionResult ProjectPointImpl(const SurfaceVolume& volume, const Point3f& point)
  {
    return ProjectPointOntoSurface(volume, point).projection;
  }

  template <typename T>
  [[nodiscard]] SampleResult SampleEnvelopeImpl(const Envelope<T>& volume, float t)
  {
    SampleResult result;
    result.t           = Clamp01(t);
    const float localT = LerpScalar(volume.runtime.startT, volume.runtime.endT, result.t);
    result.point       = volume.runtime.referencePoint +
                         volume.runtime.direction * (localT * volume.runtime.totalLength);
    result.direction   = volume.runtime.direction;
    result.radius      = volume.runtime.typicalWidth * 0.5f;
    result.valid       = volume.runtime.totalLength > kEpsilon;
    return result;
  }

  template <typename T>
  [[nodiscard]] ProjectionResult ProjectEnvelopeImpl(const Envelope<T>& volume,
                                                     const Point3f& point)
  {
    ProjectionResult result;
    const Point3f axisEnd =
        volume.runtime.referencePoint + volume.runtime.direction * volume.runtime.totalLength;
    float axisT = 0.f;
    result.closestPoint =
        ClosestPointOnSegment(point, volume.runtime.referencePoint, axisEnd, &axisT);
    result.direction      = volume.runtime.direction;
    result.t              = Clamp01(axisT);
    result.radialDistance = (point - result.closestPoint).norm();
    result.signedDistance = result.radialDistance - volume.runtime.typicalWidth * 0.5f;
    result.inside         = result.signedDistance <= 0.f;
    result.valid          = volume.runtime.totalLength > kEpsilon;
    return result;
  }

}  // namespace

SampleResult SampleVolume(const VolumeData& volume, float t)
{
  return std::visit(
      [t](const auto& value) -> SampleResult {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Envelope<HalfBall>> ||
                      std::is_same_v<T, Envelope<Capsule>>) {
          return SampleEnvelopeImpl(value, t);
        } else {
          return SampleVolumeImpl(value, t);
        }
      },
      volume);
}

ProjectionResult ProjectPointOntoVolume(const VolumeData& volume, const Point3f& point)
{
  return std::visit(
      [&point](const auto& value) -> ProjectionResult {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Envelope<HalfBall>> ||
                      std::is_same_v<T, Envelope<Capsule>>) {
          return ProjectPointOntoEnvelope(value, point).projection;
        } else {
          return ProjectPointImpl(value, point);
        }
      },
      volume);
}

// -----------------------------------------------------------------------------
// ActorBody 实现
// -----------------------------------------------------------------------------

bool ActorBody::HasBodyPart(Race race, Gender gender, PartName name)
{
  using GenderType = Gender::Type;
  using RaceType   = Race::Type;

  bool isFemaleOrFuta = (gender.Get() == GenderType::Female || gender.Get() == GenderType::Futa);
  bool isHuman        = (race.GetType() == RaceType::Human);
  bool hasPenis       = gender.HasPenis();

  switch (name) {
  case PartName::Mouth:
  case PartName::HandLeft:
  case PartName::HandRight:
  case PartName::FootLeft:
  case PartName::FootRight:
    return true;
  case PartName::BreastLeft:
  case PartName::BreastRight:
  case PartName::Cleavage:
  case PartName::ThighCleft:
  case PartName::GlutealCleft:
  case PartName::Vagina:
    return isFemaleOrFuta;
  case PartName::Throat:
  case PartName::Belly:
  case PartName::ThighLeft:
  case PartName::ThighRight:
  case PartName::ButtLeft:
  case PartName::ButtRight:
  case PartName::Anus:
    return isHuman;
  case PartName::FingerLeft:
  case PartName::FingerRight:
    return isHuman;
  case PartName::Penis:
    return hasPenis;
  default:
    return false;
  }
}

ActorBody::ActorBody(RE::Actor* actor, Race race, Gender gender) : m_actor(actor)
{
  BuildVolumes(race, gender);
  ResolveNodes();
  UpdateWorld();
}

void ActorBody::ResolveNodes()
{
  for (auto& [name, vol] : volumes) {
    std::visit(
        [this](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, HalfBall>) {
            ResolvePoint(arg.center, m_actor);
            ResolvePoint(arg.pole, m_actor);
          } else if constexpr (std::is_same_v<T, Capsule>) {
            ResolvePoint(arg.start, m_actor);
            ResolvePoint(arg.end, m_actor);
          } else if constexpr (std::is_same_v<T, SplineCapsule>) {
            for (auto& pt : arg.controlPoints)
              ResolvePoint(pt, m_actor);
          } else if constexpr (std::is_same_v<T, Funnel>) {
            std::visit(
                [this](auto&& entranceDef) {
                  using E = std::decay_t<decltype(entranceDef)>;
                  if constexpr (std::is_same_v<E, Triangle>) {
                    entranceDef.Resolve(m_actor);
                  } else if constexpr (std::is_same_v<E, Polygon>) {
                    entranceDef.Resolve(m_actor);
                  }
                },
                arg.entrance);
            ResolvePoint(arg.deep, m_actor);
          } else if constexpr (std::is_same_v<T, SurfaceVolume>) {
            std::visit(
                [this](auto&& shapeDef) {
                  using S = std::decay_t<decltype(shapeDef)>;
                  if constexpr (std::is_same_v<S, Triangle>) {
                    shapeDef.Resolve(m_actor);
                  } else if constexpr (std::is_same_v<S, Polygon>) {
                    shapeDef.Resolve(m_actor);
                  }
                },
                arg.shape);
          } else if constexpr (std::is_same_v<T, Envelope<HalfBall>>) {
            // 引用聚合：边界体由主部位自行解析。
          } else if constexpr (std::is_same_v<T, Envelope<Capsule>>) {
            // 引用聚合：边界体由主部位自行解析。
          }
        },
        vol);
  }
}

void ActorBody::UpdateWorld()
{
  for (auto& [name, vol] : volumes) {
    std::visit(
        [](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, HalfBall> || std::is_same_v<T, Capsule> ||
                        std::is_same_v<T, SplineCapsule> || std::is_same_v<T, Funnel> ||
                        std::is_same_v<T, SurfaceVolume>) {
            arg.UpdateWorld();
          } else if constexpr (std::is_same_v<T, Envelope<HalfBall>>) {
            arg.UpdateRuntime();
          } else if constexpr (std::is_same_v<T, Envelope<Capsule>>) {
            arg.UpdateRuntime();
          }
        },
        vol);
  }
}

const VolumeData* ActorBody::GetPart(PartName name) const
{
  auto it = volumes.find(name);
  return (it != volumes.end()) ? &it->second : nullptr;
}

void ActorBody::BuildVolumes(Race race, Gender gender)
{
  using RaceType    = Race::Type;
  RaceType raceType = race.GetType();
  bool isHuman      = (raceType == RaceType::Human);

  volumes.clear();
  volumes.reserve(static_cast<std::size_t>(PartName::Penis) + 1);

  // 辅助 lambda
  auto add = [this](PartName name, auto&& vol) {
    volumes.emplace(name, std::forward<decltype(vol)>(vol));
  };

  // ------------------- Mouth -------------------
  if (isHuman) {
    Funnel mouth;
    mouth.entrance       = Polygon{{Node("NPC Head [Head]", {0, 7.5, -2.0f}),         // 上嘴唇
                                    Node("NPC Head [Head]", {1.5f, 6.5f, -3.0f}),     // 右唇角
                                    Node("NPC Head [Head]", {0, 6.5f, -4.0f}),        // 下嘴唇
                                    Node("NPC Head [Head]", {-1.5f, 6.5f, -3.0f})}};  // 左唇角
    mouth.deep           = Node("NPC Head [Head]", {0.0f, 1.0f, -2.5f});              // 喉咙入口
    mouth.entranceRadius = 1.2f;
    mouth.deepRadius     = 1.0f;
    mouth.coreDepthRatio = 0.25f;
    mouth.captureExtend  = 5.0f;
    mouth.exitTolerance  = 1.8f;
    add(PartName::Mouth, mouth);
  }

  // ------------------- Throat -------------------
  if (isHuman) {
    Capsule throat;
    throat.start  = Node("NPC Head [Head]", {0.0f, 1.0f, -2.5f});  // 喉咙入口
    throat.end    = Node("NPC Neck [Neck]");
    throat.radius = 1.1f;
    add(PartName::Throat, throat);
  }

  // ------------------- BreastLeft / BreastRight -------------------
  if (gender.Get() == Gender::Type::Female || gender.Get() == Gender::Type::Futa) {
    HalfBall breastL;
    breastL.center = Node("L Breast02");  // 乳房中心
    breastL.pole   = Node("L Breast03");  // 乳头稍微更远处
    breastL.radius = 1.7f;
    add(PartName::BreastLeft, breastL);

    HalfBall breastR;
    breastR.center = Node("R Breast02");
    breastR.pole   = Node("R Breast03");
    breastR.radius = 1.7f;
    add(PartName::BreastRight, breastR);

    // Cleavage (Envelope)
    Envelope<HalfBall> cleavage{&std::get<HalfBall>(volumes.at(PartName::BreastLeft)),
                                &std::get<HalfBall>(volumes.at(PartName::BreastRight))};
    add(PartName::Cleavage, cleavage);
  }

  // ------------------- Hands -------------------
  {
    Capsule handL;
    handL.start  = Node("NPC L Finger20 [LF20]");  // 掌心位置
    handL.end    = Node("SHIELD");
    handL.radius = 0.8f;
    add(PartName::HandLeft, handL);

    Capsule handR;
    handR.start  = Node("NPC R Finger20 [RF20]");  // 掌心位置
    handR.end    = Node("WEAPON");
    handR.radius = 0.8f;
    add(PartName::HandRight, handR);
  }

  // ------------------- Fingers (Human) -------------------
  if (isHuman) {
    Capsule fingerL;
    fingerL.start  = Node("NPC L Finger22 [LF22]");                // 中指指尖
    fingerL.end    = Node("NPC L Finger22 [LF22]", {0, 0, 0.1f});  // 极短
    fingerL.radius = 0.3f;
    add(PartName::FingerLeft, fingerL);

    Capsule fingerR;
    fingerR.start  = Node("NPC R Finger22 [RF22]");
    fingerR.end    = Node("NPC R Finger22 [RF22]", {0, 0, 0.1f});
    fingerR.radius = 0.3f;
    add(PartName::FingerRight, fingerR);
  }

  // ------------------- Belly -------------------
  if (isHuman) {
    Triangle bellyTri;
    bellyTri.vertices = {Node("NPC Belly", {0.0f, 3.0f, -10.0f}),  // 下腹部
                         Node("NPC Belly", {-5.0f, 2.0f, -5.0f}),  // 左侧
                         Node("NPC Belly", {5.0f, 2.0f, -5.0f})};  // 右侧
    SurfaceVolume belly;
    belly.shape = bellyTri;
    add(PartName::Belly, belly);
  }

  // ------------------- Thighs -------------------
  if (isHuman) {
    Capsule thighL;
    thighL.start  = Node("NPC L Thigh [LThg]", {0.0f, 0.0f, 6.0f});   // 大腿根部稍微向下
    thighL.end    = Node("NPC L Calf [LClf]", {0.0f, -2.0f, -8.0f});  // 大腿末端稍微向上
    thighL.radius = 1.6f;
    add(PartName::ThighLeft, thighL);

    Capsule thighR;
    thighR.start  = Node("NPC R Thigh [RThg]", {0.0f, 0.0f, 6.0f});   // 大腿根部稍微向下
    thighR.end    = Node("NPC R Calf [RClf]", {0.0f, -2.0f, -8.0f});  // 大腿末端稍微向上
    thighR.radius = 1.6f;
    add(PartName::ThighRight, thighR);

    // ThighCleft
    Envelope<Capsule> cleft{&std::get<Capsule>(volumes.at(PartName::ThighLeft)),
                            &std::get<Capsule>(volumes.at(PartName::ThighRight))};
    add(PartName::ThighCleft, cleft);
  }

  // ------------------- Butts -------------------
  if (isHuman) {
    HalfBall buttL;
    buttL.center = Node("NPC L Butt", {-3.0f, 3.0f, -4.0f});  // 左臀部中心稍微向左
    buttL.pole   = Node("NPC L Butt", {-2.0f, -2.0f, 3.0f});  // 左臀表面最翘点
    buttL.radius = 1.5f;
    add(PartName::ButtLeft, buttL);

    HalfBall buttR;
    buttR.center = Node("NPC R Butt", {3.0f, 3.0f, -4.0f});   // 右臀部中心稍微向右
    buttR.pole   = Node("NPC R Butt", {2.0f, -2.0f, -3.0f});  // 右臀表面最翘点
    buttR.radius = 1.5f;
    add(PartName::ButtRight, buttR);

    Envelope<HalfBall> glutealCleft{&std::get<HalfBall>(volumes.at(PartName::ButtLeft)),
                                    &std::get<HalfBall>(volumes.at(PartName::ButtRight))};
    add(PartName::GlutealCleft, glutealCleft);
  }

  // ------------------- Feet -------------------
  {
    Capsule footL;
    footL.start  = Node("NPC L Foot [Lft ]");
    footL.end    = Node("NPC L Toe0 [LToe]");
    footL.radius = 0.9f;
    add(PartName::FootLeft, footL);

    Capsule footR;
    footR.start  = Node("NPC R Foot [Rft ]");
    footR.end    = Node("NPC R Toe0 [RToe]");
    footR.radius = 0.9f;
    add(PartName::FootRight, footR);
  }

  // ------------------- Vagina -------------------
  if (isHuman && (gender.Get() == Gender::Type::Female || gender.Get() == Gender::Type::Futa)) {
    Funnel vag;
    vag.entrance = Polygon{
        {Node("NPC L Pussy02"), Node("NPC R Pussy02"), Node("VaginaB1"), Node("Clitoral1")}};
    vag.deep           = Node("VaginaDeep1");
    vag.entranceRadius = 1.25f;
    vag.deepRadius     = 0.8f;
    vag.coreDepthRatio = 0.3f;
    vag.captureExtend  = 6.0f;
    vag.exitTolerance  = 1.8f;
    add(PartName::Vagina, vag);
  }

  // ------------------- Anus -------------------
  if (isHuman) {
    Funnel anus;
    anus.entrance = Polygon{
        {Node("NPC RT Anus2"), Node("NPC LT Anus2"), Node("NPC LB Anus2"), Node("NPC RB Anus2")}};
    anus.deep           = Node("NPC Anus Deep1");
    anus.entranceRadius = 0.95f;
    anus.deepRadius     = 0.6f;
    anus.coreDepthRatio = 0.3f;
    anus.captureExtend  = 5.0f;
    anus.exitTolerance  = 1.8f;
    add(PartName::Anus, anus);
  }

  // ------------------- Penis -------------------
  if (gender.HasPenis()) {
    SplineCapsule penis;
    if (isHuman) {
      penis.controlPoints = {Node("NPC Genitals01 [Gen01]"), Node("NPC Genitals02 [Gen02]"),
                             Node("NPC Genitals03 [Gen03]"), Node("NPC Genitals04 [Gen04]"),
                             Node("NPC Genitals05 [Gen05]"), Node("NPC Genitals06 [Gen06]")};
      penis.radius        = 0.55f;
    } else if (auto it = kSchlongMap.find(raceType); it != kSchlongMap.end()) {
      penis.controlPoints = {Node(it->second[0]), Node(it->second[1]), Node(it->second[2])};
      penis.radius        = 0.6f;
    } else {
      logger::warn("ActorBody::BuildVolumes: no penis mapping for race {}",
                   magic_enum::enum_name(raceType));
      penis.controlPoints = {Node("NPC Genitals01 [Gen01]"), Node("NPC Genitals06 [Gen06]")};
      penis.radius        = 0.6f;
    }
    add(PartName::Penis, penis);
  }
}

}  // namespace Define
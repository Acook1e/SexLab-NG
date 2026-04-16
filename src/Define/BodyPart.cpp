#include "Define/BodyPart.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "magic_enum/magic_enum.hpp"

namespace Define
{
namespace
{
  using VolumeList = std::vector<VolumeData>;

  struct ResolvedFace
  {
    std::vector<Point3f> points{};
    Point3f center    = Point3f::Zero();
    Vector3f normal   = Vector3f::Zero();
    Vector3f edgeHint = Vector3f::Zero();
  };

  struct ResolvedCapsule
  {
    Point3f start  = Point3f::Zero();
    Point3f end    = Point3f::Zero();
    float radius   = 0.f;
    float length   = 0.f;
    Vector3f axis  = Vector3f::Zero();
    Point3f center = Point3f::Zero();
  };

  RE::NiPoint3 Offset(float x, float y, float z)
  {
    return RE::NiPoint3{x, y, z};
  }

  Node MakeNode(std::string_view name, float x = 0.f, float y = 0.f, float z = 0.f)
  {
    return Node(name, Offset(x, y, z));
  }

  Point MakePoint(Node node)
  {
    return Point{std::move(node)};
  }

  Point MakePoint2(Node first, Node second)
  {
    return Point{Polygon<2>{std::move(first), std::move(second)}};
  }

  Face MakeFace3(Node first, Node second, Node third)
  {
    return Face{Polygon<3>{std::move(first), std::move(second), std::move(third)}};
  }

  Face MakeFace4(Node first, Node second, Node third, Node fourth)
  {
    return Face{
        Polygon<4>{std::move(first), std::move(second), std::move(third), std::move(fourth)}};
  }

  Capsule MakeCapsuleVolume(Point start, Point end, float radius)
  {
    Capsule capsule;
    capsule.start  = std::move(start);
    capsule.end    = std::move(end);
    capsule.radius = radius;
    return capsule;
  }

  VolumeProfile MakeVolumeProfile(VolumeType topology, MotionClass motionClass, bool canInitiate,
                                  bool ownsStateSlot, bool providesSupport, bool exclusiveSource,
                                  bool exclusiveTarget)
  {
    VolumeProfile profile;
    profile.topology        = topology;
    profile.motionClass     = motionClass;
    profile.canInitiate     = canInitiate;
    profile.ownsStateSlot   = ownsStateSlot;
    profile.providesSupport = providesSupport;
    profile.exclusiveSource = exclusiveSource;
    profile.exclusiveTarget = exclusiveTarget;
    return profile;
  }

  Vector MakeVectorInfo(const Point3f& start, const Point3f& end)
  {
    Vector vectorInfo;
    vectorInfo.start     = start;
    vectorInfo.end       = end;
    vectorInfo.direction = end - start;
    vectorInfo.length    = vectorInfo.direction.norm();
    if (vectorInfo.length > 1e-6f)
      vectorInfo.direction /= vectorInfo.length;
    else
      vectorInfo.direction = Vector3f::Zero();
    return vectorInfo;
  }

  CapsuleCollider MakeCapsuleCollider(const Point3f& start, const Point3f& end, float radius)
  {
    const Vector axis = MakeVectorInfo(start, end);
    CapsuleCollider collider;
    collider.start     = axis.start;
    collider.end       = axis.end;
    collider.direction = axis.direction;
    collider.length    = axis.length;
    collider.radius    = radius;
    return collider;
  }

  BoxCollider MakeBoxCollider(const Vector& axis, const Point3f& center, const Matrix3f& basis,
                              const Vector3f& halfExtents)
  {
    BoxCollider collider;
    collider.start       = axis.start;
    collider.end         = axis.end;
    collider.direction   = axis.direction;
    collider.length      = axis.length;
    collider.center      = center;
    collider.basis       = basis;
    collider.halfExtents = halfExtents;
    return collider;
  }

  FunnelCollider MakeFunnelCollider(const Point3f& entranceCenter, const Vector3f& entranceNormal,
                                    float entranceRadius, float innerRadius, float depth,
                                    std::vector<Point3f> channelSpline)
  {
    FunnelCollider collider;
    collider.entranceCenter = entranceCenter;
    collider.entranceNormal = entranceNormal;
    collider.entranceRadius = entranceRadius;
    collider.innerRadius    = innerRadius;
    collider.depth          = depth;
    collider.channelSpline  = std::move(channelSpline);
    return collider;
  }

  SurfaceCollider MakeSurfaceCollider(const Point3f& center, const Matrix3f& basis,
                                      const Vector3f& halfExtents)
  {
    SurfaceCollider collider;
    collider.center      = center;
    collider.basis       = basis;
    collider.halfExtents = halfExtents;
    collider.normal      = basis.col(2);
    return collider;
  }

  EnvelopeCollider MakeEnvelopeCollider(const Point3f& center, const Matrix3f& basis,
                                        const Vector3f& halfExtents)
  {
    EnvelopeCollider collider;
    collider.center      = center;
    collider.basis       = basis;
    collider.halfExtents = halfExtents;
    collider.clampAxis   = basis.col(0);
    return collider;
  }

  bool HasAnyCollision(const CollisionSet& collisions)
  {
    return !collisions.capsules.empty() || !collisions.boxes.empty() ||
           !collisions.funnels.empty() || !collisions.surfaces.empty() ||
           !collisions.envelopes.empty();
  }

  template <std::size_t N>
  Point3f ComputeAverage(const std::array<Point3f, N>& points)
  {
    Point3f center = Point3f::Zero();
    for (const auto& point : points)
      center += point;
    return center / static_cast<float>(N);
  }

  Vector3f ComputePolygonNormal(const std::vector<Point3f>& points)
  {
    if (points.size() < 3)
      return Vector3f::Zero();

    Vector3f normal = Vector3f::Zero();
    for (std::size_t index = 0; index < points.size(); ++index) {
      const auto& current = points[index];
      const auto& next    = points[(index + 1) % points.size()];
      normal.x() += (current.y() - next.y()) * (current.z() + next.z());
      normal.y() += (current.z() - next.z()) * (current.x() + next.x());
      normal.z() += (current.x() - next.x()) * (current.y() + next.y());
    }

    if (normal.squaredNorm() <= 1e-6f)
      normal = (points[1] - points[0]).cross(points[2] - points[0]);

    if (normal.squaredNorm() <= 1e-6f)
      return Vector3f::Zero();

    normal.normalize();
    return normal;
  }

  Matrix3f BuildBasis(const Vector3f& primaryAxis, const Vector3f* lateralHint = nullptr)
  {
    Vector3f forward = primaryAxis;
    if (forward.squaredNorm() <= 1e-6f)
      forward = Vector3f::UnitZ();
    else
      forward.normalize();

    Vector3f lateral = lateralHint ? *lateralHint : Vector3f::Zero();
    lateral -= forward * lateral.dot(forward);
    if (lateral.squaredNorm() <= 1e-6f) {
      const Vector3f fallback =
          std::abs(forward.z()) < 0.85f ? Vector3f::UnitZ() : Vector3f::UnitX();
      lateral = forward.cross(fallback);
    }
    if (lateral.squaredNorm() <= 1e-6f)
      lateral = Vector3f::UnitY();
    lateral.normalize();

    Vector3f up = forward.cross(lateral);
    if (up.squaredNorm() <= 1e-6f)
      up = Vector3f::UnitZ();
    else
      up.normalize();

    lateral = up.cross(forward);
    if (lateral.squaredNorm() <= 1e-6f)
      lateral = Vector3f::UnitX();
    else
      lateral.normalize();

    Matrix3f basis = Matrix3f::Identity();
    basis.col(0)   = lateral;
    basis.col(1)   = up;
    basis.col(2)   = forward;
    return basis;
  }

  Vector3f MeasureHalfExtents(const std::vector<Point3f>& points, const Point3f& center,
                              const Matrix3f& basis, float halfThickness)
  {
    Vector3f halfExtents = Vector3f::Zero();
    for (const auto& point : points) {
      const Vector3f offset = point - center;
      for (int axis = 0; axis < 3; ++axis)
        halfExtents[axis] = (std::max)(halfExtents[axis], std::abs(offset.dot(basis.col(axis))));
    }

    halfExtents[2] = (std::max)(halfExtents[2], halfThickness);
    return halfExtents;
  }

  template <std::size_t N>
  std::optional<std::array<Point3f, N>> ResolvePolygon(const Polygon<N>& polygon)
  {
    std::array<Point3f, N> points{};
    for (std::size_t index = 0; index < N; ++index) {
      if (!polygon[index].IsResolved())
        return std::nullopt;
      points[index] = polygon[index].GetWorldPosition();
    }
    return points;
  }

  std::optional<Point3f> ResolvePoint(const Point& point)
  {
    return std::visit(
        [](const auto& value) -> std::optional<Point3f> {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, Node>) {
            if (!value.IsResolved())
              return std::nullopt;
            return value.GetWorldPosition();
          } else {
            const auto resolved = ResolvePolygon(value);
            if (!resolved)
              return std::nullopt;
            return ComputeAverage(*resolved);
          }
        },
        point);
  }

  std::optional<ResolvedFace> ResolveFace(const Face& face)
  {
    return std::visit(
        [](const auto& polygon) -> std::optional<ResolvedFace> {
          const auto resolved = ResolvePolygon(polygon);
          if (!resolved)
            return std::nullopt;

          ResolvedFace faceInfo;
          faceInfo.points.assign(resolved->begin(), resolved->end());
          faceInfo.center = ComputeAverage(*resolved);
          faceInfo.normal = ComputePolygonNormal(faceInfo.points);
          if (faceInfo.points.size() >= 2)
            faceInfo.edgeHint = faceInfo.points[1] - faceInfo.points[0];
          else
            faceInfo.edgeHint = Vector3f::Zero();
          return faceInfo;
        },
        face);
  }

  std::optional<ResolvedCapsule> ResolveCapsule(const Capsule& capsule)
  {
    const auto start = ResolvePoint(capsule.start);
    const auto end   = ResolvePoint(capsule.end);
    if (!start || !end)
      return std::nullopt;

    ResolvedCapsule resolved;
    resolved.start  = *start;
    resolved.end    = *end;
    resolved.radius = capsule.radius;
    resolved.center = (*start + *end) * 0.5f;
    resolved.axis   = *end - *start;
    resolved.length = resolved.axis.norm();
    return resolved;
  }

  std::optional<Point3f> TryResolveNodePosition(RE::Actor* actor, std::string_view nodeName)
  {
    if (!actor || nodeName.empty())
      return std::nullopt;

    auto* object = actor->GetNodeByName(nodeName);
    auto* node   = object ? object->AsNode() : nullptr;
    if (!node)
      return std::nullopt;

    const auto& translate = node->world.translate;
    return Point3f(translate.x, translate.y, translate.z);
  }

  std::optional<Point3f> TryResolveNodeAverage(RE::Actor* actor,
                                               std::initializer_list<std::string_view> nodeNames)
  {
    Point3f center    = Point3f::Zero();
    std::size_t count = 0;
    for (const auto nodeName : nodeNames) {
      const auto position = TryResolveNodePosition(actor, nodeName);
      if (!position)
        continue;
      center += *position;
      ++count;
    }

    if (count == 0)
      return std::nullopt;
    return center / static_cast<float>(count);
  }

  Vector3f BuildAnusReferenceAxis(RE::Actor* actor, const Point3f& anusStart,
                                  const Vector3f& deepAxis)
  {
    if (!actor || deepAxis.squaredNorm() <= 1e-6f)
      return Vector3f::Zero();

    const auto leftButt  = TryResolveNodePosition(actor, "NPC L Butt");
    const auto rightButt = TryResolveNodePosition(actor, "NPC R Butt");
    if (!leftButt || !rightButt)
      return Vector3f::Zero();

    Vector3f lateral = *rightButt - *leftButt;
    if (lateral.squaredNorm() <= 1e-6f)
      return Vector3f::Zero();
    lateral.normalize();

    const auto buttMid = TryResolveNodeAverage(actor, {"NPC L Butt", "NPC R Butt"});
    if (!buttMid)
      return Vector3f::Zero();

    Vector3f cleftUp = *buttMid - anusStart;
    if (cleftUp.squaredNorm() <= 1e-6f)
      return Vector3f::Zero();
    cleftUp.normalize();

    Vector3f referenceAxis = lateral.cross(cleftUp);
    if (referenceAxis.squaredNorm() <= 1e-6f)
      return Vector3f::Zero();

    if (referenceAxis.dot(deepAxis) < 0.f)
      referenceAxis = -referenceAxis;
    referenceAxis.normalize();
    return referenceAxis * deepAxis.norm();
  }

  Vector3f BuildAnusChannelAxis(RE::Actor* actor, const Point3f& anusStart,
                                const std::vector<Point3f>& ringPositions, const Vector3f& deepAxis)
  {
    if (deepAxis.squaredNorm() <= 1e-6f || ringPositions.size() < 4)
      return deepAxis;

    const Vector3f lateral =
        (ringPositions[0] + ringPositions[3]) - (ringPositions[1] + ringPositions[2]);
    const Vector3f vertical =
        (ringPositions[0] + ringPositions[1]) - (ringPositions[2] + ringPositions[3]);

    Vector3f surfaceNormal = lateral.cross(vertical);
    if (surfaceNormal.squaredNorm() <= 1e-6f)
      return deepAxis;

    if (surfaceNormal.dot(deepAxis) < 0.f)
      surfaceNormal = -surfaceNormal;
    surfaceNormal.normalize();

    const Vector3f deepDirection = deepAxis.normalized();
    const Vector3f referenceAxis = BuildAnusReferenceAxis(actor, anusStart, deepAxis);
    const bool hasReferenceAxis  = referenceAxis.squaredNorm() > 1e-6f;
    const Vector3f referenceDirection =
        hasReferenceAxis ? referenceAxis.normalized() : Vector3f::Zero();

    Vector3f blendedAxis = surfaceNormal * 0.65f + deepDirection * 0.35f;
    if (hasReferenceAxis)
      blendedAxis = surfaceNormal * 0.50f + deepDirection * 0.30f + referenceDirection * 0.20f;
    if (blendedAxis.squaredNorm() <= 1e-6f)
      return deepAxis;

    blendedAxis.normalize();
    return blendedAxis * deepAxis.norm();
  }

  void UpdatePointNodes(Point& point, RE::Actor* actor)
  {
    std::visit(
        [&](auto& value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, Node>) {
            (void)value.UpdateNode(actor);
          } else {
            for (auto& node : value)
              (void)node.UpdateNode(actor);
          }
        },
        point);
  }

  void UpdateFaceNodes(Face& face, RE::Actor* actor)
  {
    std::visit(
        [&](auto& polygon) {
          for (auto& node : polygon)
            (void)node.UpdateNode(actor);
        },
        face);
  }

  void UpdateVolumeNodes(Ball& ball, RE::Actor* actor)
  {
    UpdatePointNodes(ball.center, actor);
  }

  void UpdateVolumeNodes(HalfBall& halfBall, RE::Actor* actor)
  {
    UpdatePointNodes(halfBall.center, actor);
    UpdatePointNodes(halfBall.pole, actor);
  }

  void UpdateVolumeNodes(Capsule& capsule, RE::Actor* actor)
  {
    UpdatePointNodes(capsule.start, actor);
    UpdatePointNodes(capsule.end, actor);
  }

  void UpdateVolumeNodes(Funnel& funnel, RE::Actor* actor)
  {
    UpdateFaceNodes(funnel.entrance, actor);
    UpdatePointNodes(funnel.deep, actor);
  }

  void UpdateVolumeNodes(Surface& surface, RE::Actor* actor)
  {
    UpdateFaceNodes(surface.face, actor);
  }

  void UpdateVolumeNodes(Envelope& envelope, RE::Actor* actor)
  {
    UpdateVolumeNodes(envelope.left, actor);
    UpdateVolumeNodes(envelope.right, actor);
  }

  void UpdateVolumeNodes(VolumeData& volume, RE::Actor* actor)
  {
    std::visit(
        [&](auto& value) {
          UpdateVolumeNodes(value, actor);
        },
        volume);
  }

  std::optional<Vector> AppendVolumeRuntime(BodyPart::Name, RE::Actor*, const Ball& ball,
                                            CollisionSet& collisions)
  {
    const auto center = ResolvePoint(ball.center);
    if (!center)
      return std::nullopt;

    collisions.capsules.push_back(MakeCapsuleCollider(*center, *center, ball.radius));
    return MakeVectorInfo(*center, *center);
  }

  std::optional<Vector> AppendVolumeRuntime(BodyPart::Name, RE::Actor*, const HalfBall& halfBall,
                                            CollisionSet& collisions)
  {
    const auto center = ResolvePoint(halfBall.center);
    const auto pole   = ResolvePoint(halfBall.pole);
    if (!center || !pole)
      return std::nullopt;

    const Vector axis = MakeVectorInfo(*center, *pole);
    collisions.capsules.push_back(MakeCapsuleCollider(*center, *pole, halfBall.radius));
    if (axis.length > 1e-6f) {
      const Matrix3f basis = BuildBasis(axis.direction);
      const Vector3f halfExtents(halfBall.radius, halfBall.radius,
                                 (std::max)(axis.length * 0.5f, halfBall.radius * 0.5f));
      collisions.surfaces.push_back(
          MakeSurfaceCollider((*center + *pole) * 0.5f, basis, halfExtents));
    }
    return axis;
  }

  std::optional<Vector> AppendVolumeRuntime(BodyPart::Name, RE::Actor*, const Capsule& capsule,
                                            CollisionSet& collisions)
  {
    const auto resolved = ResolveCapsule(capsule);
    if (!resolved)
      return std::nullopt;

    collisions.capsules.push_back(
        MakeCapsuleCollider(resolved->start, resolved->end, resolved->radius));
    return MakeVectorInfo(resolved->start, resolved->end);
  }

  std::optional<Vector> AppendVolumeRuntime(BodyPart::Name name, RE::Actor* actor,
                                            const Funnel& funnel, CollisionSet& collisions)
  {
    const auto entrance = ResolveFace(funnel.entrance);
    const auto deep     = ResolvePoint(funnel.deep);
    if (!entrance || !deep)
      return std::nullopt;

    Vector3f channelAxis = *deep - entrance->center;
    if (name == BodyPart::Name::Anus)
      channelAxis = BuildAnusChannelAxis(actor, entrance->center, entrance->points, channelAxis);

    if (channelAxis.squaredNorm() <= 1e-6f) {
      const float fallbackDepth = funnel.depth > 0.f ? funnel.depth : 1.0f;
      channelAxis = entrance->normal.squaredNorm() > 1e-6f ? entrance->normal * fallbackDepth
                                                           : Vector3f(0.f, 0.f, fallbackDepth);
    }

    Vector axis = MakeVectorInfo(entrance->center, entrance->center + channelAxis);
    if (funnel.depth > 0.f && axis.length > 1e-6f)
      axis = MakeVectorInfo(entrance->center, entrance->center + axis.direction * funnel.depth);

    const Vector3f basisForward =
        axis.length > 1e-6f
            ? axis.direction
            : (entrance->normal.squaredNorm() > 1e-6f ? entrance->normal : Vector3f::UnitZ());
    const Matrix3f basis = BuildBasis(basisForward, &entrance->edgeHint);
    const float halfThickness =
        (std::max)(axis.length > 1e-6f ? axis.length * 0.18f : funnel.depth * 0.18f, 0.25f);
    const Vector3f entranceExtents =
        MeasureHalfExtents(entrance->points, entrance->center, basis, halfThickness);
    const float entranceRadius = funnel.entranceRadius > 0.f
                                     ? funnel.entranceRadius
                                     : (std::max)(entranceExtents.x(), entranceExtents.y());
    const float innerRadius    = funnel.innerRadius > 0.f ? funnel.innerRadius : entranceRadius;

    const Vector entranceAxis =
        MakeVectorInfo(entrance->center - basis.col(2) * entranceExtents.z(),
                       entrance->center + basis.col(2) * entranceExtents.z());
    collisions.boxes.push_back(
        MakeBoxCollider(entranceAxis, entrance->center, basis, entranceExtents));
    collisions.capsules.push_back(MakeCapsuleCollider(axis.start, axis.end, innerRadius));
    collisions.funnels.push_back(MakeFunnelCollider(entrance->center, basis.col(2), entranceRadius,
                                                    innerRadius, axis.length,
                                                    std::vector<Point3f>{axis.start, axis.end}));
    return axis;
  }

  std::optional<Vector> AppendVolumeRuntime(BodyPart::Name, RE::Actor*, const Surface& surface,
                                            CollisionSet& collisions)
  {
    const auto resolved = ResolveFace(surface.face);
    if (!resolved || resolved->normal.squaredNorm() <= 1e-6f)
      return std::nullopt;

    const float halfThickness = (std::max)(surface.halfThickness, 0.25f);
    const Matrix3f basis      = BuildBasis(resolved->normal, &resolved->edgeHint);
    const Vector3f halfExtents =
        MeasureHalfExtents(resolved->points, resolved->center, basis, halfThickness);
    const Vector axis = MakeVectorInfo(resolved->center - basis.col(2) * halfThickness,
                                       resolved->center + basis.col(2) * halfThickness);

    collisions.boxes.push_back(MakeBoxCollider(axis, resolved->center, basis, halfExtents));
    collisions.surfaces.push_back(MakeSurfaceCollider(resolved->center, basis, halfExtents));
    return axis;
  }

  std::optional<Vector> AppendVolumeRuntime(BodyPart::Name, RE::Actor*, const Envelope& envelope,
                                            CollisionSet& collisions)
  {
    const auto left  = ResolveCapsule(envelope.left);
    const auto right = ResolveCapsule(envelope.right);
    if (!left || !right)
      return std::nullopt;

    const Point3f start  = (left->start + right->start) * 0.5f;
    const Point3f end    = (left->end + right->end) * 0.5f;
    const Point3f center = (left->center + right->center) * 0.5f;
    Vector axis          = MakeVectorInfo(start, end);

    Vector3f lateral = right->center - left->center;
    if (lateral.squaredNorm() <= 1e-6f)
      lateral = right->start - left->start;

    if (axis.length <= 1e-6f) {
      const Vector3f averageAxis = (right->end - right->start) + (left->end - left->start);
      if (averageAxis.squaredNorm() > 1e-6f)
        axis = MakeVectorInfo(center, center + averageAxis.normalized());
    }

    const Vector3f basisForward =
        axis.length > 1e-6f
            ? axis.direction
            : (lateral.squaredNorm() > 1e-6f ? lateral.normalized() : Vector3f::UnitZ());
    const Matrix3f basis = BuildBasis(basisForward, &lateral);

    const float averageRadius = (left->radius + right->radius) * 0.5f;
    const float averageLength = (left->length + right->length) * 0.5f;
    const float halfWidth =
        (std::max)(0.5f * lateral.norm(), (std::max)(left->radius, right->radius));
    const float halfDepth  = (std::max)(averageRadius + envelope.depthPadding, 0.35f);
    const float halfLength = (std::max)(averageLength * 0.5f, averageRadius);
    const Vector3f halfExtents(halfWidth, halfDepth, halfLength);

    const Vector boxAxis = axis.length > 1e-6f ? axis
                                               : MakeVectorInfo(center - basis.col(2) * halfLength,
                                                                center + basis.col(2) * halfLength);

    collisions.boxes.push_back(MakeBoxCollider(boxAxis, center, basis, halfExtents));
    collisions.envelopes.push_back(MakeEnvelopeCollider(center, basis, halfExtents));
    return boxAxis;
  }

  std::optional<Vector> AppendVolumeRuntime(BodyPart::Name name, RE::Actor* actor,
                                            const VolumeData& volume, CollisionSet& collisions)
  {
    return std::visit(
        [&](const auto& value) -> std::optional<Vector> {
          return AppendVolumeRuntime(name, actor, value, collisions);
        },
        volume);
  }

  VolumeList BuildHumanVolumes(BodyPart::Name name)
  {
    switch (name) {
    case BodyPart::Name::Mouth:
      return {Funnel{MakeFace3(MakeNode("NPC Head [Head]", 2.4f, 4.4f, -2.4f),
                               MakeNode("NPC Head [Head]", 4.0f, 4.4f, -2.4f),
                               MakeNode("NPC Head [Head]", 3.2f, 4.4f, -1.0f)),
                     MakePoint(MakeNode("NPC Head [Head]")), 1.00f, 0.65f, 4.25f}};
    case BodyPart::Name::Throat:
      return {MakeCapsuleVolume(MakePoint(MakeNode("NPC Head [Head]")),
                                MakePoint(MakeNode("NPC Neck [Neck]")), 1.10f)};
    case BodyPart::Name::BreastLeft:
      return {
          HalfBall{MakePoint(MakeNode("L Breast02")), MakePoint(MakeNode("L Breast03")), 1.70f}};
    case BodyPart::Name::BreastRight:
      return {
          HalfBall{MakePoint(MakeNode("R Breast02")), MakePoint(MakeNode("R Breast03")), 1.70f}};
    case BodyPart::Name::Cleavage:
      return {
          Envelope{MakeCapsuleVolume(MakePoint(MakeNode("L Breast02", 0.9f, 1.0f, -4.2f)),
                                     MakePoint(MakeNode("L Breast02", 0.3f, 1.0f, 3.8f)), 0.85f),
                   MakeCapsuleVolume(MakePoint(MakeNode("R Breast02", -0.9f, 1.0f, -4.2f)),
                                     MakePoint(MakeNode("R Breast02", -0.3f, 1.0f, 3.8f)), 0.85f),
                   0.35f}};
    case BodyPart::Name::HandLeft:
      return {Surface{MakeFace3(MakeNode("NPC L Hand [LHnd]"), MakeNode("NPC L Finger01 [LF01]"),
                                MakeNode("NPC L Finger40 [LF40]")),
                      0.60f}};
    case BodyPart::Name::HandRight:
      return {Surface{MakeFace3(MakeNode("NPC R Hand [RHnd]"), MakeNode("NPC R Finger01 [RF01]"),
                                MakeNode("NPC R Finger40 [RF40]")),
                      0.60f}};
    case BodyPart::Name::FingerLeft:
      return {Ball{MakePoint(MakeNode("NPC L Finger22 [LF22]")), 0.35f}};
    case BodyPart::Name::FingerRight:
      return {Ball{MakePoint(MakeNode("NPC R Finger22 [RF22]")), 0.35f}};
    case BodyPart::Name::Belly:
      return {Surface{MakeFace4(MakeNode("NPC Belly", -4.0f, 3.0f, -10.0f),
                                MakeNode("NPC Belly", 4.0f, 3.0f, -10.0f),
                                MakeNode("NPC Belly", 4.0f, 5.0f, -1.0f),
                                MakeNode("NPC Belly", -4.0f, 5.0f, -1.0f)),
                      0.90f}};
    case BodyPart::Name::ThighLeft:
      return {MakeCapsuleVolume(MakePoint(MakeNode("NPC L RearThigh")),
                                MakePoint(MakeNode("NPC L FrontThigh")), 1.10f)};
    case BodyPart::Name::ThighRight:
      return {MakeCapsuleVolume(MakePoint(MakeNode("NPC R RearThigh")),
                                MakePoint(MakeNode("NPC R FrontThigh")), 1.10f)};
    case BodyPart::Name::ThighCleft:
      return {
          Envelope{MakeCapsuleVolume(MakePoint(MakeNode("VaginaB1", -1.7f, -3.0f, -2.0f)),
                                     MakePoint(MakeNode("Clitoral1", -1.2f, -1.0f, -3.0f)), 0.75f),
                   MakeCapsuleVolume(MakePoint(MakeNode("VaginaB1", 1.7f, -3.0f, -2.0f)),
                                     MakePoint(MakeNode("Clitoral1", 1.2f, -1.0f, -3.0f)), 0.75f),
                   0.45f}};
    case BodyPart::Name::ButtLeft:
      return {Ball{MakePoint(MakeNode("NPC L Butt", -5.0f, -1.0f, -3.0f)), 1.45f}};
    case BodyPart::Name::ButtRight:
      return {Ball{MakePoint(MakeNode("NPC R Butt", 5.0f, 1.0f, -3.0f)), 1.45f}};
    case BodyPart::Name::GlutealCleft:
      return {
          Envelope{MakeCapsuleVolume(MakePoint(MakeNode("NPC L Butt", -1.8f, -0.5f, -4.0f)),
                                     MakePoint(MakeNode("NPC L Butt", -0.8f, -3.5f, 2.5f)), 0.85f),
                   MakeCapsuleVolume(MakePoint(MakeNode("NPC R Butt", 1.8f, 0.5f, -4.0f)),
                                     MakePoint(MakeNode("NPC R Butt", 0.8f, -3.5f, 2.5f)), 0.85f),
                   0.45f}};
    case BodyPart::Name::FootLeft:
      return {Surface{MakeFace4(MakeNode("NPC L Foot [Lft ]", -1.2f, 0.0f, 0.0f),
                                MakeNode("NPC L Foot [Lft ]", 1.2f, 0.0f, 0.0f),
                                MakeNode("NPC L Toe0 [LToe]", 0.9f, 1.0f, 0.0f),
                                MakeNode("NPC L Toe0 [LToe]", -0.9f, 1.0f, 0.0f)),
                      0.45f}};
    case BodyPart::Name::FootRight:
      return {Surface{MakeFace4(MakeNode("NPC R Foot [Rft ]", -1.2f, 0.0f, 0.0f),
                                MakeNode("NPC R Foot [Rft ]", 1.2f, 0.0f, 0.0f),
                                MakeNode("NPC R Toe0 [RToe]", 0.9f, 1.0f, 0.0f),
                                MakeNode("NPC R Toe0 [RToe]", -0.9f, 1.0f, 0.0f)),
                      0.45f}};
    case BodyPart::Name::Vagina:
      return {Funnel{
          MakeFace3(MakeNode("NPC L Pussy02"), MakeNode("NPC R Pussy02"), MakeNode("VaginaB1")),
          MakePoint(MakeNode("VaginaDeep1")), 1.25f, 0.70f, 4.50f}};
    case BodyPart::Name::Anus:
      return {Funnel{MakeFace4(MakeNode("NPC RT Anus2"), MakeNode("NPC LT Anus2"),
                               MakeNode("NPC LB Anus2"), MakeNode("NPC RB Anus2")),
                     MakePoint2(MakeNode("NPC Anus Deep1"), MakeNode("NPC Anus Deep2")), 0.95f,
                     0.60f, 4.00f}};
    default:
      return {};
    }
  }

  VolumeList BuildPenisVolumes(Race::Type race)
  {
    static const std::unordered_map<Race::Type, std::array<std::string_view, 3>> penisNodes{
        {Race::Type::Human,
         {"NPC Genitals01 [Gen01]", "NPC Genitals04 [Gen04]", "NPC Genitals06 [Gen06]"}},
        {Race::Type::Bear, {"BearD 5", "BearD 7", "BearD 9"}},
        {Race::Type::Boar, {"BoarDick04", "BoarDick05", "BoarDick06"}},
        {Race::Type::BoarMounted, {"BoarDick04", "BoarDick05", "BoarDick06"}},
        {Race::Type::Chaurus, {"CO 4", "CO 7", "CO 9"}},
        {Race::Type::ChaurusHunter, {"CO 3", "CO 5", "CO 8"}},
        {Race::Type::ChaurusReaper, {"CO 4", "CO 7", "CO 9"}},
        {Race::Type::Deer, {"ElkD03", "ElkD05", "ElkD06"}},
        {Race::Type::Dog, {"CDPenis 3", "CDPenis 5", "CDPenis 7"}},
        {Race::Type::DragonPriest, {"DD 2", "DD 4", "DD 6"}},
        {Race::Type::Draugr, {"DD 2", "DD 4", "DD 6"}},
        {Race::Type::DwarvenCenturion,
         {"DwarvenBattery01", "DwarvenBattery02", "DwarvenInjectorLid"}},
        {Race::Type::DwarvenSpider, {"Dildo01", "Dildo02", "Dildo03"}},
        {Race::Type::Falmer, {"FD 3", "FD 5", "FD 7"}},
        {Race::Type::Fox, {"CDPenis 3", "CDPenis 5", "CDPenis 7"}},
        {Race::Type::FrostAtronach, {"NPC IceGenital03", "NPC IcePenis01", "NPC IcePenis02"}},
        {Race::Type::Gargoyle, {"GD 3", "GD 5", "GD 7"}},
        {Race::Type::Giant, {"GS 3", "GS 5", "GS 7"}},
        {Race::Type::GiantSpider, {"CO 5", "CO 7", "CO tip"}},
        {Race::Type::Horse, {"HS 5", "HS 6", "HorsePenisFlareTop2"}},
        {Race::Type::LargeSpider, {"CO 5", "CO 7", "CO tip"}},
        {Race::Type::Lurker, {"GS 3", "GS 5", "GS 7"}},
        {Race::Type::Riekling, {"RD 2", "RD 4", "RD 5"}},
        {Race::Type::Sabrecat, {"SCD 3", "SCD 5", "SCD 7"}},
        {Race::Type::Skeever, {"SkeeverD 03", "SkeeverD 05", "SkeeverD 07"}},
        {Race::Type::Spider, {"CO 5", "CO 7", "CO tip"}},
        {Race::Type::StormAtronach, {"", "Torso Rock 2", ""}},
        {Race::Type::Troll, {"TD 3", "TD 5", "TD 7"}},
        {Race::Type::VampireLord, {"VLDick03", "VLDick05", "VLDick06"}},
        {Race::Type::Werebear, {"WWD 5", "WWD 7", "WWD 9"}},
        {Race::Type::Werewolf, {"WWD 5", "WWD 7", "WWD 9"}},
        {Race::Type::Wolf, {"CDPenis 3", "CDPenis 5", "CDPenis 7"}},
    };

    const auto it = penisNodes.find(race);
    if (it == penisNodes.end())
      return {};

    return {
        MakeCapsuleVolume(MakePoint(Node(it->second[0])), MakePoint(Node(it->second[2])), 0.55f)};
  }

  VolumeList BuildVolumes(Race::Type race, BodyPart::Name name)
  {
    if (name == BodyPart::Name::Penis)
      return BuildPenisVolumes(race);

    if (race == Race::Type::Human)
      return BuildHumanVolumes(name);

    return {};
  }

  const VolumeProfile& GetVolumeProfileSpec(BodyPart::Name name)
  {
    static const std::unordered_map<BodyPart::Name, VolumeProfile> profiles{
        {BodyPart::Name::Mouth,
         MakeVolumeProfile(VolumeType::Funnel, MotionClass::Fine, true, true, false, true, true)},
        {BodyPart::Name::Throat, MakeVolumeProfile(VolumeType::Capsule, MotionClass::Static, false,
                                                   true, false, false, true)},
        {BodyPart::Name::BreastLeft, MakeVolumeProfile(VolumeType::HalfBall, MotionClass::Static,
                                                       false, true, false, false, true)},
        {BodyPart::Name::BreastRight, MakeVolumeProfile(VolumeType::HalfBall, MotionClass::Static,
                                                        false, true, false, false, true)},
        {BodyPart::Name::Cleavage, MakeVolumeProfile(VolumeType::Envelope, MotionClass::Static,
                                                     false, true, true, false, true)},
        {BodyPart::Name::HandLeft, MakeVolumeProfile(VolumeType::Surface, MotionClass::Fine, true,
                                                     false, false, true, false)},
        {BodyPart::Name::HandRight, MakeVolumeProfile(VolumeType::Surface, MotionClass::Fine, true,
                                                      false, false, true, false)},
        {BodyPart::Name::FingerLeft,
         MakeVolumeProfile(VolumeType::Ball, MotionClass::Fine, true, false, false, true, false)},
        {BodyPart::Name::FingerRight,
         MakeVolumeProfile(VolumeType::Ball, MotionClass::Fine, true, false, false, true, false)},
        {BodyPart::Name::Belly, MakeVolumeProfile(VolumeType::Surface, MotionClass::Static, false,
                                                  true, true, false, true)},
        {BodyPart::Name::ThighLeft, MakeVolumeProfile(VolumeType::Capsule, MotionClass::Static,
                                                      false, false, true, false, false)},
        {BodyPart::Name::ThighRight, MakeVolumeProfile(VolumeType::Capsule, MotionClass::Static,
                                                       false, false, true, false, false)},
        {BodyPart::Name::ThighCleft, MakeVolumeProfile(VolumeType::Envelope, MotionClass::Static,
                                                       false, true, true, false, true)},
        {BodyPart::Name::ButtLeft, MakeVolumeProfile(VolumeType::Ball, MotionClass::Static, false,
                                                     false, true, false, false)},
        {BodyPart::Name::ButtRight, MakeVolumeProfile(VolumeType::Ball, MotionClass::Static, false,
                                                      false, true, false, false)},
        {BodyPart::Name::GlutealCleft, MakeVolumeProfile(VolumeType::Envelope, MotionClass::Static,
                                                         false, true, true, false, true)},
        {BodyPart::Name::FootLeft, MakeVolumeProfile(VolumeType::Surface, MotionClass::Static,
                                                     false, true, false, false, true)},
        {BodyPart::Name::FootRight, MakeVolumeProfile(VolumeType::Surface, MotionClass::Static,
                                                      false, true, false, false, true)},
        {BodyPart::Name::Vagina, MakeVolumeProfile(VolumeType::Funnel, MotionClass::Static, false,
                                                   true, true, false, true)},
        {BodyPart::Name::Anus, MakeVolumeProfile(VolumeType::Funnel, MotionClass::Static, false,
                                                 true, true, false, true)},
        {BodyPart::Name::Penis, MakeVolumeProfile(VolumeType::Capsule, MotionClass::Large, true,
                                                  false, false, true, false)},
    };

    static const VolumeProfile fallback{};
    if (const auto it = profiles.find(name); it != profiles.end())
      return it->second;
    return fallback;
  }

}  // namespace

bool BodyPart::HasBodyPart(Gender gender, Race race, Name name)
{
  const bool isFemaleOrFuta =
      gender.Get() == Gender::Type::Female || gender.Get() == Gender::Type::Futa;

  switch (name) {
  case Name::Mouth:
  case Name::HandLeft:
  case Name::HandRight:
  case Name::FootLeft:
  case Name::FootRight:
    return true;
  case Name::BreastLeft:
  case Name::BreastRight:
  case Name::Cleavage:
  case Name::ThighCleft:
  case Name::GlutealCleft:
  case Name::Vagina:
    return isFemaleOrFuta;
  case Name::Throat:
  case Name::Belly:
  case Name::ThighLeft:
  case Name::ThighRight:
  case Name::ButtLeft:
  case Name::ButtRight:
  case Name::Anus:
    return race.GetType() == Race::Type::Human;
  case Name::Penis:
    return gender.HasPenis();
  default:
    return false;
  }
}

const VolumeProfile& BodyPart::GetVolumeProfile(Name name)
{
  return GetVolumeProfileSpec(name);
}

const VolumeProfile& BodyPart::GetVolumeProfile() const noexcept
{
  return GetVolumeProfile(name);
}

BodyPart::BodyPart(RE::Actor* actor, Race race, Name name) : actor(actor), name(name)
{
  volumes = BuildVolumes(race.GetType(), name);
  if (volumes.empty()) {
    logger::warn("BodyPart volume definition not found for race: {} and name: {}",
                 magic_enum::enum_name(race.GetType()), magic_enum::enum_name(name));
    return;
  }

  UpdateNodes();
  UpdatePosition();

  if (!valid)
    logger::warn("No valid volume nodes found for BodyPart: {}", magic_enum::enum_name(name));
}

bool BodyPart::IsDirectional() const noexcept
{
  return valid && vectorInfo.length > 1e-6f;
}

void BodyPart::UpdateNodes()
{
  if (!actor)
    return;

  for (auto& volume : volumes)
    UpdateVolumeNodes(volume, actor);
}

void BodyPart::UpdatePosition()
{
  vectorInfo = {};
  collisionInfo.reset();
  valid = false;

  if (volumes.empty())
    return;

  CollisionSet collisions;
  bool hasPrimaryAxis = false;

  for (const auto& volume : volumes) {
    const auto axis = AppendVolumeRuntime(name, actor, volume, collisions);
    if (!axis)
      continue;

    if (!hasPrimaryAxis) {
      vectorInfo     = *axis;
      hasPrimaryAxis = true;
    }
  }

  if (HasAnyCollision(collisions))
    collisionInfo = std::move(collisions);

  valid = hasPrimaryAxis || collisionInfo.has_value();
}

}  // namespace Define
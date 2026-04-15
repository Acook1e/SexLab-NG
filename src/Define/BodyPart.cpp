#include "Define/BodyPart.h"

#include "magic_enum/magic_enum.hpp"

namespace Define
{
namespace
{
  using NodeList = std::vector<Node>;
  using Shape    = BodyPart::Shape;

  struct ColliderSpec
  {
    float capsuleRadius          = 0.f;
    float boxHalfWidth           = 0.f;
    float boxHalfHeight          = 0.f;
    float boxHalfLengthScale     = 0.5f;
    float boxHalfLengthMin       = 0.f;
    float boxHalfLengthMax       = 0.f;
    Vector3f entranceHalfExtents = Vector3f::Zero();
    float entranceOffset         = 0.f;
    float channelRadius          = 0.f;
    float channelLengthScale     = 1.f;
  };

  ColliderSpec MakeCapsuleSpec(float capsuleRadius)
  {
    ColliderSpec spec;
    spec.capsuleRadius = capsuleRadius;
    return spec;
  }

  ColliderSpec MakeBoxSpec(float halfWidth, float halfHeight, float halfLengthMin,
                           float halfLengthMax, float halfLengthScale = 0.5f)
  {
    ColliderSpec spec;
    spec.boxHalfWidth       = halfWidth;
    spec.boxHalfHeight      = halfHeight;
    spec.boxHalfLengthScale = halfLengthScale;
    spec.boxHalfLengthMin   = halfLengthMin;
    spec.boxHalfLengthMax   = halfLengthMax;
    return spec;
  }

  ColliderSpec MakeApertureSpec(const Vector3f& entranceHalfExtents, float entranceOffset,
                                float channelRadius, float channelLengthScale)
  {
    ColliderSpec spec;
    spec.entranceHalfExtents = entranceHalfExtents;
    spec.entranceOffset      = entranceOffset;
    spec.channelRadius       = channelRadius;
    spec.channelLengthScale  = channelLengthScale;
    return spec;
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

  bool HasEntranceBox(const ColliderSpec& spec)
  {
    return spec.entranceHalfExtents.maxCoeff() > 0.f;
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
    Point3f centroid  = Point3f::Zero();
    std::size_t count = 0;

    for (std::string_view nodeName : nodeNames) {
      const auto position = TryResolveNodePosition(actor, nodeName);
      if (!position)
        continue;

      centroid += *position;
      ++count;
    }

    if (count == 0)
      return std::nullopt;

    centroid /= static_cast<float>(count);
    return centroid;
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

  const ColliderSpec& GetColliderSpec(BodyPart::Name name)
  {
    static const std::unordered_map<BodyPart::Name, ColliderSpec> colliderMap{
        {BodyPart::Name::Mouth, MakeCapsuleSpec(0.90f)},
        {BodyPart::Name::Throat, MakeCapsuleSpec(1.10f)},
        {BodyPart::Name::BreastLeft, MakeCapsuleSpec(1.70f)},
        {BodyPart::Name::BreastRight, MakeCapsuleSpec(1.70f)},
        {BodyPart::Name::Cleavage, MakeCapsuleSpec(1.20f)},
        {BodyPart::Name::HandLeft, MakeBoxSpec(0.80f, 0.50f, 1.25f, 3.00f, 0.45f)},
        {BodyPart::Name::HandRight, MakeBoxSpec(0.80f, 0.50f, 1.25f, 3.00f, 0.45f)},
        {BodyPart::Name::Belly, MakeCapsuleSpec(1.90f)},
        {BodyPart::Name::ThighCleft, MakeCapsuleSpec(1.00f)},
        {BodyPart::Name::GlutealCleft, MakeCapsuleSpec(1.10f)},
        {BodyPart::Name::FootLeft, MakeCapsuleSpec(0.90f)},
        {BodyPart::Name::FootRight, MakeCapsuleSpec(0.90f)},
        {BodyPart::Name::Vagina, MakeApertureSpec({1.25f, 1.00f, 0.60f}, 0.10f, 0.70f, 0.70f)},
        {BodyPart::Name::Anus, MakeApertureSpec({0.95f, 0.85f, 0.50f}, 0.08f, 0.60f, 0.65f)},
        {BodyPart::Name::Penis, MakeCapsuleSpec(0.55f)},
    };
    static const ColliderSpec fallback{};
    if (const auto it = colliderMap.find(name); it != colliderMap.end())
      return it->second;
    return fallback;
  }

  bool IsNodeResolved(const Node& node)
  {
    bool hasConcreteRequest = false;
    for (std::size_t index = 0; index < node.requestedNodeNames.size(); ++index) {
      if (node.requestedNodeNames[index].empty())
        continue;
      hasConcreteRequest = true;
      if (index >= node.resolvedNodes.size() || !node.resolvedNodes[index])
        return false;
    }
    return hasConcreteRequest;
  }

  std::vector<Point3f> CollectResolvedWorldPositions(const Node& node)
  {
    std::vector<Point3f> positions;
    positions.reserve(node.resolvedNodes.size());
    for (const auto& resolvedNode : node.resolvedNodes) {
      if (!resolvedNode)
        continue;
      const auto pos = resolvedNode->world.translate;
      positions.push_back(Point3f{pos.x, pos.y, pos.z});
    }
    return positions;
  }

  void UpdateNodePosition(Node& node)
  {
    node.position = Point3f::Zero();
    if (!IsNodeResolved(node))
      return;

    if (node.HasOffset()) {
      Point3f accumulated = Point3f::Zero();
      std::size_t count   = 0;
      for (const auto& resolvedNode : node.resolvedNodes) {
        if (!resolvedNode)
          continue;

        const RE::NiPoint3 scaledLocal = resolvedNode->world.scale * node.localTranslate;
        const RE::NiPoint3 worldPos =
            resolvedNode->world.rotate * scaledLocal + resolvedNode->world.translate;
        accumulated += Point3f(worldPos.x, worldPos.y, worldPos.z);
        ++count;
      }

      if (count == 0)
        return;

      node.position = accumulated / static_cast<float>(count);
      return;
    }

    const auto positions = CollectResolvedWorldPositions(node);
    if (positions.empty())
      return;

    Point3f center = Point3f::Zero();
    for (const auto& position : positions)
      center += position;
    node.position = center / static_cast<float>(positions.size());
  }

  Vector3f BuildResolvedNodeSpanAxis(const Node& node)
  {
    const auto positions = CollectResolvedWorldPositions(node);
    if (positions.size() >= 4)
      return (positions[0] + positions[3]) - (positions[1] + positions[2]);
    if (positions.size() >= 2)
      return positions[1] - positions[0];
    return Vector3f::Zero();
  }

  Matrix3f BuildBasis(const Vector3f& primaryAxis, const Node* referenceNode)
  {
    Vector3f forward = primaryAxis;
    if (forward.squaredNorm() <= 1e-6f)
      forward = Vector3f::UnitZ();
    else
      forward.normalize();

    Vector3f lateral = Vector3f::Zero();
    if (referenceNode)
      lateral = BuildResolvedNodeSpanAxis(*referenceNode);
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

    Matrix3f basis;
    basis.col(0) = lateral;
    basis.col(1) = up;
    basis.col(2) = forward;
    return basis;
  }

  Vector BuildBodyVector(BodyPart::Name name, Shape shape, RE::Actor* actor,
                         const std::vector<Node>& nodes)
  {
    if (nodes.empty() || !IsNodeResolved(nodes.front()))
      return {};

    if (shape == Shape::Point)
      return MakeVectorInfo(nodes.front().position, nodes.front().position);

    if (nodes.size() < 2 || !IsNodeResolved(nodes.back()))
      return {};

    Point3f start = nodes.front().position;
    Point3f end   = nodes.back().position;
    if (name == BodyPart::Name::Anus) {
      const auto ringPositions = CollectResolvedWorldPositions(nodes.front());
      const Vector3f anusAxis  = BuildAnusChannelAxis(actor, start, ringPositions, end - start);
      if (anusAxis.squaredNorm() > 1e-6f)
        end = start + anusAxis;
    }

    return MakeVectorInfo(start, end);
  }

  Vector3f BuildBoxHalfExtents(const Vector& axisInfo, const ColliderSpec& spec)
  {
    const float halfWidth =
        spec.boxHalfWidth > 0.f ? spec.boxHalfWidth : (std::max)(spec.capsuleRadius, 0.35f);
    const float halfHeight =
        spec.boxHalfHeight > 0.f ? spec.boxHalfHeight : (std::max)(halfWidth * 0.7f, 0.25f);

    float halfLength = axisInfo.length * spec.boxHalfLengthScale;
    halfLength =
        (std::max)(halfLength, spec.boxHalfLengthMin > 0.f ? spec.boxHalfLengthMin : halfWidth);
    if (spec.boxHalfLengthMax > 0.f)
      halfLength = (std::min)(halfLength, spec.boxHalfLengthMax);

    return {halfWidth, halfHeight, halfLength};
  }

}  // namespace

static std::unordered_map<Race::Type, std::array<Node, 2>> mouthMap{
    {Define::Race::Type::Human,
     {Node("NPC Head [Head]", {3.229f, 4.421f, -2.296f}), Node("NPC Head [Head]")}},
};

static std::unordered_map<Race::Type, std::array<Node, 2>> handLeftMap{
    {Define::Race::Type::Human,
     {Node({"NPC L Hand [LHnd]", "NPC L Finger01 [LF01]", "NPC L Finger40 [LF40]"}),
      Node("SHIELD")}},
};
static std::unordered_map<Race::Type, std::array<Node, 2>> handRightMap{
    {Define::Race::Type::Human,
     {Node({"NPC R Hand [RHnd]", "NPC R Finger01 [RF01]", "NPC R Finger40 [RF40]"}),
      Node("WEAPON")}},
};

static std::unordered_map<Race::Type, std::array<Node, 2>> footLeftMap{
    {Define::Race::Type::Human, {Node("NPC L Foot [Lft ]"), Node("NPC L Toe0 [LToe]")}},
};
static std::unordered_map<Race::Type, std::array<Node, 2>> footRightMap{
    {Define::Race::Type::Human, {Node("NPC R Foot [Rft ]"), Node("NPC R Toe0 [RToe]")}},
};

static std::unordered_map<Race::Type, std::array<Node, 3>> schlongMap{
    {Define::Race::Type::Human,
     {Node("NPC Genitals01 [Gen01]"), Node("NPC Genitals04 [Gen04]"),
      Node("NPC Genitals06 [Gen06]")}},
    {Define::Race::Type::Bear, {Node("BearD 5"), Node("BearD 7"), Node("BearD 9")}},
    {Define::Race::Type::Boar, {Node("BoarDick04"), Node("BoarDick05"), Node("BoarDick06")}},
    {Define::Race::Type::BoarMounted, {Node("BoarDick04"), Node("BoarDick05"), Node("BoarDick06")}},
    {Define::Race::Type::Chaurus, {Node("CO 4"), Node("CO 7"), Node("CO 9")}},
    {Define::Race::Type::ChaurusHunter, {Node("CO 3"), Node("CO 5"), Node("CO 8")}},
    {Define::Race::Type::ChaurusReaper, {Node("CO 4"), Node("CO 7"), Node("CO 9")}},
    {Define::Race::Type::Deer, {Node("ElkD03"), Node("ElkD05"), Node("ElkD06")}},
    {Define::Race::Type::Dog, {Node("CDPenis 3"), Node("CDPenis 5"), Node("CDPenis 7")}},
    {Define::Race::Type::DragonPriest, {Node("DD 2"), Node("DD 4"), Node("DD 6")}},
    {Define::Race::Type::Draugr, {Node("DD 2"), Node("DD 4"), Node("DD 6")}},
    {Define::Race::Type::DwarvenCenturion,
     {Node("DwarvenBattery01"), Node("DwarvenBattery02"), Node("DwarvenInjectorLid")}},
    {Define::Race::Type::DwarvenSpider, {Node("Dildo01"), Node("Dildo02"), Node("Dildo03")}},
    {Define::Race::Type::Falmer, {Node("FD 3"), Node("FD 5"), Node("FD 7")}},
    {Define::Race::Type::Fox, {Node("CDPenis 3"), Node("CDPenis 5"), Node("CDPenis 7")}},
    {Define::Race::Type::FrostAtronach,
     {Node("NPC IceGenital03"), Node("NPC IcePenis01"), Node("NPC IcePenis02")}},
    {Define::Race::Type::Gargoyle, {Node("GD 3"), Node("GD 5"), Node("GD 7")}},
    {Define::Race::Type::Giant, {Node("GS 3"), Node("GS 5"), Node("GS 7")}},
    {Define::Race::Type::GiantSpider, {Node("CO 5"), Node("CO 7"), Node("CO tip")}},
    {Define::Race::Type::Horse, {Node("HS 5"), Node("HS 6"), Node("HorsePenisFlareTop2")}},
    {Define::Race::Type::LargeSpider, {Node("CO 5"), Node("CO 7"), Node("CO tip")}},
    {Define::Race::Type::Lurker, {Node("GS 3"), Node("GS 5"), Node("GS 7")}},
    {Define::Race::Type::Riekling, {Node("RD 2"), Node("RD 4"), Node("RD 5")}},
    {Define::Race::Type::Sabrecat, {Node("SCD 3"), Node("SCD 5"), Node("SCD 7")}},
    {Define::Race::Type::Skeever, {Node("SkeeverD 03"), Node("SkeeverD 05"), Node("SkeeverD 07")}},
    {Define::Race::Type::Spider, {Node("CO 5"), Node("CO 7"), Node("CO tip")}},
    {Define::Race::Type::StormAtronach, {Node(""), Node("Torso Rock 2"), Node("")}},
    {Define::Race::Type::Troll, {Node("TD 3"), Node("TD 5"), Node("TD 7")}},
    {Define::Race::Type::VampireLord, {Node("VLDick03"), Node("VLDick05"), Node("VLDick06")}},
    {Define::Race::Type::Werebear, {Node("WWD 5"), Node("WWD 7"), Node("WWD 9")}},
    {Define::Race::Type::Werewolf, {Node("WWD 5"), Node("WWD 7"), Node("WWD 9")}},
    {Define::Race::Type::Wolf, {Node("CDPenis 3"), Node("CDPenis 5"), Node("CDPenis 7")}},
};

static std::unordered_map<BodyPart::Name, NodeList> humanMap{
    {BodyPart::Name::Throat, {Node("NPC Head [Head]"), Node("NPC Neck [Neck]")}},
    {BodyPart::Name::BreastLeft, {Node("L Breast02"), Node("L Breast03")}},
    {BodyPart::Name::BreastRight, {Node("R Breast02"), Node("R Breast03")}},
    // 从下乳沟指向上乳沟
    {BodyPart::Name::Cleavage,
     {Node({"L Breast02", "R Breast02"}, {0.f, 1.f, -4.f}),
      Node({"L Breast02", "R Breast02"}, {0.f, 1.f, 4.f})}},
    {BodyPart::Name::FingerLeft, {Node("NPC L Finger22 [LF22]")}},
    {BodyPart::Name::FingerRight, {Node("NPC R Finger22 [RF22]")}},
    // 从下腹部指向肚脐
    {BodyPart::Name::Belly,
     {Node("NPC Belly", {0.f, 3.f, -10.f}), Node("NPC Belly", {0.f, 5.f, -1.0f})}},
    {BodyPart::Name::ThighLeft, {Node("NPC L RearThigh"), Node("NPC L FrontThigh")}},
    {BodyPart::Name::ThighRight, {Node("NPC R RearThigh"), Node("NPC R FrontThigh")}},
    {BodyPart::Name::ThighCleft,
     {Node("VaginaB1", {0.f, -3.f, -2.f}), Node("Clitoral1", {0.f, -1.f, -3.f})}},
    // 左臀中心稍微靠左
    {BodyPart::Name::ButtLeft, {Node("NPC L Butt", {-5.f, -1.f, -3.f})}},
    {BodyPart::Name::ButtRight, {Node("NPC R Butt", {5.f, 1.f, -3.f})}},
    // 臀沟末尾指向臀沟上方靠近后腰
    {BodyPart::Name::GlutealCleft,
     {Node({"NPC L Butt", "NPC R Butt"}), Node("VaginaB1", {0.f, -4.5f, 3.f})}},
    {BodyPart::Name::Vagina,
     {Node({"NPC L Pussy02", "NPC R Pussy02", "VaginaB1"}), Node("VaginaDeep1")}},
    {BodyPart::Name::Anus,
     {Node({"NPC RT Anus2", "NPC LT Anus2", "NPC LB Anus2", "NPC RB Anus2"}),
      Node({"NPC Anus Deep1", "NPC Anus Deep2"})}},
};

static const std::unordered_map<BodyPart::Name, BodyPart::Shape> shapeMap{
    {BodyPart::Name::Mouth, BodyPart::Shape::CapsuleCollider},
    {BodyPart::Name::Throat, BodyPart::Shape::CapsuleCollider},
    {BodyPart::Name::BreastLeft, BodyPart::Shape::CapsuleCollider},
    {BodyPart::Name::BreastRight, BodyPart::Shape::CapsuleCollider},
    {BodyPart::Name::Cleavage, BodyPart::Shape::CapsuleCollider},
    {BodyPart::Name::HandLeft, BodyPart::Shape::BoxCollider},
    {BodyPart::Name::HandRight, BodyPart::Shape::BoxCollider},
    {BodyPart::Name::FingerLeft, BodyPart::Shape::Point},
    {BodyPart::Name::FingerRight, BodyPart::Shape::Point},
    {BodyPart::Name::Belly, BodyPart::Shape::CapsuleCollider},
    {BodyPart::Name::ThighLeft, BodyPart::Shape::Segment},
    {BodyPart::Name::ThighRight, BodyPart::Shape::Segment},
    {BodyPart::Name::ThighCleft, BodyPart::Shape::CapsuleCollider},
    {BodyPart::Name::ButtLeft, BodyPart::Shape::Point},
    {BodyPart::Name::ButtRight, BodyPart::Shape::Point},
    {BodyPart::Name::GlutealCleft, BodyPart::Shape::CapsuleCollider},
    {BodyPart::Name::FootLeft, BodyPart::Shape::CapsuleCollider},
    {BodyPart::Name::FootRight, BodyPart::Shape::CapsuleCollider},
    {BodyPart::Name::Vagina, BodyPart::Shape::CapsuleCollider},
    {BodyPart::Name::Anus, BodyPart::Shape::CapsuleCollider},
    {BodyPart::Name::Penis, BodyPart::Shape::CapsuleCollider},
};

bool BodyPart::HasBodyPart(Gender gender, Race race, Name a_name)
{
  const bool isFemaleOrFuta =
      gender.Get() == Define::Gender::Type::Female || gender.Get() == Define::Gender::Type::Futa;

  switch (a_name) {
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
    return race.GetType() == Define::Race::Type::Human;
  case Name::Penis:
    return gender.HasPenis();
  default:
    return false;
  }
}

BodyPart::BodyPart(RE::Actor* a_actor, Race a_race, Name a_name) : actor(a_actor), name(a_name)
{
  const auto appendNodes = [&](const auto& definitions) {
    nodes.insert(nodes.end(), definitions.begin(), definitions.end());
  };

  if (const auto it = shapeMap.find(a_name); it != shapeMap.end())
    shape = it->second;

  switch (a_name) {
  case Name::Mouth:
    if (auto it = mouthMap.find(a_race.GetType()); it != mouthMap.end())
      appendNodes(it->second);
    break;
  case Name::HandLeft:
    if (auto it = handLeftMap.find(a_race.GetType()); it != handLeftMap.end())
      appendNodes(it->second);
    break;
  case Name::HandRight:
    if (auto it = handRightMap.find(a_race.GetType()); it != handRightMap.end())
      appendNodes(it->second);
    break;
  case Name::FootLeft:
    if (auto it = footLeftMap.find(a_race.GetType()); it != footLeftMap.end())
      appendNodes(it->second);
    break;
  case Name::FootRight:
    if (auto it = footRightMap.find(a_race.GetType()); it != footRightMap.end())
      appendNodes(it->second);
    break;
  case Name::Penis:
    if (auto it = schlongMap.find(a_race.GetType()); it != schlongMap.end())
      appendNodes(it->second);
    else
      logger::warn("No schlong mapping for race: {}", magic_enum::enum_name(a_race.GetType()));
    break;
  case Name::Throat:
  case Name::BreastLeft:
  case Name::BreastRight:
  case Name::Cleavage:
  case Name::FingerLeft:
  case Name::FingerRight:
  case Name::Belly:
  case Name::ThighLeft:
  case Name::ThighRight:
  case Name::ThighCleft:
  case Name::ButtLeft:
  case Name::ButtRight:
  case Name::GlutealCleft:
  case Name::Vagina:
  case Name::Anus:
    if (auto it = humanMap.find(a_name); it != humanMap.end())
      appendNodes(it->second);
    break;
  default:
    break;
  }

  if (nodes.empty())
    logger::warn("BodyPart not found for race: {} and name: {}",
                 magic_enum::enum_name(a_race.GetType()), magic_enum::enum_name(a_name));

  UpdateNodes();

  if (!IsValid())
    logger::warn("No valid nodes found for BodyPart with name: {}", magic_enum::enum_name(a_name));

  UpdatePosition();
}

void BodyPart::UpdateNodes()
{
  if (!actor)
    return;

  for (auto& node : nodes) {
    node.resolvedNodes.clear();
    node.resolvedNodes.reserve(node.requestedNodeNames.size());

    for (std::string_view nodeName : node.requestedNodeNames) {
      if (nodeName.empty()) {
        node.resolvedNodes.push_back(nullptr);
        continue;
      }

      auto* obj = actor->GetNodeByName(nodeName);
      if (!obj)
        logger::warn("Node not found for {}: {}", magic_enum::enum_name(name), nodeName);

      auto ptr = obj ? obj->AsNode() : nullptr;
      node.resolvedNodes.push_back(RE::NiPointer<RE::NiNode>(ptr));
    }

    UpdateNodePosition(node);
  }
}

bool BodyPart::IsValid() const
{
  if (nodes.empty())
    return false;

  switch (shape) {
  case Shape::Point:
    return IsNodeResolved(nodes.front());
  case Shape::Segment:
  case Shape::CapsuleCollider:
  case Shape::BoxCollider:
    return nodes.size() >= 2 && IsNodeResolved(nodes.front()) && IsNodeResolved(nodes.back());
  default:
    return false;
  }
}

void BodyPart::UpdatePosition()
{
  vectorInfo = {};
  collisionInfo.reset();

  for (auto& node : nodes)
    UpdateNodePosition(node);

  if (!IsValid()) {
    logger::warn("Cannot update position for invalid BodyPart: {}", magic_enum::enum_name(name));
    return;
  }

  vectorInfo = BuildBodyVector(name, shape, actor, nodes);

  switch (shape) {
  case Shape::Point:
    return;
  case Shape::Segment:
    return;
  case Shape::CapsuleCollider: {
    const auto& spec      = GetColliderSpec(name);
    const bool isAperture = name == Name::Vagina || name == Name::Anus;
    CollisionSet collisions;

    if (isAperture && vectorInfo.length > 1e-6f) {
      const Matrix3f basis = BuildBasis(vectorInfo.direction, &nodes.front());
      if (HasEntranceBox(spec)) {
        collisions.boxes.push_back(MakeBoxCollider(
            vectorInfo, vectorInfo.start + vectorInfo.direction * spec.entranceOffset, basis,
            spec.entranceHalfExtents));
      }
      if (spec.channelRadius > 0.f) {
        const float channelLength =
            std::clamp(vectorInfo.length * spec.channelLengthScale, 0.f, vectorInfo.length);
        collisions.capsules.push_back(MakeCapsuleCollider(
            vectorInfo.start, vectorInfo.start + vectorInfo.direction * channelLength,
            spec.channelRadius));
      }
    } else if (spec.capsuleRadius > 0.f) {
      collisions.capsules.push_back(
          MakeCapsuleCollider(vectorInfo.start, vectorInfo.end, spec.capsuleRadius));
    }

    if (!collisions.capsules.empty() || !collisions.boxes.empty())
      collisionInfo = collisions;
    return;
  }
  case Shape::BoxCollider: {
    const auto& spec     = GetColliderSpec(name);
    const Matrix3f basis = BuildBasis(vectorInfo.direction, &nodes.front());
    const Point3f center = (vectorInfo.start + vectorInfo.end) * 0.5f;

    CollisionSet collisions;
    collisions.boxes.push_back(
        MakeBoxCollider(vectorInfo, center, basis, BuildBoxHalfExtents(vectorInfo, spec)));
    collisionInfo = collisions;
    return;
  }
  default:
    return;
  }
}

}  // namespace Define
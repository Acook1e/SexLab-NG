#include "Define/BodyPart.h"

#include "magic_enum/magic_enum.hpp"

namespace Define
{
namespace
{
  using NodeList = std::vector<Node>;

  struct ColliderSpec
  {
    float capsuleRadius          = 0.f;
    Vector3f entranceHalfExtents = Vector3f::Zero();
    float entranceOffset         = 0.f;
    float channelRadius          = 0.f;
    float channelLengthScale     = 1.f;
  };

  Matrix3f MakeEulerRotation(const Vector3f& eulerRot)
  {
    return (Eigen::AngleAxisf(eulerRot.x(), Vector3f::UnitX()) *
            Eigen::AngleAxisf(eulerRot.y(), Vector3f::UnitY()) *
            Eigen::AngleAxisf(eulerRot.z(), Vector3f::UnitZ()))
        .toRotationMatrix();
  }

  Matrix3f ToEigenMatrix(const RE::NiMatrix3& rotation)
  {
    return Eigen::Map<const Matrix3f>(&rotation.entry[0][0]).transpose();
  }

  Node MakeNode(std::string_view nodeName)
  {
    Node node;
    node.requestedNodeNames.push_back(nodeName);
    return node;
  }

  Node MakeNode(std::initializer_list<std::string_view> nodeNames)
  {
    Node node;
    node.requestedNodeNames.assign(nodeNames.begin(), nodeNames.end());
    return node;
  }

  Node MakeOffsetNode(std::string_view baseNode, const Vector3f& offsetEuler,
                      const Vector3f& offsetTranslation)
  {
    Node node              = MakeNode(baseNode);
    node.offsetRotation    = MakeEulerRotation(offsetEuler);
    node.offsetTranslation = offsetTranslation;
    node.hasOffset         = true;
    return node;
  }

  Node MakeOffsetNode(std::initializer_list<std::string_view> nodeNames,
                      const Vector3f& offsetEuler, const Vector3f& offsetTranslation)
  {
    Node node              = MakeNode(nodeNames);
    node.offsetRotation    = MakeEulerRotation(offsetEuler);
    node.offsetTranslation = offsetTranslation;
    node.hasOffset         = true;
    return node;
  }

  NodeList MakeNodeList(std::initializer_list<Node> nodes)
  {
    return NodeList{nodes};
  }

  NodeList MakeVectorNodeList(const Node& startNode, const Node& endNode)
  {
    return MakeNodeList({startNode, endNode});
  }

  NodeList MakeOffsetVectorNodeList(std::string_view baseNode, const Vector3f& startOffset,
                                    const Vector3f& endOffset,
                                    const Vector3f& startEuler = Vector3f::Zero(),
                                    const Vector3f& endEuler   = Vector3f::Zero())
  {
    return MakeVectorNodeList(MakeOffsetNode(baseNode, startEuler, startOffset),
                              MakeOffsetNode(baseNode, endEuler, endOffset));
  }

  NodeList MakeOffsetVectorNodeList(std::initializer_list<std::string_view> baseNodes,
                                    const Vector3f& startOffset, const Vector3f& endOffset,
                                    const Vector3f& startEuler = Vector3f::Zero(),
                                    const Vector3f& endEuler   = Vector3f::Zero())
  {
    return MakeVectorNodeList(MakeOffsetNode(baseNodes, startEuler, startOffset),
                              MakeOffsetNode(baseNodes, endEuler, endOffset));
  }

  NodeList MakeOffsetVectorNodeList(std::string_view startBaseNode, const Vector3f& startOffset,
                                    std::string_view endBaseNode, const Vector3f& endOffset,
                                    const Vector3f& startEuler = Vector3f::Zero(),
                                    const Vector3f& endEuler   = Vector3f::Zero())
  {
    return MakeVectorNodeList(MakeOffsetNode(startBaseNode, startEuler, startOffset),
                              MakeOffsetNode(endBaseNode, endEuler, endOffset));
  }

  std::optional<Point3f> TryComputeAverageWorldPosition(const Node& node)
  {
    Point3f average   = Point3f::Zero();
    std::size_t count = 0;
    for (const auto& resolvedNode : node.resolvedNodes) {
      if (!resolvedNode)
        continue;

      const auto& translate = resolvedNode->world.translate;
      average += Point3f{translate.x, translate.y, translate.z};
      ++count;
    }

    if (count == 0)
      return std::nullopt;

    return average / static_cast<float>(count);
  }

  std::optional<Matrix3f> TryComputeAverageWorldRotation(const Node& node)
  {
    Eigen::Quaternionf averageQuat(0.f, 0.f, 0.f, 0.f);
    bool hasRotation  = false;
    std::size_t count = 0;
    for (const auto& resolvedNode : node.resolvedNodes) {
      if (!resolvedNode)
        continue;

      Eigen::Quaternionf quat(ToEigenMatrix(resolvedNode->world.rotate));
      if (!hasRotation) {
        averageQuat = quat;
        hasRotation = true;
      } else {
        if (averageQuat.dot(quat) < 0.f)
          quat.coeffs() = -quat.coeffs();
        averageQuat.coeffs() += quat.coeffs();
      }
      ++count;
    }

    if (!hasRotation || count == 0)
      return std::nullopt;

    averageQuat.coeffs() /= static_cast<float>(count);
    averageQuat.normalize();
    return averageQuat.toRotationMatrix();
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
        {BodyPart::Name::Mouth, {0.90f}},
        {BodyPart::Name::Throat, {1.10f}},
        {BodyPart::Name::BreastLeft, {1.70f}},
        {BodyPart::Name::BreastRight, {1.70f}},
        {BodyPart::Name::Cleavage, {1.20f}},
        {BodyPart::Name::HandLeft, {0.90f}},
        {BodyPart::Name::HandRight, {0.90f}},
        {BodyPart::Name::FingerLeft, {0.45f}},
        {BodyPart::Name::FingerRight, {0.45f}},
        {BodyPart::Name::Belly, {1.90f}},
        {BodyPart::Name::ThighLeft, {1.80f}},
        {BodyPart::Name::ThighRight, {1.80f}},
        {BodyPart::Name::ThighCleft, {1.00f}},
        {BodyPart::Name::ButtLeft, {1.60f}},
        {BodyPart::Name::ButtRight, {1.60f}},
        {BodyPart::Name::GlutealCleft, {1.10f}},
        {BodyPart::Name::FootLeft, {0.90f}},
        {BodyPart::Name::FootRight, {0.90f}},
        {BodyPart::Name::Vagina, {0.f, {1.25f, 1.00f, 0.60f}, 0.10f, 0.70f, 0.70f}},
        {BodyPart::Name::Anus, {0.f, {0.95f, 0.85f, 0.50f}, 0.08f, 0.60f, 0.65f}},
        {BodyPart::Name::Penis, {0.55f}},
    };
    static const ColliderSpec fallback{};
    if (const auto it = colliderMap.find(name); it != colliderMap.end())
      return it->second;
    return fallback;
  }

}  // namespace

static std::unordered_map<Race::Type, std::array<Node, 2>> mouthMap{
    {Define::Race::Type::Human,
     {MakeOffsetNode("NPC Head [Head]", {0.054f, 0.067f, 0.608f}, {-0.061f, 5.487f, -2.266f}),
      MakeNode("NPC Head [Head]")}},
};

static std::unordered_map<Race::Type, std::array<Node, 2>> handLeftMap{
    {Define::Race::Type::Human,
     {MakeNode({"NPC L Hand [LHnd]", "NPC L Finger01 [LF01]", "NPC L Finger40 [LF40]"}),
      MakeNode("SHIELD")}},
};
static std::unordered_map<Race::Type, std::array<Node, 2>> handRightMap{
    {Define::Race::Type::Human,
     {MakeNode({"NPC R Hand [RHnd]", "NPC R Finger01 [RF01]", "NPC R Finger40 [RF40]"}),
      MakeNode("WEAPON")}},
};

static std::unordered_map<Race::Type, std::array<Node, 2>> footLeftMap{
    {Define::Race::Type::Human, {MakeNode("NPC L Foot [Lft ]"), MakeNode("NPC L Toe0 [LToe]")}},
};
static std::unordered_map<Race::Type, std::array<Node, 2>> footRightMap{
    {Define::Race::Type::Human, {MakeNode("NPC R Foot [Rft ]"), MakeNode("NPC R Toe0 [RToe]")}},
};

static std::unordered_map<Race::Type, std::array<Node, 3>> schlongMap{
    {Define::Race::Type::Human,
     {MakeNode("NPC Genitals01 [Gen01]"), MakeNode("NPC Genitals04 [Gen04]"),
      MakeNode("NPC Genitals06 [Gen06]")}},
    {Define::Race::Type::Bear, {MakeNode("BearD 5"), MakeNode("BearD 7"), MakeNode("BearD 9")}},
    {Define::Race::Type::Boar,
     {MakeNode("BoarDick04"), MakeNode("BoarDick05"), MakeNode("BoarDick06")}},
    {Define::Race::Type::BoarMounted,
     {MakeNode("BoarDick04"), MakeNode("BoarDick05"), MakeNode("BoarDick06")}},
    {Define::Race::Type::Chaurus, {MakeNode("CO 4"), MakeNode("CO 7"), MakeNode("CO 9")}},
    {Define::Race::Type::ChaurusHunter, {MakeNode("CO 3"), MakeNode("CO 5"), MakeNode("CO 8")}},
    {Define::Race::Type::ChaurusReaper, {MakeNode("CO 4"), MakeNode("CO 7"), MakeNode("CO 9")}},
    {Define::Race::Type::Deer, {MakeNode("ElkD03"), MakeNode("ElkD05"), MakeNode("ElkD06")}},
    {Define::Race::Type::Dog,
     {MakeNode("CDPenis 3"), MakeNode("CDPenis 5"), MakeNode("CDPenis 7")}},
    {Define::Race::Type::DragonPriest, {MakeNode("DD 2"), MakeNode("DD 4"), MakeNode("DD 6")}},
    {Define::Race::Type::Draugr, {MakeNode("DD 2"), MakeNode("DD 4"), MakeNode("DD 6")}},
    {Define::Race::Type::DwarvenCenturion,
     {MakeNode("DwarvenBattery01"), MakeNode("DwarvenBattery02"), MakeNode("DwarvenInjectorLid")}},
    {Define::Race::Type::DwarvenSpider,
     {MakeNode("Dildo01"), MakeNode("Dildo02"), MakeNode("Dildo03")}},
    {Define::Race::Type::Falmer, {MakeNode("FD 3"), MakeNode("FD 5"), MakeNode("FD 7")}},
    {Define::Race::Type::Fox,
     {MakeNode("CDPenis 3"), MakeNode("CDPenis 5"), MakeNode("CDPenis 7")}},
    {Define::Race::Type::FrostAtronach,
     {MakeNode("NPC IceGenital03"), MakeNode("NPC IcePenis01"), MakeNode("NPC IcePenis02")}},
    {Define::Race::Type::Gargoyle, {MakeNode("GD 3"), MakeNode("GD 5"), MakeNode("GD 7")}},
    {Define::Race::Type::Giant, {MakeNode("GS 3"), MakeNode("GS 5"), MakeNode("GS 7")}},
    {Define::Race::Type::GiantSpider, {MakeNode("CO 5"), MakeNode("CO 7"), MakeNode("CO tip")}},
    {Define::Race::Type::Horse,
     {MakeNode("HS 5"), MakeNode("HS 6"), MakeNode("HorsePenisFlareTop2")}},
    {Define::Race::Type::LargeSpider, {MakeNode("CO 5"), MakeNode("CO 7"), MakeNode("CO tip")}},
    {Define::Race::Type::Lurker, {MakeNode("GS 3"), MakeNode("GS 5"), MakeNode("GS 7")}},
    {Define::Race::Type::Riekling, {MakeNode("RD 2"), MakeNode("RD 4"), MakeNode("RD 5")}},
    {Define::Race::Type::Sabrecat, {MakeNode("SCD 3"), MakeNode("SCD 5"), MakeNode("SCD 7")}},
    {Define::Race::Type::Skeever,
     {MakeNode("SkeeverD 03"), MakeNode("SkeeverD 05"), MakeNode("SkeeverD 07")}},
    {Define::Race::Type::Spider, {MakeNode("CO 5"), MakeNode("CO 7"), MakeNode("CO tip")}},
    {Define::Race::Type::StormAtronach, {MakeNode(""), MakeNode("Torso Rock 2"), MakeNode("")}},
    {Define::Race::Type::Troll, {MakeNode("TD 3"), MakeNode("TD 5"), MakeNode("TD 7")}},
    {Define::Race::Type::VampireLord,
     {MakeNode("VLDick03"), MakeNode("VLDick05"), MakeNode("VLDick06")}},
    {Define::Race::Type::Werebear, {MakeNode("WWD 5"), MakeNode("WWD 7"), MakeNode("WWD 9")}},
    {Define::Race::Type::Werewolf, {MakeNode("WWD 5"), MakeNode("WWD 7"), MakeNode("WWD 9")}},
    {Define::Race::Type::Wolf,
     {MakeNode("CDPenis 3"), MakeNode("CDPenis 5"), MakeNode("CDPenis 7")}},
};

static std::unordered_map<BodyPart::Name, NodeList> humanMap{
    {BodyPart::Name::Throat,
     MakeVectorNodeList(MakeNode("NPC Head [Head]"), MakeNode("NPC Neck [Neck]"))},
    {BodyPart::Name::BreastLeft,
     MakeVectorNodeList(MakeNode("L Breast02"), MakeNode("L Breast03"))},
    {BodyPart::Name::BreastRight,
     MakeVectorNodeList(MakeNode("R Breast02"), MakeNode("R Breast03"))},
    // 从下乳沟指向上乳沟
    {BodyPart::Name::Cleavage,
     MakeOffsetVectorNodeList({"L Breast02", "R Breast02"}, {0.f, 1.f, -4.f}, {0.f, 1.f, 4.f})},
    {BodyPart::Name::FingerLeft, MakeNodeList({MakeNode("NPC L Finger22 [LF22]")})},
    {BodyPart::Name::FingerRight, MakeNodeList({MakeNode("NPC R Finger22 [RF22]")})},
    // 从下腹部指向肚脐
    {BodyPart::Name::Belly,
     MakeOffsetVectorNodeList("NPC Belly", {0.f, 3.f, -10.f}, {0.f, 5.f, -1.f})},
    {BodyPart::Name::ThighLeft,
     MakeVectorNodeList(MakeNode("NPC L RearThigh"), MakeNode("NPC L FrontThigh"))},
    {BodyPart::Name::ThighRight,
     MakeVectorNodeList(MakeNode("NPC R RearThigh"), MakeNode("NPC R FrontThigh"))},
    {BodyPart::Name::ThighCleft,
     MakeOffsetVectorNodeList("VaginaB1", {0.f, -3.f, -2.f}, "Clitoral1", {0.f, -1.f, -3.f})},
    // 左臀中心稍微靠左
    {BodyPart::Name::ButtLeft,
     MakeNodeList({MakeOffsetNode("NPC L Butt", Vector3f::Zero(), {-5.f, -1.f, -3.f})})},
    {BodyPart::Name::ButtRight,
     MakeNodeList({MakeOffsetNode("NPC R Butt", Vector3f::Zero(), {5.f, 1.f, -3.f})})},
    // 臀沟末尾指向臀沟上方靠近后腰
    {BodyPart::Name::GlutealCleft,
     MakeVectorNodeList(MakeNode({"NPC L Butt", "NPC R Butt"}),
                        MakeOffsetNode("VaginaB1", Vector3f::Zero(), {0.f, -4.5f, 3.f}))},
    {BodyPart::Name::Vagina,
     MakeVectorNodeList(MakeNode({"NPC L Pussy02", "NPC R Pussy02", "VaginaB1"}),
                        MakeNode("VaginaDeep1"))},
    {BodyPart::Name::Anus,
     MakeVectorNodeList(MakeNode({"NPC RT Anus2", "NPC LT Anus2", "NPC LB Anus2", "NPC RB Anus2"}),
                        MakeNode({"NPC Anus Deep1", "NPC Anus Deep2"}))},
};

static std::unordered_map<BodyPart::Name, BodyPart::Shape> shapeMap{
    {BodyPart::Name::Mouth, BodyPart::Shape::Vector},
    {BodyPart::Name::Throat, BodyPart::Shape::Vector},
    {BodyPart::Name::BreastLeft, BodyPart::Shape::Vector},
    {BodyPart::Name::BreastRight, BodyPart::Shape::Vector},
    {BodyPart::Name::Cleavage, BodyPart::Shape::Vector},
    {BodyPart::Name::HandLeft, BodyPart::Shape::Vector},
    {BodyPart::Name::HandRight, BodyPart::Shape::Vector},
    {BodyPart::Name::FingerLeft, BodyPart::Shape::Point},
    {BodyPart::Name::FingerRight, BodyPart::Shape::Point},
    {BodyPart::Name::Belly, BodyPart::Shape::Vector},
    {BodyPart::Name::ThighLeft, BodyPart::Shape::Vector},
    {BodyPart::Name::ThighRight, BodyPart::Shape::Vector},
    {BodyPart::Name::ThighCleft, BodyPart::Shape::Vector},
    {BodyPart::Name::ButtLeft, BodyPart::Shape::Point},
    {BodyPart::Name::ButtRight, BodyPart::Shape::Point},
    {BodyPart::Name::GlutealCleft, BodyPart::Shape::Vector},
    {BodyPart::Name::FootLeft, BodyPart::Shape::Vector},
    {BodyPart::Name::FootRight, BodyPart::Shape::Vector},
    {BodyPart::Name::Vagina, BodyPart::Shape::Vector},
    {BodyPart::Name::Anus, BodyPart::Shape::Vector},
    {BodyPart::Name::Penis, BodyPart::Shape::Vector},
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
  if (const auto it = shapeMap.find(a_name); it != shapeMap.end())
    shape = it->second;

  info.vector.emplace();
  info.collider.reset();

  auto& vectorInfo       = info.vector.value();
  const auto appendNodes = [&](const auto& definitions) {
    for (const auto& definition : definitions)
      vectorInfo.nodes.push_back(definition);
  };

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

  if (vectorInfo.nodes.empty())
    logger::warn("BodyPart not found for race: {} and name: {}",
                 magic_enum::enum_name(a_race.GetType()), magic_enum::enum_name(a_name));

  UpdateNodes();

  if (!IsValid())
    logger::warn("No valid nodes found for BodyPart with name: {}", magic_enum::enum_name(a_name));

  UpdatePosition();
}

void BodyPart::UpdateNodes()
{
  if (!actor || !info.vector)
    return;

  for (auto& node : info.vector->nodes) {
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

bool BodyPart::IsNodeResolved(const Node& node) const
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

std::vector<Point3f> BodyPart::CollectResolvedWorldPositions(const Node& node) const
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

void BodyPart::UpdateNodePosition(Node& node)
{
  node.position = Point3f::Zero();
  if (!IsNodeResolved(node))
    return;

  if (node.hasOffset) {
    const auto basePosition = TryComputeAverageWorldPosition(node);
    const auto baseRotation = TryComputeAverageWorldRotation(node);
    if (!basePosition || !baseRotation)
      return;

    const Vector3f worldOffset = *baseRotation * (node.offsetRotation * node.offsetTranslation);
    node.position              = *basePosition + worldOffset;
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

std::vector<Point3f> BodyPart::CollectValidNodePositions(const Vector& vectorInfo) const
{
  std::vector<Point3f> positions;
  positions.reserve(vectorInfo.nodes.size());
  for (const auto& node : vectorInfo.nodes) {
    if (!IsNodeResolved(node))
      continue;
    positions.push_back(node.position);
  }
  return positions;
}

Vector3f BodyPart::BuildResolvedNodeSpanAxis(const Node& node) const
{
  const auto positions = CollectResolvedWorldPositions(node);
  if (positions.size() >= 4)
    return (positions[0] + positions[3]) - (positions[1] + positions[2]);
  if (positions.size() >= 2)
    return positions[1] - positions[0];
  return Vector3f::Zero();
}

Matrix3f BodyPart::BuildBasis(const Vector3f& primaryAxis, const Node* referenceNode) const
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
    const Vector3f fallback = std::abs(forward.z()) < 0.85f ? Vector3f::UnitZ() : Vector3f::UnitX();
    lateral                 = forward.cross(fallback);
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

bool BodyPart::IsValid() const
{
  if (!info.vector || info.vector->nodes.empty())
    return false;

  switch (shape) {
  case Shape::Point:
    return info.vector->nodes.size() >= 1 && IsNodeResolved(info.vector->nodes[0]);
  case Shape::Vector: {
    if (info.vector->nodes.size() < 2)
      return false;
    if (info.vector->nodes.size() >= 3) {
      std::size_t validCount = 0;
      for (const auto& node : info.vector->nodes) {
        if (IsNodeResolved(node))
          ++validCount;
      }
      return validCount >= 2;
    }
    return IsNodeResolved(info.vector->nodes[0]) && IsNodeResolved(info.vector->nodes[1]);
  }
  case Shape::Collider:
    return info.collider.has_value();
  default:
    return false;
  }
}

void BodyPart::UpdateVectorInfo()
{
  if (!info.vector)
    return;

  auto& vectorInfo     = info.vector.value();
  vectorInfo.start     = Point3f::Zero();
  vectorInfo.end       = Point3f::Zero();
  vectorInfo.direction = Vector3f::Zero();
  vectorInfo.length    = 0.f;

  for (auto& node : vectorInfo.nodes)
    UpdateNodePosition(node);

  if (!IsValid())
    return;

  switch (shape) {
  case Shape::Point:
    vectorInfo.start = vectorInfo.nodes[0].position;
    vectorInfo.end   = vectorInfo.start;
    break;
  case Shape::Vector: {
    if (vectorInfo.nodes.size() >= 3) {
      const auto validPositions = CollectValidNodePositions(vectorInfo);
      vectorInfo.start          = validPositions.front();
      vectorInfo.end            = validPositions.back();
      const Vector3f fit        = FitVector(vectorInfo);
      vectorInfo.length         = fit.norm();
      if (vectorInfo.length > 1e-6f)
        vectorInfo.direction = fit / vectorInfo.length;
    } else {
      vectorInfo.start = vectorInfo.nodes[0].position;
      vectorInfo.end   = vectorInfo.nodes[1].position;
      if (name == Name::Anus) {
        const auto ringPositions = CollectResolvedWorldPositions(vectorInfo.nodes[0]);
        const Vector3f anusAxis  = BuildAnusChannelAxis(actor, vectorInfo.start, ringPositions,
                                                        vectorInfo.end - vectorInfo.start);
        if (anusAxis.squaredNorm() > 1e-6f)
          vectorInfo.end = vectorInfo.start + anusAxis;
      }
      vectorInfo.direction = vectorInfo.end - vectorInfo.start;
      vectorInfo.length    = vectorInfo.direction.norm();
      if (vectorInfo.length > 1e-6f)
        vectorInfo.direction /= vectorInfo.length;
    }
    break;
  }
  default:
    break;
  }
}

void BodyPart::UpdateColliderInfo()
{
  info.collider.reset();
  if (!info.vector || !IsValid())
    return;

  const auto& vectorInfo = info.vector.value();
  const auto& spec       = GetColliderSpec(name);
  const bool isAperture  = name == Name::Vagina || name == Name::Anus;

  Collider collider;
  collider.center    = (vectorInfo.start + vectorInfo.end) * 0.5f;
  collider.direction = vectorInfo.direction;
  collider.length    = vectorInfo.length;

  const auto addCapsule = [&](const Point3f& start, const Point3f& end, float radius) {
    if (radius <= 0.f)
      return;
    collider.capsules.push_back(CapsuleCollider{start, end, radius});
  };
  const auto addBox = [&](const Point3f& center, const Matrix3f& basis,
                          const Vector3f& halfExtents) {
    if (halfExtents.maxCoeff() <= 0.f)
      return;
    collider.boxes.push_back(BoxCollider{center, basis, halfExtents});
  };

  if (isAperture && vectorInfo.length > 1e-6f) {
    const Matrix3f basis = BuildBasis(
        vectorInfo.direction, vectorInfo.nodes.empty() ? nullptr : &vectorInfo.nodes.front());
    if (HasEntranceBox(spec)) {
      addBox(vectorInfo.start + vectorInfo.direction * spec.entranceOffset, basis,
             spec.entranceHalfExtents);
    }
    if (spec.channelRadius > 0.f) {
      const float channelLength =
          std::clamp(vectorInfo.length * spec.channelLengthScale, 0.f, vectorInfo.length);
      addCapsule(vectorInfo.start, vectorInfo.start + vectorInfo.direction * channelLength,
                 spec.channelRadius);
    }
  }

  if (!isAperture && spec.capsuleRadius > 0.f) {
    const Point3f capsuleEnd = shape == Shape::Point ? vectorInfo.start : vectorInfo.end;
    addCapsule(vectorInfo.start, capsuleEnd, spec.capsuleRadius);
  }

  if (!collider.capsules.empty() || !collider.boxes.empty())
    info.collider = collider;
}

void BodyPart::UpdatePosition()
{
  UpdateVectorInfo();
  UpdateColliderInfo();

  if (!IsValid())
    logger::warn("Cannot update position for invalid BodyPart: {}", magic_enum::enum_name(name));
}

Vector3f BodyPart::FitVector(const Vector& vectorInfo) const
{
  const auto pts = CollectValidNodePositions(vectorInfo);
  if (pts.size() < 2) {
    logger::warn("FitVector: fewer than 2 valid nodes");
    return Vector3f::Zero();
  }
  if (pts.size() == 2)
    return pts[1] - pts[0];

  Vector3f centroid = Vector3f::Zero();
  for (const auto& point : pts)
    centroid += point;
  centroid /= static_cast<float>(pts.size());

  Eigen::Matrix3f cov = Eigen::Matrix3f::Zero();
  for (const auto& point : pts) {
    Vector3f diff = point - centroid;
    cov += diff * diff.transpose();
  }

  Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> solver(cov);
  Vector3f axis = solver.eigenvectors().col(2);
  if ((pts.back() - pts.front()).dot(axis) < 0.f)
    axis = -axis;

  std::vector<float> tVals;
  tVals.reserve(pts.size());
  for (const auto& point : pts)
    tVals.push_back((point - centroid).dot(axis));

  const auto [minIt, maxIt] = std::minmax_element(tVals.begin(), tVals.end());
  const Point3f startProj   = centroid + (*minIt) * axis;
  const Point3f endProj     = centroid + (*maxIt) * axis;
  return endProj - startProj;
}

}  // namespace Define
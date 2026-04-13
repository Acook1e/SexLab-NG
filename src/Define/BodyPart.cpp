#include "Define/BodyPart.h"

#include "magic_enum/magic_enum.hpp"

namespace Define
{
static std::unordered_map<Race::Type, std::array<PointName, 2>> mouthMap{
    // from central mouth to central head, which is slightly above mouth
    {Define::Race::Type::Human,
     {OffsetNodeName{"NPC Head [Head]", {0.054f, 0.067f, 0.608f}, {-0.061f, 5.487f, -2.266f}},
      "NPC Head [Head]"}},
};

static std::unordered_map<Race::Type, std::array<PointName, 2>> handLeftMap{
    {Define::Race::Type::Human,
     {CentralNodeName{"NPC L Hand [LHnd]", "NPC L Finger01 [LF01]", "NPC L Finger40 [LF40]"},
      "SHIELD"}},
};
static std::unordered_map<Race::Type, std::array<PointName, 2>> handRightMap{
    {Define::Race::Type::Human,
     {CentralNodeName{"NPC R Hand [RHnd]", "NPC R Finger01 [RF01]", "NPC R Finger40 [RF40]"},
      "WEAPON"}},
};

static std::unordered_map<Race::Type, std::array<PointName, 2>> footLeftMap{
    {Define::Race::Type::Human, {"NPC L Foot [Lft ]", "NPC L Toe0 [LToe]"}},
};
static std::unordered_map<Race::Type, std::array<PointName, 2>> footRightMap{
    {Define::Race::Type::Human, {"NPC R Foot [Rft ]", "NPC R Toe0 [RToe]"}},
};

// Per-race schlong bone names: [root, mid, tip]
// Used for Name::Penis construction
static std::unordered_map<Race::Type, std::array<PointName, 3>> schlongMap{
    {Define::Race::Type::Human,
     {"NPC Genitals01 [Gen01]", "NPC Genitals04 [Gen04]", "NPC Genitals06 [Gen06]"}},
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

static std::unordered_map<BodyPart::Name, std::vector<PointName>> humanMap{
    // from mid into head to neck
    {BodyPart::Name::Throat, {"NPC Head [Head]", "NPC Neck [Neck]"}},
    // from base breast to nipple
    {BodyPart::Name::BreastLeft, {"L Breast02", "L Breast03"}},
    {BodyPart::Name::BreastRight, {"R Breast02", "R Breast03"}},
    // normal vector of cleavage plane, facing outward
    {BodyPart::Name::Cleavage,
     {MidNodeName{"L Breast02", "R Breast02"}, MidNodeName{"L Breast03", "R Breast03"}}},
    // use point describes mid finger's tip, better than vector at finger's case
    {BodyPart::Name::FingerLeft, {"NPC L Finger22 [LF22]"}},
    {BodyPart::Name::FingerRight, {"NPC R Finger22 [RF22]"}},
    // from Spine1 to Belly — direction = body-forward normal (3D, follows actor lean)
    {BodyPart::Name::Belly, {"NPC Spine1 [Spn1]", "NPC Belly"}},
    // from mid of back thigh to mid of front thigh
    // not a pricise description of thigh, but can show how close with other part
    {BodyPart::Name::ThighLeft, {"NPC L RearThigh", "NPC L FrontThigh"}},
    {BodyPart::Name::ThighRight, {"NPC R RearThigh", "NPC R FrontThigh"}},
    // form vagina entry to mid of clitoral
    {BodyPart::Name::ThighCleft, {"VaginaB1", "Clitoral1"}},
    // single node for butt
    {BodyPart::Name::ButtLeft, {"NPC L Butt"}},
    {BodyPart::Name::ButtRight, {"NPC R Butt"}},
    // form mid of vagina and anus entry to mid of butt
    {BodyPart::Name::GlutealCleft, {"VaginaB1", MidNodeName{"NPC L Butt", "NPC R Butt"}}},
    // entry midpoint -> deep
    // direction assumption
    {BodyPart::Name::Vagina, {MidNodeName{"NPC L Pussy02", "NPC R Pussy02"}, "VaginaDeep1"}},
    // from entry to deep
    {BodyPart::Name::Anus, {"NPC LT Anus2", "NPC Anus Deep2"}},
};

static std::unordered_map<BodyPart::Name, BodyPart::Type> typeMap{
    {BodyPart::Name::Mouth, BodyPart::Type::Vector},
    {BodyPart::Name::Throat, BodyPart::Type::Vector},
    {BodyPart::Name::BreastLeft, BodyPart::Type::Vector},
    {BodyPart::Name::BreastRight, BodyPart::Type::Vector},
    {BodyPart::Name::Cleavage, BodyPart::Type::NormalVectorEnd},
    {BodyPart::Name::HandLeft, BodyPart::Type::Vector},
    {BodyPart::Name::HandRight, BodyPart::Type::Vector},
    {BodyPart::Name::FingerLeft, BodyPart::Type::Point},
    {BodyPart::Name::FingerRight, BodyPart::Type::Point},
    {BodyPart::Name::Belly, BodyPart::Type::NormalVectorEnd},
    {BodyPart::Name::ThighLeft, BodyPart::Type::Vector},
    {BodyPart::Name::ThighRight, BodyPart::Type::Vector},
    {BodyPart::Name::ThighCleft, BodyPart::Type::Vector},
    {BodyPart::Name::ButtLeft, BodyPart::Type::Point},
    {BodyPart::Name::ButtRight, BodyPart::Type::Point},
    {BodyPart::Name::GlutealCleft, BodyPart::Type::Vector},
    {BodyPart::Name::FootLeft, BodyPart::Type::Vector},
    {BodyPart::Name::FootRight, BodyPart::Type::Vector},
    {BodyPart::Name::Vagina, BodyPart::Type::Vector},
    {BodyPart::Name::Anus, BodyPart::Type::Vector},
    {BodyPart::Name::Penis, BodyPart::Type::FitVector},
};

Point3f operator~(const Point& point)
{
  if (std::holds_alternative<Node>(point)) {
    auto node      = std::get<Node>(point);
    const auto pos = node->world.translate;
    return {pos.x, pos.y, pos.z};
  } else if (std::holds_alternative<MidNode>(point)) {
    const auto& nodes = std::get<MidNode>(point);
    const auto lhs    = nodes[0]->world.translate;
    const auto rhs    = nodes[1]->world.translate;
    const auto mid    = (lhs + rhs) * 0.5f;
    return {mid.x, mid.y, mid.z};
  } else if (std::holds_alternative<CentralNode>(point)) {
    const auto& nodes = std::get<CentralNode>(point);
    const auto a      = nodes[0]->world.translate;
    const auto b      = nodes[1]->world.translate;
    const auto c      = nodes[2]->world.translate;
    const auto center = (a + b + c) / 3.0f;
    return {center.x, center.y, center.z};
  } else if (std::holds_alternative<OffsetNode>(point)) {
    const auto& off = std::get<OffsetNode>(point);
    if (!off.baseNode)
      return Point3f::Zero();

    // 1. 获取基准骨骼的世界位置和旋转矩阵
    const auto& baseWorldPos = off.baseNode->world.translate;
    const auto& baseWorldRot = off.baseNode->world.rotate;  // RE::NiMatrix3

    // 2. NiMatrix3 的 entry 内存布局与 Eigen 默认列主序不一致；这里转置后再使用，
    //    否则 mouth 这类 offset 点会在世界空间偏移 8-10 个单位。
    Matrix3f worldRotMat = Eigen::Map<const Matrix3f>(&baseWorldRot.entry[0][0]).transpose();

    // 3. 变换局部偏移：先应用局部附加旋转，再应用世界旋转
    Vector3f localOffset{off.localTrans.x(), off.localTrans.y(), off.localTrans.z()};
    Vector3f worldOffset = worldRotMat * (off.localRot * localOffset);

    // 4. 返回世界坐标
    return Point3f{baseWorldPos.x, baseWorldPos.y, baseWorldPos.z} + worldOffset;
  }
  return Point3f::Zero();
}

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

BodyPart::BodyPart(RE::Actor* a_actor, Race a_race, Name a_name)
    : actor(a_actor), name(a_name), type(typeMap[a_name])
{
  nodeNames.clear();
  nodes.clear();

  switch (a_name) {
  // ── Per-race lookups ──────────────────────────────────────────────────────
  case Name::Mouth:
    if (auto it = mouthMap.find(a_race.GetType()); it != mouthMap.end())
      for (auto& n : it->second)
        nodeNames.push_back(&n);
    break;

  case Name::HandLeft:
    if (auto it = handLeftMap.find(a_race.GetType()); it != handLeftMap.end())
      for (auto& n : it->second)
        nodeNames.push_back(&n);
    break;

  case Name::HandRight:
    if (auto it = handRightMap.find(a_race.GetType()); it != handRightMap.end())
      for (auto& n : it->second)
        nodeNames.push_back(&n);
    break;

  case Name::FootLeft:
    if (auto it = footLeftMap.find(a_race.GetType()); it != footLeftMap.end())
      for (auto& n : it->second)
        nodeNames.push_back(&n);
    break;

  case Name::FootRight:
    if (auto it = footRightMap.find(a_race.GetType()); it != footRightMap.end())
      for (auto& n : it->second)
        nodeNames.push_back(&n);
    break;

  case Name::Penis:
    if (auto it = schlongMap.find(a_race.GetType()); it != schlongMap.end())
      for (auto& n : it->second)
        nodeNames.push_back(&n);
    else
      logger::warn("No schlong mapping for race: {}", magic_enum::enum_name(a_race.GetType()));
    break;

  // ── Human-only fixed lookups ─────────────────────────────────────────────
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
      for (auto& n : it->second)
        nodeNames.push_back(&n);
    break;

  default:
    break;
  }

  if (nodeNames.empty())
    logger::warn("BodyPart not found for race: {} and name: {}",
                 magic_enum::enum_name(a_race.GetType()), magic_enum::enum_name(a_name));

  UpdateNodes();

  if (nodes.empty() || nodeNames.size() != nodes.size())
    logger::warn("No valid nodes found for BodyPart with name: {}", magic_enum::enum_name(a_name));

  UpdatePosition();
}

void BodyPart::UpdateNodes()
{
  nodes.clear();
  for (auto* variant : nodeNames) {
    if (std::holds_alternative<NodeName>(*variant)) {
      const auto& boneName = std::get<NodeName>(*variant);
      // Empty bone name is a deliberate placeholder (e.g. StormAtronach schlong root/tip)
      if (boneName.empty()) {
        nodes.push_back(static_cast<RE::NiPointer<RE::NiNode>>(nullptr));
        continue;
      }
      auto* obj = actor->GetNodeByName(boneName);
      if (!obj)
        logger::warn("Node not found: {}", boneName);
      auto ptr = obj ? obj->AsNode() : nullptr;
      nodes.push_back(RE::NiPointer<RE::NiNode>(ptr));

    } else if (std::holds_alternative<MidNodeName>(*variant)) {
      const auto& midName = std::get<MidNodeName>(*variant);
      std::array<RE::NiPointer<RE::NiNode>, 2> midNodes{};
      for (std::size_t i = 0; i < 2; ++i) {
        auto* obj = actor->GetNodeByName(midName[i]);
        if (!obj)
          logger::warn("Mid-node not found: {}", midName[i]);
        auto ptr    = obj ? obj->AsNode() : nullptr;
        midNodes[i] = RE::NiPointer<RE::NiNode>(ptr);
      }
      nodes.push_back(midNodes);
    } else if (std::holds_alternative<CentralNodeName>(*variant)) {
      const auto& centralName = std::get<CentralNodeName>(*variant);
      std::array<RE::NiPointer<RE::NiNode>, 3> centralNodes{};
      for (std::size_t i = 0; i < 3; ++i) {
        auto* obj = actor->GetNodeByName(centralName[i]);
        if (!obj)
          logger::warn("Central node not found: {}", centralName[i]);
        auto ptr        = obj ? obj->AsNode() : nullptr;
        centralNodes[i] = RE::NiPointer<RE::NiNode>(ptr);
      }
      nodes.push_back(centralNodes);
    } else if (std::holds_alternative<OffsetNodeName>(*variant)) {
      const auto& offsetNode = std::get<OffsetNodeName>(*variant);
      auto* baseObj          = actor->GetNodeByName(offsetNode.baseNode);
      if (!baseObj)
        logger::warn("Offset node base not found: {}", offsetNode.baseNode);
      Matrix3f localRot = (Eigen::AngleAxisf(offsetNode.eulerRot.x(), Vector3f::UnitX()) *
                           Eigen::AngleAxisf(offsetNode.eulerRot.y(), Vector3f::UnitY()) *
                           Eigen::AngleAxisf(offsetNode.eulerRot.z(), Vector3f::UnitZ()))
                              .toRotationMatrix();
      auto ptr          = baseObj ? baseObj->AsNode() : nullptr;
      nodes.push_back(OffsetNode{RE::NiPointer<RE::NiNode>(ptr), localRot, offsetNode.localTrans});
    }
  }
}

bool BodyPart::IsValid() const
{
  if (nodes.empty())
    return false;
  for (const auto& p : nodes) {
    if (std::holds_alternative<Node>(p)) {
      if (!std::get<Node>(p))
        return false;
    } else if (std::holds_alternative<MidNode>(p)) {
      const auto& m = std::get<MidNode>(p);
      if (!m[0] || !m[1])
        return false;
    } else if (std::holds_alternative<CentralNode>(p)) {
      const auto& c = std::get<CentralNode>(p);
      if (!c[0] || !c[1] || !c[2])
        return false;
    } else if (std::holds_alternative<OffsetNode>(p)) {
      if (!std::get<OffsetNode>(p).baseNode)
        return false;
    }
  }
  return true;
}

void BodyPart::UpdatePosition()
{
  start     = Point3f::Zero();
  end       = Point3f::Zero();
  direction = Vector3f::Zero();
  length    = 0.f;

  if (!IsValid()) {
    logger::warn("Cannot update position for invalid BodyPart: {}", magic_enum::enum_name(name));
    return;
  }

  switch (type) {

  // Point: only start matters; no direction
  case Type::Point:
    start = ~nodes[0];
    end   = start;
    break;

  // Vector / NormalVectorStart / NormalVectorEnd:
  //   direction = end - start
  case Type::Vector:
  case Type::NormalVectorStart:
  case Type::NormalVectorEnd:
    start     = ~nodes[0];
    end       = ~nodes[1];
    direction = end - start;
    length    = direction.norm();
    if (length > 1e-6f)  // normalize direction for consistent distance calculations; length is
                         // stored separately
      direction /= length;
    break;

  // FitVector: SVD principal axis through all valid points
  case Type::FitVector:
    start     = ~nodes[0];
    end       = ~nodes[nodes.size() - 1];
    direction = FitVector();  // returns a fit vector with fit length
    length    = direction.norm();
    if (length > 1e-6f)
      direction /= length;
    break;
  default:
    break;
  }
}

Vector3f BodyPart::FitVector()
{
  // Collect valid points (skip nullptr entries from StormAtronach-style gaps)
  std::vector<Vector3f> pts;
  for (const auto& node : nodes)
    if (auto p = ~node; p != Point3f::Zero())  // treat (0,0,0) as invalid to handle missing nodes
      pts.push_back(p);

  if (pts.size() < 2) {
    logger::warn("FitVector: fewer than 2 valid nodes");
    return Vector3f::Zero();
  }
  if (pts.size() == 2)
    return pts[1] - pts[0];

  // Centroid
  Vector3f centroid = Vector3f::Zero();
  for (const auto& p : pts)
    centroid += p;
  centroid /= static_cast<float>(pts.size());

  // Covariance matrix
  Eigen::Matrix3f cov = Eigen::Matrix3f::Zero();
  for (const auto& p : pts) {
    Vector3f diff = p - centroid;
    cov += diff * diff.transpose();
  }

  // Largest eigenvector = principal axis
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> solver(cov);
  Vector3f axis = solver.eigenvectors().col(2);

  // Orient from first valid point toward last valid point
  if ((pts.back() - pts.front()).dot(axis) < 0.f)
    axis = -axis;

  // Fit Length: project points onto axis, take range
  std::vector<float> tVals;
  for (const auto& p : pts) {
    float t = (p - centroid).dot(axis);
    tVals.push_back(t);
  }
  auto [minIt, maxIt] = std::minmax_element(tVals.begin(), tVals.end());
  Point3f startProj   = centroid + (*minIt) * axis;
  Point3f endProj     = centroid + (*maxIt) * axis;
  return endProj - startProj;
}

// ─── Angle ───────────────────────────────────────────────────────────────────
// Returns the absolute angle in [0, 180] degrees between the two direction vectors.
// < 90 → roughly aligned; > 90 → roughly anti-aligned; = 90 → perpendicular.
// Returns 0 if either part is Point type or has zero-length direction.

float BodyPart::Angle(const BodyPart& other) const
{
  if (type == Type::Point || other.type == Type::Point)
    return 0.f;
  if (length < 1e-6f || other.length < 1e-6f)
    return 0.f;

  const float dot = std::clamp(direction.dot(other.direction), -1.f, 1.f);
  return std::acos(dot) * (180.f / std::numbers::pi_v<float>);
}

float BodyPart::Distance(const BodyPart& other) const
{
  // ── Point-to-point ────────────────────────────────────────────────────────
  if (type == Type::Point && other.type == Type::Point)
    return (start - other.start).norm();

  // ── Helper: distance from a point to a parametric segment [s0, s0 + dN*len]
  //   dN must be a unit vector, len is the segment length.
  auto pointToSeg = [](const Point3f& p, const Point3f& s0, const Vector3f& dN,
                       float len) -> float {
    if (len < 1e-6f)
      return (p - s0).norm();
    const float t = std::clamp((p - s0).dot(dN), 0.f, len);
    return (p - (s0 + dN * t)).norm();
  };

  // Determine unit directions and lengths for each part
  // Point types have zero length; treat them as degenerate segments.
  const bool aIsVec = (type != Type::Point && length > 1e-6f);
  const bool bIsVec = (other.type != Type::Point && other.length > 1e-6f);

  const Vector3f dA = aIsVec ? direction : Vector3f::Zero();  // already unit
  const Vector3f dB = bIsVec ? other.direction : Vector3f::Zero();
  const float lA    = aIsVec ? length : 0.f;
  const float lB    = bIsVec ? other.length : 0.f;

  // ── Point-to-segment ─────────────────────────────────────────────────────
  if (!aIsVec)
    return pointToSeg(start, other.start, dB, lB);
  if (!bIsVec)
    return pointToSeg(other.start, start, dA, lA);

  // ── Segment-to-segment ───────────────────────────────────────────────────
  // Parametrize: P(s) = start + dA*s,  s ∈ [0, lA]
  //              Q(t) = other.start + dB*t,  t ∈ [0, lB]
  const Vector3f w  = other.start - start;
  const float b     = dA.dot(dB);
  const float d     = dA.dot(w);
  const float e     = dB.dot(w);
  const float denom = 1.f - b * b;  // = 1 - cos²θ = sin²θ, always ∈ [0,1]

  float s, t;

  if (denom < 1e-6f) {
    // Lines nearly parallel: fix s=0, project
    s = 0.f;
    t = std::clamp(e / lB, 0.f, 1.f) * lB;  // t ∈ [0, lB]
    // Take minimum over all four endpoint combinations for robustness
    float dist = pointToSeg(start, other.start, dB, lB);
    dist       = min(dist, pointToSeg(other.start, start, dA, lA));
    dist       = min(dist, pointToSeg(start + dA * lA, other.start, dB, lB));
    dist       = min(dist, pointToSeg(other.start + dB * lB, start, dA, lA));
    return dist;
  }

  // Unconstrained closest approach
  s = (b * e - d) / denom;
  t = (e - b * d) / denom;

  // Clamp s to [0, lA], then re-derive t; clamp t to [0, lB], then re-derive s
  if (s < 0.f) {
    s = 0.f;
    t = std::clamp(e, 0.f, lB);
  } else if (s > lA) {
    s = lA;
    t = std::clamp(e + b * lA, 0.f, lB);
  }

  if (t < 0.f) {
    t = 0.f;
    s = std::clamp(-d, 0.f, lA);
  } else if (t > lB) {
    t = lB;
    s = std::clamp(b * lB - d, 0.f, lA);
  }

  const Point3f p = start + dA * s;
  const Point3f q = other.start + dB * t;
  return (p - q).norm();
}

// ─── IsInFront ───────────────────────────────────────────────────────────────
// Returns true if the point lies on the side that direction points toward.
// For Belly (NormalVectorEnd): start=Spine1, end=Belly surface,
//   direction = Spine1→Belly = body-forward; IsInFront(p) means p is in front of the actor.
// For any Vector part the semantics follow the part's own axis direction.
// Point-type parts always return false (no directional axis).

bool BodyPart::IsInFront(const Point3f& p) const
{
  if (type == Type::Point || length < 1e-6f)
    return false;
  // Positive signed projection along direction from start
  return (p - start).dot(direction) > 0.f;
}

// ─── IsHorizontal ────────────────────────────────────────────────────────────
// Returns true if the direction vector is within toleranceDeg of the XY plane.
// Useful for Naveljob (penis must be roughly horizontal, not pointing down into vagina).

bool BodyPart::IsHorizontal(float toleranceDeg) const
{
  if (type == Type::Point || length < 1e-6f)
    return false;
  const float sinTol = std::sin(toleranceDeg * std::numbers::pi_v<float> / 180.f);
  return std::abs(direction.z()) <= sinTol;
}

}  // namespace Define
#include "Define/BodyPart.h"

#include "magic_enum/magic_enum.hpp"

namespace Define
{
static std::unordered_map<Race::Type, PointName> mouthMap{
    {Define::Race::Type::Human, "NPC Head [Head]"},
};

static std::unordered_map<Race::Type, PointName> handLeftMap{
    {Define::Race::Type::Human, "SHIELD"},
};
static std::unordered_map<Race::Type, PointName> handRightMap{
    {Define::Race::Type::Human, "WEAPON"},
};

static std::unordered_map<Race::Type, std::array<PointName, 2>> footLeftMap{
    {Define::Race::Type::Human, {"NPC L Foot [Lft ]", "NPC L Toe0 [LToe]"}},
};
static std::unordered_map<Race::Type, std::array<PointName, 2>> footRightMap{
    {Define::Race::Type::Human, {"NPC R Foot [Rgt ]", "NPC R Toe0 [RToe]"}},
};

static std::unordered_map<Race::Type, std::array<PointName, 3>> schlongMap{
    {Define::Race::Type::Human, {"NPC Genitals01 [Gen01]", "NPC Genitals04 [Gen04]", "NPC Genitals06 [Gen06]"}},
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
    {Define::Race::Type::DwarvenCenturion, {"DwarvenBattery01", "DwarvenBattery02", "DwarvenInjectorLid"}},
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
    {Define::Race::Type::StormAtronach,
     {
         "",
         "Torso Rock 2",
         "",
     }},
    {Define::Race::Type::Troll, {"TD 3", "TD 5", "TD 7"}},
    {Define::Race::Type::VampireLord, {"VLDick03", "VLDick05", "VLDick06"}},
    {Define::Race::Type::Werebear, {"WWD 5", "WWD 7", "WWD 9"}},
    {Define::Race::Type::Werewolf, {"WWD 5", "WWD 7", "WWD 9"}},
    {Define::Race::Type::Wolf, {"CDPenis 3", "CDPenis 5", "CDPenis 7"}},
};

static std::unordered_map<BodyPart::Name, std::vector<PointName>> humanMap{
    // from base breast to nipple
    {BodyPart::Name::BreastLeft, {"L Breast02", "L Breast03"}},
    {BodyPart::Name::BreastRight, {"R Breast02", "R Breast03"}},
    // from root of middle finger to its tip
    {BodyPart::Name::FingerLeft, {"NPC L Finger20 [LF20]", "NPC L Finger22 [LF22]"}},
    {BodyPart::Name::FingerRight, {"NPC R Finger20 [RF20]", "NPC R Finger22 [RF22]"}},
    // from Spine1 to Belly, almost the normal vector of belly
    {BodyPart::Name::Belly, {"NPC Spine1 [Spn1]", "NPC Belly"}},
    // from mid of back thigh to mid of front thigh
    {BodyPart::Name::ThighLeft, {"NPC L RearThigh", "NPC L FrontThigh"}},
    {BodyPart::Name::ThighRight, {"NPC R RearThigh", "NPC R FrontThigh"}},
    // the only node of butt
    {BodyPart::Name::ButtLeft, {"NPC L Butt"}},
    {BodyPart::Name::ButtRight, {"NPC R Butt"}},
    // from entry to deep then pevis
    {BodyPart::Name::Vagina, {MidNodeName{"NPC L Pussy02", "NPC R Pussy02"}, "VaginaDeep1", "NPC Pelvis [Pelv]"}},
    // from entry to deep
    {BodyPart::Name::Anus, {"NPC LT Anus2", "NPC Anus Deep2"}},
};

static std::unordered_map<BodyPart::Name, BodyPart::Type> typeMap{
    {BodyPart::Name::Mouth, BodyPart::Type::Point},        {BodyPart::Name::BreastLeft, BodyPart::Type::Vector},
    {BodyPart::Name::BreastRight, BodyPart::Type::Vector}, {BodyPart::Name::HandLeft, BodyPart::Type::Point},
    {BodyPart::Name::HandRight, BodyPart::Type::Point},    {BodyPart::Name::FingerLeft, BodyPart::Type::Vector},
    {BodyPart::Name::FingerRight, BodyPart::Type::Vector}, {BodyPart::Name::Belly, BodyPart::Type::NormalVectorEnd},
    {BodyPart::Name::ThighLeft, BodyPart::Type::Vector},   {BodyPart::Name::ThighRight, BodyPart::Type::Vector},
    {BodyPart::Name::ButtLeft, BodyPart::Type::Point},     {BodyPart::Name::ButtRight, BodyPart::Type::Point},
    {BodyPart::Name::FootLeft, BodyPart::Type::Vector},    {BodyPart::Name::FootRight, BodyPart::Type::Vector},
    {BodyPart::Name::Vagina, BodyPart::Type::FitVector},   {BodyPart::Name::Anus, BodyPart::Type::Vector},
    {BodyPart::Name::Penis, BodyPart::Type::FitVector},
};

// Calculate Point or Mid Point
Point3f operator~(Point point)
{
  if (std::holds_alternative<Node>(point)) {
    auto node = std::get<Node>(point);
    auto pos  = node->world.translate;
    return {pos.x, pos.y, pos.z};
  } else if (std::holds_alternative<MidNode>(point)) {
    auto nodes  = std::get<MidNode>(point);
    auto lhsPos = nodes[0]->world.translate;
    auto rhsPos = nodes[1]->world.translate;
    auto mid    = (lhsPos + rhsPos) * 0.5f;
    return {mid.x, mid.y, mid.z};
  }
  return {0.0f, 0.0f, 0.0f};
}

bool BodyPart::HasBodyPart(Gender gender, Race race, Name name)
{
  const bool isFemaleOrFuta =
      gender.Get() == Define::Gender::Type::Female || gender.Get() == Define::Gender::Type::Futa;

  switch (name) {
  case BodyPart::Name::Mouth:
  case BodyPart::Name::HandLeft:
  case BodyPart::Name::HandRight:
  case BodyPart::Name::FootLeft:
  case BodyPart::Name::FootRight:
    return true;
  case BodyPart::Name::BreastLeft:
  case BodyPart::Name::BreastRight:
  case BodyPart::Name::Vagina:
    return isFemaleOrFuta;
  case BodyPart::Name::Belly:
  case BodyPart::Name::ThighLeft:
  case BodyPart::Name::ThighRight:
  case BodyPart::Name::ButtLeft:
  case BodyPart::Name::ButtRight:
  case BodyPart::Name::Anus:
    return race.GetType() == Define::Race::Type::Human;
  case BodyPart::Name::Penis:
    return gender.HasPenis();
  default:
    return false;
  }
}

BodyPart::BodyPart(RE::Actor* actor, Race race, Name name) : name(name), type(typeMap[name])
{
  nodeNames.clear();
  nodes.clear();

  switch (name) {
  // All Creature
  case BodyPart::Name::Mouth:
    if (auto it = mouthMap.find(race.GetType()); it != mouthMap.end())
      nodeNames.push_back(&it->second);
    break;
  case BodyPart::Name::HandLeft:
    if (auto it = handLeftMap.find(race.GetType()); it != handLeftMap.end())
      nodeNames.push_back(&it->second);
    break;
  case BodyPart::Name::HandRight:
    if (auto it = handRightMap.find(race.GetType()); it != handRightMap.end())
      nodeNames.push_back(&it->second);
    break;
  case BodyPart::Name::FootLeft:
    if (auto it = footLeftMap.find(race.GetType()); it != footLeftMap.end())
      for (auto& name : it->second)
        nodeNames.push_back(&name);
    break;
  case BodyPart::Name::FootRight:
    if (auto it = footRightMap.find(race.GetType()); it != footRightMap.end())
      for (auto& name : it->second)
        nodeNames.push_back(&name);
    break;

    // Human only
  case BodyPart::Name::BreastLeft:
  case BodyPart::Name::BreastRight:
  case BodyPart::Name::FingerLeft:
  case BodyPart::Name::FingerRight:
  case BodyPart::Name::Belly:
  case BodyPart::Name::ThighLeft:
  case BodyPart::Name::ThighRight:
  case BodyPart::Name::ButtLeft:
  case BodyPart::Name::ButtRight:
  case BodyPart::Name::Vagina:
  case BodyPart::Name::Anus:
  case BodyPart::Name::Penis:
    if (auto it = humanMap.find(name); it != humanMap.end())
      for (auto& nodeName : it->second)
        nodeNames.push_back(&nodeName);
    break;
  default:
    break;
  }

  if (nodeNames.empty())
    logger::warn("BodyPart not found for race: {} and name: {}", magic_enum::enum_name(race.GetType()),
                 magic_enum::enum_name(name));
  UpdateNodes();

  if (nodes.empty() || nodeNames.size() != nodes.size())
    logger::warn("No valid nodes found for BodyPart with name: {}", magic_enum::enum_name(name));
  UpdatePosition();
}

void BodyPart::UpdateNodes()
{
  for (auto& variant : nodeNames) {
    if (std::holds_alternative<NodeName>(*variant)) {
      if (auto node = actor->GetNodeByName(std::get<NodeName>(*variant)); node)
        nodes.push_back(node->AsNode());
      else {
        logger::warn("Node not found for name: {}", std::get<NodeName>(*variant));
        return;
      }
    } else if (std::holds_alternative<MidNodeName>(*variant)) {
      const auto& midNodeName = std::get<MidNodeName>(*variant);
      std::array<RE::NiNode*, 2> midNodes{};
      for (std::size_t i = 0; i < 2; ++i) {
        if (auto node = actor->GetNodeByName(midNodeName[i]); node)
          midNodes[i] = node->AsNode();
        else {
          logger::warn("Node not found for name: {}", midNodeName[i]);
          return;
        }
      }
      nodes.push_back(midNodes);
    }
  }
}

void BodyPart::UpdatePosition()
{
  switch (type) {
  case BodyPart::Type::Point:
    start = ~nodes[0];
    break;
  case BodyPart::Type::Vector:
  case BodyPart::Type::NormalVectorStart:
  case BodyPart::Type::NormalVectorEnd:
    start     = ~nodes[0];
    end       = ~nodes[1];
    direction = end - start;
    length    = direction.norm();
    break;
  case BodyPart::Type::FitVector:
    start     = ~nodes[0];
    end       = ~nodes[2];
    direction = FitVector();
    length    = direction.norm();
    break;
  default:
    break;
  }
}

Vector3f BodyPart::FitVector()
{
  if (nodes.size() < 3) {
    logger::warn("FitVector requires at least 3 nodes, but got {}", nodes.size());
    return Vector3f::Zero();
  }

  Vector3f p0 = ~nodes[0];
  Vector3f p1 = ~nodes[1];
  Vector3f p2 = ~nodes[2];

  Vector3f centroid = (p0 + p1 + p2) / 3.0f;

  Eigen::Matrix3f cov            = Eigen::Matrix3f::Zero();
  std::array<Vector3f, 3> points = {p0, p1, p2};
  for (const auto& p : points) {
    Vector3f diff = p - centroid;
    cov += diff * diff.transpose();
  }

  Eigen::SelfAdjointEigenSolver<Eigen::Matrix3f> solver(cov);
  Vector3f direction = solver.eigenvectors().col(2);  // Eigenvector with the largest eigenvalue

  if ((p2 - p0).dot(direction) < 0)
    direction = -direction;

  return direction;
}

float BodyPart::Angle(const BodyPart& other)
{
  if (type == Type::Point || other.type == Type::Point)
    return 0.0f;

  if (length < 1e-6f || other.length < 1e-6f)
    return 0.0f;

  float cosTheta = direction.normalized().dot(other.direction.normalized());
  return std::acos(std::clamp(cosTheta, -1.0f, 1.0f));
}

float BodyPart::Distance(const BodyPart& other)
{
  if (type == Type::Point && other.type == Type::Point)
    return (start - other.start).norm();

  auto PointToVector = [](const Point3f& point, const Vector3f vector, const Point3f& vecStart,
                          const Point3f& vecEnd) -> float {
    float t = (point - vecStart).dot(vector);
    if (t <= 0.0f)
      return (point - vecStart).norm();
    auto len = vector.squaredNorm();
    if (len < 1e-6f)
      return (point - vecStart).norm();
    else if (t >= len)
      return (point - vecEnd).norm();
    return (point - (vecStart + (t / len) * vector)).norm();
  };

  if (type == Type::Point)
    return PointToVector(start, other.direction, other.start, other.end);

  if (other.type == Type::Point)
    return PointToVector(other.start, direction, start, end);

  // For two vectors, we can calculate the distance between them as the distance between their closest points

  auto w = other.start - start;

  auto a = direction.dot(direction);  // always >= 0
  auto b = direction.dot(other.direction);
  auto c = other.direction.dot(other.direction);  // always >= 0
  auto d = direction.dot(w);
  auto e = other.direction.dot(w);

  auto denom = a * c - b * b;  // always >= 0

  float s, t;
  if (denom < 1e-6f) {  // Lines are almost parallel
    s          = 0.0f;
    t          = std::clamp(e / c, 0.0f, 1.0f);
    float dist = PointToVector(start, other.direction, other.start, other.end);
    dist       = min(dist, PointToVector(other.start, direction, start, end));
    dist       = min(dist, PointToVector(end, other.direction, other.start, other.end));
    dist       = min(dist, PointToVector(other.end, direction, start, end));
    return dist;
  }

  s = (b * e - c * d) / denom;
  t = (a * e - b * d) / denom;

  if (s < 0.0f) {
    s = 0.0f;
    t = std::clamp(e / c, 0.0f, 1.0f);
  } else if (s > 1.0f) {
    s = 1.0f;
    t = std::clamp((b + e) / c, 0.0f, 1.0f);
  }

  if (t < 0.0f) {
    t = 0.0f;
    s = std::clamp(-d / a, 0.0f, 1.0f);
  } else if (t > 1.0f) {
    t = 1.0f;
    s = std::clamp((b - d) / a, 0.0f, 1.0f);
  }

  auto p = start + s * direction;
  auto q = other.start + t * other.direction;
  return (p - q).norm();
}

}  // namespace Define
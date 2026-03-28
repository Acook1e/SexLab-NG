#include "Define/BodyPart.h"

namespace Define
{

using NodeName    = std::string_view;
using MidNodeName = std::array<std::string_view, 2>;

using PointName     = std::variant<NodeName, MidNodeName>;
using VectorName    = std::array<PointName, 2>;  // include normal vector
using FitVectorName = std::array<PointName, 3>;  // include normal vector

static std::unordered_map<Race::Type, PointName> mouthMap{
    {Define::Race::Type::Human, "NPC Head [Head]"},
};

static std::unordered_map<Race::Type, PointName> handLeftMap{
    {Define::Race::Type::Human, "SHIELD"},
};
static std::unordered_map<Race::Type, PointName> handRightMap{
    {Define::Race::Type::Human, "WEAPON"},
};

static std::unordered_map<Race::Type, VectorName> footLeftMap{
    {Define::Race::Type::Human, {"NPC L Foot [Lft ]", "NPC L Toe0 [LToe]"}},
};
static std::unordered_map<Race::Type, VectorName> footRightMap{
    {Define::Race::Type::Human, {"NPC R Foot [Rgt ]", "NPC R Toe0 [RToe]"}},
};

static std::unordered_map<Race::Type, FitVectorName> schlongMap{
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

static std::unordered_map<BodyPart::Name, std::variant<PointName, VectorName, FitVectorName>> humanMap{
    // from base breast to nipple
    {BodyPart::Name::BreastLeft, VectorName{"L Breast02", "L Breast03"}},
    {BodyPart::Name::BreastRight, VectorName{"R Breast02", "R Breast03"}},
    // from root of middle finger to its tip
    {BodyPart::Name::FingerLeft, VectorName{"NPC L Finger20 [LF20]", "NPC L Finger22 [LF22]"}},
    {BodyPart::Name::FingerRight, VectorName{"NPC R Finger20 [RF20]", "NPC R Finger22 [RF22]"}},
    // from Spine1 to Belly, almost the normal vector of belly
    {BodyPart::Name::Belly, VectorName{"NPC Spine1 [Spn1]", "NPC Belly"}},
    // from mid of back thigh to mid of front thigh
    {BodyPart::Name::ThighLeft, VectorName{"NPC L RearThigh", "NPC L FrontThigh"}},
    {BodyPart::Name::ThighRight, VectorName{"NPC R RearThigh", "NPC R FrontThigh"}},
    // the only node of butt
    {BodyPart::Name::ButtLeft, PointName{"NPC L Butt"}},
    {BodyPart::Name::ButtRight, PointName{"NPC R Butt"}},
    // from entry to deep then pevis
    {BodyPart::Name::Vagina,
     FitVectorName{MidNodeName{"NPC L Pussy02", "NPC R Pussy02"}, "VaginaDeep1", "NPC Pelvis [Pelv]"}},
    // from entry to deep
    {BodyPart::Name::Anus, VectorName{"NPC LT Anus2", "NPC Anus Deep2"}},
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

/*
// Calculate Mid Point
Point3f operator!(std::array<RE::NiNode*, 2> nodes)
{
    auto lhsPos = nodes[0]->world.translate;
    auto rhsPos = nodes[1]->world.translate;
    auto mid    = (lhsPos + rhsPos) * 0.5f;
    return {mid.x, mid.y, mid.z};
}

// Calculate Point
Point3f operator!(RE::NiNode node)
{
    auto pos = node.world.translate;
    return {pos.x, pos.y, pos.z};
}
*/

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

}  // namespace Define
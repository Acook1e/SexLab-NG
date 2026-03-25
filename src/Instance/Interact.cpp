#include "Instance/Interact.h"

#include "Define/Race.h"

namespace Instance
{
// HEAD
constexpr static std::string_view HEAD = "NPC Head [Head]";

// HAND
constexpr static std::string_view HANDLEFT     = "NPC L Hand [LHnd]";  // actually wrist
constexpr static std::string_view HANDLEFTREF  = "SHIELD";             // actually palm
constexpr static std::string_view HANDRIGHT    = "NPC R Hand [RHnd]";
constexpr static std::string_view HANDRIGHTREF = "WEAPON";

// FINGER
constexpr static std::string_view FINGERLEFT  = "NPC L Finger20 [LF20]";  // root of middle finger
constexpr static std::string_view FINGERRIGHT = "NPC R Finger20 [RF20]";
constexpr static std::string_view THUMBLEFT   = "NPC L Finger02 [LF02]";  // middle joint of thumb, not the tip
constexpr static std::string_view THUMBRIGHT  = "NPC R Finger02 [RF02]";

// VAGINA
constexpr static std::string_view VAGINALEFT  = "NPC L Pussy02";  // actually right edge of left groin
constexpr static std::string_view VAGINARIGHT = "NPC R Pussy02";  // actually left edge of right groin
constexpr static std::string_view CLITORIS    = "Clitoral1";
constexpr static std::string_view PELVIS      = "NPC Pelvis [Pelv]";  // actually middle in vagina
constexpr static std::string_view VAGINADEEP  = "VaginaDeep1";

// ANAL
constexpr static std::string_view ANAL      = "NPC RT Anus2";
constexpr static std::string_view ANALDEEP  = "NPC Anus Deep2";
constexpr static std::string_view SPINEDOWN = "NPC Spine [Spn0]";  // at the top of ANALDEEP

// FOOT
constexpr static std::string_view FOOTLEFT  = "NPC L Foot [Lft ]";  // actually ankle
constexpr static std::string_view FOOTRIGHT = "NPC R Foot [Rft ]";
constexpr static std::string_view TOELEFT   = "NPC L Toe0 [LToe]";  // back root of middle toe, not the tip
constexpr static std::string_view TOERIGHT  = "NPC R Toe0 [RToe]";

// AnimObject
constexpr static std::string_view ANIMOBJECTA = "ANIMOBJECTA";
constexpr static std::string_view ANIMOBJECTB = "ANIMOBJECTB";
constexpr static std::string_view ANIMOBJECTR = "ANIMOBJECTR";
constexpr static std::string_view ANIMOBJECTL = "ANIMOBJECTL";

struct SchlongNode
{
  enum class Calculate
  {
    Line = 0,
    Point
  };
  std::string_view root;
  std::string_view mid;
  std::string_view head;
  Calculate calculate = Calculate::Line;
};

std::unordered_map<Define::Race::Type, SchlongNode> schlongNodes{
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
    {Define::Race::Type::StormAtronach, {"Torso Rock 2", "", "", SchlongNode::Calculate::Point}},
    {Define::Race::Type::Troll, {"TD 3", "TD 5", "TD 7"}},
    {Define::Race::Type::VampireLord, {"VLDick03", "VLDick05", "VLDick06"}},
    {Define::Race::Type::Werebear, {"WWD 5", "WWD 7", "WWD 9"}},
    {Define::Race::Type::Werewolf, {"WWD 5", "WWD 7", "WWD 9"}},
    {Define::Race::Type::Wolf, {"CDPenis 3", "CDPenis 5", "CDPenis 7"}},
};

}  // namespace Instance

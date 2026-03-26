#include "Instance/Interact.h"

#include "magic_enum/magic_enum.hpp"
#include "magic_enum/magic_enum_flags.hpp"

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

// BREAST
constexpr static std::string_view BREASTLEFT  = "L Breast02";  // actually middle into of left breast
constexpr static std::string_view BREASTRIGHT = "R Breast02";  // actually middle into of right breast
constexpr static std::string_view NIPPLELEFT  = "L Breast03";  // actually in front of left nipple
constexpr static std::string_view NIPPLERIGHT = "R Breast03";  // actually in front of right nipple

// BELLY
constexpr static std::string_view BELLY =
    "NPC Belly";  // actually middle into of belly, but it's the only node in the belly area

// THIGH
constexpr static std::string_view FRONTTHIGHLEFT  = "NPC L FrontThigh";  // actually front of mid thigh
constexpr static std::string_view FRONTTHIGHRIGHT = "NPC R FrontThigh";
constexpr static std::string_view REARTHIGHLEFT   = "NPC L RearThigh";  // actually back of mid thigh
constexpr static std::string_view REARTHIGHRIGHT  = "NPC R RearThigh";

// BUTT
constexpr static std::string_view BUTTLEFT  = "NPC L Butt";
constexpr static std::string_view BUTTRIGHT = "NPC R Butt";

// FOOT
constexpr static std::string_view FOOTLEFT  = "NPC L Foot [Lft ]";  // actually ankle
constexpr static std::string_view FOOTRIGHT = "NPC R Foot [Rft ]";
constexpr static std::string_view TOELEFT   = "NPC L Toe0 [LToe]";  // back root of middle toe, not the tip
constexpr static std::string_view TOERIGHT  = "NPC R Toe0 [RToe]";

// VAGINA
constexpr static std::string_view VAGINALEFT  = "NPC L Pussy02";  // actually right edge of left groin
constexpr static std::string_view VAGINARIGHT = "NPC R Pussy02";  // actually left edge of right groin
constexpr static std::string_view CLITORIS    = "Clitoral1";
constexpr static std::string_view PELVIS      = "NPC Pelvis [Pelv]";  // actually middle in vagina
constexpr static std::string_view VAGINADEEP  = "VaginaDeep1";

// ANAL
constexpr static std::string_view ANUS      = "NPC RT Anus2";
constexpr static std::string_view ANUSDEEP  = "NPC Anus Deep2";
constexpr static std::string_view SPINEDOWN = "NPC Spine [Spn0]";  // at the top of ANUSDEEP

// AnimObject
constexpr static std::string_view ANIMOBJECTA = "ANIMOBJECTA";
constexpr static std::string_view ANIMOBJECTB = "ANIMOBJECTB";
constexpr static std::string_view ANIMOBJECTR = "ANIMOBJECTR";
constexpr static std::string_view ANIMOBJECTL = "ANIMOBJECTL";

static const std::unordered_map<Interact::SkeletonNode, std::string_view> skeletonNodeMap{
    {Interact::SkeletonNode::Head, HEAD},
    {Interact::SkeletonNode::HandLeft, HANDLEFT},
    {Interact::SkeletonNode::HandLeftRef, HANDLEFTREF},
    {Interact::SkeletonNode::HandRight, HANDRIGHT},
    {Interact::SkeletonNode::HandRightRef, HANDRIGHTREF},
    {Interact::SkeletonNode::FingerLeft, FINGERLEFT},
    {Interact::SkeletonNode::FingerRight, FINGERRIGHT},
    {Interact::SkeletonNode::ThumbLeft, THUMBLEFT},
    {Interact::SkeletonNode::ThumbRight, THUMBRIGHT},
    {Interact::SkeletonNode::BreastLeft, BREASTLEFT},
    {Interact::SkeletonNode::BreastRight, BREASTRIGHT},
    {Interact::SkeletonNode::NippleLeft, NIPPLELEFT},
    {Interact::SkeletonNode::NippleRight, NIPPLERIGHT},
    {Interact::SkeletonNode::Belly, BELLY},
    {Interact::SkeletonNode::FrontThighLeft, FRONTTHIGHLEFT},
    {Interact::SkeletonNode::FrontThighRight, FRONTTHIGHRIGHT},
    {Interact::SkeletonNode::RearThighLeft, REARTHIGHLEFT},
    {Interact::SkeletonNode::RearThighRight, REARTHIGHRIGHT},
    {Interact::SkeletonNode::ButtLeft, BUTTLEFT},
    {Interact::SkeletonNode::ButtRight, BUTTRIGHT},
    {Interact::SkeletonNode::FootLeft, FOOTLEFT},
    {Interact::SkeletonNode::FootRight, FOOTRIGHT},
    {Interact::SkeletonNode::ToeLeft, TOELEFT},
    {Interact::SkeletonNode::ToeRight, TOERIGHT},
    {Interact::SkeletonNode::VaginaLeft, VAGINALEFT},
    {Interact::SkeletonNode::VaginaRight, VAGINARIGHT},
    {Interact::SkeletonNode::Clitoris, CLITORIS},
    {Interact::SkeletonNode::Pelvis, PELVIS},
    {Interact::SkeletonNode::VaginaDeep, VAGINADEEP},
    {Interact::SkeletonNode::Anus, ANUS},
    {Interact::SkeletonNode::AnusDeep, ANUSDEEP},
    {Interact::SkeletonNode::SpineDown, SPINEDOWN},
    {Interact::SkeletonNode::AnimObjA, ANIMOBJECTA},
    {Interact::SkeletonNode::AnimObjB, ANIMOBJECTB},
    {Interact::SkeletonNode::AnimObjR, ANIMOBJECTR},
    {Interact::SkeletonNode::AnimObjL, ANIMOBJECTL},
};

struct SchlongNode
{
  std::string_view root;
  std::string_view mid;
  std::string_view head;
  bool isPoint = false;
};

static const std::unordered_map<Define::Race::Type, SchlongNode> schlongNodes{
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
    // StormAtronach: isPoint=true, only mid node exists; root/head intentionally empty
    {Define::Race::Type::StormAtronach, {"", "Torso Rock 2", "", true}},
    {Define::Race::Type::Troll, {"TD 3", "TD 5", "TD 7"}},
    {Define::Race::Type::VampireLord, {"VLDick03", "VLDick05", "VLDick06"}},
    {Define::Race::Type::Werebear, {"WWD 5", "WWD 7", "WWD 9"}},
    {Define::Race::Type::Werewolf, {"WWD 5", "WWD 7", "WWD 9"}},
    {Define::Race::Type::Wolf, {"CDPenis 3", "CDPenis 5", "CDPenis 7"}},
};

static const std::unordered_map<Interact::SkeletonNode, Interact::BodyPart> nodeToPartMap{
    {Interact::SkeletonNode::Head, Interact::BodyPart::Mouth},
    {Interact::SkeletonNode::HandLeft, Interact::BodyPart::HandLeft},
    {Interact::SkeletonNode::HandLeftRef, Interact::BodyPart::HandLeft},
    {Interact::SkeletonNode::FingerLeft, Interact::BodyPart::HandLeft},
    {Interact::SkeletonNode::ThumbLeft, Interact::BodyPart::HandLeft},
    {Interact::SkeletonNode::HandRight, Interact::BodyPart::HandRight},
    {Interact::SkeletonNode::HandRightRef, Interact::BodyPart::HandRight},
    {Interact::SkeletonNode::FingerRight, Interact::BodyPart::HandRight},
    {Interact::SkeletonNode::ThumbRight, Interact::BodyPart::HandRight},
    {Interact::SkeletonNode::BreastLeft, Interact::BodyPart::BreastLeft},
    {Interact::SkeletonNode::NippleLeft, Interact::BodyPart::BreastLeft},
    {Interact::SkeletonNode::BreastRight, Interact::BodyPart::BreastRight},
    {Interact::SkeletonNode::NippleRight, Interact::BodyPart::BreastRight},
    {Interact::SkeletonNode::Belly, Interact::BodyPart::Belly},
    {Interact::SkeletonNode::FrontThighLeft, Interact::BodyPart::ThighLeft},
    {Interact::SkeletonNode::RearThighLeft, Interact::BodyPart::ThighLeft},
    {Interact::SkeletonNode::FrontThighRight, Interact::BodyPart::ThighRight},
    {Interact::SkeletonNode::RearThighRight, Interact::BodyPart::ThighRight},
    {Interact::SkeletonNode::ButtLeft, Interact::BodyPart::ButtLeft},
    {Interact::SkeletonNode::ButtRight, Interact::BodyPart::ButtRight},
    {Interact::SkeletonNode::FootLeft, Interact::BodyPart::FootLeft},
    {Interact::SkeletonNode::ToeLeft, Interact::BodyPart::FootLeft},
    {Interact::SkeletonNode::FootRight, Interact::BodyPart::FootRight},
    {Interact::SkeletonNode::ToeRight, Interact::BodyPart::FootRight},
    {Interact::SkeletonNode::VaginaLeft, Interact::BodyPart::Vagina},
    {Interact::SkeletonNode::VaginaRight, Interact::BodyPart::Vagina},
    {Interact::SkeletonNode::Clitoris, Interact::BodyPart::Vagina},
    {Interact::SkeletonNode::Pelvis, Interact::BodyPart::Vagina},
    {Interact::SkeletonNode::VaginaDeep, Interact::BodyPart::Vagina},
    {Interact::SkeletonNode::Anus, Interact::BodyPart::Anus},
    {Interact::SkeletonNode::AnusDeep, Interact::BodyPart::Anus},
    {Interact::SkeletonNode::AnimObjA, Interact::BodyPart::Mouth},
    {Interact::SkeletonNode::AnimObjB, Interact::BodyPart::Mouth},
    {Interact::SkeletonNode::AnimObjR, Interact::BodyPart::HandRight},
    {Interact::SkeletonNode::AnimObjL, Interact::BodyPart::HandLeft},
};

// ─── Helpers ──────────────────────────────────────────────────────────────────

Interact::BodyPart operator|(Interact::BodyPart a, Interact::BodyPart b)
{
  return static_cast<Interact::BodyPart>(static_cast<std::underlying_type_t<Interact::BodyPart>>(a) |
                                         static_cast<std::underlying_type_t<Interact::BodyPart>>(b));
}

bool operator<=(Interact::BodyPart part_a, Interact::BodyPart part_b)
{
  const auto a = static_cast<std::underlying_type_t<Interact::BodyPart>>(part_a);
  const auto b = static_cast<std::underlying_type_t<Interact::BodyPart>>(part_b);
  return (a & b) == b;
}

static Interact::BodyPart NormalizeBodyPart(Interact::BodyPart bodyPart)
{
  switch (bodyPart) {
  case Interact::BodyPart::BreastRight:
    return Interact::BodyPart::BreastLeft;
  case Interact::BodyPart::HandRight:
    return Interact::BodyPart::HandLeft;
  case Interact::BodyPart::ThighRight:
    return Interact::BodyPart::ThighLeft;
  case Interact::BodyPart::ButtRight:
    return Interact::BodyPart::ButtLeft;
  case Interact::BodyPart::FootRight:
    return Interact::BodyPart::FootLeft;
  default:
    return bodyPart;
  }
}

static Interact::DistanceCacheKey MakeDistanceCacheKey(RE::NiNode* first, RE::NiNode* second)
{
  return std::less<RE::NiNode*>{}(second, first) ? Interact::DistanceCacheKey{second, first}
                                                 : Interact::DistanceCacheKey{first, second};
}

static float
CalcNodeDistance(RE::NiNode* first, RE::NiNode* second,
                 std::unordered_map<Interact::DistanceCacheKey, float, Interact::DistanceCacheKeyHash>& distanceCache)
{
  if (!first || !second)
    return (std::numeric_limits<float>::max)();

  const auto key = MakeDistanceCacheKey(first, second);
  if (auto it = distanceCache.find(key); it != distanceCache.end())
    return it->second;

  const auto distance = (first->world.translate - second->world.translate).Length();
  distanceCache.emplace(key, distance);
  return distance;
}

static float CalcSchlongDistance(
    const Interact::SchlongData& schlong, RE::NiNode* target,
    std::unordered_map<Interact::DistanceCacheKey, float, Interact::DistanceCacheKeyHash>& distanceCache)
{
  auto* source = schlong.isPoint ? schlong.mid : schlong.head;
  return CalcNodeDistance(source, target, distanceCache);
}

static float CalcSchlongRootDistance(
    const Interact::SchlongData& schlong, RE::NiNode* target,
    std::unordered_map<Interact::DistanceCacheKey, float, Interact::DistanceCacheKeyHash>& distanceCache)
{
  return CalcNodeDistance(schlong.isPoint ? schlong.mid : schlong.root, target, distanceCache);
}

// ─── Rules ────────────────────────────────────────────────────────────────────
// Each rule key contains exactly 1-2 distinct BodyPart bits (Right-side parts
// are folded to Left by NormalizeBodyPart before lookup, so only Left-side
// keys are needed for paired parts).
// Matching: ruleKey must be a subset of the rule key (IsSubset).
// Disambiguation: when multiple rules match, pick the one with the most bits
// set (most specific), using std::popcount.

static const std::unordered_map<Interact::BodyPart, Interact::Rule> rules{
    // Mouth only (Kiss: both actors contribute Mouth, OR-folded to single Mouth bit)
    {Interact::BodyPart::Mouth, {Interact::Type::Kiss, 2.0f, false}},

    // Mouth + other
    {Interact::BodyPart::Mouth | Interact::BodyPart::FootLeft, {Interact::Type::ToeSucking, 2.0f, false}},
    {Interact::BodyPart::Mouth | Interact::BodyPart::Vagina, {Interact::Type::Cunnilingus, 2.0f, false}},
    {Interact::BodyPart::Mouth | Interact::BodyPart::Anus, {Interact::Type::Anilingus, 2.0f, false}},
    {Interact::BodyPart::Mouth | Interact::BodyPart::Penis, {Interact::Type::Fellatio, 2.0f, true}},

    // Breast + other (BreastRight normalized to BreastLeft)
    {Interact::BodyPart::BreastLeft | Interact::BodyPart::HandLeft, {Interact::Type::GropeBreast, 2.0f, false}},
    {Interact::BodyPart::BreastLeft | Interact::BodyPart::Penis, {Interact::Type::Titfuck, 2.0f, true}},

    // Hand + other (HandRight normalized to HandLeft)
    {Interact::BodyPart::HandLeft | Interact::BodyPart::Vagina, {Interact::Type::FingerVagina, 2.0f, false}},
    {Interact::BodyPart::HandLeft | Interact::BodyPart::Anus, {Interact::Type::FingerAnus, 2.0f, false}},
    {Interact::BodyPart::HandLeft | Interact::BodyPart::Penis, {Interact::Type::Handjob, 2.0f, true}},

    // Belly + other
    {Interact::BodyPart::Belly | Interact::BodyPart::Penis, {Interact::Type::Naveljob, 2.0f, true}},

    // Thigh + other (ThighRight normalized to ThighLeft)
    {Interact::BodyPart::ThighLeft | Interact::BodyPart::Penis, {Interact::Type::Thighjob, 2.0f, true}},

    // Butt + other (ButtRight normalized to ButtLeft)
    {Interact::BodyPart::ButtLeft | Interact::BodyPart::Penis, {Interact::Type::Frottage, 2.0f, true}},

    // Foot + other (FootRight normalized to FootLeft)
    {Interact::BodyPart::FootLeft | Interact::BodyPart::Penis, {Interact::Type::Footjob, 2.0f, true}},

    // Vagina only (Tribbing: both actors contribute Vagina, OR-folded to single Vagina bit)
    {Interact::BodyPart::Vagina, {Interact::Type::Tribbing, 2.0f, false}},

    // Vagina + other
    {Interact::BodyPart::Vagina | Interact::BodyPart::Penis, {Interact::Type::Vaginal, 2.0f, true}},

    // Anus + other (Human only, enforced by HasBodyPart)
    {Interact::BodyPart::Anus | Interact::BodyPart::Penis, {Interact::Type::Anal, 2.0f, true}},
};

static const Interact::Info kEmptyInfo{};
static const Interact::Data kEmptyData{};

// ─── HasBodyPart ──────────────────────────────────────────────────────────────

static bool HasBodyPart(const Interact::Data& data, Interact::BodyPart bodyPart)
{
  const bool isFemaleOrFuta =
      data.gender.Get() == Define::Gender::Type::Female || data.gender.Get() == Define::Gender::Type::Futa;

  switch (bodyPart) {
  // All creatures
  case Interact::BodyPart::Mouth:
  case Interact::BodyPart::HandLeft:
  case Interact::BodyPart::HandRight:
  case Interact::BodyPart::FootLeft:
  case Interact::BodyPart::FootRight:
    return true;

  // Female or Futa only
  case Interact::BodyPart::BreastLeft:
  case Interact::BodyPart::BreastRight:
  case Interact::BodyPart::Belly:
  case Interact::BodyPart::ThighLeft:
  case Interact::BodyPart::ThighRight:
  case Interact::BodyPart::ButtLeft:
  case Interact::BodyPart::ButtRight:
  case Interact::BodyPart::Vagina:
    return isFemaleOrFuta;

  // Human only (NPC RT Anus2 node only exists on human skeleton)
  case Interact::BodyPart::Anus:
    return data.race.GetType() == Define::Race::Type::Human;

  case Interact::BodyPart::Penis:
    return data.gender.HasPenis();

  default:
    return false;
  }
}

// ─── Constructor ──────────────────────────────────────────────────────────────

Interact::Interact(std::vector<RE::Actor*> actors)
{
  datas.clear();
  for (auto* actor : actors) {
    if (!actor)
      continue;

    Data data{};
    data.gender = Define::Gender(actor);
    data.race   = Define::Race(actor);

    for (auto [bodyPart, _] : magic_enum::enum_entries<BodyPart>())
      data.bodyParts.emplace(bodyPart, Info{});

    auto* nodes = actor->Get3D();
    if (!nodes) {
      datas[actor] = std::move(data);
      continue;
    }

    for (auto [skeletonNode, _] : magic_enum::enum_entries<SkeletonNode>()) {
      auto node = nodes->GetObjectByName(skeletonNodeMap.at(skeletonNode));
      data.skeletonNodes.emplace(skeletonNode, node ? node->AsNode() : nullptr);
    }

    if (data.gender.HasPenis()) {
      if (auto schlongIt = schlongNodes.find(data.race.GetType()); schlongIt != schlongNodes.end()) {
        const auto& sn = schlongIt->second;
        // Guard empty strings (e.g. StormAtronach root/head are intentionally empty)
        if (!sn.root.empty())
          if (auto node = nodes->GetObjectByName(sn.root); node)
            data.schlong.root = node->AsNode();
        if (!sn.mid.empty())
          if (auto node = nodes->GetObjectByName(sn.mid); node)
            data.schlong.mid = node->AsNode();
        if (!sn.head.empty())
          if (auto node = nodes->GetObjectByName(sn.head); node)
            data.schlong.head = node->AsNode();
        data.schlong.isPoint = sn.isPoint;
      }
    }

    datas[actor] = std::move(data);
  }
}

// ─── Update ───────────────────────────────────────────────────────────────────

void Interact::Update()
{
  const float nowMs = static_cast<float>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
          .count());

  std::unordered_map<DistanceCacheKey, float, DistanceCacheKeyHash> distanceCache{};

  // Save previous frame for velocity, then clear current frame
  std::unordered_map<RE::Actor*, std::unordered_map<BodyPart, Info>> prevBodyParts{};
  for (auto& [actor, data] : datas) {
    prevBodyParts[actor] = data.bodyParts;
    for (auto& [bodyPart, info] : data.bodyParts)
      info = {};
  }

  for (auto sourceIt = datas.begin(); sourceIt != datas.end(); ++sourceIt) {
    auto targetIt = sourceIt;
    ++targetIt;

    for (; targetIt != datas.end(); ++targetIt) {
      auto* sourceActor = sourceIt->first;
      auto* targetActor = targetIt->first;
      auto& sourceData  = sourceIt->second;
      auto& targetData  = targetIt->second;

      // Per-part-pair best distance table.
      // Key   = NormalizeBodyPart(srcPart) | NormalizeBodyPart(tgtPart)  (for rule lookup)
      // Value = best candidate so far, storing the ORIGINAL (non-normalized) parts for correct write-back
      struct PairBest
      {
        float distance      = (std::numeric_limits<float>::max)();
        BodyPart sourcePart = BodyPart::Mouth;  // original, preserves Left/Right
        BodyPart targetPart = BodyPart::Mouth;
        bool fromSchlong    = false;
      };
      std::unordered_map<BodyPart, PairBest> bestPerPair{};

      auto tryUpdate = [&](BodyPart rawSrc, BodyPart rawTgt, float dist, bool fromSchlong) {
        // Normalize only for the lookup key so Left/Right variants share one slot
        const BodyPart key = NormalizeBodyPart(rawSrc) | NormalizeBodyPart(rawTgt);
        auto& best         = bestPerPair[key];
        if (dist < best.distance) {
          best.distance    = dist;
          best.sourcePart  = rawSrc;
          best.targetPart  = rawTgt;
          best.fromSchlong = fromSchlong;
        }
      };

      // 1. Normal skeleton node pairs (no penis nodes)
      for (const auto& [sourceNode, sourcePtr] : sourceData.skeletonNodes) {
        if (!sourcePtr)
          continue;
        const auto sourcePartIt = nodeToPartMap.find(sourceNode);
        if (sourcePartIt == nodeToPartMap.end() || !HasBodyPart(sourceData, sourcePartIt->second))
          continue;

        for (const auto& [targetNode, targetPtr] : targetData.skeletonNodes) {
          if (!targetPtr)
            continue;
          const auto targetPartIt = nodeToPartMap.find(targetNode);
          if (targetPartIt == nodeToPartMap.end() || !HasBodyPart(targetData, targetPartIt->second))
            continue;

          const float dist = CalcNodeDistance(sourcePtr, targetPtr, distanceCache);
          tryUpdate(sourcePartIt->second, targetPartIt->second, dist, false);
        }
      }

      // 2. Source has penis → tip distance to all target nodes
      if (sourceData.gender.HasPenis() && (sourceData.schlong.head || sourceData.schlong.mid)) {
        for (const auto& [targetNode, targetPtr] : targetData.skeletonNodes) {
          if (!targetPtr)
            continue;
          const auto targetPartIt = nodeToPartMap.find(targetNode);
          if (targetPartIt == nodeToPartMap.end() || !HasBodyPart(targetData, targetPartIt->second))
            continue;

          const float dist = CalcSchlongDistance(sourceData.schlong, targetPtr, distanceCache);
          tryUpdate(BodyPart::Penis, targetPartIt->second, dist, true);
        }
      }

      // 3. Target has penis → tip distance to all source nodes
      if (targetData.gender.HasPenis() && (targetData.schlong.head || targetData.schlong.mid)) {
        for (const auto& [sourceNode, sourcePtr] : sourceData.skeletonNodes) {
          if (!sourcePtr)
            continue;
          const auto sourcePartIt = nodeToPartMap.find(sourceNode);
          if (sourcePartIt == nodeToPartMap.end() || !HasBodyPart(sourceData, sourcePartIt->second))
            continue;

          const float dist = CalcSchlongDistance(targetData.schlong, sourcePtr, distanceCache);
          tryUpdate(sourcePartIt->second, BodyPart::Penis, dist, true);
        }
      }

      // Vaginal/Anal mutual exclusion: anatomy puts Vagina and Anus close together,
      // so when both are within radius for the same penis keep only the nearer one.
      {
        const BodyPart vagKey  = BodyPart::Vagina | BodyPart::Penis;
        const BodyPart anusKey = BodyPart::Anus | BodyPart::Penis;
        auto vagIt             = bestPerPair.find(vagKey);
        auto anusIt            = bestPerPair.find(anusKey);
        if (vagIt != bestPerPair.end() && anusIt != bestPerPair.end()) {
          if (vagIt->second.distance <= anusIt->second.distance)
            bestPerPair.erase(anusIt);
          else
            bestPerPair.erase(vagIt);
        }
      }

      // Match rules for each part pair independently
      for (const auto& [pairKey, best] : bestPerPair) {
        // Select the most specific rule (highest popcount) whose key is a subset of pairKey,
        // within radius, with matching schlong requirement.
        const Rule* matchedRule = nullptr;
        int matchedBits         = -1;

        for (const auto& [key, rule] : rules) {
          // rule key must be a subset of pairKey:
          // every bit required by the rule must be present in pairKey
          if (!(pairKey <= key))
            continue;
          if (rule.requiresSchlong != best.fromSchlong)
            continue;
          if (best.distance > rule.radius)
            continue;

          const int bits = std::popcount(static_cast<std::underlying_type_t<BodyPart>>(key));
          if (bits > matchedBits) {
            matchedBits = bits;
            matchedRule = &rule;
          }
        }

        if (!matchedRule)
          continue;

        auto type = matchedRule->type;

        // DeepThroat sub-variant: upgrade Fellatio when schlong root reaches the mouth
        if (type == Type::Fellatio) {
          RE::NiNode* mouthNode      = nullptr;
          const SchlongData* schlong = nullptr;

          if (best.sourcePart == BodyPart::Mouth) {
            if (auto it = sourceData.skeletonNodes.find(SkeletonNode::Head); it != sourceData.skeletonNodes.end())
              mouthNode = it->second;
            schlong = &targetData.schlong;
          } else {
            if (auto it = targetData.skeletonNodes.find(SkeletonNode::Head); it != targetData.skeletonNodes.end())
              mouthNode = it->second;
            schlong = &sourceData.schlong;
          }

          if (schlong && mouthNode && CalcSchlongRootDistance(*schlong, mouthNode, distanceCache) <= 8.0f)
            type = Type::DeepThroat;
        }

        // Write-back using ORIGINAL (non-normalized) parts as keys,
        // preserving Left/Right distinction in bodyParts map.
        // For penis interactions the penis side mirrors the receiver's conclusion;
        // only update if this candidate is closer than what is already recorded.
        if (best.fromSchlong) {
          const bool sourcePenis   = (best.sourcePart == BodyPart::Penis);
          auto* penisActor         = sourcePenis ? sourceActor : targetActor;
          auto* otherActor         = sourcePenis ? targetActor : sourceActor;
          auto& penisData          = sourcePenis ? sourceData : targetData;
          auto& otherData          = sourcePenis ? targetData : sourceData;
          const BodyPart otherPart = sourcePenis ? best.targetPart : best.sourcePart;

          auto& receiverInfo = otherData.bodyParts[otherPart];
          if (!receiverInfo.actor || best.distance < receiverInfo.distance)
            receiverInfo = Info{penisActor, best.distance, 0.0f, type};

          // Penis side mirrors receiver; keeps closest across all penis interactions
          auto& penisInfo = penisData.bodyParts[BodyPart::Penis];
          if (!penisInfo.actor || best.distance < penisInfo.distance)
            penisInfo = Info{otherActor, best.distance, 0.0f, type};

        } else {
          auto& srcInfo = sourceData.bodyParts[best.sourcePart];
          if (!srcInfo.actor || best.distance < srcInfo.distance)
            srcInfo = Info{targetActor, best.distance, 0.0f, type};

          auto& tgtInfo = targetData.bodyParts[best.targetPart];
          if (!tgtInfo.actor || best.distance < tgtInfo.distance)
            tgtInfo = Info{sourceActor, best.distance, 0.0f, type};
        }
      }
    }
  }

  // Velocity: exponential smoothing of distance delta per body part
  static constexpr float kVelocityAlpha = 0.4f;

  for (auto& [actor, data] : datas) {
    const float prevTime = data.lastUpdateMs;
    const float deltaMs  = (prevTime > 0.0f) ? (nowMs - prevTime) : 0.0f;
    data.lastUpdateMs    = nowMs;

    if (deltaMs <= 0.0f)
      continue;

    const auto prevIt = prevBodyParts.find(actor);
    if (prevIt == prevBodyParts.end())
      continue;

    for (auto& [bodyPart, info] : data.bodyParts) {
      if (!info.actor)
        continue;

      const auto& prevMap   = prevIt->second;
      const auto prevInfoIt = prevMap.find(bodyPart);

      if (prevInfoIt == prevMap.end() || !prevInfoIt->second.actor) {
        info.velocity = 0.0f;
        continue;
      }

      const float rawVel = (info.distance - prevInfoIt->second.distance) / deltaMs;
      info.velocity      = prevInfoIt->second.velocity + kVelocityAlpha * (rawVel - prevInfoIt->second.velocity);
    }
  }

  // Debug output
  for (const auto& [actor, data] : datas) {
    const auto actorName = actor ? actor->GetDisplayFullName() : "null";
    for (const auto& [bodyPart, info] : data.bodyParts) {
      if (!info.actor || info.type == Type::None)
        continue;

      logger::info("[Interact Debug] actor='{}' part='{}' type='{}' distance={:.3f} velocity={:.4f} target='{}'",
                   actorName, magic_enum::enum_flags_name(bodyPart), magic_enum::enum_name(info.type), info.distance,
                   info.velocity, info.actor->GetDisplayFullName());
    }
  }
}

// ─── FlashNodeData ────────────────────────────────────────────────────────────
// Call manually after stage transitions or equipment changes, not every frame.

void Interact::FlashNodeData()
{
  for (auto& [actor, data] : datas) {
    if (!actor)
      continue;

    auto* nodes = actor->Get3D();
    if (!nodes)
      continue;

    for (auto [skeletonNode, _] : magic_enum::enum_entries<SkeletonNode>()) {
      auto node = nodes->GetObjectByName(skeletonNodeMap.at(skeletonNode));
      if (!node)
        logger::warn("[Interact] Failed to find node '{}' for actor '{}'", skeletonNodeMap.at(skeletonNode),
                     actor->GetDisplayFullName());
      data.skeletonNodes[skeletonNode] = node ? node->AsNode() : nullptr;
    }

    if (data.gender.HasPenis()) {
      if (auto schlongIt = schlongNodes.find(data.race.GetType()); schlongIt != schlongNodes.end()) {
        const auto& sn = schlongIt->second;
        if (!sn.root.empty())
          if (auto node = nodes->GetObjectByName(sn.root); node)
            data.schlong.root = node->AsNode();
        if (!sn.mid.empty())
          if (auto node = nodes->GetObjectByName(sn.mid); node)
            data.schlong.mid = node->AsNode();
        if (!sn.head.empty())
          if (auto node = nodes->GetObjectByName(sn.head); node)
            data.schlong.head = node->AsNode();
        data.schlong.isPoint = sn.isPoint;
      }
    }
  }
}

// ─── Accessors ────────────────────────────────────────────────────────────────

const Interact::Data& Interact::GetData(RE::Actor* actor) const
{
  if (auto it = datas.find(actor); it != datas.end())
    return it->second;
  return kEmptyData;
}

const Interact::Info& Interact::GetInfo(RE::Actor* actor, BodyPart bodyPart) const
{
  if (auto dataIt = datas.find(actor); dataIt != datas.end()) {
    if (auto infoIt = dataIt->second.bodyParts.find(bodyPart); infoIt != dataIt->second.bodyParts.end())
      return infoIt->second;
  }
  return kEmptyInfo;
}

}  // namespace Instance
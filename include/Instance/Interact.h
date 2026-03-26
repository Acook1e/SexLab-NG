#pragma once

#include "Define/Gender.h"
#include "Define/Race.h"

#include <optional>

namespace Instance
{
class Interact
{
public:
  enum class Type : std::uint8_t
  {
    None = 0,
    // For Mouth
    Kiss,          // Mouth and Mouth
    ToeSucking,    // Mouth and Foot
    Cunnilingus,   // Mouth and Vagina
    Anilingus,     // Mouth and Anus
    Fellatio,      // Mouth and Penis
    DeepThroat,    // Mouth and Penis, deeper than Fellatio
    MouthAnimObj,  // Mouth and AnimObject

    // For Breast
    GropeBreast,  // Breast and Hand
    Titfuck,      // Breast and Penis

    // For Hand
    FingerVagina,  // Hand and Vagina
    FingerAnus,    // Hand and Anus
    Handjob,       // Hand and Penis

    // For Belly
    Naveljob,  // Belly and Penis

    // For Thigh
    Thighjob,  // Thigh and Penis

    // For Butt
    Frottage,  // Butt and Penis

    // For foot
    Footjob,  // Foot and Penis

    // For Vagina
    Tribbing,  // Vagina and Vagina
    Vaginal,   // Vagina and Penis

    // For Anus
    Anal,  //  Anus and Penis

    // For Penis
    PenisAnimObj,  // Penis and AnimObject

    Total
  };

  enum class BodyPart : std::uint16_t
  {
    Mouth       = 1 << 0,   // All Creature
    BreastLeft  = 1 << 1,   // Female or Futa
    BreastRight = 1 << 2,   // Female or Futa
    HandLeft    = 1 << 3,   // All Creature
    HandRight   = 1 << 4,   // All Creature
    Belly       = 1 << 5,   // Female or Futa
    ThighLeft   = 1 << 6,   // Female or Futa
    ThighRight  = 1 << 7,   // Female or Futa
    ButtLeft    = 1 << 8,   // Female or Futa
    ButtRight   = 1 << 9,   // Female or Futa
    FootLeft    = 1 << 10,  // Female or Futa
    FootRight   = 1 << 11,  // Female or Futa
    Vagina      = 1 << 12,  // Female or Futa
    Anus        = 1 << 13,  // Human only
    Penis       = 1 << 14,  // Male, Futa or CreatureMale
  };

  enum class SkeletonNode : std::uint8_t
  {
    Head = 0,
    HandLeft,
    HandLeftRef,
    HandRight,
    HandRightRef,
    FingerLeft,
    FingerRight,
    ThumbLeft,
    ThumbRight,
    BreastLeft,
    BreastRight,
    NippleLeft,
    NippleRight,
    Belly,
    FrontThighLeft,
    FrontThighRight,
    RearThighLeft,
    RearThighRight,
    ButtLeft,
    ButtRight,
    FootLeft,
    FootRight,
    ToeLeft,
    ToeRight,
    VaginaLeft,
    VaginaRight,
    Clitoris,
    Pelvis,
    VaginaDeep,
    Anus,
    AnusDeep,
    SpineDown,
    AnimObjA,
    AnimObjB,
    AnimObjR,
    AnimObjL
  };

  struct SchlongData
  {
    RE::NiNode* root = nullptr;
    RE::NiNode* mid  = nullptr;
    RE::NiNode* head = nullptr;
    bool isPoint     = false;
  };

  struct Info
  {
    RE::Actor* actor = nullptr;
    float distance   = 0.0f;
    Type type        = Type::None;
  };

  struct Data
  {
    std::unordered_map<BodyPart, Info> bodyParts{};
    std::unordered_map<SkeletonNode, RE::NiNode*> skeletonNodes{};
    SchlongData schlong{};
    Define::Race race     = Define::Race::Type::Unknown;
    Define::Gender gender = Define::Gender::Type::Unknown;
  };

  struct Rule
  {
    Type type                = Type::None;
    float radius             = 0.0f;
    bool requiresSchlong     = false;
    bool requiresSchlongRoot = false;
  };

  struct DistanceCacheKey
  {
    RE::NiNode* first  = nullptr;
    RE::NiNode* second = nullptr;

    [[nodiscard]] bool operator==(const DistanceCacheKey& other) const
    {
      return first == other.first && second == other.second;
    }
  };

  struct DistanceCacheKeyHash
  {
    [[nodiscard]] std::size_t operator()(const DistanceCacheKey& key) const
    {
      const auto firstHash  = std::hash<RE::NiNode*>{}(key.first);
      const auto secondHash = std::hash<RE::NiNode*>{}(key.second);
      return firstHash ^ (secondHash << 1);
    }
  };

  Interact() = default;
  Interact(std::vector<RE::Actor*> actors);

  void Update();
  void FlashNodeData();

  [[nodiscard]] const Data& GetData(RE::Actor* actor) const;
  [[nodiscard]] const Info& GetInfo(RE::Actor* actor, BodyPart bodyPart) const;

private:
  std::unordered_map<RE::Actor*, Data> datas{};
};
}  // namespace Instance
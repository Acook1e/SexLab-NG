#pragma once

#include "Define/BodyPart.h"
#include "Define/Gender.h"
#include "Define/Race.h"

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

  struct Info
  {
    Define::BodyPart part{};
    RE::Actor* prevActor = nullptr;
    RE::Actor* actor     = nullptr;
    float prevDistance   = 0.0f;
    float distance       = 0.0f;
    Type prevType        = Type::None;
    Type type            = Type::None;
    float velocity       = 0.0f;
  };

  struct Data
  {
    std::unordered_map<Define::BodyPart::Name, Info> bodyParts{};
    Define::Race race     = Define::Race::Type::Unknown;
    Define::Gender gender = Define::Gender::Type::Unknown;
    float lastUpdateMs    = 0.0f;
  };

  Interact() = default;
  Interact(std::vector<RE::Actor*> actors) {}

  void Update() {}
  void FlashNodeData() {}

  [[nodiscard]] const Data& GetData(RE::Actor* actor) const;
  [[nodiscard]] const Info& GetInfo(RE::Actor* actor, Define::BodyPart::Name bodyPart) const;

private:
  std::unordered_map<RE::Actor*, Data> datas{};
};
}  // namespace Instance
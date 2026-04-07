#pragma once

#include "Define/BodyPart.h"
#include "Define/Gender.h"
#include "Define/Race.h"

namespace Instance
{

class Interact
{
public:
  // ── 交互类型 ─────────────────────────────────────────────────────────────
  enum class Type : std::uint8_t
  {
    None = 0,
    // Mouth
    Kiss,         // Mouth  <-> Mouth
    ToeSucking,   // Mouth  ->  Foot
    Cunnilingus,  // Mouth  ->  Vagina
    Anilingus,    // Mouth  ->  Anus
    Fellatio,     // Mouth  <-  Penis
    DeepThroat,   // Mouth  <-  Penis  (deeper)
    MouthAnimObj,
    // Breast
    GropeBreast,  // Breast <-> Hand
    Titfuck,      // Breast <-> Penis
    // Hand / Finger
    FingerVagina,  // Finger ->  Vagina
    FingerAnus,    // Finger ->  Anus
    Handjob,       // Hand   ->  Penis
    // Belly
    Naveljob,  // Belly  <-  Penis
    // Thigh
    Thighjob,  // Thigh  <-  Penis
    // Butt
    Frottage,  // Butt   <-  Penis
    // Foot
    Footjob,  // Foot   <-  Penis
    // Vagina
    Tribbing,  // Vagina <-> Vagina
    Vaginal,   // Vagina <-  Penis
    // Anus
    Anal,  // Anus   <-  Penis
    // Penis
    PenisAnimObj,

    Total
  };

  struct Info
  {
    Define::BodyPart bodypart{};
    RE::Actor* actor     = nullptr;
    RE::Actor* prevActor = nullptr;
    float distance       = 0.f;
    float prevDistance   = 0.f;
    Type type            = Type::None;
    Type prevType        = Type::None;
    float velocity       = 0.f;  // 靠近速度（units/ms，负=靠近）
  };

  struct ActorData
  {
    std::unordered_map<Define::BodyPart::Name, Interact::Info> infos{};
    Define::Race race     = Define::Race::Type::Unknown;
    Define::Gender gender = Define::Gender::Type::Unknown;
    float lastUpdateMs    = 0.f;
  };

  Interact() = default;
  explicit Interact(std::vector<RE::Actor*> actors);

  void FlashNodeData();
  void Update();

  // ── 查询 ────────────────────────────────────────────────────────────────
  [[nodiscard]] const Info& GetInfo(RE::Actor* actor, Define::BodyPart::Name part) const;

private:
  std::unordered_map<RE::Actor*, ActorData> datas{};
};

}  // namespace Instance
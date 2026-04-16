#pragma once

#include "Define/BodyPart.h"
#include "Define/Gender.h"
#include "Define/Race.h"
#include "Define/Tag.h"

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
    Kiss,           // Mouth <-> Mouth
    BreastSucking,  // Mouth <-> Breast
    ToeSucking,     // Mouth <-> Foot
    Cunnilingus,    // Mouth <-> Vagina
    Anilingus,      // Mouth <-> Anus
    Fellatio,       // Mouth <-> Penis
    DeepThroat,     // Throat <-> Penis
    MouthAnimObj,
    // Breast
    GropeBreast,  // Breast <-> Hand
    Titfuck,      // Breast <-> Penis
    // Hand / Finger
    GropeThigh,    // Hand <-> Thigh
    GropeButt,     // Hand <-> Butt
    GropeFoot,     // Hand <-> Foot
    FingerVagina,  // Finger <-> Vagina
    FingerAnus,    // Finger <-> Anus
    Handjob,       // Hand <-> Penis
    Masturbation,  // FingerVagina/FingerAnus/Handjob But Self
    HandAnimObj,
    // Belly
    Naveljob,  // Belly <-> Penis
    // Thigh
    Thighjob,  // Thigh <-> Penis
    // Butt
    Frottage,  // GlutealCleft <-> Penis
    // Foot
    Footjob,  // Foot <-> Penis
    // Vagina
    Tribbing,  // Vagina <-> Vagina
    Vaginal,   // Vagina <-> Penis
    VaginaAnimObj,
    // Anus
    Anal,  // Anus <-> Penis
    AnalAnimObj,
    // Penis
    PenisAnimObj,

    // Multi Interact
    SixtyNine,  // Actor A Fellatio/DeepThroat with Actor B and Actor B Cunnilingus with Actor A
    Spitroast,  // Actor has Fellatio/DeepThroat and Vaginal/Anal at same time
    DoublePenetration,  // Actor has Vaginal and Anal at same time
    TriplePenetration,  // Actor has Fellatio/DeepThroat and Vaginal and Anal at same time

    Total
  };

  struct InteractionSnapshot
  {
    RE::Actor* partner  = nullptr;
    Type type           = Type::None;
    float distance      = 0.f;
    float approachSpeed = 0.f;  // 靠近速度（units/ms，负=靠近）

    [[nodiscard]] bool IsActive() const noexcept { return type != Type::None; }
  };

  struct MotionSnapshot
  {
    Define::Point3f start      = Define::Point3f::Zero();
    Define::Point3f end        = Define::Point3f::Zero();
    Define::Vector3f direction = Define::Vector3f::Zero();
    float length               = 0.f;
    float timeMs               = 0.f;
    bool valid                 = false;
    bool directional           = false;
    Define::CollisionSet collisions{};

    [[nodiscard]] bool HasCollider() const noexcept
    {
      return !collisions.capsules.empty() || !collisions.boxes.empty() ||
             !collisions.funnels.empty() || !collisions.surfaces.empty() ||
             !collisions.envelopes.empty();
    }
  };

  struct MotionHistory
  {
    MotionSnapshot current{};
    MotionSnapshot previous{};
    MotionSnapshot older{};
  };

  struct PartState
  {
    Define::BodyPart bodyPart{};
    InteractionSnapshot current{};
    InteractionSnapshot previous{};
    MotionHistory motion{};

    [[nodiscard]] bool HasInteraction() const noexcept { return current.IsActive(); }
  };

  struct SlotMemory
  {
    RE::Actor* partner = nullptr;
    float continuityMs = 0.f;
    float holdMs       = 0.f;

    [[nodiscard]] bool HasRecentPartner() const noexcept
    {
      return partner != nullptr && holdMs > 0.f;
    }
  };

  struct ActorState
  {
    std::unordered_map<Define::BodyPart::Name, Interact::PartState> parts{};
    SlotMemory vaginal{};
    SlotMemory anal{};
    Define::Race race     = Define::Race::Type::Unknown;
    Define::Gender gender = Define::Gender::Type::Unknown;
  };

  Interact() = default;
  explicit Interact(std::vector<RE::Actor*> actors);

  void FlashNodeData();
  void Update(float deltaMs);

  // ── 查询 ────────────────────────────────────────────────────────────────
  [[nodiscard]] const ActorState& GetActorState(RE::Actor* actor) const
  {
    return actorStates.at(actor);
  }
  [[nodiscard]] const ActorState& GetData(RE::Actor* actor) const { return GetActorState(actor); }
  [[nodiscard]] const PartState& GetPartState(RE::Actor* actor, Define::BodyPart::Name part) const;
  [[nodiscard]] const PartState& GetInfo(RE::Actor* actor, Define::BodyPart::Name part) const
  {
    return GetPartState(actor, part);
  }
  [[nodiscard]] const Define::InteractTags& GetObservedInteractTags(RE::Actor* actor) const;

private:
  std::unordered_map<RE::Actor*, ActorState> actorStates{};
  std::unordered_map<RE::Actor*, Define::InteractTags> observedInteractTags{};
  float timelineMs = 0.f;
};

}  // namespace Instance
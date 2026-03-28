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

  // ── 每个部位的当前交互结果（外部只读）────────────────────────────────────
  struct Info
  {
    RE::Actor* actor = nullptr;  // 与之交互的 actor（nullptr = 无交互）
    float distance   = 0.f;      // 交互距离
    float velocity   = 0.f;      // 靠近速度（units/ms，负=靠近）
    Type type        = Type::None;
    // 上一帧镜像，用于滞后判断
    RE::Actor* prevActor = nullptr;
    float prevDistance   = 0.f;
    Type prevType        = Type::None;
  };

  struct ActorData
  {
    // 每个该 actor 拥有的 BodyPart 实例（按 Name 索引）
    std::unordered_map<Define::BodyPart::Name, Define::BodyPart> parts{};
    // 每个 BodyPart 当前帧的交互结果
    std::unordered_map<Define::BodyPart::Name, Interact::Info> infos{};
    Define::Race race     = Define::Race::Type::Unknown;
    Define::Gender gender = Define::Gender::Type::Unknown;
    float lastUpdateMs    = 0.f;
  };

  // ── 生命周期 ─────────────────────────────────────────────────────────────
  Interact() = default;
  explicit Interact(std::vector<RE::Actor*> actors);

  // 节点指针刷新（骨骼树重建后调用，如换装）
  void FlashNodeData();

  // 每帧位置更新 + 交互计算
  void Update();

  // ── 查询 ────────────────────────────────────────────────────────────────
  [[nodiscard]] const Info& GetInfo(RE::Actor* actor, Define::BodyPart::Name part) const;

private:
  std::unordered_map<RE::Actor*, ActorData> datas{};
};

}  // namespace Instance
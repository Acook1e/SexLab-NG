#pragma once

#include "Define/Enjoyment.h"
#include "Define/Scene.h"
#include "Instance/Interact.h"
#include "Instance/SceneInstance.h"

namespace Registry
{

class ActorStat
{
public:
  struct Level
  {
    // level from 1 to 999
    std::uint16_t level = 1;
    float XP            = 0.0f;
  };

  enum class ExperienceType : std::uint8_t
  {
    Main,
    Oral,
    Vagina,
    Anus,
    Solo,
    Group,
    Aggressive,
    Submissive,

    Total
  };

  struct Stat
  {
    std::unordered_map<ExperienceType, Level> experienceLevels{};
    std::vector<std::string> recentPartners{};

    // 性别倾向: 同性 vs 异性
    std::uint16_t TimesSameSex = 0;
    std::uint16_t TimesDiffSex = 0;
    // 种族倾向: 同族 vs 异族
    std::uint16_t TimesSameRace = 0;
    std::uint16_t TimesDiffRace = 0;

    // 记录全局享受值，随游戏更新/场景更新变化
    Define::Enjoyment enjoy{};

    // 性欲值 [0, 100]
    float arouse = 0.0f;
  };

  // 传入 UpdateOnSceneEnd 的每人记录: 最终 enjoyment 值 + 是否为主动方
  struct EndSceneRecord
  {
    float enjoyment  = 0.f;
    bool isAggressor = false;
  };

  static ActorStat& GetSingleton()
  {
    static ActorStat instance;
    return instance;
  }

  Stat& GetStat(RE::Actor* actor);

  // 向指定经验类型添加 XP，自动升级（上限 999）
  void AddXP(RE::Actor* actor, ExperienceType type, float amount);

  // 全局游戏更新且不在场景的时候，性欲/享受等随时间变化的值
  void Update();

  // 根据 actor 当前 stat 计算初始 enjoyment 值 [-100, 100]
  // scene: 当前场景
  // position:  该 actor 分配到的 position
  // interactData: 该 actor 的交互信息
  void UpdateEnjoyment(RE::Actor* actor, const Define::Scene* scene,
                       const Define::Position& position,
                       const Instance::Interact::ActorData& interactData);

  // 场景结束时统一更新所有 actor 的 stat
  void
  UpdateStat(std::unordered_map<RE::Actor*, Instance::SceneInstance::SceneActorInfo> actorInfoMap,
             const Define::Scene* scene);

  static void onSave(SKSE::SerializationInterface* serial);
  static void onLoad(SKSE::SerializationInterface* serial);
  static void onRevert(SKSE::SerializationInterface* serial);

private:
  ActorStat();
  // For serialization actorStats
  constexpr static std::uint32_t SerializationType = 'ASTA';
  static inline std::unordered_map<RE::FormID, Stat> actorStats;
  static inline std::unordered_map<RE::Actor*, Stat> runtimeActorStats;
};
}  // namespace Registry
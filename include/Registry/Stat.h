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
    Main = 0,

    // Actor Counts
    Solo,
    Group,

    // Scnen Type
    Aggressive,
    Submissive,

    // Body Part
    Mouth,
    Hand,
    Body,
    Breast,
    Vagina,
    Anus,
    Penis,

    Total
  };

  struct Stat
  {
    std::unordered_map<ExperienceType, Level> experienceLevels{};
    // 每种 body/style 轨道的倾向 [-1, 1]，越高越容易从对应刺激中获得 enjoy。
    std::unordered_map<ExperienceType, float> interactionTendencies{};
    std::vector<std::string> recentPartners{};

    // 性别倾向: 同性 vs 异性
    std::uint16_t TimesSameSex = 0;
    std::uint16_t TimesDiffSex = 0;
    // 种族倾向: 同族 vs 异族
    std::uint16_t TimesSameRace = 0;
    std::uint16_t TimesDiffRace = 0;

    // 性欲值 [0, 100]
    float arouse = 0.0f;

    // 记录全局享受值，随游戏更新/场景更新变化
    Define::Enjoyment enjoy{};

    // 敏感度值 [-10, 10] 影响enjoy增量
    float sensitive = 1.0f;
  };

  static ActorStat& GetSingleton()
  {
    static ActorStat instance;
    return instance;
  }

  Stat& GetStat(RE::Actor* actor);
  const Stat& GetStat(RE::Actor* actor) const;

  // 向指定 body/style 轨道添加 XP，自动升级（上限 999）
  void AddXP(RE::Actor* actor, ExperienceType type, float amount);
  void AddInteractionXP(RE::Actor* actor, Instance::Interact::Type type, float amount);

  std::uint16_t GetTrackLevel(RE::Actor* actor, ExperienceType type) const;
  float GetTrackTendency(RE::Actor* actor, ExperienceType type) const;

  // 这里返回的是交互归一化后的代表性 body 轨道数值。
  std::uint16_t GetInteractionLevel(RE::Actor* actor, Instance::Interact::Type type) const;
  float GetInteractionTendency(RE::Actor* actor, Instance::Interact::Type type) const;

  // 全局游戏更新且不在场景的时候，性欲/享受等随时间变化的值
  // deltaGameHours: 距上次调用经过的游戏内小时数（由 Calendar 计算）
  void Update(float deltaGameHours);

  // 根据全员 actorInfoMap 及交互数据为每个 actor 计算并更新 enjoy 增量
  // 每 0.1 秒由 SceneInstance 在 UI 更新前调用一次
  // actorInfoMap: 全员 position/climaxCount 信息，会在高潮时写入 climaxCount
  // interact:     全员实时交互数据
  void UpdateEnjoyment(
      const Define::Scene* scene,
      std::unordered_map<RE::Actor*, Instance::SceneInstance::SceneActorInfo>& actorInfoMap,
      const Instance::Interact& interact);

  // 场景结束时统一更新所有 actor 的 stat
  void UpdateStat(
      const Define::Scene* scene,
      const std::unordered_map<RE::Actor*, Instance::SceneInstance::SceneActorInfo>& actorInfoMap);

  static void onSave(SKSE::SerializationInterface* serial);
  static void onLoad(SKSE::SerializationInterface* serial);
  static void onProfileSave(SKSE::SerializationInterface* serial);
  static void onProfileLoad(SKSE::SerializationInterface* serial);
  static void onRevert(SKSE::SerializationInterface* serial);

private:
  ActorStat();
  // For serialization actorStats
  constexpr static std::uint32_t SerializationType        = 'ASTA';
  constexpr static std::uint32_t ProfileSerializationType = 'ASPF';
  static inline std::unordered_map<RE::FormID, Stat> actorStats;
  static inline std::unordered_map<RE::Actor*, Stat> runtimeActorStats;
  static inline bool loadStatePrepared = false;
};
}  // namespace Registry
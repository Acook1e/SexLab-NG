#pragma once

#include "Define/Scene.h"

namespace Registry
{

class ActorStat
{
public:
  struct Level
  {
    // level from 0 to 999
    std::uint16_t level = 0;
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

  // 根据 actor 当前 stat 计算初始 enjoyment 值 [-100, 100]
  // sceneTags: 当前场景标签
  // position:  该 actor 分配到的 position
  // others:    场景中其余 actor 列表（用于倾向计算）
  float GetInitialEnjoyment(RE::Actor* actor, const Define::SceneTags& sceneTags,
                            const Define::Position& position,
                            const std::vector<RE::Actor*>& others);

  // 场景结束时统一更新所有 actor 的 stat
  void UpdateOnSceneEnd(const std::vector<RE::Actor*>& actors, const Define::Scene* scene,
                        const std::unordered_map<RE::Actor*, EndSceneRecord>& records);

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
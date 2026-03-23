#pragma once

#include "Define/Enjoyment.h"
#include "Define/Scene.h"

namespace Instance
{
class SceneInstance
{
public:
  struct SceneActorInfo
  {
    Define::Position* position;
    Define::Enjoyment enjoy{};
    std::vector<RE::TESBoundObject*> strippedItems{};

    SceneActorInfo(Define::Position* position) : position(position) {}
  };

  explicit SceneInstance(RE::Actor* central, std::vector<RE::Actor*> participants, std::vector<Define::Scene*> scenes);
  ~SceneInstance();

  bool Update();

  void LockActors();
  void StripActors();
  void ReadyActors();

  void ResetActors();
  void DressActors();
  void UnlockActors();

  Define::Scene* GetCurrentScene() const;
  bool SetStage(std::uint32_t stage);

private:
  std::vector<Define::Scene*> availableScenes;
  std::size_t currentScene;
  std::uint32_t currentStage;
  std::uint64_t lastUpdateTime      = 0;
  std::uint64_t lastStageUpdateTime = 0;

  std::vector<RE::Actor*> actors;
  std::unordered_map<RE::Actor*, SceneActorInfo> actorInfoMap;
};
}  // namespace Instance
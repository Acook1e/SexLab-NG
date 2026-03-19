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
    Define::EnjoyDegree degree;
    float enjoyment;
    const Define::Position* position;
    std::vector<RE::TESBoundObject*> strippedItems;

    SceneActorInfo(Define::EnjoyDegree degree, float enjoyment, const Define::Position& position)
        : degree(degree), enjoyment(enjoyment), position(&position)
    {}
  };

  explicit SceneInstance(RE::Actor* central, std::vector<RE::Actor*> participants,
                         std::vector<const Define::Scene*> scenes);
  ~SceneInstance();

  bool Update();

  void LockActors();
  void UnlockActors();

  void StripActors();
  void DressActors();

  bool NextStage();
  bool PrevStage();

private:
  std::vector<const Define::Scene*> availableScenes;
  const Define::Scene* currentScene;
  std::uint32_t currentStage;
  std::uint64_t lastUpdateTime      = 0;
  std::uint64_t lastStageUpdateTime = 0;

  std::vector<RE::Actor*> actors;
  std::unordered_map<RE::Actor*, SceneActorInfo> actorInfoMap;
};
}  // namespace Instance
#pragma once

#include "Define/Enjoyment.h"
#include "Define/Scene.h"
#include "Instance/Interact.h"

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

  enum class InstanceState : std::uint8_t
  {
    CreateInstance = 0,
    ActorApproach,  // Only used when enable Actor Walk to Center, otherwise will be skipped and
                    // directly set to Ready
    SceneReady,     // Include lock strip and ready
    LeadIn,         // Only used when enable Lead-In, otherwise will be skipped and directly set to
                    // ScenePlay
    ScenePlay,
    DestroyInstance
  };

  SceneInstance(RE::Actor* central, std::vector<RE::Actor*> participants,
                std::vector<Define::Scene*> scenes, Define::Scene* leadIn = nullptr);
  ~SceneInstance();

  bool Update();

  void LockActors();
  void StripActors();
  void ReadyActors();

  void ResetActors();
  void DressActors();
  void UnlockActors();

  Define::Scene* RandomScene();
  void SetPositions();
  bool SetStage(std::uint32_t stage);

  Define::Scene* GetCurrentScene() const { return currentScene; }
  const std::vector<RE::Actor*>& GetActors() const { return actors; }
  const SceneActorInfo& GetActorInfo(RE::Actor* actor) const { return actorInfoMap.at(actor); }
  const Interact& GetInteract() const { return interact; }

private:
  std::vector<Define::Scene*> availableScenes;
  Define::Scene* currentScene;
  std::uint32_t currentStage;

  std::uint64_t lastUpdateTime      = 0;
  std::uint64_t lastStageUpdateTime = 0;

  std::vector<RE::Actor*> actors;
  std::unordered_map<RE::Actor*, SceneActorInfo> actorInfoMap;

  InstanceState state;
  RE::TESObjectREFR* furniture;

  Interact interact;
};
}  // namespace Instance
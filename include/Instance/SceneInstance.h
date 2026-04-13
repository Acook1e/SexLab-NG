#pragma once

#include "Define/Enjoyment.h"
#include "Define/Scene.h"
#include "Instance/Interact.h"

namespace Instance
{
using SceneSearchResult = std::unordered_map<Define::Scene*, std::uint64_t>;

class SceneInstance
{
public:
  struct SceneActorInfo
  {
    Define::Position* position;
    std::vector<RE::TESBoundObject*> strippedItems{};
    // 记录发生过的交互类型，用于修正position的tag
    Define::InteractTags interactTags{};
    std::uint8_t climaxCount = 0;

    SceneActorInfo(Define::Position* position) : position(position) {}
  };

  enum class InstanceState : std::uint8_t
  {
    CreateInstance = 0,
    ActorApproach,  // Only used when enable Actor Walk to Center, otherwise will be skipped and
                    // directly set to Ready
    SceneReady,     // Include lock strip and ready
    ScenePlay,
    // TODO(API): When reaching SceneEnd, allow injecting a new SceneSearchResult and restart
    // ScenePlay without recreating SceneInstance.
    SceneEnd,
    DestroyInstance
  };

  SceneInstance(RE::Actor* central, std::vector<RE::Actor*> participants, SceneSearchResult scenes);
  ~SceneInstance();

  bool Update();

  void LockActors();
  void StripActors();
  void ReadyActors();

  void ResetActors();
  void DressActors();
  void UnlockActors();

  Define::Scene* RandomScene();
  bool SetPositions();
  bool SetStage(std::uint32_t stage);

  Define::Scene* GetCurrentScene() const { return currentScene; }
  const std::vector<RE::Actor*>& GetActors() const { return actors; }
  const SceneActorInfo& GetActorInfo(RE::Actor* actor) const { return actorInfoMap.at(actor); }
  const Interact& GetInteract() const { return interact; }

private:
  SceneSearchResult availableScenes;
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
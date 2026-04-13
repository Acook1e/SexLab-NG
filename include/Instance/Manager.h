#pragma once

#include "Define/AnimPack.h"
#include "Define/Tag.h"
#include "Instance/SceneInstance.h"
#include "Utils/Settings.h"

namespace Instance
{
struct SearchOptions
{
  Define::SceneTags sceneTags = 0;
  std::unordered_map<RE::Actor*, Define::InteractTags> actorInteractTags{};
  bool strictMatchSceneTags    = Settings::bStrictSceneTags;
  bool strictMatchInteractTags = Settings::bStrictInteractTags;
};

class SceneManager
{
public:
  static SceneManager& GetSingleton()
  {
    static SceneManager singleton;
    return singleton;
  }

  SceneSearchResult SearchScenes(std::vector<RE::Actor*> actors, SearchOptions options = {});
  static std::uint64_t CreateInstance(std::vector<RE::Actor*> actors, SceneSearchResult scenes);
  static void DestroyInstance(std::uint64_t id);
  static bool IsActorInScene(RE::Actor* actor);

  void UpdateScenes();

  void AddAnimPack(Define::AnimPack animPack) { animPacks.push_back(std::move(animPack)); }

private:
  std::vector<Define::AnimPack> animPacks;

  static inline std::mutex mapMutex;
  static inline std::unordered_map<std::uint64_t, SceneInstance> sceneInstances;
};
}  // namespace Instance
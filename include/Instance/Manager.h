#pragma once

#include "Define/AnimPack.h"
#include "Instance/SceneInstance.h"

namespace Instance
{
class SceneManager
{
public:
  static SceneManager& GetSingleton()
  {
    static SceneManager singleton;
    return singleton;
  }

  std::vector<const Define::Scene*> SearchScenes(std::vector<RE::Actor*> actors,
                                                 Define::Scene::Type sceneType = Define::Scene::Type::Normal);
  static std::uint64_t CreateInstance(std::vector<RE::Actor*> actors, std::vector<const Define::Scene*> scenes);
  static void DestoryInstance(std::uint64_t id);

  void UpdateScenes();

  void AddAnimPack(Define::AnimPack animPack) { animPacks.push_back(std::move(animPack)); }

private:
  std::vector<Define::AnimPack> animPacks;
  std::unordered_map<std::uint64_t, SceneInstance*> sceneInstances;
};
}  // namespace Instance
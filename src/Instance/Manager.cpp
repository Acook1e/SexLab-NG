#include "Instance/Manager.h"

namespace Instance
{
std::uint64_t SceneManager::CreateInstance(std::vector<RE::Actor*> actors,
                                           std::vector<std::reference_wrapper<Define::Scene>> scenes)
{
  auto* instance =
      new SceneInstance(actors.front(), std::vector<RE::Actor*>(actors.begin() + 1, actors.end()), std::move(scenes));
  std::uint64_t id = reinterpret_cast<std::uint64_t>(instance);
  GetSingleton().sceneInstances.emplace(id, instance);
  return id;
}

void SceneManager::DestoryInstance(std::uint64_t id)
{
  auto& sceneInstances = GetSingleton().sceneInstances;
  auto it              = sceneInstances.find(id);
  if (it != sceneInstances.end()) {
    delete it->second;
    sceneInstances.erase(it);
  }
}

void SceneManager::UpdateScenes()
{
  if (sceneInstances.empty())
    return;

  constexpr static std::uint64_t UPDATE_INTERVAL = 500;  // Update every 500 ms
  static std::uint64_t lastUpdateTime            = 0;
  std::uint64_t currentTime = GetCurrentTime();  // Implement this function to get current time in ms
  if (currentTime - lastUpdateTime < UPDATE_INTERVAL) {
    return;  // Not time to update yet
  }
  lastUpdateTime = currentTime;

  std::vector<std::uint64_t> endedScenes;

  // Update all active scenes
  for (auto& [id, scene] : sceneInstances) {
    if (!scene || !scene->Update()) {
      endedScenes.push_back(id);
    }
  }

  for (auto id : endedScenes)
    DestoryInstance(id);
}
}  // namespace Instance
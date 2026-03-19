#include "Instance/Manager.h"

namespace Instance
{
std::vector<const Define::Scene*> SceneManager::SearchScenes(std::vector<RE::Actor*> actors,
                                                             Define::Scene::Type sceneType)
{
  for (auto* actor : actors)
    if (!actor)
      return {};

  std::vector<const Define::Scene*> res;

  std::unordered_map<RE::Actor*, Define::Gender> genderMap;
  std::unordered_map<RE::Actor*, Define::Race> raceMap;
  for (auto* actor : actors) {
    genderMap.emplace(actor, Define::Gender::GetGender(actor));
    raceMap.emplace(actor, Define::Race::GetRace(actor));
  }

  std::uint64_t racesMask = 0;
  for (auto& [_, race] : raceMap)
    racesMask |= race.Get();

  for (auto& animPack : animPacks) {
    for (const auto& scene : animPack.GetScenes()) {
      if (scene.GetType() != sceneType)
        continue;

      if (!(scene.GetRaces() >= Define::Race(racesMask)))
        continue;

      const auto& positions = scene.GetPositions();
      if (positions.size() != actors.size())
        continue;

      std::vector<bool> positionAssigned(positions.size(), false);
      bool allActorsMatched = true;
      for (auto* actor : actors) {
        const auto& actorRace   = raceMap.at(actor);
        const auto& actorGender = genderMap.at(actor);

        bool matched = false;
        for (std::size_t i = 0; i < positions.size(); ++i) {
          if (positionAssigned[i])
            continue;

          const auto& position = positions[i];
          if (position.GetRace() == actorRace && position.GetGender() == actorGender) {
            positionAssigned[i] = true;
            matched             = true;
            break;
          }
        }

        if (!matched) {
          allActorsMatched = false;
          break;
        }
      }

      if (!allActorsMatched)
        continue;

      res.push_back(&scene);
    }
  }

  return res;
}

std::uint64_t SceneManager::CreateInstance(std::vector<RE::Actor*> actors, std::vector<const Define::Scene*> scenes)
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
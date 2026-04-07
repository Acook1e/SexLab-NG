#include "Instance/Manager.h"

namespace Instance
{
std::vector<Define::Scene*> SceneManager::SearchScenes(std::vector<RE::Actor*> actors,
                                                       Define::Scene::Type sceneType)
{
  ScopeTimer timer("SceneManager::SearchScenes");

  for (auto* actor : actors)
    if (!actor)
      return {};

  std::vector<Define::Scene*> res;

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
    for (auto& scene : animPack.GetScenes()) {
      if (scene.GetType() != sceneType)
        continue;

      if (!(scene.GetRaces() >= Define::Race(racesMask)))
        continue;

      auto& positions = scene.GetPositions();
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

std::uint64_t SceneManager::CreateInstance(std::vector<RE::Actor*> actors,
                                           std::vector<Define::Scene*> scenes)
{
  std::lock_guard<std::mutex> lock(mapMutex);
  static std::mt19937_64 rng{std::random_device{}()};
  std::uint64_t id;
  do
    id = rng();
  while (id == 0 || sceneInstances.contains(id));  // 0 is reserved for invalid ID
  sceneInstances.try_emplace(id, actors.front(),
                             std::vector<RE::Actor*>(actors.begin() + 1, actors.end()),
                             std::move(scenes));
  return id;
}

void SceneManager::DestroyInstance(std::uint64_t id)
{
  std::lock_guard<std::mutex> lock(mapMutex);
  sceneInstances.erase(id);
}

void SceneManager::UpdateScenes()
{
  std::lock_guard<std::mutex> lock(mapMutex);
  for (auto it = sceneInstances.begin(); it != sceneInstances.end();) {
    if (!it->second.Update())
      it = sceneInstances.erase(it);
    else
      ++it;
  }
}
}  // namespace Instance
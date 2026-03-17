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

  void AddAnimPack(Define::AnimPack animPack) { animPacks.push_back(std::move(animPack)); }

  std::vector<std::reference_wrapper<Define::Scene>> SearchScenes(std::vector<RE::Actor*> actors)
  {
    for (auto* actor : actors)
      if (!actor)
        return {};

    std::vector<std::reference_wrapper<Define::Scene>> res;

    std::unordered_map<RE::Actor*, Define::Gender> genderMap;
    std::unordered_map<RE::Actor*, Define::Race> raceMap;
    for (auto* actor : actors) {
      genderMap.emplace(actor, Define::Gender::GetGender(actor));
      raceMap.emplace(actor, Define::Race::GetRace(actor));
    }

    std::uint64_t racesMask = 0;
    for (auto& [_, race] : raceMap)
      racesMask |= race.GetMask();

    std::vector<Define::Gender> actorGenders;
    actorGenders.reserve(actors.size());
    for (auto* actor : actors)
      actorGenders.push_back(genderMap.at(actor));

    for (auto& animPack : animPacks) {
      for (auto& scene : animPack.GetScenes()) {
        if (!scene.IsCompact(static_cast<Define::Race::Type>(racesMask)))
          continue;
        if (!scene.IsGenderCompact(actorGenders))
          continue;
        res.push_back(scene);
      }
    }

    return res;
  }

  static void CreateInstance() {}

  static void DestoryInstance(std::uint64_t id) {}

  void UpdateScenes() {}

private:
  std::vector<Define::AnimPack> animPacks;
  std::unordered_map<std::uint64_t, SceneInstance> sceneInstances;
};
}  // namespace Instance
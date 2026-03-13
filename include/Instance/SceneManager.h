#pragma once

#include "Define/AnimPack.h"

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
  std::vector<std::reference_wrapper<Define::Scene>> SearchScenes();

private:
  std::vector<Define::AnimPack> animPacks;
};
}  // namespace Instance
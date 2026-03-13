#include "Define/Scene.h"

namespace Instance
{
class SceneInstance
{
  SceneInstance(RE::Actor* central, std::vector<RE::Actor*> participants, Define::Scene::Type sceneType, Define::Tags sceneTags);

private:
  std::vector<std::reference_wrapper<Define::Scene>> availableScenes;
  Define::Scene& currentScene;
  Define::Stage& currentStage;
};
}  // namespace Instance
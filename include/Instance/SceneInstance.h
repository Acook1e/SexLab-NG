#include "Define/Enjoyment.h"
#include "Define/Scene.h"

namespace Instance
{
class SceneInstance
{
public:
  struct SceneActorInfo
  {
    Define::EnjoyDegree degree;
    float enjoyment;
    Define::Position& position;
  };

  SceneInstance(RE::Actor* central, std::vector<RE::Actor*> participants, Define::Scene::Type sceneType);

private:
  std::vector<std::reference_wrapper<Define::Scene>> availableScenes;
  Define::Scene& currentScene;
  std::uint32_t currentStage;

  std::vector<RE::Actor*> actors;
  std::unordered_map<RE::Actor*, SceneActorInfo> actorInfoMap;
};
}  // namespace Instance
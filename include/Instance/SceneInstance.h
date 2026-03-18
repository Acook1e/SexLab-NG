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
    std::vector<RE::TESBoundObject*> strippedItems;
  };

  explicit SceneInstance(RE::Actor* central, std::vector<RE::Actor*> participants,
                         std::vector<std::reference_wrapper<Define::Scene>> scenes);
  ~SceneInstance();

  bool Update();

  void LockActors();
  void UnlockActors();

  void StripActors();
  void DressActors();

  bool NextStage();
  bool PrevStage();

private:
  std::vector<std::reference_wrapper<Define::Scene>> availableScenes;
  Define::Scene* currentScene;
  std::uint32_t currentStage;
  std::uint64_t lastStageUpdateTime = 0;

  std::vector<RE::Actor*> actors;
  std::unordered_map<RE::Actor*, SceneActorInfo> actorInfoMap;
};
}  // namespace Instance
#include "Instance/SceneInstance.h"
#include "Instance/SceneManager.h"

namespace Instance
{
class Manager
{
public:
  static Manager& GetSingleton()
  {
    static Manager singleton;
    return singleton;
  }

private:
  std::unordered_map<std::uint64_t, SceneInstance> sceneInstances;
};
}  // namespace Instance
#include "Registry/SceneLoader.h"
#include "Instance/SceneManager.h"
#include "Misc/SlrReader.h"

namespace Registry
{
void SLSB_Loader::LoadSceneData()
{
  constexpr std::string_view DADA_DIR = "Data/SKSE/SexLab/Registry";

  for (const auto& entry : std::filesystem::directory_iterator{std::filesystem::path{DADA_DIR}}) {
    if (entry.is_regular_file() && entry.path().extension() == ".slr") {
      SlrReader::RawPackage pkg = SlrReader::ReadFile(entry.path().string());
      for (const auto& rawScene : pkg.scenes) {
        Define::AnimPack animPack(pkg.name, pkg.author, pkg.hash);
        for (const auto& rawScene : pkg.scenes) {
          Define::Scene scene(rawScene.name, rawScene.id,
                              Define::Furniture(static_cast<Define::Furniture::Type>(rawScene.furniture_types),
                                                {rawScene.furniture_x, rawScene.furniture_y, rawScene.furniture_z, rawScene.furniture_rotation}));
          // Additional scene data population would go here
          animPack.AddScene(scene);
        }
        Instance::SceneManager::GetSingleton().AddAnimPack(animPack);
      }
    }
  }
};

void SLAL_Loader::LoadSceneData()
{
  constexpr std::string_view DADA_DIR = "SLAnims/Json";

  for (const auto& entry : std::filesystem::directory_iterator{std::filesystem::path{DADA_DIR}}) {
    if (entry.is_regular_file() && entry.path().extension() == ".json") {
      // Load and parse the JSON file
      // (Implementation details would go here)
    }
  }
};
}  // namespace Registry
#include "Registry/SceneLoader.h"
#include "Instance/Manager.h"

#include "nlohmann/json.hpp"

namespace Registry::SceneLoader
{

bool LoadFromJson(const std::filesystem::path& path)
{
  nlohmann::json raw;
  try {
    std::ifstream file(path);
    if (!file.is_open()) {
      logger::error("[SexLab NG] Failed to open scene json file: {}", path.string());
      return false;
    }
    file >> raw;
  } catch (const std::exception& e) {
    logger::error("[SexLab NG] Failed to load scene json file: {}: {}", path.string(), e.what());
    return false;
  }

  std::string name        = path.stem().string();
  std::string author      = raw.value("author", "Unknown");
  std::string animPackTag = raw["pack_tag"];
  Define::AnimPack pack(std::move(name), std::move(author), std::move(animPackTag));

  std::vector<Define::Scene> scenes;
  for (const auto& [scene_name, scene_data] : raw["scenes"].items()) {
    std::string event_prefix = scene_data.value("event_prefix", "");
    // TODO: impl
    Define::Furniture furniture(Define::Furniture::Type::BedDouble);
    std::uint32_t total_actors = scene_data.value("total_actors", 0);
    std::uint32_t total_stages = scene_data.value("total_stages", 0);
    Define::Race races         = scene_data.value("races", Define::Race::Type::Unknown);
    if (total_actors == 0 || total_stages == 0) {
      logger::warn("[SexLab NG] Scene {} has invalid total_actors or total_stages, skipping", scene_name);
      continue;
    }

    // Parse actor data
    std::vector<Define::Position> positions;
    positions.reserve(total_actors);
    for (auto i = 1; i <= total_actors; ++i) {
      std::string actor_key = "actor" + std::to_string(i);
      if (!scene_data.contains("positions") || !scene_data["positions"].contains(actor_key)) {
        logger::warn("[SexLab NG] Scene {} is missing data for {}, skipping", scene_name, actor_key);
        break;
      }
      const auto& actor_data = scene_data["positions"][actor_key];
      Define::Position p(actor_data.value("race", Define::Race::Type::Unknown),
                         actor_data.value("gender", Define::Gender::Type::Unknown), 1.0f);
      std::vector<std::string> event;
      event.reserve(total_stages);
      for (auto j = 1; j <= total_stages; ++j) {
        event.push_back(event_prefix + "_A" + std::to_string(i) + "_S" + std::to_string(j));
      }
      p.SetEvents(std::move(event));
      positions.push_back(std::move(p));
    }

    if (positions.empty()) {
      logger::warn("[SexLab NG] Scene {} has no valid actor positions, skipping", scene_name);
      continue;
    }
    Define::Scene scene =
        Define::Scene(std::move(scene_name), std::move(event_prefix), std::move(furniture), std::move(races));
    scene.SetPositions(std::move(positions));
    // logger::info("{}", scene.verbose());
    scenes.push_back(std::move(scene));
  }
  logger::info("[SexLab NG] Loaded AnimPack {} with {} scenes", path.stem().string(), scenes.size());
  pack.SetScenes(std::move(scenes));
  Instance::SceneManager::GetSingleton().AddAnimPack(std::move(pack));
  return true;
}

void SaveToBinary(const Define::AnimPack& pack, const std::filesystem::path& path)
{
  constexpr static std::uint8_t VERSION = 1;
}

bool LoadFromBinary(const std::filesystem::path& path)
{
  return false;
}

void LoadData()
{
  constexpr static std::string_view dataPath = "Data/SKSE/Plugins/SexLabNG/Scenes";

  std::set<std::string> names;
  if (std::filesystem::exists(dataPath)) {
    for (const auto& entry : std::filesystem::directory_iterator(dataPath))
      if (entry.path().extension() == ".json" || entry.path().extension() == ".slng")
        names.insert(entry.path().stem().string());
  } else {
    logger::warn("[SexLab NG] Scene data path does not exist: {}", dataPath);
    return;
  }

  for (const auto& name : names) {
    try {
      auto jsonPath = std::filesystem::path(dataPath) / (name + ".json");
      auto binPath  = std::filesystem::path(dataPath) / (name + ".slng");
      if (std::filesystem::exists(binPath)) {
        if (!LoadFromBinary(binPath))
          logger::warn("[SexLab NG] Failed to load AnimPack {} from binary, fallback to json", name);
        logger::info("[SexLab NG] Fallback loaded AnimPack {} from json {}", name,
                     LoadFromJson(jsonPath) ? "success" : "failed");
      } else {
        if (!LoadFromJson(jsonPath))
          logger::warn("[SexLab NG] Failed to load AnimPack {} from json", name);
      }
    } catch (const std::exception& e) {
      logger::error("[SexLab NG] Failed to load AnimPack {} data: {}", name, e.what());
    }
  }
}

}  // namespace Registry::SceneLoader
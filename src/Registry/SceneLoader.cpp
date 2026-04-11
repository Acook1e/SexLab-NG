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
  std::string animPackTag = raw.value("pack_tag", "");

  std::vector<Define::Scene> scenes;
  for (const auto& [scene_name, scene_data] : raw["scenes"].items()) {
    std::string event_prefix = scene_data.value("event_prefix", "");
    // TODO: impl
    Define::Furniture furniture(Define::Furniture::Type::BedDouble);
    std::uint32_t total_actors = scene_data.value("total_actors", 0);
    std::uint32_t total_stages = scene_data.value("total_stages", 0);
    Define::Race races         = scene_data.value("races", Define::Race::Type::Unknown);
    if (total_actors == 0 || total_stages == 0) {
      logger::warn("[SexLab NG] Scene {} has invalid total_actors or total_stages, skipping",
                   scene_name);
      continue;
    }

    // Parse actor data
    std::vector<Define::Position> positions;
    positions.reserve(total_actors);
    for (auto i = 1; i <= total_actors; ++i) {
      std::string actor_key = "actor" + std::to_string(i);
      if (!scene_data.contains("positions") || !scene_data["positions"].contains(actor_key)) {
        logger::warn("[SexLab NG] Scene {} is missing data for {}, skipping", scene_name,
                     actor_key);
        break;
      }
      const auto& actor_data = scene_data["positions"][actor_key];
      std::vector<std::string> events;
      // TODO: Load offsets and strips from json
      std::vector<Define::Position::Offset> offsets(
          total_stages, Define::Position::Offset{0.0f, 0.0f, 0.0f, 0.0f});
      std::vector<std::int8_t> schlongAngles(total_stages, 0);
      std::vector<Define::StageStrip> strips(total_stages, Define::StageStrip::Type::None);

      if (actor_data.contains("stage_params") && actor_data["stage_params"].is_object())
        for (auto i = 1; i <= total_stages; ++i) {
          std::string stage_key = std::to_string(i);
          if (actor_data["stage_params"].contains(stage_key) &&
              actor_data["stage_params"][stage_key].is_object()) {
            const auto& stage_params = actor_data["stage_params"][stage_key];
            if (stage_params.contains("sos") && stage_params["sos"].is_number_integer())
              schlongAngles[i - 1] = stage_params["sos"].get<std::int8_t>();
          }
        }

      events.reserve(total_stages);
      for (auto j = 1; j <= total_stages; ++j) {
        events.push_back(event_prefix + "_A" + std::to_string(i) + "_S" + std::to_string(j));
      }
      auto position =
          Define::Position(actor_data.value("race", Define::Race::Type::Unknown),
                           actor_data.value("gender", Define::Gender::Type::Unknown),
                           actor_data.value("scale", 1.0f), std::move(events), std::move(offsets),
                           std::move(schlongAngles), std::move(strips), Define::InteractTags(0));
      positions.push_back(std::move(position));
    }

    if (positions.empty()) {
      logger::warn("[SexLab NG] Scene {} has no valid actor positions, skipping", scene_name);
      continue;
    }
    Define::Scene scene =
        Define::Scene(std::move(scene_name), std::move(event_prefix), std::move(furniture),
                      std::move(races), Define::SceneTags(0), std::move(positions));
    // logger::info("{}", scene.verbose());
    scenes.push_back(std::move(scene));
  }
  logger::info("[SexLab NG] Loaded AnimPack {} with {} scenes", path.stem().string(),
               scenes.size());
  Define::AnimPack pack(std::move(name), std::move(author), std::move(animPackTag),
                        std::move(scenes));
  Instance::SceneManager::GetSingleton().AddAnimPack(std::move(pack));
  return true;
}

void LoadData()
{
  ScopeTimer timer("SceneLoader::LoadData");

  constexpr static std::string_view dataPath = "Data/SKSE/Plugins/SexLabNG/Scenes";

  if (std::filesystem::exists(dataPath)) {
    for (const auto& entry : std::filesystem::directory_iterator(dataPath))
      if (entry.path().extension() == ".json") {
        try {
          LoadFromJson(entry.path());
        } catch (const std::exception& e) {
          logger::error("[SexLab NG] Failed to load scene from file: {}: {}", entry.path().string(),
                        e.what());
        }
      }
  } else {
    logger::warn("[SexLab NG] Scene data path does not exist: {}", dataPath);
    return;
  }
}

}  // namespace Registry::SceneLoader
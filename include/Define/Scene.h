#pragma once

#include "Define/Furniture.h"
#include "Define/Position.h"
#include "Define/Tag.h"

namespace Define
{
class Scene
{
public:
  Scene(const std::string& name, const std::string& event_prefix, const Furniture& furniture,
        const Race& races, const SceneTags& tags, std::vector<Position> positions)
      : name(name), furniture(furniture), races(races), tags(tags), positions(std::move(positions))
  {}

  Scene(std::string&& name, std::string&& event_prefix, Furniture&& furniture, Race&& races,
        SceneTags&& tags, std::vector<Position>&& positions)
      : name(std::move(name)), furniture(std::move(furniture)), races(std::move(races)),
        tags(std::move(tags)), positions(std::move(positions))
  {}

  [[nodiscard]] const std::string& GetName() const { return name; }
  [[nodiscard]] const Furniture& GetFurniture() const { return furniture; }
  [[nodiscard]] const Race& GetRaces() const { return races; }
  [[nodiscard]] const SceneTags& GetTags() const { return tags; }
  [[nodiscard]] std::vector<Position>& GetPositions() { return positions; }

  [[nodiscard]] const std::string verbose() const
  {
    std::string res;
    res += "Scene: " + name + "\n";
    res += "Races: " + std::to_string(races.Get()) + "\n";
    res += "Tags: " + std::to_string(tags.Get()) + "\n";
    res += "Positions: " + std::to_string(positions.size()) + "\n";
    for (const auto& position : positions) {
      res += position.verbose();
    }
    return res;
  }

private:
  std::string name;
  Furniture furniture;
  Race races;
  SceneTags tags;
  std::vector<Position> positions;
};
}  // namespace Define
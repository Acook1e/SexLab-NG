#pragma once

#include "Define/Furniture.h"
#include "Define/Position.h"
#include "Define/Tag.h"

namespace Define
{
class Scene
{
public:
  enum Type : std::uint8_t
  {
    LeadIn = 0,
    Normal,
    Aggressive,
    Total
  };

  Scene(const std::string& name, const std::string& event_prefix, const Furniture& furniture,
        const Race& races, Type type, const InteractTags& interactTags,
        std::vector<Position> positions)
      : name(name), furniture(furniture), races(races), type(type), interactTags(interactTags),
        positions(std::move(positions))
  {}

  Scene(std::string&& name, std::string&& event_prefix, Furniture&& furniture, Race&& races,
        Type type, InteractTags&& interactTags, std::vector<Position>&& positions)
      : name(std::move(name)), furniture(std::move(furniture)), races(std::move(races)), type(type),
        interactTags(std::move(interactTags)), positions(std::move(positions))
  {}

  [[nodiscard]] const std::string& GetName() const { return name; }
  [[nodiscard]] const Furniture& GetFurniture() const { return furniture; }
  [[nodiscard]] const Race& GetRaces() const { return races; }
  [[nodiscard]] const Type& GetType() const { return type; }
  [[nodiscard]] const InteractTags& GetInteractTags() const { return interactTags; }
  [[nodiscard]] std::vector<Position>& GetPositions() { return positions; }

  [[nodiscard]] const std::string verbose() const
  {
    std::string res;
    res += "Scene: " + name + "\n";
    res += "Races: " + std::to_string(races.Get()) + "\n";
    res += "InteractTags: " + std::to_string(interactTags.Get()) + "\n";
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
  Type type;
  InteractTags interactTags;
  std::vector<Position> positions;
};
}  // namespace Define
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
    Primary,
    LeatIn,
  };

  Scene(const std::string& name, const std::string& event_prefix, const Furniture& furniture, const Race& races)
      : name(name), event_prefix(event_prefix), furniture(furniture), races(races)
  {}

  Scene(std::string&& name, std::string&& event_prefix, Furniture&& furniture, Race&& races)
      : name(std::move(name)), event_prefix(std::move(event_prefix)), furniture(std::move(furniture)),
        races(std::move(races))
  {}

  bool IsCompact(Race::Type mask) { return races.all(mask); }

  void SetPositions(std::vector<Position> a_positions) { positions = std::move(a_positions); }

  bool IsGenderCompact(const std::vector<Gender>& actorGenders) const
  {
    if (actorGenders.size() != positions.size())
      return false;

    std::vector<bool> used(positions.size(), false);
    return MatchGender(actorGenders, used, 0);
  }

  std::string verbose()
  {
    std::string res;
    res += "Scene: " + name + "\n";
    res += "Races: " + std::to_string(races.GetMask()) + "\n";
    res += "Positions: " + std::to_string(positions.size()) + "\n";
    for (auto& position : positions) {
      res += position.verbose();
    }
    return res;
  }

private:
  bool MatchGender(const std::vector<Gender>& actorGenders, std::vector<bool>& used, std::size_t actorIndex) const
  {
    if (actorIndex >= actorGenders.size())
      return true;

    for (std::size_t i = 0; i < positions.size(); ++i) {
      if (used[i])
        continue;

      if (!positions[i].GetGender().IsCompatible(actorGenders[actorIndex]))
        continue;

      used[i] = true;
      if (MatchGender(actorGenders, used, actorIndex + 1))
        return true;
      used[i] = false;
    }

    return false;
  }

  std::string name;
  std::string event_prefix;
  Furniture furniture;
  Race races;
  Type type;
  InteractTags interactTags;
  std::vector<Position> positions;
};
}  // namespace Define
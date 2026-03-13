#pragma once

#include "Define/Furniture.h"
#include "Define/Stage.h"

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

  Scene(std::string name, std::string id, Furniture furniture) : name(std::move(name)), id(std::move(id)), furniture(std::move(furniture)) {}

private:
  std::string name;
  std::string id;
  Furniture furniture;
  Type type;
  Tags tags;
  std::vector<Stage> stages;
};
}  // namespace Define
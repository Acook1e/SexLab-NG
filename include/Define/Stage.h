#pragma once

#include "Define/Position.h"
#include "Define/Tag.h"

namespace Define
{

class Stage
{
public:
  Stage() = default;

private:
  std::string name;
  std::string id;
  std::vector<Position> positions;
  Tags tags;
};
}  // namespace Define
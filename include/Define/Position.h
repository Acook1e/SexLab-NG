#pragma once

#include "Define/Gender.h"
#include "Define/Race.h"
#include "Define/Strip.h"

namespace Define
{
class Position
{
public:
  Position() = default;

private:
  Race race;
  Gender gender;
  Strip strip;
  std::string event;
  std::array<float, 4> offset{0.0f, 0.0f, 0.0f, 0.0f};
  float schlong{0.0f};
};
}  // namespace Define
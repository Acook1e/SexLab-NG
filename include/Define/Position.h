#pragma once

#include "Define/Gender.h"
#include "Define/Race.h"
#include "Define/Strip.h"

namespace Define
{
class Position
{
public:
  using Offset = std::array<float, 4>;

  Position(const Race& race, const Gender& gender) : race(race), gender(gender) {}
  Position(Race&& race, Gender&& gender) : race(std::move(race)), gender(std::move(gender)) {}

private:
  Race race;
  Gender gender;
  std::vector<Strip> strips;
  std::vector<std::string> events;
  std::vector<Offset> offsets;
  std::vector<std::int8_t> schlongAngles;
};
}  // namespace Define
#pragma once

#include "Define/Gender.h"
#include "Define/Race.h"
#include "Define/Scale.h"
#include "Define/Strip.h"

namespace Define
{
class Position
{
public:
  using Offset = std::array<float, 4>;

  Position(const Race& race, const Gender& gender, const Scale& scale) : race(race), gender(gender), scale(scale) {}
  Position(Race&& race, Gender&& gender, Scale&& scale) : race(std::move(race)), gender(std::move(gender)), scale(std::move(scale)) {}

private:
  Race race;
  Gender gender;
  Scale scale;
  std::vector<Strip> strips;
  std::vector<std::string> events;
  std::vector<Offset> offsets;
  std::vector<std::int8_t> schlongAngles;
};
}  // namespace Define
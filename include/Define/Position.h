#pragma once

#include "Define/Gender.h"
#include "Define/Race.h"
#include "Define/Strip.h"
#include "Utils/Scale.h"

namespace Define
{
class Position
{
public:
  using Offset = std::array<float, 4>;

  Position(const Race& race, const Gender& gender, const float& scale) : race(race), gender(gender), scale(scale) {}
  Position(Race&& race, Gender&& gender, float&& scale)
      : race(std::move(race)), gender(std::move(gender)), scale(std::move(scale))
  {}

  void SetEvents(std::vector<std::string> a_events) { events = std::move(a_events); }
  void SetOffsets(std::vector<Offset> a_offsets) { offsets = std::move(a_offsets); }
  void SetSchlongAngles(std::vector<std::int8_t> a_schlongAngles) { schlongAngles = std::move(a_schlongAngles); }
  void SetStrips(std::vector<Strip> a_strips) { strips = std::move(a_strips); }

  [[nodiscard]] const Race& GetRace() const { return race; }
  [[nodiscard]] const Gender& GetGender() const { return gender; }

  std::string verbose()
  {
    std::string res;
    res += "event_f: " + events.front() + "\n";
    res += "event_b: " + events.back() + "\n";
    return res;
  }

private:
  Race race;
  Gender gender;
  float scale;
  std::vector<std::string> events;
  std::vector<Offset> offsets;
  std::vector<std::int8_t> schlongAngles;
  std::vector<Strip> strips;
};
}  // namespace Define
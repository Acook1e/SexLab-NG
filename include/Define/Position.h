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

  Position(const Race& race, const Gender& gender, const float& scale, const std::vector<std::string>& events)
      : race(race), gender(gender), scale(scale), events(events)
  {}
  Position(Race&& race, Gender&& gender, float&& scale, std::vector<std::string>&& events)
      : race(std::move(race)), gender(std::move(gender)), scale(std::move(scale)), events(std::move(events))
  {}

  [[nodiscard]] const Race& GetRace() const { return race; }
  [[nodiscard]] const Gender& GetGender() const { return gender; }
  [[nodiscard]] const float& GetScale() const { return scale; }
  [[nodiscard]] const std::vector<std::string>& GetEvents() const { return events; }

  [[nodiscard]] const std::vector<Offset>& GetOffsets() const { return offsets; }
  void SetOffsets(std::vector<Offset> a_offsets) { offsets = std::move(a_offsets); }

  [[nodiscard]] const std::vector<std::int8_t>& GetSchlongAngles() const { return schlongAngles; }
  void SetSchlongAngles(std::vector<std::int8_t> a_schlongAngles) { schlongAngles = std::move(a_schlongAngles); }

  [[nodiscard]] const std::vector<StageStrip>& GetStrips() const { return strips; }
  void SetStrips(std::vector<StageStrip> a_strips) { strips = std::move(a_strips); }

  [[nodiscard]] const std::string verbose() const
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
  std::vector<StageStrip> strips;
};
}  // namespace Define
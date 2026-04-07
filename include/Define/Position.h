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

  Position(const Race& race, const Gender& gender, const float& scale,
           const std::vector<std::string>& events, std::vector<Offset> offsets,
           std::vector<std::int8_t> schlongAngles, std::vector<StageStrip> strips)
      : race(race), gender(gender), scale(scale), events(events), offsets(offsets),
        schlongAngles(schlongAngles), strips(strips)
  {}
  Position(Race&& race, Gender&& gender, float&& scale, std::vector<std::string>&& events,
           std::vector<Offset>&& offsets, std::vector<std::int8_t>&& schlongAngles,
           std::vector<StageStrip>&& strips)
      : race(std::move(race)), gender(std::move(gender)), scale(std::move(scale)),
        events(std::move(events)), offsets(std::move(offsets)),
        schlongAngles(std::move(schlongAngles)), strips(std::move(strips))
  {}

  [[nodiscard]] const Race& GetRace() const { return race; }
  [[nodiscard]] const Gender& GetGender() const { return gender; }
  [[nodiscard]] const float& GetScale() const { return scale; }
  [[nodiscard]] const std::vector<std::string>& GetEvents() const { return events; }
  [[nodiscard]] std::vector<Offset>& GetOffsets() { return offsets; }
  [[nodiscard]] std::vector<std::int8_t>& GetSchlongAngles() { return schlongAngles; }
  [[nodiscard]] std::vector<StageStrip>& GetStrips() { return strips; }

  void SetOffset(Offset offset, std::size_t idx) { offsets[idx] = offset; }
  void SetSchlongAngle(std::int8_t angle, std::size_t idx) { schlongAngles[idx] = angle; }
  void SetStrip(StageStrip strip, std::size_t idx) { strips[idx] = strip; }

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
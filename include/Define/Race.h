#pragma once

namespace Define
{
class Race
{
public:
  enum class Type : std::uint64_t
  {
    // Boar, BoarMounted has same behavior graph
    // Chaurus, ChaurusReaper has same behavior graph
    // Dog, Fox, Wolf has same behavior graph
    // Spider, GiantSpider, LargeSpider has same behavior graph
    Unknown          = 0ULL,
    Human            = 1ULL << 0,
    AshHopper        = 1ULL << 1,
    Bear             = 1ULL << 2,
    Boar             = 1ULL << 3,
    BoarMounted      = 1ULL << 4,
    Chaurus          = 1ULL << 5,
    ChaurusHunter    = 1ULL << 6,
    ChaurusReaper    = 1ULL << 7,
    Chicken          = 1ULL << 8,
    Cow              = 1ULL << 9,
    Deer             = 1ULL << 10,
    Dog              = 1ULL << 11,
    Dragon           = 1ULL << 12,
    DragonPriest     = 1ULL << 13,
    Draugr           = 1ULL << 14,
    DwarvenBallista  = 1ULL << 15,
    DwarvenCenturion = 1ULL << 16,
    DwarvenSphere    = 1ULL << 17,
    DwarvenSpider    = 1ULL << 18,
    Falmer           = 1ULL << 19,
    FlameAtronach    = 1ULL << 20,
    Fox              = 1ULL << 21,
    FrostAtronach    = 1ULL << 22,
    Gargoyle         = 1ULL << 23,
    Giant            = 1ULL << 24,
    GiantSpider      = 1ULL << 25,
    Goat             = 1ULL << 26,
    Hagraven         = 1ULL << 27,
    Hare             = 1ULL << 28,
    Horker           = 1ULL << 29,
    Horse            = 1ULL << 30,
    IceWraith        = 1ULL << 31,
    LargeSpider      = 1ULL << 32,
    Lurker           = 1ULL << 33,
    Mammoth          = 1ULL << 34,
    Mudcrab          = 1ULL << 35,
    Netch            = 1ULL << 36,
    Riekling         = 1ULL << 37,
    Sabrecat         = 1ULL << 38,
    Seeker           = 1ULL << 39,
    Skeever          = 1ULL << 40,
    Slaughterfish    = 1ULL << 41,
    Spider           = 1ULL << 42,
    Spriggan         = 1ULL << 43,
    StormAtronach    = 1ULL << 44,
    Troll            = 1ULL << 45,
    VampireLord      = 1ULL << 46,
    Werewolf         = 1ULL << 47,
    Wisp             = 1ULL << 48,
    Wispmother       = 1ULL << 49,
    Wolf             = 1ULL << 50
  };

  Race(RE::Actor* actor) : type(static_cast<std::uint64_t>(GetRace(actor))) {}
  Race(Type type) : type(static_cast<std::uint64_t>(type)) {}
  Race(std::uint64_t mask) : type(mask) {}

  [[nodiscard]] const std::uint64_t Get() const { return type.to_ullong(); }

  [[nodiscard]] bool operator==(const Race& other) const { return type == other.type; }
  [[nodiscard]] bool operator!=(const Race& other) const { return type != other.type; }
  [[nodiscard]] bool operator>=(const Race& other) const { return (type & other.type) == other.type; }
  [[nodiscard]] bool operator<=(const Race& other) const { return (other.type & type) == type; }

  [[nodiscard]] static Type GetRace(RE::Actor* actor);

  [[nodiscard]] bool IsHuman();
  [[nodiscard]] bool HasSchlong();

private:
  std::bitset<sizeof(Type) * CHAR_BIT> type;
};
}  // namespace Define
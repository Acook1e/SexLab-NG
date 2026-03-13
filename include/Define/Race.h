#pragma once

namespace Define
{
class Race
{
public:
  enum class Type : std::uint8_t
  {
    Unknown = 0,
    Human   = 1,
    AshHopper,
    Bear,
    BoarAny,
    BoarMounted,
    BoarSingle,
    Canine,
    Chaurus,
    ChaurusHunter,
    ChaurusReaper,
    Chicken,
    Cow,
    Deer,
    Dog,
    Dragon,
    DragonPriest,
    Draugr,
    DwarvenBallista,
    DwarvenCenturion,
    DwarvenSphere,
    DwarvenSpider,
    Falmer,
    FlameAtronach,
    Fox,
    FrostAtronach,
    Gargoyle,
    Giant,
    GiantSpider,
    Goat,
    Hagraven,
    Hare,
    Horker,
    Horse,
    IceWraith,
    LargeSpider,
    Lurker,
    Mammoth,
    Mudcrab,
    Netch,
    Riekling,
    Sabrecat,
    Seeker,
    Skeever,
    Slaughterfish,
    Spider,
    Spriggan,
    StormAtronach,
    Troll,
    VampireLord,
    Werewolf,
    Wisp,
    Wispmother,
    Wolf,
  };

  Race(RE::Actor* actor);

  static Type GetRace(RE::Actor* actor);

private:
  Type type{Type::Unknown};
};
}  // namespace Define
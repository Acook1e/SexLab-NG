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

  Race(RE::Actor* actor) : type(GetRace(actor)) {}
  Race(Type type) : type(type) {}
  Race(std::uint64_t mask) : type(static_cast<Type>(mask)) {}

  bool operator==(const Race& other) const { return type == other.type; }
  bool operator!=(const Race& other) const { return !(*this == other); }

  static Type GetRace(RE::Actor* actor)
  {
    static std::unordered_map<std::string, Type> raceMap = {
        {"0_Master.hkx", Type::Human},
        {"WolfBehavior.hkx", Type::Wolf},
        {"DogBehavior.hkx", Type::Dog},
        {"ChickenBehavior.hkx", Type::Chicken},
        {"HareBehavior.hkx", Type::Hare},
        {"AtronachFlameBehavior.hkx", Type::FlameAtronach},
        {"AtronachFrostBehavior.hkx", Type::FrostAtronach},
        {"AtronachStormBehavior.hkx", Type::StormAtronach},
        {"BearBehavior.hkx", Type::Bear},
        {"ChaurusBehavior.hkx", Type::Chaurus},
        {"H-CowBehavior.hkx", Type::Cow},
        {"DeerBehavior.hkx", Type::Deer},
        {"CHaurusFlyerBehavior.hkx", Type::ChaurusHunter},
        {"VampireBruteBehavior.hkx", Type::Gargoyle},
        {"BenthicLurkerBehavior.hkx", Type::Lurker},
        {"BoarBehavior.hkx", Type::Boar},
        {"BCBehavior.hkx", Type::DwarvenBallista},
        {"HMDaedra.hkx", Type::Seeker},
        {"NetchBehavior.hkx", Type::Netch},
        {"RieklingBehavior.hkx", Type::Riekling},
        {"ScribBehavior.hkx", Type::AshHopper},
        {"DragonBehavior.hkx", Type::Dragon},
        {"Dragon_Priest.hkx", Type::DragonPriest},
        {"DraugrBehavior.hkx", Type::Draugr},
        {"SCBehavior.hkx", Type::DwarvenSphere},
        {"DwarvenSpiderBehavior.hkx", Type::DwarvenSpider},
        {"SteamBehavior.hkx", Type::DwarvenCenturion},
        {"FalmerBehavior.hkx", Type::Falmer},
        {"FrostbiteSpiderBehavior.hkx", Type::Spider},
        {"GiantBehavior.hkx", Type::Giant},
        {"GoatBehavior.hkx", Type::Goat},
        {"HavgravenBehavior.hkx", Type::Hagraven},
        {"HorkerBehavior.hkx", Type::Horker},
        {"HorseBehavior.hkx", Type::Horse},
        {"IceWraithBehavior.hkx", Type::IceWraith},
        {"MammothBehavior.hkx", Type::Mammoth},
        {"MudcrabBehavior.hkx", Type::Mudcrab},
        {"SabreCatBehavior.hkx", Type::Sabrecat},
        {"SkeeverBehavior.hkx", Type::Skeever},
        {"SlaughterfishBehavior.hkx", Type::Slaughterfish},
        {"SprigganBehavior.hkx", Type::Spriggan},
        {"TrollBehavior.hkx", Type::Troll},
        {"VampireLord.hkx", Type::VampireLord},
        {"WerewolfBehavior.hkx", Type::Werewolf},
        {"WispBehavior.hkx", Type::Wispmother},
        {"WitchlightBehavior.hkx", Type::Wisp},
    };

    auto behaviorPath = actor->GetRace()->rootBehaviorGraphNames[actor->GetActorBase()->IsFemale() ? 1 : 0].data();
    auto behaviorName = std::filesystem::path(behaviorPath).filename().string();

    auto res = Type::Unknown;
    if (auto it = raceMap.find(behaviorName); it != raceMap.end()) {
      res = it->second;
    } else {
      logger::warn("[SexLab NG] Unknown behavior graph: {}", behaviorName);
      return Type::Unknown;
    }

    if (res == Type::Human)
      return res;

    auto editorId = std::string{actor->GetRace()->GetFormEditorID()};
    switch (res) {
    case Type::Boar:
      static const auto DLC2RieklingMountedKeyword =
          RE::TESDataHandler::GetSingleton()->LookupForm<RE::BGSKeyword>(0x03A159, "Dragonborn.esm");
      if (actor->GetRace()->HasKeyword(DLC2RieklingMountedKeyword))
        res = Type::BoarMounted;
      break;
    case Type::Chaurus:
      if (editorId.find("reaper") != std::string::npos)
        res = Type::ChaurusReaper;
      else
        res = Type::Chaurus;
      break;
    case Type::Spider:
      if (editorId.find("giant") != std::string::npos)
        res = Type::GiantSpider;
      else if (editorId.find("large") != std::string::npos)
        res = Type::LargeSpider;
      break;
    case Type::Wolf:
      if (editorId.find("fox") != std::string::npos)
        res = Type::Fox;
      break;
    default:
      break;
    }
    return res;
  }

  static bool IsHuman(RE::Actor* actor) { return GetRace(actor) == Type::Human; }

private:
  REX::EnumSet<Type> type;
};
}  // namespace Define
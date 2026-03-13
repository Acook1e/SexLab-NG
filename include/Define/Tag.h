#pragma once

namespace Define
{
class Tags
{
public:
  enum class SLSB : uint64_t
  {
    SixtyNine         = 1ULL << 0,
    Anal              = 1ULL << 1,
    Asphyxiation      = 1ULL << 2,
    Blowjob           = 1ULL << 3,
    Boobjob           = 1ULL << 4,
    BreastSucking     = 1ULL << 5,
    Buttjob           = 1ULL << 6,
    Cowgirl           = 1ULL << 7,
    Cunnilingus       = 1ULL << 8,
    Deepthroat        = 1ULL << 9,
    Doggy             = 1ULL << 10,
    Dominant          = 1ULL << 11,
    DoublePenetration = 1ULL << 12,
    FaceSitting       = 1ULL << 13,
    Facial            = 1ULL << 14,
    Feet              = 1ULL << 15,
    Fingering         = 1ULL << 16,
    Fisting           = 1ULL << 17,
    Footjob           = 1ULL << 18,
    Forced            = 1ULL << 19,
    Grinding          = 1ULL << 20,
    Handjob           = 1ULL << 21,
    Humiliation       = 1ULL << 22,
    LeadIn            = 1ULL << 23,
    LotusPosition     = 1ULL << 24,
    Masturbation      = 1ULL << 25,
    Missionary        = 1ULL << 26,
    Oral              = 1ULL << 27,
    Penetration       = 1ULL << 28,
    ProneBone         = 1ULL << 29,
    ReverseCowgirl    = 1ULL << 30,
    ReverseSpitroast  = 1ULL << 31,
    Rimming           = 1ULL << 32,
    Spanking          = 1ULL << 33,
    Spitroast         = 1ULL << 34,
    Teasing           = 1ULL << 35,
    Toys              = 1ULL << 36,
    Tribadism         = 1ULL << 37,
    TriplePenetration = 1ULL << 38,
    Vaginal           = 1ULL << 39,

    Behind   = 1ULL << 40,
    Facing   = 1ULL << 41,
    Holding  = 1ULL << 42,
    Hugging  = 1ULL << 43,
    Kissing  = 1ULL << 44,
    Kneeling = 1ULL << 45,
    Loving   = 1ULL << 46,
    Lying    = 1ULL << 47,
    Magic    = 1ULL << 48,
    Sitting  = 1ULL << 49,
    Spooning = 1ULL << 50,
    Standing = 1ULL << 51,

    Ryona       = 1ULL << 52,
    Gore        = 1ULL << 53,
    Oviposition = 1ULL << 54,
  };

  Tags() = default;

  template <typename E>
  bool HasTag(E tag) const
  {
    if (std::is_same_v<SLSB, E>())
      return slsbTags.all(tag);
  }

  template <typename E>
  void AddTag(E tag)
  {
    if (std::is_same_v<SLSB, E>())
      slsbTags.set(tag, true);
  }

  template <typename E>
  void RemoveTag(E tag)
  {
    if (std::is_same_v<SLSB, E>())
      slsbTags.set(tag, false);
  }

private:
  REX::EnumSet<SLSB> slsbTags;
};
}  // namespace Define
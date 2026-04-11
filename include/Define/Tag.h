#pragma once

namespace Define
{

// ═══════════════════════════════════════════════════
// Position Tag — 严格由 Interact::Type 驱动
// ═══════════════════════════════════════════════════
class InteractTags
{
public:
  enum class Type : std::uint32_t
  {
    // ── 单部位交互（直接映射 Interact::Type）──
    Kiss          = 1U << 0,
    BreastSucking = 1U << 1,
    ToeSucking    = 1U << 2,
    Cunnilingus   = 1U << 3,
    Anilingus     = 1U << 4,
    Fellatio      = 1U << 5,
    DeepThroat    = 1U << 6,
    MouthAnimObj  = 1U << 7,
    GropeBreast   = 1U << 8,
    Titfuck       = 1U << 9,
    FingerVagina  = 1U << 10,
    FingerAnus    = 1U << 11,
    Handjob       = 1U << 12,
    Masturbation  = 1U << 13,
    HandAnimObj   = 1U << 14,
    Naveljob      = 1U << 15,
    Thighjob      = 1U << 16,
    Frottage      = 1U << 17,
    Footjob       = 1U << 18,
    Tribbing      = 1U << 19,
    Vaginal       = 1U << 20,
    VaginaAnimObj = 1U << 21,
    Anal          = 1U << 22,
    AnalAnimObj   = 1U << 23,
    PenisAnimObj  = 1U << 24,

    // ── 多 Actor 组合（Interact Phase 5 推导）──
    SixtyNine         = 1U << 25,
    Spitroast         = 1U << 26,
    DoublePenetration = 1U << 27,
    TriplePenetration = 1U << 28,
  };
  // 29/32 used, 3 bits 余量

  InteractTags() : tags(0) {}
  InteractTags(Type t) : tags(static_cast<std::uint32_t>(t)) {}
  InteractTags(std::uint32_t t) : tags(t) {}

  void Set(Type t) { tags |= std::bitset<32>(static_cast<std::uint32_t>(t)); }
  void Clear(Type t) { tags &= ~std::bitset<32>(static_cast<std::uint32_t>(t)); }
  [[nodiscard]] bool Has(Type t) const
  {
    return (tags & std::bitset<32>(static_cast<std::uint32_t>(t))).any();
  }

  [[nodiscard]] std::uint32_t Get() const { return static_cast<std::uint32_t>(tags.to_ulong()); }

  [[nodiscard]] bool operator==(const InteractTags& o) const { return tags == o.tags; }
  [[nodiscard]] bool operator!=(const InteractTags& o) const { return tags != o.tags; }

private:
  std::bitset<32> tags;
};

// ═══════════════════════════════════════════════════
// Scene Tag — 预处理器赋值，运行时不变
// ═══════════════════════════════════════════════════
class SceneTags
{
public:
  enum class Type : std::uint64_t
  {
    // ── 场景类型 ──
    LeadIn     = 1ULL << 0,
    Aggressive = 1ULL << 1,

    // ── 场景氛围/性质 ──
    Forced      = 1ULL << 2,
    Dominant    = 1ULL << 3,
    Humiliation = 1ULL << 4,
    Loving      = 1ULL << 5,
    Teasing     = 1ULL << 6,

    // ── 体位（预处理器根据动画标注）──
    Cowgirl        = 1ULL << 7,
    ReverseCowgirl = 1ULL << 8,
    Missionary     = 1ULL << 9,
    Doggy          = 1ULL << 10,
    ProneBone      = 1ULL << 11,
    LotusPosition  = 1ULL << 12,
    Spooning       = 1ULL << 13,
    FaceSitting    = 1ULL << 14,

    // ── 姿态 ──
    Standing = 1ULL << 15,
    Kneeling = 1ULL << 16,
    Lying    = 1ULL << 17,
    Sitting  = 1ULL << 18,
    Behind   = 1ULL << 19,
    Facing   = 1ULL << 20,
    Holding  = 1ULL << 21,
    Hugging  = 1ULL << 22,

    // ── 特殊 / 分类 ──
    Fisting     = 1ULL << 23,
    Spanking    = 1ULL << 24,
    Toys        = 1ULL << 25,
    Magic       = 1ULL << 26,
    Facial      = 1ULL << 27,
    Ryona       = 1ULL << 28,
    Gore        = 1ULL << 29,
    Oviposition = 1ULL << 30,

    // ── 聚合/父类标签（查询便利）──
    Oral    = 1ULL << 31,  // 预处理器：场景含 Fellatio/Cunnilingus/Anilingus 时自动设置
    Vaginal = 1ULL << 32,  // 预处理器：场景含 Vaginal 时自动设置
    Anal    = 1ULL << 33,  // 预处理器：场景含 Anal 时自动设置
    Feet    = 1ULL << 34,  // 预处理器：场景含 Footjob/ToeSucking 时自动设置
  };
  // 35/64 used, 29 bits 余量

  SceneTags() : tags(0) {}
  SceneTags(Type t) : tags(static_cast<std::uint64_t>(t)) {}
  SceneTags(std::uint64_t t) : tags(t) {}

  void Set(Type t) { tags |= std::bitset<64>(static_cast<std::uint64_t>(t)); }
  void Clear(Type t) { tags &= ~std::bitset<64>(static_cast<std::uint64_t>(t)); }
  [[nodiscard]] bool Has(Type t) const
  {
    return (tags & std::bitset<64>(static_cast<std::uint64_t>(t))).any();
  }

  [[nodiscard]] std::uint64_t Get() const { return tags.to_ullong(); }

  [[nodiscard]] bool operator==(const SceneTags& o) const { return tags == o.tags; }
  [[nodiscard]] bool operator!=(const SceneTags& o) const { return tags != o.tags; }

private:
  std::bitset<64> tags;
};

class CustomTags
{
public:
  CustomTags() : tags(0) {}
  CustomTags(const std::vector<std::string>& tags) : tags(tags) {}
  CustomTags(std::vector<std::string>&& tags) : tags(std::move(tags)) {}

  bool Has(const std::string& tag) const
  {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
  }

private:
  std::vector<std::string> tags;
};

}  // namespace Define
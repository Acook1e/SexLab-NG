#pragma once

namespace Define
{
class Furniture
{
public:
  enum class Type : std::uint32_t
  {
    None = 0,

    BedRoll   = 1 << 0,
    BedSingle = 1 << 1,
    BedDouble = 1 << 2,

    Wall    = 1 << 3,
    Railing = 1 << 4,

    CraftCookingPot = 1 << 5,
    CraftAlchemy    = 1 << 6,
    CraftEnchanting = 1 << 7,
    CraftSmithing   = 1 << 8,
    CraftAnvil      = 1 << 9,
    CraftWorkbench  = 1 << 10,
    CraftGrindstone = 1 << 11,

    Table        = 1 << 12,
    TableCounter = 1 << 13,

    Chair       = 1 << 14,  // No arm, high back
    ChairCommon = 1 << 15,  // Common chair
    ChairWood   = 1 << 16,  // Wooden Chair
    ChairBar    = 1 << 17,  // Bar stool
    ChairNoble  = 1 << 18,  // Noble Chair
    ChairMisc   = 1 << 19,  // Unspecified

    Bench      = 1 << 20,  // With back
    BenchNoble = 1 << 21,  // Noble Bench (no back, with arm)
    BenchMisc  = 1 << 20,  // No specification on back or arm

    Throne       = 1 << 22,
    ThroneRiften = 1 << 23,
    ThroneNordic = 1 << 24,
    // COMEBACK: These might want to be removed?
    XCross  = 1 << 25,
    Pillory = 1 << 26,
  };

  Furniture(Type type, std::array<float, 4> offset = {0.0f, 0.0f, 0.0f, 0.0f})
      : type(type), offset(offset)
  {}

  bool IsType(Type a_type) const { return type.all(a_type); }

private:
  REX::EnumSet<Type> type;
  std::array<float, 4> offset{0.0f, 0.0f, 0.0f, 0.0f};
};
}  // namespace Define
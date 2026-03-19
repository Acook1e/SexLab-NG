#pragma once

namespace Define
{
class StageStrip
{
public:
  enum class Type : uint8_t
  {
    None   = 0,
    Helmet = 1,
    Body   = 2,
    Gloves = 3,
    Boots  = 4,
    All    = 5,
  };

  StageStrip(Type type) : type(type) {}
  [[nodiscard]] const Type& Get() const { return type; }

  [[nodiscard]] bool operator==(const StageStrip& other) const { return type == other.type; }
  [[nodiscard]] bool operator!=(const StageStrip& other) const { return type != other.type; }

private:
  Type type;
};

struct ItemStrip
{
  enum class Type : uint8_t
  {
    Default = 0,
    Always,
    Never,
  };

  [[nodiscard]] const Type Get(RE::FormID form) const
  {
    // TODO implement item stripping logic based on form ID and strip type
    return Type::Default;
  }
};
}  // namespace Define
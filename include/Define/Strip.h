#pragma once

namespace Define
{
class Strip
{
public:
  enum class Type : uint8_t
  {
    None   = 0,
    Helmet = 1 << 0,
    Body   = 1 << 1,
    Gloves = 1 << 2,
    Boots  = 1 << 3,
    All    = 1 << 4,
  };

  Strip(Type type) { types.set(type); }

  bool IsType(Type a_type) const { return types.all(a_type); }

private:
  REX::EnumSet<Type> types;
};
}  // namespace Define
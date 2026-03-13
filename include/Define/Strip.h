#pragma once

namespace Define
{
class Strip
{
public:
  enum class Type : uint8_t
  {
    Default = 1 << 0,
    All     = 1 << 1,
    None    = 1 << 2,
    Helmet  = 1 << 3,
    Gloves  = 1 << 4,
    Boots   = 1 << 5
  };

  Strip(Type type) { types.set(type); }

  bool IsType(Type a_type) const { return types.all(a_type); }

private:
  REX::EnumSet<Type> types;
};
}  // namespace Define
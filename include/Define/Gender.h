#pragma once

namespace Define
{
class Gender
{
public:
  enum class Type : uint8_t
  {
    Unknown = 0,
    Female  = 1,
    Male,
    Futa
  };

  Gender(RE::Actor* actor);

  static Type GetGender(RE::Actor* actor);

private:
  Type type;
};
}  // namespace Define
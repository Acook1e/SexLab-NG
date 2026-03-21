#pragma once

#include "Define/Race.h"

namespace Define
{
class Gender
{
public:
  enum class Type : std::uint8_t
  {
    Unknown = 0,
    Female  = 1,
    Male,
    Futa,
    CreatureMale,
    CreatureFemale,
  };

  Gender(RE::Actor* actor) : type(GetGender(actor)) {}
  Gender(Type type) : type(type) {}

  [[nodiscard]] bool operator==(const Gender& other) const { return type == other.type; }
  [[nodiscard]] bool operator!=(const Gender& other) const { return type != other.type; }

  [[nodiscard]] const Type& Get() const { return type; }

  [[nodiscard]] bool HasPenis() const { return type == Type::Male || type == Type::Futa || type == Type::CreatureMale; }

  [[nodiscard]] static Type GetGender(RE::Actor* actor)
  {
    bool isFemale = actor->GetActorBase()->IsFemale();
    if (Define::Race(actor).IsHuman()) {
      return isFemale ? (IsFuta(actor) ? Type::Futa : Type::Female) : Type::Male;
    } else
      return isFemale ? Type::CreatureFemale : Type::CreatureMale;
  }

  [[nodiscard]] static bool IsFuta(RE::Actor* actor)
  {

    static const auto TNGKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("TNG_SkinWithPenis");
    static const auto SOSfaction =
        RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESFaction>(0x00AFF8, "Schlongs of Skyrim.esp");

    if (TNGKeyword)
      return actor->GetSkin()->HasKeyword(TNGKeyword);

    if (SOSfaction)
      return actor->IsInFaction(SOSfaction);

    return false;
  }

private:
  Type type;
};
}  // namespace Define
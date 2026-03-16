#pragma once

#include "Define/Race.h"

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
    Futa,
    CreatureMale,
    CreatureFemale,
  };

  Gender(RE::Actor* actor) : type(GetGender(actor)) {}
  Gender(Type type) : type(type) {}

  bool operator==(const Gender& other) const { return type == other.type; }
  bool operator!=(const Gender& other) const { return !(*this == other); }

  static Type GetGender(RE::Actor* actor)
  {
    bool isFemale = actor->GetActorBase()->IsFemale();
    if (Race::IsHuman(actor)) {
      return isFemale ? (IsFuta(actor) ? Type::Futa : Type::Female) : Type::Male;
    } else
      return isFemale ? Type::CreatureFemale : Type::CreatureMale;
  }

  static bool IsFuta(RE::Actor* actor)
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
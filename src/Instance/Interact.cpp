#include "Instance/Interact.h"

#include "magic_enum/magic_enum.hpp"
#include "magic_enum/magic_enum_flags.hpp"

namespace Instance
{

/*
static const std::unordered_map<Interact::BodyPart, Interact::Rule> rules{
  {Interact::BodyPart::Mouth, {Interact::Type::Kiss, 2.0f, false}},
  {Interact::BodyPart::Mouth | Interact::BodyPart::FootLeft, {Interact::Type::ToeSucking, 2.0f, false}},
  {Interact::BodyPart::Mouth | Interact::BodyPart::Vagina, {Interact::Type::Cunnilingus, 2.0f, false}},
  {Interact::BodyPart::Mouth | Interact::BodyPart::Anus, {Interact::Type::Anilingus, 2.0f, false}},
  {Interact::BodyPart::Mouth | Interact::BodyPart::Penis, {Interact::Type::Fellatio, 2.0f, true}},
  {Interact::BodyPart::BreastLeft | Interact::BodyPart::HandLeft, {Interact::Type::GropeBreast, 2.0f, false}},
  {Interact::BodyPart::BreastLeft | Interact::BodyPart::Penis, {Interact::Type::Titfuck, 2.0f, true}},
  {Interact::BodyPart::HandLeft | Interact::BodyPart::Vagina, {Interact::Type::FingerVagina, 2.0f, false}},
  {Interact::BodyPart::HandLeft | Interact::BodyPart::Anus, {Interact::Type::FingerAnus, 2.0f, false}},
  {Interact::BodyPart::HandLeft | Interact::BodyPart::Penis, {Interact::Type::Handjob, 2.0f, true}},
  {Interact::BodyPart::Belly | Interact::BodyPart::Penis, {Interact::Type::Naveljob, 2.0f, true}},
  {Interact::BodyPart::ThighLeft | Interact::BodyPart::Penis, {Interact::Type::Thighjob, 2.0f, true}},
  {Interact::BodyPart::ButtLeft | Interact::BodyPart::Penis, {Interact::Type::Frottage, 2.0f, true}},
  {Interact::BodyPart::FootLeft | Interact::BodyPart::Penis, {Interact::Type::Footjob, 2.0f, true}},
  {Interact::BodyPart::Vagina, {Interact::Type::Tribbing, 2.0f, false}},
  {Interact::BodyPart::Vagina | Interact::BodyPart::Penis, {Interact::Type::Vaginal, 2.0f, true}},
  {Interact::BodyPart::Anus | Interact::BodyPart::Penis, {Interact::Type::Anal, 2.0f, true}},
};
*/

const Interact::Data& Interact::GetData(RE::Actor* actor) const
{
  static const Interact::Data EmptyData{};
  if (auto it = datas.find(actor); it != datas.end())
    return it->second;
  return EmptyData;
}

const Interact::Info& Interact::GetInfo(RE::Actor* actor, Define::BodyPart::Name bodyPart) const
{
  static const Interact::Info EmptyInfo{};
  if (auto dataIt = datas.find(actor); dataIt != datas.end())
    if (auto infoIt = dataIt->second.bodyParts.find(bodyPart); infoIt != dataIt->second.bodyParts.end())
      return infoIt->second;
  return EmptyInfo;
}

}  // namespace Instance
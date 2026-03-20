#pragma once

#include "API/SKEE.h"

namespace Instance
{
class Scale
{
public:
  static Scale& GetSingleton()
  {
    static Scale instance;
    return instance;
  }

  Scale()
  {
    SKEE::InterfaceExchangeMessage msg;
    const auto* const intfc{SKSE::GetMessagingInterface()};
    intfc->Dispatch(SKEE::InterfaceExchangeMessage::kExchangeInterface, &msg, sizeof(SKEE::InterfaceExchangeMessage*),
                    "skee");
    if (!msg.interfaceMap) {
      logger::critical("[SexLab NG] Scale : Couldn't get interface!");
      return;
    }
    transformInterface =
        reinterpret_cast<SKEE::INiTransformInterface*>(msg.interfaceMap->QueryInterface("NiTransform"));
    if (!transformInterface) {
      logger::critical("[SexLab NG] Scale : Couldn't get NiTransformInterface!");
      return;
    }
    logger::info("[SexLab NG] Scale : Successfully obtained NiTransformInterface!");
  }

  static float CalculateScale(RE::Actor* actor)
  {
    if (!actor)
      return 1.0f;

    auto node = actor->GetNodeByName(baseNode);
    return actor->GetScale() * (node ? node->local.scale : 1.0f);
  }

  void ApplyScale(RE::Actor* actor, float scale)
  {
    if (!transformInterface)
      return;

    if (!actor)
      return;

    auto basescale = CalculateScale(actor);
    auto isFemale  = actor->GetActorBase()->IsFemale();
    // multiply scale mode
    transformInterface->AddNodeTransformScaleMode(actor, false, isFemale, baseNode.data(), "SexLabNG", 0);

    // remove existing scale and update base scale
    if (transformInterface->RemoveNodeTransformScale(actor, false, isFemale, baseNode.data(), "SexLabNG")) {
      transformInterface->UpdateNodeTransforms(actor, false, isFemale, baseNode.data());
      basescale = CalculateScale(actor);
    }

    // calculate multiplicative scale factor and apply
    float k = scale / basescale;

    transformInterface->AddNodeTransformScale(actor, false, isFemale, baseNode.data(), "SexLabNG", k);
    transformInterface->UpdateNodeTransforms(actor, false, isFemale, baseNode.data());
  }

  void RemoveScale(RE::Actor* actor)
  {
    if (!transformInterface)
      return;

    if (!actor)
      return;

    auto isFemale = actor->GetActorBase()->IsFemale();
    if (transformInterface->RemoveNodeTransformScale(actor, false, isFemale, baseNode.data(), "SexLabNG")) {
      transformInterface->UpdateNodeTransforms(actor, false, isFemale, baseNode.data());
    }
  }

private:
  constexpr static inline std::string_view baseNode = "NPC";
  SKEE::INiTransformInterface* transformInterface   = nullptr;
};
}  // namespace Instance
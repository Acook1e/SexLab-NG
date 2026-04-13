#include "Papyrus/SexLab.h"

#include "Registry/CumFx.h"

namespace Papyrus
{

namespace
{
  constexpr std::string_view kCumFxScriptName = "SexLabNGCumFx";

  std::optional<Registry::CumFx::Type> ToCumFxType(std::int32_t type)
  {
    if (type < 0 || type >= static_cast<std::int32_t>(Registry::CumFx::Type::Total))
      return std::nullopt;
    return static_cast<Registry::CumFx::Type>(type);
  }

  bool IsAvailable(RE::StaticFunctionTag*)
  {
    return Registry::CumFx::GetSingleton().IsAvailable();
  }

  bool RegisterFxSet(RE::StaticFunctionTag*, std::int32_t type, std::string setName,
                     std::int32_t maxLayer, float weight)
  {
    const auto fxType = ToCumFxType(type);
    if (!fxType || maxLayer <= 0)
      return false;

    return Registry::CumFx::GetSingleton().RegisterSet(*fxType, std::move(setName),
                                                       static_cast<std::uint8_t>(maxLayer), weight);
  }

  bool BeginOverlay(RE::StaticFunctionTag*, RE::Actor* actor, std::int32_t type)
  {
    const auto fxType = ToCumFxType(type);
    return fxType && Registry::CumFx::GetSingleton().BeginOverlay(actor, *fxType);
  }

  bool ClearOverlay(RE::StaticFunctionTag*, RE::Actor* actor, std::int32_t type)
  {
    const auto fxType = ToCumFxType(type);
    return fxType && Registry::CumFx::GetSingleton().ClearOverlay(actor, *fxType);
  }

  bool ClearAllOverlays(RE::StaticFunctionTag*, RE::Actor* actor)
  {
    return Registry::CumFx::GetSingleton().ClearAllOverlays(actor);
  }

  bool ResetActiveSet(RE::StaticFunctionTag*, RE::Actor* actor, std::int32_t type)
  {
    const auto fxType = ToCumFxType(type);
    return fxType && Registry::CumFx::GetSingleton().ResetActiveSet(actor, *fxType);
  }

  std::int32_t GetActiveLayer(RE::StaticFunctionTag*, RE::Actor* actor, std::int32_t type)
  {
    const auto fxType = ToCumFxType(type);
    return fxType ? Registry::CumFx::GetSingleton().GetActiveLayer(actor, *fxType) : 0;
  }

  std::string GetActiveTexture(RE::StaticFunctionTag*, RE::Actor* actor, std::int32_t type)
  {
    const auto fxType = ToCumFxType(type);
    return fxType ? Registry::CumFx::GetSingleton().GetActiveTexture(actor, *fxType)
                  : std::string{};
  }
}  // namespace

bool RegisterFunctions(RE::BSScript::IVirtualMachine* vm)
{
  if (!vm)
    return false;

  vm->RegisterFunction("IsAvailable", kCumFxScriptName, IsAvailable);
  vm->RegisterFunction("RegisterFxSet", kCumFxScriptName, RegisterFxSet);
  vm->RegisterFunction("BeginOverlay", kCumFxScriptName, BeginOverlay);
  vm->RegisterFunction("ClearOverlay", kCumFxScriptName, ClearOverlay);
  vm->RegisterFunction("ClearAllOverlays", kCumFxScriptName, ClearAllOverlays);
  vm->RegisterFunction("ResetActiveSet", kCumFxScriptName, ResetActiveSet);
  vm->RegisterFunction("GetActiveLayer", kCumFxScriptName, GetActiveLayer);
  vm->RegisterFunction("GetActiveTexture", kCumFxScriptName, GetActiveTexture);
  return true;
}

}  // namespace Papyrus
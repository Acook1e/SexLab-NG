#pragma once

#include "API/SKEE.h"

namespace Registry
{
class CumFx
{
public:
  enum class Type : std::uint8_t
  {
    Vaginal = 0,
    Anal,
    Oral,
    Total
  };

  enum class Area : std::uint8_t
  {
    Face = 0,
    Body,
    Hands,
    Feet,
    Total
  };

  struct FxSet
  {
    std::string name{};
    std::uint8_t maxLayer = 1;
    float weight          = 1.0f;
  };

  struct EffectState
  {
    std::string set{};
    std::uint8_t layer = 0;
    std::string texturePath{};
    float lastAppliedTime = 0.0f;
    std::array<std::int8_t, static_cast<std::size_t>(Area::Total)> slots{-1, -1, -1, -1};
  };

  struct ActorState
  {
    std::array<EffectState, static_cast<std::size_t>(Type::Total)> effects{};
  };

  static CumFx& GetSingleton()
  {
    static CumFx instance;
    return instance;
  }

  [[nodiscard]] bool IsAvailable() const;

  bool RegisterSet(Type type, std::string setName, std::uint8_t maxLayer, float weight = 1.0f);
  void ClearRegisteredSets();

  bool BeginOverlay(RE::Actor* actor, Type type, bool forceNewSet = false);
  bool ClearOverlay(RE::Actor* actor, Type type);
  bool ClearAllOverlays(RE::Actor* actor);
  bool ResetActiveSet(RE::Actor* actor, Type type);

  [[nodiscard]] std::int32_t GetActiveLayer(RE::Actor* actor, Type type) const;
  [[nodiscard]] std::string GetActiveTexture(RE::Actor* actor, Type type) const;

  static void onSave(SKSE::SerializationInterface* serial);
  static void onLoad(SKSE::SerializationInterface* serial);
  static void onRevert(SKSE::SerializationInterface* serial);

private:
  CumFx();

  [[nodiscard]] ActorState* FindActorState(RE::Actor* actor);
  [[nodiscard]] const ActorState* FindActorState(RE::Actor* actor) const;

  constexpr static std::uint32_t SerializationType = 'CUFX';

  SKEE::IOverlayInterface* overlayInterface     = nullptr;
  SKEE::IOverrideInterface* overrideInterface   = nullptr;
  SKEE::IActorUpdateManager* actorUpdateManager = nullptr;

  mutable std::mutex stateLock;
  std::array<std::vector<FxSet>, static_cast<std::size_t>(Type::Total)> registeredSets{};
  std::unordered_map<RE::FormID, ActorState> actorStates{};
  bool loadStatePrepared = false;
};
}  // namespace Registry
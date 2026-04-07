#pragma once

namespace Hooks
{
class MainUpdate
{
public:
  static void Install()
  {
    auto& trampoline = SKSE::GetTrampoline();
    SKSE::AllocTrampoline(14);

    REL::Relocation addr{REL::VariantID(35565, 36564, 0x5BAB10),
                         REL::VariantOffset(0x748, 0xc26, 0x7ee)};
    _Update = trampoline.write_call<5>(addr.address(), Update);
  }

private:
  static void Update();
  static inline REL::Relocation<decltype(Update)> _Update;
};

class PlayerUpdate
{
public:
  static void Install()
  {
    REL::Relocation<uintptr_t> addr{RE::VTABLE_PlayerCharacter[0]};
    _Update = addr.write_vfunc(0xAD, Update);
  }

private:
  static void Update(RE::PlayerCharacter* a_this, float a_delta);
  static inline REL::Relocation<decltype(Update)> _Update;
};

class NPCUpdate
{
public:
  static void Install()
  {
    REL::Relocation<uintptr_t> addr{RE::VTABLE_Character[0]};
    _Update = addr.write_vfunc(0xAD, Update);
  }

private:
  static void Update(RE::Actor* a_this, float a_delta);
  static inline REL::Relocation<decltype(Update)> _Update;
};

class CollisionEnable
{
public:
  static void Install()
  {
    REL::Relocation<uintptr_t> addr{RE::VTABLE_bhkCollisionFilter[1]};
    _IsCollisionEnabled = addr.write_vfunc(0x1, IsCollisionEnabled);
  }

private:
  static bool* IsCollisionEnabled(RE::hkpCollidableCollidableFilter* a_this, bool* a_result,
                                  const RE::hkpCollidable* a_collidableA,
                                  const RE::hkpCollidable* a_collidableB);
  static inline REL::Relocation<decltype(IsCollisionEnabled)> _IsCollisionEnabled;
};

class ApplyMovement
{
public:
  static void Install()
  {
    auto& trampoline = SKSE::GetTrampoline();
    SKSE::AllocTrampoline(14);

    // TODO: Find VR address
    REL::Relocation<uintptr_t> addr{REL::VariantID(36359, 37350, 0x0),
                                    REL::VariantOffset(0xF0, 0xFB, 0x0)};
    _ApplyMovementDelta = trampoline.write_call<5>(addr.address(), ApplyMovementDelta);
  }

private:
  static void ApplyMovementDelta(RE::Actor* a_actor, float a_delta);
  static inline REL::Relocation<decltype(ApplyMovementDelta)> _ApplyMovementDelta;
};

inline void Install()
{
  MainUpdate::Install();
  PlayerUpdate::Install();
  NPCUpdate::Install();
  CollisionEnable::Install();
#if !defined(SKYRIMVR)
  ApplyMovement::Install();
#endif
}
}  // namespace Hooks

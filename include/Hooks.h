#pragma once

namespace Hook
{
class MainUpdate
{
  static void Install()
  {
    auto& trampoline = SKSE::GetTrampoline();
    SKSE::AllocTrampoline(14);

    REL::Relocation main_update{REL::VariantID(35565, 36564, 0x5BAB10), REL::Relocate(0x748, 0xc26, 0x7ee)};
    _Update = trampoline.write_call<5>(main_update.address(), Update);
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
}  // namespace Hook

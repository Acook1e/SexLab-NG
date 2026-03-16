#include "Hooks.h"

namespace Hook
{
void MainUpdate::Update()
{
  _Update();
  // Placeholder for main update logic
}

void PlayerUpdate::Update(RE::PlayerCharacter* a_this, float a_delta)
{
  _Update(a_this, a_delta);
  // Placeholder for player update logic
}

void NPCUpdate::Update(RE::Actor* a_this, float a_delta)
{
  _Update(a_this, a_delta);
  // Placeholder for NPC update logic
}
}  // namespace Hook
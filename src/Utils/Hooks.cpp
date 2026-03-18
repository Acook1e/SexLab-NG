#include "Utils/Hooks.h"

#include "Instance/Manager.h"

namespace Hooks
{
void MainUpdate::Update()
{
  _Update();
  // Placeholder for main update logic
  try {
    Instance::SceneManager::GetSingleton().UpdateScenes();
  } catch (const std::exception& e) {
    logger::error("Error in MainUpdate: {}", e.what());
  }
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
}  // namespace Hooks
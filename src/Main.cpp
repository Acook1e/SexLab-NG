#include "Instance/Scale.h"
#include "Registry/SceneLoader.h"
#include "Utils/Hooks.h"
#include "Utils/Menu.h"
#include "Utils/Serialization.h"

inline void onPostLoad()
{
  auto* messaging = SKSE::GetMessagingInterface();
  if (messaging) {
    // messaging->RegisterListener("SexLab", APIMessageHandler);
  }
}

inline void onPostPostLoad()
{
  Instance::Scale::GetSingleton();
}

inline void onDataLoaded()
{
  Menu::GetSingleton();
  Hooks::Install();
}

inline void onEnterGame() {}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
  switch (a_msg->type) {
  case SKSE::MessagingInterface::kPostLoad:
    onPostLoad();
    break;
  case SKSE::MessagingInterface::kPostPostLoad:
    onPostPostLoad();
    break;
  case SKSE::MessagingInterface::kDataLoaded:
    onDataLoaded();
    break;
  case SKSE::MessagingInterface::kNewGame:
    onEnterGame();
    break;
  case SKSE::MessagingInterface::kPreLoadGame:
    break;
  case SKSE::MessagingInterface::kPostLoadGame:
    onEnterGame();
    break;
  }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
  SKSE::Init(skse, true);

  logger::info("Runtime version: {}", skse->RuntimeVersion());

  auto* messaging = SKSE::GetMessagingInterface();
  if (!messaging->RegisterListener(MessageHandler)) {
    return false;
  }

  // Initialize serialization system
  Serialization::Initialize();

  // Load scene data
  Registry::SceneLoader::LoadData();

  // 注册 Papyrus Native 函数
  auto* papyrus = SKSE::GetPapyrusInterface();
  if (papyrus) {
    // papyrus->Register(Papyrus::RegisterFunctions);
  }

  return true;
}

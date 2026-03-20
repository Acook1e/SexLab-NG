#include "Instance/Scale.h"
#include "Registry/SceneLoader.h"
#include "Utils/Hooks.h"
#include "Utils/Menu.h"
#include "Utils/Serialization.h"

/*
void APIMessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
  if (a_msg->type == InflationFrameworkAPI::InterfaceExchangeMessage::kExchangeInterface) {
    auto* msg = static_cast<InflationFrameworkAPI::InterfaceExchangeMessage*>(a_msg->data);
    if (msg) {
      msg->interfacePtr = InflationFrameworkAPI::InflationFrameworkInterfaceImpl::GetSingleton();
      logger::info("[InflationFramework] API interface dispatched to {}", a_msg->sender ? a_msg->sender : "unknown");
    }
  }
}
*/

// =========================================================================
//  生命周期
// =========================================================================

inline void onPostLoad()
{
  Registry::SceneLoader::LoadData();
  // 注册为消息监听器, 接收其他 DLL 的 API 请求
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

  // 注册 Papyrus Native 函数
  auto* papyrus = SKSE::GetPapyrusInterface();
  if (papyrus) {
    // papyrus->Register(Papyrus::RegisterFunctions);
  }

  return true;
}

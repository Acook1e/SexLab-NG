#include "Utils/Serialization.h"

namespace Serialization
{
static std::unordered_map<std::uint32_t, Callback> saveCallbacks;
static std::unordered_map<std::uint32_t, Callback> loadCallbacks;
static std::unordered_map<std::uint32_t, Callback> revertCallbacks;

constexpr static std::uint32_t SerializationVersion = 1;

void Initialize()
{
  auto* serial = SKSE::GetSerializationInterface();
  serial->SetUniqueID(MOD);
  serial->SetSaveCallback([](SKSE::SerializationInterface* serial) {
    for (const auto& [type, callback] : saveCallbacks)
      if (serial->OpenRecord(type, SerializationVersion))
        callback(serial);
      else
        logger::error("[SexLab NG] Failed to open record for type: {}", type);
  });
  serial->SetLoadCallback([](SKSE::SerializationInterface* serial) {
    std::uint32_t type, version, length;
    while (serial->GetNextRecordInfo(type, version, length)) {
      if (version != SerializationVersion) {
        logger::error("[SexLab NG] Unsupported serialization version: {} for type: {}, expected: "
                      "{}. Skipping {} bytes",
                      version, type, SerializationVersion, length);
        continue;
      }

      auto it = loadCallbacks.find(type);
      if (it != loadCallbacks.end())
        it->second(serial);
      else
        logger::warn("[SexLab NG] No load callback registered for type: {}, skipping {} bytes",
                     type, length);
    }
  });
  serial->SetRevertCallback([](SKSE::SerializationInterface* serial) {
    for (const auto& [type, callback] : revertCallbacks)
      callback(serial);
  });
}

std::uint32_t GetVersion()
{
  return SerializationVersion;
}

bool RegisterSaveCallback(std::uint32_t type, Callback callback)
{
  if (saveCallbacks.find(type) != saveCallbacks.end())
    return false;  // Type already registered
  return saveCallbacks.emplace(type, std::move(callback)).second;
}

bool RegisterLoadCallback(std::uint32_t type, Callback callback)
{
  if (loadCallbacks.find(type) != loadCallbacks.end())
    return false;  // Type already registered
  return loadCallbacks.emplace(type, std::move(callback)).second;
}

bool RegisterRevertCallback(std::uint32_t type, Callback callback)
{
  if (revertCallbacks.find(type) != revertCallbacks.end())
    return false;  // Type already registered
  return revertCallbacks.emplace(type, std::move(callback)).second;
}
}  // namespace Serialization
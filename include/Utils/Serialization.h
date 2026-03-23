#pragma once

namespace Serialization
{
using Callback = std::function<void(SKSE::SerializationInterface*)>;

void Initialize();
std::uint32_t GetVersion();
bool RegisterSaveCallback(std::uint32_t type, Callback callback);
bool RegisterLoadCallback(std::uint32_t type, Callback callback);
bool RegisterRevertCallback(std::uint32_t type, Callback callback);
}  // namespace Serialization
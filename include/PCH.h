#pragma once

#include "RE/Skyrim.h"
#include "REL/REL.h"
#include "REX/REX.h"
#include "SKSE/SKSE.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <format>
#include <future>
#include <map>
#include <mutex>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

constexpr inline std::string_view PLUGIN_NAME = "SexLabNG";
constexpr inline std::uint32_t MOD            = 'SLNG';

constexpr inline std::uint32_t hash(const char* data, size_t const size) noexcept
{
  uint32_t hash = MOD;
  for (const char* c = data; c < data + size; ++c) {
    hash = ((hash << 5) + hash) + (unsigned char)*c;
  }
  return hash;
}
constexpr inline std::uint32_t hash(std::string_view str) noexcept
{
  return hash(str.data(), str.size());
}
constexpr std::uint32_t operator""_h(const char* str, size_t size) noexcept
{
  return hash(str, size);
}

namespace logger = SKSE::log;

class ScopeTimer
{
public:
  ScopeTimer(std::string_view a_name) : name(a_name), start(std::chrono::high_resolution_clock::now()) {}

  ~ScopeTimer()
  {
    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    logger::info("{} took {} ms", name, duration);
  }

private:
  std::string_view name;
  std::chrono::high_resolution_clock::time_point start;
};

inline std::string join(std::vector<std::string>& vec, char delimiter)
{
  return std::views::join_with(vec, delimiter) | std::ranges::to<std::string>();
}

inline std::vector<std::string> split(const std::string& str, char delimiter)
{
  return std::views::split(str, delimiter) | std::views::transform([](auto&& part) {
           return std::string(part.begin(), part.end());
         }) |
         std::ranges::to<std::vector>();
}

inline std::uint64_t ToPersistForm(RE::FormID formID)
{
  if (formID >= 0xFF000000)
    return 0;  // Not a mod form, cannot persist
  auto* dataHandler      = RE::TESDataHandler::GetSingleton();
  const RE::TESFile* mod = nullptr;
  std::uint32_t rawForm  = 0;
  if (formID >= 0xFE000000) {
    mod     = dataHandler->LookupLoadedLightModByIndex((formID & 0x00FFF000) >> 12);
    rawForm = formID & 0x00000FFF;
  } else {
    mod     = dataHandler->LookupLoadedModByIndex(formID >> 24);
    rawForm = formID & 0x00FFFFFF;
  }
  if (!mod || !rawForm)
    return 0;  // Mod not found or invalid formID
  return (static_cast<std::uint64_t>(hash(mod->fileName)) << 32) | rawForm;
}

inline RE::FormID ToForm(std::uint64_t persistForm)
{
  static std::unordered_map<std::uint32_t, std::uint32_t> modHashToIndexCache;

  std::uint32_t modHash = persistForm >> 32;
  std::uint32_t rawForm = persistForm & 0xFFFFFFFF;
  if (auto it = modHashToIndexCache.find(modHash); it != modHashToIndexCache.end()) {
    return it->second | rawForm;
  } else {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler)
      return 0;
    auto* mods = dataHandler->GetLoadedMods();
    for (std::size_t i = 0; i < dataHandler->GetLoadedModCount(); ++i) {
      auto* mod = mods[i];
      if (mod->compileIndex && mod->compileIndex != 0xFF)
        modHashToIndexCache[hash(mod->fileName)] = mod->compileIndex << 24;
    }
    mods = dataHandler->GetLoadedLightMods();
    for (std::size_t i = 0; i < dataHandler->GetLoadedLightModCount(); ++i) {
      auto* mod = mods[i];
      if (mod->smallFileCompileIndex)
        modHashToIndexCache[hash(mod->fileName)] = (mod->smallFileCompileIndex << 12) | 0xFE000000;
    }
    if (auto it = modHashToIndexCache.find(modHash); it != modHashToIndexCache.end())
      return it->second | rawForm;
    return 0;  // Not found
  }
}
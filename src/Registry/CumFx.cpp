#include "Registry/CumFx.h"

#include "Utils/Serialization.h"

namespace Registry
{

namespace
{
  using FxType      = CumFx::Type;
  using FxArea      = CumFx::Area;
  using FxSet       = CumFx::FxSet;
  using EffectState = CumFx::EffectState;
  using ActorState  = CumFx::ActorState;

  constexpr std::size_t ToIndex(FxType type)
  {
    return static_cast<std::size_t>(type);
  }

  constexpr std::size_t ToIndex(FxArea area)
  {
    return static_cast<std::size_t>(area);
  }

  constexpr bool IsValidType(FxType type)
  {
    return type < FxType::Total;
  }

  constexpr std::array<FxArea, static_cast<std::size_t>(FxArea::Total)> kAllAreas{
      FxArea::Face,
      FxArea::Body,
      FxArea::Hands,
      FxArea::Feet,
  };

  constexpr std::string_view kOverlayOwner          = "SexLabNGCumFx";
  constexpr std::string_view kTextureRoot           = "SexLab/CumFx/";
  constexpr std::string_view kDefaultOverlayTexture = "actors\\character\\overlays\\default.dds";
  constexpr std::uint16_t kColorKey                 = 0;
  constexpr std::uint16_t kGlossKey                 = 2;
  constexpr std::uint16_t kSpecularStrengthKey      = 3;
  constexpr std::uint16_t kTintColorKey             = 7;
  constexpr std::uint16_t kAlphaKey                 = 8;
  constexpr std::uint16_t kTextureKey               = 9;
  constexpr std::uint8_t kTextureIndex              = 0;
  constexpr std::uint8_t kWholeNodeIndex            = (std::numeric_limits<std::uint8_t>::max)();
  constexpr float kDefaultAlpha                     = 1.0f;

  template <class T>
  T* QuerySkeeInterface(SKEE::IInterfaceMap* interfaceMap, std::initializer_list<const char*> names)
  {
    if (!interfaceMap)
      return nullptr;

    for (const char* name : names) {
      if (auto* raw = interfaceMap->QueryInterface(name))
        return reinterpret_cast<T*>(raw);
    }

    return nullptr;
  }

  std::string_view TypeToString(FxType type)
  {
    switch (type) {
    case FxType::Vaginal:
      return "Vaginal";
    case FxType::Anal:
      return "Anal";
    case FxType::Oral:
      return "Oral";
    case FxType::Total:
    default:
      return "Unknown";
    }
  }

  std::string_view AreaToString(FxArea area)
  {
    switch (area) {
    case FxArea::Face:
      return "Face";
    case FxArea::Body:
      return "Body";
    case FxArea::Hands:
      return "Hands";
    case FxArea::Feet:
      return "Feet";
    case FxArea::Total:
    default:
      return "Body";
    }
  }

  bool ShouldApplyToArea(FxType type, FxArea area)
  {
    return area != FxArea::Face || type == FxType::Oral;
  }

  SKEE::IOverlayInterface::OverlayLocation ToOverlayLocation(FxArea area)
  {
    switch (area) {
    case FxArea::Face:
      return SKEE::IOverlayInterface::OverlayLocation::Face;
    case FxArea::Hands:
      return SKEE::IOverlayInterface::OverlayLocation::Hand;
    case FxArea::Feet:
      return SKEE::IOverlayInterface::OverlayLocation::Feet;
    case FxArea::Body:
    case FxArea::Total:
    default:
      return SKEE::IOverlayInterface::OverlayLocation::Body;
    }
  }

  std::string BuildNodeName(FxArea area, std::int32_t slot)
  {
    return std::format("{} [ovl{}]", AreaToString(area), slot);
  }

  std::string BuildTexturePath(FxType type, std::string_view setName, std::uint8_t layer)
  {
    return std::format("{}{}/{}/{}.dds", kTextureRoot, TypeToString(type), setName, layer);
  }

  float GetCurrentGameTime()
  {
    if (const auto* calendar = RE::Calendar::GetSingleton())
      return calendar->GetCurrentGameTime();
    return 0.0f;
  }

  bool EffectStateIsEmpty(const EffectState& effect)
  {
    return effect.set.empty() && effect.layer == 0 && effect.texturePath.empty() &&
           std::ranges::all_of(effect.slots, [](std::int8_t slot) {
             return slot < 0;
           });
  }

  bool ActorStateIsEmpty(const ActorState& actorState)
  {
    return std::ranges::all_of(actorState.effects, EffectStateIsEmpty);
  }

  bool PathBelongsToType(std::string_view texturePath, FxType type)
  {
    if (texturePath.empty())
      return false;

    const std::string forwardPath = std::format("{}{}/", kTextureRoot, TypeToString(type));
    const std::string backPath    = std::format("SexLab\\CumFx\\{}\\", TypeToString(type));
    return texturePath.find(forwardPath) != std::string_view::npos ||
           texturePath.find(backPath) != std::string_view::npos;
  }

  bool IsReusableTexture(std::string_view texturePath, FxType type, std::string_view currentTexture)
  {
    return texturePath.empty() || texturePath == kDefaultOverlayTexture ||
           texturePath == currentTexture || PathBelongsToType(texturePath, type);
  }

  bool WriteString(SKSE::SerializationInterface* serial, const std::string& value)
  {
    const std::size_t size = value.size();
    return serial->WriteRecordData(&size, sizeof(size)) &&
           (size == 0 || serial->WriteRecordData(value.data(), size));
  }

  bool ReadString(SKSE::SerializationInterface* serial, std::string& value)
  {
    std::size_t size = 0;
    if (!serial->ReadRecordData(&size, sizeof(size)))
      return false;

    value.assign(size, '\0');
    return size == 0 || serial->ReadRecordData(value.data(), size);
  }

  class IntVariant final : public SKEE::IOverrideInterface::SetVariant
  {
  public:
    explicit IntVariant(std::int32_t value) : value(value) {}

    Type GetType() override { return Type::Int; }
    std::int32_t Int() override { return value; }

  private:
    std::int32_t value;
  };

  class FloatVariant final : public SKEE::IOverrideInterface::SetVariant
  {
  public:
    explicit FloatVariant(float value) : value(value) {}

    Type GetType() override { return Type::Float; }
    float Float() override { return value; }

  private:
    float value;
  };

  class StringVariant final : public SKEE::IOverrideInterface::SetVariant
  {
  public:
    explicit StringVariant(std::string value) : value(std::move(value)) {}

    Type GetType() override { return Type::String; }
    const char* String() override { return value.c_str(); }

  private:
    std::string value;
  };

  class StringVisitor final : public SKEE::IOverrideInterface::GetVariant
  {
  public:
    void Int(std::int32_t) override {}
    void Float(float) override {}
    void String(const char* str) override { value = str ? str : ""; }
    void Bool(bool) override {}
    void TextureSet(const RE::BGSTextureSet*) override {}

    [[nodiscard]] const std::string& Get() const { return value; }

  private:
    std::string value{};
  };
}  // namespace

CumFx::CumFx()
{
  Serialization::RegisterSaveCallback(SerializationType, onSave);
  Serialization::RegisterLoadCallback(SerializationType, onLoad);
  Serialization::RegisterRevertCallback(SerializationType, onRevert);

  const auto* messaging = SKSE::GetMessagingInterface();
  if (!messaging) {
    logger::warn("[SexLab NG] CumFx: SKSE messaging interface unavailable");
    return;
  }

  SKEE::InterfaceExchangeMessage msg;
  messaging->Dispatch(SKEE::InterfaceExchangeMessage::kExchangeInterface, &msg,
                      sizeof(SKEE::InterfaceExchangeMessage*), "skee");
  if (!msg.interfaceMap) {
    logger::warn("[SexLab NG] CumFx: Could not obtain SKEE interface map");
    return;
  }

  overlayInterface  = QuerySkeeInterface<SKEE::IOverlayInterface>(msg.interfaceMap,
                                                                  {"Overlay", "OverlayInterface"});
  overrideInterface = QuerySkeeInterface<SKEE::IOverrideInterface>(
      msg.interfaceMap, {"Override", "OverrideInterface", "NiOverride"});
  actorUpdateManager = QuerySkeeInterface<SKEE::IActorUpdateManager>(
      msg.interfaceMap, {"ActorUpdateManager", "ActorUpdate", "ActorUpdateInterface"});

  if (!overlayInterface || !overrideInterface) {
    logger::warn("[SexLab NG] CumFx: Missing required SKEE interfaces. overlay={}, override={}",
                 static_cast<bool>(overlayInterface), static_cast<bool>(overrideInterface));
    return;
  }

  logger::info("[SexLab NG] CumFx: Ready. actorUpdateManager={}",
               static_cast<bool>(actorUpdateManager));
}

bool CumFx::IsAvailable() const
{
  return overlayInterface && overrideInterface;
}

CumFx::ActorState* CumFx::FindActorState(RE::Actor* actor)
{
  if (!actor)
    return nullptr;

  const auto it = actorStates.find(actor->GetFormID());
  return it != actorStates.end() ? std::addressof(it->second) : nullptr;
}

const CumFx::ActorState* CumFx::FindActorState(RE::Actor* actor) const
{
  if (!actor)
    return nullptr;

  const auto it = actorStates.find(actor->GetFormID());
  return it != actorStates.end() ? std::addressof(it->second) : nullptr;
}

bool CumFx::RegisterSet(Type type, std::string setName, std::uint8_t maxLayer, float weight)
{
  if (!IsValidType(type) || setName.empty() || maxLayer == 0)
    return false;

  std::scoped_lock lock(stateLock);
  auto& definitions = registeredSets[ToIndex(type)];
  auto it           = std::ranges::find(definitions, setName, &FxSet::name);
  if (it != definitions.end()) {
    it->maxLayer = maxLayer;
    it->weight   = (std::max)(weight, 0.0f);
    return true;
  }

  definitions.push_back(FxSet{std::move(setName), maxLayer, (std::max)(weight, 0.0f)});
  return true;
}

void CumFx::ClearRegisteredSets()
{
  std::scoped_lock lock(stateLock);
  for (auto& definitions : registeredSets)
    definitions.clear();
}

bool CumFx::BeginOverlay(RE::Actor* actor, Type type, bool forceNewSet)
{
  if (!IsAvailable() || !actor || !IsValidType(type))
    return false;

  const bool isFemale = actor->GetActorBase() ? actor->GetActorBase()->IsFemale() : false;

  std::scoped_lock lock(stateLock);
  auto& definitions = registeredSets[ToIndex(type)];
  if (definitions.empty()) {
    logger::warn("[SexLab NG] CumFx: No registered fx set for {}", TypeToString(type));
    return false;
  }

  auto pickSet = [&]() -> const FxSet* {
    float totalWeight = 0.0f;
    for (const auto& definition : definitions)
      totalWeight += (std::max)(definition.weight, 0.0f);

    if (totalWeight <= 0.0f)
      return std::addressof(definitions.front());

    thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(0.0f, totalWeight);
    float cursor = dist(rng);

    for (const auto& definition : definitions) {
      cursor -= (std::max)(definition.weight, 0.0f);
      if (cursor <= 0.0f)
        return std::addressof(definition);
    }

    return std::addressof(definitions.back());
  };

  auto& actorState = actorStates[actor->GetFormID()];
  auto& effect     = actorState.effects[ToIndex(type)];

  auto findDefinition = [&](std::string_view name) -> const FxSet* {
    const auto it = std::ranges::find(definitions, name, &FxSet::name);
    return it != definitions.end() ? std::addressof(*it) : nullptr;
  };

  const FxSet* definition = forceNewSet ? nullptr : findDefinition(effect.set);
  if (!definition) {
    definition = pickSet();
    if (!definition)
      return false;

    effect.set = definition->name;
    if (forceNewSet) {
      effect.layer = 0;
      effect.texturePath.clear();
    }
  }

  const std::uint8_t nextLayer  = effect.layer < definition->maxLayer
                                      ? static_cast<std::uint8_t>(effect.layer + 1)
                                      : definition->maxLayer;
  const std::string texturePath = BuildTexturePath(type, effect.set, nextLayer);
  if (nextLayer == effect.layer && texturePath == effect.texturePath)
    return true;

  bool overlaysChanged = false;
  if (!overlayInterface->HasOverlays(actor)) {
    overlayInterface->AddOverlays(actor, actorUpdateManager != nullptr);
    overlaysChanged = true;
  }

  auto getOverlayCount = [&](FxArea area) -> std::int32_t {
    return static_cast<std::int32_t>(overlayInterface->GetOverlayCount(
        SKEE::IOverlayInterface::OverlayType::Normal, ToOverlayLocation(area)));
  };

  auto readSlotTexture = [&](FxArea area, std::int32_t slot) -> std::string {
    if (slot < 0)
      return {};

    StringVisitor visitor;
    const auto node = BuildNodeName(area, slot);
    if (!overrideInterface->GetNodeOverride(actor, isFemale, node.c_str(), kTextureKey,
                                            kTextureIndex, visitor)) {
      return {};
    }
    return visitor.Get();
  };

  auto applySlot = [&](FxArea area, std::int32_t slot) {
    const auto node = BuildNodeName(area, slot);
    StringVariant textureValue(texturePath);
    IntVariant zeroInt(0);
    FloatVariant alphaValue(kDefaultAlpha);
    FloatVariant zeroFloat(0.0f);

    overrideInterface->AddNodeOverride(actor, isFemale, node.c_str(), kTextureKey, kTextureIndex,
                                       textureValue);
    overrideInterface->AddNodeOverride(actor, isFemale, node.c_str(), kTintColorKey,
                                       kWholeNodeIndex, zeroInt);
    overrideInterface->AddNodeOverride(actor, isFemale, node.c_str(), kColorKey, kWholeNodeIndex,
                                       zeroInt);
    overrideInterface->AddNodeOverride(actor, isFemale, node.c_str(), kAlphaKey, kWholeNodeIndex,
                                       alphaValue);
    overrideInterface->AddNodeOverride(actor, isFemale, node.c_str(), kGlossKey, kWholeNodeIndex,
                                       zeroFloat);
    overrideInterface->AddNodeOverride(actor, isFemale, node.c_str(), kSpecularStrengthKey,
                                       kWholeNodeIndex, zeroFloat);
  };

  bool applied = false;
  for (FxArea area : kAllAreas) {
    if (!ShouldApplyToArea(type, area))
      continue;

    const auto overlayCount = getOverlayCount(area);
    if (overlayCount <= 0)
      continue;

    auto canUseSlot = [&](std::int32_t slot) {
      if (slot < 0 || slot >= overlayCount)
        return false;
      return IsReusableTexture(readSlotTexture(area, slot), type, effect.texturePath);
    };

    std::int32_t chosenSlot = effect.slots[ToIndex(area)];
    if (!canUseSlot(chosenSlot)) {
      chosenSlot = -1;
      for (std::int32_t slot = overlayCount - 1; slot >= 0; --slot) {
        if (!canUseSlot(slot))
          continue;
        chosenSlot = slot;
        break;
      }
    }

    if (chosenSlot < 0) {
      logger::warn("[SexLab NG] CumFx: No reusable slot for actor '{}' area '{}' type '{}'",
                   actor->GetDisplayFullName(), AreaToString(area), TypeToString(type));
      continue;
    }

    applySlot(area, chosenSlot);
    effect.slots[ToIndex(area)] = static_cast<std::int8_t>(chosenSlot);
    applied                     = true;
  }

  if (!applied)
    return false;

  effect.layer           = nextLayer;
  effect.texturePath     = texturePath;
  effect.lastAppliedTime = GetCurrentGameTime();

  overrideInterface->SetNodeProperties(actor, actorUpdateManager == nullptr);
  if (actorUpdateManager) {
    if (overlaysChanged)
      actorUpdateManager->AddOverlayUpdate(actor->GetFormID());
    actorUpdateManager->AddNodeOverrideUpdate(actor->GetFormID());
    actorUpdateManager->Flush();
  }

  return true;
}

bool CumFx::ClearOverlay(RE::Actor* actor, Type type)
{
  if (!IsAvailable() || !actor || !IsValidType(type))
    return false;

  const bool isFemale = actor->GetActorBase() ? actor->GetActorBase()->IsFemale() : false;
  std::scoped_lock lock(stateLock);

  auto actorIt = actorStates.find(actor->GetFormID());
  EffectState fallbackEffect;
  EffectState* effect = std::addressof(fallbackEffect);
  if (actorIt != actorStates.end())
    effect = std::addressof(actorIt->second.effects[ToIndex(type)]);

  auto getOverlayCount = [&](FxArea area) -> std::int32_t {
    return static_cast<std::int32_t>(overlayInterface->GetOverlayCount(
        SKEE::IOverlayInterface::OverlayType::Normal, ToOverlayLocation(area)));
  };

  auto readSlotTexture = [&](FxArea area, std::int32_t slot) -> std::string {
    StringVisitor visitor;
    const auto node = BuildNodeName(area, slot);
    if (!overrideInterface->GetNodeOverride(actor, isFemale, node.c_str(), kTextureKey,
                                            kTextureIndex, visitor)) {
      return {};
    }
    return visitor.Get();
  };

  auto clearSlot = [&](FxArea area, std::int32_t slot) {
    const auto node = BuildNodeName(area, slot);
    StringVariant defaultTexture{std::string(kDefaultOverlayTexture)};
    overrideInterface->AddNodeOverride(actor, isFemale, node.c_str(), kTextureKey, kTextureIndex,
                                       defaultTexture);
    overrideInterface->RemoveNodeOverride(actor, isFemale, node.c_str(), kTextureKey,
                                          kTextureIndex);
    overrideInterface->RemoveNodeOverride(actor, isFemale, node.c_str(), kTintColorKey,
                                          kWholeNodeIndex);
    overrideInterface->RemoveNodeOverride(actor, isFemale, node.c_str(), kColorKey,
                                          kWholeNodeIndex);
    overrideInterface->RemoveNodeOverride(actor, isFemale, node.c_str(), kAlphaKey,
                                          kWholeNodeIndex);
    overrideInterface->RemoveNodeOverride(actor, isFemale, node.c_str(), kGlossKey,
                                          kWholeNodeIndex);
    overrideInterface->RemoveNodeOverride(actor, isFemale, node.c_str(), kSpecularStrengthKey,
                                          kWholeNodeIndex);
  };

  bool changed = false;
  for (FxArea area : kAllAreas) {
    if (!ShouldApplyToArea(type, area))
      continue;

    const auto overlayCount = getOverlayCount(area);
    if (overlayCount <= 0)
      continue;

    std::vector<std::int32_t> slotsToClear;
    const std::int32_t preferredSlot = effect->slots[ToIndex(area)];
    if (preferredSlot >= 0 && preferredSlot < overlayCount) {
      const auto texture = readSlotTexture(area, preferredSlot);
      if (texture == effect->texturePath || PathBelongsToType(texture, type))
        slotsToClear.push_back(preferredSlot);
    }

    if (slotsToClear.empty()) {
      for (std::int32_t slot = overlayCount - 1; slot >= 0; --slot) {
        const auto texture = readSlotTexture(area, slot);
        if (texture == effect->texturePath || PathBelongsToType(texture, type))
          slotsToClear.push_back(slot);
      }
    }

    for (std::int32_t slot : slotsToClear) {
      clearSlot(area, slot);
      effect->slots[ToIndex(area)] = -1;
      changed                      = true;
    }
  }

  if (!changed)
    return false;

  *effect = EffectState{};
  overrideInterface->SetNodeProperties(actor, actorUpdateManager == nullptr);
  if (actorUpdateManager) {
    actorUpdateManager->AddNodeOverrideUpdate(actor->GetFormID());
    actorUpdateManager->Flush();
  }

  if (actorIt != actorStates.end() && ActorStateIsEmpty(actorIt->second))
    actorStates.erase(actorIt);

  return true;
}

bool CumFx::ClearAllOverlays(RE::Actor* actor)
{
  bool changed = false;
  changed |= ClearOverlay(actor, Type::Vaginal);
  changed |= ClearOverlay(actor, Type::Anal);
  changed |= ClearOverlay(actor, Type::Oral);
  return changed;
}

bool CumFx::ResetActiveSet(RE::Actor* actor, Type type)
{
  if (!actor || !IsValidType(type))
    return false;

  std::scoped_lock lock(stateLock);
  auto* state = FindActorState(actor);
  if (!state)
    return false;

  auto& effect = state->effects[ToIndex(type)];
  effect.set.clear();
  effect.layer = 0;
  effect.texturePath.clear();
  effect.lastAppliedTime = 0.0f;
  return true;
}

std::int32_t CumFx::GetActiveLayer(RE::Actor* actor, Type type) const
{
  if (!actor || !IsValidType(type))
    return 0;

  std::scoped_lock lock(stateLock);
  const auto* state = FindActorState(actor);
  return state ? state->effects[ToIndex(type)].layer : 0;
}

std::string CumFx::GetActiveTexture(RE::Actor* actor, Type type) const
{
  if (!actor || !IsValidType(type))
    return {};

  std::scoped_lock lock(stateLock);
  const auto* state = FindActorState(actor);
  return state ? state->effects[ToIndex(type)].texturePath : std::string{};
}

void CumFx::onSave(SKSE::SerializationInterface* serial)
{
  auto& self = GetSingleton();
  std::scoped_lock lock(self.stateLock);

  std::vector<std::pair<std::uint64_t, ActorState>> persistedStates;
  persistedStates.reserve(self.actorStates.size());
  for (const auto& [formID, state] : self.actorStates) {
    if (ActorStateIsEmpty(state))
      continue;

    const auto persistedForm = ToPersistForm(formID);
    if (persistedForm == 0)
      continue;

    persistedStates.emplace_back(persistedForm, state);
  }

  const std::size_t stateCount = persistedStates.size();
  serial->WriteRecordData(&stateCount, sizeof(stateCount));
  for (const auto& [persistedForm, state] : persistedStates) {
    serial->WriteRecordData(&persistedForm, sizeof(persistedForm));

    std::uint8_t effectCount = 0;
    for (const auto& effect : state.effects) {
      if (!EffectStateIsEmpty(effect))
        ++effectCount;
    }

    serial->WriteRecordData(&effectCount, sizeof(effectCount));
    for (std::uint8_t index = 0; index < static_cast<std::uint8_t>(FxType::Total); ++index) {
      const auto& effect = state.effects[index];
      if (EffectStateIsEmpty(effect))
        continue;

      serial->WriteRecordData(&index, sizeof(index));
      serial->WriteRecordData(&effect.layer, sizeof(effect.layer));
      serial->WriteRecordData(&effect.lastAppliedTime, sizeof(effect.lastAppliedTime));
      serial->WriteRecordData(effect.slots.data(), static_cast<std::uint32_t>(effect.slots.size() *
                                                                              sizeof(std::int8_t)));
      WriteString(serial, effect.set);
      WriteString(serial, effect.texturePath);
    }
  }
}

void CumFx::onLoad(SKSE::SerializationInterface* serial)
{
  auto& self = GetSingleton();
  std::scoped_lock lock(self.stateLock);

  if (!self.loadStatePrepared) {
    self.actorStates.clear();
    self.loadStatePrepared = true;
  }

  std::size_t stateCount = 0;
  if (!serial->ReadRecordData(&stateCount, sizeof(stateCount)))
    return;

  for (std::size_t stateIndex = 0; stateIndex < stateCount; ++stateIndex) {
    std::uint64_t persistedForm = 0;
    if (!serial->ReadRecordData(&persistedForm, sizeof(persistedForm)))
      return;

    std::uint8_t effectCount = 0;
    if (!serial->ReadRecordData(&effectCount, sizeof(effectCount)))
      return;

    ActorState loadedState;
    for (std::uint8_t effectIndex = 0; effectIndex < effectCount; ++effectIndex) {
      std::uint8_t typeIndex = 0;
      if (!serial->ReadRecordData(&typeIndex, sizeof(typeIndex)))
        return;

      if (typeIndex >= static_cast<std::uint8_t>(FxType::Total))
        return;

      auto& effect = loadedState.effects[typeIndex];
      if (!serial->ReadRecordData(&effect.layer, sizeof(effect.layer)))
        return;
      if (!serial->ReadRecordData(&effect.lastAppliedTime, sizeof(effect.lastAppliedTime)))
        return;
      if (!serial->ReadRecordData(
              effect.slots.data(),
              static_cast<std::uint32_t>(effect.slots.size() * sizeof(std::int8_t)))) {
        return;
      }
      if (!ReadString(serial, effect.set))
        return;
      if (!ReadString(serial, effect.texturePath))
        return;
    }

    const auto formID = ToForm(persistedForm);
    if (formID != 0)
      self.actorStates[formID] = std::move(loadedState);
  }
}

void CumFx::onRevert(SKSE::SerializationInterface* /*serial*/)
{
  auto& self = GetSingleton();
  std::scoped_lock lock(self.stateLock);
  self.actorStates.clear();
  self.loadStatePrepared = false;
}

}  // namespace Registry
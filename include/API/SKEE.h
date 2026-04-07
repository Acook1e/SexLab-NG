#pragma once

namespace SKEE
{
class IPluginInterface
{
public:
  IPluginInterface() {};
  virtual ~IPluginInterface() {};

  virtual uint32_t GetVersion() = 0;
  virtual void Revert()         = 0;
};

class IInterfaceMap
{
public:
  virtual IPluginInterface* QueryInterface(const char* name)                     = 0;
  virtual bool AddInterface(const char* name, IPluginInterface* pluginInterface) = 0;
  virtual IPluginInterface* RemoveInterface(const char* name)                    = 0;
};

struct InterfaceExchangeMessage
{
  enum : uint32_t
  {
    kExchangeInterface = 0x9E3779B9
  };

  IInterfaceMap* interfaceMap = nullptr;
};

class IAddonAttachmentInterface
{
public:
  virtual void OnAttach(RE::TESObjectREFR* refr, RE::TESObjectARMO* armor, RE::TESObjectARMA* addon,
                        RE::NiAVObject* object, bool isFirstPerson, RE::NiNode* skeleton,
                        RE::NiNode* root) = 0;
};

class IBodyMorphInterface : public IPluginInterface
{
public:
  class MorphKeyVisitor
  {
  public:
    virtual void Visit(const char*, float) = 0;
  };

  class StringVisitor
  {
  public:
    virtual void Visit(const char*) = 0;
  };

  class ActorVisitor
  {
  public:
    virtual void Visit(RE::TESObjectREFR*) = 0;
  };

  class MorphValueVisitor
  {
  public:
    virtual void Visit(RE::TESObjectREFR*, const char*, const char*, float) = 0;
  };

  class MorphVisitor
  {
  public:
    virtual void Visit(RE::TESObjectREFR*, const char*) = 0;
  };

  virtual void SetMorph(RE::TESObjectREFR* actor, const char* morphName, const char* morphKey,
                        float relative)                                                         = 0;
  virtual float GetMorph(RE::TESObjectREFR* actor, const char* morphName, const char* morphKey) = 0;
  virtual void ClearMorph(RE::TESObjectREFR* actor, const char* morphName,
                          const char* morphKey)                                                 = 0;

  virtual float GetBodyMorphs(RE::TESObjectREFR* actor, const char* morphName)      = 0;
  virtual void ClearBodyMorphNames(RE::TESObjectREFR* actor, const char* morphName) = 0;

  virtual void VisitMorphs(RE::TESObjectREFR* actor, MorphVisitor& visitor)                    = 0;
  virtual void VisitKeys(RE::TESObjectREFR* actor, const char* name, MorphKeyVisitor& visitor) = 0;
  virtual void VisitMorphValues(RE::TESObjectREFR* actor, MorphValueVisitor& visitor)          = 0;

  virtual void ClearMorphs(RE::TESObjectREFR* actor) = 0;

  virtual void ApplyVertexDiff(RE::TESObjectREFR* refr, RE::NiAVObject* rootNode,
                               bool erase = false) = 0;

  virtual void ApplyBodyMorphs(RE::TESObjectREFR* refr, bool deferUpdate = true)  = 0;
  virtual void UpdateModelWeight(RE::TESObjectREFR* refr, bool immediate = false) = 0;

  virtual void SetCacheLimit(size_t limit)                      = 0;
  virtual bool HasMorphs(RE::TESObjectREFR* actor)              = 0;
  virtual uint32_t EvaluateBodyMorphs(RE::TESObjectREFR* actor) = 0;

  virtual bool HasBodyMorph(RE::TESObjectREFR* actor, const char* morphName,
                            const char* morphKey)                                 = 0;
  virtual bool HasBodyMorphName(RE::TESObjectREFR* actor, const char* morphName)  = 0;
  virtual bool HasBodyMorphKey(RE::TESObjectREFR* actor, const char* morphKey)    = 0;
  virtual void ClearBodyMorphKeys(RE::TESObjectREFR* actor, const char* morphKey) = 0;
  virtual void VisitStrings(StringVisitor& visitor)                               = 0;
  virtual void VisitActors(ActorVisitor& visitor)                                 = 0;
  virtual size_t ClearMorphCache()                                                = 0;
};

class INiTransformInterface : public IPluginInterface
{
public:
  enum
  {
    kPluginVersion1 = 1,
    kPluginVersion2,
    kPluginVersion3,
    kCurrentPluginVersion  = kPluginVersion3,
    kSerializationVersion1 = 1,
    kSerializationVersion2,
    kSerializationVersion3,
    kSerializationVersion = kSerializationVersion3
  };
  struct Position
  {
    float x, y, z;
  };
  struct Rotation
  {
    float heading, attitude, bank;
  };

  class NodeVisitor
  {
  public:
    virtual bool VisitPosition(const char* node, const char* key, Position& position)  = 0;
    virtual bool VisitRotation(const char* node, const char* key, Rotation& rotation)  = 0;
    virtual bool VisitScale(const char* node, const char* key, float scale)            = 0;
    virtual bool VisitScaleMode(const char* node, const char* key, uint32_t scaleMode) = 0;
  };

  virtual bool HasNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                        const char* node, const char* name)  = 0;
  virtual bool HasNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                        const char* node, const char* name)  = 0;
  virtual bool HasNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                     const char* node, const char* name)     = 0;
  virtual bool HasNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                         const char* node, const char* name) = 0;

  virtual void AddNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                        const char* node, const char* name, Position& position) = 0;
  virtual void AddNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                        const char* node, const char* name, Rotation& rotation) = 0;
  virtual void AddNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                     const char* node, const char* name, float scale)           = 0;
  virtual void AddNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                         const char* node, const char* name,
                                         uint32_t scaleMode)                                    = 0;

  virtual Position GetNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                            const char* node, const char* name)                 = 0;
  virtual Rotation GetNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                            const char* node, const char* name)                 = 0;
  virtual float GetNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                      const char* node, const char* name)                       = 0;
  virtual uint32_t GetNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson,
                                             bool isFemale, const char* node, const char* name) = 0;

  virtual bool RemoveNodeTransformPosition(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                           const char* node, const char* name)  = 0;
  virtual bool RemoveNodeTransformRotation(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                           const char* node, const char* name)  = 0;
  virtual bool RemoveNodeTransformScale(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                        const char* node, const char* name)     = 0;
  virtual bool RemoveNodeTransformScaleMode(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                            const char* node, const char* name) = 0;

  virtual bool RemoveNodeTransform(RE::TESObjectREFR* refr, bool firstPerson, bool isFemale,
                                   const char* node, const char* name) = 0;
  virtual void RemoveAllReferenceTransforms(RE::TESObjectREFR* refr)   = 0;

  virtual bool GetOverrideNodeTransform(RE::TESObjectREFR* refr, bool firstPerson, bool isFemale,
                                        const char* node, const char* name, uint16_t key,
                                        RE::NiTransform* result) = 0;

  virtual void UpdateNodeAllTransforms(RE::TESObjectREFR* ref) = 0;

  virtual void VisitNodes(RE::TESObjectREFR* refr, bool firstPerson, bool isFemale,
                          NodeVisitor& visitor)       = 0;
  virtual void UpdateNodeTransforms(RE::TESObjectREFR* ref, bool firstPerson, bool isFemale,
                                    const char* node) = 0;
};

class IAttachmentInterface : public IPluginInterface
{
public:
  virtual bool AttachMesh(RE::TESObjectREFR* ref, const char* nifPath, const char* name,
                          bool firstPerson, bool replace, const char** filter, uint32_t filterSize,
                          RE::NiAVObject*& out, char* err, uint64_t errBufLen)        = 0;
  virtual bool DetachMesh(RE::TESObjectREFR* ref, const char* name, bool firstPerson) = 0;
};

class IItemDataInterface : public IPluginInterface
{
public:
  enum
  {
    kPluginVersion1 = 1,
    kPluginVersion2,
    kPluginVersion3,
    kCurrentPluginVersion  = kPluginVersion3,
    kSerializationVersion1 = 1,
    kSerializationVersion2,
    kSerializationVersion = kSerializationVersion2
  };

  struct Identifier
  {
    enum
    {
      kTypeNone   = 0,
      kTypeRank   = (1 << 0),
      kTypeUID    = (1 << 1),
      kTypeSlot   = (1 << 2),
      kTypeSelf   = (1 << 3),
      kTypeDirect = (1 << 4)
    };

    enum
    {
      kHandSlot_Left = 0,
      kHandSlot_Right
    };

    uint16_t type                = kTypeNone;
    uint16_t uid                 = 0;
    uint32_t ownerForm           = 0;
    uint32_t weaponSlot          = 0;
    uint32_t slotMask            = 0;
    uint32_t rankId              = 0;
    RE::TESForm* form            = nullptr;
    RE::BaseExtraList* extraData = nullptr;

    void SetRankID(uint32_t _rank)
    {
      type |= kTypeRank;
      rankId = _rank;
    }
    void SetSlotMask(uint32_t _slotMask, uint32_t _weaponSlot = 0)
    {
      type |= kTypeSlot;
      slotMask   = _slotMask;
      weaponSlot = _weaponSlot;
    }
    void SetUniqueID(uint16_t _uid, uint32_t _ownerForm)
    {
      type |= kTypeUID;
      uid       = _uid;
      ownerForm = _ownerForm;
    }
    void SetDirect(RE::TESForm* _baseForm, RE::BaseExtraList* _extraData)
    {
      type |= kTypeDirect;
      form      = _baseForm;
      extraData = _extraData;
    }

    bool IsDirect() { return (type & kTypeDirect) == kTypeDirect; }

    bool IsSelf() { return (type & kTypeSelf) == kTypeSelf; }

    void SetSelf() { type |= kTypeSelf; }
  };

  class StringVisitor
  {
  public:
    virtual void Visit(const char*) = 0;
  };

  virtual uint32_t GetItemUniqueID(RE::TESObjectREFR* reference, Identifier& identifier,
                                   bool makeUnique)                                    = 0;
  virtual void SetItemTextureLayerColor(uint32_t uniqueID, int32_t textureIndex, int32_t layerIndex,
                                        uint32_t color)                                = 0;
  virtual void SetItemTextureLayerType(uint32_t uniqueID, int32_t textureIndex, int32_t layerIndex,
                                       uint32_t type)                                  = 0;
  virtual void SetItemTextureLayerBlendMode(uint32_t uniqueID, int32_t textureIndex,
                                            int32_t layerIndex, const char* blendMode) = 0;
  virtual void SetItemTextureLayerTexture(uint32_t uniqueID, int32_t textureIndex,
                                          int32_t layerIndex, const char* texture)     = 0;

  virtual uint32_t GetItemTextureLayerColor(uint32_t uniqueID, int32_t textureIndex,
                                            int32_t layerIndex)                         = 0;
  virtual uint32_t GetItemTextureLayerType(uint32_t uniqueID, int32_t textureIndex,
                                           int32_t layerIndex)                          = 0;
  virtual bool GetItemTextureLayerBlendMode(uint32_t uniqueID, int32_t textureIndex,
                                            int32_t layerIndex, StringVisitor& visitor) = 0;
  virtual bool GetItemTextureLayerTexture(uint32_t uniqueID, int32_t textureIndex,
                                          int32_t layerIndex, StringVisitor& visitor)   = 0;

  virtual void ClearItemTextureLayerColor(uint32_t uniqueID, int32_t textureIndex,
                                          int32_t layerIndex)                 = 0;
  virtual void ClearItemTextureLayerType(uint32_t uniqueID, int32_t textureIndex,
                                         int32_t layerIndex)                  = 0;
  virtual void ClearItemTextureLayerBlendMode(uint32_t uniqueID, int32_t textureIndex,
                                              int32_t layerIndex)             = 0;
  virtual void ClearItemTextureLayerTexture(uint32_t uniqueID, int32_t textureIndex,
                                            int32_t layerIndex)               = 0;
  virtual void ClearItemTextureLayer(uint32_t uniqueID, int32_t textureIndex) = 0;

  virtual RE::TESForm* GetFormFromUniqueID(uint32_t uniqueID) = 0;
  virtual RE::TESForm* GetOwnerOfUniqueID(uint32_t uniqueID)  = 0;

  virtual bool HasItemData(uint32_t uniqueID, const char* key)                         = 0;
  virtual bool GetItemData(uint32_t uniqueID, const char* key, StringVisitor& visitor) = 0;
  virtual void SetItemData(uint32_t uniqueID, const char* key, const char* value)      = 0;
  virtual void ClearItemData(uint32_t uniqueID, const char* key)                       = 0;
};

class ICommandInterface : public IPluginInterface
{
public:
  using CommandCallback = bool (*)(RE::TESObjectREFR* ref, const char* argumentString);
  virtual bool RegisterCommand(const char* command, const char* desc, CommandCallback cb) = 0;
};

class IActorUpdateManager : public IPluginInterface
{
public:
  enum
  {
    kPluginVersion1 = 1,
    kPluginVersion2,
    kCurrentPluginVersion = kPluginVersion2,
  };
  virtual void AddBodyUpdate(uint32_t formId)                       = 0;
  virtual void AddTransformUpdate(uint32_t formId)                  = 0;
  virtual void AddOverlayUpdate(uint32_t formId)                    = 0;
  virtual void AddNodeOverrideUpdate(uint32_t formId)               = 0;
  virtual void AddWeaponOverrideUpdate(uint32_t formId)             = 0;
  virtual void AddAddonOverrideUpdate(uint32_t formId)              = 0;
  virtual void AddSkinOverrideUpdate(uint32_t formId)               = 0;
  virtual void Flush()                                              = 0;
  virtual void AddInterface(IAddonAttachmentInterface* observer)    = 0;
  virtual void RemoveInterface(IAddonAttachmentInterface* observer) = 0;

  using FlushCallback = void (*)(uint32_t* formId, uint32_t length);
  virtual bool RegisterFlushCallback(const char* key, FlushCallback cb) = 0;
  virtual bool UnregisterFlushCallback(const char* key)                 = 0;
};

class IOverlayInterface : public IPluginInterface
{
public:
  enum
  {
    kPluginVersion1 = 1,
    kPluginVersion2,
    kCurrentPluginVersion  = kPluginVersion2,
    kSerializationVersion1 = 1,
    kSerializationVersion  = kSerializationVersion1
  };
  virtual bool HasOverlays(RE::TESObjectREFR* reference)                               = 0;
  virtual void AddOverlays(RE::TESObjectREFR* reference, bool defer = true)            = 0;
  virtual void RemoveOverlays(RE::TESObjectREFR* reference, bool defer = true)         = 0;
  virtual void RevertOverlays(RE::TESObjectREFR* reference, bool resetDiffuse,
                              bool defer = true)                                       = 0;
  virtual void RevertOverlay(RE::TESObjectREFR* reference, const char* nodeName, uint32_t armorMask,
                             uint32_t addonMask, bool resetDiffuse, bool defer = true) = 0;
  virtual void EraseOverlays(RE::TESObjectREFR* reference, bool defer = true)          = 0;
  virtual void RevertHeadOverlays(RE::TESObjectREFR* reference, bool resetDiffuse,
                                  bool defer = true)                                   = 0;
  virtual void RevertHeadOverlay(RE::TESObjectREFR* reference, const char* nodeName,
                                 uint32_t partType, uint32_t shaderType, bool resetDiffuse,
                                 bool defer = true)                                    = 0;
  enum class OverlayType
  {
    Normal,
    Spell
  };
  enum class OverlayLocation
  {
    Body,
    Hand,
    Feet,
    Face
  };
  virtual uint32_t GetOverlayCount(OverlayType type, OverlayLocation location)     = 0;
  virtual const char* GetOverlayFormat(OverlayType type, OverlayLocation location) = 0;

  using OverlayInstallCallback = void (*)(RE::TESObjectREFR* ref, RE::NiAVObject* node);
  virtual bool RegisterInstallCallback(const char* key, OverlayInstallCallback cb) = 0;
  virtual bool UnregisterInstallCallback(const char* key)                          = 0;
};

class IOverrideInterface : public IPluginInterface
{
public:
  enum
  {
    kPluginVersion1 = 1,
    kPluginVersion2,
    kCurrentPluginVersion  = kPluginVersion2,
    kSerializationVersion1 = 1,
    kSerializationVersion2,
    kSerializationVersion3,
    kSerializationVersion = kSerializationVersion3
  };

  class GetVariant
  {
  public:
    virtual void Int(int32_t i)                                  = 0;
    virtual void Float(float f)                                  = 0;
    virtual void String(const char* str)                         = 0;
    virtual void Bool(bool b)                                    = 0;
    virtual void TextureSet(const RE::BGSTextureSet* textureSet) = 0;
  };

  class SetVariant
  {
  public:
    enum class Type
    {
      None,
      Int,
      Float,
      String,
      Bool,
      TextureSet
    };
    virtual Type GetType() { return Type::None; }
    virtual int32_t Int() { return 0; }
    virtual float Float() { return 0.0f; }
    virtual const char* String() { return nullptr; }
    virtual bool Bool() { return false; }
    virtual RE::BGSTextureSet* TextureSet() { return nullptr; }
  };

  virtual bool HasArmorAddonNode(RE::TESObjectREFR* refr, bool firstPerson,
                                 RE::TESObjectARMO* armor, RE::TESObjectARMA* addon,
                                 const char* nodeName, bool debug) = 0;

  virtual bool HasArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor,
                                RE::TESObjectARMA* addon, const char* nodeName, uint16_t key,
                                uint8_t index)                                    = 0;
  virtual void AddArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor,
                                RE::TESObjectARMA* addon, const char* nodeName, uint16_t key,
                                uint8_t index, SetVariant& value)                 = 0;
  virtual bool GetArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor,
                                RE::TESObjectARMA* addon, const char* nodeName, uint16_t key,
                                uint8_t index, GetVariant& visitor)               = 0;
  virtual void RemoveArmorOverride(RE::TESObjectREFR* refr, bool isFemale, RE::TESObjectARMO* armor,
                                   RE::TESObjectARMA* addon, const char* nodeName, uint16_t key,
                                   uint8_t index)                                 = 0;
  virtual void SetArmorProperties(RE::TESObjectREFR* refr, bool immediate)        = 0;
  virtual void SetArmorProperty(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor,
                                RE::TESObjectARMA* addon, const char* nodeName, uint16_t key,
                                uint8_t index, SetVariant& value, bool immediate) = 0;
  virtual bool GetArmorProperty(RE::TESObjectREFR* refr, bool firstPerson, RE::TESObjectARMO* armor,
                                RE::TESObjectARMA* addon, const char* nodeName, uint16_t key,
                                uint8_t index, GetVariant& value)                 = 0;
  virtual void ApplyArmorOverrides(RE::TESObjectREFR* refr, RE::TESObjectARMO* armor,
                                   RE::TESObjectARMA* addon, RE::NiAVObject* object,
                                   bool immediate)                                = 0;
  virtual void RemoveAllArmorOverrides()                                          = 0;
  virtual void RemoveAllArmorOverridesByReference(RE::TESObjectREFR* reference)   = 0;
  virtual void RemoveAllArmorOverridesByArmor(RE::TESObjectREFR* refr, bool isFemale,
                                              RE::TESObjectARMO* armor)           = 0;
  virtual void RemoveAllArmorOverridesByAddon(RE::TESObjectREFR* refr, bool isFemale,
                                              RE::TESObjectARMO* armor,
                                              RE::TESObjectARMA* addon)           = 0;
  virtual void RemoveAllArmorOverridesByNode(RE::TESObjectREFR* refr, bool isFemale,
                                             RE::TESObjectARMO* armor, RE::TESObjectARMA* addon,
                                             const char* nodeName)                = 0;

  virtual bool HasNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName,
                               uint16_t key, uint8_t index)                                    = 0;
  virtual void AddNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName,
                               uint16_t key, uint8_t index, SetVariant& value)                 = 0;
  virtual bool GetNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName,
                               uint16_t key, uint8_t index, GetVariant& visitor)               = 0;
  virtual void RemoveNodeOverride(RE::TESObjectREFR* refr, bool isFemale, const char* nodeName,
                                  uint16_t key, uint8_t index)                                 = 0;
  virtual void SetNodeProperties(RE::TESObjectREFR* refr, bool immediate)                      = 0;
  virtual void SetNodeProperty(RE::TESObjectREFR* refr, bool firstPerson, const char* nodeName,
                               uint16_t key, uint8_t index, SetVariant& value, bool immediate) = 0;
  virtual bool GetNodeProperty(RE::TESObjectREFR* refr, bool firstPerson, const char* nodeName,
                               uint16_t key, uint8_t index, GetVariant& value)                 = 0;
  virtual void ApplyNodeOverrides(RE::TESObjectREFR* refr, RE::NiAVObject* object,
                                  bool immediate)                                              = 0;
  virtual void RemoveAllNodeOverrides()                                                        = 0;
  virtual void RemoveAllNodeOverridesByReference(RE::TESObjectREFR* reference)                 = 0;
  virtual void RemoveAllNodeOverridesByNode(RE::TESObjectREFR* refr, bool isFemale,
                                            const char* nodeName)                              = 0;

  virtual bool HasSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson,
                               uint32_t slotMask, uint16_t key, uint8_t index)                 = 0;
  virtual void AddSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson,
                               uint32_t slotMask, uint16_t key, uint8_t index,
                               SetVariant& value)                                              = 0;
  virtual bool GetSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson,
                               uint32_t slotMask, uint16_t key, uint8_t index,
                               GetVariant& visitor)                                            = 0;
  virtual void RemoveSkinOverride(RE::TESObjectREFR* refr, bool isFemale, bool firstPerson,
                                  uint32_t slotMask, uint16_t key, uint8_t index)              = 0;
  virtual void SetSkinProperties(RE::TESObjectREFR* refr, bool immediate)                      = 0;
  virtual void SetSkinProperty(RE::TESObjectREFR* refr, bool firstPerson, uint32_t slotMask,
                               uint16_t key, uint8_t index, SetVariant& value, bool immediate) = 0;
  virtual bool GetSkinProperty(RE::TESObjectREFR* refr, bool firstPerson, uint32_t slotMask,
                               uint16_t key, uint8_t index, GetVariant& value)                 = 0;
  virtual void ApplySkinOverrides(RE::TESObjectREFR* refr, bool firstPerson,
                                  RE::TESObjectARMO* armor, RE::TESObjectARMA* addon,
                                  uint32_t slotMask, RE::NiAVObject* object, bool immediate)   = 0;
  virtual void RemoveAllSkinOverrides()                                                        = 0;
  virtual void RemoveAllSkinOverridesByReference(RE::TESObjectREFR* reference)                 = 0;
  virtual void RemoveAllSkinOverridesBySlot(RE::TESObjectREFR* refr, bool isFemale,
                                            bool firstPerson, uint32_t slotMask)               = 0;
};

class IPresetInterface : public IPluginInterface
{
public:
  enum
  {
    kPluginVersion1       = 1,
    kCurrentPluginVersion = kPluginVersion1,
  };

  enum ApplyTypes
  {
    kPresetApplyFace          = (0 << 0),
    kPresetApplyOverrides     = (1 << 0),
    kPresetApplyBodyMorphs    = (1 << 1),
    kPresetApplyTransforms    = (1 << 2),
    kPresetApplySkinOverrides = (1 << 3),
    kPresetApplyAll           = kPresetApplyFace | kPresetApplyOverrides | kPresetApplyBodyMorphs |
                                kPresetApplyTransforms | kPresetApplySkinOverrides
  };

  virtual bool SavePreset(const char* filePath, const char* tintPath, RE::Actor* actor) = 0;
  virtual bool LoadPreset(const char* filePath, const char* tintPath, RE::Actor* actor,
                          ApplyTypes applyTypes = kPresetApplyAll)                      = 0;
};
}  // namespace SKEE
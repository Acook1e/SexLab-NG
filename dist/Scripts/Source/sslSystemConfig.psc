scriptname sslSystemConfig extends sslSystemLibrary

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslSystemConfig 在 SLNG 中的迁移方案，不提供运行时实现。
; 当前总策略:
; - 注册统一在 SKSE 侧实现。
; - 能直接 wrap 到 NG runtime/native backend 的接口优先做原签名转发。
; - 不能等价迁移的接口允许空实现或安全默认值，优先保证旧脚本可加载、旧模组不因缺符号直接崩溃。
; - Quest / Alias / Thread 对象在 NG 中可以退化为稳定句柄或 ID，不要求复刻旧 SexLab.esm 的 Quest 拓扑。
; - 旧 MCM 不作为迁移目标；UI/配置迁移到 SKSE Menu Framework / PrismaUI / NG UI。
; - 若下方旧英文说明与本段冲突，以本段为准。
; Native 优先级: P1
;
; File role:
; - Configuration quest for gameplay flags, hotkeys, profiles, and resource handles.
; - Main carrier for CK-bound properties such as Faction, Spell, Keyword, FormList, Message, Sound, and VisualEffect.

; Native API: keep these in SKSE
;
; bool SetAdjustmentProfile(string ProfileName) global native
; Purpose: switch the active adjustment profile.
; Params: ProfileName = profile key to load.
; SKSE work: keep a profile store keyed by name and bind it to the active animation adjustment backend.
;
; bool SaveAdjustmentProfile() global native
; Purpose: persist the active adjustment profile.
; Params: none.
; SKSE work: serialize current profile contents and expose success/failure.

; Non-native property groups
;
; DebugMode property
; Purpose: toggle debug mode and trigger side effects.
; Side effects in original script: user log open/close, console print, add/remove spells, send ModEvent.
; Why not native: this is script-side orchestration over PlayerRef, spell forms, Debug API, and ModEvent dispatch.
;
; Resource properties
; Examples: AnimatingFaction, ForbiddenFaction, NudeSuit, SelectedSpell, CumOralKeyword, BedsList, UseBed, HotkeyUp, HotkeyDown.
; Why not native: these values come from ESM/CK property binding and should remain data owned by the plugin content, not hardcoded in DLL logic.
;
; Direct ESM lookups already present in the original script
; - Spells 0x073CC and 0x5FE9B for debug mode toggles.
; - VisualEffect 0x8FC39 and 0x8FC3A.
; - Hotkey sounds 0x8AAF0..0x8AAF5.
; Why this matters: not every legacy boundary is an API; many are direct content references that still require an ESM-backed data source.

; Legacy alias properties
; Examples: bRestrictAggressive, bAllowCreatures, bUseStrapons, bUseCum, bAutoTFC.
; Purpose: old naming compatibility only.
; Why not native: these are simple aliases over stored config values.
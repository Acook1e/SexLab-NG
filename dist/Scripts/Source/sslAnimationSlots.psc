scriptname sslAnimationSlots extends Quest

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslAnimationSlots 在 SLNG 中的迁移方案，不提供运行时实现。
; 当前总策略:
; - 注册统一在 SKSE 侧实现。
; - 能直接 wrap 到 NG runtime/native backend 的接口优先做原签名转发。
; - 不能等价迁移的接口允许空实现或安全默认值，优先保证旧脚本可加载、旧模组不因缺符号直接崩溃。
; - Quest / Alias / Thread 对象在 NG 中可以退化为稳定句柄或 ID，不要求复刻旧 SexLab.esm 的 Quest 拓扑。
; - 旧 MCM 不作为迁移目标；UI/配置迁移到 SKSE Menu Framework / PrismaUI / NG UI。
; - 若下方旧英文说明与本段冲突，以本段为准。
; Native 优先级: P0
;
; File role:
; - Main animation registry and cache for human animations.
; - Handles filtering by tags, actor count, genders, stage count, suppression, paging, and registration.

; Main API groups
; - Query: GetByTags, GetByType, PickByActors, GetByDefault, GetByDefaultTags, GetBySlot, GetByName.
; - Registry: Register(string Registrar), RegisterAnimation(string Registrar, Form CallbackForm = none, ReferenceAlias CallbackAlias = none), UnregisterAnimation.
; - Cache and stats: ValidateCache, CheckCache, CountTagUsage, GetAllTags, GetDisabledCount, GetSuppressedList.

; ESM dependencies
; - Loads default content through (Game.GetFormFromFile(0x639DF, "SexLab.esm") as sslAnimationDefaults).LoadAnimations().
; - Rehydrates framework references via SexLab.esm quest 0x0D62.

; Why this file should not be fully native
; - Registration is callback-driven through Papyrus forms and aliases.
; - Cache keys, suppression lists, and tag filtering are part of the legacy script contract external mods already expect.
; - Native indexing can exist underneath, but this facade should remain script-owned.
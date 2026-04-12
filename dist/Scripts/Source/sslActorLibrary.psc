scriptname sslActorLibrary extends sslSystemLibrary

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslActorLibrary 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Central actor helper library for cum application, stripping, validation, and gender logic.
; - Strongly tied to config quest properties such as factions, spells, keywords, and equipment forms.

; Main API groups
; - Cum helpers: CountCum, ApplyCum, AddCum, ClearCum.
; - Strip helpers: StripActor(Actor ActorRef, Actor VictimRef = none, bool DoAnimate = true, bool LeadIn = false), StripSlot, StripSlots.
; - Validation helpers: ValidateActor(Actor ActorRef), CanAnimate(Actor ActorRef), IsValidActor(Actor ActorRef), IsForbidden(Actor ActorRef).
; - Gender helpers: GetGender(Actor ActorRef), GenderCount(Actor[] Positions), MaleCount, FemaleCount, CreatureCount, MakeGenderTag.

; Why this file should not be fully native
; - Most functions combine CK-bound properties from SexLab.esm with inventory state, factions, spells, worn forms, and keyword checks.
; - Strip and cum logic are gameplay policy functions, not just math/storage helpers.
; - Small count/tag helpers could be optimized later, but the file-level contract belongs to script-side behavior.

; ESM-backed data used by this file
; - Faction/weapon/armor/spell/keyword properties injected from the framework quest.
; - These records are content data, not algorithmic state that belongs in SKSE.
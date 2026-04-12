scriptname sslThreadLibrary extends sslSystemLibrary

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslThreadLibrary 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Search and sorting library for actor selection, bed selection, and position ordering.

; Main API groups
; - Actor search: FindAvailableActor(ObjectReference CenterRef, float Radius = 5000.0, int FindGender = -1, ...), FindAvailableActorInFaction, FindAvailableActorWornForm.
; - Partner search: FindAvailablePartners(Actor[] Positions, int total, int males = -1, int females = -1, float radius = 10000.0).
; - Sorting: SortActors, SortActors_Legacy, SortActorsByAnimation, SortCreatures.
; - Bed helpers: IsBedRoll, IsDoubleBed, IsSingleBed, GetBedType, IsBedAvailable, FindBed.

; Why this file should not be fully native
; - Searches depend on ActorLib validation rules, config flags, loaded references, same-floor heuristics, and legacy fallback ordering.
; - Bed lookup depends on FormList content from SexLab.esm and live reference state in the loaded area.
; - A future NG native can accelerate candidate enumeration, but the legacy policy layer should stay script-visible.

; ESM-backed data used by this file
; - BedsList, DoubleBedsList, BedRollsList, FurnitureBedRoll are CK data carried by SexLab.esm.
scriptname sslActorAlias extends ReferenceAlias

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslActorAlias 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Runtime alias for one actor inside a running legacy thread.
; - Owns event flow, stage sync, position data, and per-alias state.

; Native API: implement only the heavy math helpers in SKSE
;
; function OffsetCoords(float[] Output, float[] CenterCoords, float[] OffsetBy) global native
; Purpose: vector math helper used while placing scene actors.
; Params: Output = target array, CenterCoords = source coordinates, OffsetBy = delta.
;
; bool IsInPosition(Actor CheckActor, ObjectReference CheckMarker, float maxdistance = 30.0) global native
; Purpose: fast actor-vs-marker proximity and placement check.
; Params: actor, marker, maxdistance.
;
; int CalcEnjoyment(float[] XP, float[] SkillsAmounts, bool IsLeadin, bool IsFemaleActor, float Timer, int OnStage, int MaxStage) global native
; Purpose: raw enjoyment formula for one actor in one update tick.
; Params: skill arrays, flags, timer, current stage, max stage.
;
; SKSE work for this file:
; - Keep math and spatial checks deterministic.
; - Avoid owning alias lifecycle in native code.

; Why the rest should stay non-native:
; - sslActorAlias is a ReferenceAlias script with heavy event/state behavior.
; - It reacts to quest stages, actor package changes, expression application, orgasm flow, and animation events.
; - Native code can accelerate formulas, but should not replace alias state machines owned by the ESM quest layout.
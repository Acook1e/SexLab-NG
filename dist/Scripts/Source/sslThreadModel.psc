scriptname sslThreadModel extends Quest hidden

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslThreadModel 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Per-scene thread data model.
; - Owns actors, genders, timers, bed state, hook tags, animation candidates, and startup configuration.

; Main API groups
; - Actor setup: AddActor(Actor ActorRef, bool IsVictim = false, sslBaseVoice Voice = none, bool ForceSilent = false), AddActors(Actor[] ActorList, Actor VictimActor = none).
; - Startup: StartThread(), CenterOnBed(bool AskPlayer = true, float Radius = 750.0).
; - Queries: GetVoice(Actor ActorRef), GetExpression(Actor ActorRef), GetEnjoyment(Actor ActorRef), GetPosition(Actor ActorRef), HasActor(Actor ActorRef), IsVictim(Actor ActorRef), IsAggressor(Actor ActorRef).
; - Animation selection state: GetForcedAnimations(), GetAnimations(), FilterAnimations().
; - Hook/tag state: GetHook(), GetHooks(), AddTag(string Tag), CheckTags(string[] CheckTags, bool RequireAll = true, bool Suppress = false).

; ESM dependencies
; - Reloads framework quest 0x0D62, registry quest 0x664FB, and animation quest 0x639DF.

; Why this file should not be fully native
; - It is the core quest-owned state machine for one running scene.
; - It stores many Papyrus-visible properties and aliases and depends on quest topology defined in the ESM.
; - Native code should only assist with formulas or backend storage, not replace the model object.
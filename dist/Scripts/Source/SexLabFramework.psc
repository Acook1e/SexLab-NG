scriptname SexLabFramework extends Quest

; 说明文件 / Documentation only
; 本文件只用于描述 legacy SexLabFramework 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Main external API entry used by older mods.
; - High-level facade over actor libraries, thread slots, animation registries, stats, and factory objects.
;
; ESM dependencies:
; - SexLab.esm quest 0x0D62: framework, config, actor lib, thread lib, stats, thread slots.
; - SexLab.esm quest 0x639DF: animation slots.
; - SexLab.esm quest 0x664FB: creature slots, expression slots, voice slots.
; - SexLab.esm quest 0x78818: object factory.

; Scene and thread API
; - sslThreadModel NewThread(float TimeOut = 30.0)
; - int StartSex(Actor[] Positions, sslBaseAnimation[] Anims, Actor Victim = none, ObjectReference CenterOn = none, bool AllowBed = true, string Hook = "")
; - sslThreadController QuickStart(Actor Actor1, Actor Actor2 = none, Actor Actor3 = none, Actor Actor4 = none, Actor Actor5 = none, Actor Victim = none, string Hook = "", string AnimationTags = "")
; Purpose: expose one-call scene startup and thread reservation to external mods.
; Why not fully native: this layer is a legacy facade that composes thread allocation, actor ordering, event hooks, animation fallback, and config checks.
; Native split that does make sense: scene search, scene start primitives, and low-level controller lookup.

; Actor and controller API
; - int GetGender(Actor ActorRef)
; - int ValidateActor(Actor ActorRef)
; - Actor FindAvailableActor(ObjectReference CenterRef, float Radius = 5000.0, int FindGender = -1, ...)
; - sslThreadController GetController(int tid)
; - int FindActorController(Actor ActorRef)
; Purpose: query actor eligibility, search nearby actors, and map actors to active thread controllers.
; Why not fully native: results depend on running quest slots, alias state, config factions, and legacy fallback rules.

; Animation API
; - sslBaseAnimation[] GetAnimationsByTags(int ActorCount, string Tags, string TagSuppress = "", bool RequireAll = true)
; - sslBaseAnimation[] GetAnimationsByDefault(int Males, int Females, bool IsAggressive = false, bool UsingBed = false, bool RestrictAggressive = true)
; - sslBaseAnimation RegisterAnimation(string Registrar, Form CallbackForm = none, ReferenceAlias CallbackAlias = none)
; - sslBaseAnimation RegisterCreatureAnimation(string Registrar, Form CallbackForm = none, ReferenceAlias CallbackAlias = none)
; - sslBaseVoice RegisterVoice(string Registrar, Form CallbackForm = none, ReferenceAlias CallbackAlias = none)
; - sslBaseExpression RegisterExpression(string Registrar, Form CallbackForm = none, ReferenceAlias CallbackAlias = none)
; Purpose: query animation registries and let addon packs register content.
; Why not fully native: registration is callback-driven and tightly coupled to slots quests plus object factory script objects.

; Stats API
; - int RegisterStat(string Name, string Value, string Prepend = "", string Append = "")
; - string GetActorStat(Actor ActorRef, string Name)
; - int GetActorStatInt(Actor ActorRef, string Name)
; - string SetStat(string Name, string Value)
; Purpose: legacy custom-stat facade for external mods.
; Why not native at framework level: storage can move native, but name-based compatibility mapping and PlayerRef shortcut semantics belong to script wrappers.

; Pure helper API
; - int CalcSexuality(bool IsFemale, int males, int females)
; - int CalcLevel(float total, float curve = 0.65)
; - string ParseTime(int time)
; Purpose: tiny math and formatting helpers.
; Why not native: deterministic one-screen script logic, no runtime benefit from DLL ownership.
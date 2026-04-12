scriptname sslActorStats extends sslSystemLibrary

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslActorStats 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Actor skill/stat backend.
; - Best candidate for a real SKSE native-backed store.

; Native API: implement in SKSE under script name sslActorStats
;
; bool IsSkilled(Actor ActorRef) global native
; Purpose: test whether the actor already has a native stat record.
; Params: ActorRef = target actor.
;
; function _SeedActor(Actor ActorRef, float RealTime, float GameTime) global native
; Purpose: initialize a legacy stat record.
; Params: ActorRef, RealTime, GameTime.
;
; float _GetSkill(Actor ActorRef, int Stat) global native
; function _SetSkill(Actor ActorRef, int Stat, float Value) global native
; float _AdjustSkill(Actor ActorRef, int Stat, float By) global native
; float[] GetSkills(Actor ActorRef) global native
; Purpose: raw skill read/write API keyed by actor plus numeric stat id.
;
; function RecordThread(Actor ActorRef, int Gender, int HadRelation, float StartedAt, float RealTime, float GameTime, bool WithPlayer, Actor VictimRef, int[] Genders, float[] SkillXP) global native
; Purpose: push scene-end results into persistent actor stats.
; Params: actor identity, scene timing, relation flags, group gender summary, XP gains.
;
; function _ResetStats(Actor ActorRef) global native
; Actor[] GetAllSkilledActors() global native
; Purpose: maintenance and enumeration helpers.
;
; SKSE work for this file:
; - Keep a persistent per-actor store keyed by form id.
; - Serialize and restore data across save/load.
; - Preserve legacy stat slot ordering so string wrappers keep working.

; Non-native wrappers: keep in script
; - SeedActor(Actor ActorRef)
; - GetSkill(Actor ActorRef, string Skill)
; - SetSkill(Actor ActorRef, string Skill, int Amount)
; Why not native: these functions translate legacy skill names, apply config policy, and keep old wrapper semantics around the raw backend.
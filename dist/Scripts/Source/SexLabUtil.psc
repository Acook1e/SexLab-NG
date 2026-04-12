scriptname SexLabUtil hidden

; 说明文件 / Documentation only
; 本文件只用于描述 legacy SexLabUtil 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Global utility facade used by external mods and internal legacy scripts.
; - Mixes true native helpers with script-side orchestration wrappers.
;
; ESM dependencies:
; - GetAPI() / GetConfig() depend on SexLab.esm quest 0x0D62.

; Native API: implement directly in SKSE under script name SexLabUtil
;
; int GetPluginVersion() global native
; Purpose: expose the NG plugin version in a legacy-friendly integer form.
; Params: none.
; SKSE work: read plugin version metadata and return a stable packed integer.
;
; bool HasKeywordSub(Form ObjRef, string LookFor) global native
; Purpose: substring match against an object's keyword data.
; Params: ObjRef = target form, LookFor = substring to find.
; SKSE work: inspect keywords or cached keyword text and apply consistent matching rules.
;
; string RemoveSubString(string InputString, string RemoveString) global native
; Purpose: legacy string helper.
; Params: InputString = source text, RemoveString = text to remove.
; SKSE work: pure string processing only.
;
; function PrintConsole(string output) global native
; Purpose: send legacy debug text to console/log.
; Params: output = final message.
; SKSE work: forward to logger and optional in-game console.
;
; function VehicleFixMode(int mode) global native
; Purpose: legacy compatibility hook for old runtime fixes.
; Params: mode = legacy fix mode id.
; SKSE work: keep as no-op plus logging until a real NG use case is confirmed.
;
; Native helper group:
; - FloatIfElse / IntIfElse / StringIfElse / FormIfElse / ActorIfElse / ObjectIfElse / AliasIfElse
; - MakeActorArray
; - IntMinMaxValue / IntMinMaxIndex / FloatMinMaxValue / FloatMinMaxIndex
; - GetCurrentGameRealTime
; Common params: boolean selector, values to return, or source arrays.
; SKSE work: lightweight VM value selection, array construction, min/max scans, and real-time lookup.

; Non-native API: keep as script-side facade
;
; SexLabFramework GetAPI() global
; sslSystemConfig GetConfig() global
; Purpose: resolve legacy quest script instances from SexLab.esm.
; Why not native: this is ESM form binding and script cast policy, not algorithmic work.
;
; int StartSex(Actor[] sexActors, sslBaseAnimation[] anims, Actor victim = none, ObjectReference centerOn = none, bool allowBed = true, string hook = "") global
; sslThreadModel NewThread(float timeout = 30.0) global
; sslThreadController QuickStart(Actor a1, Actor a2 = none, Actor a3 = none, Actor a4 = none, Actor a5 = none, Actor victim = none, string hook = "", string animationTags = "") global
; Purpose: legacy convenience entry points for scene startup and thread reservation.
; Why not native: these are orchestration wrappers over quest state, animation registries, hook dispatch, and config rules.
;
; ActorName / GetGender / IsActorActive / IsValidActor / HasCreature / HasRace
; Purpose: actor and race queries.
; Why not fully native: the heavy primitives can move native, but this legacy facade still depends on config quest state and old wrapper semantics.
;
; MakeGenderTag / GetGenderTag / GetReverseGenderTag / IsActor / IsImportant
; Purpose: small formatting and validation helpers.
; Why not native: trivial script logic, low value in moving to DLL.
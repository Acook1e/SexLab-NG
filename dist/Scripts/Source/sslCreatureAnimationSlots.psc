scriptname sslCreatureAnimationSlots extends sslAnimationSlots

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslCreatureAnimationSlots 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Creature race-key registry plus creature animation lookup facade.
; - Good candidate for native-backed race metadata storage.

; Native API: implement in SKSE under script name sslCreatureAnimationSlots
;
; string GetRaceKey(Race RaceRef) global native
; string GetRaceKeyByID(string RaceID) global native
; function AddRaceID(string RaceKey, string RaceID) global native
; bool HasRaceID(string RaceKey, string RaceID) global native
; bool HasRaceKey(string RaceKey) global native
; bool ClearRaceKey(string RaceKey) global native
; bool HasRaceIDType(string RaceID) global native
; bool HasCreatureType(Actor ActorRef) global native
; bool HasRaceType(Race RaceRef) global native
; string[] GetAllRaceKeys(Race RaceRef = none) global native
; string[] GetAllRaceIDs(string RaceKey) global native
; Race[] GetAllRaces(string RaceKey) global native
;
; SKSE work:
; - Maintain a persistent bidirectional race-key mapping.
; - Resolve actor/race classification quickly.
; - Return arrays in legacy order expected by existing scripts.

; Non-native wrappers: keep in script
; - GetByRace, GetByRaceTags, GetByCreatureActorsTags, cache helpers.
; Why not native: these functions mostly combine tags, cache strings, fallback rules, and array filtering over the registry result.
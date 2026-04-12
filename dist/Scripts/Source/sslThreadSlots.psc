scriptname sslThreadSlots extends Quest

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslThreadSlots 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Owner of the 15 preplaced legacy thread controller quests.
; - Provides thread/model lookup to the rest of the framework.

; Public API
; - sslThreadModel PickModel(float TimeOut = 30.0)
; - sslThreadController GetController(int tid)
; - int FindActorController(Actor ActorRef)
; - sslThreadController GetActorController(Actor ActorRef)
; - bool IsRunning()
; - int ActiveThreads()
; - bool TestSlots()
;
; ESM dependencies
; - Framework quest: SexLab.esm 0x0D62.
; - 15 fixed controller quest records: 0x61EEF, 0x62452, 0x6C62C..0x6C638.

; Why not fully native:
; - These are real quest instances placed in the ESM, not abstract runtime handles.
; - Setup() stops and restarts quest records and relies on quest state transitions.
; - A helper native for fast lookup is reasonable, but ownership of slot lifecycle should stay script-side.
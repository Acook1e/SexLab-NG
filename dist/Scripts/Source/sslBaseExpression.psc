scriptname sslBaseExpression extends sslBaseObject

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslBaseExpression 在 SLNG 中的迁移方案，不提供运行时实现。
; 当前总策略:
; - 注册统一在 SKSE 侧实现。
; - 能直接 wrap 到 NG runtime/native backend 的接口优先做原签名转发。
; - 不能等价迁移的接口允许空实现或安全默认值，优先保证旧脚本可加载、旧模组不因缺符号直接崩溃。
; - Quest / Alias / Thread 对象在 NG 中可以退化为稳定句柄或 ID，不要求复刻旧 SexLab.esm 的 Quest 拓扑。
; - 旧 MCM 不作为迁移目标；UI/配置迁移到 SKSE Menu Framework / PrismaUI / NG UI。
; - 若下方旧英文说明与本段冲突，以本段为准。
; Native 优先级: P2
;
; File role:
; - Expression preset object used by face and mouth animation helpers.

; Native API: implement direct MFG reads in SKSE
;
; float GetModifier(Actor ActorRef, int id) global native
; float GetPhoneme(Actor ActorRef, int id) global native
; float GetExpression(Actor ActorRef, bool getId) global native
;
; Purpose: read current actor facial state without script-side loops.
; Params: ActorRef = target actor, id = modifier or phoneme index, getId = expression mode flag.
; SKSE work: read actor face data safely and return values in the exact legacy ranges.

; Non-native logic: keep in script
; - ClearPhoneme, apply/remove preset arrays, tag filtering, and expression selection.
; Why not fully native: these parts are mostly wrapper logic around presets and external helper mods such as MfgConsoleFunc.
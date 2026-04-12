scriptname sslSystemLibrary extends Quest hidden

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslSystemLibrary 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Shared loader for core quest references used by multiple legacy scripts.
; - Central place where legacy scripts re-hydrate quest-backed libraries after load/reset.

; Public script-side entry
; - function LoadLibs(bool Forced = false)
; Purpose: resolve framework, thread, actor, stats, animation, creature, expression, and voice libraries.
; Params: Forced = re-resolve even if cached references already exist.

; ESM dependencies resolved here
; - SexLab.esm 0x0D62: framework/config/threadlib/threadslots/actorlib/stats.
; - SexLab.esm 0x639DF: animation slots.
; - SexLab.esm 0x664FB: creature slots, expression slots, voice slots.

; Why not native:
; - This file is mostly ESM form lookup plus script casting.
; - It also registers for the SexLabDebugMode mod event at the script layer.
; - Replacing it with native code would move content ownership and quest binding policy into DLL code for little gain.
scriptname TestGlobalHook extends sslThreadHook

; 说明文件 / Documentation only
; 本文件只用于描述 legacy TestGlobalHook 在 SLNG 中的迁移方案，不提供运行时实现。
; 当前总策略:
; - 注册统一在 SKSE 侧实现。
; - 能直接 wrap 到 NG runtime/native backend 的接口优先做原签名转发。
; - 不能等价迁移的接口允许空实现或安全默认值，优先保证旧脚本可加载、旧模组不因缺符号直接崩溃。
; - Quest / Alias / Thread 对象在 NG 中可以退化为稳定句柄或 ID，不要求复刻旧 SexLab.esm 的 Quest 拓扑。
; - 旧 MCM 不作为迁移目标；UI/配置迁移到 SKSE Menu Framework / PrismaUI / NG UI。
; - 若下方旧英文说明与本段冲突，以本段为准。
; Native 优先级: P3
;
; File role:
; - Example/test implementation of sslThreadHook.

; Example callbacks shown here
; - AnimationStarting(sslThreadModel Thread)
; - AnimationPrepare(sslThreadController Thread)
;
; Purpose
; - Demonstrates how external Papyrus mods can observe or gate the legacy hook flow.

; Why this file should not be native
; - It is sample/test consumer content, not framework runtime logic.
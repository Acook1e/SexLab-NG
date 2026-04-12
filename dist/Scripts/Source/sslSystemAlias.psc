scriptname sslSystemAlias extends ReferenceAlias

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslSystemAlias 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - System bootstrap alias used to initialize or repair the legacy framework runtime.

; Main API groups
; - SetupSystem()
; - global helpers IsActor(Form FormRef) and IsImportant(Actor ActorRef, bool Strict = false)
; - internal LoadLibs() style resource recovery and default content loading.

; ESM dependencies
; - Framework quest 0x0D62
; - Animations quest 0x639DF
; - Registry quest 0x664FB
; - Object factory quest 0x78818
; - Can also trigger creature voice defaults from 0x664FB.

; Why this file should not be native
; - It exists specifically to coordinate quest aliases and recovery logic after load/install.
; - The job is ESM topology management, not computational work.
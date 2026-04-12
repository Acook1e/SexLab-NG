scriptname sslExpressionSlots extends Quest

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslExpressionSlots 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Registry and picker for expression presets.

; Main API groups
; - Picking: PickExpression(Actor ActorRef, Actor VictimRef = none), PickByStatus, GetByStatus, RandomByTag, GetByTag.
; - Registry: Register(string Registrar), RegisterExpression(string Registrar, Form CallbackForm = none, ReferenceAlias CallbackAlias = none), UnregisterExpression.
; - Browsing: GetBySlot, GetByName, GetSlots, PageCount, FindPage.

; ESM dependencies
; - Loads defaults via (Game.GetFormFromFile(0x664FB, "SexLab.esm") as sslExpressionDefaults).LoadExpressions().

; Why this file should not be fully native
; - Preset objects, selection rules, and callback registration are legacy Papyrus content flows.
; - Native code should only own the low-level facial state access already declared in sslBaseExpression.
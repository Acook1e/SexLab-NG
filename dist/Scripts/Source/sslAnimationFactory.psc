scriptname sslAnimationFactory extends Quest hidden

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslAnimationFactory 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Build and register animation objects from script and JSON.
; - Attach default SFX resources to new animation objects.

; Main entry points
; - function PrepareFactory()
; - function PrepareFactoryCreatures()
; - function RegisterAnimation(string Registrar)
; - sslBaseAnimation Create(int id)
; - sslBaseAnimation RegisterJSON(string Filename)
; - bool ValidateJSON(string Filename)
; - Sound StringSFX(string sfx)

; ESM dependencies
; - Animation slots quest 0x639DF.
; - Creature animation slots quest 0x664FB.
; - Sound records 0x65A31..0x65A34.

; Why not fully native:
; - This is content authoring and callback glue, not just data storage.
; - JSON validation can move native later if needed, but the current script flow is strongly tied to slot registration and modder callback conventions.
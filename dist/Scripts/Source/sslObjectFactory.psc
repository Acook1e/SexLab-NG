scriptname sslObjectFactory extends sslSystemLibrary

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslObjectFactory 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Temporary object registry for animation, voice, and expression objects.
; - Also exposes legacy constant getters and shared SFX getters.

; Public API groups
; - Global constant helpers: Male, Female, Creature, Vaginal, Oral, Anal, Misc, Sexual, Foreplay.
; - Shared SFX getters: Squishing, Sucking, SexMix, Squirting.
; - Animation object flow: NewAnimation, GetSetAnimation, NewAnimationCopy, GetAnimation, ReleaseAnimation, ReleaseOwnerAnimations.
; - Voice object flow: NewVoice, GetSetVoice, NewVoiceCopy, GetVoice, ReleaseVoice, ReleaseOwnerVoices.
; - Expression object flow: NewExpression, GetSetExpression, NewExpressionCopy, GetExpression, ReleaseExpression, ReleaseOwnerExpressions.

; ESM dependencies
; - Shared SFX sound records 0x65A31..0x65A34.

; Why not fully native:
; - This file manages Papyrus object identity, callback names, and owner forms.
; - Native code can accelerate backing storage later, but should not replace the script-visible object lifecycle contract too early.
scriptname SLNG_LegacyMigrationPlan hidden

; 说明文件 / Documentation only
; 本文件汇总 legacy Papyrus 迁移总策略、ESM 数据用途、替代方案与 native 优先级。
; 不提供运行时实现。

; 一、总迁移原则
; 1. 所有“注册”统一下沉到 SKSE：动画、语音、表情、种族键值、统计后端、调整参数后端都优先在 native 层实现。
; 2. 能直接 wrap 的 legacy 接口保留原签名：Papyrus 只是壳，SKSE 直接转发到 NG runtime/backend。
; 3. 无法等价迁移的 legacy 接口允许空实现或安全默认值：目标是先保证旧脚本加载成功、旧模组不会因为找不到符号或对象而直接崩溃。
; 4. 旧 Quest / Alias / Thread Controller 不再默认视为必须复刻的 ESM 拓扑：在 NG 中它们可以只是稳定句柄、随机分配的 ID、或由 SKSE 维护的运行时对象映射。
; 5. 旧 MCM 不再是目标实现：配置和调试入口迁移到 SKSE Menu Framework / PrismaUI / NG UI。

; 二、从 ESM 获取数据的目的
; A. 运行时拓扑
; - 旧版用 Quest / Alias / Thread Slot / Registry Quest 作为运行时骨架。
; - 典型记录: 0x0D62, 0x639DF, 0x664FB, 0x78818, 以及 15 个 thread controller quest。
; - 这些数据的目的不是“内容”，而是承载旧版运行时对象图。
;
; B. 资源内容
; - Spell, Sound, Keyword, Message, FormList, Faction, Armor, Weapon, Topic, VisualEffect 等。
; - 这些记录的目的是给 legacy 脚本提供可直接引用的 CK 对象。
;
; C. 默认内容入口
; - 默认动画、默认语音、默认表情等内容装填脚本，会通过 ESM 上的 quest/defaults 记录触发注册。

; 三、NG 能否不依靠 ESM
; 1. 如果“不依靠 ESM”指的是“不依赖旧 SexLab.esm”: 可以。
; - 运行时拓扑可以完全搬到 SKSE。
; - 注册行为可以完全搬到 SKSE。
; - 线程/控制器/registry 可以变成 runtime handle + Papyrus facade。
;
; 2. 如果“不依靠 ESM”指的是“不依赖任何插件数据文件”: 基本不现实。
; - Spell / Sound / Message / Keyword / FormList / Armor / Topic / VisualEffect 这类 CK 对象仍然需要某种插件数据源承载。
; - 你可以不再依赖旧 SexLab.esm，但通常仍需要 NG 自己的 esm/esp/esl，或者把一部分非 Form 数据改成 JSON 并由 SKSE 在运行时构造代理对象。
;
; 3. 最合理的目标
; - 不依赖旧 SexLab.esm 的 Quest 拓扑和旧注册流程。
; - 尽量减少对旧 FormID 的硬依赖。
; - 用 NG 自己的数据源替代必须保留的资源记录。

; 四、初版 native 优先级
; P0: 必须最先落地
; - SexLabFramework
; - sslActorStats
; - sslAnimationSlots
; - sslCreatureAnimationSlots
; - sslVoiceSlots
; - sslExpressionSlots
; - sslBaseAnimation
;
; P1: 紧随其后
; - SexLabUtil
; - sslSystemConfig
; - sslActorLibrary
; - sslThreadLibrary
; - sslThreadSlots
; - sslThreadModel
; - sslThreadController
; - sslObjectFactory
; - sslActorAlias
;
; P2: 可以后移，更多是 facade / helper / compatibility shell
; - sslBaseExpression
; - sslBaseVoice
; - sslBaseObject
; - sslSystemLibrary
; - sslSystemAlias
; - sslAnimationFactory
; - sslVoiceFactory
; - sslExpressionFactory
; - sslThreadHook
; - sslUtility
;
; P3: 可以直接空实现或重定向，不值得早期投入
; - sslConfigMenu
; - sslTroubleshoot
; - sslActorCumEffect
; - sslAnimationDefaults
; - sslCreatureAnimationDefaults
; - sslExpressionDefaults
; - sslVoiceDefaults
; - sslEffectDebug
; - sslEffectEditor
; - sslBenchmark
; - TestGlobalHook
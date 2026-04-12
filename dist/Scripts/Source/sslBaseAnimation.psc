scriptname sslBaseAnimation extends sslBaseObject

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslBaseAnimation 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Animation metadata object.
; - Owns stage count, positions, tags, offsets, and adjustment profile access.

; Native API: implement adjustment storage and retrieval in SKSE
;
; float[] _GetStageAdjustments(string Registrar, string AdjustKey, int Stage) global native
; float[] _GetAllAdjustments(string Registrar, string AdjustKey) global native
; bool _HasAdjustments(string Registrar, string AdjustKey, int Stage) global native
; function _PositionOffsets(string Registrar, string AdjustKey, string LastKey, int Stage, float[] RawOffsets) global native
; function _SetAdjustment(string Registrar, string AdjustKey, int Stage, int Slot, float Adjustment) global native
; float _GetAdjustment(string Registrar, string AdjustKey, int Stage, int nth) global native
; float _UpdateAdjustment(string Registrar, string AdjustKey, int Stage, int nth, float by) global native
; function _ClearAdjustments(string Registrar, string AdjustKey) global native
; bool _CopyAdjustments(string Registrar, string AdjustKey, float[] Array) global native
; string[] _GetAdjustKeys(string Registrar) global native
; int _GetSchlong(string Registrar, string AdjustKey, string LastKey, int Stage) global native
;
; SKSE work:
; - Keep a persistent adjustment backend keyed by Registrar + AdjustKey + Stage + Slot.
; - Support profile save/load used by sslSystemConfig.
; - Merge or override offsets in the same order legacy scripts expect.

; Non-native logic: keep in script
; - Registry metadata, tag strings, stage arrays, fallback offset composition, and legacy object semantics.
; Why not fully native: this object is more than data storage; it is also a script-visible content model.
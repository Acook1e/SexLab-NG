scriptname sslConfigMenu extends SKI_ConfigBase

; 说明文件 / Documentation only
; 本文件只用于描述 legacy sslConfigMenu 在 SLNG 中的迁移方案，不提供运行时实现。
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
; - Main MCM menu for legacy SexLab.

; Representative API groups
; - Version/UI helpers: GetVersion(), GetStringVer(), PaginationMenu(...), MapOptions().
; - Input/config helpers: KeyConflict(int newKeyCode, string conflictControl, string conflictName), GetTimers(), GetStripping(int type).
; - Inventory helpers: GetItems(Actor ActorRef, bool FullInventory = false), GetEquippedItems(Actor ActorRef), GetFullInventory(Actor ActorRef), GetStripState(Form ItemRef).
; - Maintenance: ResetAllQuests(), ResetQuest(Quest QuestRef), DoDisable(bool check).

; ESM dependencies
; - Framework/animation/registry/object-factory quests: 0x0D62, 0x639DF, 0x664FB, 0x78818.
; - Reads debug mode from the framework quest and can reset those quest records directly.

; Why this file should not be native
; - It is a SkyUI MCM script with UI callbacks, option state, and menu-only flow.
; - Native code has no advantage owning UI menu layout or user-option scripting behavior.
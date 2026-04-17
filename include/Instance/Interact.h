#pragma once

#include "Define/ActorBody.h"
#include "Define/Gender.h"
#include "Define/Race.h"
#include "Define/Tag.h"

namespace Instance
{

// -----------------------------------------------------------------------------
// 前向声明
// -----------------------------------------------------------------------------
class Interact;
struct InteractionSnapshot;
struct MotionSnapshot;

// -----------------------------------------------------------------------------
// 交互类型枚举（精简，复合类型由后期推导）
// -----------------------------------------------------------------------------
enum class InteractionType : std::uint8_t
{
  None = 0,

  // 口部
  Kiss,
  BreastSucking,
  ToeSucking,
  Cunnilingus,
  Anilingus,
  Fellatio,
  DeepThroat,

  // 手/手指
  GropeBreast,
  GropeThigh,
  GropeButt,
  GropeFoot,
  FingerVagina,
  FingerAnus,
  Handjob,
  Masturbation,

  // 阴茎
  Titfuck,
  Naveljob,
  Thighjob,
  Frottage,
  Footjob,
  Vaginal,
  Anal,

  // 女同
  Tribbing,

  // 复合类型（由系统推断，不直接参与候选生成）
  SixtyNine,
  Spitroast,
  DoublePenetration,
  TriplePenetration,

  Count
};

// -----------------------------------------------------------------------------
// 资源类型（可被占用的身体部位）
// -----------------------------------------------------------------------------
enum class ResourceType : std::uint8_t
{
  Mouth,
  LeftHand,
  RightHand,
  Penis,
  // 可扩展：Tongue, LeftFoot, RightFoot...
};

// -----------------------------------------------------------------------------
// 槽位类型（可接收交互的身体部位）
// -----------------------------------------------------------------------------
enum class SlotType : std::uint8_t
{
  Mouth,
  Throat,
  Vagina,
  Anus,
  BreastLeft,
  BreastRight,
  Cleavage,
  Belly,
  ThighCleft,
  GlutealCleft,
  FootLeft,
  FootRight,
  // 可扩展...
};

// -----------------------------------------------------------------------------
// 几何证据（由证据层计算，结构化且不混合）
// -----------------------------------------------------------------------------
struct GeometricEvidence
{
  // 穿透相关（漏斗）
  float corePenetration = 0.f;  // 0~1，进入核心区程度
  float entryDistance   = 0.f;  // 带符号，入口平面距离（内为正）
  float radialDeviation = 0.f;  // 径向偏移归一化值

  // 表面接触相关（表面、夹缝）
  float surfaceContact   = 0.f;  // 0~1，表面压迫程度
  float enclosureFactor  = 0.f;  // 0~1，双侧包络度（夹缝专用）
  float alignmentQuality = 0.f;  // 0~1，轴向对齐质量

  // 辅助
  float depth    = 0.f;  // 当前穿透深度（世界单位）
  float maxDepth = 0.f;  // 槽位最大深度（世界单位）
};

// -----------------------------------------------------------------------------
// 意图先验（来自动画相位、轨迹预测等，仅用于调整置信度）
// -----------------------------------------------------------------------------
struct IntentPrior
{
  float animationBonus  = 0.f;  // 来自动画元数据的加成
  float trajectoryBonus = 0.f;  // 来自速度外推的加成
  float historyBonus    = 0.f;  // 与上一帧相同伙伴/槽位的加成
};

// -----------------------------------------------------------------------------
// 提议（由配对提议构建器生成，提交给槽位状态机）
// -----------------------------------------------------------------------------
struct Proposal
{
  RE::Actor* sourceActor = nullptr;
  RE::Actor* targetActor = nullptr;
  ResourceType sourceResource;
  SlotType targetSlot;
  InteractionType inferredType = InteractionType::None;

  GeometricEvidence evidence;
  IntentPrior prior;
  float timestamp = 0.f;  // 生成时间（毫秒）

  // 计算该提议针对特定槽位当前状态的最终置信度（由槽位状态机调用）
  [[nodiscard]] float ComputeConfidence(bool isCurrentPartner, bool isSustaining) const;
};

// -----------------------------------------------------------------------------
// 槽位状态（每个接收槽位独立维护）
// -----------------------------------------------------------------------------
class SlotStateMachine
{
public:
  enum class State : std::uint8_t
  {
    Idle,
    Approaching,
    Engaging,
    Sustaining,
    Ending
  };

  SlotStateMachine(SlotType type, RE::Actor* owner);

  // 每帧更新：接收针对本槽位的所有提议，决定是否维持或切换
  void Update(float deltaMs, const std::vector<Proposal>& proposals);

  // 获取当前状态信息
  [[nodiscard]] State GetState() const { return m_state; }
  [[nodiscard]] RE::Actor* GetCurrentPartner() const { return m_currentPartner; }
  [[nodiscard]] InteractionType GetCurrentType() const { return m_currentType; }
  [[nodiscard]] float GetConfidence() const { return m_currentConfidence; }

  // 生成“愿望”提交给仲裁器
  struct Desire
  {
    SlotType slot;
    RE::Actor* owner;
    RE::Actor* preferredSource;
    ResourceType desiredResource;
    InteractionType desiredType;
    float confidence;
    bool isSustaining;
  };
  [[nodiscard]] Desire GetDesire() const;

  // 接收仲裁结果
  void AcceptAssignment(RE::Actor* partner, ResourceType resource, InteractionType type);
  void RejectAssignment();

private:
  SlotType m_type;
  RE::Actor* m_owner;
  State m_state = State::Idle;

  RE::Actor* m_currentPartner    = nullptr;
  ResourceType m_currentResource = ResourceType::Mouth;
  InteractionType m_currentType  = InteractionType::None;
  float m_currentConfidence      = 0.f;

  float m_timeInState       = 0.f;
  float m_lockoutTimer      = 0.f;  // 切换锁定计时器
  float m_sustainGraceTimer = 0.f;  // 短暂脱离宽限期

  // 内部辅助
  bool ShouldMaintain(const Proposal* currentProp) const;
  bool ShouldSwitchTo(const Proposal& newProp) const;
  void TransitionTo(State newState, RE::Actor* partner, ResourceType resource, InteractionType type,
                    float confidence);
};

// -----------------------------------------------------------------------------
// 资源锁（由仲裁器管理）
// -----------------------------------------------------------------------------
struct ResourceLock
{
  ResourceType resource;
  RE::Actor* owner;
  SlotType occupiedSlot;
  RE::Actor* partner;
  InteractionType interactionType;
  float confidence;
  float lockoutTimer;  // 被抢占后的冷却
  bool isSustaining;
};

// -----------------------------------------------------------------------------
// 资源仲裁器
// -----------------------------------------------------------------------------
class ResourceArbiter
{
public:
  void Update(float deltaMs);

  // 接收所有槽位的愿望，进行全局仲裁
  void Resolve(const std::vector<SlotStateMachine::Desire>& desires);

  // 查询资源是否可用
  [[nodiscard]] bool IsResourceAvailable(ResourceType res, RE::Actor* owner, RE::Actor* requester,
                                         SlotType slot) const;

  // 获取分配结果（供槽位状态机查询）
  struct Assignment
  {
    SlotType slot;
    RE::Actor* targetActor;  // 槽位拥有者
    RE::Actor* sourceActor;
    ResourceType resource;
    InteractionType type;
    bool accepted;
  };
  [[nodiscard]] const std::vector<Assignment>& GetLastAssignments() const
  {
    return m_lastAssignments;
  }

private:
  std::unordered_map<RE::Actor*, std::unordered_map<ResourceType, ResourceLock>> m_resourceLocks;
  std::vector<Assignment> m_lastAssignments;

  bool TryAssign(const SlotStateMachine::Desire& desire, Assignment& outAssign);
  bool CanPreempt(const ResourceLock& current, const SlotStateMachine::Desire& challenger) const;
  void ReleaseResource(RE::Actor* owner, ResourceType resource);
};

// -----------------------------------------------------------------------------
// 运动快照（与之前类似，但基于新的体积系统）
// -----------------------------------------------------------------------------
struct MotionSnapshot
{
  Define::Point3f start      = Define::Point3f::Zero();
  Define::Point3f end        = Define::Point3f::Zero();
  Define::Vector3f direction = Define::Vector3f::Zero();
  float length               = 0.f;
  float timeMs               = 0.f;
  bool valid                 = false;
  bool directional           = false;

  // 对于 SplineCapsule，可存储更多采样点
  std::vector<Define::Point3f> sampledPoints;
};

// -----------------------------------------------------------------------------
// 交互快照（每个部位每帧的状态）
// -----------------------------------------------------------------------------
struct InteractionSnapshot
{
  RE::Actor* partner   = nullptr;
  InteractionType type = InteractionType::None;
  float distance       = 0.f;
  float approachSpeed  = 0.f;
  bool active          = false;
};

// -----------------------------------------------------------------------------
// 部位运行时状态（挂载在 Actor 下）
// -----------------------------------------------------------------------------
struct PartRuntime
{
  const Define::VolumeData* volume = nullptr;  // 指向 ActorBody 中的体积
  MotionSnapshot motion;
  InteractionSnapshot interaction;

  // 对于槽位，有独立状态机
  std::unique_ptr<SlotStateMachine> slotFSM;

  bool IsSlot() const { return slotFSM != nullptr; }
};

// -----------------------------------------------------------------------------
// 角色交互状态（每个 Actor 一个）
// -----------------------------------------------------------------------------
struct ActorInteractState
{
  RE::Actor* actor = nullptr;
  Define::ActorBody body;  // 所有部位体积
  std::unordered_map<Define::ActorBody::PartName, PartRuntime> parts;

  // 资源占用情况（由仲裁器填充）
  std::unordered_map<ResourceType, bool> resourceBusy;

  // 观察到的交互标签（供外部查询）
  Define::InteractTags observedTags;

  void UpdateBody(float deltaMs);
  void CaptureMotion(float nowMs);
};

// -----------------------------------------------------------------------------
// 几何证据评估器（静态方法集合）
// -----------------------------------------------------------------------------
namespace EvidenceEvaluator
{
  // 主入口：评估源体积对目标槽位体积的证据
  GeometricEvidence Evaluate(const Define::VolumeData& source, const Define::VolumeData& target,
                             SlotType targetSlotType);

  // 各类型具体实现
  GeometricEvidence EvaluateCapsuleToFunnel(const Define::Capsule& cap,
                                            const Define::Funnel& funnel);
  GeometricEvidence EvaluateSplineCapsuleToFunnel(const Define::SplineCapsule& spline,
                                                  const Define::Funnel& funnel);
  GeometricEvidence EvaluateCapsuleToEnvelope(const Define::Capsule& cap,
                                              const Define::Envelope<Define::Capsule>& env);
  GeometricEvidence EvaluateSplineCapsuleToEnvelope(const Define::SplineCapsule& spline,
                                                    const Define::Envelope<Define::Capsule>& env);
  GeometricEvidence EvaluateCapsuleToHalfBall(const Define::Capsule& cap,
                                              const Define::HalfBall& hb);
  // ... 其他组合
}  // namespace EvidenceEvaluator

// -----------------------------------------------------------------------------
// 提议构建器（遍历所有角色对，生成 Proposal）
// -----------------------------------------------------------------------------
class ProposalBuilder
{
public:
  void BuildProposals(const std::vector<ActorInteractState*>& actors,
                      std::vector<Proposal>& outProposals, float nowMs);

private:
  void GenerateForPair(ActorInteractState& a, ActorInteractState& b, std::vector<Proposal>& out,
                       float nowMs);
  void AddIfValid(const Proposal& prop, std::vector<Proposal>& out);
};

// -----------------------------------------------------------------------------
// Interact 主类（对外接口）
// -----------------------------------------------------------------------------
class Interact
{
public:
  Interact() = default;
  explicit Interact(std::vector<RE::Actor*> actors);

  void FlashNodeData();
  void Update(float deltaMs);

  // 查询接口
  [[nodiscard]] const ActorInteractState* GetActorState(RE::Actor* actor) const;
  [[nodiscard]] const PartRuntime* GetPartRuntime(RE::Actor* actor,
                                                  Define::ActorBody::PartName part) const;
  [[nodiscard]] const Define::InteractTags& GetObservedTags(RE::Actor* actor) const;

  // 调试
  void DebugDraw();

private:
  std::unordered_map<RE::Actor*, std::unique_ptr<ActorInteractState>> m_actorStates;
  ResourceArbiter m_arbiter;
  ProposalBuilder m_builder;
  float m_timelineMs = 0.f;
};

}  // namespace Instance
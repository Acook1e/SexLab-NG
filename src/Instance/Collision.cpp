#include "Instance/Collision.h"

namespace Instance
{
void Collision::AddActor(RE::Actor* actor)
{
  std::unique_lock<std::shared_mutex> lock(mtx);
  actors.insert(actor);

  auto* process = actor->GetMiddleHighProcess();
  if (!process || !process->charController)
    return;

  auto* controller = process->charController.get();
  if (!controller)
    return;

  ConfigureControllerForScene(controller);

  // DisableRigidBodyPhysics
  controller->flags.set(RE::CHARACTER_FLAGS::kNotPushablePermanent);
  controller->flags.reset(RE::CHARACTER_FLAGS::kPossiblePathObstacle);

  controller->surfaceInfo.surfaceVelocity.quad = _mm_setzero_ps();

  auto* rigidBody = controller->GetRigidBody();
  if (!rigidBody || !rigidBody->GetCollidableRW())
    return;

  // SetMassInv is not linkable, update the mass-inverse component (W lane) directly instead.
  float inertiaAndMassInv[4]{};
  _mm_storeu_ps(inertiaAndMassInv, rigidBody->motion.inertiaAndMassInv.quad);
  inertiaAndMassInv[3]                     = 0.0f;
  rigidBody->motion.inertiaAndMassInv.quad = _mm_loadu_ps(inertiaAndMassInv);
  // Set gravity factor to 0 to prevent the character from sliding down slopes when on ground
  rigidBody->motion.gravityFactor = 0.0f;

  // Disable Foot IK
  RE::BSAnimationGraphManagerPtr graphMgr;
  if (!actor->GetAnimationGraphManager(graphMgr) || !graphMgr)
    return;

  RE::BSSpinLockGuard locker(graphMgr->GetRuntimeData().updateLock);
  for (auto& graph : graphMgr->graphs) {
    if (!graph)
      continue;

    auto* driver = graph->characterInstance.footIkDriver.get();
    if (driver) {
      auto driverBase             = reinterpret_cast<std::uintptr_t>(driver);
      auto* disableFootIk         = reinterpret_cast<bool*>(driverBase + 0x4A);
      auto* alignedGroundRotation = reinterpret_cast<RE::hkQuaternion*>(driverBase + 0x30);

      //*disableFootIk             = true;
      // alignedGroundRotation->vec = {0.0f, 0.0f, 0.0f, 1.0f};
    }
  }
}

void Collision::RemoveActor(RE::Actor* actor)
{
  std::unique_lock<std::shared_mutex> lock(mtx);
  actors.erase(actor);

  // Enable Foot IK
  RE::BSAnimationGraphManagerPtr graphMgr;
  if (!actor->GetAnimationGraphManager(graphMgr) || !graphMgr)
    return;

  RE::BSSpinLockGuard locker(graphMgr->GetRuntimeData().updateLock);
  for (auto& graph : graphMgr->graphs) {
    if (!graph)
      continue;

    auto* driver = graph->characterInstance.footIkDriver.get();
    if (driver) {
      auto driverBase             = reinterpret_cast<std::uintptr_t>(driver);
      auto* disableFootIk         = reinterpret_cast<bool*>(driverBase + 0x4A);
      auto* alignedGroundRotation = reinterpret_cast<RE::hkQuaternion*>(driverBase + 0x30);

      //*disableFootIk             = false;
      // alignedGroundRotation->vec = {0.0f, 0.0f, 0.0f, 1.0f};
    }
  }

  // EnableRigidBodyPhysics
  auto* process = actor->GetMiddleHighProcess();
  if (!process)
    return;

  auto* controller = process->charController.get();
  if (!controller)
    return;

  controller->flags.reset(RE::CHARACTER_FLAGS::kNoGravityOnGround, RE::CHARACTER_FLAGS::kNoSim,
                          RE::CHARACTER_FLAGS::kNotPushablePermanent);

  controller->flags.set(RE::CHARACTER_FLAGS::kSupport, RE::CHARACTER_FLAGS::kCheckSupport);

  controller->context.currentState = RE::hkpCharacterStateType::kOnGround;

  auto* rigidBody = controller->GetRigidBody();
  if (!rigidBody || !rigidBody->GetCollidableRW())
    return;

  // SetMassInv is not linkable, update the mass-inverse component (W lane) directly instead.
  float inertiaAndMassInv[4]{};
  _mm_storeu_ps(inertiaAndMassInv, rigidBody->motion.inertiaAndMassInv.quad);
  inertiaAndMassInv[3]                     = 1.0f;
  rigidBody->motion.inertiaAndMassInv.quad = _mm_loadu_ps(inertiaAndMassInv);
  // Set gravity factor to 0 to prevent the character from sliding down slopes when on ground
  rigidBody->motion.gravityFactor = 1.0f;
}

bool Collision::HasActor(RE::Actor* actor)
{
  std::shared_lock<std::shared_mutex> lock(mtx);
  return actors.find(actor) != actors.end();
}

void Collision::ConfigureControllerForScene(RE::bhkCharacterController* controller)
{
  // Placeholder for configuring the actor's collision controller for scene interactions

  controller->pitchAngle          = 0.0f;
  controller->rollAngle           = 0.0f;
  controller->calculatePitchTimer = 55.0f;

  auto zero = _mm_setzero_ps();
  controller->SetLinearVelocityImpl(zero);

  controller->outVelocity.quad      = zero;
  controller->initialVelocity.quad  = zero;
  controller->velocityMod.quad      = zero;
  controller->pushDelta.quad        = zero;
  controller->fakeSupportStart.quad = zero;
  controller->supportNorm.quad      = _mm_setr_ps(0.0f, 0.0f, 1.0f, 0.0f);

  controller->flags.set(RE::CHARACTER_FLAGS::kNoGravityOnGround, RE::CHARACTER_FLAGS::kNoSim,
                        RE::CHARACTER_FLAGS::kSupport);

  controller->flags.reset(RE::CHARACTER_FLAGS::kCheckSupport, RE::CHARACTER_FLAGS::kHasPotentialSupportManifold,
                          RE::CHARACTER_FLAGS::kStuckQuad, RE::CHARACTER_FLAGS::kOnStairs,
                          RE::CHARACTER_FLAGS::kTryStep);

  controller->context.currentState = RE::hkpCharacterStateType::kSwimming;
}
}  // namespace Instance
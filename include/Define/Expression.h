#pragma once

namespace Expression
{
using ExpressionId = RE::BSFaceGenKeyframeMultiple::Expression;
using ModifierID   = RE::BSFaceGenKeyframeMultiple::Modifier;
using PhonemeID    = RE::BSFaceGenKeyframeMultiple::Phoneme;

// 确保在主线程调用，否则直接崩溃
inline void SetActorExpression(RE::Actor* actor, ExpressionId expressionId, float value)
{
  if (!actor)
    return;

  auto animData = actor->GetFaceGenAnimationData();
  if (!animData)
    return;

  RE::BSSpinLockGuard locker(animData->lock);

  // 清除旧的 override，然后设置新的
  animData->ClearExpressionOverride();
  animData->SetExpressionOverride(expressionId, value);  // 调用游戏原生函数
  animData->exprOverride = true;
}
}  // namespace Expression
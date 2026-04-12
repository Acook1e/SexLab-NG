#include "Instance/Interact.h"

#include "magic_enum/magic_enum.hpp"

namespace Instance
{

using Name      = Define::BodyPart::Name;
using BP        = Define::BodyPart;
using Type      = Interact::Type;
using ActorData = Interact::ActorData;
using Info      = Interact::Info;

namespace
{

  enum class CandidateFamily : std::uint8_t
  {
    Penetration = 0,
    Oral        = 1,
    Manual      = 2,
    Contact     = 3,
  };

  struct AngleRange
  {
    float min = 0.f;
    float max = 180.f;
  };

  struct PairContext
  {
    RE::Actor* actorA;
    RE::Actor* actorB;
    const ActorData& dataA;
    const ActorData& dataB;
    bool self;
  };

  struct DirectedContext
  {
    RE::Actor* sourceActor;
    RE::Actor* targetActor;
    const ActorData& sourceData;
    const ActorData& targetData;
    bool self;
  };

  struct Candidate
  {
    RE::Actor* sourceActor;
    Name sourcePart;
    RE::Actor* targetActor;
    Name targetPart;
    Type type;
    float distance;
    float score;
    CandidateFamily family;
  };

  struct AssignInfo
  {
    RE::Actor* partner;
    Name partSelf;
    Name partPartner;
    float distance;
    Type type;
  };

  struct ActorInteractSummary
  {
    bool hasOral              = false;
    bool givesOral            = false;
    bool hasVaginal           = false;
    bool hasAnal              = false;
    RE::Actor* oralPartner    = nullptr;
    RE::Actor* vaginalPartner = nullptr;
    RE::Actor* analPartner    = nullptr;
    RE::Actor* givesOralTo    = nullptr;
  };

  static constexpr std::array<Name, 1> kMouthParts{Name::Mouth};
  static constexpr std::array<Name, 1> kThroatParts{Name::Throat};
  static constexpr std::array<Name, 1> kPenisParts{Name::Penis};
  static constexpr std::array<Name, 1> kVaginaParts{Name::Vagina};
  static constexpr std::array<Name, 1> kAnusParts{Name::Anus};
  static constexpr std::array<Name, 1> kThighCleftParts{Name::ThighCleft};
  static constexpr std::array<Name, 1> kGlutealCleftParts{Name::GlutealCleft};
  static constexpr std::array<Name, 1> kCleavageParts{Name::Cleavage};
  static constexpr std::array<Name, 1> kBellyParts{Name::Belly};
  static constexpr std::array<Name, 2> kHandParts{Name::HandLeft, Name::HandRight};
  static constexpr std::array<Name, 2> kFingerParts{Name::FingerLeft, Name::FingerRight};
  static constexpr std::array<Name, 2> kBreastParts{Name::BreastLeft, Name::BreastRight};
  static constexpr std::array<Name, 2> kFootParts{Name::FootLeft, Name::FootRight};
  static constexpr std::array<Name, 2> kThighParts{Name::ThighLeft, Name::ThighRight};
  static constexpr std::array<Name, 2> kButtParts{Name::ButtLeft, Name::ButtRight};

  const BP* TryGetBodyPart(const ActorData& data, Name name)
  {
    auto it = data.infos.find(name);
    if (it == data.infos.end() || !it->second.bodypart.IsValid())
      return nullptr;
    return &it->second.bodypart;
  }

  const Info* TryGetInfo(const ActorData& data, Name name)
  {
    auto it = data.infos.find(name);
    return it == data.infos.end() ? nullptr : &it->second;
  }

  template <std::size_t N>
  bool HasAnyPart(const ActorData& data, const std::array<Name, N>& names)
  {
    for (Name name : names) {
      if (TryGetBodyPart(data, name))
        return true;
    }
    return false;
  }

  bool IsAngleUnconstrained(AngleRange range)
  {
    return range.min <= 0.f && range.max >= 179.9f;
  }

  bool CheckAngle(float absAngle, AngleRange range)
  {
    if (IsAngleUnconstrained(range))
      return true;
    if (range.min <= range.max)
      return absAngle >= range.min && absAngle <= range.max;
    return absAngle <= range.max || absAngle >= range.min;
  }

  float AngleDeviation(float absAngle, AngleRange range)
  {
    if (IsAngleUnconstrained(range))
      return 0.f;

    const bool wrap = range.min > range.max;
    if (!wrap) {
      const float half = (range.max - range.min) * 0.5f;
      const float mid  = (range.min + range.max) * 0.5f;
      return half > 1e-4f ? std::abs(absAngle - mid) / half : 0.f;
    }

    const float midLow   = range.max * 0.5f;
    const float midHigh  = (range.min + 180.f) * 0.5f;
    const float halfLow  = range.max > 1e-4f ? range.max * 0.5f : 1e-4f;
    const float halfHigh = (180.f - range.min) > 1e-4f ? (180.f - range.min) * 0.5f : 1e-4f;
    const float devLow   = std::abs(absAngle - midLow) / halfLow;
    const float devHigh  = std::abs(absAngle - midHigh) / halfHigh;
    return devLow < devHigh ? devLow : devHigh;
  }

  float NormalizeDistance(float distance, float maxDistance)
  {
    if (maxDistance <= 1e-4f)
      return 0.f;
    return std::clamp(distance / maxDistance, 0.f, 2.f);
  }

  float TemporalBonus(const ActorData& data, Name part, RE::Actor* partner, Type type)
  {
    const Info* info = TryGetInfo(data, part);
    if (!info)
      return 0.f;
    if (info->prevActor == partner && info->prevType == type)
      return 0.08f;
    if (info->prevActor == partner)
      return 0.04f;
    if (info->prevType == type)
      return 0.02f;
    return 0.f;
  }

  float ProjectionOnAxis(const Define::Point3f& point, const BP& axisPart)
  {
    if (axisPart.GetType() == BP::Type::Point || axisPart.GetLength() < 1e-6f)
      return 0.f;
    return (point - axisPart.GetStart()).dot(axisPart.GetDirection());
  }

  float DistanceToAxisSegment(const Define::Point3f& point, const BP& axisPart)
  {
    if (axisPart.GetType() == BP::Type::Point || axisPart.GetLength() < 1e-6f)
      return (point - axisPart.GetStart()).norm();

    const float t      = std::clamp((point - axisPart.GetStart()).dot(axisPart.GetDirection()), 0.f,
                                    axisPart.GetLength());
    const auto closest = axisPart.GetStart() + axisPart.GetDirection() * t;
    return (point - closest).norm();
  }

  float CalculatePenetrationContextBias(const ActorData& receiverData, const BP& penisPart,
                                        Type type)
  {
    if (type != Type::Vaginal && type != Type::Anal)
      return 0.f;

    const BP* thighCleft  = TryGetBodyPart(receiverData, Name::ThighCleft);
    const BP* glutealPart = TryGetBodyPart(receiverData, Name::GlutealCleft);

    // kSupportScale:
    //   把前后区域支撑点的距离差映射到 score 偏置时使用的缩放尺。
    //   数值越大，前后差异必须更明显才能改变排序；数值越小，系统更敏感。
    constexpr float kSupportScale = 8.f;

    // kMaxBias:
    //   前后区域上下文最多能对 Vaginal / Anal 候选施加多大的 score 修正。
    //   它只能用来打破僵局，不应覆盖距离与角度本身，因此保持在较小范围内。
    constexpr float kMaxBias = 0.22f;

    const float vagSupport =
        thighCleft ? thighCleft->Distance(penisPart) : std::numeric_limits<float>::infinity();
    const float analSupport =
        glutealPart ? glutealPart->Distance(penisPart) : std::numeric_limits<float>::infinity();

    if (std::isfinite(vagSupport) && std::isfinite(analSupport)) {
      const float delta =
          std::clamp((vagSupport - analSupport) / kSupportScale, -kMaxBias, kMaxBias);
      return type == Type::Vaginal ? delta : -delta;
    }

    if (type == Type::Vaginal && std::isfinite(vagSupport))
      return -std::clamp(1.f - vagSupport / kSupportScale, 0.f, kMaxBias);
    if (type == Type::Anal && std::isfinite(analSupport))
      return -std::clamp(1.f - analSupport / kSupportScale, 0.f, kMaxBias);
    return 0.f;
  }

  void AddCandidate(std::vector<Candidate>& out, const DirectedContext& ctx, Name sourcePart,
                    Name targetPart, Type type, CandidateFamily family, float distance, float score)
  {
    out.push_back(Candidate{ctx.sourceActor, sourcePart, ctx.targetActor, targetPart, type,
                            distance, score, family});
  }

  template <std::size_t NS, std::size_t NT>
  void AddSimpleDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx,
                                   const std::array<Name, NS>& sourceParts,
                                   const std::array<Name, NT>& targetParts, Type type,
                                   CandidateFamily family, float maxDistance, AngleRange angleRange,
                                   float angleWeight, float scoreOffset = 0.f)
  {
    for (Name sourceName : sourceParts) {
      const BP* sourcePart = TryGetBodyPart(ctx.sourceData, sourceName);
      if (!sourcePart)
        continue;

      for (Name targetName : targetParts) {
        if (ctx.self && sourceName == targetName)
          continue;

        const BP* targetPart = TryGetBodyPart(ctx.targetData, targetName);
        if (!targetPart)
          continue;

        const float distance = sourcePart->Distance(*targetPart);
        if (distance > maxDistance)
          continue;

        const float angle = sourcePart->Angle(*targetPart);
        if (!CheckAngle(angle, angleRange))
          continue;

        const float distNorm = NormalizeDistance(distance, maxDistance);
        float score          = distNorm;
        if (!IsAngleUnconstrained(angleRange)) {
          const float angleNorm = AngleDeviation(angle, angleRange);
          score                 = distNorm * (1.f - angleWeight) + angleNorm * angleWeight;
        }

        const float bonus = TemporalBonus(ctx.sourceData, sourceName, ctx.targetActor, type) +
                            TemporalBonus(ctx.targetData, targetName, ctx.sourceActor, type);
        score             = (std::max)(0.f, score + scoreOffset - bonus);

        AddCandidate(out, ctx, sourceName, targetName, type, family, distance, score);
      }
    }
  }

  void TryAddSymmetricCandidate(std::vector<Candidate>& out, const PairContext& pair, Name partA,
                                Name partB, Type type, CandidateFamily family, float maxDistance,
                                AngleRange angleRange, float angleWeight, float scoreOffset = 0.f)
  {
    if (pair.self)
      return;

    const BP* lhs = TryGetBodyPart(pair.dataA, partA);
    const BP* rhs = TryGetBodyPart(pair.dataB, partB);
    if (!lhs || !rhs)
      return;

    const float distance = lhs->Distance(*rhs);
    if (distance > maxDistance)
      return;

    const float angle = lhs->Angle(*rhs);
    if (!CheckAngle(angle, angleRange))
      return;

    const float distNorm = NormalizeDistance(distance, maxDistance);
    float score          = distNorm;
    if (!IsAngleUnconstrained(angleRange)) {
      const float angleNorm = AngleDeviation(angle, angleRange);
      score                 = distNorm * (1.f - angleWeight) + angleNorm * angleWeight;
    }

    const float bonus = TemporalBonus(pair.dataA, partA, pair.actorB, type) +
                        TemporalBonus(pair.dataB, partB, pair.actorA, type);
    score             = (std::max)(0.f, score + scoreOffset - bonus);

    DirectedContext ctx{pair.actorA, pair.actorB, pair.dataA, pair.dataB, false};
    AddCandidate(out, ctx, partA, partB, type, family, distance, score);
  }

  void TryAddNaveljobCandidate(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (!TryGetBodyPart(ctx.sourceData, Name::Penis) ||
        !TryGetBodyPart(ctx.targetData, Name::Belly))
      return;

    const BP& penis = *TryGetBodyPart(ctx.sourceData, Name::Penis);
    const BP& belly = *TryGetBodyPart(ctx.targetData, Name::Belly);

    const float distance = belly.Distance(penis);
    if (distance > 9.f)
      return;

    const float angle = belly.Angle(penis);
    if (angle > 50.f)
      return;

    if (!belly.IsInFront(penis.GetEnd()) || !penis.IsHorizontal())
      return;

    const float distNorm  = NormalizeDistance(distance, 9.f);
    const float angleNorm = AngleDeviation(angle, {0.f, 50.f});
    const float bonus =
        TemporalBonus(ctx.sourceData, Name::Penis, ctx.targetActor, Type::Naveljob) +
        TemporalBonus(ctx.targetData, Name::Belly, ctx.sourceActor, Type::Naveljob);
    const float score = (std::max)(0.f, distNorm * 0.55f + angleNorm * 0.45f - bonus);

    AddCandidate(out, ctx, Name::Penis, Name::Belly, Type::Naveljob, CandidateFamily::Contact,
                 distance, score);
  }

  void TryAddPenetrationCandidate(std::vector<Candidate>& out, const DirectedContext& ctx,
                                  Name receiverPartName, Type type, float maxDistance,
                                  float maxAngle, float maxTipAxis, float minDepth, float maxDepth)
  {
    const BP* penisPart    = TryGetBodyPart(ctx.sourceData, Name::Penis);
    const BP* receiverPart = TryGetBodyPart(ctx.targetData, receiverPartName);
    if (!penisPart || !receiverPart)
      return;

    const float distance = receiverPart->Distance(*penisPart);
    if (distance > maxDistance)
      return;

    const float angle = receiverPart->Angle(*penisPart);
    if (angle > maxAngle)
      return;

    const float tipAxisDistance = DistanceToAxisSegment(penisPart->GetEnd(), *receiverPart);
    if (tipAxisDistance > maxTipAxis)
      return;

    const float depth = ProjectionOnAxis(penisPart->GetEnd(), *receiverPart);
    if (depth < minDepth || depth > maxDepth)
      return;

    const float distNorm  = NormalizeDistance(distance, maxDistance);
    const float angleNorm = AngleDeviation(angle, {0.f, maxAngle});
    const float axisNorm  = NormalizeDistance(tipAxisDistance, maxTipAxis);

    float depthPenalty = 0.f;
    if (depth < 0.f)
      depthPenalty = std::abs(depth) / (std::max)(std::abs(minDepth), 1e-4f);
    else if (depth > receiverPart->GetLength())
      depthPenalty = (depth - receiverPart->GetLength()) /
                     (std::max)(maxDepth - receiverPart->GetLength(), 1e-4f);

    const float contextBias = CalculatePenetrationContextBias(ctx.targetData, *penisPart, type);
    const float bonus = TemporalBonus(ctx.sourceData, Name::Penis, ctx.targetActor, type) +
                        TemporalBonus(ctx.targetData, receiverPartName, ctx.sourceActor, type);

    const float score =
        (std::max)(0.f, distNorm * 0.42f + angleNorm * 0.22f + axisNorm * 0.22f +
                            std::clamp(depthPenalty, 0.f, 1.f) * 0.14f + contextBias - bonus);

    AddCandidate(out, ctx, Name::Penis, receiverPartName, type, CandidateFamily::Penetration,
                 distance, score);
  }

  void GenerateOralDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (ctx.self)
      return;
    if (!HasAnyPart(ctx.sourceData, kMouthParts) && !HasAnyPart(ctx.sourceData, kThroatParts))
      return;

    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kBreastParts, Type::BreastSucking,
                                CandidateFamily::Oral, 7.f, {0.f, 180.f}, 0.f);
    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kFootParts, Type::ToeSucking,
                                CandidateFamily::Oral, 7.f, {0.f, 180.f}, 0.f);
    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kThighCleftParts, Type::Cunnilingus,
                                CandidateFamily::Oral, 6.f, {0.f, 180.f}, 0.f, -0.03f);
    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kVaginaParts, Type::Cunnilingus,
                                CandidateFamily::Oral, 6.5f, {0.f, 180.f}, 0.f, 0.02f);
    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kAnusParts, Type::Anilingus,
                                CandidateFamily::Oral, 6.f, {0.f, 180.f}, 0.f);
    AddSimpleDirectedCandidates(out, ctx, kMouthParts, kPenisParts, Type::Fellatio,
                                CandidateFamily::Oral, 7.f, {0.f, 35.f}, 0.45f);
    AddSimpleDirectedCandidates(out, ctx, kThroatParts, kPenisParts, Type::DeepThroat,
                                CandidateFamily::Oral, 6.f, {0.f, 28.f}, 0.50f, -0.01f);
  }

  void GenerateManualDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (!HasAnyPart(ctx.sourceData, kHandParts) && !HasAnyPart(ctx.sourceData, kFingerParts))
      return;

    AddSimpleDirectedCandidates(out, ctx, kHandParts, kBreastParts, Type::GropeBreast,
                                CandidateFamily::Manual, 5.f, {150.f, 180.f}, 0.28f);
    AddSimpleDirectedCandidates(out, ctx, kHandParts, kThighParts, Type::GropeThigh,
                                CandidateFamily::Manual, 5.5f, {0.f, 180.f}, 0.f);
    AddSimpleDirectedCandidates(out, ctx, kHandParts, kButtParts, Type::GropeButt,
                                CandidateFamily::Manual, 4.5f, {0.f, 180.f}, 0.f);
    AddSimpleDirectedCandidates(out, ctx, kHandParts, kFootParts, Type::GropeFoot,
                                CandidateFamily::Manual, 5.5f, {0.f, 180.f}, 0.f);

    AddSimpleDirectedCandidates(out, ctx, kFingerParts, kThighCleftParts, Type::FingerVagina,
                                CandidateFamily::Manual, 4.5f, {0.f, 180.f}, 0.f, -0.03f);
    AddSimpleDirectedCandidates(out, ctx, kFingerParts, kVaginaParts, Type::FingerVagina,
                                CandidateFamily::Manual, 5.5f, {0.f, 180.f}, 0.f, 0.02f);
    AddSimpleDirectedCandidates(out, ctx, kFingerParts, kAnusParts, Type::FingerAnus,
                                CandidateFamily::Manual, 5.f, {0.f, 180.f}, 0.f);
    AddSimpleDirectedCandidates(out, ctx, kHandParts, kPenisParts, Type::Handjob,
                                CandidateFamily::Manual, 7.f, {60.f, 120.f}, 0.25f);
  }

  void GenerateContactDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    if (!HasAnyPart(ctx.sourceData, kPenisParts))
      return;

    AddSimpleDirectedCandidates(out, ctx, kPenisParts, kBreastParts, Type::Titfuck,
                                CandidateFamily::Contact, 7.f, {0.f, 50.f}, 0.38f);
    AddSimpleDirectedCandidates(out, ctx, kPenisParts, kCleavageParts, Type::Titfuck,
                                CandidateFamily::Contact, 7.f, {75.f, 105.f}, 0.48f, -0.02f);
    TryAddNaveljobCandidate(out, ctx);
    AddSimpleDirectedCandidates(out, ctx, kPenisParts, kThighCleftParts, Type::Thighjob,
                                CandidateFamily::Contact, 5.5f, {0.f, 25.f}, 0.30f, -0.03f);
    AddSimpleDirectedCandidates(out, ctx, kPenisParts, kThighParts, Type::Thighjob,
                                CandidateFamily::Contact, 4.5f, {0.f, 25.f}, 0.28f, 0.02f);
    AddSimpleDirectedCandidates(out, ctx, kPenisParts, kGlutealCleftParts, Type::Frottage,
                                CandidateFamily::Contact, 7.f, {145.f, 35.f}, 0.35f);
    AddSimpleDirectedCandidates(out, ctx, kPenisParts, kFootParts, Type::Footjob,
                                CandidateFamily::Contact, 7.f, {0.f, 180.f}, 0.f);
  }

  void GeneratePenetrationDirectedCandidates(std::vector<Candidate>& out,
                                             const DirectedContext& ctx)
  {
    if (ctx.self || !HasAnyPart(ctx.sourceData, kPenisParts))
      return;

    TryAddPenetrationCandidate(out, ctx, Name::Vagina, Type::Vaginal, 7.f, 22.f, 4.5f, -2.5f, 10.f);
    TryAddPenetrationCandidate(out, ctx, Name::Anus, Type::Anal, 6.f, 18.f, 4.f, -2.f, 9.f);
  }

  void GenerateSymmetricCandidates(std::vector<Candidate>& out, const PairContext& pair)
  {
    TryAddSymmetricCandidate(out, pair, Name::Mouth, Name::Mouth, Type::Kiss, CandidateFamily::Oral,
                             6.f, {145.f, 180.f}, 0.55f);
    TryAddSymmetricCandidate(out, pair, Name::Vagina, Name::Vagina, Type::Tribbing,
                             CandidateFamily::Contact, 7.f, {145.f, 180.f}, 0.40f);
  }

  void GenerateDirectedCandidates(std::vector<Candidate>& out, const DirectedContext& ctx)
  {
    GenerateOralDirectedCandidates(out, ctx);
    GenerateManualDirectedCandidates(out, ctx);
    GenerateContactDirectedCandidates(out, ctx);
    GeneratePenetrationDirectedCandidates(out, ctx);
  }

  void GeneratePairCandidates(std::vector<Candidate>& out, const PairContext& pair)
  {
    if (pair.self) {
      const DirectedContext selfCtx{pair.actorA, pair.actorA, pair.dataA, pair.dataA, true};
      GenerateManualDirectedCandidates(out, selfCtx);
      return;
    }

    GenerateSymmetricCandidates(out, pair);
    GenerateDirectedCandidates(
        out, DirectedContext{pair.actorA, pair.actorB, pair.dataA, pair.dataB, false});
    GenerateDirectedCandidates(
        out, DirectedContext{pair.actorB, pair.actorA, pair.dataB, pair.dataA, false});
  }

  Type ResolveSelfType(const Candidate& candidate)
  {
    if (candidate.sourceActor != candidate.targetActor)
      return candidate.type;

    if (candidate.type == Type::FingerVagina || candidate.type == Type::FingerAnus ||
        candidate.type == Type::Handjob)
      return Type::Masturbation;
    return candidate.type;
  }

  using AssignMap = std::unordered_map<RE::Actor*, std::unordered_map<Name, AssignInfo>>;

  void BuildSummaries(const AssignMap& assigns,
                      std::unordered_map<RE::Actor*, ActorInteractSummary>& summaries)
  {
    for (const auto& [actor, partMap] : assigns) {
      ActorInteractSummary& summary = summaries[actor];
      for (const auto& [partName, assign] : partMap) {
        switch (assign.type) {
        case Type::Fellatio:
          if (partName == Name::Mouth) {
            summary.hasOral     = true;
            summary.oralPartner = assign.partner;
          }
          break;
        case Type::DeepThroat:
          if (partName == Name::Throat) {
            summary.hasOral     = true;
            summary.oralPartner = assign.partner;
          }
          break;
        case Type::Cunnilingus:
        case Type::Anilingus:
          if (partName == Name::Mouth) {
            summary.givesOral   = true;
            summary.givesOralTo = assign.partner;
          }
          break;
        case Type::Vaginal:
          if (partName == Name::Vagina) {
            summary.hasVaginal     = true;
            summary.vaginalPartner = assign.partner;
          }
          break;
        case Type::Anal:
          if (partName == Name::Anus) {
            summary.hasAnal     = true;
            summary.analPartner = assign.partner;
          }
          break;
        default:
          break;
        }
      }
    }
  }

  void BuildComboTypes(const std::unordered_map<RE::Actor*, ActorInteractSummary>& summaries,
                       std::unordered_map<RE::Actor*, std::vector<Type>>& comboTypes)
  {
    for (const auto& [actor, summary] : summaries) {
      if (summary.givesOral && summary.givesOralTo) {
        auto it = summaries.find(summary.givesOralTo);
        if (it != summaries.end()) {
          const ActorInteractSummary& partnerSummary = it->second;
          if ((partnerSummary.givesOral && partnerSummary.givesOralTo == actor) ||
              (partnerSummary.hasOral && partnerSummary.oralPartner == actor)) {
            comboTypes[actor].push_back(Type::SixtyNine);
          }
        }
      }

      if (summary.hasVaginal && summary.hasAnal && summary.vaginalPartner != summary.analPartner)
        comboTypes[actor].push_back(Type::DoublePenetration);

      if (summary.hasOral && summary.hasVaginal && summary.hasAnal &&
          summary.oralPartner != summary.vaginalPartner &&
          summary.oralPartner != summary.analPartner &&
          summary.vaginalPartner != summary.analPartner) {
        comboTypes[actor].push_back(Type::TriplePenetration);
      }

      if (summary.hasOral && (summary.hasVaginal || summary.hasAnal)) {
        const bool diffVaginal =
            summary.hasVaginal && summary.oralPartner != summary.vaginalPartner;
        const bool diffAnal = summary.hasAnal && summary.oralPartner != summary.analPartner;
        if (diffVaginal || diffAnal)
          comboTypes[actor].push_back(Type::Spitroast);
      }
    }
  }

  void ApplyComboOverrides(AssignMap& assigns,
                           const std::unordered_map<RE::Actor*, ActorInteractSummary>& summaries,
                           const std::unordered_map<RE::Actor*, std::vector<Type>>& comboTypes)
  {
    for (const auto& [actor, combos] : comboTypes) {
      auto assignIt = assigns.find(actor);
      if (assignIt == assigns.end())
        continue;

      auto summaryIt = summaries.find(actor);
      if (summaryIt == summaries.end())
        continue;

      auto& partMap                     = assignIt->second;
      const ActorInteractSummary& state = summaryIt->second;

      auto has = [&](Type type) {
        return std::find(combos.begin(), combos.end(), type) != combos.end();
      };
      auto set = [&](Name part, Type type) {
        auto it = partMap.find(part);
        if (it != partMap.end())
          it->second.type = type;
      };

      const bool hasTP = has(Type::TriplePenetration);
      const bool hasDP = has(Type::DoublePenetration);
      const bool hasSR = has(Type::Spitroast);
      const bool hasSN = has(Type::SixtyNine);

      if (hasTP) {
        set(Name::Mouth, Type::TriplePenetration);
        set(Name::Throat, Type::TriplePenetration);
        set(Name::Vagina, Type::TriplePenetration);
        set(Name::Anus, Type::TriplePenetration);
        continue;
      }

      if (hasDP) {
        set(Name::Vagina, Type::DoublePenetration);
        set(Name::Anus, Type::DoublePenetration);
      }

      if (hasSR) {
        set(Name::Mouth, Type::Spitroast);
        set(Name::Throat, Type::Spitroast);
        if (!hasDP) {
          if (state.hasVaginal)
            set(Name::Vagina, Type::Spitroast);
          if (state.hasAnal)
            set(Name::Anus, Type::Spitroast);
        }
      }

      if (hasSN)
        set(Name::Mouth, Type::SixtyNine);
    }
  }

}  // namespace

Interact::Interact(std::vector<RE::Actor*> actors)
{
  for (auto* actor : actors) {
    if (!actor)
      continue;

    ActorData data;
    data.race   = Define::Race(actor);
    data.gender = Define::Gender(actor);

    for (std::uint8_t index = 0; index < static_cast<std::uint8_t>(Name::Penis) + 1; ++index) {
      const auto name = static_cast<Name>(index);
      if (!BP::HasBodyPart(data.gender, data.race, name))
        continue;
      data.infos.emplace(name, Info{BP{actor, data.race, name}});
    }

    datas.emplace(actor, std::move(data));
  }
}

void Interact::FlashNodeData()
{
  for (auto& [actor, data] : datas)
    for (auto& [name, info] : data.infos)
      info.bodypart.UpdateNodes();
}

void Interact::Update()
{
  for (auto& [actor, data] : datas) {
    for (auto& [name, info] : data.infos) {
      info.prevType     = info.type;
      info.prevActor    = info.actor;
      info.prevDistance = info.distance;
      info.type         = Type::None;
      info.actor        = nullptr;
      info.distance     = 0.f;
      info.velocity     = 0.f;
    }
  }

  for (auto& [actor, data] : datas)
    for (auto& [name, info] : data.infos)
      info.bodypart.UpdatePosition();

  std::vector<Candidate> candidates;
  std::vector<RE::Actor*> actors;
  actors.reserve(datas.size());
  for (const auto& [actor, _] : datas)
    actors.push_back(actor);

  for (std::size_t i = 0; i < actors.size(); ++i) {
    for (std::size_t j = i; j < actors.size(); ++j) {
      RE::Actor* actorA = actors[i];
      RE::Actor* actorB = actors[j];
      GeneratePairCandidates(candidates, PairContext{actorA, actorB, datas.at(actorA),
                                                     datas.at(actorB), actorA == actorB});
    }
  }

  std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
    if (lhs.score != rhs.score)
      return lhs.score < rhs.score;
    if (lhs.family != rhs.family)
      return static_cast<std::uint8_t>(lhs.family) < static_cast<std::uint8_t>(rhs.family);
    if (lhs.distance != rhs.distance)
      return lhs.distance < rhs.distance;
    return static_cast<std::uint8_t>(lhs.type) < static_cast<std::uint8_t>(rhs.type);
  });

  using PartKey = std::pair<RE::Actor*, Name>;
  struct PartKeyHash
  {
    std::size_t operator()(const PartKey& key) const
    {
      return std::hash<RE::Actor*>{}(key.first) ^ (static_cast<std::size_t>(key.second) << 8);
    }
  };

  std::unordered_set<PartKey, PartKeyHash> usedParts;
  AssignMap assigns;

  for (const Candidate& candidate : candidates) {
    if (usedParts.count({candidate.sourceActor, candidate.sourcePart}) ||
        usedParts.count({candidate.targetActor, candidate.targetPart})) {
      continue;
    }

    const Type resolvedType = ResolveSelfType(candidate);

    assigns[candidate.sourceActor][candidate.sourcePart] = {
        candidate.targetActor, candidate.sourcePart, candidate.targetPart, candidate.distance,
        resolvedType};
    usedParts.insert({candidate.sourceActor, candidate.sourcePart});

    assigns[candidate.targetActor][candidate.targetPart] = {
        candidate.sourceActor, candidate.targetPart, candidate.sourcePart, candidate.distance,
        resolvedType};
    usedParts.insert({candidate.targetActor, candidate.targetPart});
  }

  std::unordered_map<RE::Actor*, ActorInteractSummary> summaries;
  BuildSummaries(assigns, summaries);

  std::unordered_map<RE::Actor*, std::vector<Type>> comboTypes;
  BuildComboTypes(summaries, comboTypes);
  ApplyComboOverrides(assigns, summaries, comboTypes);

  const float nowMs = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(
                                             std::chrono::steady_clock::now().time_since_epoch())
                                             .count()) /
                      1000.f;

  for (auto& [actor, data] : datas) {
    const float dt    = nowMs - data.lastUpdateMs;
    data.lastUpdateMs = nowMs;

    auto assignIt = assigns.find(actor);
    if (assignIt == assigns.end())
      continue;

    for (auto& [partName, info] : data.infos) {
      auto partIt = assignIt->second.find(partName);
      if (partIt == assignIt->second.end())
        continue;

      const AssignInfo& assign = partIt->second;
      info.actor               = assign.partner;
      info.distance            = assign.distance;
      info.type                = assign.type;
      info.velocity            = dt > 1e-4f ? (assign.distance - info.prevDistance) / dt : 0.f;
    }
  }
}

const Interact::Info& Interact::GetInfo(RE::Actor* actor, Name partName) const
{
  static const Info kEmpty{};
  auto it = datas.find(actor);
  if (it == datas.end())
    return kEmpty;
  auto partIt = it->second.infos.find(partName);
  if (partIt == it->second.infos.end())
    return kEmpty;
  return partIt->second;
}

}  // namespace Instance

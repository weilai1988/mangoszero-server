#include "BotBrain.h"

using namespace ai;

BotBrainContext::BotBrainContext() :
    inCombat(false),
    inDungeon(false),
    inRaid(false),
    hasMaster(false),
    isInGroup(false),
    groupSize(0),
    aliveMemberCount(0),
    deadMemberCount(0),
    tankCount(0),
    healerCount(0),
    dpsCount(0),
    healthPercent(0.0f),
    manaPercent(-1.0f),
    threatPercent(0),
    role(BOT_BRAIN_ROLE_UNKNOWN),
    partyPhase(BOT_BRAIN_PARTY_SOLO),
    partyHealTargetHealthPercent(0.0f),
    weakestMemberHealthPercent(0.0f)
{
}

BotDecision::BotDecision() :
    intent(BOT_BRAIN_INTENT_IDLE),
    priority(0.0f)
{
}

BotDecision::BotDecision(BotBrainIntent intent, const std::string& action, float priority) :
    intent(intent),
    action(action),
    priority(priority)
{
}

BotBrain::BotBrain(PlayerbotAI* ai) : ai(ai)
{
}

BotBrain::~BotBrain()
{
}

std::string BotBrain::GetName() const
{
    return "base";
}

void BotBrain::Update(uint32_t /*elapsed*/)
{
}

BotDecision BotBrain::SelectNextDecision(const BotBrainContext& context)
{
    if (context.healthPercent > 0.0f && context.healthPercent < 25.0f)
        return BotDecision(BOT_BRAIN_INTENT_MOVE, "survive", 100.0f);

    if (context.partyPhase == BOT_BRAIN_PARTY_RECOVER)
        return BotDecision(BOT_BRAIN_INTENT_PREPARE, "recover party", 95.0f);

    if (context.partyPhase == BOT_BRAIN_PARTY_PULL)
        return BotDecision(BOT_BRAIN_INTENT_PULL,
            context.mainTargetName.empty() ? "wait for marked target" : "open on marked target",
            90.0f);

    if (!context.inCombat)
        return BotDecision(BOT_BRAIN_INTENT_PREPARE, "group refresh", 10.0f);

    if (context.inRaid && !context.encounterName.empty())
        return BotDecision(BOT_BRAIN_INTENT_ENCOUNTER, context.encounterName, 75.0f);

    switch (context.role)
    {
        case BOT_BRAIN_ROLE_TANK:
            return BotDecision(BOT_BRAIN_INTENT_TANK,
                context.tankTargetName.empty() ? "find loose target" : "hold threat",
                80.0f);
        case BOT_BRAIN_ROLE_HEALER:
            if (!context.partyHealTargetName.empty())
            {
                if (context.partyHealTargetHealthPercent < 25.0f)
                    return BotDecision(BOT_BRAIN_INTENT_HEAL, "emergency party heal", 99.0f);
                if (context.partyHealTargetHealthPercent < 60.0f)
                    return BotDecision(BOT_BRAIN_INTENT_HEAL, "critical party heal", 94.0f);
                if (context.partyHealTargetHealthPercent < 92.0f)
                    return BotDecision(BOT_BRAIN_INTENT_HEAL, "steady party heal", 75.0f);
            }
            return BotDecision(BOT_BRAIN_INTENT_HEAL, "heal group", 90.0f);
        case BOT_BRAIN_ROLE_DPS:
            if (context.threatPercent >= 90)
                return BotDecision(BOT_BRAIN_INTENT_DPS, "wait for threat", 85.0f);
            return BotDecision(BOT_BRAIN_INTENT_DPS, "assist tank", 60.0f);
        default:
            return BotDecision(BOT_BRAIN_INTENT_DPS, "assist group", 40.0f);
    }
}

const char* ai::BotBrainRoleName(BotBrainRole role)
{
    switch (role)
    {
        case BOT_BRAIN_ROLE_TANK:
            return "tank";
        case BOT_BRAIN_ROLE_HEALER:
            return "healer";
        case BOT_BRAIN_ROLE_DPS:
            return "dps";
        case BOT_BRAIN_ROLE_SUPPORT:
            return "support";
        default:
            return "unknown";
    }
}

const char* ai::BotBrainIntentName(BotBrainIntent intent)
{
    switch (intent)
    {
        case BOT_BRAIN_INTENT_PREPARE:
            return "prepare";
        case BOT_BRAIN_INTENT_PULL:
            return "pull";
        case BOT_BRAIN_INTENT_TANK:
            return "tank";
        case BOT_BRAIN_INTENT_HEAL:
            return "heal";
        case BOT_BRAIN_INTENT_DPS:
            return "dps";
        case BOT_BRAIN_INTENT_MOVE:
            return "move";
        case BOT_BRAIN_INTENT_LOOT:
            return "loot";
        case BOT_BRAIN_INTENT_ENCOUNTER:
            return "encounter";
        default:
            return "idle";
    }
}

const char* ai::BotBrainPartyPhaseName(BotBrainPartyPhase phase)
{
    switch (phase)
    {
        case BOT_BRAIN_PARTY_PREPARE:
            return "prepare";
        case BOT_BRAIN_PARTY_PULL:
            return "pull";
        case BOT_BRAIN_PARTY_FIGHT:
            return "fight";
        case BOT_BRAIN_PARTY_RECOVER:
            return "recover";
        default:
            return "solo";
    }
}

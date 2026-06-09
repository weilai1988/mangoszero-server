#include "botpch.h"
#include "../../playerbot.h"
#include "HealthTriggers.h"

using namespace ai;

float HealthInRangeTrigger::GetValue()
{
    return AI_VALUE2(uint8, "health", GetTargetName());
}

bool PartyMemberDeadTrigger::IsActive()
{
	return GetTarget();
}

bool DeadTrigger::IsActive()
{
    return AI_VALUE2(bool, "dead", GetTargetName());
}

bool BrainPartyMemberHealthTrigger::IsActive()
{
    BotBrainContext brainContext = ai->BuildBrainContext();
    if (brainContext.role != BOT_BRAIN_ROLE_HEALER || brainContext.partyHealTargetName.empty())
        return false;

    Unit* target = GetTarget();
    if (!target || !target->IsAlive())
        return false;

    float health = target->GetHealthPercent();
    return health < maxValue && health >= minValue;
}

bool AoeHealTrigger::IsActive()
{
    return AI_VALUE2(uint8, "aoe heal", type) >= count;
}


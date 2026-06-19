#include "botpch.h"
#include "../../playerbot.h"
#include "../../HumanPlayerStrategyMgr.h"
#include "HumanStrategy.h"

using namespace ai;

NextAction** HumanPlayerPatternStrategy::getDefaultActions()
{
    return sHumanPlayerStrategyMgr.CreateDefaultActions(ai);
}

void HumanPlayerPatternStrategy::InitMultipliers(std::list<Multiplier*> &multipliers)
{
    multipliers.push_back(new HumanPlayerPatternMultiplier(ai));
}

float HumanPlayerPatternMultiplier::GetValue(Action* action)
{
    return sHumanPlayerStrategyMgr.GetMultiplier(ai, action);
}

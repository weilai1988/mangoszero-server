#pragma once

#include "../Strategy.h"

namespace ai
{
    class HumanPlayerPatternMultiplier : public Multiplier
    {
    public:
        HumanPlayerPatternMultiplier(PlayerbotAI* ai) : Multiplier(ai, "human") {}
        virtual float GetValue(Action* action);
    };

    class HumanPlayerPatternStrategy : public Strategy
    {
    public:
        HumanPlayerPatternStrategy(PlayerbotAI* ai) : Strategy(ai) {}

        virtual string getName() { return "human"; }
        virtual int GetType() { return STRATEGY_TYPE_COMBAT; }
        virtual NextAction** getDefaultActions();
        virtual void InitMultipliers(std::list<Multiplier*> &multipliers);
    };
}

#pragma once

#include "Ai/Base/BotBrain.h"

namespace ai
{
    class WarriorBotBrain : public BotBrain
    {
    public:
        explicit WarriorBotBrain(PlayerbotAI* ai) : BotBrain(ai) {}

        virtual std::string GetName() const { return "warrior"; }
        virtual BotDecision SelectNextDecision(const BotBrainContext& context)
        {
            if (!context.inCombat)
                return BotDecision(BOT_BRAIN_INTENT_PREPARE, "battle shout and defensive stance", 25.0f);
            if (context.role == BOT_BRAIN_ROLE_TANK)
                return BotDecision(BOT_BRAIN_INTENT_TANK, "taunt and hold threat", 95.0f);
            return BotDecision(BOT_BRAIN_INTENT_DPS, "melee assist tank", 60.0f);
        }
    };
}

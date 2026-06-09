#pragma once

#include "Ai/Base/BotBrain.h"

namespace ai
{
    class HunterBotBrain : public BotBrain
    {
    public:
        explicit HunterBotBrain(PlayerbotAI* ai) : BotBrain(ai) {}

        virtual std::string GetName() const { return "hunter"; }
        virtual BotDecision SelectNextDecision(const BotBrainContext& context)
        {
            if (!context.inCombat)
                return BotDecision(BOT_BRAIN_INTENT_PREPARE, "prepare pet and ammo", 20.0f);
            return BotDecision(BOT_BRAIN_INTENT_DPS, "ranged assist tank", 65.0f);
        }
    };
}

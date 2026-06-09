#pragma once

#include "Ai/Base/BotBrain.h"

namespace ai
{
    class MageBotBrain : public BotBrain
    {
    public:
        explicit MageBotBrain(PlayerbotAI* ai) : BotBrain(ai) {}

        virtual std::string GetName() const { return "mage"; }
        virtual BotDecision SelectNextDecision(const BotBrainContext& context)
        {
            if (!context.inCombat)
                return BotDecision(BOT_BRAIN_INTENT_PREPARE, "refresh intellect and food", 20.0f);
            return BotDecision(BOT_BRAIN_INTENT_DPS, "ranged assist tank", 65.0f);
        }
    };
}

#pragma once

#include "Ai/Base/BotBrain.h"

namespace ai
{
    class PriestBotBrain : public BotBrain
    {
    public:
        explicit PriestBotBrain(PlayerbotAI* ai) : BotBrain(ai) {}

        virtual std::string GetName() const { return "priest"; }
        virtual BotDecision SelectNextDecision(const BotBrainContext& context)
        {
            if (!context.inCombat)
                return BotDecision(BOT_BRAIN_INTENT_PREPARE, "refresh fortitude and spirit", 25.0f);
            return BotDecision(BOT_BRAIN_INTENT_HEAL, "heal and shield group", 95.0f);
        }
    };
}

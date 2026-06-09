#pragma once

#include "Ai/Base/BotBrain.h"

namespace ai
{
    class WarlockBotBrain : public BotBrain
    {
    public:
        explicit WarlockBotBrain(PlayerbotAI* ai) : BotBrain(ai) {}

        virtual std::string GetName() const { return "warlock"; }
        virtual BotDecision SelectNextDecision(const BotBrainContext& context)
        {
            if (!context.inCombat)
                return BotDecision(BOT_BRAIN_INTENT_PREPARE, "prepare demon and stones", 20.0f);
            return BotDecision(BOT_BRAIN_INTENT_DPS, "dot and assist tank", 65.0f);
        }
    };
}

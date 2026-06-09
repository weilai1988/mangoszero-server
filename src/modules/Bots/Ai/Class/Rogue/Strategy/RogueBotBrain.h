#pragma once

#include "Ai/Base/BotBrain.h"

namespace ai
{
    class RogueBotBrain : public BotBrain
    {
    public:
        explicit RogueBotBrain(PlayerbotAI* ai) : BotBrain(ai) {}

        virtual std::string GetName() const { return "rogue"; }
        virtual BotDecision SelectNextDecision(const BotBrainContext& context)
        {
            if (!context.inCombat)
                return BotDecision(BOT_BRAIN_INTENT_PREPARE, "prepare melee opener", 15.0f);
            return BotDecision(BOT_BRAIN_INTENT_DPS, "melee assist tank", 65.0f);
        }
    };
}

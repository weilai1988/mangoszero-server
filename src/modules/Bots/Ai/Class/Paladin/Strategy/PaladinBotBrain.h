#pragma once

#include "Ai/Base/BotBrain.h"

namespace ai
{
    class PaladinBotBrain : public BotBrain
    {
    public:
        explicit PaladinBotBrain(PlayerbotAI* ai) : BotBrain(ai) {}

        virtual std::string GetName() const { return "paladin"; }
        virtual BotDecision SelectNextDecision(const BotBrainContext& context)
        {
            if (context.role == BOT_BRAIN_ROLE_HEALER)
                return BotDecision(BOT_BRAIN_INTENT_HEAL, "heal and cleanse group", 90.0f);
            if (context.role == BOT_BRAIN_ROLE_TANK)
                return BotDecision(BOT_BRAIN_INTENT_TANK, "hold aoe threat", 80.0f);
            return BotBrain::SelectNextDecision(context);
        }
    };
}

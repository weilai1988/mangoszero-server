#pragma once

#include "Ai/Base/BotBrain.h"

namespace ai
{
    class DruidBotBrain : public BotBrain
    {
    public:
        explicit DruidBotBrain(PlayerbotAI* ai) : BotBrain(ai) {}

        virtual std::string GetName() const { return "druid"; }
        virtual BotDecision SelectNextDecision(const BotBrainContext& context)
        {
            if (context.role == BOT_BRAIN_ROLE_HEALER)
                return BotDecision(BOT_BRAIN_INTENT_HEAL, "heal group", 90.0f);
            if (context.role == BOT_BRAIN_ROLE_TANK)
                return BotDecision(BOT_BRAIN_INTENT_TANK, "bear tank", 80.0f);
            return BotBrain::SelectNextDecision(context);
        }
    };
}

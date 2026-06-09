#pragma once

#include "Ai/Base/BotBrain.h"

namespace ai
{
    class ShamanBotBrain : public BotBrain
    {
    public:
        explicit ShamanBotBrain(PlayerbotAI* ai) : BotBrain(ai) {}

        virtual std::string GetName() const { return "shaman"; }
        virtual BotDecision SelectNextDecision(const BotBrainContext& context)
        {
            if (!context.inCombat)
                return BotDecision(BOT_BRAIN_INTENT_PREPARE, "prepare weapon imbue and totems", 20.0f);
            if (context.role == BOT_BRAIN_ROLE_HEALER)
                return BotDecision(BOT_BRAIN_INTENT_HEAL, "heal and cure group", 90.0f);
            return BotBrain::SelectNextDecision(context);
        }
    };
}

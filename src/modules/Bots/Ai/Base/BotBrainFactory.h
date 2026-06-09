#pragma once

class Player;
class PlayerbotAI;

namespace ai
{
    class BotBrain;

    class BotBrainFactory
    {
    public:
        static BotBrain* Create(Player* bot, PlayerbotAI* ai);
    };
}

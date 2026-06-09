#include "BotBrainFactory.h"

#include "BotBrain.h"
#include "Player.h"
#include "Ai/Class/Druid/Strategy/DruidBotBrain.h"
#include "Ai/Class/Hunter/Strategy/HunterBotBrain.h"
#include "Ai/Class/Mage/Strategy/MageBotBrain.h"
#include "Ai/Class/Paladin/Strategy/PaladinBotBrain.h"
#include "Ai/Class/Priest/Strategy/PriestBotBrain.h"
#include "Ai/Class/Rogue/Strategy/RogueBotBrain.h"
#include "Ai/Class/Shaman/Strategy/ShamanBotBrain.h"
#include "Ai/Class/Warlock/Strategy/WarlockBotBrain.h"
#include "Ai/Class/Warrior/Strategy/WarriorBotBrain.h"

using namespace ai;

BotBrain* BotBrainFactory::Create(Player* bot, PlayerbotAI* ai)
{
    if (!bot)
        return new BotBrain(ai);

    switch (bot->getClass())
    {
        case CLASS_DRUID:
            return new DruidBotBrain(ai);
        case CLASS_HUNTER:
            return new HunterBotBrain(ai);
        case CLASS_MAGE:
            return new MageBotBrain(ai);
        case CLASS_PALADIN:
            return new PaladinBotBrain(ai);
        case CLASS_PRIEST:
            return new PriestBotBrain(ai);
        case CLASS_ROGUE:
            return new RogueBotBrain(ai);
        case CLASS_SHAMAN:
            return new ShamanBotBrain(ai);
        case CLASS_WARLOCK:
            return new WarlockBotBrain(ai);
        case CLASS_WARRIOR:
            return new WarriorBotBrain(ai);
        default:
            return new BotBrain(ai);
    }
}

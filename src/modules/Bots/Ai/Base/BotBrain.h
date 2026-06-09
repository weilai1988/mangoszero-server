#pragma once

#include <stdint.h>
#include <string>

class PlayerbotAI;

namespace ai
{
    enum BotBrainRole
    {
        BOT_BRAIN_ROLE_UNKNOWN = 0,
        BOT_BRAIN_ROLE_TANK,
        BOT_BRAIN_ROLE_HEALER,
        BOT_BRAIN_ROLE_DPS,
        BOT_BRAIN_ROLE_SUPPORT
    };

    enum BotBrainIntent
    {
        BOT_BRAIN_INTENT_IDLE = 0,
        BOT_BRAIN_INTENT_PREPARE,
        BOT_BRAIN_INTENT_PULL,
        BOT_BRAIN_INTENT_TANK,
        BOT_BRAIN_INTENT_HEAL,
        BOT_BRAIN_INTENT_DPS,
        BOT_BRAIN_INTENT_MOVE,
        BOT_BRAIN_INTENT_LOOT,
        BOT_BRAIN_INTENT_ENCOUNTER
    };

    enum BotBrainPartyPhase
    {
        BOT_BRAIN_PARTY_SOLO = 0,
        BOT_BRAIN_PARTY_PREPARE,
        BOT_BRAIN_PARTY_PULL,
        BOT_BRAIN_PARTY_FIGHT,
        BOT_BRAIN_PARTY_RECOVER
    };

    struct BotBrainContext
    {
        BotBrainContext();

        bool inCombat;
        bool inDungeon;
        bool inRaid;
        bool hasMaster;
        bool isInGroup;
        uint32_t groupSize;
        uint32_t aliveMemberCount;
        uint32_t deadMemberCount;
        uint32_t tankCount;
        uint32_t healerCount;
        uint32_t dpsCount;
        float healthPercent;
        float manaPercent;
        uint8_t threatPercent;
        BotBrainRole role;
        BotBrainPartyPhase partyPhase;
        std::string zoneName;
        std::string encounterName;
        std::string currentTargetName;
        std::string tankTargetName;
        std::string partyHealTargetName;
        float partyHealTargetHealthPercent;
        std::string tankName;
        std::string mainTargetMarker;
        std::string mainTargetName;
        std::string crowdControlMarker;
        std::string crowdControlTargetName;
        std::string weakestMemberName;
        float weakestMemberHealthPercent;
    };

    struct BotDecision
    {
        BotDecision();
        BotDecision(BotBrainIntent intent, const std::string& action, float priority);

        BotBrainIntent intent;
        std::string action;
        std::string target;
        float priority;
    };

    class BotBrain
    {
    public:
        explicit BotBrain(PlayerbotAI* ai);
        virtual ~BotBrain();

        virtual std::string GetName() const;
        virtual void Update(uint32_t elapsed);
        virtual BotDecision SelectNextDecision(const BotBrainContext& context);

    protected:
        PlayerbotAI* ai;
    };

    const char* BotBrainRoleName(BotBrainRole role);
    const char* BotBrainIntentName(BotBrainIntent intent);
    const char* BotBrainPartyPhaseName(BotBrainPartyPhase phase);
}

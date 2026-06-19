#pragma once

#include "strategy/Action.h"

#include <string>
#include <vector>

class Player;
class PlayerbotAI;

namespace ai
{
    struct HumanPlayerStrategyRule
    {
        std::string playerClass;
        std::string role;
        std::string context;
        std::string action;
        float relevance;
        float multiplier;
        std::string note;
    };
}

class HumanPlayerStrategyMgr
{
public:
    bool Initialize();
    ai::NextAction** CreateDefaultActions(PlayerbotAI* ai);
    float GetMultiplier(PlayerbotAI* ai, ai::Action* action);
    std::string FormatSummary(PlayerbotAI* ai);

private:
    void LoadBuiltInDefaults();
    bool LoadFromFile(std::string const& path);
    bool ParseRule(std::string const& line, ai::HumanPlayerStrategyRule& rule);
    bool Matches(PlayerbotAI* ai, ai::HumanPlayerStrategyRule const& rule);
    std::string GetClassName(Player* bot);
    std::string GetRoleName(PlayerbotAI* ai);
    std::string GetContextName(Player* bot);
    std::string Normalize(std::string text);
    std::string Trim(std::string text);

private:
    std::vector<ai::HumanPlayerStrategyRule> rules;
    bool loadedFromFile = false;
};

#define sHumanPlayerStrategyMgr MaNGOS::Singleton<HumanPlayerStrategyMgr>::Instance()

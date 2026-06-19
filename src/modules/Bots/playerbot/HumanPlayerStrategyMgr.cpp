#include "../botpch.h"
#include "HumanPlayerStrategyMgr.h"
#include "PlayerbotAIConfig.h"
#include "playerbot.h"
#include "Group.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>

using namespace ai;
using namespace std;

INSTANTIATE_SINGLETON_1(HumanPlayerStrategyMgr);

namespace
{
    bool WildcardOrEqual(string const& pattern, string const& value)
    {
        return pattern.empty() || pattern == "*" || pattern == value;
    }
}

bool HumanPlayerStrategyMgr::Initialize()
{
    rules.clear();
    loadedFromFile = false;

    if (!sPlayerbotAIConfig.humanStrategyProfilePath.empty())
        loadedFromFile = LoadFromFile(sPlayerbotAIConfig.humanStrategyProfilePath);

    if (!loadedFromFile)
        LoadBuiltInDefaults();

    sLog.outString("AI Playerbot human strategy profiles: %u rules%s",
        (uint32)rules.size(), loadedFromFile ? " (profile file loaded)" : " (built-in defaults)");

    return true;
}

void HumanPlayerStrategyMgr::LoadBuiltInDefaults()
{
    static const char* defaults[] =
    {
        "warrior|tank|dungeon|tank assist|96|1.35|snap to the loose or marked tank target",
        "warrior|tank|dungeon|taunt|95|2.50|rescue party members and ranged mobs quickly",
        "warrior|tank|dungeon|reach melee|93|1.60|close distance after selecting ranged targets",
        "warrior|tank|dungeon|set facing|92|1.40|avoid back-facing mobs and daze",
        "warrior|tank|dungeon|shield bash|88|1.80|interrupt dangerous caster mobs",
        "warrior|tank|dungeon|shield bash on enemy healer|87|1.80|interrupt enemy healers",
        "warrior|tank|dungeon|revenge|45|1.70|high value rage-efficient threat",
        "warrior|tank|dungeon|shield slam|44|1.60|strong single-target threat when available",
        "warrior|tank|dungeon|sunder armor|43|1.50|stack steady single-target threat",
        "warrior|tank|dungeon|thunder clap|42|1.55|early multi-target threat and mitigation",
        "warrior|tank|dungeon|demoralizing shout|41|1.45|wide initial threat and damage reduction",
        "warrior|tank|dungeon|battle shout taunt|40|1.35|backup group threat pulse",
        "warrior|tank|dungeon|cleave|39|1.25|rage dump for two-target threat",
        "warrior|tank|dungeon|heroic strike|38|1.20|single-target rage dump",
        "warrior|tank|dungeon|shield block|37|1.20|enable revenge and reduce spike damage",
        "warrior|tank|dungeon|bloodrage|36|1.15|rage setup when safe",
        "warrior|tank|dungeon|shoot|31|1.15|controlled ranged opener/fallback",
        "warrior|tank|dungeon|charge|30|1.10|opener when position allows",
        NULL
    };

    for (uint32 i = 0; defaults[i]; ++i)
    {
        HumanPlayerStrategyRule rule;
        if (ParseRule(defaults[i], rule))
            rules.push_back(rule);
    }
}

bool HumanPlayerStrategyMgr::LoadFromFile(string const& path)
{
    ifstream input(path.c_str());
    if (!input.is_open())
        return false;

    uint32 loaded = 0;
    string line;
    while (getline(input, line))
    {
        HumanPlayerStrategyRule rule;
        if (!ParseRule(line, rule))
            continue;

        rules.push_back(rule);
        ++loaded;
    }

    if (loaded)
        sLog.outString("AI Playerbot loaded %u human strategy profile rules from %s", loaded, path.c_str());

    return loaded > 0;
}

bool HumanPlayerStrategyMgr::ParseRule(string const& line, HumanPlayerStrategyRule& rule)
{
    string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#')
        return false;

    vector<string> parts = split(trimmed, '|');
    if (parts.size() < 6)
        return false;

    rule.playerClass = Normalize(parts[0]);
    rule.role = Normalize(parts[1]);
    rule.context = Normalize(parts[2]);
    rule.action = Normalize(parts[3]);
    rule.relevance = (float)atof(Trim(parts[4]).c_str());
    rule.multiplier = (float)atof(Trim(parts[5]).c_str());
    rule.note = parts.size() > 6 ? Trim(parts[6]) : "";

    return !rule.action.empty() && (rule.relevance > 0.0f || rule.multiplier > 0.0f);
}

NextAction** HumanPlayerStrategyMgr::CreateDefaultActions(PlayerbotAI* ai)
{
    if (!sPlayerbotAIConfig.humanStrategyEnabled || !ai || !ai->GetBot())
        return NULL;

    vector<HumanPlayerStrategyRule const*> matches;
    for (vector<HumanPlayerStrategyRule>::const_iterator i = rules.begin(); i != rules.end(); ++i)
    {
        if (i->relevance > 0.0f && Matches(ai, *i))
            matches.push_back(&(*i));
    }

    if (matches.empty())
        return NULL;

    NextAction** actions = new NextAction*[matches.size() + 1];
    for (uint32 i = 0; i < matches.size(); ++i)
        actions[i] = new NextAction(matches[i]->action, matches[i]->relevance);

    actions[matches.size()] = NULL;
    return actions;
}

float HumanPlayerStrategyMgr::GetMultiplier(PlayerbotAI* ai, Action* action)
{
    if (!sPlayerbotAIConfig.humanStrategyEnabled || !ai || !ai->GetBot() || !action)
        return 1.0f;

    float result = 1.0f;
    string actionName = Normalize(action->getName());

    for (vector<HumanPlayerStrategyRule>::const_iterator i = rules.begin(); i != rules.end(); ++i)
    {
        if (i->multiplier <= 0.0f || i->action != actionName || !Matches(ai, *i))
            continue;

        result *= i->multiplier;
    }

    if (result < 0.0f)
        return 0.0f;

    if (result > 8.0f)
        return 8.0f;

    return result;
}

string HumanPlayerStrategyMgr::FormatSummary(PlayerbotAI* ai)
{
    ostringstream out;
    uint32 count = 0;

    out << "Human profile";
    if (!sPlayerbotAIConfig.humanStrategyEnabled)
    {
        out << " disabled";
        return out.str();
    }

    out << " " << GetClassName(ai ? ai->GetBot() : NULL) << "/" << GetRoleName(ai);
    out << "/" << GetContextName(ai ? ai->GetBot() : NULL) << ": ";

    for (vector<HumanPlayerStrategyRule>::const_iterator i = rules.begin(); i != rules.end(); ++i)
    {
        if (!Matches(ai, *i))
            continue;

        if (count)
            out << ", ";

        out << i->action << "@" << i->relevance << "x" << i->multiplier;
        ++count;

        if (count >= 12)
        {
            out << ", ...";
            break;
        }
    }

    if (!count)
        out << "no matching rules";

    return out.str();
}

bool HumanPlayerStrategyMgr::Matches(PlayerbotAI* ai, HumanPlayerStrategyRule const& rule)
{
    if (!ai || !ai->GetBot())
        return false;

    return WildcardOrEqual(rule.playerClass, GetClassName(ai->GetBot())) &&
        WildcardOrEqual(rule.role, GetRoleName(ai)) &&
        WildcardOrEqual(rule.context, GetContextName(ai->GetBot()));
}

string HumanPlayerStrategyMgr::GetClassName(Player* bot)
{
    if (!bot)
        return "";

    switch (bot->getClass())
    {
        case CLASS_WARRIOR: return "warrior";
        case CLASS_PALADIN: return "paladin";
        case CLASS_HUNTER: return "hunter";
        case CLASS_ROGUE: return "rogue";
        case CLASS_PRIEST: return "priest";
        case CLASS_SHAMAN: return "shaman";
        case CLASS_MAGE: return "mage";
        case CLASS_WARLOCK: return "warlock";
        case CLASS_DRUID: return "druid";
        default: return "";
    }
}

string HumanPlayerStrategyMgr::GetRoleName(PlayerbotAI* ai)
{
    if (!ai || !ai->GetBot())
        return "";

    Player* bot = ai->GetBot();
    if (ai->IsTank(bot))
        return "tank";

    if (ai->IsHeal(bot))
        return "heal";

    return ai->IsRanged(bot) ? "ranged-dps" : "melee-dps";
}

string HumanPlayerStrategyMgr::GetContextName(Player* bot)
{
    if (!bot)
        return "";

    Group* group = bot->GetGroup();
    if (!group)
        return "solo";

    return group->isRaidGroup() ? "raid" : "dungeon";
}

string HumanPlayerStrategyMgr::Normalize(string text)
{
    text = Trim(text);
    transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return text;
}

string HumanPlayerStrategyMgr::Trim(string text)
{
    string::size_type start = text.find_first_not_of(" \t\r\n");
    if (start == string::npos)
        return "";

    string::size_type end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

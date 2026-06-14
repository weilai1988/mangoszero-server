#include "botpch.h"
#include "../../playerbot.h"
#include "../../PlayerbotFactory.h"
#include "ChangeTalentsAction.h"
#include <cctype>

using namespace ai;

namespace
{
    string NormalizeTalentCommand(string text)
    {
        size_t start = text.find_first_not_of(" \t\r\n");
        if (start == string::npos)
            return "";

        size_t end = text.find_last_not_of(" \t\r\n");
        text = text.substr(start, end - start + 1);

        for (string::iterator i = text.begin(); i != text.end(); ++i)
            *i = char(tolower((unsigned char)*i));

        size_t space = text.find(' ');
        return space == string::npos ? text : text.substr(0, space);
    }
}

bool ChangeTalentsAction::Execute(Event event)
{
    if (!GetMaster())
        return false;

    string command = NormalizeTalentCommand(event.getParam());
    if (command.empty() || command == "?" || command == "help" || command == "list")
    {
        ai->TellMaster("Usage: talents tank/heal/dps/auto/train");
        return true;
    }

    PlayerbotFactory factory(bot, bot->getLevel());
    if (command == "train" || command == "spell" || command == "spells")
    {
        factory.TrainAvailableSpells();
        ai->TellMaster("Available spells refreshed");
        return true;
    }

    string appliedRole, appliedSpec;
    if (!factory.ApplyRoleTalents(command, appliedRole, appliedSpec))
    {
        ai->TellError("This role is not available for my class. Use talents tank/heal/dps/auto/train");
        return false;
    }

    ostringstream out;
    out << "Talents set to " << appliedRole;
    if (!appliedSpec.empty())
        out << " (" << appliedSpec << ")";
    out << "; available spells refreshed";
    ai->TellMaster(out.str());
    return true;
}

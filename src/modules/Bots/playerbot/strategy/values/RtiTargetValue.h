#pragma once
#include "../../PlayerbotAIConfig.h"
#include "../../ServerFacade.h"
#include "../Value.h"
#include "AttackersValue.h"
#include "Group.h"
#include "TargetValue.h"

namespace ai
{
    class RtiTargetValue : public TargetValue
    {
    public:
        RtiTargetValue(PlayerbotAI* ai, string type = "rti") : type(type), TargetValue(ai)
        {}

    public:
        static int GetRtiIndex(string rti)
        {
            int index = -1;
            if(rti == "star") index = 0;
            else if(rti == "circle") index = 1;
            else if(rti == "diamond") index = 2;
            else if(rti == "triangle") index = 3;
            else if(rti == "moon") index = 4;
            else if(rti == "square") index = 5;
            else if(rti == "cross") index = 6;
            else if(rti == "skull") index = 7;
            return index;
        }

        Unit *Calculate()
        {
            Group *group = bot->GetGroup();
            if(!group)
                return NULL;

            string rti = AI_VALUE(string, type);
            if (type == "rti" && IsOrderedKillRti(rti))
            {
                const char* killOrder[] = { "skull", "cross", "square", "moon" };
                for (uint32 i = 0; i < sizeof(killOrder) / sizeof(killOrder[0]); ++i)
                {
                    Unit* unit = GetMarkedTarget(group, GetRtiIndex(killOrder[i]));
                    if (unit)
                        return unit;
                }

                return NULL;
            }

            int index = GetRtiIndex(rti);

            if (index == -1)
                return NULL;

            return GetMarkedTarget(group, index);
        }

    private:
        bool IsOrderedKillRti(string rti)
        {
            return rti.empty() || rti == "kill" || rti == "order" ||
                rti == "skull" || rti == "cross" || rti == "square" || rti == "moon";
        }

        Unit* GetMarkedTarget(Group* group, int index)
        {
            if (index == -1)
                return NULL;

            ObjectGuid guid = group->GetTargetIcon(index);
            if (!guid)
                return NULL;

            Unit* unit = ai->GetUnit(ObjectGuid(guid));
            if (!unit || sServerFacade.UnitIsDead(unit) ||
                    !sServerFacade.IsWithinLOSInMap(bot, unit) ||
                    sServerFacade.IsDistanceGreaterThan(sServerFacade.GetDistance2d(bot, unit), sPlayerbotAIConfig.sightDistance) ||
                    !AttackersValue::IsPossibleTarget(unit, bot))
                return NULL;

            list<ObjectGuid> attackers = context->GetValue<list<ObjectGuid> >("attackers")->Get();
            if (find(attackers.begin(), attackers.end(), guid) != attackers.end())
                return unit;

            if (IsMasterEngagingMarkedTarget(unit))
                return unit;

            return NULL;
        }

        bool IsMasterEngagingMarkedTarget(Unit* unit)
        {
            Player* master = ai->GetMaster();
            if (!master || !unit || !master->IsAlive() || !master->IsInCombat())
                return false;

            ObjectGuid guid = unit->GetObjectGuid();
            if (master->GetSelectionGuid() == guid)
                return true;

            if (master->GetTargetGuid() == guid)
                return true;

            return master->getVictim() == unit;
        }

        string type;
    };

    class RtiCcTargetValue : public RtiTargetValue
    {
    public:
        RtiCcTargetValue(PlayerbotAI* ai) : RtiTargetValue(ai, "rti cc") {}
    };
}

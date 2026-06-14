#include "botpch.h"
#include "../../playerbot.h"
#include "TankTargetValue.h"

#include "AttackersValue.h"
#include "../../ServerFacade.h"
#include <cmath>
using namespace ai;

class FindTargetForTankStrategy : public FindNonCcTargetStrategy
{
public:
    FindTargetForTankStrategy(PlayerbotAI* ai) : FindNonCcTargetStrategy(ai)
    {
        minThreat = 0;
        bestPriority = -1;
        bestVictimDistance = 0.0f;
    }

public:
    virtual void CheckAttacker(Unit* creature, ThreatManager* threatManager)
    {
        Player* bot = ai->GetBot();
        if (IsCcTarget(creature)) return;

        Unit* victim = GetLooseMobVictim(creature, threatManager);
        int priority = GetLooseMobPriority(victim);
        float threat = threatManager->getThreat(bot);
        float victimDistance = victim ? sServerFacade.GetDistance2d(creature, victim) : sServerFacade.GetDistance2d(bot, creature);
        bool lowerThreat = priority == bestPriority && (minThreat - threat) > 0.1f;
        bool closerToVictim = priority == bestPriority && fabs(minThreat - threat) <= 0.1f &&
            (!bestVictimDistance || victimDistance + 0.5f < bestVictimDistance);

        if (!result || priority > bestPriority || lowerThreat || closerToVictim)
        {
            bestPriority = priority;
            minThreat = threat;
            bestVictimDistance = victimDistance;
            result = creature;
        }
    }

protected:
    Unit* GetLooseMobVictim(Unit* creature, ThreatManager* threatManager)
    {
        HostileReference* ref = threatManager->getCurrentVictim();
        if (ref)
            return ref->getTarget();

        if (creature->GetTargetGuid())
            return ai->GetUnit(creature->GetTargetGuid());

        return NULL;
    }

    int GetLooseMobPriority(Unit* victim)
    {
        if (!victim)
            return 0;

        Player* bot = ai->GetBot();
        if (victim == bot)
            return 1;

        Player* victimPlayer = dynamic_cast<Player*>(victim);
        if (!victimPlayer)
            return 0;

        Group* group = bot->GetGroup();
        bool sameGroup = group && group->IsMember(victimPlayer->GetObjectGuid());
        if (sameGroup)
        {
            Player* master = ai->GetMaster();
            if (ai->IsHeal(victimPlayer))
                return 6;

            if (master && victimPlayer == master)
                return 5;

            if (!ai->IsTank(victimPlayer))
                return 4;

            if (victimPlayer != bot)
                return 3;
        }

        return ai->IsTank(victimPlayer) ? 1 : 2;
    }

    float minThreat;
    int bestPriority;
    float bestVictimDistance;
};


Unit* TankTargetValue::Calculate()
{
    FindTargetForTankStrategy strategy(ai);
    return FindTarget(&strategy);
}

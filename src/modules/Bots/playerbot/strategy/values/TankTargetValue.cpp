#include "botpch.h"
#include "../../playerbot.h"
#include "TankTargetValue.h"

#include "AttackersValue.h"
using namespace ai;

class FindTargetForTankStrategy : public FindNonCcTargetStrategy
{
public:
    FindTargetForTankStrategy(PlayerbotAI* ai) : FindNonCcTargetStrategy(ai)
    {
        minThreat = 0;
        bestPriority = -1;
    }

public:
    virtual void CheckAttacker(Unit* creature, ThreatManager* threatManager)
    {
        Player* bot = ai->GetBot();
        if (IsCcTarget(creature)) return;

        int priority = GetLooseMobPriority(creature, threatManager);
        float threat = threatManager->getThreat(bot);
        if (!result || priority > bestPriority || (priority == bestPriority && (minThreat - threat) > 0.1f))
        {
            bestPriority = priority;
            minThreat = threat;
            result = creature;
        }
    }

protected:
    int GetLooseMobPriority(Unit* creature, ThreatManager* threatManager)
    {
        Player* bot = ai->GetBot();
        Unit* victim = NULL;

        HostileReference* ref = threatManager->getCurrentVictim();
        if (ref)
            victim = ref->getTarget();

        if (!victim && creature->GetTargetGuid())
            victim = ai->GetUnit(creature->GetTargetGuid());

        if (!victim)
            return 0;

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
};


Unit* TankTargetValue::Calculate()
{
    FindTargetForTankStrategy strategy(ai);
    return FindTarget(&strategy);
}

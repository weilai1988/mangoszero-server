#include "botpch.h"
#include "../../playerbot.h"
#include "TargetValue.h"

#include "../../ServerFacade.h"
#include "RtiTargetValue.h"
#include "Unit.h"

using namespace ai;

namespace
{
    bool IsHardControlled(Unit* attacker)
    {
        return !attacker ||
            attacker->IsPolymorphed() ||
            attacker->HasAuraType(SPELL_AURA_MOD_CONFUSE) ||
            attacker->HasAuraType(SPELL_AURA_MOD_FEAR) ||
            attacker->HasAuraType(SPELL_AURA_MOD_STUN) ||
            attacker->HasAuraType(SPELL_AURA_MOD_PACIFY) ||
            attacker->HasAuraType(SPELL_AURA_MOD_PACIFY_SILENCE) ||
            sServerFacade.IsFeared(attacker) ||
            sServerFacade.IsCharmed(attacker);
    }

    Unit* GetCurrentVictim(PlayerbotAI* ai, Unit* attacker)
    {
        if (!attacker)
            return NULL;

        ThreatManager& threatManager = sServerFacade.GetThreatManager(attacker);
        HostileReference* ref = threatManager.getCurrentVictim();
        if (ref)
            return ref->getTarget();

        ObjectGuid targetGuid = attacker->GetTargetGuid();
        return targetGuid && ai ? ai->GetUnit(targetGuid) : NULL;
    }

    bool IsActiveThreatToParty(PlayerbotAI* ai, Unit* attacker)
    {
        if (!ai || IsHardControlled(attacker))
            return false;

        Player* bot = ai->GetBot();
        Group* group = bot ? bot->GetGroup() : NULL;
        if (!group)
            return false;

        Unit* victim = GetCurrentVictim(ai, attacker);
        if (!victim || !sServerFacade.IsAlive(victim))
            return false;

        Player* victimPlayer = dynamic_cast<Player*>(victim);
        return victimPlayer && group->IsMember(victimPlayer->GetObjectGuid());
    }
}

Unit* TargetValue::FindTarget(FindTargetStrategy* strategy)
{
    list<ObjectGuid> attackers = ai->GetAiObjectContext()->GetValue<list<ObjectGuid> >("attackers")->Get();
    for (list<ObjectGuid>::iterator i = attackers.begin(); i != attackers.end(); ++i)
    {
        Unit* unit = ai->GetUnit(*i);
        if (!unit)
            continue;

        ThreatManager &threatManager = sServerFacade.GetThreatManager(unit);
        strategy->CheckAttacker(unit, &threatManager);
    }

    return strategy->GetResult();
}


bool FindNonCcTargetStrategy::IsCcTarget(Unit* attacker)
{
    if (IsActiveThreatToParty(ai, attacker))
        return false;

    Group* group = ai->GetBot()->GetGroup();
    if (group)
    {
        Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
        for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); itr++)
        {
            Player *member = sObjectMgr.GetPlayer(itr->guid);
            if( !member || !sServerFacade.IsAlive(member))
                continue;

            PlayerbotAI *ai = member->GetPlayerbotAI();
            if (ai)
            {
                if (ai->GetAiObjectContext()->GetValue<Unit*>("rti cc target")->Get() == attacker)
                    return true;

                string rti = ai->GetAiObjectContext()->GetValue<string>("rti cc")->Get();
                int index = RtiTargetValue::GetRtiIndex(rti);
                if (index != -1)
                {
                    uint64 guid = group->GetTargetIcon(index);
                    if (guid && attacker->GetObjectGuid() == ObjectGuid(guid))
                        return true;
                }
            }
        }

    }

    return false;
}

void FindTargetStrategy::GetPlayerCount(Unit* creature, int* tankCount, int* dpsCount)
{
    Player* bot = ai->GetBot();
    if (tankCountCache.find(creature) != tankCountCache.end())
    {
        *tankCount = tankCountCache[creature];
        *dpsCount = dpsCountCache[creature];
        return;
    }

    *tankCount = 0;
    *dpsCount = 0;

    Unit::AttackerSet attackers(creature->getAttackers());
    for (set<Unit*>::const_iterator i = attackers.begin(); i != attackers.end(); i++)
    {
        Unit* attacker = *i;
        if (!attacker || !sServerFacade.IsAlive(attacker) || attacker == bot)
            continue;

        Player* player = dynamic_cast<Player*>(attacker);
        if (!player)
            continue;

        if (ai->IsTank(player))
            (*tankCount)++;
        else
            (*dpsCount)++;
    }

    tankCountCache[creature] = *tankCount;
    dpsCountCache[creature] = *dpsCount;
}

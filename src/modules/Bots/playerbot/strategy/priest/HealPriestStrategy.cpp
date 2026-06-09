#include "botpch.h"
#include "../../playerbot.h"
#include "PriestMultipliers.h"
#include "HealPriestStrategy.h"

using namespace ai;

NextAction** HealPriestStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("shoot", 5.0f), NULL);
}

void HealPriestStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericPriestStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "enemy out of spell",
        NextAction::array(0, new NextAction("reach spell", ACTION_NORMAL + 9), NULL)));

    triggers.push_back(new TriggerNode(
        "critical health",
        NextAction::array(0, new NextAction("power word: shield", ACTION_EMERGENCY + 2), new NextAction("flash heal", ACTION_EMERGENCY + 2), NULL)));

    triggers.push_back(new TriggerNode(
        "party member critical health",
        NextAction::array(0, new NextAction("power word: shield on party", ACTION_EMERGENCY + 1), new NextAction("flash heal on party", ACTION_EMERGENCY + 1), NULL)));

    triggers.push_back(new TriggerNode(
        "brain party member emergency heal",
        NextAction::array(0, new NextAction("power word: shield on party", ACTION_EMERGENCY + 8), new NextAction("flash heal on party", ACTION_EMERGENCY + 7), NULL)));

    triggers.push_back(new TriggerNode(
        "low health",
        NextAction::array(0, new NextAction("flash heal", ACTION_CRITICAL_HEAL + 4), new NextAction("greater heal", ACTION_CRITICAL_HEAL + 3), NULL)));

    triggers.push_back(new TriggerNode(
        "party member low health",
        NextAction::array(0, new NextAction("flash heal on party", ACTION_CRITICAL_HEAL + 3), new NextAction("greater heal on party", ACTION_CRITICAL_HEAL + 2), NULL)));

    triggers.push_back(new TriggerNode(
        "brain party member critical heal",
        NextAction::array(0, new NextAction("flash heal on party", ACTION_CRITICAL_HEAL + 8), new NextAction("greater heal on party", ACTION_CRITICAL_HEAL + 7), NULL)));

    triggers.push_back(new TriggerNode(
        "party member medium health",
        NextAction::array(0, new NextAction("flash heal on party", ACTION_MEDIUM_HEAL + 3), NULL)));

    triggers.push_back(new TriggerNode(
        "brain party member steady heal",
        NextAction::array(0, new NextAction("renew on party", ACTION_MEDIUM_HEAL + 8), new NextAction("flash heal on party", ACTION_MEDIUM_HEAL + 7), NULL)));

	triggers.push_back(new TriggerNode(
		"medium aoe heal",
		NextAction::array(0, new NextAction("circle of healing", 27.0f), NULL)));

    triggers.push_back(new TriggerNode(
        "almost full health",
        NextAction::array(0, new NextAction("renew", 15.0f), NULL)));

    triggers.push_back(new TriggerNode(
        "party member almost full health",
        NextAction::array(0, new NextAction("renew on party", 10.0f), NULL)));

    triggers.push_back(new TriggerNode(
        "enemy is close",
        NextAction::array(0, new NextAction("fade", 50.0f), NULL)));

    triggers.push_back(new TriggerNode(
        "party member to heal out of spell range",
        NextAction::array(0, new NextAction("reach party member to heal", ACTION_CRITICAL_HEAL + 1), NULL)));
}

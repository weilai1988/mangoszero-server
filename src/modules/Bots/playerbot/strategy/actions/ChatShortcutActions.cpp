#include "botpch.h"
#include "../../playerbot.h"
#include "ChatShortcutActions.h"
#include "NonCombatActions.h"
#include "StayActions.h"
#include "../../PlayerbotAIConfig.h"
#include "../values/PositionValue.h"

using namespace ai;

static void PrepareRest(PlayerbotAI* ai)
{
    Event emptyEvent;
    StayAction stay(ai);
    stay.Execute(emptyEvent);
    SitAction sit(ai);
    sit.Execute(emptyEvent);
}

static bool TryDrink(PlayerbotAI* ai)
{
    Player* bot = ai->GetBot();
    if (!bot || bot->GetPowerType() != POWER_MANA || !bot->GetMaxPower(POWER_MANA) ||
        bot->GetPower(POWER_MANA) >= bot->GetMaxPower(POWER_MANA))
        return false;

    Event emptyEvent;
    DrinkAction drink(ai);
    return drink.Execute(emptyEvent);
}

static bool TryEat(PlayerbotAI* ai)
{
    Player* bot = ai->GetBot();
    if (!bot || bot->GetHealth() >= bot->GetMaxHealth())
        return false;

    Event emptyEvent;
    EatAction eat(ai);
    return eat.Execute(emptyEvent);
}

void ReturnPositionResetAction::ResetReturnPosition()
{
    ai::PositionMap& posMap = context->GetValue<ai::PositionMap&>("position")->Get();
    ai::Position pos = posMap["return"];
    pos.Reset();
    posMap["return"] = pos;
}

void ReturnPositionResetAction::SetReturnPosition(float x, float y, float z)
{
    ai::PositionMap& posMap = context->GetValue<ai::PositionMap&>("position")->Get();
    ai::Position pos = posMap["return"];
    pos.Set(x, y, z, ai->GetBot()->GetMapId());
    posMap["return"] = pos;
}

bool FollowChatShortcutAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    ai->Reset();
    ai->ChangeStrategy("+follow,-passive", BOT_STATE_NON_COMBAT);
    ai->ChangeStrategy("-follow,-passive", BOT_STATE_COMBAT);
    ResetReturnPosition();
    if (bot->GetMapId() != master->GetMapId() || bot->GetDistance(master) > sPlayerbotAIConfig.sightDistance)
    {
        ai->TellError("I will not follow you - too far away");
        return true;
    }
    ai->TellMaster("Following");
    return true;
}

bool StayChatShortcutAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    ai->Reset();
    ai->ChangeStrategy("+stay,-passive", BOT_STATE_NON_COMBAT);
    ai->ChangeStrategy("-follow,-passive", BOT_STATE_COMBAT);

    SetReturnPosition(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    ai->TellMaster("Staying");
    return true;
}

bool RestChatShortcutAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    ai->Reset();
    ai->ChangeStrategy("+stay,+food,+sit,-buff,-grind,-rpg,-runaway,-passive", BOT_STATE_NON_COMBAT);

    SetReturnPosition(bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());

    PrepareRest(ai);
    TryDrink(ai);
    TryEat(ai);

    ai->TellMaster("Resting");
    return true;
}

bool DrinkChatShortcutAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    ai->ChangeStrategy("+stay,+food,+sit,-buff,-grind,-rpg,-runaway,-passive", BOT_STATE_NON_COMBAT);

    PrepareRest(ai);
    return TryDrink(ai);
}

bool EatChatShortcutAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    ai->ChangeStrategy("+stay,+food,+sit,-buff,-grind,-rpg,-runaway,-passive", BOT_STATE_NON_COMBAT);

    PrepareRest(ai);
    return TryEat(ai);
}

bool FleeChatShortcutAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    ai->Reset();
    ai->ChangeStrategy("+follow,+passive", BOT_STATE_NON_COMBAT);
    ai->ChangeStrategy("+follow,+passive", BOT_STATE_COMBAT);
    ResetReturnPosition();
    if (bot->GetMapId() != master->GetMapId() || bot->GetDistance(master) > sPlayerbotAIConfig.sightDistance)
    {
        ai->TellError("I will not flee with you - too far away");
        return true;
    }
    ai->TellMaster("Fleeing");
    return true;
}

bool GoawayChatShortcutAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    ai->Reset();
    ai->ChangeStrategy("+runaway", BOT_STATE_NON_COMBAT);
    ai->ChangeStrategy("+runaway", BOT_STATE_COMBAT);
    ResetReturnPosition();
    ai->TellMaster("Running away");
    return true;
}

bool GrindChatShortcutAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    ai->Reset();
    ai->ChangeStrategy("+grind,-passive", BOT_STATE_NON_COMBAT);
    ResetReturnPosition();
    ai->TellMaster("Grinding");
    return true;
}

bool TankAttackChatShortcutAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    if (!ai->IsTank(bot))
        return false;

    ai->Reset();
    ai->ChangeStrategy("-passive", BOT_STATE_NON_COMBAT);
    ai->ChangeStrategy("-passive", BOT_STATE_COMBAT);
    ResetReturnPosition();
    ai->TellMaster("Attacking");
    return true;
}

bool MaxDpsChatShortcutAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    ai->Reset();
    ai->ChangeStrategy("-threat,-conserve mana,-cast time,+dps debuff", BOT_STATE_COMBAT);
    ai->TellMaster("Max DPS");
    return true;
}

bool PartyBuffsChatShortcutAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    ai->ChangeStrategy("+follow,+buff", BOT_STATE_NON_COMBAT);
    ai->TellMaster("Refreshing party buffs");
    return true;
}

bool BrainStatusChatShortcutAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
        return false;

    ai->TellMaster(ai->FormatBrainState());
    return true;
}

#include "botpch.h"
#include "../../playerbot.h"
#include "LootRollAction.h"

#include "../values/ItemUsageValue.h"

using namespace ai;

bool LootRollAction::Execute(Event event)
{
    Player *bot = QueryItemUsageAction::ai->GetBot();

    WorldPacket p(event.getPacket());
    ObjectGuid lootTargetGuid;
    uint32 slot;
    uint8 rollType;

    if (p.size() < 13)
        return false;

    p.rpos(0);
    p >> lootTargetGuid;
    p >> slot;
    p >> rollType;

    Group* group = bot->GetGroup();
    if(!group)
        return false;

#ifdef MANGOS
    Roll const* roll = group->GetRollForLoot(lootTargetGuid, slot);
    if (!roll)
        return false;

    RollVote vote = ROLL_PASS;
    ItemPrototype const *proto = sItemStorage.LookupEntry<ItemPrototype>(roll->itemid);
    if (proto)
        vote = CalculateRollVote(proto);

    switch (group->GetLootMethod())
    {
    case MASTER_LOOT:
    case FREE_FOR_ALL:
        group->CountRollVote(bot, lootTargetGuid, slot, ROLL_PASS);
        break;
    default:
        group->CountRollVote(bot, lootTargetGuid, slot, vote);
        break;
    }
#endif

#ifdef CMANGOS
    Loot* loot = sLootMgr.GetLoot(bot, lootTargetGuid);
    if (!loot)
        return false;

    LootItem* item = loot->GetLootItemInSlot(slot);
    ItemPrototype const *proto = sItemStorage.LookupEntry<ItemPrototype>(item->itemId);
    if (!proto)
        return false;

    RollVote vote = CalculateRollVote(proto);

    GroupLootRoll* lootRoll = loot->GetRollForSlot(slot);
    if (!lootRoll)
        return false;

    lootRoll->PlayerVote(bot, vote);
#endif

    return true;
}

RollVote LootRollAction::CalculateRollVote(ItemPrototype const *proto)
{
    ostringstream out; out << proto->ItemId;
    ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", out.str());

    RollVote needVote = ROLL_GREED;
    switch (usage)
    {
    case ITEM_USAGE_EQUIP:
    case ITEM_USAGE_REPLACE:
    case ITEM_USAGE_GUILD_TASK:
        needVote = ROLL_NEED;
        break;
    case ITEM_USAGE_SKILL:
    case ITEM_USAGE_USE:
    case ITEM_USAGE_DISENCHANT:
        needVote = ROLL_GREED;
        break;
    }

    return StoreLootAction::IsLootAllowed(proto->ItemId, bot->GetPlayerbotAI()) ? needVote : ROLL_PASS;
}

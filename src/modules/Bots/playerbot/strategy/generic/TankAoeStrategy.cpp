#include "botpch.h"
#include "../../playerbot.h"
#include "TankAoeStrategy.h"

using namespace ai;

void TankAoeStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "tank aoe",
        NextAction::array(0,
            new NextAction("tank assist", ACTION_EMERGENCY + 5),
            new NextAction("reach melee", ACTION_EMERGENCY + 4),
            NULL)));
}

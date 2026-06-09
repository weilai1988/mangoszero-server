#pragma once

#include "../Action.h"

namespace ai
{
    class InviteToGroupAction : public Action
    {
    public:
        InviteToGroupAction(PlayerbotAI* ai) : Action(ai, "invite") {}

        virtual bool Execute(Event event)
        {
            Player* master = event.getOwner();
            if (!master)
                return false;

            WorldPacket p;
            p << bot->GetName();
            master->GetSession()->HandleGroupInviteOpcode(p);

            return true;
        }
    };

}

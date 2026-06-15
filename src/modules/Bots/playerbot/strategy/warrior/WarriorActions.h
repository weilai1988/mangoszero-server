#pragma once
#include "../actions/GenericActions.h"
#include "../actions/ChooseTargetActions.h"

namespace ai
{
    // all
    class CastHeroicStrikeAction : public CastMeleeSpellAction {
    public:
        CastHeroicStrikeAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "heroic strike") {}
    };

    // all
    class CastCleaveAction : public CastMeleeSpellAction {
    public:
        CastCleaveAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "cleave") {}
        virtual ActionThreatType getThreatType() { return ACTION_THREAT_AOE; }
    };

    // battle, berserker
    class CastMockingBlowAction : public CastMeleeSpellAction {
    public:
        CastMockingBlowAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "mocking blow") {}
    };

    class CastBloodthirstAction : public CastMeleeSpellAction {
    public:
        CastBloodthirstAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "bloodthirst") {}
    };

    // battle, berserker
    class CastExecuteAction : public CastMeleeSpellAction {
    public:
        CastExecuteAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "execute") {}
    };

    // battle
    class CastOverpowerAction : public CastMeleeSpellAction {
    public:
        CastOverpowerAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "overpower") {}
    };

    // battle, berserker
    class CastHamstringAction : public CastSnareSpellAction {
    public:
        CastHamstringAction(PlayerbotAI* ai) : CastSnareSpellAction(ai, "hamstring") {}
    };

    // defensive
    class CastTauntAction : public CastSpellAction {
    public:
        CastTauntAction(PlayerbotAI* ai) : CastSpellAction(ai, "taunt") {}
        virtual bool isUseful() { return CastSpellAction::isUseful() && !AI_VALUE2(bool, "has aggro", "current target"); }
    };

    // defensive
    class CastShieldBlockAction : public CastBuffSpellAction {
    public:
        CastShieldBlockAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "shield block") {}
    };

    // defensive
    class CastShieldWallAction : public CastMeleeSpellAction {
    public:
        CastShieldWallAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "shield wall") {}
    };

    class CastBloodrageAction : public CastBuffSpellAction {
    public:
        CastBloodrageAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "bloodrage") {}
    };

    // defensive
    class CastDevastateAction : public CastMeleeSpellAction {
    public:
        CastDevastateAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "devastate") {}
    };

    // all
    class CastSlamAction : public CastMeleeSpellAction {
    public:
        CastSlamAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "slam") {}
    };

	// all
	class CastShieldSlamAction : public CastMeleeSpellAction {
	public:
		CastShieldSlamAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "shield slam") {}
	};

    // after dodge
    BEGIN_MELEE_SPELL_ACTION(CastRevengeAction, "revenge")
    END_SPELL_ACTION()


    //debuffs
    BEGIN_DEBUFF_ACTION(CastRendAction, "rend")
    END_SPELL_ACTION()

    class CastRendOnAttackerAction : public CastDebuffSpellOnAttackerAction
    {
    public:
        CastRendOnAttackerAction(PlayerbotAI* ai) : CastDebuffSpellOnAttackerAction(ai, "rend") {}
    };

    BEGIN_DEBUFF_ACTION(CastDisarmAction, "disarm")
    END_SPELL_ACTION()

    class CastSunderArmorAction : public CastDebuffSpellAction
    {
    public:
        CastSunderArmorAction(PlayerbotAI* ai) : CastDebuffSpellAction(ai, "sunder armor") {
            range = ATTACK_DISTANCE;
        }
        virtual bool isUseful() { return CastSpellAction::isUseful(); }
    };

    class CastDemoralizingShoutAction : public CastDebuffSpellAction {
    public:
        CastDemoralizingShoutAction(PlayerbotAI* ai) : CastDebuffSpellAction(ai, "demoralizing shout") {
            range = ATTACK_DISTANCE;
        }
        virtual ActionThreatType getThreatType() { return ACTION_THREAT_AOE; }
    };

    class CastChallengingShoutAction : public CastMeleeSpellAction {
    public:
        CastChallengingShoutAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "challenging shout") {}
        virtual ActionThreatType getThreatType() { return ACTION_THREAT_AOE; }
    };

    // stuns
    BEGIN_MELEE_SPELL_ACTION(CastShieldBashAction, "shield bash")
    END_SPELL_ACTION()

    BEGIN_MELEE_SPELL_ACTION(CastIntimidatingShoutAction, "intimidating shout")
    END_SPELL_ACTION()

    class CastThunderClapAction : public CastMeleeSpellAction {
    public:
        CastThunderClapAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "thunder clap") {}
        virtual ActionThreatType getThreatType() { return ACTION_THREAT_AOE; }
    };

    // buffs
	class CastBattleShoutAction : public CastBuffSpellAction {
	public:
		CastBattleShoutAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "battle shout") {
		    range = ATTACK_DISTANCE;
		}
		virtual bool isUseful() { return CastSpellAction::isUseful(); }
	};

	class CastDefensiveStanceAction : public CastBuffSpellAction {
	public:
		CastDefensiveStanceAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "defensive stance") {}
	};

	class CastBattleStanceAction : public CastBuffSpellAction {
	public:
		CastBattleStanceAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "battle stance") {}
	};

    BEGIN_RANGED_SPELL_ACTION(CastChargeAction, "charge")
    END_SPELL_ACTION()

	class CastDeathWishAction : public CastBuffSpellAction {
	public:
		CastDeathWishAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "death wish") {}
	};

	class CastBerserkerRageAction : public CastBuffSpellAction {
	public:
		CastBerserkerRageAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "berserker rage") {}
	};

	class CastLastStandAction : public CastBuffSpellAction {
	public:
		CastLastStandAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "last stand") {}
	};

	// defensive
	class CastShockwaveAction : public CastMeleeSpellAction {
	public:
		CastShockwaveAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "shockwave") {}
	};

	// defensive
	class CastConcussionBlowAction : public CastSnareSpellAction {
	public:
		CastConcussionBlowAction(PlayerbotAI* ai) : CastSnareSpellAction(ai, "concussion blow") {}
	};

	BEGIN_MELEE_SPELL_ACTION(CastVictoryRushAction, "victory rush")
	END_SPELL_ACTION()

    class CastShieldBashOnEnemyHealerAction : public CastSpellOnEnemyHealerAction
    {
    public:
        CastShieldBashOnEnemyHealerAction(PlayerbotAI* ai) : CastSpellOnEnemyHealerAction(ai, "shield bash") {}
    };

    class CastBattleShoutTauntAction : public CastMeleeSpellAction
    {
    public:
        CastBattleShoutTauntAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "battle shout") {}
        virtual bool isUseful() { return CastSpellAction::isUseful(); }
        virtual ActionThreatType getThreatType() { return ACTION_THREAT_AOE; }
    };

    class WarriorTankThreatAction : public AttackAction
    {
    public:
        WarriorTankThreatAction(PlayerbotAI* ai) : AttackAction(ai, "tank threat") {}
        virtual string GetTargetName() { return "tank target"; }
        virtual NextAction** getContinuers()
        {
            return NextAction::array(0,
                new NextAction("taunt", ACTION_EMERGENCY + 7),
                new NextAction("revenge", ACTION_HIGH + 9),
                new NextAction("shield slam", ACTION_HIGH + 8),
                new NextAction("sunder armor", ACTION_HIGH + 7),
                new NextAction("heroic strike", ACTION_HIGH + 6),
                new NextAction("thunder clap", ACTION_HIGH + 5),
                new NextAction("demoralizing shout", ACTION_HIGH + 4),
                new NextAction("cleave", ACTION_HIGH + 3),
                new NextAction("battle shout taunt", ACTION_HIGH + 2),
                NULL);
        }
    };
}

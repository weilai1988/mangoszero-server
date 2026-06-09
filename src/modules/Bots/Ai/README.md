# Playerbot AI Layout

This tree is the MaNGOS Zero version of the cleaner mod-playerbots layout.
The current legacy implementation still lives under `playerbot/strategy`;
new and migrated behavior should move here in small compile-tested slices.

## Layers

- `Base`: shared actions, triggers, values, strategy helpers, and utility code.
- `Class`: class-specific brains and spell priorities for Druid, Hunter, Mage,
  Paladin, Priest, Rogue, Shaman, Warlock, and Warrior.
- `Dungeon`: Vanilla dungeon strategies. Add one dungeon folder at a time.
- `Raid`: Vanilla raid strategies. Molten Core, Blackwing Lair, and AQ20 are the
  first targets because mod-playerbots already has useful reference structure.
- `World`: open-world/rpg behavior for bots that should live like players.

The long-term target is individual bot thinking: each bot chooses its own next
decision from its class, role, equipment, threat, health, mana, nearby enemies,
and encounter context, while group commands only bias or coordinate those
decisions.

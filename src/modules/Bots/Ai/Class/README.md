# Class AI

Each class should own its combat priorities instead of depending on one giant
generic action pile. The target shape for each class is:

- `Action`: class spells and class-only commands.
- `Strategy`: role/spec priorities such as tank, healer, ranged dps, melee dps,
  and non-combat preparation.
- `Trigger`: class-specific conditions such as missing buffs, low rage, pet
  danger, or dispel opportunities.

The current legacy class files are still under `playerbot/strategy/<class>`.
Move one class at a time and compile after each class.

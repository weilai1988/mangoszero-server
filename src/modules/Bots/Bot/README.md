# Bot Runtime Layout

This folder is reserved for the runtime-facing playerbot pieces from the
mod-playerbots style layout:

- `Cmd`: chat and command handling.
- `Debug`: profiling and diagnostics.
- `Engine`: action, trigger, strategy, and value execution.
- `Factory`: bot and AI construction.

The current runtime implementation still lives under `playerbot/`. Move files
here only when the include paths and CMake build are ready for that slice.

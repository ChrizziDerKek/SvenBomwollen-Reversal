# Sven Zwo rebuild

Build with Visual Studio v143 C++ tools and a Windows SDK, targeting x86/Win32:

```powershell
powershell -File rebuild/Build.ps1
powershell -File rebuild/Run-Sven2.ps1
```

FSR1 remains the default. F6 cycles filters, F7/F8 adjust sharpening and F11
toggles fullscreen. `-Upscaler native` selects original presentation. Loose
content loads from `assets`; supported original DLLs load from `games/sven2`.

## Ownership and behavior

`LevelScene` coordinates the grid, resources and phases. `ObjectManager` owns
individual objects through `std::unique_ptr<BaseObject>`. Each object owns its
`WorldObject` runtime state; serialized `LevelObjectRecord` data stays separate.

`BaseCharacter` derives from `BaseObject`. PlayerCharacter, EnemyCharacter and
AnimalCharacter own their behavior. Alien, Dog, Farmer, Girl, Sheep, Chicken,
Goat and Pig are concrete types. Shared state machines are methods on those
instances, not controllers that iterate over an entire population. Helpers
operate on the instance's own stable index/state; Sheep selects its specialized
update. One-player combat coordination belongs to the manager.

Scenery, items and world effects share manager ownership and phase scheduling.
Item spawning, player powerup state, terrain collision and particle batches
remain focused services rather than being duplicated into every instance.

* `ObjectCatalog` centralizes character IDs, classifications, standing assets,
  occupancy codes and concrete factories, without a separate count constant.
  Registered new characters are accepted by the loader without another
  `LevelScene` switch case. An animal can override `GetInteraction` to supply
  its own sequence; absent animation bindings fail safely rather than indexing
  an empty sprite/action slot.
* `ObjectKind`/`PlayerActionState` name native IDs. Sprite/rule/action caches
  use keyed containers instead of fixed kind-index arrays.
* `GetObjects().Get(Index)` accesses an instance; `Find(ObjectKind::Goat)`
  searches live objects. `CanOccupy`, `PlayerInRange` and `CanCatchPlayer`
  provide shared collision/interaction queries.
* Collection growth preserves instance addresses. Freed slots are explicitly
  replaced with the correct concrete type; live objects/player cannot be replaced.
* Rendering sorts state snapshots without rearranging manager slots. The grid
  retains original terrain/reservation semantics.

Scheduling remains effects, player-cell marking, items, enemies, small animals,
sheep, UFO, player, with the same substeps and random-call order.

## Audio and native contracts

Gameplay emits `SoundRequest` values with explicit OneShot/Loop playback. The
UFO uses `SfxUFO` resource metadata; playback does not special-case its filename.
Sample caches separate playback modes, preventing a loop-configured sample from
contaminating one-shot playback. Music releases native stream objects as before.

`LegacyAudioAbi.h` names verified native slots/flags. These and the v6 layout
remain fixed contracts of the supported original build. Original animation
tables, AI defaults and family-specific state transitions retain their behavior.
This refactor adds neither XXX/004 support nor the removed horse experiment.

## Style and verification

Use PascalCase for classes, methods and attributes, camelCase for parameters and local variables, and `Types.h` aliases. `.clang-format` records Allman/tab layout;
[AGENTS.md](AGENTS.md) records the editing conventions. Explicit short types are
preferred; `auto` remains for long types, structured bindings and lambdas.
Required SDK/library spellings and language hooks are exceptions.

```powershell
powershell -File rebuild/Build.ps1 -Checks
rebuild/bin/Win32/Debug/Sven2Checks.exe
rebuild/bin/Win32/Debug/Sven2Checks.exe --visual
rebuild/bin/Win32/Debug/Sven2Checks.exe --upscaler
python rebuild/tools/VerifyRefactor.py
```

The suite covers original gameplay/UI and simulation of all 105 Zwo levels.
Ownership checks verify concrete instances, stable references, typed lookup
and slot replacement. Native visual/audio/FSR checks require a desktop session.
Generated captures and logs stay in `runtime`.

The unchanged working-copy check binary supplied the migration baseline:
105 maps × 2 seeds × 480 frames = **100,800 frame fingerprints**, including
render commands/pixels, progression and sound events. The new trace formatter
retains the legacy UFO event's root-directory casing; actual playback uses the
XML path. Matching traces are regression evidence, not exhaustive proof of all
possible interactions. `refactor-validation.json` records the trace comparison
and protected-backup source hash check.

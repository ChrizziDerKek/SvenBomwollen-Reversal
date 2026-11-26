# Sven Zwo rebuild

The in-game editor supports objects, free scenery, terrain/teleports, random
spawners, draw order, global themes and named multi-level folder projects.
**F3** on the title screen starts a blank project; in a level it edits the map.
Use **T** to select cells beneath scenery, **V** for objects, drag the sidebar
header to move it, and use its Project tab for blank/import/duplicate/open flows.
**B** opens the thumbnail placement sidebar (scenery/actors/items/tiles/properties); **O** lists
already placed objects. The I menu is the sidebar Props tab. Special objects are under Actors; tile/spawner brushes
have their own tab. The second toolbar row has been replaced by this sidebar.
**H** hides the interface/playtest HUD,
and **A** changes the scenery placement anchor (visible center or canvas origin).
Press **I** for properties, **Ctrl+S** to save a separate folder pack, and **F5** to
playtest without changing the running game or stored progress.
The editor loops the supplied `XXX/SvenXXX/sfx/editor.wav`; playtest uses level
music and closing the editor restores the previous track. Its loudness now
matches the title theme, with the original quiet WAV backed up under runtime.
See [editor controls and scope](EDITOR.md) and the persistent
[editor progress](research/editor/PROGRESS.md).

The supplied Zwo/XXX features have passed automated checks and user acceptance.
The persistent [XXX roadmap and validation status](research/xxx/PROGRESS.md)
retains the implementation history and native evidence.
Source data uses `assets/ZWO` and `assets/XXX`. Both catalogs are discovered together,
with a shared XXX runtime and a common XXX menu/HUD/font theme for both
editions. For Zwo alone with its original presentation, pass
`-DataDirectory assets/ZWO` to `Run-Sven2.ps1`. Builds copy the existing asset layout
to the executable directory without moving or changing the sources.

The shared card browser has 39 animations: all 33 Zwo poses plus six new XXX poses. In Help → Steuerung,
click a character or key label, then press a key; Escape cancels. Overrides are saved
beside the EXE in `saves/controls.tsv`. XXX uses arrows/right Control by default.
The combined installation uses these bindings for both editions and supports
local two-player games, including generated second-player starts for Zwo levels.
The Zwo-only fallback keeps its previous controls until player 1 is customized.

Build with Visual Studio v143 C++ tools and a Windows SDK, targeting x86/Win32:

```powershell
powershell -File rebuild/Build.ps1
powershell -File rebuild/Run-Sven2.ps1
```

FSR1 remains the default. F6 cycles filters, F7/F8 adjust sharpening and F11
toggles fullscreen. `-Upscaler native` selects original presentation.

Each game build deploys a portable folder in `rebuild/bin/Win32/Debug` (or
`Release`). Launch `Sven2Bootstrap.exe` directly or copy that entire folder
elsewhere; the working directory does not matter. The folder contains the EXE,
the original runtime DLLs, `assets/`, `saves/` for highscores and `logs/`.
The three FSR source files are embedded in the EXE, including its header
includes; no loose shader folder is needed. Shaders still compile locally using
D3DCompiler when the renderer starts.

Source assets remain in the repository for editing. Builds populate missing
assets and update DLLs without overwriting edited runtime assets or deleting
additional files in the destination. Use `Build.ps1 -RefreshAssets` explicitly
to replace runtime copies with the source assets. Existing saves
from `rebuild/runtime/sven2/saves` are copied only when the destination save does
not exist. Build deployment leaves source data and the backup unchanged.
`--data-dir` and `--game-dir` remain explicit overrides for development.

Press **F2** to open the debug/cheat menu. Gameplay and its timer pause while it
is open; **F2** or **Escape** closes it. Use the mouse or Up/Down and Enter;
Left/Right cycle item and animal choices. Commands skip/restart a level, add or
remove a life, and spawn any registered item or animal on a free nearby tile.
Spawning animals adds completion targets and respects occupied tiles and the
native object-pool limit. The final level skip opens the pack-completion screen.

An intentional change from Zwo: the stinky mushroom and flower now set nearby
chickens, goats and pigs' impatience, too. They still never kick Sven or turn
into demons. Sheep retain their existing sounds and special behavior.

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
XXX data/resources are being integrated into these same classes. Full XXX gameplay,
004 support and the removed horse experiment are not implemented by this milestone.

## Style and verification

Use PascalCase for classes, methods and attributes, camelCase for parameters and local variables, and `Types.h` aliases. `.clang-format` records Allman/tab layout;
[AGENTS.md](AGENTS.md) records the editing conventions. Explicit short types are
preferred; `auto` remains for long types, structured bindings and lambdas.
Required SDK/library spellings and language hooks are exceptions.

Keep ordinary expressions and calls on a single line, with initializer spacing
such as `{ 1, 1 }`. Parenthesize nested conditional branches to keep clang-format
from wrapping ternary chains. Data tables retain their row layout.

`ObjectKind` is used directly. `ObjectCatalog` maps character categories, item
sprites/frames, lifetimes, pickup effects, distraction codes and native table
indices explicitly; callers use helpers such as `IsAnimal`, `IsEnemy`, `IsItem`
and `IsPowerup` instead of interpreting kind-number ranges.
`ActorState`, `UfoState`, `OccupancyCode`, `ItemCode` and `SpawnerCode` keep
runtime domains separate. Native numeric IDs are converted when reading level
records. Actor-state aliases document IDs reused by different native classes.

```powershell
powershell -File rebuild/Build.ps1 -Checks
rebuild/bin/Win32/Debug/Sven2Checks.exe
rebuild/bin/Win32/Debug/Sven2Checks.exe --visual
rebuild/bin/Win32/Debug/Sven2Checks.exe --editions
rebuild/bin/Win32/Debug/Sven2Checks.exe --upscaler
powershell -File rebuild/Test-PortableBuild.ps1
```

The suite covers original gameplay/UI and simulation of all 105 Zwo levels.
`--editions` checks the combined catalog, v6/v7 loading, single-player actor
filtering and XXX resource/UI initialization for all 234 shipped maps; it does
not certify full XXX gameplay parity.
Ownership checks verify concrete instances, stable references, typed lookup
and slot replacement. Native visual/audio/FSR checks require a desktop session.
Generated captures and logs stay in `runtime`.

The unchanged working-copy check binary supplied the migration baseline:
105 maps × 2 seeds × 480 frames = **100,800 frame fingerprints**, including
render commands/pixels, progression and sound events. The new trace formatter
retains the legacy UFO event's root-directory casing; actual playback uses the
XML path. Matching traces are regression evidence, not exhaustive proof of all
possible interactions. `refactor-validation.json` records the trace comparison
and protected-backup source hash check from that migration. The archival
`tools/VerifyRefactor.py` check requires the old working-copy backup and retained
baseline/current traces; use the native suite above after clearing those files.
The animal mood change is an intentional deviation from the original baseline.


### XXX bonus rounds and local multiplayer

The combined runtime supports the shipped Lars, Broemse and alien bonus controllers. Finishing Frigida on the first uninterrupted attempt unlocks the following bonus; locked bonus maps are skipped. Wine/teddy distractions cost time, artifacts end bonus play, and bonus rounds do not award a life bonus.

Choose **2 Spieler** and a Zwo or XXX episode for local multiplayer in the combined runtime. Both sets of bindings can be checked/changed in **Steuerung**. Players share score, lives and time, with individual actions/items, contribution rows and a separate multiplayer highscore file. Multiplayer bonus rounds use two aliens. Zwo-authored episodes generate a second Sven on a nearby free, reachable tile in two-player mode, leaving the level files unchanged. **1 Spieler** retains the original single spawn; archival Zwo-only data remains single-player.

Implementation/evidence/status: [XXX roadmap](research/xxx/PROGRESS.md) and [bonus/multiplayer notes](research/xxx/modes/BONUS_AND_MULTIPLAYER.md). This does not claim that all remaining XXX features or original-runtime parity work are complete.

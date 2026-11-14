# Sven Zwo remake

Open [SvenRebuild.sln](SvenRebuild.sln) in Visual Studio 2022 with the C++ desktop workload, v143, and a Windows SDK. Build **x86** in Debug or Release (the C++ project uses Win32).

From the repository root:

```powershell
powershell -File rebuild/Build.ps1
powershell -File rebuild/Run-Sven2.ps1
```

The remake starts at the original title menu. Start opens a single episode list, with entries such as `XS/Einleitung` and `XXL/Wald`. Hover to highlight a row green, then click to start, or use Up/Down and Enter. Use the list arrows to scroll. Left/Right selects the bottom menu buttons on the title screen. Arrow keys or WASD move Sven. Press Space repeatedly to interact with the animal in front of him. Escape pauses the round and displays the leave-game prompt; No or Escape resumes.

Hilfe contains the four original text tabs. On its character tab, Up/Down or the arrow buttons selects the character. Highscore shows the selected episode's local table. Click its arrows or use Left/Right or Up/Down to traverse episodes across packages. A qualifying episode score opens name entry and saves after Enter.

## Folder game data

The default data root is the existing repository `assets` directory. The remake reads the extracted XML, levels, images, and sounds directly. Its native sound plugin receives absolute paths to those loose files.

```text
assets/
  Sven2/
    Sven2.xml
    StringTable.xml
    gfx/
    sfx/
  XS/
    XS.xml
    levels/
    gfx_download/
    sfx/
  XL/
  XXL/
  01/
  02/
```

Each immediate child package folder has a descriptor named after the folder. Descriptors retain their original package-relative asset paths. Add an extracted package folder and restart to discover it. The supplied data contains five packages and 105 v6 levels.

Use another data root with:

```powershell
powershell -File rebuild/Run-Sven2.ps1 -DataDirectory E:/MySvenData
```

The original DLLs are loaded from `games/sven2`. Use `--game-dir PATH` when launching the executable to choose another directory containing the supported original DLLs. Game content comes from `--data-dir PATH`. These are independent locations.

The original core XML declares UTF-8 while containing Windows-1252 characters. The loader converts that legacy encoding in memory. The bitmap renderer uses the original glyph mapping and width tables, including umlauts.

## Code and verification

- `GameData` discovers folder packages and reads localization, parameters and named sound mappings.
- `LevelScene` loads and simulates level objects.
- `GameSession` tracks the timer, score, lives, bonuses, and level transitions.
- `GameUi` renders menus, Help, results, and name entry.
- `BitmapFont` draws the original text.
- `Highscores` persists local tables through an atomic file replacement.
- `InteractionAnimations` shares the original sequences between gameplay and the card browser.
- `EngineAbi` connects the original video and audio DLLs.
- `GameApp` owns the window input and main loop.

Runtime checks have their own executable under `tests`. The game entry point contains startup and command-line handling. Frame captures are requested explicitly by the visual checks.

```powershell
powershell -File rebuild/Test-Bootstrap.ps1
powershell -File rebuild/Test-Bootstrap.ps1 -Configuration Release

# Native UI capture checks
rebuild/bin/Win32/Debug/Sven2Checks.exe --visual

# Start directly in the first XS tutorial
powershell -File rebuild/Run-Sven2.ps1 -Tutorial

# Timed native smoke run
powershell -File rebuild/Run-Sven2.ps1 -Tutorial -Seconds 3
```

The checks cover catalog selection, original text, highscore persistence, collision, sheep interactions, and completing the first tutorial round with its time/sheep/lives bonuses. Native UI captures are written under `runtime/checks`. Runtime logs and local score tables are under `runtime/sven2`. Score tables use UTF-8 TSV files per package/levelset.

## Original behavior and remaining parity work

The highscore screen includes the card browser. Use the arrows or Left/Right to browse, then click a card or press Enter to animate it with sound. Availability follows the original `ShagAnims` configuration; all 33 cards are available with the supplied full-game data. There is no gameplay unlock requirement. Playback uses the original browser's 15 fps clock and cannot restart while playing.

Actor handling covers original initial types 0 through 13, item spawners and pickups, enemy pursuit, animal interactions, UFO timeout collection, completion effects, and floating time adjustments. Every original level is simulated for 30 seconds, including a timeout phase. This checks loading, frame bounds and sound references rather than every possible interaction.

Demon transformation, pursuit, shock and recovery are reconstructed and tested; see [DEMON_SHEEP_PARITY.md](docs/DEMON_SHEEP_PARITY.md). Known remaining comparisons include recovery timing, original global random-number sequencing across levels, and ordering when dynamically created objects share a depth. These need further reconstruction and original-game comparisons before claiming exact behavior.

The enemy audit corrected notice sounds, ordinary versus blocked idle delays, proximity wake timing, Lars's activity-ended grumble and sleep sounds, alien idle animations, and Broemse's dog/UFO state chains. The interaction follow-up adds dig/Zzz effects, corrected per-family mirroring, complete combined sprites, beam recovery and cancelled-step cleanup. Tests cover strict detection boundaries, enemy distractions and artifacts, all eight enemy directions, timed powerups, gifts, animal meters and interactions. Exact original frame scheduling still needs comparison. See [INTERACTION_PARITY.md](docs/INTERACTION_PARITY.md), [RESULTS_UI_PARITY.md](docs/RESULTS_UI_PARITY.md), and the earlier [enemy audit](docs/ENEMY_PARITY_AUDIT.md) for source evidence and limits.

The game logic is being reconstructed from the named IDA dump and the saved reverse-engineering evidence. Original functions include `0x4105F0` for the round loop, `0x4296D0` for player input and interactions, `0x407750` for sheep, `0x412740` for bonus conversion, and `0x4171A0` for highscores.

The timer and score are distinct values. Interaction penalties can change time. Successful actions and resolved animals award score. End-of-round bonuses convert remaining seconds at one point each, completed sheep at ten points each, and lives at twenty-five points each.

Life changes include the original flying-loss and falling-gain animations. The logical count and displayed icon count are separate. The original insurance policy can refund the first attack at unchanged zero score and closely spaced attacks; refunds still animate. Campaign score and the previous attack score now survive level changes correctly.

For focused manual tests, run `powershell -File rebuild/Run-ParityLab.ps1`. This prepares an isolated resource overlay and opens 31 authored lab levels, including corner and attack scenarios, each enemy distraction/artifact path, all player pickups, Frigida, angry animals, demon sheep, sleep/wake, teleports, and a two-level Lives campaign. See [PARITY_LAB.md](docs/PARITY_LAB.md). The current prioritized remaining-work list is [PARITY_CHECKLIST.md](docs/PARITY_CHECKLIST.md).

A complete remake still requires comparison against the original runtime for every actor state, animation, transition, and UI detail. The native audio calls are checked for successful playback requests. Audio output has not been recorded for a waveform comparison. Local scores use the remake's persistence format. Original signed highscore XML import and online submission remain outside the current implementation.

The original DirectInput3/7 initialization fails on this Windows host. Normal input uses Win32. `--native-input` remains a diagnostic opt-in. The video device presents an 800 by 600 window with a 32-bit surface. The original requested 16-bit fullscreen.

See [ENGINE_ABI.md](docs/ENGINE_ABI.md) for the dependency interface and [validation.json](validation.json) for the recorded checks.

See [REGRESSION_FIXES.md](docs/REGRESSION_FIXES.md) for the nine playtest corrections, their original-code evidence and regression coverage.

## Code organization

The OOP cleanup separates character behavior, level data/collision/items/effects, rendering/resources and Win32 input into focused classes. See [ARCHITECTURE.md](docs/ARCHITECTURE.md) for ownership, update order and deterministic refactor checks. The protected `rebuild_working_copy` backup is never modified by the build or cleanup.

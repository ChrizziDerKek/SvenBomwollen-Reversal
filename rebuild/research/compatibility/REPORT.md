# Zwo → XXX → 004 compatibility audit

Analysis date: 2026-11-15. Scope: the original files currently supplied under
`games/sven2`, `games/svenxxx` and `games/sven004`, plus the three live IDA
databases on ports 13337–13339. This is an implementation assessment, not a
claim that the later games have already been ported.

**XXX is a manageable extension of our Zwo implementation. 004 needs a new
versioned world/level layer and several substantial gameplay systems.** Both
can share the renderer, resource infrastructure and character framework, but
must retain their own data interpretation and gameplay rules.

## 1. What was covered

Every supplied PAK and DAT was decoded in memory; every physical `.lvl` was
parsed, its active object types and grid codes counted, and descriptor links
checked. All archive members have SHA-256 entries in [inventory.json](inventory.json).
No original archives or gameplay source files were modified.

| Game | PAKs | Core DATs | Levelsets | Physical levels | Level version |
|---|---:|---:|---:|---:|---:|
| Zwo | 5 | 1 | 11 | 105 | 6 |
| XXX | 5 | 1 | 12 | 129 | 7 |
| 004 | 5 | 1 | 27 | 202 | 12 |
| Total | 15 | 3 | 50 | 436 | |

There are **zero missing map references and zero unreferenced physical levels**.
This includes the Christmas, Easter and ruins releases, not just the XS packs.
The reproducible [pack matrix](pack-matrix.csv) lists each archive's levelsets,
object kinds, terrain codes, item codes and marker bytes.
[Object counts](object-kinds.csv) cover every observed active kind, including
unresolved kinds; nothing unfamiliar was dropped from the inventory.

| Game | Archive | Levels | Levelsets |
|---|---|---:|---|
| Zwo | XS.pak | 25 | Einleitung; Wiese 01; Wiese 02 |
| Zwo | XL.pak | 20 | Bauernhof; Schrottplatz |
| Zwo | XXL.pak | 20 | Wald; Kornfeld |
| Zwo | 01.pak | 20 | Bauernhof 01; Schrottplatz 01 |
| Zwo | 02.pak | 20 | Tulpenfeld; Inseln |
| XXX | 01_XS.pak | 30 | Training; Garten 01; Garten 02 |
| XXX | 02_XL.pak | 22 | Furni; Hühnerfarm |
| XXX | 03_XXL.pak | 33 | Schnee; Höhle; Planet |
| XXX | 04_Xmas2003.pak | 22 | Xmas 2003 01/02 |
| XXX | 05_Easter2004.pak | 22 | Ostern 2004 01/02 |
| 004 | 01_xs.pak | 34 | Training; Team-Training; Sven/Sven Team/Brömse Kino |
| 004 | 02_xl.pak | 24 | Sven/Sven Team/Wötan Fabrik |
| 004 | 03_xxl.pak | 80 | Sven/Lars/Alien Team Bauernhof; Sven/Sven Team Villa; Sven/Sven Team Norwegen; Sven/Sven Team/Alien/Alien Team UFO |
| 004 | 04_Xmas2004.pak | 32 | Sven/Sven Team XMas 01/02 |
| 004 | 05_ruins.pak | 32 | Sven/Sven Team Ruinen 01/02 |

Names with accents above are human-readable labels; some original descriptors
contain replacement characters. Exact source attribute strings are retained in
the inventory rather than silently rewritten.

## 2. Formats: why 004 cannot use the Zwo reader

Offsets below are relative to `B = 16 + nameByteLength`; integers are little endian.

| Section | Zwo v6 | XXX v7 | 004 v12 |
|---|---|---|---|
| Grid | 22×17 padded; 20×15 interior | Same | 62×47 padded; 60×45 interior |
| Cell size | 24 bytes | 24 bytes | 24 bytes, **different packing** |
| Grid bytes | 8,976 | 8,976 | 69,936 |
| Legacy teleport section | B+8976, 68 bytes | Same | B+69936, 68 bytes |
| Timer float | B+9044 | B+9044 | B+70004 |
| Object table | B+9048 | B+9048 | B+70008 |
| Object capacity × stride | 256×100 | 256×100 | 2048×112 |
| Help metadata | None | B+34648, 4096 bytes | B+299384, 4096 bytes |
| Further suffix | None | None | B+303480, 1456 bytes |
| Exact observed size | B+34648 | B+38744 | B+304936 |

All 202 supplied 004 levels use this fixed larger allocation. It is evidence for
a 60×45 interior, **not evidence of arbitrary file dimensions**. The editor can
offer flexible views, but should enforce each original format's capabilities.

Important field changes:

* Object kind remains int32 at record +44. In 004, dimensions are unsigned
  16-bit fields at +22 and +24, confirmed by creation/rendering code. Treating
  +20/+24 as Zwo-style full words produces absurd dimensions and loses flags.
* 004 has additional per-object storage at +100…+111. Initialization uses it
  differently for different families; it is not a universal set of three floats.
* Rendered position at +0/+4 and world position at +8/+12 must remain separate.
  004's scrolling means they can differ dramatically in saved records.
* XXX reads spawner markers from the low word of cell +20; the upper word can
  carry help metadata. 004's native spawner scan reads **byte +18**, while its
  item-slot links occupy the low word at +16. Reusing our Zwo cell struct fails.
* 004 collects teleport cells into dynamic arrays rather than relying solely on
  the legacy 16-entry serialized section. Its spawner lists are also dynamic.
* 004's reader consumes 1440 bytes of the final suffix, then a four-byte field
  into game state +1224. The final 12 file bytes are not consumed by that
  routine. Preserve all 1456 bytes; the full meaning is not yet established.
* That +1224 state is used by camera-bound calculations. The camera follows one
  player's center or the midpoint of two players, then clamps to configured
  bounds. It is incorrect to describe this field as an exit counter.

The native 004 reader rejects versions below 12. That does not establish support
for hypothetical later formats. Our importers should accept only understood
versions and retain original raw records for lossless round trips.

## 3. Object kinds: separate number spaces

These are **object record kinds**, not grid terrain IDs or item IDs. Confusing
these three namespaces is a major source of conversion errors.

| Meaning | Zwo | XXX | 004 |
|---|---:|---:|---:|
| Player | 0 | 0 | 0 |
| Scenery | 1 | 1 | 1 |
| Sheep | 2 | 2 | 2 |
| Alien enemy | 3 | 3 | 15 |
| Dog enemy | 4 | 4 | 16 |
| Farmer | 5 | 5 | 17 |
| Girl | 6 | 6 | 18 |
| Beer | 7 | 7 | 22 |
| Teddy | 8 | 8 | 21 |
| Bone | 9 | 9 | 20 |
| Alien artifact/plant | 10 | 10 | 23 |
| Chicken | 11 | 11 | 7 |
| Goat | 12 | 12 | 8 |
| Pig | 13 | 13 | 9 |
| Rocket | 14* | 14 | 24 |
| Present | 15* | 15 | 25 |
| Slow mushroom | 16* | 16 | 26 |
| Stinky mushroom | 17* | 17 | 27 |
| Flower | 18* | 18 | 28 |
| Penguin | — | 25 | 4 |
| Turtle | — | 26 | 5 |
| Rabbit | — | 27 | 6 |
| Mermaid | — | 28 | 3 |
| Groundhog system | — | — | 10 |
| Alien pet | — | — | 11 |
| Hedgehog | — | — | 12 |
| Peacock | — | — | 13 |
| Skunk | — | — | 14 |
| Hunter enemy | — | — | 19 |
| Horn | — | — | 29 |
| Coin | — | — | 30 |
| Vehicle | — | — | 31 |
| Barrel | — | — | 32 |
| Configurable collision/moving object | — | — | 33 |
| Thin ice | — | — | 34 |
| Extras box | — | — | 35 |
| Exit tile | — | — | 36 |
| Bubble/yellow effect emitters | — | 43,44 | Not observed as saved kinds |

\* Existing Zwo runtime types; none are pre-placed in the supplied original Zwo
maps. The backport already added serialized loading for them. Their appearance
in XXX files does **not** make the underlying powerup a new XXX invention.

004 kind 32 binds `barrel.bmp`; it should not be mislabeled a transporter.
Kind 33 renders a tileset-selected image and exposes a **transporter setting**
in native editor diagnostics. It needs its own behavior/configuration model,
not just a static collision rectangle. Kinds 34/35/36 bind `ice_thin.bmp`,
`extras_box.bmp` and `icon_exit.bmp`, respectively.

XXX's new animals across all five packs: **65 penguins, 11 turtles, 42 rabbits
and 34 mermaids**. Its 43/44 records occur 44/219 times. Native resource binding
connects them to `pfx_bubbles.bmp` and `pfx_yellow.bmp`, respectively. Their
editor-only marker rendering and runtime particle dispatch are confirmed;
the full emission parameters still need tracing. Do not silently discard them
as decoration or confuse these records with rain/snow.

004 contains **every saved kind from 0 through 36**, including 186 hunters,
134 vehicles, 100 barrels, 1058 configurable collision objects, 604 thin-ice
objects, 201 extras boxes and 204 exits. Counts are record counts, not promises
that every record begins visible or active in gameplay.

## 4. Grid and spawn codes

Observed terrain values:

* Zwo: 0…9, 10, 11, 100.
* XXX: 0…15, 100; new animal terrain 12=penguin, 13=turtle,
  14=rabbit, 15=mermaid.
* 004: 0…20, 30, 31, 100. Terrain 3…6 remains
  alien/dog/farmer/girl; 7…9 becomes penguin/turtle/rabbit; 12…14
  chicken/goat/pig; 15 mermaid; 16 vehicle; 17…20
  alien-pet/hedgehog/peacock/skunk; 30 hunter; 31 barrel.
  Sheep remains 2, teleport remains 10. Code 11 must retain its special
  collision semantics rather than become a generic actor brush.

Terrain-to-actor links and raw samples are in the per-level inventory. Terrain
0/1/11/100 also serve world/grid roles; no one-to-one actor enum should be used.

Item codes 128…136 remain present in the later games, but their linked object
kinds change. 004 additionally uses **32, 54, 55, 160, 162 and 165**:
linked records identify configurable collision objects, horns, coins, thin ice,
groundhogs and exits, respectively. These are not six ordinary collectible
powerups. Item 162 is explicitly collected into the native groundhog system,
which checks a maximum of 128 groundhog positions.

Native ordinary spawner processing recognizes markers **140, 141 and 142**.
Across the later format's byte +18, 004 files also contain **1, 143, 144 and
145**. Their presence is counted in the matrix, but that does not prove they
are additional random-item spawner categories. Their consumers and exact
semantics remain an explicit investigation item.

## 5. What XXX adds to the current rebuild

1. **Local multiplayer and selectable player roles.** Every supplied XXX map
   stores two player records, including single-player maps. Correct player
   activation, separate controls, role animations/interactions, shared-world
   collision, score/life handling and multiplayer UI replace the backport's
   deliberate removal of slot 1. Header bits 1…4 select the primary mode;
   bits 5…7 select a secondary mode. Bit 0 controls UFO availability. These
   mode bits must not be mislabeled as weather flags.
2. **Four new animal families.** Penguin, turtle and rabbit are completion
   targets in the native count routine. Mermaid is handled separately. We need
   their exact interaction, anger/idle and completion behavior and role-specific
   animations. Replacing turtles with goats was appropriate only for the XS
   compatibility experiment, not a faithful XXX port.
3. **Tutorial/help data and later visual/effect behavior.** Preserve the 4 KB
   help table, cell help references, matching HelpNN strings and popups. Audit
   the additional emitter records and later particle/weather resources.
4. **Later rule/configuration and presentation sets.** Import XXX's own
   animation, sound, UI, bonus-level, role and progress data. Core XML defines
   254 named parameters, 198 sprite entries and 125 sounds, versus Zwo's
   83 parameters and 89 sounds. Zwo's sprite setup is largely hardcoded, so
   its zero XML sprite definitions is not an absence of sprites.

The complete named parameter comparison, including shared names whose values
change, is available in [core-parameters.csv](core-parameters.csv).

The working XXX XS backport intentionally remains Zwo gameplay: substituted
animals, removed mermaids/help and one player. It is not a foundation for
claiming full XXX compatibility without the work above.

## 6. What 004 adds beyond XXX

* **Larger scrolling worlds:** new grid/pool sizes, camera behavior, culling,
  world-to-screen transforms, interaction distances and pathfinding over the
  larger map. Multiplayer camera behavior must be reproduced too.
* **Five further animal families/systems:** alien pet, hedgehog, peacock,
  skunk and groundhog. Groundhogs have appearance/disappearance and catching
  states and a catch-all bonus, not the ordinary sheep completion lifecycle.
  Existing animals also have richer animation/state sets. Sheep initialization
  masks the facing field with 7, so a universal four-direction assumption fails.
* **Hunter and changed AI:** the core describes five AI roles, per-role walking,
  running and driving settings, hunt distances, route lengths, idle/random
  behavior, shooting, distraction and hit rules for different player roles.
  `AI_1…AI_5 = 6,4,5,3,30` are role/tile identifiers, **not** 004 object kinds.
  We need the actual later state machines, not Zwo AI with new textures.
* **Vehicles and world mechanisms:** entering/driving, collisions, braking,
  crashes, barrels, configurable moving collision objects and transporter
  settings. Core audio explicitly names doors, gates and lifts as trigger uses.
  Exact trigger/action field decoding and all vehicle variants remain to be
  documented before implementation can be called faithful.
* **Additional items and objectives:** coins, horns, extras boxes, thin ice,
  groundhog catching and exits. The core has separate exit-open and exit-use
  sounds; finishing cannot universally mean immediately leaving after the last
  ordinary animal. Native completion counting tracks exit records separately.
* **Expanded presentation/audio:** level entries use **a1…a9** instead of one
  `music` attribute, and the loader creates nine music resources. The switching
  policy still needs tracing. Add the extra UI, role feedback, particles,
  vehicle/hunter/trigger/animal sounds and exact later asset conventions.
  Core XML contains 681 parameters, 317 sprite entries and 195 sounds.

Resources establish that a feature exists; they do not by themselves establish
its timing or every gameplay rule. The state-transition details above require
focused native tracing and parity tests during the actual port.

### Original engine dependencies

We can continue retaining the original dependencies, but should not assume
the Zwo DLL build is the later games' build. File hashes show that XXX and 004
share identical `mudGE.dll` and `wtnlib.dll` files; both differ from Zwo's.
`directX3.dll` is identical in Zwo/XXX but differs in 004. The sound plugin is
identical across all three supplied games. See [engine-library-hashes.json](engine-library-hashes.json).

The executables import 18/26/26 mudGE symbols and 77/81/83 wtnlib symbols.
Later imports include `cPicture` load/save/pixel-format/size functions; 004 also
imports `cBitmap::GetWidth` and additional scheduler/application helpers.
[engine-imports.json](engine-imports.json) contains the exact decorated names.
This establishes an ABI audit requirement for our wrappers and virtual calls;
it does not prove every additional import is needed in our implementation or
that the later DLLs can be dropped into the Zwo bootstrap unchanged.

## 7. Recommended changes to our architecture and editor

1. **Versioned import/export first.** Add separate Zwo/XXX/004 codecs producing
   semantic entities, with raw unknown fields retained. Keep source numeric IDs
   at the codec boundary. Add lossless read/write checks before editing v12.
2. **Explicit game profiles.** A package selects a ruleset, asset namespace,
   configuration, UI and format capabilities. Do not overwrite Zwo parameters
   with a merged XXX/004 XML dictionary. Keep our working Zwo behavior stable.
3. **Extend the existing character classes.** Share position/collision/render
   interfaces and common helpers. Use family-specific animal controllers and
   game-specific AI/interaction policies; avoid one enormous switch or a base
   class filled with behavior irrelevant to most actors.
4. **Multiple players as session state.** Separate input/player slots from
   character species and AI, then implement activation, scores, lives,
   role-specific interactions and camera policy from the selected profile.
5. **World mechanisms as entities/components.** Add vehicles, triggers,
   collision/moving objects, ice, extras boxes, groundhog management and exits
   with their own state machines and event connections.
6. **Editor capabilities per profile.** Expose the right animal/enemy brushes,
   two starts, larger-map scrolling, packed fields, help, trigger connections,
   facing/state values, nine music entries and game-specific validation.
   Keep unresolved metadata editable only as preserved raw data initially.
7. **Unified loose-file library.** Keep package-qualified IDs, e.g.
   `zwo/XS`, `xxx/01_XS`, `004/01_xs`; names and filenames collide across games.
   The pack reader can remain an import/export adapter. The final library can
   expose all **436 original levels**, while loading each with its correct
   profile. Backported variants should have separate identities and scores.

Recommended order: faithful XXX XS (no substitutions) → all XXX packs → 004
lossless loading and scrolling → new actors/groundhogs → vehicles/world
mechanisms → multiplayer/UI/progress parity → all-pack regression pass.
Multiplayer infrastructure can start earlier; the ordering describes useful
playable milestones, not a requirement to postpone its design.

## 8. Evidence and limits

Key saved native evidence in [ida/](ida/):

| Build | Address | Evidence |
|---|---|---|
| Zwo | 0x433520 | Original v6 reader; existing named dump/research |
| Zwo | 0x422370 | Original spawner scan; existing research |
| XXX | 0x440EF0 | v6/v7 reader; `../../runtime/xxx-xs-analysis/xxx-read-binary.txt` |
| XXX | 0x430490 / 0x430570 / 0x430630 / 0x4308E0 | Animal counts, spawners, actor/mode initialization, help; prior XS analysis files |
| XXX | 0x412270 | Animal/emitter rendering; prior `xxx-draw-new-animals.txt` |
| XXX | 0x42F755 / 0x42F7A2 | Bind bubble/yellow resources to members +96236/+96240 used by kinds 43/44; `xxx-emitter-resource-bindings.json` |
| 004 | 0x466AF0 | `004-read-binary.txt`: actual v12 section reads |
| 004 | 0x466C80 | `004-xml-level-load-candidate.txt`, `004-xml-load-reading.txt`: descriptor loading/music |
| 004 | 0x43FF60 | `004-count-objects.txt`: completion/exit counts |
| 004 | 0x44F8F0 | `004-open-exit-actual.txt`: activates kind-36 exits, changes grid item byte to 163, plays SfxOpenExit |
| 004 | 0x440130 | `004-spawners.txt`: marker byte, dynamic lists, groundhog scan |
| 004 | 0x440370 | `004-init-actors.txt`: role flags, family initialization |
| 004 | 0x414EC0 | `004-create-object*.txt`: 112-byte object creation and packed dimensions |
| 004 | 0x419830 | `004-render-objects*.txt`: family rendering, vehicle/barrel/collision/ice/box/exit dispatch |
| 004 | 0x430A80, 0x439BA8…0x439C65 | Resource binding: ice, extras box, exit |
| 004 | 0x44F1F0 / 0x44F470 / 0x44F950 | Camera following/clamping and bounds tests; saved filenames containing `music-zone`/`open-exits` were exploratory labels, **not established function names** |

Queries were read-only against the supplied live IDA servers; no dump renaming,
comments or configuration changes were made. The local helper saves complete
responses, including truncation/cursor information. Large decompilations have
numbered continuation pages. Core XML originals are under [core-xml/](core-xml/).
Legacy XML parsing repairs encoding/unescaped characters **in memory only**.

Reproduce the static inventory from the repository root:

```powershell
python -B rebuild/tools/audit_game_compatibility.py
python -B rebuild/research/compatibility/summarize.py
```

This audit covers every supplied saved type/code, not every reachable native
state. Native runtime dispatch also includes transient particles/projectiles and
effects not saved in any original level. They need an effects/state-machine
audit during implementation, rather than being mistaken for missing map types.

Remaining RE questions: XXX emitter timing/parameters; 004 marker
1/143/144/145 semantics; the full final-suffix schema; precise trigger links and
actions; music switching; all per-family extra record fields; exact later AI,
multiplayer and completion transitions. These are known gaps, not supported
features. Static parsing and native inspection were performed; **the 436 levels
were not individually played or runtime-validated** in this task.

# Integrated editor

Press **F3** from the title/menus to start a blank project using the selected
pack as the format/theme template. In a running or paused level, F3 opens that
authored level. Your running game freezes and resumes unchanged on editor close.

| Action | Control |
|---|---|
| Select and move | **V**, click an opaque part of an object, drag and release; **Ctrl+click** cycles overlapping objects |
| Place | Click Place or **B**, choose a sidebar thumbnail, then click the level; **P** repeats the current object; scenery returns to Select after one placement |
| Scenery placement anchor | **A** switches between visible-art center and canvas origin; the yellow reticle shows the anchor |
| Zoom | **Ctrl+wheel** over the canvas; 25% to 400%, anchored at the cursor |
| Pan | Hold the **middle mouse button** and drag over the canvas |
| Reset view | **0**, or Reset in the sidebar View tab |
| Visibility layers | **L** or the sidebar View tab; toggles affect the editor only |
| Change placement choice | Click a sidebar thumbnail; wheel or Left/Right over the canvas cycles the current tool choices |
| Tile brush | Click Tiles in the main toolbar, then choose a brush in the sidebar; **C** activates the current brush |
| Erase tile/spawner | Right-click in Tiles mode; terrain and spawner brushes erase their own layer |
| Scenery snapping | **N** toggles 40px/pixel placement and dragging; free scenery nudges by 1px, Shift+arrows by 10px |
| Level/object/cell inspector | **I** or the sidebar Props tab; choose Level/Object/Cell/Project, then a field |
| Select gameplay cells beneath artwork | **T**, the inspector's Cell tab, or Shift+click; **V** returns to object selection |
| Rotate selected animal/enemy | **R**, or the inspector's Facing field |
| Delete selected object | Delete or Backspace |
| Cancel selection/drag | Right mouse button |
| Undo / redo | Ctrl+Z / Ctrl+Y; toolbar buttons |
| Hide/show toolbars | Tab; this exposes the cells under the toolbars |
| Hide/show all editor interface / playtest HUD | **H**; restores panel state when shown again |
| Visual placement sidebar | **B** or Place; click a thumbnail, then click the level |
| Existing-object list | **O** or Object → Object list |
| Toggle grid | Grid in the sidebar header |
| Save a separate folder pack | Ctrl+S or Save copy |
| Enter/leave playtest | F5 or Playtest; Escape also returns from playtest |
| Close editor and resume original game | F3, Escape or Close |

The movable sidebar shows actual texture thumbnails. Filter with **All**, **Scenery**,
**Actors** (animals, enemies, player starts and special objects), **Items**, **Tiles**, **Props**, or **View**. Tabs use two rows.
The Tiles tab contains clear terrain, collision, puddle destinations, water
entries, distraction/powerup/mixed random spawners and remove-spawner brushes.
Tiles use color-coded icons; D/P/M distinguish random-spawner groups. Brush
selection alone never edits data. Left-click paints and right-click erases the
chosen layer, keeping terrain and spawners independent. Properties has its own
Props tab; Grid remains in the sidebar header; **A** and **N** still control scenery anchoring and snapping.
Drag the sidebar header to move the entire panel; its position is shared by all
tabs and clamped inside the game view. The former placement selector row and
floating inspector are removed, exposing more canvas.

Scroll over the sidebar to browse; click **Find** to search, Ctrl+A clears it,
and Enter selects the first match. Escape leaves search. Choosing a thumbnail
only selects the placement tool; the level changes when you click the canvas.
Scenery still returns to Select after one placement. **B** hides the sidebar to
reach the left part of the level; **H** hides the whole interface. **O** opens
the separate browser of objects already placed, replacing the palette.

**View** separates background, scenery, player starts, animals, enemies, items,
special actors, tiles/spawners, grid and object-slot labels. Hidden object layers
are excluded from canvas picking; the instance browser still inventories the
whole document. Picking a placement thumbnail reveals that object's layer.
Layer/zoom/pan changes never enter level history or exports. Playtest uses normal
game presentation, then restores the editor's view and layers on return.

Ctrl+wheel zooms the canvas under the pointer; ordinary wheel retains its tool
choice/thumbnail scrolling behavior. Middle-drag pans, including across panels
once the gesture starts on the canvas. **0** restores 100% and the original
origin. The sidebar, toolbars and cursor keep their normal size. Selection,
dragging, grid painting, actor snapping and free scenery use world coordinates.
Panning keeps a strip of the map reachable; Reset recovers the default view.

New scenery with visible overlap defaults above earlier **world-sorted scenery**.
Its saved depth offset encodes that order, including when a deleted slot is
reused; one Undo removes the entire placement. Separate scenery retains its
natural depth. Existing levels are unchanged until edited. Native character
sorting and explicit foreground passes still take precedence; use Object's
Depth/Render pass/Draw earlier/later when deliberately overriding the defaults.

While editing, the game loops `assets/XXX/SvenXXX/sfx/editor.wav`. **F5** switches
to the level's music and restores the editor track on return. Closing the editor
restores the previous game/menu track. WAV music uses Windows' built-in playback;
OGG music and sound effects retain the original engine backend. Missing editor
music or a Zwo-only installation falls back to the level's music; `--no-audio`
continues to silence it. The editor WAV is now normalized to the title music's
measured loudness (about -10.4 LUFS), with a true-peak ceiling near -1 dBTP.
The original quiet WAV is backed up at `rebuild/runtime/editor-music-original.wav`;
its PCM format and duration are preserved, and title music/textures are unchanged.

Actors and items always snap to the original 40-pixel grid. The palette uses
the current tileset and the runtime's registered animals, enemies, items,
player starts and special objects. Scenery and collision are separate: moving
a bush does **not** move the level's collision cells. Use the collision tool to
edit those cells explicitly. Free scenery also accepts fractional X/Y positions
in its inspector. Puddle/water brushes edit gameplay cells, not scenery: place
their visible artwork separately, as in the standalone editor.

Scenery defaults to centering its visible pixels around the pointer (or the
snapped cell center), ignoring transparent sprite padding. **A** restores the
old canvas-origin placement when needed. Preview and placement use the same
calculation, including scenery partly outside the level. Actors keep their
native grid/collision anchors; the reticle shows that cell center. The cursor
uses the same hotspot as the game's menus, so its tip matches the click position.

**Props** replaces the floating I menu inside the 320-pixel sidebar. **I** opens
this tab and returns to placement when pressed again. It contains four subtabs:

| Tab | Available properties |
|---|---|
| Level | Display title, timer, texture pack, music path, UFO visibility, puddle destination list/rebuild; XXX weather and bonus controller |
| Object | Position, depth offset; scenery texture ID/frame, sorted/foreground pass and Draw Earlier/Later; animal/enemy facing, animal impatience and item permanence |
| Cell | Raw visible-cell index, terrain, spawner group, direction; XXX help ID |
| Project | Pack/levelset names, level list, blank/duplicate/import, new blank pack and open a full installed pack |

Cell selection ignores all artwork; it never starts an object drag. Object
selection uses opaque sprite pixels and shows the sprite's bounding rectangle.
Use Ctrl+click to cycle overlaps, or the persistent object list to search by
slot/name and select a hidden object directly. Its rows remain visible while
you select, move, delete or undo. Click Find to filter; Enter selects the first
match and leaves text-entry mode. Escape cancels text focus. Wheel scrolls;
drag its header to move it and use +/- to fold it. Clicking a row never begins
a world drag or activates a toolbar control underneath. **O** closes the list.
Switching to Cell clears the
object selection so Delete cannot remove a previously selected sprite.

**H** hides the toolbar, status, sidebar and object list, releasing their
screen areas for editing. Grid/selection guides remain usable; Grid toggles
the grid separately. During F5 playtest, H hides the clock/score/lives, help/card
overlays and playtest banner. It does not change gameplay or saved level data.

Click a text field, type to replace its value, then press **Enter**. Backspace
edits, Ctrl+A selects the whole value, and Escape cancels. Spaces are accepted
in names/paths. Click a choice to cycle forward, right-click to cycle backward.
**Show UFO** uses the source format's actual flag polarity. XXX-only flags are
shown only for v7 levels; controller choices include the shipped Farmer, Girl
and Alien modes, not the unused Dog mode.

**Level → Theme** opens a searchable chooser of all installed Zwo and XXX
themes. Type to filter; click an entry or use arrows/wheel and Enter. Escape
cancels. Any theme works on any level: the background and placement palette
change, while existing scenery retains its actual artwork. Existing scenery
texture IDs are safely remapped into a merged table instead of being reinterpreted
as unrelated pictures. Its position, dimensions, frames and collision remain intact.
Use an object's Texture ID or replace/place artwork to redesign existing scenery.
Music paths are relative to the active data directory (including `ZWO/` or
`XXX/` in combined installations); blank disables music. Playtest uses the
edited track and restores the original game's track when returning.

Spawner brushes place distraction, player-effect or mixed random markers.
Amber outlines and D/E/M labels show their groups. Each group allows 16 cells.
Puddles are cyan/P and water entries blue/W. Painting/removing a puddle updates
its destination table as one undoable edit; the table accepts at most 16 distinct
puddle cells. The Level inspector can restrict/reorder destinations by comma-
separated raw indices, or rebuild from all puddles. Actors/items must be moved
or deleted through their own tools before their cells can be repainted.

Draw Earlier/Later changes depth offsets without renumbering slots. World depth
is `Y + height - depth offset`; larger offsets draw earlier. Foreground scenery
draws after the sorted world in authored slot order. Actor animation positioning
continues to follow the native runtime.

Ctrl+S saves the **whole project**, creating `Editor_1`, `Editor_2`, etc. beneath the source edition in the
active runtime data directory, normally beside the EXE:
`assets/ZWO/Editor_N` or `assets/XXX/Editor_N`. Subsequent saves in the same
editor session update that new pack. Closing and reopening starts another copy.
The pack references installed source tilesets/music, including cross-edition
assets; it is not a self-contained asset bundle. Existing packs and the
repository source assets are not overwritten. On closing the editor, the level
menu refreshes so the new pack can be selected without restarting.

To create a pack:

1. F3 from the title screen, or **Project → New blank pack**. Dirty projects
   require a second action to discard; saving first preserves them.
2. Set **Pack name**, **Levelset**, and the **Level** tab's title/theme/timer.
   A blank contains only valid player starts: one for v6, two for v7. Place
   animals/completion targets before trying to complete it.
3. **Add blank level** adds another level to the active levelset. **Duplicate**
   copies its authored data. **Project levels** switches without losing edits.
   Change Levelset to register a level under another episode; names need not be
   filesystem names. Pack names must not collide with an installed pack and
   cannot contain filename separators/reserved characters (they identify highscores).
4. **Import .lvl** registers an existing level, retaining its bytes and using
   the current theme/music as its resource template. It must match the project's
   v6/v7 format. Check its scenery before playtest because raw files lack tilesets.
5. Ctrl+S commits all levels and episode metadata. Files are staged under a new
   revision, then the descriptor is atomically replaced. A failed save leaves
   the last playable revision and dirty edits intact.

**Open full pack** browses installed packs and loads a working copy of every
episode/level. Saving produces a separate named pack, preserving the original.
F3 during gameplay initially copies only the current level; use Open full pack
when you want the entire source pack. Playtest still runs the active level only.

Playtest uses a fresh game session with the current one/two-player selection
and configured controls. Its scores, cards and progress are temporary. F5
returns to the unchanged edit document. Unsaved edits require a second close
command to discard, including when closing the game window.

Undo/redo and savepoints include both binary level changes and display metadata.
Unrelated v6/v7 fields remain preserved. The parity checklist still includes
advanced/raw inspectors, CSV inventory, teleport/link annotations, continuous
painting and further project conveniences such as level deletion/reordering.
The canvas shows static authored object
sprites; playtest provides the real animation, bonus-controller and gameplay
presentation.

Validation and continuation details: [editor progress](research/editor/PROGRESS.md).

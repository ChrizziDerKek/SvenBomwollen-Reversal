# Integrated editor

Click **Editor** above Exit on the title screen, or press **F3** from the
title/menus, to start a blank project using the selected
pack as the format/theme template. In a running or paused level, F3 opens that
authored level. Your running game freezes and resumes unchanged on editor close.

| Action | Control |
|---|---|
| Select and move | **V**, click an opaque part of an object, drag and release; **Ctrl+click** cycles overlapping objects |
| Place | Click Place or **B**, choose a sidebar thumbnail, then click the level; **P** repeats the current object; scenery stays active for repeated clicks |
| Scenery placement anchor | **A** switches between visible-art center and canvas origin; the yellow reticle shows the anchor |
| Zoom | **Ctrl+wheel** over the canvas; 25% to 400%, anchored at the cursor |
| Pan | Hold the **middle mouse button** and drag over the canvas |
| Reset view | **0**, or Reset in the sidebar View tab |
| Visibility layers | **L** or the sidebar View tab; toggles affect the editor only |
| Change placement choice | Click a sidebar thumbnail; wheel or Left/Right over the canvas cycles the current tool choices |
| Tile brush | Click Tiles in the main toolbar, then choose a brush in the sidebar; **C** activates the current brush |
| Erase tile/spawner | Right-click or right-drag in Tiles mode; terrain and spawner brushes erase their own layer |
| Scenery snapping | **N** toggles 40px/pixel placement and dragging; free scenery nudges by 1px, Shift+arrows by 10px |
| Raw object/cell words | Sidebar **Raw** tab; pick Object words or Cell words |
| Level/object/cell inspector | **I** or the sidebar Props tab; choose Level/Object/Cell/Project, then a field |
| Select gameplay cells beneath artwork | **T**, the inspector's Cell tab, or Shift+click; **V** returns to object selection |
| Animal placement defaults | Select an animal in All/Actors, then click footer Facing/Impatience; right-click cycles backward |
| Rotate selected animal/enemy | **R**, or the inspector's Facing field |
| Delete selected object | Delete or Backspace |
| Return to object selection / clear selection | **Escape**; text fields and pickers cancel first |
| Delete scenery under the pointer | **Right-click** while placing scenery, or on topmost scenery in Select mode; actors are preserved |
| Undo / redo | Ctrl+Z / Ctrl+Y; toolbar buttons; project history while the Project levels list is open |
| Hide/show toolbars | Tab; this exposes the cells under the toolbars |
| Hide/show all editor interface / playtest HUD | **H**; restores panel state when shown again |
| Visual placement sidebar | **B** or Place; click a thumbnail, then click the level |
| Existing-object list | **O** or Object → Object list |
| Toggle grid | Grid in the sidebar header |
| Export active level / inventories | **Props ? Project ? Export .lvl / Object CSV / Cell CSV** |
| Manage level order / deletion | **Props > Project > Project levels**; Earlier / Later / Delete |
| Save a separate folder pack | Ctrl+S or Save copy |
| Enter/leave playtest | F5 or Playtest; Escape also returns from playtest |
| Close editor and resume original game | **F3** or Close; Escape closes when already in idle selection mode |

The movable sidebar shows actual texture thumbnails. Filter with **All**, **Scenery**,
**Actors** (animals, enemies, player starts and special objects), **Items**, **Tiles**, **Props**, **View**, or **Raw**. Tabs use two rows.
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
Scenery placement stays active: every left-click places another object. Each
right-click deletes one opaque scenery hit (not actors); each click is a separate
undo step. Escape removes the placement preview and returns to Select, or clears
the selected object before a later Escape can close the editor. **B** hides the sidebar to
reach the left part of the level; **H** hides the whole interface. **O** opens
the separate browser of objects already placed, replacing the palette.

Select an animal thumbnail in **All** or **Actors** to expose **Facing** and
**Impatience** in the sidebar footer. Click the left field to cycle facing 0..3;
click the right field to cycle impatience 0..10. Right-click reverses either
field. Animal thumbnails and the placement ghost show the chosen standing frame.
Defaults start at facing 0 and impatience 6, retain their values across animal
choices and level switches in this editor session, and apply only to subsequent
placements. They do not modify existing animals or create undo entries on their
own. Each configured placement remains one undoable edit. Existing animals can
still be changed in Props/Object. The native runtime rounds odd impatience down
to the preceding even value when loading; the authored value remains unchanged
in the file. These settings do not apply to enemies, player starts or mermaids.

**View** separates background, scenery, player starts, animals, enemies, items,
special actors, tiles/spawners, grid and object-slot labels. Hidden object layers
are excluded from canvas picking; the instance browser still inventories the
whole document. Picking a placement thumbnail reveals that object's layer.
Layer/zoom/pan changes never enter level history or exports. Playtest uses normal
game presentation, then restores the editor's view and layers on return.

Tile brushes now support continuous strokes. Hold **left mouse** and drag to
paint; hold **right mouse** and drag to erase. Gaps between mouse samples are
filled along the grid, including diagonals. A stroke keeps its chosen brush and
layer until finished; each cell is attempted once. **Ctrl+Z/Y** undoes/redoes the
whole stroke as one edit. A stroke that changes nothing preserves redo history.

Painting pauses over panels, toolbars and outside the canvas, then resumes at
the current cell without connecting across that hidden segment. The held stroke
cannot activate controls. Releasing the button finishes it; Escape finishes the
stroke without closing the editor. Tool/view commands, save, F5, window close and
focus loss finish it first. Unsupported actor/item cells and marker-limit excesses
are skipped with a status message; valid cells continue to paint. Terrain and
spawner brushes still modify only their own layer. Native limits remain 16 puddle
destinations and 16 spawners per group.

Ctrl+wheel zooms the canvas under the pointer; ordinary wheel retains its tool
choice/thumbnail scrolling behavior. Middle-drag pans, including across panels
once the gesture starts on the canvas. **0** restores 100% and the original
origin. The sidebar, toolbars and cursor keep their normal size. Selection,
dragging, grid painting, actor snapping and free scenery use world coordinates.
Panning keeps a strip of the map reachable; Reset recovers the default view.

New scenery now defaults to **Natural** depth (sorted pass, offset 0). Its
sorting line is its own Y + height, so characters appear behind it when above
that line and in front when below it. Overlapping another sprite no longer
pushes its depth to that sprite's bottom: grass placed over a tall river bitmap
therefore does not inherit the river's ground line or accumulate large negative
offsets from repeated placement. Native gameplay sorting itself is unchanged.

The Scenery sidebar footer cycles placement **Depth** with left/right clicks:

- **Natural**: normal world depth; use for upright scenery such as grass/bushes.
- **Stack scenery**: the previous behavior, drawing new overlapping world scenery
  later than existing scenery, even in recycled slots. This deliberately changes
  its relation to actors too; use only when that layering is wanted.
- **Floor**: draw underneath actors; use for rivers, paths and ground decoration.

These are placement preferences, not edits to existing objects. For already
placed scenery, **Props > Object > Natural depth > Reset** clears custom depth
and Foreground in one undoable edit. It also leaves floor mode. Original levels
and custom packs are never bulk-rewritten; repair only selected pieces. Use
Depth/Render pass/Draw earlier/later for deliberate manual overrides. No texture
name guesses or universal per-texture depth table are imposed: original levels
use different adjustments for the same texture in different compositions.

For floor art, select the scenery and open **Props ? Object ? Draw at bottom**.
**Yes** uses the existing native sorted pass and its largest signed depth
adjustment, placing the object beneath actors. It clears Foreground, survives
moves/texture changes and exports normally in both Zwo and XXX formats.
The depth and earlier/later controls show Floor while enabled. Turning it off
restores natural depth (offset 0); Undo restores the previous custom setting.
Choosing Foreground explicitly turns floor mode off. This changes visual order
only: scenery still has no automatic collision cells.

**Raw** exposes every 32-bit word: object words **0..24** for slots **0..255**,
and cell words **0..5** for all **374** records (**0..373**, including borders).
These are zero-based word indices; the standalone editor labelled object fields
1..25. Click the slot/cell index to choose a record, or pick the object/cell on
the canvas. Object/Cell words selects the matching canvas selection mode.

Click a word row or the Word value field, type a signed decimal or `0x`-prefixed
32-bit hex value, then Enter. The footer shows the same bits as hexadecimal and
IEEE float; float aliases are a readout, not an automatic conversion. Wheel or
Prev/Next or Left/Right browses words; Up/Down selects words while the pointer
is in the panel. Delete/Backspace there does not delete the world selection.
Ctrl+A replaces text; Escape cancels a pending edit. Ctrl+S/F5 commit a valid
pending value before saving/playtesting. Invalid input remains editable and does
not change the document. Each applied word uses normal undo/redo and saving.

Raw writes preserve every unrelated word/packed bit. Kind and ownership links
stay on the placement/movement tools; live position edits use those tools' link
repair. Known size, sprite/frame, float, depth-overflow and marker constraints
are checked. Cell terrain edits maintain the puddle table. Packed direction,
item and spawner/help words preserve or validate their semantic low bits, while
uninterpreted bits remain raw. Inactive objects and border cells are inspectable;
structural boundary changes remain protected.

**Props ? Project** has three separate Save As actions for the active level:

- **Export .lvl** writes the exact edited binary level, retaining its v6/v7 version
  and unknown bytes. The filename can be changed freely, including Unicode names.
- **Object CSV** lists all 256 slots, including inactive records. It includes
  kind, position, tile link, pass/mirror, texture/permanence and depth, followed
  by all 25 raw words (`w0..w24`) as signed decimal integers.
- **Cell CSV** lists all 374 records, including borders: raw index, zero-based
  visible-grid column/row, a visible flag, and all six raw words (`w0..w5`).
  Visible cells have columns 0..19 and rows 0..14; borders fall outside those ranges.

CSV means comma-separated values: a spreadsheet-style inventory for filtering,
comparing and investigating raw data. Files use comma delimiters, decimal dots
and CRLF rows, independent of regional number formatting. Raw word bits are
preserved exactly in signed integer form. This is an export, not CSV import.

Choose a location outside the active installed assets directory. The file dialog
adds the appropriate extension when omitted and asks before replacing a file.
Exports use an exclusively reserved temporary file and atomic replacement;
failed writes retain the prior destination. Asset-directory links and case
variations cannot bypass source protection.

These actions export copies of the active level only. They do not change the
project's dirty/savepoint state or add undo entries. A `.lvl` does **not** include
its pack descriptor, display name, theme mapping, music or textures; import it
using a matching resource template. Use **Ctrl+S** for the complete playable
folder project, especially when themes include retained/mixed resources.

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
| Object | Position, depth offset; scenery texture ID/frame, sorted/foreground pass, Draw at bottom and Draw Earlier/Later; animal/enemy facing, animal impatience and item permanence |
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

**Project levels** also manages the pack's level order. Its highlighted row is
initialized to the active level; use search, arrows or the wheel to highlight
another. **Earlier/Later** swaps it with the previous/next level in the same
levelset, retaining that level's document, properties and edits. Other levelsets
are unaffected. Click a row or press Enter to open it for editing.

**Delete** needs a second click for the same highlighted level. Escape, changing
the filter/selection or another action cancels confirmation. The final project
level cannot be deleted. Removing the last level of a levelset omits that empty
levelset when saving. Deletion changes the working project; Ctrl+S commits it.

**Undo project / Redo project** restore project operations: level reorder,
deletion, blank/duplicate/import additions, pack naming and levelset assignment.
While this list is open, Ctrl+Z/Y and the toolbar Undo/Redo use that same project
history. Outside the list, Undo/Redo continue to edit the current level's objects,
tiles and properties. These are separate histories: restoring a deleted level
retains its exact document and object-edit history, including edits made before
it was deleted. Saving retains project history and tracks its savepoint. New
project actions clear project redo; opening another pack or creating a new pack
clears project history. Up to 128 project actions are retained in memory; history
is not persisted after closing the editor.

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
teleport/link annotations and the final standalone-editor parity review.
The canvas shows static authored object
sprites; playtest provides the real animation, bonus-controller and gameplay
presentation.

Validation and continuation details: [editor progress](research/editor/PROGRESS.md).

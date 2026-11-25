# Integrated editor

Press **F3** from the title/menus to start a blank project using the selected
pack as the format/theme template. In a running or paused level, F3 opens that
authored level. Your running game freezes and resumes unchanged on editor close.

| Action | Control |
|---|---|
| Select and move | **V**, click an opaque part of an object, drag and release; **Ctrl+click** cycles overlapping objects |
| Place | **P**, choose a category/type in the second toolbar row, then click the level |
| Change placement choice | Mouse wheel or Left/Right; click the left/right portions of the type selector |
| Tile brush | **C** or Tiles; wheel/Left/Right chooses collision, puddle, water or a random spawner group |
| Erase tile/spawner | Right-click in Tiles mode; terrain and spawner brushes erase their own layer |
| Scenery snapping | **N** toggles 40px/pixel placement and dragging; free scenery nudges by 1px, Shift+arrows by 10px |
| Level/object/cell inspector | **I** or Props; click a tab, then a field |
| Select gameplay cells beneath artwork | **T**, the inspector's Cell tab, or Shift+click; **V** returns to object selection |
| Rotate selected animal/enemy | **R**, or the inspector's Facing field |
| Delete selected object | Delete or Backspace |
| Cancel selection/drag | Right mouse button |
| Undo / redo | Ctrl+Z / Ctrl+Y; toolbar buttons |
| Hide/show toolbars | Tab; this exposes the cells under the toolbars |
| Toggle grid | Grid in the second toolbar row |
| Save a separate folder pack | Ctrl+S or Save copy |
| Enter/leave playtest | F5 or Playtest; Escape also returns from playtest |
| Close editor and resume original game | F3, Escape or Close |

Actors and items always snap to the original 40-pixel grid. The palette uses
the current tileset and the runtime's registered animals, enemies, items,
player starts and special objects. Scenery and collision are separate: moving
a bush does **not** move the level's collision cells. Use the collision tool to
edit those cells explicitly. Free scenery also accepts fractional X/Y positions
in its inspector. Puddle/water brushes edit gameplay cells, not scenery: place
their visible artwork separately, as in the standalone editor.

The inspector is 320 pixels wide and adjusts its height to its contents. Drag
its green title bar to move it; the **- / +** button folds/unfolds it. Only its
visible rectangle blocks canvas input. The inspector has four tabs:

| Tab | Available properties |
|---|---|
| Level | Display title, timer, texture pack, music path, UFO visibility, puddle destination list/rebuild; XXX weather and bonus controller |
| Object | Position, depth offset; scenery texture ID/frame, sorted/foreground pass and Draw Earlier/Later; animal/enemy facing, animal impatience and item permanence |
| Cell | Raw visible-cell index, terrain, spawner group, direction; XXX help ID |
| Project | Pack/levelset names, level list, blank/duplicate/import, new blank pack and open a full installed pack |

Cell selection ignores all artwork; it never starts an object drag. Object
selection uses opaque sprite pixels and shows the sprite's bounding rectangle.
Use Ctrl+click to cycle overlaps, or **Object → Object list** to search by
slot/name and select a hidden object directly. Switching to Cell clears the
object selection so Delete cannot remove a previously selected sprite.

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
advanced/raw inspectors, CSV inventory, layer controls, zoom/pan, continuous
painting and further project conveniences such as level deletion/reordering.
The canvas shows static authored object
sprites; playtest provides the real animation, bonus-controller and gameplay
presentation.

Validation and continuation details: [editor progress](research/editor/PROGRESS.md).

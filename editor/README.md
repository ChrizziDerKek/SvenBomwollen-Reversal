# Sven Zwo Level Editor

This is the development editor. The original `codexhandoff/data/SvenLevelEditor_m1_fix2` beta remains untouched. The older milestone README files in this directory describe its baseline, not the current behavior.

## Run

Double-click `Start-Editor.cmd` (requires the .NET 8 SDK or a later SDK with .NET 8 desktop targeting support), or run from the repository root:

```powershell
dotnet run --project editor/leveleditor_clean.csproj --configuration Release
```

The executable from a Release build is `editor/bin/Release/net8.0-windows/leveleditor_clean.exe` and requires the .NET 8 Desktop Runtime. Extracted images in the repository's `assets` directory load automatically. Use **Open Assets Folder** for another asset library. Opening a pack adds its own images to the library.

## Pack workflow

1. **Open Pack** opens `.pak` or `.dat`, extracts a private working copy, reads the package XML, and opens its first level. Double-click a project level to switch.
2. Edit, then **Save Pack As**. Choose any new filename, for example `MyLevel.pak`. When the name changes, the editor also renames the archive root and XML descriptor, updates the package name and its own resource paths, retains all levels, and continues editing the renamed copy. The source package stays unchanged. Existing destinations receive a `.bak` backup. All untouched archive metadata and payloads are preserved; the writer reopens and verifies output before replacing it.
3. To add levels, enter a levelset name and level display name on the left, then **Add to Levelset**. An existing set is extended; a new name creates another set. The current tileset and selected level's music are used; the music field can override the resource path. **Save Pack As** writes all staged levels and XML to the archive.
4. For a new package, open a working pack or extracted folder as a resource template, then **New Pack from Template**. Choose a unique filename such as `Custom.pak`. The editor copies resources/dependency metadata, rewrites package-relative resource paths and creates an empty levelset list. Add the displayed blank level (or open/import an existing `.lvl`) to a levelset and save the pack.
5. **Import Level into Project** opens an existing standalone `.lvl` while keeping the pack project. Then use **Add to Levelset** to register it. **Open Level** instead switches to standalone mode. New package resources come from its template; it is not a resource authoring or image-conversion tool.

**Save Replace In Folder** writes only the extracted/staged `.lvl`; **Save Standalone** exports a `.lvl`. Neither writes the archive. Working copies remain under `%TEMP%/SvenEditor` for recovery. Save Pack As is the durable archive operation.

## Controls

- Click/drag to select and move. **V** or **Escape** returns to Select.
- **Ctrl/Alt-click** or right-click opens an overlap picker. **Delete** deletes the selected object; Sven cannot be deleted.
- Arrow keys nudge scenery by 1 pixel, **Shift** by 10. Linked actors and Sven nudge by one gameplay cell. Actors snap to cells; the scenery snap checkbox controls placement and dragging.
- **Ctrl+wheel** zooms (25–400%). Middle-drag or scrollbars navigate the level.
- **Ctrl+Z / Ctrl+Y** undo/redo; **Ctrl+S** saves; **Ctrl+O** opens a pack. Text fields retain normal text-edit shortcuts.
- Scenery placement is one-shot. Actor and tile tools remain active. Choosing an empty palette tab returns to Select.
- Right-click in tile mode clears a tile. Linked actor/item cells must be moved/deleted through their object tools.
- Search the object list by name or slot. The position/depth fields accept ordinary numeric coordinates; Apply Position / Depth updates actor tile links automatically.
- **Draw Earlier / Draw Later** changes the selected scenery object's depth offset (`field24`), without moving object slots. Rendering and overlap selection use the game's depth sorter.
- **Labels / Links**, tiles, and grid overlays are optional. Tile painting turns on the tile overlay. Object and tile raw inspectors are advanced tools; they can create invalid game states if arbitrary unknown fields are changed.

## What the project buttons do

| Button | Action |
| --- | --- |
| New Pack from Template | Creates a separate new package using the open project's graphics, sounds, tilesets and dependencies. Starts with no registered levels and a blank editable level. Add that blank level or an imported level to a levelset, then save the pack. |
| Save Replace In Folder | Replaces the current `.lvl` in its extracted project folder, or its source `.lvl` in standalone mode. For a directly opened pack this is only the temporary working copy; Save Pack As is still required to update the archive. |
| Select / Move Tool | Leaves the current placement/painting tool. Click selects an existing object; dragging moves it. Actors' tile links move with them. |
| Import Level into Project | Opens an existing `.lvl` for editing without closing the pack project. It does not register or save that imported level into the pack yet. |
| Add to Levelset | Copies the current level to a new map entry and adds it to the named levelset in package XML. A new levelset name creates a set. It does not overwrite a level with the same display name. Save Pack As commits it to the archive. |

For a new independent package: Open Pack -> New Pack from Template -> create/import level -> Add to Levelset -> Save Pack As. To edit an already registered level, simply edit it and Save Pack As; Add to Levelset is not needed.

## Animal settings

The left-side **New animals: facing** and **New sheep: initial impatience** controls apply to subsequent placements. Select an existing sheep/chicken/goat/pig and use the matching right-side controls plus **Apply Animal Settings** to edit that object (undoable).

Facing codes are 0 north-east, 1 south-east, 2 south-west, 3 north-west, corresponding to the four vertical frames of the standing sprite. The sheep impatience selector is 0..5, stored as even values 0,2,4,6,8,10 in object field22. This is a discrete stage, not a percentage. The game normally raises it over time and can trigger devil behavior at the following threshold. Special behavior flags and timers remain preserved when editing it.

Sheep/chicken/goat/pig placement now uses explicit constructor-based defaults in `AnimalFactory.cs`, with no source-level record or preexisting animal required. Existing level objects remain unchanged until you edit them. Enemy/item templates are separate and have not all been replaced with constructors.

## Supported logic

Sheep, aliens, Wotan, Lars, Bromse, chickens, goats, pigs, and fixed beer/teddy/bone/artifact objects have placement tools with linked tiles. Tile brushes include collision, puddles, ponds and three random-item spawn groups. The game has room for 16 spawn markers per group. Rebuild Teleporters creates the separate 16-slot destination table from T10 nodes; the list editor allows explicit ordering. The time limit is editable in seconds.

See `../docs/VERIFIED_FORMAT.md` for the corrected binary layout, draw sorting, evidence addresses, and remaining limitations. The user's first custom pack (`test/XS.pak`, screenshot `test/new.png`) has confirmed collision, characters and item spawners in game. The latest rename handling, constructor-based animal defaults and animal-setting controls have regression coverage but still need a new in-game check. Modified packages still need the signature-check bypass described in the repository README; this editor does not generate publisher signatures.

## Validation

```powershell
dotnet build editor/leveleditor_clean.csproj --configuration Release
dotnet run --project editor-checks/EditorChecks.csproj -- .
```

Checks cover the supplied levels, exact unmodified pack roundtrips, preservation of untouched payloads after replacement, new package creation, XML updates, actor movement/deletion, adjacent object boundaries, tile coordinates, teleporter data, spawner capacity, depth changes, and WPF startup/tool switching/undo/redo. Generated packs and an offscreen editor preview are retained in `editor-checks/artifacts` (Git-ignored).

# Sven Clean Level Editor M1 Fix 1

This patch fixes the first usability issues found in M1.

## Fixes

- Static placement is now one-shot:
  - select a static in the sidebar
  - place it once
  - the static palette selection is cleared
  - the editor returns to Select / Move automatically
- Switching palette tabs now synchronizes the active tool with the visibly selected palette item.
- Static palette refresh no longer auto-selects the first static by default.
- Tile overlays are rendered above objects again, so collider/trigger editing remains visible.
- Static scenery is drawn in reverse object-table order to better match the game draw order.
  - This fixes cases like the top-left tree/bush ordering in `tutorial01v01`.
- Object hit-testing now follows the same visual order as rendering:
  - non-static markers first
  - then statics in topmost visual order

## Not changed

- Character placement remains persistent.
- Tile painting remains persistent.
- Ctrl/Alt-click object picker still works.
- Right-click object deletion still works.

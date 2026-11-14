# Sven Clean Level Editor M1 Fix 2

This patch fixes the two issues found after M1 Fix 1.

## Fixes

- Static scenery draw order now follows the object array order again:
  - lower object slots are drawn first
  - higher object slots are drawn later/on top
  - this fixes the bottom-right pond rendering above bushes/grasses in `tutorial01v01`

- Object hit-testing now follows that same visual order.

- Loading an extracted pack XML no longer resets all tile/static sizes to `80x80`.
  - XML tileset entries preserve built-in texture sizes when available.
  - If assets are loaded, the editor reads real image pixel sizes and updates the current tileset metadata.

## Notes

The object array order is now treated as the source of truth for static draw order.
If a specific level still has a mismatch, the next thing to inspect is whether that object uses a special field for layer/depth, but for the known pond/bush/grass case the array order is the correct behavior.

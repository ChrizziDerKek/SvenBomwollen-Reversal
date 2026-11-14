# Next milestones

## Milestone 2: Pack workflow

- Integrate the existing pack archive reader/writer logic.
- Open `.pak` directly.
- Extract to a workspace folder.
- Rebuild `.pak` after saving.
- Add UI for:
  - replace current level in pack
  - add level to existing levelset
  - create new levelset / levelpack entry

## Milestone 3: Unknown tile and item research

- Add tile pattern inspector.
- Group all non-zero / special tile records by full 6-field pattern.
- Show positions and linked object references.
- Use this to confirm dynamic item spawners and alien/enemy spawners.
- Implement fixed item placement after the patterns are fully verified.

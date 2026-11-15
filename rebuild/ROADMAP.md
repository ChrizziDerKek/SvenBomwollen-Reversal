# Roadmap

Agreed order (2026-11-15):

1. **Graphics upscaler experiment with FSR1.** Present the original 800×600
   frame using AMD EASU followed by RCAS. Compare with nearest-neighbor and
   bilinear scaling; check resizing, aspect ratio, input, fullscreen and
   sprite/UI appearance. FSR1 is now the default following user acceptance;
   native presentation remains available with `-Upscaler native`.
2. **Full Sven XXX port.** Preserve original animals, help, effects, player
   roles and multiplayer across all 129 supplied levels. Follow the
   [compatibility audit](research/compatibility/REPORT.md); the adapted XS
   backport is not yet a faithful XXX port.
3. **Implement/integrate the level editor.** Build on the existing editor and
   format research with a coherent play/edit workflow, loose-file packages,
   game-specific properties and preservation of unknown data. Leave the
   archived `SvenLevelEditor_m1_fix2` beta untouched.
4. **Full Sven 004 port plus an old-format converter.** Add the larger world,
   camera, animals, hunter, vehicles, mechanisms, objectives, presentation and
   multiplayer rules. Convert Zwo v6/XXX v7 content into the supported 004
   representation by mapping entities, grid fields, references and assets;
   changing the version byte is insufficient. Preserve originals and report
   conversion differences. Packages from all generations should coexist with
   separate identities and their appropriate rules and scores.

Zwo parity remains the regression baseline. Graphics presentation changes do
not change simulation coordinates or level formats.

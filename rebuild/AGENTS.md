# Rebuild conventions

* Work in `rebuild`. Never modify `rebuild_working_copy`, the user's frozen backup.
* Follow `.clang-format`: Allman braces, tabs, four-column indentation. Empty constructor bodies use `{}`; separate method definitions with a blank line. Lambda opening braces stay on the same line as their signature; keep short lambdas inline.
* Keep braces around nested loop bodies, including an outer loop whose only statement is another loop.
* Use PascalCase for project classes, methods and attributes; use camelCase for parameters and local variables.
* Keep ordinary expressions/calls on one line; parenthesize nested conditional branches so formatting does not split ternary chains. Keep initializer spacing like `{ 1, 1 }`.
* Use `ObjectKind` directly, classification helpers and catalog mappings; never derive array indices or cell codes by subtracting from a kind. Keep scoped state/cell enums at runtime and convert raw values at serialization boundaries.
* Prefer the aliases in `Sven2Bootstrap/Types.h`, including `str`, `fpath`, `bytestream` and numeric aliases.
* Prefer explicit short/readable types. Keep `auto` for unwieldy iterator/container expressions, structured bindings and unnamed lambdas.
* Preserve required SDK/library names, XML keys, ABI export strings and C++ hooks (`main`, `begin`, `end`).
* Leave AMD's `ffx_a.h` and `ffx_fsr1.h` unchanged.
* Objects own their state through `BaseObject`/`BaseCharacter`; `ObjectManager` owns their lifetimes. Register characters through `ObjectCatalog`, not scattered kind numbers.
* Preserve original phases, stable slots and random-call order. Run gameplay regressions after behavior-sensitive changes.
* Sound requests explicitly specify looping. Never infer it from an asset filename.
* In a combined installation, use XXX gameplay, character resources, frame lists and bindings for both v6 Zwo and v7 XXX levels. Select gameplay through GameData.GetGameplayEdition(), never the pack's edition. Keep source-format decoding, scenery, episode identity and spawn-count validation tied to the source pack. Legacy behavior is only an asset-availability fallback when XXX data is absent.
* Keep binary-layout/vtable constants in documented format/ABI definitions. Do not guess another DLL layout or retune original gameplay merely to remove a literal.

* Zwo co-op generates a second runtime player only when two-player mode is selected in the shared XXX runtime. Preserve the authored one-player record validation and source files; keep archival Zwo-only resources single-player. See research/xxx/modes/ZWO_COOP.md.

# Rebuild conventions

* Work in `rebuild`. Never modify `rebuild_working_copy`, the user's frozen backup.
* Follow `.clang-format`: Allman braces, tabs, four-column indentation.
* Use PascalCase for project classes, methods and attributes; use camelCase for parameters and local variables.
* Prefer the aliases in `Sven2Bootstrap/Types.h`, including `str`, `fpath`, `bytestream` and numeric aliases.
* Prefer explicit short/readable types. Keep `auto` for unwieldy iterator/container expressions, structured bindings and unnamed lambdas.
* Preserve required SDK/library names, XML keys, ABI export strings and C++ hooks (`main`, `begin`, `end`).
* Leave AMD's `ffx_a.h` and `ffx_fsr1.h` unchanged.
* Objects own their state through `BaseObject`/`BaseCharacter`; `ObjectManager` owns their lifetimes. Register characters through `ObjectCatalog`, not scattered kind numbers.
* Preserve original phases, stable slots and random-call order. Run gameplay regressions after behavior-sensitive changes.
* Sound requests explicitly specify looping. Never infer it from an asset filename.
* Keep binary-layout/vtable constants in documented format/ABI definitions. Do not guess another DLL layout or retune original gameplay merely to remove a literal.

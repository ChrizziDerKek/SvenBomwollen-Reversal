# Export versions and publisher signatures

The toolbar's **Export version** applies to standalone saves, replace-in-folder saves, and every `.lvl` in an exported pack:

- **Preserve source** is the default and retains the original bytes/layout.
- **Sven Zwo v6** writes the familiar core and removes the XXX extension if present.
- **Sven XXX v7** retains the extension if present, or adds 256 observed default tuples `(0,0,400,2)` when starting from v6.
- **Sven 004 v12** is shown disabled. Its larger grid, 112-byte object records and later data require a dedicated converter.

Export uses a copy and does not convert the working level in place. These are layout exports, not complete game conversions: they do not supply assets, change actor behavior, or remove the second Sven. They need play-testing in the target game. Converting down to v6 discards the extension in that exported copy; keep the source for later XXX edits.

**Verify Pack Signature** checks any `.pak`/`.dat` without loading its levels, including 004. Opened/saved packs also show their on-disk signature status. Signature validity is separate from archive structural validity. Standalone `.lvl` files have no publisher signature.

The checker imports the public key extracted from Sven2.exe and uses .NET DSA/SHA-1 verification. It reverses each CryptoAPI signature integer and public-key integer to the verifier's byte order. Original signatures are retained, and modified content correctly reports invalid. Untouched exports preserve valid signatures byte-for-byte. A new valid publisher signature requires the matching private key, which the user confirmed is unavailable.

Implementation API: [.NET DSA verification](https://learn.microsoft.com/en-us/dotnet/api/system.security.cryptography.dsa?view=net-8.0). Research: [three-game format comparison](../docs/validation/GENERATIONS.md).

# Building QD COOP

## Runtime

The repository contains the Lua mod, native startup supervisor and installer, bridge source, and native C helpers. Build on Windows x64 with the .NET 9 SDK and a MinGW-w64 x64 C compiler.

```powershell
.\Build-Runtime.ps1 -Compiler 'C:\path\to\mingw64\bin\gcc.exe'
```

The build places the bridge and native helpers under `dist/runtime/QDCoop-Direct/`. To assemble a runnable folder, also stage Install.cmd, Uninstall.cmd, Install-Native.ps1, Native-Session.ps1, Lua scripts, licenses, and UE4SS configuration from the repository. Supply the matching Windows x64 UE4SS distribution and the required game-specific Paks separately. The original release pins UE4SS build `e84db30c`. The native helpers resolve its exports and should be revalidated before changing that dependency. Run Install.cmd once to deploy the mod to the original game. The native bootstrap starts Native-Session.ps1, which writes per-run configuration under QDCoop-Direct/Local/native; no isolated game copy or save redirection is used.

## Animation and other asset inputs

The source archive omits cooked game-derived assets, game binaries, captured transforms, and extracted game packages. A runnable build expects five Paks in `QDCoop-Direct/assets/`.

| File | Purpose |
| --- | --- |
| `QDCoop-NativeBody.pak` | Full-body graph, locomotion, retargeted clips, and montages |
| `QDCoop-NativeSpear.pak` | Spear graph and animation resources |
| `QDCoop-GuestTarget.pak` | In-game target proxy for the guest |
| `QDCoop-PlacedItem.pak` | Placeable item actor resources |
| `QDCoop-Skins.pak` | Two soldier meshes rebound to the existing skeleton and five menu portraits |

`tools/animation/bake.cjs <input-directory> <output-directory>` processes locally prepared inputs. The input directory layout is as follows.

- `source-rigs.json` describes the arm-to-body bone mapping and bind rotations.
- `baked-motion.json` contains the target skeleton, local reference transforms, and locomotion clips.
- `downed-motion.json` supplies the downed and recovery clips.
- `source-manifest.json` and `animation-properties.json` describe the sampled animation clips, montages, and their source segments.
- `source/<clip-name>.bin` contains 120 Hz samples with ten little-endian float32 values per mapped bone, ordered translation XYZ, quaternion XYZW, then scale XYZ.

The baker writes local bone frames and a manifest with a reference-pose buffer. Add the included editor-only `QDAuthor` plugin to a UE4.27 authoring project with the compatible skeleton, templates, and compression assets. Run the included `import-all.py` from that editor with `QDCOOP_BAKED_MANIFEST` set to the generated manifest. Cook the generated `/Game/QDCoopBody` content for WindowsNoEditor and pack it at the game's corresponding mount path. `QDAuthor` also contains the spear graph authoring functions.

The proxy and placement source under `tools/blueprints/` can serialize locally supplied UAssetAPI JSON descriptions and package the resulting asset pairs. Each serializer requires UAssetAPI 1.1.0 and the .NET 9 SDK. Each `pack.cjs` accepts the generated asset directory and the output Pak path.

The repository provides the implementation and authoring code. Exact recreation of the five Paks requires the local game-specific inputs and editor templates described here. See [asset provenance](../QDCoop-Direct/THIRD-PARTY-NOTICES.txt) before redistributing generated assets.

For skin binding, canonical reference transforms, and portrait authoring, see [SKINS.md](SKINS.md) and `tools/skins/`.

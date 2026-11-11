# Player appearance implementation

The appearance selector provides Prince, Arjaot (helmet 00, face 02), Ippibi (00, 04), Ujji (00, 06), and Imba (closed helmet 01, face 05). Names and original material paths come from the game's Knight assets. Both players select independently. This is a third-person cosmetic feature; it does not replace the local pawn, first-person arms, gameplay properties, or collision.

## UI and persistence

`skin_catalog.lua` defines stable IDs and explicit asset paths. `skin_menu.lua` adds the selector to the right of the native Multiplayer panel. Portrait textures are rendered from the bound meshes, at the same camera angle and relaxed arms-down pose, with the upper arms opened approximately 13.5 degrees to clear the armour. There are no preview NPCs, animation drivers, or per-frame scene captures. Chinese uses 王子 / 选择; other languages use Prince / Select. NPC names retain their original spelling.

`skin_selection.lua` separates the browsed ID from the selected ID. Only Select commits a change; reopening resets the preview to the selection. The preference is stored beside the current IPC directory as `player-skin.txt`, under `QDCoop-Direct/Local/native` for native startup. A failed write keeps the old selection; a backup preference can recover an interrupted replacement. The preference is per local installation rather than per character save. Preserve the Local directory when updating if you want to retain this preference.

## Transport and presentation

An optional `Y|1|<skin-id>` line carries the cosmetic ID in the existing state frame. The `S` line remains the save-slot tag. Unknown skin IDs or unsupported cosmetic tag versions use Prince without rejecting gameplay state; the old parser ignores the extra line. Interpolation copies skin as discrete state. New versions still require matching build fingerprints for ordinary connections.

`skin_visual.lua` resolves only catalog assets and changes the existing display mesh when the ID or display component changes. Native and legacy display paths retain the original Prince skeleton. Material overrides are cleared, and the face material is assigned using the named `QD_Face` slot rather than a hard-coded index. The helmet is part of the mesh and weighted to `head_x`. No original NPC actor or AI is spawned. Native presentation uses the existing ABP_QDBody graph and clips; it does not add a second live skeleton to drive the skin. A missing skin mesh falls back to Prince.

## Offline binding

The original Knight skeleton has 58 bones and cannot directly play the 110-bone Prince animation setup. `tools/skins/bind_models.py` transfers the Knight mesh into the Prince reference shape, remaps weights by semantic bone names, preserves torso spacing with intermediate anchors, and joins one of the two original helmet meshes. It exports two skeletal meshes, five portraits, and an inspectable Blender scene. The three helmet-00 appearances share geometry and differ by original face material.

An identical hierarchy is not enough: the FBX round trip changes local reference axes. `QDSkinAuthor` checks every bone and parent by name, restores the mesh's local reference transforms from the canonical Prince skeleton, and rebuilds inverse bind matrices. This happens in the editor only. The import script verifies that the shared skeleton asset has not changed. Only `/Game/QDCoopSkins/` is packaged; neither the shared skeleton nor placeholder materials are included. The runtime loads original Knight materials from the game.

## Authoring

Use Blender 5.2 and UE 4.27.2. Set `QDCOOP_SKIN_INPUTS` to a local directory with legally obtained game inputs and `QDCOOP_SKIN_WORK` to a separate output directory. No extracted meshes, textures, animation samples, editor templates, or cooked game-derived assets are included in this source archive.

Required input layout:

```text
target-reference.json             # {"bones":[{"name", "parent", "p", "q", "s"}, ...]}
Knight/
  Knight_mesh.glb
  Helmet_00_SM.glb
  Helmet_01_SM.glb
  Knight_Body_00_TX.png
  Knight_Helmet_00_TX.png
  Knight_Face_02_TX.png
  Knight_Face_04_TX.png
  Knight_Face_05_TX.png
  Knight_Face_06_TX.png
Player/
  PlayerArm_00.png
  PlayerArmor_00_TX.png
  Player2026/
    Player_FullChar_2026.glb
    PlayerFace_26_tx.png
    playerhair26_tx.png
```

Reference transforms use the original parent indices, centimetres, and UE quaternion XYZW. They must match the actual skeleton used by QDCoop-NativeBody; do not substitute a similarly named skeleton. The original indexed skeleton/editor template must be prepared separately as described in BUILD.md.

1. Run Blender in background mode with `--python tools/skins/bind_models.py`.
2. Build the editor-only `tools/skins/QDSkinAuthor` plugin with UE 4.27's BuildPlugin command and install it into the authoring project. Enable PythonScriptPlugin and EditorScriptingUtilities.
3. Place the compatible authoring project at `$QDCOOP_SKIN_WORK/editor-project`, with its original skeleton at `/Game/Player/Assets/FPModel/Player2026/Player_FullChar_2026_Skeleton`. Run `tools/skins/import-skins.py` in that editor. It imports FBX, normalizes mesh bind transforms, sets original material references, and imports portraits.
4. Cook `/Game/QDCoopSkins` for WindowsNoEditor. Use `tools/skins/pack.cjs` to include only those cooked assets. Copy the Pak into the player release's `QDCoop-Direct/assets/` before installation.

Inspect in-game deformation, weapon grips, armour intersections, prone/recovery poses, and material assignments after changing weights or bone mappings. Skeleton identity and finite bone transforms alone cannot detect a wrongly oriented bind pose.

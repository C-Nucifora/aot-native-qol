# Parked experiments

Work is stopped at the user's request. This branch preserves source for review;
it is not the restored played build and is not ready to merge or install.

## Native SMAA

The current candidate accepts the observed RGBA typeless resource / sRGB view
pair as well as the original BGRA pair. It verifies the initialized empty UI
arena at 440 bytes and inserts the deferred pass before UI drawing.
Actual draw callbacks and merged command lists were observed in visible 720p
play. At 1440p callbacks also succeeded while the world was black. A cold run
with the native AA hooks disabled still had the black background, so AA was not
a necessary cause. That does not establish the renderer is correct at 1440p.
The latest report also described a missing title/main-menu background.

The C adapter, renderer source, HLSL and licensed SMAA/CAS sources are retained.
CAS was not enabled in the deployed candidate. Preserve THIRD_PARTY_NOTICES.txt.
Compiled shader headers, private native guard headers, game binaries and local
runtime captures are excluded. No experimental installation script is provided.

Generate private AA signature headers locally with the main tree's
`tools/generate_component_guards.py`, using the supported image and each of
`experimental/native-aa/src/private-inputs.json` and `arena-inputs.json`.
Shader generation tools are preserved under `native-aa/tools`; they need an
explicit external compiler and a build-only Wine prefix. The original local
Windows fixture and complete deployment harness remain private. End-to-end
portable experimental builds have not been established.

## HD menu headings

The owned 2x atlas adapter validates the native immutable BC3 typeless resource
and its paired typed SRV. Generation and actual heading draws were observed.
It had not generated or drawn anything in the captured black gameplay launch.
Original-game-derived atlases and the generated `guards.h` are omitted.
`private-input-contract.json` records the exact local input requirements.
The original atlas generation/deployment workflow is not published here.

## Additional resolution bound

The core manifest on this branch also includes the missing bound at RVA
0x41b6a0 and the full accessor guard. Indices 11/12 otherwise fall back to the
640x480 aspect classification. Native-byte checks confirmed that defect.
Use of this helper on the black rendering path was not established, and the
user still reported a missing background after installing the correction.
It is therefore not presented as a black-screen fix.

## Remaining issue

A native color/depth compatibility check can reject a render-target bind.
The required live native-wrapper fields were not captured before the game
exited, so a mismatch remains a hypothesis. The restored played setup uses
main's earlier core, external vkBasalt SMAA and the prior display settings.
Further debugging and runtime testing are stopped. No user saves or account
files are included in this branch.

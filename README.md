# AOT native QoL

Native resolution and FPS settings for Attack on Titan / A.O.T. Wings of Freedom.
This source snapshot preserves the setup played on 12 November 2026 before the
later native-AA and HD-menu experiments. It is version-specific, not a finished
installer or a broadly tested release.

The core includes seven FPS choices, native resolution menu extensions, menu
navigation and animation timing fixes, verified controller-camera rate correction,
a focus-pause watchdog correction, and the high-FPS nape cut-in countdown fix.
FPS choices are 60, 90, 120, 144, 165, 240 and Unlimited. Full gameplay timing
coverage has not been established. Raw mouse input is not normalized.

The nape sequence was observed progressing and finishing during a user-played
Unlimited-FPS run. That verifies the reported stall case, not every mission.
The later native SMAA, HD menu adapter and extra resolution-bound correction are
kept off main and belong to the experimental draft branch.

## Build

Requires Python 3, GNU objdump, Clang targeting Windows x64 MSVC, llvm-dlltool,
llvm-readobj and lld-link. Asset generation also uses Pillow and NumPy.
No build command installs files or launches the game.

```sh
sh tools/build_core.sh /path/to/supported-unpacked.exe /path/to/generated-seven-value-assets
sh tools/check_portable.sh
```

Output: `build/core/aot-native-fps-nape-fix.asi`.
The required private image SHA256 is
`0a2d9e152db3bfe298017029804fe172d2a20bc51573feee874040ca3b1684d7`.
Preparing that image remains an external prerequisite; no unpacker is distributed.
See [build and rollback instructions](docs/build-and-manual-rollback.md),
[local FPS assets](docs/local-seven-value-assets.md),
[local resolution assets](docs/local-resolution-assets.md), and
[separate baseline modules](components/README.md).

The played setup used external vkBasalt SMAA without CAS, the FPS counter,
and the separate focus-window module. Example configuration is in `config`.
Third-party loaders, Proton, DXVK, vkBasalt and Discord RPC are not bundled.

## Status and distribution

Only mod source, local generators and documentation are published. No game
executable, extracted game artwork, generated game-signature headers, saves,
account data or runtime memory captures are included. Build outputs stay ignored.
No license has been selected for original project code. Third-party components
retain their own notices. See [remaining build and release gaps](docs/release-gaps.md).

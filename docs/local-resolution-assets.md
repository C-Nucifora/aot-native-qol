# Generate native resolution labels locally

The current core reads four additional game-derived sidecars from the folder
`aot-native-assets` beside its ASI: `UIOption.g1t`, `UIOptionLayout.bin`,
`CmnParts.g1t` and `CmnPartsLayout.bin`. The resolution pair is loaded by the
startup bridge and the CmnParts pair by the FPS resource stage. No other private
atlas sidecar is read by this core. The original game archives/resources and ASI
loader remain runtime prerequisites, not bundled dependencies.

This workflow generates the UIOption pair. The seven-value workflow generates
CmnParts. Together these reproduce the complete *additional asset sidecar*
closure for the current resolution/FPS core. They do not generate the game,
settings/saves, loader, HD UI, native AA or focus-window modules.

From the source root, with Python, Pillow13.0.0 and NumPy2.5.4:

```sh
python3 tools/assets/build_resolution_values.py \
  --archive /path/to/local/LINKDATA_EU_A.BIN \
  --output build/private/resolution-values
```

Only slots1702/1703 are extracted using the unchanged accepted bounded reader.
Full5352-entry table/extent validation,32MiB selected limits,32768-byte bounded
chunk decode and exact stream/terminator checks apply. Exact stock texture/layout
hashes are required before any output is created. The original donor composition
build function is unchanged: it takes normal numeral masks from stock UIOption,
translates them at integer offsets, and retains the accepted approximate selected
Gaussian glow. No replacement font, new glyph or extra resolution is introduced.
A separate Font.g1t is not used by this resolution path.

Alternatively supply explicitly extracted donor files:

```sh
python3 tools/assets/build_resolution_values.py \
  --texture /path/to/private/UIOption.g1t \
  --layout /path/to/private/UIOptionLayout.bin \
  --output build/private/resolution-from-files
```

Optional `--donor-tiles` accepts exactly four previously generated private raw BC3
files in this order: 2560-normal,2560-selected,3840-normal,3840-selected. All four
hashes/order must match the supported accepted labels before output creation.
Without that option, the tiles are generated from stock donors locally. The
original packer then preserves all61 stock layout records and original BC3 blocks,
appending the four label records for1440p/2160p. Existing resolutions stay intact.

Outputs beneath the requested new private directory:

- inputs/: locally extracted original donor bytes, private.
- tiles/: four private BC3 files and local review/validation outputs.
- aot-native-assets/UIOption.g1t SHA256
  181ca09afe62d8c5165d1dc7f741e4099c82650fb29ea26f54c8decbd69078ce
- aot-native-assets/UIOptionLayout.bin SHA256
  a5eb6855538f9d2ea69513648f2b0aca893fd0574f0a2fac1d31f1537100bc3d

All original alpha/silhouette/BC3 roundtrip checks run, and generated tile and
packed-file hashes must match accepted outputs. On output mismatch, keep files
private for diagnosis; no installation occurs. Existing output directories are
refused. In-tree outputs are restricted to ignored build/ or private/ descendants;
external private output paths are also allowed. Missing/unsupported donors fail
before any output directory is created. No source input is overwritten.

Source closure: build_resolution_values.py, resolution_donors.py, accepted
assets/glyph_helpers.py, original tools/build_resolution_assets.py and its
asset_format.py helper. Pillow/NumPy are not vendored. Their notices and exact
versions are described in local-seven-value-assets.md. All game-derived texture,
layout, label, preview, manifest and header outputs are excluded from the source
allowlist. Do not recursively archive build/ or private/.

The core ASI build still requires the separately prepared supported unpacked
executable. No unpacker, decrypted executable, original archive or glyph data is
distributed. This workflow does not implement that remaining prerequisite.

# Generate seven FPS values locally

This workflow extracts only five supported assets from a user-supplied local
archive, validates their exact SHA256 identities, and reproduces the accepted
seven-value menu atlas. No game font, glyph mask, texture, layout or generated
preview is distributed. No downloads, installation or game/process access occurs.

Dependencies in addition to Python: Pillow13.0.0 and NumPy2.5.4 were tested.
Pillow's DDS/BC3 encoder and NumPy palette search affect output bytes. Other
versions are not claimed byte-reproducible; the tool rejects mismatching final
hashes. Install dependencies through your normal environment management.
No package manager is invoked by this workflow. Installed Pillow13.0.0 declares MIT-CMU. Installed NumPy2.5.4 declares
BSD-3-Clause AND 0BSD AND MIT AND Zlib AND CC0-1.0. Retain their complete upstream
distribution notices if you
separately redistribute dependencies. Neither dependency is vendored here.

From the source root:

```sh
python3 tools/assets/build_values.py \
  --archive /path/to/local/LINKDATA_EU_A.BIN \
  --output build/private/seven-values
```

The archive reader uses the original format interpretation in a bounded
`glyph_helpers.read_slot` implementation.
Only slots1621Font,1622CmnParts,1623CmnPartsLayout,1702UIOption and1703UIOptionLayout
are read. The entire5352-entry table and every data extent are checked. Selected stored
and declared sizes are capped32MiB; each zlib call allows at most32768 bytes
plus one overflow sentinel, and must reach exact stream EOF without unused or
unconsumed compressed data. Chunks must fit the remaining stored extent and
produce exactly the native32768-byte unit or final remaining length. A final
zero chunk terminator must consume the stored extent exactly. All five asset hashes
must match `build_fps_assets.EXPECTED` before an output directory is created.
The full archive is never unpacked. A missing/unsupported input fails before
writing output. An existing output directory is refused to preserve prior files.

Alternatively, use five already extracted files, named `Font.g1t`,
`CmnParts.g1t`, `CmnPartsLayout.bin`, `UIOption.g1t`, `UIOptionLayout.bin`:

```sh
python3 tools/assets/build_values.py \
  --inputs /path/to/private/extracted-files \
  --font /path/to/private/Font.g1t \
  --output build/private/seven-values-from-files
```

`--font` is optional when Font.g1t is in the input directory. An explicitly
supplied font still requires the exact supported game Font.g1t hash; replacement
fonts are refused. All generated files must remain private. Inside this source
tree output is restricted to ignored build/ or private/ paths; external private
output directories may also be supplied explicitly.

Expected final files:

- CmnParts.g1t SHA256
  46ab9e4274c50c382285a8579783d3149f124ed9b69264f02083fb93032ae45c
- CmnPartsLayout.bin SHA256
  a8ffad74ab3d5e47a5e1e16490b2919deb8c9e04b132c67c59eb0c9944e728d5

The local manifest records all21 dim/normal/focused tiles in target order
60/90/120/144/165/240/Unlimited,276 layout records and preserved original blocks.
Original BC3 round-trip, independent alpha decoder, packing, original-record
preservation and silhouette checks execute during generation. Unlimited retains
the accepted tight190px original-glyph arrangement and approximate clipped side
halo; this workflow does not change those visuals or invent new glyphs.
Local PNG/HTML/BC3/G1T/layout/manifest outputs remain excluded from source archives.

Use the generated directory directly for the core build:

```sh
sh tools/build_core.sh /path/to/private/supported-unpacked.exe \
  "$PWD/build/private/seven-values"
```

The supported unpacked executable remains a separate prerequisite. This workflow
does not unpack the executable or distribute it. Resolution-label asset generation and donor inputs are documented separately
in local-resolution-assets.md. No AA,
HD UI, loader or runtime installation is included here.

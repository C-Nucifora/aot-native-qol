# Exact core dependency allowlist

Build executables on PATH, no automatic downloads:
- sh, a POSIX shell, and mkdir, used by tools/build_core.sh.
- Python 3 standard library only for core build generators. No pip packages.
  Tested Python3.14.8. Python software uses the PSF license and bundled notices.
- GNU objdump, tested Binutils2.47, for local instruction audit. Binutils uses
  GPL licensing; no executable is bundled or linked into the ASI.
- clang, llvm-dlltool, llvm-readobj and lld-link, tested LLVM/LLD23.1.4. LLVM uses
  Apache2.0 with LLVM exceptions and upstream notices. Tools are not bundled.

Included source inputs:
- src/ and include/ authored core code, exact reviewed source bytes.
- runtime/{native_backend.c,native_backend.h,runtime.c,runtime.h}, original
  reviewed runtime source, with exact hashes enforced before compilation.
- tools/{build_core.sh,runtime_manifest.py,audit.py,resolution_manifest.py,
  bridge_manifest.py,fps_manifest.py,timing_manifest.py,nape_manifest.py,
  source_replay_manifest.py}.
- Optional resolution-asset packer tools/build_resolution_assets.py and its
  only local helper asset_format.py. These use Python standard library only.

Explicit private local inputs:
- Supported original-layout unpacked executable passed as first build argument.
- Seven-value asset directory passed as second build argument, containing
  CmnParts.g1t and CmnPartsLayout.bin. Exact accepted hashes are enforced.
- Resolution-asset packing additionally requires stock graphics texture/layout
  and four original-glyph-derived raw BC3 label tiles, all explicit CLI paths.
  Their bounded extraction, donor composition and packing workflow is documented
  in local-resolution-assets.md.

Generated private build-only dependencies:
- guards.h, asset_guards.h, resolution_manifest.h, bridge_guards.h,
  aot_fps_menu_guards.h, fps_stage_guards.h, camera-private-guards.h,
  repeat-private-guards.h, ui-clock-private-guards.h, nape-private-guards.h,
  fps_session_private_guards.h, source-replay-private.h and local audit JSON. Generated under build/core.
- kernel32.def/import library, object files, PE-check report and ASI.

Runtime imports are listed exactly in build_core.sh's kernel32.def. No Windows
SDK/import library or runtime DLL is bundled. The game and compatible ASI loader
are external prerequisites; loader redistribution/licensing is outside this tree.
Native runtime assets must be generated locally and stay outside source archives.
AA shaders, SMAA/CAS libraries, HD UI and shader compilers are excluded.
Optional local seven-value asset generation adds Pillow13.0.0 and NumPy2.5.4;
see local-seven-value-assets.md. The core compiler itself uses neither.
Its exact original generator closure is tools/assets/{build_values.py,
build_seven_value_assets.py,build_six_value_assets.py,build_fps_assets.py,
glyph_helpers.py}, with only Python standard library, Pillow and NumPy imports.
Resolution generation adds assets/{build_resolution_values.py,resolution_donors.py}
and reuses glyph_helpers.py plus tools/{build_resolution_assets.py,asset_format.py}.

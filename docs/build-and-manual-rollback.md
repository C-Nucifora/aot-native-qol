# Local build and manual deployment notes

Source snapshot. Project licensing and portable image preparation remain unresolved.
The reported nape stall passed a user-played retest on 12 November 2026; broader gameplay validation is incomplete.
These are manual instructions, not an installer. Nothing in the source build
launches the game, modifies Steam, writes saves or installs files.

## Build

From the extracted source root:

```sh
sh tools/check_portable.sh
python3 tools/assets/build_values.py \
  --archive /path/to/private/LINKDATA_EU_A.BIN \
  --output build/private/seven-values
python3 tools/assets/build_resolution_values.py \
  --archive /path/to/private/LINKDATA_EU_A.BIN \
  --output build/private/resolution-values
sh tools/build_core.sh /path/to/private/supported-unpacked.exe \
  "$PWD/build/private/seven-values"
```

See unpacked-image-prerequisite.md for the exact required executable input and
remaining preparation gap. Dependency versions and asset hashes are documented
in dependencies.md, local-seven-value-assets.md and local-resolution-assets.md.
Original-font/glyph-derived outputs stay private and must not be redistributed.

## Manual installation, only after review and a closed-game checkpoint

1. Quit normally. Back up the whole game, Proton prefix/registry and saves before
   modifying it. Record hashes and whether each prospective destination existed.
   Keep that backup outside loader-scanned game directories. Also preserve the
   latest aot-native-fps.cfg, launch options, RPC and unrelated mod configurations.
2. This core requires an already validated compatible ASI loader and startup
   ordering. The tested loader configuration deferred loading out of DllMain with
   DontLoadFromDllMain=1 and LoadFromAPI=GetStartupInfoW under [GlobalSets]. No loader
   is bundled. Audit all effective loader INIs before changing anything; don't
   overwrite an unrelated configuration. A clean loader setup is not certified by
   these notes. Runtime checks should refuse unsupported/conflicting setups.
3. Check for overlapping resolution modifications. In the tested existing SUWSF
   setup, its three resolution-table rewrites were disabled for this core while
   unrelated settings were preserved. Do not stack both table modifications.
   No global Proton/Wine/compositor settings are needed by this package build.
4. Stage outside the game first. The compiled ASI is
   build/core/aot-native-fps-nape-fix.asi. For the established setup its deployment
   name is aot-native-resolution-candidate.asi. Replace that one core module only;
   never co-load old/new copies under different .asi names or leave backups ending
   in .asi in a scanned directory.
5. The ASI needs four sidecars in aot-native-assets beside it. Use the new local
   resolution-values/aot-native-assets/{UIOption.g1t,UIOptionLayout.bin} and
   seven-values/{CmnParts.g1t,CmnPartsLayout.bin}. Preserve/record every preexisting
   file and its hash, then manually replace only those four destination files.
   Verify the destination hashes against the generated local files before launch.
6. Preserve existing aot-native-fps.cfg and saves. Do not copy an experiment config
   from another setup. A missing config defaults to stock60. Higher values remain
   opt-in; this draft does not certify all gameplay timing at every target.
   Keep unrelated RPC, Steam options, AA, counter and other module files unchanged.
7. On the user's next launch inspect aot-fps-session.log for a NEW LAUNCH and actual
   activation/refusal. Menu entry alone doesn't prove correct rendering/timing.
   Nape/cut-in progression, pause/resume and mission behavior need user validation.

Focus-window, native AA and HD UI modules are EXCLUDED from this core package.
Their separate reviews/artifacts do not authorize silently merging them here.
Existing independently installed modules must be preserved and assessed separately.

## Manual rollback

Quit normally before restoring files. Preserve a fresh copy of current config and
saves so progress/settings made after installation are not lost. Restore the prior
core ASI and exactly the four prior sidecars from the per-file backup, checking
hashes. If a file did not exist before, remove only that newly introduced file;
never delete the whole assets directory or unrelated mods. Restore loader/SUWSF
settings only if you changed them and recorded their prior values. Keep the user's
latest FPS config by default; do not silently restore stale saves or registry.
If merely disabling this core, move its ASI out of every loader scan directory;
renaming/keeping another .asi copy in the same directory is not a rollback.
Do not launch the private analysis EXE at any point.

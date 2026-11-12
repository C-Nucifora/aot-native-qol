# Separate baseline modules

`focus` preserves the game window on focus loss. `observer` is the getter-only
AA diagnostic that was present in the played setup. It does not perform AA.
Neither module is part of the core build.

Generate private signature headers from the same supported local image as the core:

```sh
python3 tools/generate_component_guards.py /path/to/supported-unpacked.exe components/focus/private-inputs.json
python3 tools/generate_component_guards.py /path/to/supported-unpacked.exe components/observer/private-inputs.json
sh components/focus/build.sh
```

The observer source is retained for reproducibility of the local setup. Its old
private audit/build harness is not bundled. It requires a freestanding Windows
x64 build with kernel32 imports. Do not combine this observer with native SMAA,
because they own the same call site. No generated guards or ASIs are published.

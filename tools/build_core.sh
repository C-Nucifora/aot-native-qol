#!/bin/sh
# Native resolution+seven-value session menu candidate; never overwrites installed/frozen ASI.
set -eu
cd "$(dirname "$0")/.."
: "${1:?Usage: build_fps_timing_ui.sh private-unpacked.exe private-v3-seven-values-assets}"
: "${2:?Private FPS artwork required}"
out=build/core
mkdir -p "$out/generated" "$out/runtime"
python3 tools/runtime_manifest.py "$1" "$out/runtime/fps_session_private_guards.h"
python3 - runtime "$out/runtime" <<'PYRUNTIME'
from pathlib import Path
import hashlib,sys,shutil
src,out=map(Path,sys.argv[1:])
expected={'native_backend.h': 'e5da5b508fb57af2dc73ba688aac2d9cc1c88541e55bc95b10221d604feb8d9d', 'runtime.h': 'ea0c651249c29b482964513acf31569aaa93c28c4a0405e0cbb1bb1b3cdab011', 'native_backend.c': '3c5b08bae03dd4198ef7d9d6af46dd800f82c6fef071e7191fa1ac7eb1db5f26', 'runtime.c': '152700eaea44db482c1fcea85efc7dbb4727c5824571554530cc235b3d0aa792'}
for name,sha in expected.items():
 if hashlib.sha256((src/name).read_bytes()).hexdigest()!=sha:raise SystemExit('Reviewed runtime input drift: '+name)
for name in ('native_backend.c','native_backend.h','runtime.c','runtime.h'):
 shutil.copyfile(src/name,out/('fps_native_backend.c' if name=='native_backend.c' else name))
if hashlib.sha256((out/'fps_session_private_guards.h').read_bytes()).hexdigest()!='5047e9189cf022584c1876fe35723d5f4314101177b25b6fe7e579e9902f955d':raise SystemExit('Generated runtime guard mismatch')
print('Frozen tested session runtime copied unchanged into ignored build')
PYRUNTIME
python3 tools/audit.py "$1" --output "$out/generated"
python3 tools/bridge_manifest.py "$1" --output "$out/generated"
python3 tools/fps_manifest.py "$1" --assets "$2" --output "$out/generated"
python3 tools/timing_manifest.py "$1" --output "$out/generated"
python3 tools/nape_manifest.py "$1" "$out/generated"
python3 tools/source_replay_manifest.py "$1" "$out/generated"
# Identical kernel imports to the separate stage, plus loader ABI lookup.
cat > "$out/kernel32.def" <<'IMPORTS'
LIBRARY KERNEL32.dll
EXPORTS
GetModuleHandleW
VirtualProtect
VirtualAlloc
VirtualFree
GetCurrentProcess
FlushInstructionCache
OutputDebugStringA
DisableThreadLibraryCalls
CreateFileW
ReadFile
WriteFile
FlushFileBuffers
CloseHandle
DeleteFileW
MoveFileExW
GetLastError
GetModuleFileNameW
GetModuleHandleExW
GetCurrentThreadId
GetProcAddress
TerminateProcess
GetTickCount64
WaitForSingleObject
VirtualQuery
CreateThread
Sleep
IMPORTS
llvm-dlltool -m i386:x86-64 -d "$out/kernel32.def" -l "$out/kernel32.lib"
objects=""
for unit in asi patch resolution resolution_runtime settings settings_io sidecar native_assets owned_files startup bridge native_fps_state fps_menu fps_stage fps_session fps_commit repeat_adapter camera_rate camera_hook timing_normalization ui_clock ui_clock_provider ui_clock_hook nape_hook; do
 clang --target=x86_64-pc-windows-msvc -std=c11 -O2 -Wall -Wextra -Werror \
  -ffreestanding -fno-builtin -fno-stack-protector -DAOT_RESOLUTION_CANDIDATE -DAOT_FPS_CANDIDATE -DAOT_FPS_SESSION -DAOT_FPS_TIMING -DAOT_UI_CLOCK -DAOT_NAPE_FIX \
  -Iinclude -I"$out/generated" -I"$out/runtime" -c "src/$unit.c" -o "$out/$unit.obj"
 objects="$objects $out/$unit.obj"
done
clang --target=x86_64-pc-windows-msvc -std=c11 -O2 -Wall -Wextra -Werror \
 -ffreestanding -fno-builtin -fno-stack-protector -I"$out/runtime" -c "$out/runtime/runtime.c" -o "$out/runtime.obj"
objects="$objects $out/runtime.obj"
# Generated paths contain no spaces; no user-controlled shell expansion here.
lld-link /dll /entry:DllMain /nodefaultlib /machine:x64 /dynamicbase /nxcompat \
 /out:"$out/aot-native-fps-nape-fix.asi" $objects "$out/kernel32.lib"
llvm-readobj --file-headers --coff-imports --coff-exports "$out/aot-native-fps-nape-fix.asi" > "$out/pe-check.txt"
python3 - <<'PY'
from pathlib import Path
import hashlib
s=Path('build/core/pe-check.txt').read_text()
for name in ('IMAGE_FILE_MACHINE_AMD64','InitializeASI','TerminateProcess','GetProcAddress','GetTickCount64','WaitForSingleObject','VirtualQuery','CreateThread'):
 if name not in s:raise SystemExit('Missing candidate property: '+name)
print('Native FPS SESSION menu built DISTINCTLY. No hotkey or expiry. NOT INSTALLED.')
PY

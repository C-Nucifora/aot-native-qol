#!/bin/sh
set -eu
cd "$(dirname "$0")"
cat > kernel32.def <<'IMPORTS'
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
GetCurrentThreadId
GetModuleHandleExW
GetProcAddress
GetEnvironmentVariableW
TerminateProcess
IMPORTS
llvm-dlltool -m i386:x86-64 -d kernel32.def -l kernel32.lib
for unit in focus_asi patch; do
    clang --target=x86_64-pc-windows-msvc -std=c11 -O2 -Wall -Wextra -Werror \
      -ffreestanding -fno-builtin -fno-stack-protector -c "$unit.c" -o "$unit.obj"
done
lld-link /dll /entry:DllMain /nodefaultlib /machine:x64 /dynamicbase /nxcompat \
    /out:aot-focus-keep-window.asi focus_asi.obj patch.obj kernel32.lib
llvm-readobj --file-headers --coff-imports --coff-exports aot-focus-keep-window.asi > pe-check.txt
python3 - <<'PY'
from pathlib import Path
s=Path('pe-check.txt').read_text()
for x in ('IMAGE_FILE_MACHINE_AMD64','InitializeASI','AotFocusStatus','AotFocusRollbackCold','KERNEL32.dll','GetModuleHandleExW'):
    assert x in s,x
assert 'USER32.dll' not in s
print('AMD64 focus ASI built and exports/imports verified; NOT INSTALLED')
PY

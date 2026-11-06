#!/usr/bin/env bash
set -euo pipefail

# UTN developer build/run helper for MSYS2 MINGW64.
# Keeps generated UI resources and the runnable install tree in sync.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

cmake -S . -B build -G Ninja -DCMAKE_INSTALL_PREFIX=install
ninja -C build
cmake --build build --target install

# Windows development builds can leave stale copies in the install tree.
if [[ -f build/UTN-app.exe ]]; then
    cp -f build/UTN-app.exe build/install/bin/UTN-app.exe
fi

if [[ -f build/UTN.exe ]]; then
    cp -f build/UTN.exe build/install/bin/UTN.exe
fi

if [[ -f build/resources-templates/toolbar.ini ]]; then
    cp -f build/resources-templates/toolbar.ini build/install/share/xournalpp/ui/toolbar.ini
fi

if [[ -f build/resources-templates/pagetemplates.ini ]]; then
    cp -f build/resources-templates/pagetemplates.ini build/install/share/xournalpp/ui/pagetemplates.ini
fi

exec ./build/install/bin/xournalpp

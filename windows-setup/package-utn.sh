#!/usr/bin/env bash
# UTN Windows beta: collect runtime files, then build installer and portable ZIP.
set -euo pipefail
build_dir=$(cd "${1:?Usage: package-utn.sh BUILD_DIR}" && pwd)
script_dir=$(cd "$(dirname "$0")" && pwd)
prefix=${MSYSTEM_PREFIX:-/mingw64}
version=0.2.0-beta
setup_dir="$build_dir/utn-dist"
output_dir="$build_dir/utn-release"
[[ -f "$build_dir/xournalpp.exe" ]] || { echo 'Build UTN first.' >&2; exit 1; }
# These are dedicated packaging directories, never the installed application.
rm -rf "$setup_dir" "$output_dir"
mkdir -p "$setup_dir/lib" "$output_dir"
cmake --install "$build_dir" --prefix "$setup_dir"
cp "$setup_dir/bin/xournalpp-wrapper.exe" "$setup_dir/bin/UTN.exe"

# GTK modules and Lua plugins load libraries that do not appear in the main EXE imports.
cp -r "$prefix/lib/gdk-pixbuf-2.0" "$setup_dir/lib/"
cp -r "$prefix/lib/girepository-1.0" "$setup_dir/lib/"
for folder in icons glib-2.0 poppler gtksourceview-4 lua licenses; do
    cp -r "$prefix/share/$folder" "$setup_dir/share/"
done
cp -r "$prefix/lib/lua" "$setup_dir/lib/"
cp "$prefix/bin/gspawn-win64-helper.exe" "$setup_dir/bin/"
cp "$prefix/bin/gspawn-win64-helper-console.exe" "$setup_dir/bin/"
cp "$prefix/bin/gdbus.exe" "$setup_dir/bin/"
cp "$prefix/bin/libgirepository-"*.dll "$setup_dir/bin/"
cp "$prefix/bin/libqpdf"*.dll "$setup_dir/bin/"

# Walk imports for executables and dynamically loaded modules. Fail on unresolved imports.
declare -A visited
mapfile -d '' queue < <(find "$setup_dir" -type f \( -name '*.exe' -o -name '*.dll' \) -print0)
for ((i=0; i<${#queue[@]}; i++)); do
    file=${queue[i]}
    [[ -v 'visited[$file]' ]] && continue
    visited["$file"]=1
    imports=$(ldd "$file")
    if [[ "$imports" == *'not found'* ]]; then
        echo "Unresolved dependency in $file: $imports" >&2
        exit 1
    fi
    while IFS= read -r dependency; do
        [[ -f "$dependency" ]] || continue
        target="$setup_dir/bin/$(basename "$dependency")"
        if [[ ! -f "$target" ]]; then
            cp "$dependency" "$target"
            queue+=("$target")
        fi
    done < <(printf '%s\n' "$imports" | sed -n "s|.*=> \($prefix/.*\.dll\) (.*|\1|p")
done

# Runtime relocates the cache into UTN's user folder, so installs need no MSYS2 paths.
mapfile -d '' loaders < <(find "$setup_dir/lib/gdk-pixbuf-2.0" -name '*.dll' -print0)
[[ ${#loaders[@]} -gt 0 ]] || { echo 'Missing pixbuf loaders.' >&2; exit 1; }
"$prefix/bin/gdk-pixbuf-query-loaders.exe" "${loaders[@]}" > "$setup_dir/share/utn-loaders.cache.raw"
awk '/\.dll"[[:space:]]*$/ { gsub(/\\/, "/"); gsub(/\/+/, "/"); sub(/^".*\/lib\/gdk-pixbuf/, "\"@UTN_ROOT@/lib/gdk-pixbuf") } { print }' "$setup_dir/share/utn-loaders.cache.raw" > "$setup_dir/share/utn-loaders.cache.in"
grep -q '@UTN_ROOT@' "$setup_dir/share/utn-loaders.cache.in" || { cat "$setup_dir/share/utn-loaders.cache.raw"; echo 'Could not relocate pixbuf loaders.' >&2; exit 1; }
rm "$setup_dir/share/utn-loaders.cache.raw"
find "$setup_dir/lib/gdk-pixbuf-2.0" -name loaders.cache -delete
mkdir -p "$setup_dir/etc/fonts"
cat > "$setup_dir/etc/fonts/fonts.conf" <<'FONTS'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <dir>WINDOWSFONTDIR</dir>
  <cachedir prefix="xdg">fontconfig</cachedir>
  <config><rescan><int>30</int></rescan></config>
</fontconfig>
FONTS
cp "$script_dir/../LICENSE" "$setup_dir/LICENSE.txt"
cp "$script_dir/../AUTHORS" "$setup_dir/Xournalpp-AUTHORS.txt"
cat > "$setup_dir/README.txt" <<'README'
Ultimate Teacher Notepad 0.2 beta for Windows x64
Run bin/UTN.exe. No MSYS2 installation is required.
This is a teacher test build, not a certified Microsoft Store release.
Based on Xournal++; GNU GPLv2 or later. See LICENSE.txt and Xournalpp-AUTHORS.txt.
Source and build instructions: https://github.com/Musitharthan8/ultimate-teacher-notepad
Keep scan background attachments beside their .xopp journals.
README
pacman -Q > "$setup_dir/DEPENDENCIES.txt"
printf '%s\n' "$version" > "$setup_dir/UTN-VERSION.txt"
git -C "$script_dir/.." rev-parse HEAD > "$setup_dir/SOURCE-COMMIT.txt"
makensis=${MAKENSIS:-}
if [[ -z "$makensis" ]]; then
    makensis=$(command -v makensis || command -v makensis.exe || true)
fi
if [[ -z "$makensis" && -f '/c/Program Files (x86)/NSIS/makensis.exe' ]]; then
    makensis='/c/Program Files (x86)/NSIS/makensis.exe'
fi
[[ -n "$makensis" ]] || { echo 'Install NSIS or set MAKENSIS.' >&2; exit 1; }
MSYS2_ARG_CONV_EXCL='*' "$makensis" -NOCD \
    "-DUTN_VERSION=$version" \
    "-DSETUP_DIR=$(cygpath -w "$setup_dir")" \
    "-DOUTPUT_INSTALLER_FILE=$(cygpath -w "$output_dir/UTN-$version-Windows-x64-Setup.exe")" \
    "-DLICENSE_FILE=$(cygpath -w "$setup_dir/LICENSE.txt")" \
    "-DICON_FILE=$(cygpath -w "$build_dir/src/win32/xournalpp.ico")" \
    "$(cygpath -w "$script_dir/utn.nsi")"
(cd "$setup_dir" && 7z a -tzip "$(cygpath -w "$output_dir/UTN-$version-Windows-x64-Portable.zip")" .)
(cd "$output_dir" && sha256sum ./*.exe ./*.zip > SHA256SUMS.txt)

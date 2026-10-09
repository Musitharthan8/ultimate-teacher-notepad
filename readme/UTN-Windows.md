# Ultimate Teacher Notepad for Windows

UTN is a teacher-focused Xournal++ fork, licensed under GPLv2 or later.
It is a Windows desktop application. It has not been submitted to Microsoft Store.

## Download and install

Open the latest successful **UTN Windows Installer** run on the repository's Actions page.
Download its **UTN-Windows-x64** artifact and extract the download.
Run `UTN-<version>-Windows-x64-Setup.exe` (for example `UTN-0.3.0-beta-Windows-x64-Setup.exe`).
The version is set once, in `windows-setup/package-utn.sh`, and appears in the file names, the installer and Help › About. Launch UTN from Start or the desktop shortcut.
The per-user installation does not need MSYS2 or administrator rights.
The beta installer is unsigned; signing and Store certification remain release work.

Alternatively extract `UTN-<version>-Windows-x64-Portable.zip` into an empty folder,
and run `bin/UTN.exe`. Keep the entire folder together. Settings still use the user profile.

UTN has its own settings directory (`utn`), install directory and Open With registration.
First launch copies existing `xournalpp` configuration if present, without changing the original.
Classic toolbar layouts remain available. The new identity does not alter XOPP format compatibility.

## Upgrade and remove

Close UTN before installing another build into the same UTN directory.
The installer retains user configuration and documents. Remove UTN using Windows installed apps.
Updates are currently installed manually. No automatic updater or Store update channel is included.
Scan background attachment files must remain alongside the journal until lesson bundling is implemented.

## Build

In MSYS2 MINGW64, build the existing project with CMake/Ninja and install NSIS and 7-Zip.
Run `bash windows-setup/package-utn.sh build` after compiling.
`MAKENSIS` can specify the path to `makensis.exe`.
Outputs are in `build/utn-release`, with SHA-256 checksums.
CI builds Release, runs unit tests, packages, installs into a path containing spaces,
launches with MSYS2 removed from PATH, and uninstalls before uploading the artifact.

Each beta embeds its source commit. Corresponding source is available in the public repository.
Xournal++ contributors, licence notices and upstream attribution are retained.

## Teacher pilot checks

Test both desktop and touch density, all themes, and Classic layouts.
Open a PDF, write, highlight, erase, add/edit/delete an Answer Box, undo/redo, save/reopen and export.
Check fonts and image import after installing outside MSYS2, including a Windows profile with non-ASCII characters.
Test Student View with the projector both duplicating and extending the screen: hidden answers must never appear
on the projector until revealed. High-zoom eraser performance still needs measuring before public release.

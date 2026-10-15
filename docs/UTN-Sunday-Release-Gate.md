# Sunday Windows beta gate — 18 October 2026

Goal: a usable Windows x64 teaching beta, with an installer and portable bundle.

## Verified on 15 October

- Release build, enabled unit tests, NSIS packaging, clean installation into a path containing spaces, startup with MSYS2 removed from PATH, and uninstall passed in GitHub Actions run 38704956034 (commit 5ded5d8ef).
- Local enabled unit suite: 271 tests across 43 suites passed; two existing shape-recognition tests remain disabled.
- Packaged application opens the teaching layout and signature dialog.
- Packaged CLI opens the text notebook fixture and exports PDF and PNG with only Windows system folders on PATH.
- Classroom popup now has an opaque surface, preventing the underlying tool rail from showing through its text.

The release workflow now also checks PDF and PNG output signatures after clean installation. A later commit/run must pass before treating those extra gates as verified in CI.

## Required before classroom sign-off

- Open a representative PDF worksheet, annotate, add an Answer Box, undo/redo, save XOPP, close/reopen, and export a marked PDF. Check content and placement.
- Draw/insert/save a signature; verify transparency and persistence after reopen and PDF export.
- Try read-aloud voice selection, speed/pitch, Stop, and closing the panel while speech runs.
- Test the actual classroom tablet: pressure, palm rejection, eraser button, scrolling, high DPI, touch target size, and projector/student-view behaviour.
- Check upgrades retain notebooks and settings; test the final artifact on a second Windows computer without MSYS2.

## Known limits to carry into beta notes

- Partial ink erasing still removes pixels beyond a small eraser disc on thick strokes. Stroke/Object modes provide alternatives, but the precision defect is not fixed. Do not describe this as pixel-accurate erasing.
- OCR requires an external Tesseract installation. Equations require a local TeX installation; the optional syntax editor may be unavailable on current MSYS2 packages.
- Dictionary lookup uses an online English service. Read aloud accepts pasted text and installed Windows voices; selected-PDF reading and synchronized highlighting are not implemented.
- Tablet and audible voice quality need device testing. Automated tests do not establish either.

## Build and handoff

Use the UTN Windows Installer workflow on feature/utn-classroom-ui-recovery. It produces Setup.exe, a portable ZIP, and SHA256SUMS.txt. For a local portable bundle without NSIS or 7-Zip:

```sh
bash windows-setup/package-utn.sh build --portable-only
```

Extract the complete portable ZIP and run bin/UTN.exe. The executable depends on the accompanying runtime folders. Keep notebook attachments with their notebooks. This beta is unsigned.

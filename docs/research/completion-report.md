# UTN research and reliability completion report

Date: 2026-10-19. Working branch: `feature/utn-research-reliability`. Base: `f802b7b28ab745a5f82885214cc34be64d28f9ec`, fetched PR #19 head. Implementation commit: `5b9238b9a` (`fix: match whole-stroke erasing to rendered pressure widths`). Documentation is committed separately at handoff. No push, PR creation or merge was performed. Pre-existing untracked `install/` was preserved.

## Deliverables

1. [Kami research interpretation](kami-findings.md): all 12 supplied files, all seven images, evidence IDs, contradictions and missing functional evidence.
2. [UTN current-state audit](utn-current-state.md) and [remote state](remote-state.md): actual architecture/features, stacked PRs, branch inventory, recent commits, CI and known limits.
3. [Kami versus UTN feature matrix](kami-utn-matrix.md): annotation, interface, teaching, document and optional features; unknown behavior is not parity evidence.
4. [Safe skills/tooling audit](skills-tooling-audit.md): all 20 requested categories, source/version/security/load status. No skill installed; existing tools suffice.
5. [Implementation roadmap](implementation-roadmap.md): problems, design, sources, dependencies, risks, acceptance and regressions across all seven requested areas.
6. Completed changes and review/test evidence below. This completes an initial audit plus a bounded P1 reliability change; deferred roadmap items are not implemented.

## Changes

`src/core/control/tools/EraseHandler.cpp`: whole-stroke hit testing now uses the outgoing segment's pressure width, matching `StrokeViewHelper`/`StrokeContour`. It ignores the final point's unused pressure and uses constant fallback width for highlighters and unpressured strokes. Before this change a later wide point could make nearby thin ink erase even when the rendered ink missed the eraser. No allocations, document-format changes or new dependencies were introduced by the product patch. Partial erasing, broad phase, swept sample radii and history ownership remain as before.

`test/unit_tests/control/EraseHandlerTest.cpp`: two regressions cover pressure increasing/decreasing, terminal pressure, highlighter fallback, coincident-point dots, unpressured fallback, tangency, no undo on misses and undo/redo on hits. A Cairo A8 raster check verifies the real renderer leaves the chosen thin-segment near-miss pixel empty. Analytical segment distances establish the miss; the test is not a general pixel-equivalence proof.

`test/gtk_tests/dialog/SettingsDialogPaletteTabTest.cpp`: compare normalized filesystem paths rather than UTF-8 path spelling. A Windows GTK run exposed a pre-existing `/` versus `\` separator mismatch in the fixture comparison. The assertion still compares the complete selected path, and changes no application behavior.

## Actual build and test evidence

Existing MSYS2 MINGW64 GCC 16.2.0/CMake 4.4.3/Ninja were used. Process-local PATH included `C:/msys64/mingw64/bin` and `C:/msys64/usr/bin`; no global environment changes. QA directory `build-qa` is ignored by the existing `build*/` rule. Configuration: Debug, `ENABLE_GTEST=ON`, `DOWNLOAD_GTEST=ON`, `ENABLE_CPPTRACE=OFF`, `ENABLE_GTK_SOURCEVIEW=OFF`. Official repository-pinned GoogleTest v1.16.0 source was downloaded within the build tree after network approval; it was not globally installed. No app deployment into `install/` was performed.

| Check | Actual result | Local evidence |
|---|---|---|
| Initial QA configure | Initial optional cpptrace clone blocked by sandbox network; rerun with approved network and cpptrace disabled succeeded | Tool output; `build-qa/CMakeCache.txt` |
| Initial native app + test build | Succeeded, 466 build steps | `build-qa/baseline-build.log` |
| Existing enabled unit baseline, excluding new tests | **280/280 passed**, 45 suites; two pre-existing tests disabled | `build-qa/baseline-units.log` |
| New tests before product correction | **Both failed**, incorrect element removal (`0` elements vs expected `1`); regression process exit 1 | `build-qa/red-tests.log` |
| Corrected native app + test build | Succeeded; targeted regression **2/2 passed** | `build-qa/green-build.log`, `green-tests.log` |
| Full enabled unit suite | **282/282 passed**, 45 suites, exit 0; two pre-existing recognizer tests disabled | `build-qa/final-units.log` |
| First GTK suite | 28 passed, one failed: selected palette fixture path used native separator instead of configured slash spelling | Tool output of first GTK run; exact mismatch described above |
| GTK after normalized path assertion | **29/29 passed**, six suites, exit 0; no display skips | `build-qa/final-gtk.log`, `gtk-portability-build.log` |
| Full default build/resources/translations/wrapper | Succeeded, 93 remaining generated-resource/build steps | `build-qa/all-build.log` |
| Packaging script syntax | `bash -n windows-setup/package-utn.sh` exited 0 | Tool output |
| Portable Windows package | Succeeded, exit 0; 98,955,476-byte ZIP; generated SHA256 independently matches | `build-qa/packaging.log`, `utn-release/SHA256SUMS.txt` |
| Packaged runtime with system-only PATH | Waited hidden process exit 0 for `--version`, PDF export and PNG export | `build-qa/packaged-{version,pdf,png}.{log,err}` |
| Export structure/content/visual check | PDF 1.7, one letter page, 3,882 bytes, extracts green/red/blue; PNG 7,991 bytes. `%PDF-` and PNG signatures valid. Poppler render exit 0; both PNGs visually show the expected coloured text and ruled background | `build-qa/packaged-smoke.pdf`, `packaged-smoke.png`, `packaged-pdf-render.png` |

Portable ZIP: `build-qa/utn-release/UTN-0.2.1-beta-Windows-x64-Portable.zip`. SHA256: `0026ef08d241a6e5c19807fda9f70280e5b2840646f94604709f66a71dce9136`. This is an unsigned Debug QA bundle with optional cpptrace/GtkSourceView disabled, not a signed Release installer. It was built from the tested working tree before commit creation, so embedded Git metadata may identify `f802b7b2-dirty` despite containing the correction subsequently committed as `5b9238b9a`. NSIS installation/uninstallation, upgrades and a separate clean Windows machine were not tested. Portable staging stayed under `build-qa/utn-dist.sgNmzi`; the user's `install/` was not overwritten. Initial sandbox MSYS2 mkdir denial required approved rerun. PowerShell's direct invocation did not provide waited GUI-subsystem exit codes, so the authoritative smoke checks used hidden `Start-Process -Wait -PassThru` runs.

Enabled unit coverage exercised circular/partial eraser geometry, sparse sweeps, pressure reach, overlap/order/history, text/list continuation, shape transitions/recognition, layer audience and visible write target, Answer Box raster invalidation, lesson/eraser XOPP roundtrips and PDF export. Existing automated Student View/audience checks do not establish physical projector operation.

Reproduce with the existing MINGW64 environment:

```sh
cmake -S . -B build-qa -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_GTEST=ON -DDOWNLOAD_GTEST=ON -DENABLE_CPPTRACE=OFF -DENABLE_GTK_SOURCEVIEW=OFF
cmake --build build-qa --target test-units test-gtk-integration xournalpp -j 4
CI=true ./build-qa/test/test-units.exe
./build-qa/test/test-gtk-integration.exe
cmake --build build-qa -j 4
bash windows-setup/package-utn.sh build-qa --portable-only
```

Build warnings remain in existing code: deprecated LegacyRedrawable/Image APIs, integer/double conversions, Windows `%F` strftime formatting, and CMake GoogleTest discovery's malformed optional property list. MinGW locale initialization emits a documented warning in the targeted regression run without CI mode. These were not concealed or treated as new eraser regressions. Tests were executed directly through the native binaries; no claim of a clean warning-free CTest configuration is made.

## Independent review

A separate reviewer inspected the full product/test diff and audit documents. It found no material product/test issue after corrections. Earlier findings were fixed: use endian-independent A8 alpha sampling, align the audit with handler-only scope, and preserve already implemented line spacing in the roadmap. Review checked pressure/render agreement, terminal widths, highlighter behavior, joins, allocations and undo ownership. The reviewer did not independently execute tests; the coordinator's actual results are above.

## Remaining issues, limits and next priorities

1. **P1: thick-stroke partial erasing still over-deletes pixels.** This change fixes whole-stroke target selection only. Subtractive geometry/masks require a separate save/render/export compatibility design.
2. **P1: actual classroom release gate.** Test a representative PDF worksheet through pen/highlight/text/Answer Box, hide/reveal, Student View, undo/redo, save/reopen and PDF export on the real tablet and second display. Validate pressure, palm rejection, hover, eraser button, high DPI and projector hotplug.
3. **P1: latest artifact packaging/upgrade sign-off.** Current release workflow push triggers omit the recovery branch; manual dispatch remains available. No release workflow was dispatched or uploaded by this task. Upgrade preservation and clean-machine install/uninstall remain separate checks.
4. **P2: disabled shape-recognition cases.** `largeImperfectSquareIsNotCircleInPipeline` and `roughSmallSquareIsNotCircleInPipeline` remain explicitly disabled; passing enabled tests does not resolve them.
5. **P2: native UI and accessibility.** Existing GTK widget/property/shape tests passed; these are not full screenshot, screen-reader, keyboard-only, theme/DPI or device audits. No new UI redesign was made.
6. **P2: memory/performance evidence.** No sanitizer, leak detector, long-running stress test or hardware latency profile was run. No crash occurred in the completed tests, which is narrower than memory-safety certification. No security/dependency-vulnerability scan was run.

Whole-stroke hit tests remain capsule approximations for dashed gaps, filled interiors and non-round caps; generic `CircularEraser.h` geometry is unchanged. Existing single-point semantics remain for malformed strokes, although normal renderer input uses at least two points. No comparative Kami performance claim is supported by the supplied evidence.

## Draft PR text for review only

**Title:** Match whole-stroke erasing to rendered pressure widths

Whole-stroke erasing could delete nearby thin ink using the next point's pressure, even though UTN renders each segment at its starting point's width. Match that rendering convention, ignore unused terminal pressure, and retain constant-width highlighters. Add regression coverage for near misses, true hits, history and dots; normalize a Windows GTK palette path assertion discovered during validation.

Validation: native Windows Debug build, 282 enabled unit tests and 29 GTK tests pass; two existing recognizer cases remain disabled. Includes evidence-based Kami/UTN/tooling audits and a phased roadmap. Partial-erasure pixel precision and actual tablet/projector sign-off remain open.

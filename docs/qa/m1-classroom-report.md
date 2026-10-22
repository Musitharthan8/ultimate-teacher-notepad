# M1 classroom workflow QA

22 October 2026. Base: `origin/feature/utn-research-reliability` at
`93803707fd3b9509c96216b606e3633b85991c00`. Worktree: `build-classroom-qa`, branch
`feature/utn-m1-classroom-qa`. The base is the locally available remote-tracking commit; no remote freshness claim is
made. Original worktree and its untracked files were preserved. CI, packaging and dependency configuration are outside
this change.

## Confirmed defects

1. **Resized Answer Box/wrapped text changes on reopen.** The text serializer folds a homogeneous scale into font
   size for legacy encoding but leaves wrapping and box dimensions in the old local coordinate system. The new mixed
   GTK lesson test reproduced a box-boundary change of about 0.79 document units per edge. The focused wrapped-text
   regression reproduced a height difference exceeding ten document units. Boxed or wrapped text now uses the already
   supported matrix encoding, preserving local font, wrap, padding, border and radius together. Plain unwrapped text
   retains the legacy optimization. `build/mixed-first.log` and `build/scaled-text-red.log` record failures before the
   serialization correction. No assertion tolerance was relaxed.
2. **Toolbar callback owners can die before their widgets.** The real MainWindow harness crashed in destruction
   callbacks in `ToolUtnContextBar.cpp:330` and `ToolTextMode.cpp:44`. GDB traces in `build/student-gdb-full.log` and
   `build/workflow-gdb.log` show the Glade builder releasing widgets after tool handlers had been destroyed. The
   MainWindow destructor now unloads dynamic toolbar widgets while their handlers are alive. It also disconnects its
   callbacks from shared GtkSettings, preventing a later window/theme change from calling the destroyed MainWindow.
   This concerns the teacher window's lifecycle; it is not evidence that Student View's renderer leaked private data.
3. **Repeated canvas disposal emits two criticals.** Window destruction followed by XournalView cleanup disposes the
   canvas again. `gtk_xournal_dispose` unconditionally unrefs adjustments already cleared on the first call. A new
   teardown assertion failed with exactly two GLib-GObject criticals (`build/dispose-red.log`); GDB located the unref
   (`build/cleanup-gdb.log`). Using `g_clear_object` makes these releases safe on repeated disposal. Tests continue to
   assert zero cleanup criticals.
4. **Student overlays outlive their tool.** Freeze creates a teacher outline whose draw/destroy callbacks reference
   ToolStudentView. Closing the student window only hides that outline. Destroying the tool left the overlay attached
   to the teacher window, so later teacher-window teardown called a dead object. The destructor now disconnects and
   destroys both outline and blank-cover overlays. The regression compares the teacher overlay count before creation
   and after tool destruction, with a pending redraw queued, as well as exercising real close/reopen behavior.
   With cleanup temporarily removed, `build/overlay-red.log` records five overlay children versus the original four.

The suspected hide-during-edit privacy fault did not reproduce: existing layer switching finalizes the editor before
changing the selected layer. No replacement deadlock fix or renderer/eraser rewrite was made.

## Coverage

Five new `ClassroomWorkflow` GTK cases exercise:

- Ten Answers activations with distinct names and Answers audience, repeated hide/reveal and writable-layer routing.
- Editing a real Answer Box while hiding, checking private layer ownership and formatting through undo/redo.
- Hidden teacher ghost editing, page-navigation finalization, deletion/restoration, Unicode text and production
  save/load with Answers still hidden.
- Student View's real menu controls and drawing callback: ordinary/Answers/TeacherOnly pixels; revealed frame Freeze;
  Blank overriding Freeze; resume on the next page; resize; three close/reopen cycles. Main-window teardown is also
  exercised at the end of the classroom suite.
- The mixed lesson fixture's real selection, movement, resize, deletion, history, tool transitions and save/reopen.
  Checks retain element identity/order through history and type/geometry through persistence. Existing shape and
  eraser tests supply complementary coverage; synthetic events do not certify physical selection handles.

The suite keeps one teacher application alive and resets documents between cases, matching the application's
single-main-window lifetime. Early multi-application harness runs crashed and are retained in the logs; they are not
passing final evidence. Student windows are still created and destroyed repeatedly within the suite.

One new `TextReliability` unit case checks scaled boxed and plain wrapped text through production XOPP save/load,
including matrix, content, wrap, style dimensions and bounds.

The harness follows real menu/resource initialization. Its initial page broadcast explicitly initializes all
listeners, including the layer controller; simply asking Control to select already-current page zero suppresses that
broadcast. Event-loop draining is bounded so a continually ready source cannot hang the test helper. These setup
corrections are not product defects.

## Reproduction environment

PowerShell has Git 2.55.0 at `C:/Program Files/Git/cmd/git.exe`. CMake and Ninja are installed in
`C:/msys64/mingw64/bin` but absent from the initial PATH. Builds use process-local PATH changes, MSYS2 MINGW64 GCC
16.2.0, CMake 4.4.3, Ninja 1.13.2, Debug and installed GTK3/Poppler dependencies. Existing pinned GoogleTest sources under
the original checkout's `build-qa/_deps/googletest-src` are reused without downloading or changing dependency files.

From the isolated worktree, configure/build using:

```powershell
$env:PATH = 'C:/msys64/mingw64/bin;C:/msys64/usr/bin;' + $env:PATH
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_GTEST=ON -DDOWNLOAD_GTEST=ON -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=C:/Users/musit/Documents/ultimate-teacher-notepad/build-qa/_deps/googletest-src -DENABLE_CPPTRACE=OFF -DENABLE_GTK_SOURCEVIEW=OFF
cmake --build build --target resources-templates/pagetemplates.ini resources-templates/toolbar.ini test-units test-gtk-integration -j 6
$env:XDG_CONFIG_HOME = "$PWD/build/test-config"
$env:XDG_CACHE_HOME = "$PWD/build/test-cache"
./build/test/test-units.exe --gtest_output=xml:build/units-final.xml
./build/test/test-gtk-integration.exe --gtest_output=xml:build/gtk-final.xml
```

Use isolated config/cache directories before starting each test process, so personal classroom settings and plugins
are not loaded. GTK requires a display; the new cases skip explicitly when `gtk_init_check` fails. Generated template
resources are build prerequisites, not packaging changes. Log and XML evidence stays in ignored `build/`.

## Results

Fresh unchanged-base baseline: **312/312 unit tests and 29/29 GTK tests passed**, exit 0, in
`build/baseline-{units,gtk}.{log,xml}`. Final full suites passed **313/313 units and 34/34 GTK tests**, both exit 0,
zero disabled/skipped tests. Evidence: `build/units-final.{log,xml}`, `build/gtk-verified.log` and
`build/gtk-final.xml`. Final builds passed (`build/verified-build.log`, `build/verified-test-build.log`), and
`git diff --check` passed. No CTest, physical-hardware or CI run is implied by these direct binary results.

Earlier failed iterations remain in the ignored build directory, including `gtk-final.log` and `gtk-suite.log`;
those are not the successful final GTK log. A first overlay-count assertion incorrectly assumed the teacher overlay
had only its main child; the final assertion compares against its measured pre-tool count, preserving existing
toolbox/toast overlays. The regression against the original destructor still fails with the extra orphan overlay.

## Remaining acceptance

Use [the teacher checklist](m1-classroom-acceptance.md). Physical second-display placement/hotplug, projector output,
stylus/touch/palm rejection, native IME, accessibility and scaling/theme review remain unverified or hardware-blocked.
Freeze deliberately retains earlier revealed content. Ordinary and explicit layer-range exports differ from Student
View audience filtering. Existing masked-PDF operator comparisons from the earlier M1 handoff remain a separate open
question; this QA does not treat ruling loss as a proven defect or change that operator.

Native tests emit device-discovery warnings in this execution environment.
GTK also reported permission-denied warnings while attempting recent-file bookkeeping under the Windows user data
directory (`recently-used.xbel`); the production save/load assertions used writable temporary files and passed.
No warning-free execution is claimed. No physical device, Release installer,
memory sanitizer, leak detector, formatter or CI pipeline is claimed. No packaging or release was produced. Integration
after Claude's packaging work should include these bounded source fixes and tests, then rebuild the packaged candidate
and perform the checklist on the classroom hardware.

## Files

- `src/core/control/xojfile/SaveHandler.cpp`
- `src/core/gui/MainWindow.cpp`
- `src/core/gui/toolbarMenubar/ToolStudentView.cpp`
- `src/core/gui/widgets/XournalWidget.cpp`
- `test/gtk_tests/toolbar/ClassroomWorkflowTest.cpp`
- `test/unit_tests/control/TextReliabilityTest.cpp`
- `docs/qa/m1-classroom-acceptance.md`
- `docs/qa/m1-classroom-report.md`

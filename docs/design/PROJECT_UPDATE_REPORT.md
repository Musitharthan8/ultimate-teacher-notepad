# Ultimate Teacher Notepad: project update report

Written 2026-10-30 for sharing with another assistant. It summarises the teacher-first UX programme up to the end of Phase 3.

## Project

Ultimate Teacher Notepad (UTN) is a Windows-native, teacher-first PDF annotation application derived from Xournal++. It is native C++ on GTK 3, built with CMake and Ninja under MSYS2 UCRT64. The work is a renovation of the existing application, not a rewrite.

- Repository: `github.com/Musitharthan8/ultimate-teacher-notepad`
- Branch: `feature/utn-teacher-ux-program` (pushed; tip `09321b85b`)
- Baseline: `aafdea359` on `design/utn-teacher-first-2026-10-30`
- Engineering handoff with full detail: `docs/design/TEACHER_UX_IMPLEMENTATION.md`

The programme implements 14 approved design and productivity features in dependency-ordered phases. Rules in force: local commits, no merge into the main design branch, no force-push, no soak or endurance runs, no GUI framework switch, no native-PDF migration, the real user settings file is never modified, and teacher-only content never appears in student-facing output.

## Phase status

| Phase | Features | State |
| --- | --- | --- |
| 1 | 13 document status centre, 15 visual system | 13 implemented; 15 partly implemented |
| 2 | 4 pen presets, 10 Beginner/Expert, 1 customisable dock, 2 context panel | 4, 10, 1 implemented; 2 already existed and was audited |
| 3 | 3 Teach/Mark/Present workspaces, 6 Classroom controls, 8 distraction-free presentation | implemented on existing systems; native checks limited |
| 4 | 7 page navigator, 11 command palette (Ctrl+K) | not started |
| 5 | 9 floating selection toolbar, 5 annotation bank | not started |
| 6 | 12 undo history panel, final polish, acceptance matrix | not started |

## Phase 1 (document status, visual tokens)

- A document status model drives an always-visible save status in the header: saved, unsaved, saving, failed, exported. The status chip is a button that opens a details popover with "Save now".
- Save, autosave, PDF export and crash recovery report into the status.
- CSS status tokens and High Contrast tokens replace 82 colour literals.

## Phase 2 (presets, mode, dock)

- **Pen presets:** red marking pen, blue explanation pen, yellow highlighter, save-current-as-preset, manage dialog, stored in the `utn` settings element. A preset shows as selected only while the tool matches it.
- **Beginner/Expert:** Beginner shows tool names and hints, Expert shows icons only. It is independent of the Teach/Mark/Present choice.
- **Customisable dock:** pin, unpin, hide, reorder inside a group, reset. Essential tools (pen, eraser, both selection tools) can never be hidden. A dialog with named buttons works by keyboard.
- **Bug found by native testing:** the Workspace menu panel was never shown, which made the Phase 2 entries unreachable. Fixed.

## Phase 3 (this update)

### What already existed and was reused

Teach, Mark and Present are the layouts `UTN Teacher`, `UTN Marking` and `UTN Present`. Spotlight, Curtain, Freeze, Blank, Student View and the Escape exit for overlays already existed. Switching workspaces already kept the active tool, colour, preset, Beginner/Expert mode, zoom and document; a test now proves it.

### Added

- **Classroom controls (Feature 6).** The "Spotlight & Curtain" popover became "Classroom controls". It has:
  - a status line in words ("Classroom is normal", "On: Spotlight, Freeze");
  - Spotlight, Curtain, Freeze, Blank and Student View open/close;
  - one "Back to normal" that clears every effect together.
  A small pure model, `model/ClassroomMode`, names the active effects so the status line and the button agree.
- **Escape.** Escape now also takes down the Blank cover that hides the teacher's own canvas on a single display. It does nothing while a popover or menu holds the grab, and leaves Freeze and a projector-only Blank alone, so text editing and selections keep Escape.
- **Present (Features 3 and 8).**
  - Leaving Present returns to the workspace it was entered from (Teach if that layout is gone).
  - Present hides the tool dock until the teacher asks for it. A later Present starts quiet again.
  - In Present, the Classroom controls list "Show tools" / "Hide tools" and "Leave Present".
  - Shortcuts: Ctrl+Shift+1, 2, 3 choose Teach, Mark, Present; Ctrl+Shift+P enters or leaves Present; Ctrl+Shift+T shows or hides the dock in Present. They are skipped while a text entry has focus and read the key without Shift, so other keyboard layouts work.

### Defect found and fixed

Turning Blank off did not take the teacher cover down when Student View had been opened while the cover was up. The teacher was left behind a cover that hid the only controls able to remove it. Found by the new Escape test, fixed in `ToolStudentView::setBlanked`.

### Privacy

The review of every transition path (workspace switch, Back to normal, Escape, Blank, Freeze, opening Student View from the new popover) found no way for private layers or hidden answers to reach the student-facing output. Restore, unblank and unfreeze re-render through the layer filter before painting, and an unsafe frozen frame is replaced by a blank one.

## Verification

| Check | Result |
| --- | --- |
| Unit tests | 483 passed, 1 skipped (`CrashChild.run`) |
| GTK integration tests, 120 DPI | 109 of 109 |
| GTK integration tests, 96 DPI | 109 of 109 |
| Real user settings file | same SHA-256 and modification time before and after |
| After the final test-only fix | only the two affected tests were re-run (pass, no GTK CRITICAL); the full suites were not repeated |

Soak and stress tests were excluded, as agreed.

New tests: five unit tests for the classroom model, and GTK tests for the Classroom controls and Back to normal, Escape, workspaces keeping tool/dock/mode/page, Present hiding and revealing the dock, shortcuts, and Present's reveal and leave controls. Mutation checks turned the tests red for: the Escape cover branch, `leavePresent` always going to Teach, Present not hiding the dock, and the Blank-cover fix.

Independent read-only reviews were run by four agents: a C++ reviewer, an accessibility architect, a security reviewer (privacy of classroom transitions) and a silent-failure hunter. None reported a critical issue. Functional findings were fixed: `leavePresent` with a deleted layout, a stale remembered workspace, shortcuts in text entries and on non-US layouts, Escape taking a popover's Escape, and the reveal button's initial label.

## Not done, not verified, or open

- **Native checks.** The Classroom controls popover was checked with a real mouse and renders as designed. A native Blank test opened Student View across the whole display while the user was screen sharing, so native checks were stopped. Native keyboard, Escape, Present and shortcut checks, and the narrow-width (800, 1024) captures, are NOT TESTED. Dark mode, High Contrast, touch and stylus are NOT TESTED.
- **Accessibility gaps (found by review, not fixed):**
  - a screen reader is not told when the classroom status changes after the popover closes;
  - Freeze, Blank, Spotlight and Curtain are relabelled buttons rather than toggle buttons with a pressed state;
  - the Blank cover has no alert role and the controls behind it stay in the Tab order;
  - focus is not moved before the Present dock hides;
  - the shortcuts are not GAction accelerators, so they are neither listed in menus nor rebindable.
- **Hardening items from review, not done:** listeners are moved aside during notification in `ToolStudentView::updateState`; a tool-changed listener added per popover build is never removed; `restoreNormal` notifies twice; closing Student View while Blank or Frozen clears both; the popover's Freeze is greyed out without Student View and does not say why.
- **Present** has no pointer-edge reveal, only the button and the shortcut.
- **No automated test** for unsaved annotations through Present, or for a restart after classroom use.
- **Carried over from earlier phases:** the rail colour dot does not change when a preset changes the pen colour (cause not investigated); restart persistence of presets and dock is tested as a saved-string round trip, not through a second `Control`; Expert mode adds no shortcut emphasis; the dock customises only the teacher rail; some visual tokens and a written type and spacing scale are not done.
- **Not started in this phase:** the translation-string sync, a full skill inventory matrix and a consolidated privacy audit.

## Environment note

This machine reports two monitors to GTK while Windows reports one. That makes Blank open the Student View window instead of covering the teacher's canvas, so same-screen Blank behaviour is tested by raising the cover directly. Worktree path: `C:\Users\musit\Documents\utn-teacher-ux`; build directory: `C:\Users\musit\Documents\utn-teacher-ux-build`.

## Reproduce the build and tests

From MSYS2 UCRT64, with the build directory above:

```
ninja -j2 test-units test-gtk-integration xournalpp
./test/test-units.exe --gtest_filter='-*Soak*:*Stress*'
./test/test-gtk-integration.exe --gtest_filter='-*Soak*:*Stress*:*heavySession*'
__COMPAT_LAYER=DPIUNAWARE ./test/test-gtk-integration.exe --gtest_filter='-*Soak*:*Stress*:*heavySession*'   # 96 DPI
```

Run the GTK suite directly, not through an MSYS `timeout`, which mangles the environment. Use `-j2` and run the suites one after another on a low-memory machine.

## Next

Phase 4 (page navigator, Ctrl+K command palette), then Phase 5 (floating selection toolbar, annotation bank) and Phase 6 (undo history panel, final polish, acceptance matrix). Before Phase 4, run the native Escape, Present and shortcut checks when the screen is not being shared, and decide whether to fix the accessibility gaps above first.

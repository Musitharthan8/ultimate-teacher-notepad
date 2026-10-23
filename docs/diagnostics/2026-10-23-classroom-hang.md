# 2026-10-23 classroom hang: preserved findings

Status: investigation paused by product decision. No root cause is established. No fix has been applied or proposed.

## Build and environment

- Candidate build: `f9535300210ce36e1dbf04cf46d64bf52a75755b` ("Fix visual-polish review findings: scrollbar, colour chooser, HC swatch, divider width"), branch `feature/utn-visual-polish-review-fix`.
- Package: `UTN-Windows-x64-test-f9535300210ce36e1dbf04cf46d64bf52a75755b.zip`.
- Application string: `xournalpp 1.3.8+dev`.
- Windows 11 26H2, GTK 3.24.52, GLib 2.90.1, Cairo 1.18.6, integrated laptop display, physical stylus.

## Reported timeline (teacher laptop, 23 October 2026)

- Process started about 10:55, PID 22760, about 34 minutes of operation.
- First pressure warning 11:23:37; warnings continue to 11:29:31.
- Last recorded autosave attempt 11:28:29.
- Process exited with status 1 at 11:29:59.
- The log holds 547 lines of `Pressure-sensitive device omitted pressure this time` (116 during 11:27:32, 74 during 11:29:31).
- No stack trace, segmentation fault, fatal assertion or explicit crash reason in the log.
- A GDBus launch warning at startup. No evidence links it to the failure.

## Evidence gathered

1. Windows Application log, 23/10/2026 11:29:59, provider `Application Hang`, Event ID 1002: "The program xournalpp.exe version 0.2.1.0 stopped interacting with Windows and was closed." The time matches the observed exit. This points to a hang (UI thread unresponsive), not a crash. The version string reported is the executable's file version, not the `1.3.8+dev` build string.
2. No Windows Error Reporting entry and no dump for xournalpp in `%LOCALAPPDATA%\CrashDumps` or the WER queue/archive on the development machine. The event was seen on the machine this investigation ran on, so confirm it is the same machine as the teacher laptop before treating it as the primary record.
3. The raw log file and the recovery/autosave document were not available at the time of writing. Only the summary above exists. They should be preserved unmodified when obtained.

## Pressure warning: source and path

- Source: `src/core/gui/inputdevices/PenInputHandler.cpp`, `PenInputHandler::filterPressure()`, the `g_debug` call at line 229.
- Condition: pressure mode is `DEVICE_PRESSURE` (set on button press when the press event carried a pressure value) and a later event arrives with `pos.pressure == Point::NO_PRESSURE`.
- Pressure origin: `gdk_event_get_axis(..., GDK_AXIS_PRESSURE, ...)` in `src/core/gui/inputdevices/InputEvents.cpp`. When GDK reports no pressure axis, the field keeps its default `Point::NO_PRESSURE`. Touch events are forced to `NO_PRESSURE`.
- Handling: the code substitutes `lastPressure`, then applies `std::max(getMinimumPressure(), filtered * getPressureMultiplier())`. The minimum is asserted to be at least 0.01, so the substituted width cannot be zero or negative. `lastPressure` starts at 0.0 and is only used after a pressure-carrying press.
- It is a `g_debug`, so the log capturing it had debug-level output enabled.
- Valid events omitting pressure is plausible by design for some Windows pen drivers. Not yet confirmed on the teacher's hardware.
- Not established: whether the warnings relate to the hang. 547 lines over about six minutes is a small volume and is currently treated as a correlate only.

## Remaining hypotheses (all untested)

- Cost that grows on dense pages, such as redraw of masked or eraser paths, or growth in undo history.
- UI-thread stall during autosave (last attempt 11:28:29, hang about 90 seconds later).
- A lock-up in code added by the visual-polish changes.
- Driver or GTK-on-Windows pen input behaviour.
- Deferred, not ruled out: debug logging overhead, event queue saturation.

## Not done

- Branch for fixes, stress harness, controlled stylus/mouse/synthetic comparisons, performance measurements, dump collection.

## Suggested evidence to collect when work resumes

- Original log and recovery document, byte-preserved.
- Teacher machine's Application and WER records for PID 22760.
- A hang dump (for example via Task Manager "Create dump file" while frozen, or WER LocalDumps configured for xournalpp.exe), reviewed or redacted so lesson content is not exposed.

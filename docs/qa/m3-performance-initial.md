# M3 repeated-ink investigation: initial measurements

2026-10-23. **Verdict: NOT ASSESSED for the reported physical-stylus freeze; release blocker remains open.** The deterministic workload below completed. It is not a proof that a 34-minute hardware/driver/autosave session is reliable.

## Workload and reproduction

New GTK test `ClassroomWorkflow.repeatedInkThroughPageInputRetainsHistoryAndRoundTrips` drives the real `XojPageView` press/motion/release path with 16 motion samples per stroke, pressure 0.7, dense overlapping short zigzags, first Pen then freehand Highlighter. It pumps up to eight pending GTK events after each stroke, retains undo history, undoes/redoes one stroke and saves/reopens the document, asserting the expected element count. It uses the ordinary scheduler and renderer; audio and autosave are disabled by the existing GTK fixture. Device input filtering and real Windows pen/pressure history are bypassed.

Default suite load is 32 strokes per tool. Set `UTN_STRESS_STROKES` to an integer (clamped to 1–20000) for larger runs, then execute:

```powershell
$env:PATH = 'C:\msys64\mingw64\bin;C:\msys64\usr\bin;' + $env:PATH
$env:UTN_STRESS_STROKES = '20000'
& .\build-m1\test\test-gtk-integration.exe --gtest_filter=ClassroomWorkflow.repeatedInkThroughPageInputRetainsHistoryAndRoundTrips --gtest_output=xml:build-m3-logs/stress-20000.xml
```

Build: MinGW C++20 Debug, current first M3 layout batch based on `f4e54c4d9`. Per-stroke times include input handling, pen-up and the bounded event pump, not display-to-photon latency. Properties recorded every 100 strokes are cumulative sorted samples; percentile index is floor((N−1)×p/100). No source performance optimisation has been applied.

## Results

| Strokes per tool (total) | Pen p95 / p99 / max | Highlighter p95 / p99 / max | Test duration | Result |
| --- | --- | --- | --- | --- |
| 1000 (2000) | 0.221 / 4.463 / 24.792 ms | 0.199 / 0.596 / 52.541 ms | 3.532 s | Passed, exit 0; exploratory run during background compilation |
| 5000 (10000) | 0.519 / 3.048 / 69.230 ms | 0.665 / 3.186 / 78.056 ms | 9.839 s | XML and stdout pass; process-wrapper ExitCode was empty, so it is not independent exit-status evidence |
| 20000 (40000) | 0.582 / 3.506 / 66.395 ms | 0.569 / 2.949 / 65.001 ms | 22.732 s | Passed, waited exit 0 |

5000/tool run sampled process memory every 200 ms: observed maximum working set 132,235,264 bytes; private bytes 84,860,928. These are sampled absolute peaks, not a retained-memory leak analysis. No before/after speedup is claimed. Raw logs, XML with all intermediate percentile properties, and process sampling output are under ignored `build-m3-logs/`.

## Independent review leads

Claude's separate static audit (`m3-claude-initial-audit.md`) identified a fresh-stroke rerender suppression candidate: `StrokeHandler` transfers its stroke before the subsequent `elementChanged` callback compares against `inputHandler->getStroke()`. Source inspection confirms that sequence. It also identified document-lock contention between UI mutations and worker renders/autosave as a plausible stall mechanism. Neither has been established as the field-hang cause. Do not change ordering or copy raw element pointers outside locks without render/persistence/privacy proof.

## Required next measurements

- Actual pen-device events, missing-pressure and timestamp history, different speeds/zoom/widths and shapes.
- A prolonged workload with autosave enabled, multiple pages, repeated erasing, masked highlighter overlaps and zoom changes.
- Sampling profile/ETW and document-lock wait measurements correlated with the main-loop heartbeat.
- Controlled A/B of the rerender candidate with pixel-exact overlap/upper-layer tests and undo/save/export checks.
- Memory after quiescence and repeated document close/reopen, plus a self-contained portable Release build.

The existing pressure-warning log is a correlation, not a causal explanation. The preserved Windows Application Hang record remains the primary historical symptom. No request for minor owner testing is made.

## Resumed synthetic measurement

A fresh 20,000 pen + 20,000 highlighter stroke run passed with undo/redo and native save/reopen: 10.166 s test duration. Pen p95/p99/max: 0.154/0.521/9.792 ms. Highlighter: 0.142/0.466/53.002 ms. Source: build-m3-logs/batch-stress.xml. These timings reflect this run, not a controlled before/after speedup; build configuration, load and warm caches differ from earlier runs. No hardware-reliability inference.

Extended the existing stress harness to separately record release-event p99/max and a 1 ms GLib heartbeat's largest serviced gap. Timer lifetime is scoped to the test, including assertion exits. The gap includes time between event-loop servicing and sampling overhead; it is not a measured document-lock wait or OS pen latency. Autosave/multipage/real-stylus evidence remains required before clearing the freeze gate.

Instrumented 40,000-stroke result (menu-instrumented-stress.xml): passed in 10.409 s with history and save/reopen. Pen total p95/p99/max 0.160/0.816/10.942 ms; release p99/max 0.086/1.748 ms; largest serviced heartbeat gap 10.519 ms. Highlighter total 0.180/0.870/27.928 ms; release 0.104/1.231 ms; heartbeat gap 20.720 ms. Fatal-critical mode and strict XML gate passed. This run does not reproduce the field hang, profile contention or include hardware pen events/autosave/multipage workloads; the freeze remains unresolved.

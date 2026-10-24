# Claude stylus lag/freeze investigation, batch 1

**Verdict: the release blocker stays OPEN.** Nothing here reproduces the field hang and nothing here is hardware evidence.

## What was established (measured on this machine, MSYS2 mingw64, gcc 16.2, Debug)

1. **A real hang mechanism exists in this toolchain.** `Document::documentLock` is a `std::shared_mutex`. A 3-thread probe
   (`rwprobe/rw.cpp`, kept outside the repo) shows that on this toolchain a thread which takes a shared lock it already holds,
   while another thread is waiting for the exclusive lock, blocks forever: `recursiveAcquired=0 readerDone=0 writerDone=0`
   after 5 s. A render worker (`RenderJob::renderToBuffer`, shared lock for the whole `drawPage`) plus a UI-thread
   `doc->lock()` on pen-up (`StrokeHandler::onButtonReleaseEvent`) is exactly that shape if anything under `drawPage` or a
   save re-locks the document. The UI thread would then wait for the document and Windows would report "Application Hang".
2. **That nesting was not found on the exercised paths.** New opt-in tool `-DUTN_LOCK_AUDIT=ON`
   (`src/core/model/DocumentLockAudit.*`, wired in `Document::lock*`) reports any blocking acquisition by a thread that already
   holds the lock, and any wait over 50 ms, each with a stack trace. Results of an audit build:
   * existing `test-units` 344/344 and `test-gtk-integration` 45/45: 0 nested acquisitions, 0 slow waits;
   * new soak `ClassroomWorkflow.longSessionWithAutosaveAndSeveralPagesKeepsTheDocumentLockHealthy`
     (5 pages, 24-point strokes spread over the pages, undo/redo every 11th stroke, the real `AutosaveJob` on the scheduler every 20th
     stroke while worker threads render):

| Strokes | Autosaves | p95 / p99 / max per stroke | Longest process-wide lock wait | Nested / slow |
| --- | --- | --- | --- | --- |
| 600 | 30 | 23.9 / 37.2 / 56.5 ms | 0.03 ms | 0 / 0 |
| 2000 | 100 | 13.9 / 18.3 / 25.0 ms | 0.17 ms | 0 / 0 |
| 6000 | 300 | 16.5 / 24.6 / 46.2 ms | 0.08 ms | 0 / 0 |

   Per-stroke time includes 24 motion events, pen-up, undo/redo churn and a bounded event pump. No growth with session length.
3. Memory growth, release builds, real Windows Ink/pen filtering, hours-long sessions and zoom/scroll churn were **not** measured here.

## What this does not show

* It does not clear the blocker. The synthetic input bypasses the Windows pen device path and runs minutes, not the owner's ~34 minute session.
* The audit proves absence of nesting only on code that the tests execute. Eraser, selection, PDF page import, LaTeX, export and
  audio paths still need to be driven under the audit build.
* The pressure warning in the owner's log remains a correlation.

## How to use the audit for the one final hardware procedure

Build with `-DUTN_LOCK_AUDIT=ON` (Debug or RelWithDebInfo). Run the owner's real drawing session. The log (`g_warning`, domain `xopp`)
then names the first 25 nested acquisitions with the requesting stack and the first 25 waits over 50 ms.
Pending-wait watchdog warnings identify mutex/thread/mode, not a blocked-thread stack. Absence of an audit line does not rule out a document-lock wait or establish another root cause.

## Smallest safe remedy if a nesting is ever found

Do not switch mutex type blindly. Remove the inner lock by passing the already-held state down (the caller holds it), or take the
shared lock once at the outer level. A writer-fair mutex that tolerates recursion would hide the bug and change latency for everything.

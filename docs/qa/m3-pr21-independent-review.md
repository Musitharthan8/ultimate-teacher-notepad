# PR #21 independent integration review

Reviewed 2026-10-24 against M3 base `a02b9fb6ebf8d785bc238430a1326461073a473a` and Claude head
`191e232af2a5f77cb486e71b86257d9f3175a718`. All four commits and all 23 changed files were covered by independent
correctness, privacy and reliability/standards lenses, with parent review of the resulting fixes. This is an
integration review, not M3 acceptance or a stylus-freeze diagnosis.

## Findings that changed integration

1. **P1: a blocked native mutex never reaches the slow-wait warning.** The old audit logged slow waits only after
   acquisition returned. A real blocked shared waiter regression failed before the change; the new watchdog records
   pending blocking attempts and reports an overdue wait while acquisition is still blocked. Reports identify the
   mutex, requesting thread and shared/exclusive mode. A watchdog observation does not include the blocked thread's
   stack; nested acquisition reports still print the requesting thread's stack. Report volume is capped, but counters
   continue. Absence of a warning does not exonerate the document lock.
2. **P2: unrelated documents were reported as recursive locking.** Depth is now keyed by mutex identity within the
   requesting thread. A two-Document regression failed before the change and passed afterward. Pending registrations
   are scoped to the attempt, including exception/cancellation cleanup. The native `std::shared_mutex` operations and
   their ordering are unchanged. Audit remains OFF for the ordinary application; CI separately exercises the enabled
   diagnostic implementation.
3. **P1: `.xoj` layer filtering did not protect source assets or editable erased ink.** Actual Teacher export files
   and their linked/attached PDF asset preserved a removed second page, its private text and private PDF title. The
   visible erased-stroke fallback also carried `utn-stroke-v1`, from which original ink/audio metadata can be recovered.
   Student output now omits that editable metadata and page/layer names. A whole-document preflight rejects PDF/image
   backgrounds and included LaTeX payloads before writing output or any sidecar, with an explicit rendered-PNG
   alternative. This deliberately limits student compatibility export until those formats have verified sanitisation.
   Native/private lesson saving retains the original data.
4. **P2: Enter after a soft list continuation lost the next marker.** The new Shift+Enter inserts indented continuation
   lines, but the old Enter handler examined only the current line. It now finds the preceding marker across matching
   continuation indentation, stopping at an unindented blank paragraph or different indentation. Empty continuation
   exits the item. The plain-text format cannot distinguish manually typed matching spaces from a soft continuation.

## Assessment of each Claude commit

- `96fca9ed0`: responsive decisions preserve the existing thresholds and hysteresis. Fixed logical-width tests plus
  real widget allocation checks replace the invalid assumption that a CI desktop grants a requested 1280 px window.
  No skipped tests or weakened width policy were needed. Native high-DPI/theme visual acceptance remains open.
- `fc1ef4e5d`: the default audience and preview removal fix a genuine teacher-layer leak, but were insufficient for
  full source-asset confidentiality. Integrate together with the above fail-closed boundary and real-file tests.
- `207076027`: useful opt-in instrumentation/soak, with the diagnostic flaws above corrected before integration.
  Ordinary Windows CI previously compiled this option OFF. The new dedicated audit executable exercises the enabled
  implementation without enabling it in the packaged application.
- `191e232af`: key bindings preserve the IME-first dispatch order. `contentsChanged(true)` and `contentsChanged()`
  both invalidated layout/cursor state; their boolean argument does not create an undo transaction. The duplicate
  invalidation is removed from `softLinebreak`. Editing still creates one text undo action when the editor closes.

## Export evidence and boundary

Tests inspect the actual gzip XML, referenced/attached PDFs with Poppler, and decoded exported PNG payloads. They
check omitted private/hidden layers and preview; erased raster transparency and surviving ink; absent editable
original-stroke/audio metadata and private names; no output/sidecar on rejected PDF/image/LaTeX input; and public
image pixels retained after removing source PNG text metadata. Teacher/native round trips remain covered.

Student compatibility output is **annotation filtering, not visual redaction**. Intentionally public text, link
destinations, image pixels and revealed Answers remain shareable content, including content covered by other objects.
The rejection does not sanitise the source files themselves. Existing PDF export likewise has no general background
metadata/redaction guarantee; rendered PNG is the suggested alternative. CustomExportJob dialog-to-job execution,
physical printing, and every general PDF embedded-object type are not newly certified by these tests.

## Text and input evidence

GTK regressions exercise Return, keypad Enter and ISO Enter; repeated soft breaks; numbered/bullet continuation and
exit; Unicode midline splitting; selection replacement; unrelated modifier handling; plain text and Answer Boxes;
single edit-session undo/redo; and real native save/reopen. A synthetic Compose attempt did not enter the GTK IME
path and is not counted as validation. Static dispatch review preserves IME-first routing. Physical keyboards,
Windows language-specific IME preedit/candidate selection and hardware stylus
input remain unverified. No native desktop control was resumed.

## Validation

Fresh local Windows Debug builds completed for application and test executables with audit OFF and ON.

| Build | Unit tests | GTK tests | Dedicated diagnostic tests |
| --- | --- | --- | --- |
| Ordinary application, audit OFF | 354/354 | 51/51 | 6/6 (separate target enables audit) |
| Audit-enabled application | 356/356 (includes two real Document regressions) | 51/51 | Same implementation covered above |

Strict XML completeness checking rejects failures, errors, disabled or skipped tests. GTK audit-enabled run used
`G_DEBUG=fatal-criticals`. Existing nonfatal allocation, settings, palette and mingw locale/compiler warnings remain.
Focused privacy tests passed 9/9, including actual source assets and decoded exported image payloads.

Raw local evidence resides in `Documents/utn-claude-m3-build` (`review-unit-final.xml/.log`,
`review-privacy-green.log`, `lock-audit-tests.xml/.log`, `test/m3-codex-text-gtk.xml/.log`) and
`Documents/utn-claude-m3-build-audit` (`review-audit-red.log`, `review-audit-green.xml/.log`,
`review-final-build.log`, `review-unit-final.xml/.log`, `review-gtk-final.xml/.log`). Source whitespace gate passed.

The pre-review hosted run [40058198234](https://github.com/Musitharthan8/ultimate-teacher-notepad/actions/runs/40058198234)
was independently checked: Claude head, 349 unit/47 GTK tests, packaging and portable validation passed. It is not
validation of these follow-up changes. A new complete hosted Windows run on the integrated revision remains the
mandatory remote gate; its result must be reported separately after integration.

## Integration limits

Only `feature/utn-m3-integrated-classroom` may receive this work. PR #20 remains draft and must not be merged into
stable. No release and no minor owner testing. The prolonged hardware-stylus freeze remains an unresolved release
blocker. Professional contextual settings, handwriting recognition and the complete native visual/scaling matrix
remain unfinished in the feature register.

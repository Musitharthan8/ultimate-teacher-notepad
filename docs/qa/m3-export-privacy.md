# M3 export privacy correction — 2026-10-24

Confirmed the initial Claude audit concern with generated files, not source inspection alone. The original PDF renderer exported visible TeacherOnly content; explicit layer ranges bypassed audience filtering; progressive export temporarily revealed hidden layers. PNG/SVG and printing shared unfiltered rendering paths.

Shared output now defaults to Student audience: only visible public content and revealed Answers are rendered. PDF (including progressive and explicit ranges), PNG and SVG use the same audience predicate as Student View. Printing installs that predicate too. Custom export offers an unchecked, explicitly labelled Teacher copy option; this choice is not persisted. Native XOPP/XOJ lessons remain private editable documents containing all layers. Progressive export no longer changes model visibility or adds pages for private layers.

Six new unit tests inspect actual PDF text and raster pixels, actual PNG/SVG rendering, imported-PDF overlays through default and Cairo exporters, explicit teacher output and native save/reopen. Initial tests failed on private/hidden content before the correction. All six pass after it. One intermediate imported-PDF test crashed because its fixture omitted the required DocumentHandler; GDB located that fixture error and the test now supplies the production dependency.

The GTK export-dialog regression verifies Student default and explicit Teacher opt-in through the real dialog lifecycle. An intermediate stack-owned dialog fixture produced destruction criticals; using the production PopupWindowWrapper fixed the fixture. The focused GTK run and full suite passed with G_DEBUG=fatal-criticals.

Fresh complete validation: 327 unit tests and 41 GTK integration tests passed, zero disabled/skipped; strict XML gate passed. Reports: build-m3-logs/batch-units.xml and batch-gtk.xml. Existing GTK negative-gadget-dimension, settings-file and palette warnings remain; no critical-free claim is inferred from warning-free output.

Scope: this protects UTN annotation layers. It is not a PDF redaction or embedded-metadata sanitisation service. Native lessons must not be distributed as student copies. Physical printing and fresh native GUI interaction are not tested in this batch; native control remains stopped pending explicit authorisation. Hardware-stylus reliability and final candidate acceptance remain open.

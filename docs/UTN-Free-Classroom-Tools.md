# Free classroom tools

These tools use UTN's native editor, installed Windows voices, and the existing dictionary integration. They require no Kami account or subscription.

## Read aloud

Open **Classroom → Reading & Dictionary**, paste a passage, choose an installed voice, and set speed, volume, and pitch. Press **Read Aloud**. **Stop reading** ends the current reading; a second reading cannot start until the previous process finishes stopping. Rebuilding or closing the panel's widgets also stops reading.

Speech is generated locally using Windows System.Speech. Pitch support and pronunciation depend on the installed voice. This is currently a Windows feature. The passage limit is 5,000 characters. Reading directly from a selected PDF region, synchronized word highlighting, pause/resume, and persistent voice preferences are not implemented.

## Dictionary

The existing Windows dictionary is under the same tab. Type a word and choose **Find definition**. It sends the entered word to dictionaryapi.dev and requires Internet access; it is not an offline dictionary. It does not automatically extract or upload the PDF. An offline, licensed dictionary, Spanish definitions, and integrated pronunciation are still future work.

## Signatures

Choose **Classroom → Materials → Draw and insert signature…**. Draw with mouse or pen, use **Clear** to start again, and choose **Insert** to add the signature to the current page. It becomes a normal embedded image, using the editor's existing selection and undo path.

**Save PNG…** saves a transparent signature image for reuse. Insert it later with **Insert picture from file**. Keep saved signature images somewhere private. This is a handwritten image, not a cryptographic PDF signature. Keyboard users can insert a pre-existing image; keyboard drawing is not implemented. Pen pressure and palm rejection in the pad require device testing.

## Equations, images, audio, and answer fields

- **Shapes & Lines → Equation / TeX** selects the existing equation tool. Click the page to open the TeX editor. It requires a working local TeX installation; UTN does not bundle one in this change.
- **Classroom → Materials** offers picture insertion and the existing notebook audio record, play/pause, and stop actions. Audio requires a build with audio support. There is no new video object or per-object audio attachment in this change.
- The teacher rail already offers **Answer Box** and teacher feedback. These are editable worksheet annotations, not an online quiz, automatic grading, or student-response collection system.

## Validation checklist

Automated: compile the Windows application; run the signature PNG transparency/tap/clear tests and the unit suite; validate the actual speech script with XML-sensitive and Unicode test text rendered to a WAV file.

Verified locally on 15 October 2026: Windows application and test executable built with the installed MSYS2 MINGW64 toolchain; **271/271 enabled tests passed across 43 suites**, including both SignaturePad tests. Two pre-existing disabled shape tests remain disabled. The actual speech script rendered WAV files at speed/pitch −10, 0, and +10 with XML-sensitive and Unicode text. Rendering verifies successful synthesis, not audible voice quality.

Manual still required: pen/mouse drawing, Clear/Cancel, save errors and overwrite prompts, insert/undo/redo, XOPP reopen, PDF export, voice selection, Stop, panel destruction while speaking, high DPI, dark mode, and tablet layout. Compiling alone does not verify these interactions.

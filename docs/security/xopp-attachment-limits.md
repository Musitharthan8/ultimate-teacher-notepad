# XOPP attachment limits

A zip based `.xopp` file is untrusted input. Its attachments (pictures, PDF background, audio) and its XML are read
under the limits below. Defaults live in `src/core/control/xojfile/AttachmentLimits.h`; tests override them with
`LoadHandler::setAttachmentLimits()`.

| Limit | Default | Why |
| --- | --- | --- |
| One picture (background, inserted, LaTeX) | 64 MiB encoded/decoded | A 100 megapixel JPEG is about 30-50 MiB |
| One attached PDF | 256 MiB | Held in memory for rendering. Scanned textbooks reach ~200 MiB |
| One audio recording | 256 MiB | UTN records Vorbis: ~82 MiB for 90 min at 128 kbit/s |
| Memory budget (pictures + PDF), whole document | 512 MiB | A PDF and the pictures of one lesson fit together |
| Disk budget (audio temp files), whole document | 512 MiB | Two maximum recordings |
| Distinct archive attachment entries per document | 4096 | Repeated names for one ZIP entry share cache and budget; distinct failed entries count |
| Inline image payloads per document | 4096 | Repeated inline payloads consume the document budget |
| Decompressed XML | 256 MiB | Largest of 40 repository `.xopp` files tested was 18,333,645 bytes; cap is about 14x larger |
| Decoded raster image | 33,554,432 pixels per image | GdkPixbuf loaders bound dimensions before allocation, including backgrounds |
| Thumbnail preview | 1 MiB | A thumbnail is ~50 kB |

The sizes recorded in the archive are never trusted. An entry reserves its declared memory/disk budget before reading.
It is read in 64 KiB chunks and must deliver exactly the declared number of bytes: an early end, extra data or a libzip
error (CRC) discards the attachment. Failed reads remain cached by ZIP entry index and consume bounded budget. Decoded
image storage is shared for repeated archive references. Inline base64 is length checked before decoding, and decoded
output is bounded by the image and whole-document memory budgets.

Behaviour on failure: the attachment is skipped, the reason is added to the load messages (internal attachment name
only, never a host path), the rest of the document may open, but it is not bound to its file name so ordinary save
cannot overwrite the original without "Save as". Partial audio files are deleted. The XML cap was checked against 40
repository `.xopp` samples; that is corpus evidence, not a broad survey of third-party scanned workbooks. Large scanned
workbooks beyond the 64 MiB per-image or 512 MiB whole-document memory caps may need image reduction or splitting.

libzip < 1.12 (GHSA-m3wq-j48p-jxwh) can spend quadratic time in `zip_open()` on an archive with colliding file names.
No limit of ours can run before that call. Distributed builds must bundle libzip >= 1.12
(`-DUTN_REQUIRE_PATCHED_LIBZIP=ON`; `windows-setup/package-utn.sh` refuses older versions).

# XOPP attachment limits

A zip based `.xopp` file is untrusted input. Its attachments (pictures, PDF background, audio) and its XML are read
under the limits below. Defaults live in `src/core/control/xojfile/AttachmentLimits.h`; tests override them with
`LoadHandler::setAttachmentLimits()`.

| Limit | Default | Why |
| --- | --- | --- |
| One picture (background, inserted, LaTeX) | 64 MiB | A 100 megapixel JPEG is about 30-50 MiB |
| One attached PDF | 256 MiB | Held in memory for rendering. Scanned textbooks reach ~200 MiB |
| One audio recording | 256 MiB | UTN records Vorbis: ~82 MiB for 90 min at 128 kbit/s |
| Memory budget (pictures + PDF), whole document | 512 MiB | A PDF and the pictures of one lesson fit together |
| Disk budget (audio temp files), whole document | 512 MiB | Two maximum recordings |
| Attachments extracted per document | 4096 | Repeated and failed references count |
| Decompressed XML | 1 GiB | Real documents are 20-50 MiB |
| Thumbnail preview | 1 MiB | A thumbnail is ~50 kB |

The sizes recorded in the archive are never trusted. An entry is refused before any byte is read if its declared size
exceeds the limit. Otherwise it is read in 64 KiB chunks and must deliver exactly the declared number of bytes: an early
end, extra data or a libzip error (CRC) discards the attachment. Memory grows with the data really read, not with the
declared size.

Behaviour on failure: the attachment is skipped, the reason is added to the load messages (internal attachment name
only, never a host path), the rest of the document opens, and the document is not bound to its file name so the
original is not overwritten without "Save as". Partial audio files are deleted; audio files extracted for a document
that then fails to load are deleted.

libzip < 1.12 (GHSA-m3wq-j48p-jxwh) can spend quadratic time in `zip_open()` on an archive with colliding file names.
No limit of ours can run before that call. Distributed builds must bundle libzip >= 1.12
(`-DUTN_REQUIRE_PATCHED_LIBZIP=ON`; `windows-setup/package-utn.sh` refuses older versions).

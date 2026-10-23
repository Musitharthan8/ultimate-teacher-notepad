/*
 * Xournal++
 *
 * Resource limits applied to the attachments of a zip based .xopp document while it is loaded.
 *
 * A .xopp file is untrusted input: a teacher opens documents received by mail or a shared drive. The limits are sized
 * so that real teaching material loads unchanged, and a crafted archive cannot take the machine down.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <cstddef>  // for size_t
#include <cstdint>  // for uint64_t

namespace xoj::load {

constexpr uint64_t MiB = 1024ULL * 1024ULL;

struct AttachmentLimits {
    /// Background pictures, inserted pictures and LaTeX images. A 100 megapixel JPEG is ~30-50 MiB.
    uint64_t maxImageBytes = 64 * MiB;
    /// Attached PDF background. Kept in memory for rendering, so it is bounded more tightly than a disk file.
    /// Scanned textbooks of several hundred pages are the largest realistic case (up to ~200 MiB).
    uint64_t maxPdfBytes = 256 * MiB;
    /// One audio recording, extracted to a temporary file. UTN records Vorbis: a 90 minute lecture is ~82 MiB at
    /// 128 kbit/s, so 256 MiB covers ~4.5 hours at 128 kbit/s and ~2.3 hours at 256 kbit/s.
    uint64_t maxAudioBytes = 256 * MiB;

    /// Everything kept in memory (images and PDF) over the whole document
    uint64_t maxMemoryBudgetBytes = 512 * MiB;
    /// Everything written to temporary files (audio) over the whole document
    uint64_t maxDiskBudgetBytes = 512 * MiB;

    /// Number of attachments that may be extracted for one document, including repeated and failed attempts.
    /// A 300 page notebook with a picture background and a few stamps per page stays far below.
    size_t maxAttachmentCount = 4096;

    /// Size of the (decompressed) XML. Benchmarked repository corpus is 18 MiB at the largest; 256 MiB leaves ample
    /// room for inline image base64 and very large notes while preventing a multi-gigabyte XML allocation/parse.
    uint64_t maxXmlBytes = 256 * MiB;
};

}  // namespace xoj::load

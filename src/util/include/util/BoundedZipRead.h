/*
 * Xournal++
 *
 * Bounded, incremental reading of one entry of a zip archive.
 *
 * The size recorded in a zip archive is attacker controlled. Nothing here trusts it: it is only an upper bound that is
 * checked against the caller's limit before any byte is read, and it is verified against what the archive actually
 * delivers. The read loop never allocates from the declared size and never spins when the archive stops delivering.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <algorithm>  // for min
#include <cstddef>    // for size_t
#include <cstdint>    // for uint64_t, INT64_MAX
#include <limits>     // for numeric_limits
#include <new>        // for bad_alloc
#include <optional>   // for optional
#include <stdexcept>  // for length_error
#include <string>     // for string
#include <utility>    // for in_range
#include <vector>     // for vector

#include <zip.h>      // for zip_file_t, zip_stat_t, zip_fread
#include <zipconf.h>  // for zip_int64_t, zip_uint64_t

namespace xoj::util::zip {

enum class ReadStatus : uint8_t {
    Ok,
    SizeUnknown,           ///< The archive does not provide a size for the entry
    SizeNotRepresentable,  ///< The declared size does not fit in zip_int64_t or size_t
    TooLarge,              ///< The declared size is above the caller's limit. Nothing was read.
    Truncated,             ///< The archive stopped delivering data before the declared size
    LargerThanDeclared,    ///< The archive delivers more data than it declared
    ReadError,             ///< libzip reported an error (corrupt data, CRC mismatch, ...)
    SinkError,             ///< The consumer of the data (file, buffer) refused it
    OutOfMemory
};

/// Read this many bytes per zip_fread() call. Small enough to keep the memory use flat, large enough to be fast.
constexpr size_t BOUNDED_READ_CHUNK = size_t{64} * 1024;

/**
 * Checked conversion of a size recorded in an archive to size_t.
 * @return std::nullopt if the value does not fit in zip_int64_t (what libzip itself can address) or size_t
 */
inline auto checkedSize(zip_uint64_t declared) -> std::optional<size_t> {
    if (!std::in_range<zip_int64_t>(declared) || !std::in_range<size_t>(declared)) {
        return std::nullopt;
    }
    return static_cast<size_t>(declared);
}

/**
 * @return The uncompressed size of an entry if (and only if) libzip marked it as valid
 */
inline auto declaredSize(const zip_stat_t& stat) -> std::optional<zip_uint64_t> {
    if (!(stat.valid & ZIP_STAT_SIZE)) {
        return std::nullopt;
    }
    return stat.size;
}

/**
 * Reads exactly `declared` bytes from `read` and passes them on to `sink`, in chunks of BOUNDED_READ_CHUNK at most.
 *
 * @param read   Callable with the signature of zip_fread() without the file: zip_int64_t(void* buf, zip_uint64_t n).
 *               A return value of -1 is an error, 0 is end of data.
 * @param declared The size the archive claims for the entry
 * @param maxBytes The caller's limit. A declared size above it is refused without reading anything.
 * @param sink   Callable bool(const char* data, size_t size). Returning false aborts the read.
 * @param[out] bytesRead Number of bytes handed to the sink (also on failure)
 */
template <class Read, class Sink>
auto readBounded(Read&& read, zip_uint64_t declared, uint64_t maxBytes, Sink&& sink, uint64_t& bytesRead)
        -> ReadStatus {
    bytesRead = 0;
    if (!checkedSize(declared)) {
        return ReadStatus::SizeNotRepresentable;
    }
    if (declared > maxBytes) {
        return ReadStatus::TooLarge;
    }

    std::vector<char> buffer(BOUNDED_READ_CHUNK);
    try {
        while (bytesRead < declared) {
            const zip_uint64_t wanted = std::min<zip_uint64_t>(buffer.size(), declared - bytesRead);
            const zip_int64_t got = read(buffer.data(), wanted);
            if (got < 0) {
                return ReadStatus::ReadError;
            }
            if (got == 0) {
                // The archive ended before it delivered what it announced. Looping here would never terminate.
                return ReadStatus::Truncated;
            }
            if (static_cast<zip_uint64_t>(got) > wanted) {
                return ReadStatus::ReadError;  // a misbehaving source must not make us overrun the bound
            }
            if (!sink(buffer.data(), static_cast<size_t>(got))) {
                return ReadStatus::SinkError;
            }
            bytesRead += static_cast<uint64_t>(got);
        }

        // libzip reports CRC and size mismatches at the end of the entry, and a lying archive may have more data.
        // One more read tells both apart.
        char probe = 0;
        const zip_int64_t extra = read(&probe, 1);
        if (extra < 0) {
            return ReadStatus::ReadError;
        }
        if (extra > 0) {
            return ReadStatus::LargerThanDeclared;
        }
    } catch (const std::bad_alloc&) {
        return ReadStatus::OutOfMemory;
    } catch (const std::length_error&) {
        return ReadStatus::OutOfMemory;
    }
    return ReadStatus::Ok;
}

/**
 * Appends to a std::string without ever reserving more than has been read so far times two, and never more than the
 * declared size: a lying archive that announces a huge size but delivers little cannot make us allocate it.
 */
class StringSink {
public:
    StringSink(std::string& out, size_t declared): out(out), declared(declared) {}

    bool operator()(const char* data, size_t size) {
        if (out.capacity() - out.size() < size) {
            const size_t needed = out.size() + size;
            const size_t grown = std::max<size_t>(out.capacity() * 2, size_t{1024} * 1024);
            out.reserve(std::max(needed, std::min(grown, declared)));
        }
        out.append(data, size);
        return true;
    }

private:
    std::string& out;
    size_t declared;
};

/// Convenience wrapper around readBounded() for an opened zip_file_t
template <class Sink>
auto readZipFileBounded(zip_file_t* file, zip_uint64_t declared, uint64_t maxBytes, Sink&& sink, uint64_t& bytesRead)
        -> ReadStatus {
    return readBounded([file](void* buf, zip_uint64_t n) { return zip_fread(file, buf, n); }, declared, maxBytes,
                       std::forward<Sink>(sink), bytesRead);
}

}  // namespace xoj::util::zip

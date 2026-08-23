#include "usdraster/RandomAccessSource.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace usdraster {

const char* GetReadStatusName(ReadStatus status) {
    switch (status) {
        case ReadStatus::Ok: return "ok";
        case ReadStatus::ShortRead: return "shortRead";
        case ReadStatus::EndOfSource: return "endOfSource";
        case ReadStatus::Failed: return "failed";
    }
    return "failed";
}

// --- MemorySource ----------------------------------------------------------

MemorySource::MemorySource(const void* data, std::size_t size,
                           std::string identifier)
    : _data(static_cast<const std::uint8_t*>(data)),
      _size(data == nullptr ? 0 : size),
      _identifier(std::move(identifier)) {}

ReadResult MemorySource::Read(std::uint64_t offset, std::size_t size,
                              void* dst) {
    if (_data == nullptr || dst == nullptr) {
        return ReadResult::Failed();
    }
    // A zero-length read at a valid offset succeeds. A reader that plans an
    // empty range should not have to special-case it before calling.
    if (offset > _size) {
        return ReadResult::End();
    }
    if (offset == _size) {
        return size == 0 ? ReadResult::Ok(0) : ReadResult::End();
    }

    const std::uint64_t available = _size - offset;
    const std::size_t toCopy =
        static_cast<std::size_t>(std::min<std::uint64_t>(available, size));
    if (toCopy > 0) {
        std::memcpy(dst, _data + offset, toCopy);
    }
    return toCopy == size ? ReadResult::Ok(toCopy) : ReadResult::Short(toCopy);
}

// --- LocalFileSource -------------------------------------------------------

namespace {

// 64-bit file positioning. Plain fseek/ftell take a `long`, which is 32 bits
// on Windows and caps every offset at 2 GB -- in a library whose reason to
// exist is rasters larger than memory. The failure would also be quiet: a
// truncated `long` seeks somewhere valid and returns the wrong pixels.

int SeekTo(std::FILE* file, std::uint64_t offset) {
#if defined(_WIN32)
    return ::_fseeki64(file, static_cast<__int64>(offset), SEEK_SET);
#else
    return ::fseeko(file, static_cast<off_t>(offset), SEEK_SET);
#endif
}

bool GetFileSize(std::FILE* file, std::uint64_t& size) {
#if defined(_WIN32)
    if (::_fseeki64(file, 0, SEEK_END) != 0) {
        return false;
    }
    const __int64 end = ::_ftelli64(file);
#else
    if (::fseeko(file, 0, SEEK_END) != 0) {
        return false;
    }
    const off_t end = ::ftello(file);
#endif
    if (end < 0) {
        return false;
    }
    size = static_cast<std::uint64_t>(end);
    return true;
}

}  // namespace

LocalFileSource::LocalFileSource(const std::string& path) : _path(path) {
    // Binary mode is not optional on Windows: a text-mode read would translate
    // CRLF inside a TIFF and corrupt every offset past the first 0x0d 0x0a.
#if defined(_WIN32)
    if (::fopen_s(&_file, path.c_str(), "rb") != 0) {
        _file = nullptr;
    }
#else
    _file = std::fopen(path.c_str(), "rb");
#endif
    if (_file == nullptr) {
        return;
    }
    if (!GetFileSize(_file, _size)) {
        _size = 0;
    }
}

LocalFileSource::~LocalFileSource() {
    if (_file != nullptr) {
        std::fclose(_file);
    }
}

ReadResult LocalFileSource::Read(std::uint64_t offset, std::size_t size,
                                 void* dst) {
    if (_file == nullptr || dst == nullptr) {
        return ReadResult::Failed();
    }
    if (offset > _size) {
        return ReadResult::End();
    }
    if (offset == _size) {
        return size == 0 ? ReadResult::Ok(0) : ReadResult::End();
    }
    if (SeekTo(_file, offset) != 0) {
        return ReadResult::Failed();
    }
    const std::size_t got = std::fread(dst, 1, size, _file);
    if (got == size) {
        return ReadResult::Ok(got);
    }
    // A short count with no error flag is a truncated source, which is a
    // short read; a set error flag is a failure. The two need different
    // diagnostics, so they are not collapsed here.
    if (std::ferror(_file) != 0) {
        return ReadResult::Failed();
    }
    return ReadResult::Short(got);
}

// --- RecordingSource -------------------------------------------------------

RecordingSource::RecordingSource(RandomAccessSource& inner) : _inner(inner) {}

ReadResult RecordingSource::Read(std::uint64_t offset, std::size_t size,
                                 void* dst) {
    // Recorded before delegating, so a range that fails still shows up. A read
    // plan that asks for the wrong bytes is a defect whether or not the source
    // could satisfy it.
    _ranges.push_back(Range{offset, size});
    _bytesRead += size;
    return _inner.Read(offset, size, dst);
}

std::uint64_t RecordingSource::GetDistinctBytesRead() const {
    if (_ranges.empty()) {
        return 0;
    }
    std::vector<Range> sorted = _ranges;
    std::sort(sorted.begin(), sorted.end(),
              [](const Range& left, const Range& right) {
                  return left.offset < right.offset;
              });

    std::uint64_t distinct = 0;
    std::uint64_t coveredTo = 0;
    bool started = false;
    for (const Range& range : sorted) {
        if (range.size == 0) {
            continue;
        }
        const std::uint64_t end = range.offset + range.size;
        if (!started || range.offset >= coveredTo) {
            distinct += range.size;
            coveredTo = end;
            started = true;
        } else if (end > coveredTo) {
            distinct += end - coveredTo;
            coveredTo = end;
        }
    }
    return distinct;
}

bool RecordingSource::DidReadRange(std::uint64_t offset,
                                   std::size_t size) const {
    const std::uint64_t queryEnd = offset + size;
    for (const Range& range : _ranges) {
        const std::uint64_t rangeEnd = range.offset + range.size;
        if (range.offset < queryEnd && offset < rangeEnd) {
            return true;
        }
    }
    return false;
}

void RecordingSource::Reset() {
    _ranges.clear();
    _bytesRead = 0;
}

}  // namespace usdraster

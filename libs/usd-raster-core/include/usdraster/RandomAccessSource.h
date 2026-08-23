#ifndef USDRASTER_RANDOMACCESSSOURCE_H
#define USDRASTER_RANDOMACCESSSOURCE_H

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace usdraster {

/// The outcome of one byte-range read.
enum class ReadStatus {
    Ok,
    /// Fewer bytes than requested were available, and the source is not at its
    /// end. Reported, never retried inside the reader: retry policy is
    /// transport policy, and transport belongs to the resolver. See ADR-0002.
    ShortRead,
    /// The requested offset is at or past the end of the source.
    EndOfSource,
    /// The source could not be read at all.
    Failed
};

struct ReadResult {
    ReadStatus status = ReadStatus::Failed;
    std::size_t bytesRead = 0;

    bool IsOk() const { return status == ReadStatus::Ok; }

    static ReadResult Ok(std::size_t bytes) {
        return ReadResult{ReadStatus::Ok, bytes};
    }
    static ReadResult Short(std::size_t bytes) {
        return ReadResult{ReadStatus::ShortRead, bytes};
    }
    static ReadResult End() { return ReadResult{ReadStatus::EndOfSource, 0}; }
    static ReadResult Failed() { return ReadResult{ReadStatus::Failed, 0}; }
};

const char* GetReadStatusName(ReadStatus status);

/// The byte source every reader consumes.
///
/// A reader never opens a file itself. That is what lets the same decoder run
/// against a local file, an in-memory fixture, and a resolver-provided remote
/// asset with no knowledge of which -- and it is why this interface carries no
/// path, no URL, and no scheme. `usdGeoTiff` never sees an `ArAsset`; the
/// adapter that does lives in the plugin bundle, which is the only place
/// OpenUSD is available. See docs/architecture/RESOLVER_SOURCE.md.
///
/// Threading: an implementation is not required to be thread-safe. A caller
/// reading from several threads gives each its own source.
class RandomAccessSource {
public:
    virtual ~RandomAccessSource() = default;

    RandomAccessSource(const RandomAccessSource&) = delete;
    RandomAccessSource& operator=(const RandomAccessSource&) = delete;

    /// The source's size in bytes, or 0 when it is unavailable.
    virtual std::uint64_t GetSize() const = 0;

    /// Read `size` bytes at `offset` into `dst`, which the caller owns and
    /// which must hold at least `size` bytes.
    virtual ReadResult Read(std::uint64_t offset, std::size_t size,
                            void* dst) = 0;

    /// A stable identifier for messages. Never a credential, a token, or a
    /// signed URL -- invariant 18 of the workspace contract -- so a remote
    /// source returns the identifier the resolver already exposed, not the URL
    /// it was signed with.
    virtual std::string GetIdentifier() const { return {}; }

protected:
    RandomAccessSource() = default;
};

/// A source over a caller-owned buffer.
///
/// Does not copy: the buffer must outlive the source. That is the opposite of
/// `RasterGrid`'s rule, and it is deliberate -- a fixture is a static array in
/// a test, and copying every fixture to read it would make the instrumented
/// byte counts measure the copy rather than the read.
class MemorySource : public RandomAccessSource {
public:
    MemorySource(const void* data, std::size_t size,
                 std::string identifier = "memory");

    std::uint64_t GetSize() const override { return _size; }
    ReadResult Read(std::uint64_t offset, std::size_t size, void* dst) override;
    std::string GetIdentifier() const override { return _identifier; }

private:
    const std::uint8_t* _data = nullptr;
    std::uint64_t _size = 0;
    std::string _identifier;
};

/// A source over a local file.
///
/// Opened once and read with positioned reads. No caching, no read-ahead, and
/// no retry: those are policy, and the point of the interface is that policy
/// lives on one side of it. A local file is the baseline a remote source is
/// compared against, so it must not quietly do more work than it was asked to.
class LocalFileSource : public RandomAccessSource {
public:
    explicit LocalFileSource(const std::string& path);
    ~LocalFileSource() override;

    bool IsOpen() const { return _file != nullptr; }

    std::uint64_t GetSize() const override { return _size; }
    ReadResult Read(std::uint64_t offset, std::size_t size, void* dst) override;
    std::string GetIdentifier() const override { return _path; }

private:
    std::string _path;
    std::FILE* _file = nullptr;
    std::uint64_t _size = 0;
};

/// A decorator that records every byte range requested of the source it wraps.
///
/// This is how I/O selectivity stops being a claim and becomes an assertion: a
/// window read from a tiled source must touch only the intersecting tiles, and
/// a read from a striped source must report its amplification rather than hide
/// it. Both are properties of which ranges were requested, and neither is
/// observable without recording them. See section 5 of
/// docs/architecture/RASTER_READER.md.
///
/// It ships in the library rather than in a test directory because every
/// module's tests need it and, from milestone 3, because the amplification
/// counters are part of what a read reports.
class RecordingSource : public RandomAccessSource {
public:
    struct Range {
        std::uint64_t offset = 0;
        std::size_t size = 0;
    };

    explicit RecordingSource(RandomAccessSource& inner);

    std::uint64_t GetSize() const override { return _inner.GetSize(); }
    ReadResult Read(std::uint64_t offset, std::size_t size, void* dst) override;
    std::string GetIdentifier() const override { return _inner.GetIdentifier(); }

    const std::vector<Range>& GetRanges() const { return _ranges; }

    /// How many `Read` calls were issued. Distinct from the byte count: a
    /// remote source pays per request as well as per byte, so coalescing that
    /// halves the call count matters even when the bytes are unchanged.
    std::size_t GetReadCount() const { return _ranges.size(); }

    /// Bytes actually requested of the inner source.
    std::uint64_t GetBytesRead() const { return _bytesRead; }

    /// Distinct bytes covered by the recorded ranges, counting an overlap
    /// once. Compared against `GetBytesRead`, this is what exposes a read plan
    /// that fetches the same bytes twice.
    std::uint64_t GetDistinctBytesRead() const;

    /// True when some byte of `[offset, offset + size)` was requested.
    bool DidReadRange(std::uint64_t offset, std::size_t size) const;

    void Reset();

private:
    RandomAccessSource& _inner;
    std::vector<Range> _ranges;
    std::uint64_t _bytesRead = 0;
};

}  // namespace usdraster

#endif  // USDRASTER_RANDOMACCESSSOURCE_H

#ifndef USDGEO_CACHEKEY_H
#define USDGEO_CACHEKEY_H

#include <cstdint>
#include <string>

namespace usdgeo {

/// Accumulates normalized inputs into a stable digest.
///
/// Two properties matter and neither is negotiable:
///
///   * **Determinism across hosts and runs.** The same inputs produce the same
///     digest on Windows, Linux, and macOS, in this process and the next one.
///     A generated cache entry is named by this digest, so a digest that
///     varied by host would make the cache useless on the second machine and,
///     worse, would let one host read another host's entry as a miss and
///     regenerate it forever.
///
///   * **Unambiguous framing.** Every field is length-prefixed and
///     type-tagged, so `AddString("ab") + AddString("c")` never collides with
///     `AddString("a") + AddString("bc")`. Concatenating raw bytes is the
///     classic way a cache key silently conflates two different requests.
///
/// This is not a cryptographic hash. It defends against accidental collision
/// between distinct requests, not against an adversary constructing one. If a
/// cache ever spans a trust boundary, that is the point at which this is
/// replaced, and the replacement gets its own ADR.
///
/// What goes in is normalized inputs, not raw ones: a file-format argument is
/// normalized before it reaches here, so that two spellings of the same
/// request produce one entry. See docs/architecture/FILE_FORMAT_ARGUMENTS.md
/// and docs/architecture/CACHE.md.
///
/// Credentials, tokens, authorization headers, and signed URLs never enter a
/// cache key. That is invariant 18 of the workspace contract; a digest built
/// from a signed URL would also change every time the signature was reissued,
/// so the rule and the behavior agree.
class CacheKey {
public:
    CacheKey();

    void AddString(const std::string& value);
    void AddUInt(std::uint64_t value);
    void AddInt(std::int64_t value);
    void AddBool(bool value);

    /// Append a double by its exact bit pattern, so 0.1 is the value it
    /// actually is rather than a rounded decimal spelling of it. NaN is
    /// canonicalized to one quiet NaN first: distinct NaN payloads mean the
    /// same thing to every consumer here, and letting them differ would key
    /// two identical requests to two entries.
    void AddDouble(double value);

    /// Mark a field absent. Distinct from any present value, so an omitted
    /// optional and a defaulted one are not the same key.
    void AddNull();

    std::uint64_t GetDigest() const { return _low; }

    /// The digest as 32 lowercase hex characters. This is the spelling that
    /// appears in generated paths and manifests.
    std::string GetHexDigest() const;

private:
    void Mix(std::uint8_t byte);
    void MixTag(char tag);
    void MixBytes(const void* data, std::size_t size);

    // Two independent FNV-1a lanes with different offset bases, concatenated
    // to 128 bits. One 64-bit lane is thin for a key space that names every
    // generated tile of every source; two are cheap and are enough that
    // accidental collision stops being something to reason about.
    std::uint64_t _low = 0;
    std::uint64_t _high = 0;
};

}  // namespace usdgeo

#endif  // USDGEO_CACHEKEY_H

#pragma once

#include "lupine/network/NetworkTypes.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace lupine {
namespace network {

/**
 * Append-only binary writer for building message payloads.
 *
 * Integers use LEB128 varints (unsigned) / zig-zag varints (signed) so small
 * values cost one byte; floats/doubles are fixed little-endian; strings and
 * blobs are length-prefixed. A single JSON value codec (WriteJson) reuses these
 * primitives so component properties, RPC arguments, and spawn payloads all
 * share one wire encoding - the engine already speaks JSON everywhere (component
 * props, ComponentRef::Call, NodeRef::Get/Set), so this is the natural currency.
 */
class ByteWriter {
public:
    ByteWriter() = default;

    void WriteU8(uint8_t value);
    void WriteBool(bool value);
    void WriteU16(uint16_t value);   // varint
    void WriteU32(uint32_t value);   // varint
    void WriteU64(uint64_t value);   // varint
    void WriteI32(int32_t value);    // zig-zag varint
    void WriteI64(int64_t value);    // zig-zag varint
    void WriteFloat(float value);
    void WriteDouble(double value);

    // ---- Compression helpers (lossy; used by the replicated transform/physics
    // components when compression is enabled). All are self-delimiting so a reader
    // recovers the value with the matching Read* call and no side channel. ----

    // IEEE-754 binary16 half float (2 bytes, fixed). ~3 significant decimal digits;
    // good for positions/scales within a few thousand units and for velocities.
    void WriteHalf(float value);

    // Fixed-point quantization of a value known to lie in [min, max], using `bits`
    // (1..32) of precision, written as a varint. Outside the range it is clamped.
    void WriteQuantizedFloat(float value, float min, float max, uint8_t bits);

    // Quantize an angle in radians to `bits` over the full [-pi, pi] circle.
    void WriteQuantizedAngle(float radians, uint8_t bits);

    // Smallest-three unit-quaternion compression: drops the largest-magnitude
    // component (reconstructed from the others), storing a 2-bit index plus three
    // 10-bit components in a fixed 4 bytes. The quaternion is assumed normalized.
    void WriteUnitQuat(float w, float x, float y, float z);

    void WriteString(const std::string& value);
    void WriteBytes(const uint8_t* data, size_t size);  // length-prefixed blob
    void WriteRaw(const uint8_t* data, size_t size);    // verbatim, no length prefix

    // Encode an arbitrary JSON value (null/bool/int/uint/float/string/array/object).
    void WriteJson(const nlohmann::json& value);

    const std::vector<uint8_t>& Data() const { return m_Data; }
    std::vector<uint8_t> Take() { return std::move(m_Data); }
    size_t Size() const { return m_Data.size(); }
    void Clear() { m_Data.clear(); }

private:
    void WriteVarU64(uint64_t value);

    std::vector<uint8_t> m_Data;
};

/**
 * Bounds-checked reader over a received frame. Every read validates remaining
 * length; on underrun (or a value that violates a hard bound such as an
 * over-long string or too-deeply-nested JSON) the reader latches a permanent
 * failure flag and subsequent reads return zero/empty. Callers check Ok() once
 * after decoding rather than per-field, and a corrupt/hostile frame can never
 * read out of bounds.
 */
class ByteReader {
public:
    ByteReader(const uint8_t* data, size_t size) : m_Data(data), m_Size(size) {}
    explicit ByteReader(const std::vector<uint8_t>& data)
        : m_Data(data.data()), m_Size(data.size()) {}

    uint8_t ReadU8();
    bool ReadBool();
    uint16_t ReadU16();
    uint32_t ReadU32();
    uint64_t ReadU64();
    int32_t ReadI32();
    int64_t ReadI64();
    float ReadFloat();
    double ReadDouble();

    // Inverse of the matching ByteWriter compression helpers.
    float ReadHalf();
    float ReadQuantizedFloat(float min, float max, uint8_t bits);
    float ReadQuantizedAngle(uint8_t bits);
    // Reconstructs the unit quaternion. Outputs are normalized.
    void ReadUnitQuat(float& w, float& x, float& y, float& z);

    std::string ReadString();
    std::vector<uint8_t> ReadBytes(size_t size);

    nlohmann::json ReadJson();

    bool Ok() const { return !m_Failed; }
    size_t Remaining() const { return m_Failed ? 0 : (m_Size - m_Pos); }
    bool AtEnd() const { return m_Pos >= m_Size; }

    // Maximum length accepted for a single string/blob and maximum JSON nesting
    // depth. Guards against memory-exhaustion / stack-overflow from a hostile
    // peer. 64 KiB matches the plan's per-string bound.
    static constexpr size_t kMaxStringLength = 64u * 1024u;
    static constexpr int kMaxJsonDepth = 32;

private:
    bool Require(size_t bytes);
    uint64_t ReadVarU64();
    nlohmann::json ReadJsonDepth(int depth);

    const uint8_t* m_Data = nullptr;
    size_t m_Size = 0;
    size_t m_Pos = 0;
    bool m_Failed = false;
};

} // namespace network
} // namespace lupine

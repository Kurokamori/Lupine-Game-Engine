#include "lupine/network/NetworkSerializer.hpp"
#include <cmath>
#include <cstring>

namespace lupine {
namespace network {

namespace {

// Tags identifying a JSON value's kind on the wire.
enum class JsonTag : uint8_t {
    Null = 0,
    False = 1,
    True = 2,
    Int = 3,      // zig-zag varint (signed integer)
    UInt = 4,     // varint (unsigned integer)
    Double = 5,   // 8 fixed bytes
    String = 6,   // varint length + UTF-8 bytes
    Array = 7,    // varint count + values
    Object = 8    // varint count + (key string + value) pairs
};

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

// IEEE-754 binary32 -> binary16, round-to-nearest-even, with overflow saturating
// to +/-inf and subnormals/zero handled. Used by WriteHalf.
uint16_t FloatToHalf(float value) {
    uint32_t x = 0;
    std::memcpy(&x, &value, sizeof(x));
    const uint32_t sign = (x >> 16) & 0x8000u;
    int32_t exponent = static_cast<int32_t>((x >> 23) & 0xFFu) - 127 + 15;
    uint32_t mantissa = x & 0x7FFFFFu;

    if (((x >> 23) & 0xFFu) == 0xFFu) {
        // Inf or NaN: preserve (NaN keeps a non-zero mantissa).
        return static_cast<uint16_t>(sign | 0x7C00u | (mantissa != 0 ? 0x200u : 0u));
    }
    if (exponent >= 0x1F) {
        return static_cast<uint16_t>(sign | 0x7C00u);  // Overflow -> inf.
    }
    if (exponent <= 0) {
        if (exponent < -10) {
            return static_cast<uint16_t>(sign);  // Too small -> signed zero.
        }
        mantissa |= 0x800000u;  // Restore the implicit leading 1.
        const int shift = 14 - exponent;
        uint32_t half = mantissa >> shift;
        // Round to nearest even on the bit shifted out.
        if ((mantissa >> (shift - 1)) & 1u) {
            half += 1u;
        }
        return static_cast<uint16_t>(sign | half);
    }
    uint16_t half = static_cast<uint16_t>(sign | (static_cast<uint32_t>(exponent) << 10) |
                                          (mantissa >> 13));
    if (mantissa & 0x1000u) {
        half += 1u;  // Round to nearest (carry into exponent handled by the bit add).
    }
    return half;
}

float HalfToFloat(uint16_t half) {
    const uint32_t sign = static_cast<uint32_t>(half & 0x8000u) << 16;
    uint32_t exponent = (half >> 10) & 0x1Fu;
    uint32_t mantissa = half & 0x3FFu;
    uint32_t bits = 0;
    if (exponent == 0) {
        if (mantissa == 0) {
            bits = sign;
        } else {
            exponent = 127 - 15 + 1;
            while ((mantissa & 0x400u) == 0u) {
                mantissa <<= 1;
                --exponent;
            }
            mantissa &= 0x3FFu;
            bits = sign | (exponent << 23) | (mantissa << 13);
        }
    } else if (exponent == 0x1Fu) {
        bits = sign | 0x7F800000u | (mantissa << 13);
    } else {
        bits = sign | ((exponent - 15 + 127) << 23) | (mantissa << 13);
    }
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

uint32_t QuantizeToRange(float value, float min, float max, uint8_t bits) {
    if (bits == 0) {
        return 0;
    }
    if (bits > 32) {
        bits = 32;
    }
    const uint32_t maxCode = (bits >= 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    if (max <= min) {
        return 0;
    }
    float t = (value - min) / (max - min);
    if (t < 0.0f) {
        t = 0.0f;
    } else if (t > 1.0f) {
        t = 1.0f;
    }
    return static_cast<uint32_t>(t * static_cast<float>(maxCode) + 0.5f);
}

float DequantizeFromRange(uint32_t code, float min, float max, uint8_t bits) {
    if (bits == 0) {
        return min;
    }
    if (bits > 32) {
        bits = 32;
    }
    const uint32_t maxCode = (bits >= 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    if (maxCode == 0) {
        return min;
    }
    const float t = static_cast<float>(code) / static_cast<float>(maxCode);
    return min + t * (max - min);
}

} // namespace

// ============================================================================
// ByteWriter
// ============================================================================

void ByteWriter::WriteVarU64(uint64_t value) {
    while (value >= 0x80u) {
        m_Data.push_back(static_cast<uint8_t>(value) | 0x80u);
        value >>= 7;
    }
    m_Data.push_back(static_cast<uint8_t>(value));
}

void ByteWriter::WriteU8(uint8_t value) {
    m_Data.push_back(value);
}

void ByteWriter::WriteBool(bool value) {
    m_Data.push_back(value ? 1u : 0u);
}

void ByteWriter::WriteU16(uint16_t value) {
    WriteVarU64(value);
}

void ByteWriter::WriteU32(uint32_t value) {
    WriteVarU64(value);
}

void ByteWriter::WriteU64(uint64_t value) {
    WriteVarU64(value);
}

void ByteWriter::WriteI32(int32_t value) {
    const uint32_t zig = (static_cast<uint32_t>(value) << 1) ^ static_cast<uint32_t>(value >> 31);
    WriteVarU64(zig);
}

void ByteWriter::WriteI64(int64_t value) {
    const uint64_t zig = (static_cast<uint64_t>(value) << 1) ^ static_cast<uint64_t>(value >> 63);
    WriteVarU64(zig);
}

void ByteWriter::WriteFloat(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int i = 0; i < 4; ++i) {
        m_Data.push_back(static_cast<uint8_t>(bits & 0xFFu));
        bits >>= 8;
    }
}

void ByteWriter::WriteDouble(double value) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int i = 0; i < 8; ++i) {
        m_Data.push_back(static_cast<uint8_t>(bits & 0xFFu));
        bits >>= 8;
    }
}

void ByteWriter::WriteHalf(float value) {
    const uint16_t half = FloatToHalf(value);
    m_Data.push_back(static_cast<uint8_t>(half & 0xFFu));
    m_Data.push_back(static_cast<uint8_t>((half >> 8) & 0xFFu));
}

void ByteWriter::WriteQuantizedFloat(float value, float min, float max, uint8_t bits) {
    WriteVarU64(QuantizeToRange(value, min, max, bits));
}

void ByteWriter::WriteQuantizedAngle(float radians, uint8_t bits) {
    // Wrap into [-pi, pi] so the full-circle range maps cleanly.
    float wrapped = std::fmod(radians + kPi, kTwoPi);
    if (wrapped < 0.0f) {
        wrapped += kTwoPi;
    }
    wrapped -= kPi;
    WriteVarU64(QuantizeToRange(wrapped, -kPi, kPi, bits));
}

void ByteWriter::WriteUnitQuat(float w, float x, float y, float z) {
    // Normalize defensively so reconstruction's sqrt stays valid.
    float len = std::sqrt(w * w + x * x + y * y + z * z);
    if (len < 1e-8f) {
        w = 1.0f;
        x = y = z = 0.0f;
        len = 1.0f;
    }
    w /= len; x /= len; y /= len; z /= len;

    const float comps[4] = {w, x, y, z};
    int largest = 0;
    float largestMag = std::fabs(comps[0]);
    for (int i = 1; i < 4; ++i) {
        const float mag = std::fabs(comps[i]);
        if (mag > largestMag) {
            largestMag = mag;
            largest = i;
        }
    }
    // q and -q are the same rotation; choose the sign that makes the dropped
    // (largest) component positive so it is unambiguously sqrt(1 - sum).
    const float sign = (comps[largest] < 0.0f) ? -1.0f : 1.0f;

    // The three retained components each lie in [-1/sqrt2, 1/sqrt2].
    constexpr float kRange = 0.70710678f;
    uint32_t packed = static_cast<uint32_t>(largest) & 0x3u;  // 2-bit index.
    int written = 0;
    for (int i = 0; i < 4; ++i) {
        if (i == largest) {
            continue;
        }
        const uint32_t code = QuantizeToRange(sign * comps[i], -kRange, kRange, 10);
        packed |= (code & 0x3FFu) << (2 + written * 10);
        ++written;
    }
    // Fixed 4 bytes, little-endian.
    for (int i = 0; i < 4; ++i) {
        m_Data.push_back(static_cast<uint8_t>((packed >> (8 * i)) & 0xFFu));
    }
}

void ByteWriter::WriteString(const std::string& value) {
    WriteVarU64(value.size());
    m_Data.insert(m_Data.end(), value.begin(), value.end());
}

void ByteWriter::WriteBytes(const uint8_t* data, size_t size) {
    WriteVarU64(size);
    if (data != nullptr && size > 0) {
        m_Data.insert(m_Data.end(), data, data + size);
    }
}

void ByteWriter::WriteRaw(const uint8_t* data, size_t size) {
    if (data != nullptr && size > 0) {
        m_Data.insert(m_Data.end(), data, data + size);
    }
}

void ByteWriter::WriteJson(const nlohmann::json& value) {
    switch (value.type()) {
        case nlohmann::json::value_t::null:
            WriteU8(static_cast<uint8_t>(JsonTag::Null));
            break;
        case nlohmann::json::value_t::boolean:
            WriteU8(static_cast<uint8_t>(value.get<bool>() ? JsonTag::True : JsonTag::False));
            break;
        case nlohmann::json::value_t::number_integer:
            WriteU8(static_cast<uint8_t>(JsonTag::Int));
            WriteI64(value.get<int64_t>());
            break;
        case nlohmann::json::value_t::number_unsigned:
            WriteU8(static_cast<uint8_t>(JsonTag::UInt));
            WriteU64(value.get<uint64_t>());
            break;
        case nlohmann::json::value_t::number_float:
            WriteU8(static_cast<uint8_t>(JsonTag::Double));
            WriteDouble(value.get<double>());
            break;
        case nlohmann::json::value_t::string:
            WriteU8(static_cast<uint8_t>(JsonTag::String));
            WriteString(value.get_ref<const std::string&>());
            break;
        case nlohmann::json::value_t::array:
            WriteU8(static_cast<uint8_t>(JsonTag::Array));
            WriteVarU64(value.size());
            for (const nlohmann::json& element : value) {
                WriteJson(element);
            }
            break;
        case nlohmann::json::value_t::object:
            WriteU8(static_cast<uint8_t>(JsonTag::Object));
            WriteVarU64(value.size());
            for (auto it = value.begin(); it != value.end(); ++it) {
                WriteString(it.key());
                WriteJson(it.value());
            }
            break;
        default:
            // binary / discarded JSON kinds are not used by the engine's value model.
            WriteU8(static_cast<uint8_t>(JsonTag::Null));
            break;
    }
}

// ============================================================================
// ByteReader
// ============================================================================

bool ByteReader::Require(size_t bytes) {
    if (m_Failed) {
        return false;
    }
    // Subtract rather than add: m_Pos + bytes can wrap for an attacker-supplied
    // length, which would let an out-of-bounds read pass this check.
    if (m_Pos > m_Size || bytes > m_Size - m_Pos) {
        m_Failed = true;
        return false;
    }
    return true;
}

uint64_t ByteReader::ReadVarU64() {
    uint64_t result = 0;
    int shift = 0;
    while (shift < 64) {
        if (!Require(1)) {
            return 0;
        }
        const uint8_t byte = m_Data[m_Pos++];
        result |= static_cast<uint64_t>(byte & 0x7Fu) << shift;
        if ((byte & 0x80u) == 0) {
            return result;
        }
        shift += 7;
    }
    m_Failed = true;
    return 0;
}

uint8_t ByteReader::ReadU8() {
    if (!Require(1)) {
        return 0;
    }
    return m_Data[m_Pos++];
}

bool ByteReader::ReadBool() {
    return ReadU8() != 0;
}

uint16_t ByteReader::ReadU16() {
    return static_cast<uint16_t>(ReadVarU64());
}

uint32_t ByteReader::ReadU32() {
    return static_cast<uint32_t>(ReadVarU64());
}

uint64_t ByteReader::ReadU64() {
    return ReadVarU64();
}

int32_t ByteReader::ReadI32() {
    const uint32_t zig = static_cast<uint32_t>(ReadVarU64());
    return static_cast<int32_t>((zig >> 1) ^ (~(zig & 1) + 1));
}

int64_t ByteReader::ReadI64() {
    const uint64_t zig = ReadVarU64();
    return static_cast<int64_t>((zig >> 1) ^ (~(zig & 1) + 1));
}

float ByteReader::ReadFloat() {
    if (!Require(4)) {
        return 0.0f;
    }
    uint32_t bits = 0;
    for (int i = 0; i < 4; ++i) {
        bits |= static_cast<uint32_t>(m_Data[m_Pos++]) << (8 * i);
    }
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

double ByteReader::ReadDouble() {
    if (!Require(8)) {
        return 0.0;
    }
    uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) {
        bits |= static_cast<uint64_t>(m_Data[m_Pos++]) << (8 * i);
    }
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

float ByteReader::ReadHalf() {
    if (!Require(2)) {
        return 0.0f;
    }
    const uint16_t half = static_cast<uint16_t>(m_Data[m_Pos] |
                                                (static_cast<uint16_t>(m_Data[m_Pos + 1]) << 8));
    m_Pos += 2;
    return HalfToFloat(half);
}

float ByteReader::ReadQuantizedFloat(float min, float max, uint8_t bits) {
    const uint32_t code = static_cast<uint32_t>(ReadVarU64());
    if (m_Failed) {
        return min;
    }
    return DequantizeFromRange(code, min, max, bits);
}

float ByteReader::ReadQuantizedAngle(uint8_t bits) {
    return ReadQuantizedFloat(-kPi, kPi, bits);
}

void ByteReader::ReadUnitQuat(float& w, float& x, float& y, float& z) {
    w = 1.0f; x = 0.0f; y = 0.0f; z = 0.0f;
    if (!Require(4)) {
        return;
    }
    uint32_t packed = 0;
    for (int i = 0; i < 4; ++i) {
        packed |= static_cast<uint32_t>(m_Data[m_Pos + i]) << (8 * i);
    }
    m_Pos += 4;

    const int largest = static_cast<int>(packed & 0x3u);
    constexpr float kRange = 0.70710678f;
    float comps[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float sumSq = 0.0f;
    int read = 0;
    for (int i = 0; i < 4; ++i) {
        if (i == largest) {
            continue;
        }
        const uint32_t code = (packed >> (2 + read * 10)) & 0x3FFu;
        comps[i] = DequantizeFromRange(code, -kRange, kRange, 10);
        sumSq += comps[i] * comps[i];
        ++read;
    }
    const float remainder = 1.0f - sumSq;
    comps[largest] = (remainder > 0.0f) ? std::sqrt(remainder) : 0.0f;

    w = comps[0]; x = comps[1]; y = comps[2]; z = comps[3];
    // Renormalize to absorb quantization error.
    const float len = std::sqrt(w * w + x * x + y * y + z * z);
    if (len > 1e-8f) {
        w /= len; x /= len; y /= len; z /= len;
    } else {
        w = 1.0f; x = y = z = 0.0f;
    }
}

std::string ByteReader::ReadString() {
    const uint64_t length = ReadVarU64();
    if (m_Failed || length > kMaxStringLength) {
        m_Failed = true;
        return std::string();
    }
    if (!Require(static_cast<size_t>(length))) {
        return std::string();
    }
    std::string result(reinterpret_cast<const char*>(m_Data + m_Pos), static_cast<size_t>(length));
    m_Pos += static_cast<size_t>(length);
    return result;
}

std::vector<uint8_t> ByteReader::ReadBytes(size_t size) {
    if (!Require(size)) {
        return std::vector<uint8_t>();
    }
    std::vector<uint8_t> result(m_Data + m_Pos, m_Data + m_Pos + size);
    m_Pos += size;
    return result;
}

nlohmann::json ByteReader::ReadJson() {
    return ReadJsonDepth(0);
}

nlohmann::json ByteReader::ReadJsonDepth(int depth) {
    if (m_Failed || depth > kMaxJsonDepth) {
        m_Failed = true;
        return nlohmann::json(nullptr);
    }

    const uint8_t tag = ReadU8();
    if (m_Failed) {
        return nlohmann::json(nullptr);
    }

    switch (static_cast<JsonTag>(tag)) {
        case JsonTag::Null:
            return nlohmann::json(nullptr);
        case JsonTag::False:
            return nlohmann::json(false);
        case JsonTag::True:
            return nlohmann::json(true);
        case JsonTag::Int:
            return nlohmann::json(ReadI64());
        case JsonTag::UInt:
            return nlohmann::json(ReadU64());
        case JsonTag::Double:
            return nlohmann::json(ReadDouble());
        case JsonTag::String:
            return nlohmann::json(ReadString());
        case JsonTag::Array: {
            const uint64_t count = ReadVarU64();
            // Every element costs at least its one-byte tag, so a count larger
            // than the bytes left in the frame is necessarily malformed. Rejecting
            // it up front stops a hostile count from driving the container's
            // growth ahead of the underrun that would eventually stop it.
            if (m_Failed || count > kMaxStringLength ||
                count > static_cast<uint64_t>(Remaining())) {
                m_Failed = true;
                return nlohmann::json(nullptr);
            }
            nlohmann::json array = nlohmann::json::array();
            for (uint64_t i = 0; i < count && !m_Failed; ++i) {
                nlohmann::json element = ReadJsonDepth(depth + 1);
                if (m_Failed) {
                    return nlohmann::json(nullptr);
                }
                array.push_back(std::move(element));
            }
            return array;
        }
        case JsonTag::Object: {
            const uint64_t count = ReadVarU64();
            // Each entry costs at least a one-byte key length plus a one-byte
            // value tag; bounding by the bytes left in the frame is conservative.
            if (m_Failed || count > kMaxStringLength ||
                count > static_cast<uint64_t>(Remaining())) {
                m_Failed = true;
                return nlohmann::json(nullptr);
            }
            nlohmann::json object = nlohmann::json::object();
            for (uint64_t i = 0; i < count && !m_Failed; ++i) {
                const std::string key = ReadString();
                if (m_Failed) {
                    return nlohmann::json(nullptr);
                }
                nlohmann::json value = ReadJsonDepth(depth + 1);
                if (m_Failed) {
                    return nlohmann::json(nullptr);
                }
                object[key] = std::move(value);
            }
            return object;
        }
        default:
            m_Failed = true;
            return nlohmann::json(nullptr);
    }
}

} // namespace network
} // namespace lupine

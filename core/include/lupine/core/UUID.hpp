#pragma once

#include <string>
#include <cstdint>

namespace lupine {
namespace core {

/**
 * Simple UUID class for unique identification of objects
 * Uses a 64-bit integer for simplicity
 */
class UUID {
public:
    UUID();
    explicit UUID(uint64_t uuid);

    operator uint64_t() const { return m_UUID; }
    
    bool operator==(const UUID& other) const { return m_UUID == other.m_UUID; }
    bool operator!=(const UUID& other) const { return m_UUID != other.m_UUID; }
    bool operator<(const UUID& other) const { return m_UUID < other.m_UUID; }

    std::string ToString() const;
    static UUID FromString(const std::string& str);

    bool IsValid() const { return m_UUID != 0; }

private:
    uint64_t m_UUID;
};

} // namespace core
} // namespace lupine

// Hash function for UUID to use in unordered_map
namespace std {
    template<>
    struct hash<lupine::core::UUID> {
        size_t operator()(const lupine::core::UUID& uuid) const {
            return hash<uint64_t>()(static_cast<uint64_t>(uuid));
        }
    };
}


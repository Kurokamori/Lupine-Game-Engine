#pragma once

#include "lupine/math/Math.hpp"
#include <nlohmann/json.hpp>

namespace lupine {
namespace core {

/**
 * LinkedProperty - A reusable utility for properties that can be linked or unlinked.
 * 
 * When linked, all values are synchronized to the same value.
 * When unlinked, each value can be edited independently.
 * 
 * Common use cases:
 * - Corner radius (4 corners: top-left, top-right, bottom-right, bottom-left)
 * - Border width (4 edges: top, right, bottom, left)
 * - Padding/Margin (4 sides)
 * 
 * Usage:
 *   LinkedProperty<4> cornerRadius;  // 4 corners
 *   LinkedProperty<4> borderWidth;   // 4 edges
 */
template<size_t N>
class LinkedProperty {
public:
    LinkedProperty()
        : m_Linked(true)
    {
        for (size_t i = 0; i < N; ++i) {
            m_Values[i] = 0.0f;
        }
    }

    explicit LinkedProperty(float initialValue, bool linked = true)
        : m_Linked(linked)
    {
        for (size_t i = 0; i < N; ++i) {
            m_Values[i] = initialValue;
        }
    }

    // Get/Set linked state
    bool IsLinked() const { return m_Linked; }
    void SetLinked(bool linked) { 
        m_Linked = linked;
        // When linking, sync all values to the first value
        if (linked && N > 0) {
            float firstValue = m_Values[0];
            for (size_t i = 1; i < N; ++i) {
                m_Values[i] = firstValue;
            }
        }
    }

    // Get individual value
    float Get(size_t index) const {
        if (index >= N) return 0.0f;
        return m_Values[index];
    }

    // Set individual value
    void Set(size_t index, float value) {
        if (index >= N) return;
        
        if (m_Linked) {
            // When linked, set all values
            for (size_t i = 0; i < N; ++i) {
                m_Values[i] = value;
            }
        } else {
            // When unlinked, set only the specified value
            m_Values[index] = value;
        }
    }

    // Set all values at once
    void SetAll(float value) {
        for (size_t i = 0; i < N; ++i) {
            m_Values[i] = value;
        }
    }

    // Get all values as array
    const float* GetValues() const { return m_Values; }

    // Get as Vec4 (for N=4)
    math::Vec4 AsVec4() const {
        static_assert(N == 4, "AsVec4 only works with LinkedProperty<4>");
        return math::Vec4(m_Values[0], m_Values[1], m_Values[2], m_Values[3]);
    }

    // Set from Vec4 (for N=4)
    void FromVec4(const math::Vec4& vec) {
        static_assert(N == 4, "FromVec4 only works with LinkedProperty<4>");
        m_Values[0] = vec.x;
        m_Values[1] = vec.y;
        m_Values[2] = vec.z;
        m_Values[3] = vec.w;
    }

    // Serialization
    nlohmann::json Serialize() const {
        nlohmann::json json;
        json["linked"] = m_Linked;
        json["values"] = nlohmann::json::array();
        for (size_t i = 0; i < N; ++i) {
            json["values"].push_back(m_Values[i]);
        }
        return json;
    }

    void Deserialize(const nlohmann::json& json) {
        if (json.contains("linked")) {
            m_Linked = json["linked"].get<bool>();
        }
        if (json.contains("values") && json["values"].is_array()) {
            for (size_t i = 0; i < N && i < json["values"].size(); ++i) {
                m_Values[i] = json["values"][i].get<float>();
            }
        }
    }

    // Equality operators
    bool operator==(const LinkedProperty<N>& other) const {
        if (m_Linked != other.m_Linked) return false;
        for (size_t i = 0; i < N; ++i) {
            if (m_Values[i] != other.m_Values[i]) return false;
        }
        return true;
    }

    bool operator!=(const LinkedProperty<N>& other) const {
        return !(*this == other);
    }

private:
    bool m_Linked;
    float m_Values[N];
};

// Common type aliases
using LinkedProperty4 = LinkedProperty<4>;

} // namespace core
} // namespace lupine

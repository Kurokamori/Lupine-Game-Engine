#pragma once

#include "lupine/save/SaveTypes.hpp"
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace lupine {
namespace save {

/**
 * ISaveFormat - pluggable encoder/decoder for the save envelope.
 *
 * A format turns the complete envelope JSON (header + data payload) into bytes
 * and back. The engine ships JSON (text + compact) and binary (CBOR, MessagePack)
 * implementations; games may install a custom ISaveFormat on the SaveGameManager
 * to use any container they like. Encode/Decode return false on failure rather
 * than throwing.
 */
class ISaveFormat {
public:
    virtual ~ISaveFormat() = default;
    virtual SaveFormatType Type() const = 0;
    virtual bool IsBinary() const = 0;
    virtual bool Encode(const nlohmann::json& envelope, std::vector<uint8_t>& out) const = 0;
    virtual bool Decode(const std::vector<uint8_t>& bytes, nlohmann::json& out) const = 0;
};

// Construct one of the built-in formats.
std::shared_ptr<ISaveFormat> CreateSaveFormat(SaveFormatType type);

/**
 * ISaveTransform - optional reversible byte transform applied after encoding and
 * before decoding (obfuscation, compression, encryption, ...). The manager holds
 * at most one transform; Apply runs on write, Revert on read. Games install a
 * custom transform to protect or compress their saves.
 */
class ISaveTransform {
public:
    virtual ~ISaveTransform() = default;
    virtual std::string Name() const = 0;
    virtual bool Apply(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) const = 0;
    virtual bool Revert(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) const = 0;
};

/**
 * XorObfuscationTransform - a ready-made reversible XOR with a repeating key.
 *
 * This is NOT cryptographically secure; it only deters casual save-file editing
 * and serves as a reference for custom ISaveTransform implementations. Apply and
 * Revert are the same operation.
 */
class XorObfuscationTransform : public ISaveTransform {
public:
    explicit XorObfuscationTransform(std::string key);
    std::string Name() const override { return "xor"; }
    bool Apply(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) const override;
    bool Revert(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) const override;

private:
    std::string m_Key;
};

} // namespace save
} // namespace lupine

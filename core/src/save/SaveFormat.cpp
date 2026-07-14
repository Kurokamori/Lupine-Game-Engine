#include "lupine/save/SaveFormat.hpp"

#include <string>

namespace lupine {
namespace save {

namespace {

// Pretty-printed and single-line JSON text encodings.
class JsonTextFormat : public ISaveFormat {
public:
    explicit JsonTextFormat(bool pretty) : m_Pretty(pretty) {}

    SaveFormatType Type() const override {
        return m_Pretty ? SaveFormatType::JsonText : SaveFormatType::JsonCompact;
    }
    bool IsBinary() const override { return false; }

    bool Encode(const nlohmann::json& envelope, std::vector<uint8_t>& out) const override {
        try {
            std::string text = envelope.dump(m_Pretty ? 2 : -1);
            out.assign(text.begin(), text.end());
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    bool Decode(const std::vector<uint8_t>& bytes, nlohmann::json& out) const override {
        try {
            out = nlohmann::json::parse(bytes.begin(), bytes.end());
            return out.is_object();
        } catch (const std::exception&) {
            return false;
        }
    }

private:
    bool m_Pretty;
};

// CBOR binary encoding (nlohmann::json::to_cbor / from_cbor).
class CborFormat : public ISaveFormat {
public:
    SaveFormatType Type() const override { return SaveFormatType::BinaryCbor; }
    bool IsBinary() const override { return true; }

    bool Encode(const nlohmann::json& envelope, std::vector<uint8_t>& out) const override {
        try {
            out = nlohmann::json::to_cbor(envelope);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    bool Decode(const std::vector<uint8_t>& bytes, nlohmann::json& out) const override {
        try {
            out = nlohmann::json::from_cbor(bytes);
            return out.is_object();
        } catch (const std::exception&) {
            return false;
        }
    }
};

// MessagePack binary encoding (nlohmann::json::to_msgpack / from_msgpack).
class MsgpackFormat : public ISaveFormat {
public:
    SaveFormatType Type() const override { return SaveFormatType::BinaryMsgpack; }
    bool IsBinary() const override { return true; }

    bool Encode(const nlohmann::json& envelope, std::vector<uint8_t>& out) const override {
        try {
            out = nlohmann::json::to_msgpack(envelope);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    bool Decode(const std::vector<uint8_t>& bytes, nlohmann::json& out) const override {
        try {
            out = nlohmann::json::from_msgpack(bytes);
            return out.is_object();
        } catch (const std::exception&) {
            return false;
        }
    }
};

} // namespace

std::shared_ptr<ISaveFormat> CreateSaveFormat(SaveFormatType type) {
    switch (type) {
        case SaveFormatType::JsonText:      return std::make_shared<JsonTextFormat>(true);
        case SaveFormatType::JsonCompact:   return std::make_shared<JsonTextFormat>(false);
        case SaveFormatType::BinaryCbor:    return std::make_shared<CborFormat>();
        case SaveFormatType::BinaryMsgpack: return std::make_shared<MsgpackFormat>();
    }
    return std::make_shared<JsonTextFormat>(true);
}

XorObfuscationTransform::XorObfuscationTransform(std::string key)
    : m_Key(std::move(key)) {
    if (m_Key.empty()) {
        m_Key = "lupine";
    }
}

bool XorObfuscationTransform::Apply(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) const {
    out.resize(in.size());
    const size_t keyLen = m_Key.size();
    for (size_t i = 0; i < in.size(); ++i) {
        out[i] = static_cast<uint8_t>(in[i] ^ static_cast<uint8_t>(m_Key[i % keyLen]));
    }
    return true;
}

bool XorObfuscationTransform::Revert(const std::vector<uint8_t>& in, std::vector<uint8_t>& out) const {
    return Apply(in, out);
}

} // namespace save
} // namespace lupine

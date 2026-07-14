#include "lupine/network/NetworkProtocol.hpp"

namespace lupine {
namespace network {
namespace protocol {

std::vector<uint8_t> MakeFrame(Channel channel, MsgType type,
                               const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> frame;
    frame.reserve(payload.size() + 2);
    frame.push_back(static_cast<uint8_t>(channel));
    frame.push_back(static_cast<uint8_t>(type));
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

bool ReadFrameHeader(ByteReader& reader, Channel& outChannel, MsgType& outType) {
    const uint8_t channelByte = reader.ReadU8();
    const uint8_t typeByte = reader.ReadU8();
    if (!reader.Ok()) {
        return false;
    }
    if (channelByte >= kChannelCount || typeByte > static_cast<uint8_t>(MsgType::Custom)) {
        return false;
    }
    outChannel = static_cast<Channel>(channelByte);
    outType = static_cast<MsgType>(typeByte);
    return true;
}

void EncodeHandshake(ByteWriter& writer, const HandshakeMsg& msg) {
    writer.WriteU32(msg.engineProtocol);
    writer.WriteU32(msg.gameProtocol);
    writer.WriteString(msg.gameId);
    writer.WriteU32(msg.selfId);
}

bool DecodeHandshake(ByteReader& reader, HandshakeMsg& out) {
    out.engineProtocol = reader.ReadU32();
    out.gameProtocol = reader.ReadU32();
    out.gameId = reader.ReadString();
    out.selfId = reader.ReadU32();
    return reader.Ok();
}

void EncodeWelcome(ByteWriter& writer, const WelcomeMsg& msg) {
    writer.WriteU32(msg.assignedPeerId);
    writer.WriteU32(msg.serverPeerId);
    writer.WriteU32(msg.resumeToken);
    writer.WriteU32(static_cast<uint32_t>(msg.existingPeers.size()));
    for (const PeerId peer : msg.existingPeers) {
        writer.WriteU32(peer);
    }
}

bool DecodeWelcome(ByteReader& reader, WelcomeMsg& out) {
    out.assignedPeerId = reader.ReadU32();
    out.serverPeerId = reader.ReadU32();
    out.resumeToken = reader.ReadU32();
    const uint32_t count = reader.ReadU32();
    if (!reader.Ok() || count > 0xFFFFu) {
        return false;
    }
    out.existingPeers.clear();
    out.existingPeers.reserve(count);
    for (uint32_t i = 0; i < count && reader.Ok(); ++i) {
        out.existingPeers.push_back(reader.ReadU32());
    }
    return reader.Ok();
}

void EncodeResume(ByteWriter& writer, const ResumeMsg& msg) {
    writer.WriteU32(msg.engineProtocol);
    writer.WriteU32(msg.gameProtocol);
    writer.WriteString(msg.gameId);
    writer.WriteU32(msg.peerId);
    writer.WriteU32(msg.resumeToken);
}

bool DecodeResume(ByteReader& reader, ResumeMsg& out) {
    out.engineProtocol = reader.ReadU32();
    out.gameProtocol = reader.ReadU32();
    out.gameId = reader.ReadString();
    out.peerId = reader.ReadU32();
    out.resumeToken = reader.ReadU32();
    return reader.Ok();
}

void EncodePeerEvent(ByteWriter& writer, const PeerEventMsg& msg) {
    writer.WriteU32(msg.peerId);
}

bool DecodePeerEvent(ByteReader& reader, PeerEventMsg& out) {
    out.peerId = reader.ReadU32();
    return reader.Ok();
}

void EncodePing(ByteWriter& writer, const PingMsg& msg) {
    writer.WriteU32(msg.token);
}

bool DecodePing(ByteReader& reader, PingMsg& out) {
    out.token = reader.ReadU32();
    return reader.Ok();
}

void EncodePong(ByteWriter& writer, const PongMsg& msg) {
    writer.WriteU32(msg.token);
}

bool DecodePong(ByteReader& reader, PongMsg& out) {
    out.token = reader.ReadU32();
    return reader.Ok();
}

} // namespace protocol
} // namespace network
} // namespace lupine

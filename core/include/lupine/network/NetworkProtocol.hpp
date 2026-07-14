#pragma once

#include "lupine/network/NetworkMessage.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include <cstdint>
#include <vector>

namespace lupine {
namespace network {

/**
 * Frame helpers and (de)serialization for the built-in session messages.
 *
 * Every frame on the wire is: [u8 channel][u8 MsgType][payload]. The transport
 * already delivers discrete length-delimited frames, so no extra length prefix
 * is needed here; the channel byte lets stream transports (WebSocket) preserve
 * the logical channel a packet was sent on.
 */
namespace protocol {

// Build a complete frame from an already-encoded payload.
std::vector<uint8_t> MakeFrame(Channel channel, MsgType type,
                               const std::vector<uint8_t>& payload);

// Read the channel + type header from a frame, leaving the reader positioned at
// the payload. Returns false if the frame is too short / malformed.
bool ReadFrameHeader(ByteReader& reader, Channel& outChannel, MsgType& outType);

void EncodeHandshake(ByteWriter& writer, const HandshakeMsg& msg);
bool DecodeHandshake(ByteReader& reader, HandshakeMsg& out);

void EncodeWelcome(ByteWriter& writer, const WelcomeMsg& msg);
bool DecodeWelcome(ByteReader& reader, WelcomeMsg& out);

void EncodeResume(ByteWriter& writer, const ResumeMsg& msg);
bool DecodeResume(ByteReader& reader, ResumeMsg& out);

void EncodePeerEvent(ByteWriter& writer, const PeerEventMsg& msg);
bool DecodePeerEvent(ByteReader& reader, PeerEventMsg& out);

void EncodePing(ByteWriter& writer, const PingMsg& msg);
bool DecodePing(ByteReader& reader, PingMsg& out);

void EncodePong(ByteWriter& writer, const PongMsg& msg);
bool DecodePong(ByteReader& reader, PongMsg& out);

} // namespace protocol
} // namespace network
} // namespace lupine

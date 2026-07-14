#pragma once

#include <cstdint>

namespace lupine {
namespace network {

class ByteWriter;
class ByteReader;

/**
 * Implemented by components that take part in state replication
 * (NetworkSynchronizer, NetworkTransform2D/3D, ...). The ReplicationManager
 * enumerates the INetworkSerializable components on each networked node in a
 * stable order (their order in the node's component list, identical on every
 * peer) and uses that index to route a gathered blob back to the right component
 * on the receiving side.
 */
class INetworkSerializable {
public:
    virtual ~INetworkSerializable() = default;

    // Authority side: write the current replicated state. Return false to be
    // omitted from this snapshot (e.g. nothing changed since the last send).
    //
    // When `force` is true the component writes its full state unconditionally
    // (ignoring change detection) and returns true. This backs the periodic
    // reliable keyframe and the initial state sent to a late joiner, so a dropped
    // unreliable delta can never strand a receiver at a stale value. The change-
    // detection baseline is updated either way, so a forced gather does not cause
    // the following delta to re-send the same data.
    virtual bool NetGather(ByteWriter& writer, bool force) = 0;

    // Non-authority side: consume a received state blob. serverTimeMs is the
    // authority's timestamp for the snapshot, used for interpolation timing.
    virtual void NetApply(ByteReader& reader, uint32_t serverTimeMs) = 0;
};

} // namespace network
} // namespace lupine

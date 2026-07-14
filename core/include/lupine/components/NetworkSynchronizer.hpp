#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/network/INetworkSerializable.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

namespace lupine {
namespace components {

/**
 * NetworkSynchronizer - declaratively replicates a list of node/component
 * properties from the authority to the other peers.
 *
 * Each entry in `replicatedProperties` names a property to keep in sync, either
 * as "ComponentType:propertyName" (a specific component) or just "propertyName"
 * (the first component that declares it - this covers script @export variables
 * and component custom properties). Values cross the wire as the engine's JSON
 * property representation, with per-property change detection so only properties
 * that actually changed since the last snapshot are sent.
 *
 * An entry may append "@hz" to cap how often that property is re-sent regardless
 * of how fast it changes (e.g. "Health:current@2" sends at most twice a second).
 * Properties without a rate send on every change. Periodic keyframes ignore the
 * cap so a receiver is always brought fully up to date.
 *
 * Transforms are better handled by NetworkTransform2D/3D (which interpolate);
 * use this for gameplay state (health, ammo, state flags, ...).
 */
class NetworkSynchronizer : public core::Component, public network::INetworkSerializable {
public:
    NetworkSynchronizer();
    explicit NetworkSynchronizer(const std::string& name);

    std::string GetTypeName() const override { return "NetworkSynchronizer"; }
    void DefineProperties() override;

    bool NetGather(network::ByteWriter& writer, bool force) override;
    void NetApply(network::ByteReader& reader, uint32_t serverTimeMs) override;

private:
    struct ResolvedProperty {
        core::Component* component = nullptr;
        std::string propertyName;
    };

    std::vector<std::string> GetEntries() const;
    ResolvedProperty Resolve(const std::string& entry) const;

    // Parse a trailing "@hz" send-rate cap off an entry, returning the rate (0 =
    // uncapped) and leaving `spec` as the bare "Type:prop" / "prop" string.
    static float ParseSendRate(const std::string& entry, std::string& outSpec);

    // entryIndex -> last value sent (authority-side change detection).
    std::unordered_map<uint32_t, nlohmann::json> m_LastSent;
    // entryIndex -> last send time (ms, monotonic) for rate-capped entries.
    std::unordered_map<uint32_t, double> m_LastSentTime;
};

} // namespace components
} // namespace lupine

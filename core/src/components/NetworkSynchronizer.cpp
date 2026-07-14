#include "lupine/components/NetworkSynchronizer.hpp"
#include "lupine/network/NetworkSerializer.hpp"
#include "lupine/core/Node.hpp"

#include <chrono>

namespace lupine {
namespace components {

namespace {
double NowMs() {
    using namespace std::chrono;
    return static_cast<double>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()) / 1000.0;
}
}  // namespace

NetworkSynchronizer::NetworkSynchronizer() : core::Component("NetworkSynchronizer") {
    DefineProperties();
}

NetworkSynchronizer::NetworkSynchronizer(const std::string& name) : core::Component(name) {
    DefineProperties();
}

void NetworkSynchronizer::DefineProperties() {
    if (m_PropertiesDefined) {
        return;
    }
    DefineProperty(core::PropertyDescriptor("replicatedProperties", core::PropertyValueType::StringArray,
                                            core::PropertyDefaultValue(std::vector<std::string>{})));
    m_PropertiesDefined = true;
}

std::vector<std::string> NetworkSynchronizer::GetEntries() const {
    return const_cast<NetworkSynchronizer*>(this)->GetPropertyValue<std::vector<std::string>>(
        "replicatedProperties");
}

float NetworkSynchronizer::ParseSendRate(const std::string& entry, std::string& outSpec) {
    const std::size_t at = entry.find('@');
    if (at == std::string::npos) {
        outSpec = entry;
        return 0.0f;
    }
    outSpec = entry.substr(0, at);
    try {
        return std::stof(entry.substr(at + 1));
    } catch (...) {
        return 0.0f;
    }
}

NetworkSynchronizer::ResolvedProperty NetworkSynchronizer::Resolve(const std::string& rawEntry) const {
    ResolvedProperty resolved;
    core::Node* owner = GetOwner();
    std::string entry;
    ParseSendRate(rawEntry, entry);  // Drop any "@hz" suffix before resolving.
    if (owner == nullptr || entry.empty()) {
        return resolved;
    }

    const std::size_t colon = entry.find(':');
    if (colon != std::string::npos) {
        const std::string typeName = entry.substr(0, colon);
        resolved.propertyName = entry.substr(colon + 1);
        resolved.component = owner->GetComponent(typeName).get();
        return resolved;
    }

    resolved.propertyName = entry;
    for (const std::shared_ptr<core::Component>& comp : owner->GetComponents()) {
        if (comp.get() == this) {
            continue;
        }
        if (comp->GetPropertyRegistry().HasProperty(entry)) {
            resolved.component = comp.get();
            break;
        }
    }
    return resolved;
}

bool NetworkSynchronizer::NetGather(network::ByteWriter& writer, bool force) {
    const std::vector<std::string> entries = GetEntries();

    network::ByteWriter changedBuf;
    uint32_t changedCount = 0;
    for (uint32_t i = 0; i < entries.size(); ++i) {
        ResolvedProperty resolved = Resolve(entries[i]);
        if (resolved.component == nullptr) {
            continue;
        }
        const core::ComponentProperty* prop =
            resolved.component->GetPropertyRegistry().GetProperty(resolved.propertyName);
        if (prop == nullptr) {
            continue;
        }
        const nlohmann::json value = prop->GetValueAsJson();
        auto it = m_LastSent.find(i);
        // A keyframe (force) writes every property so a receiver that missed an
        // unreliable delta is fully resynchronised, but leaves the change-
        // detection baseline untouched so it does not perturb the delta stream.
        if (force) {
            changedBuf.WriteU32(i);
            changedBuf.WriteJson(value);
            ++changedCount;
            continue;
        }
        if (it != m_LastSent.end() && it->second == value) {
            continue;  // Unchanged since the last delta.
        }
        // Optional per-property send-rate cap ("entry@hz").
        std::string spec;
        const float rateHz = ParseSendRate(entries[i], spec);
        if (rateHz > 0.0f) {
            const double now = NowMs();
            auto t = m_LastSentTime.find(i);
            const double minInterval = 1000.0 / rateHz;
            if (t != m_LastSentTime.end() && (now - t->second) < minInterval) {
                continue;  // Too soon to re-send this property.
            }
            m_LastSentTime[i] = now;
        }
        changedBuf.WriteU32(i);
        changedBuf.WriteJson(value);
        m_LastSent[i] = value;
        ++changedCount;
    }

    writer.WriteU32(changedCount);
    writer.WriteRaw(changedBuf.Data().data(), changedBuf.Size());
    return changedCount > 0;
}

void NetworkSynchronizer::NetApply(network::ByteReader& reader, uint32_t serverTimeMs) {
    (void)serverTimeMs;
    const uint32_t count = reader.ReadU32();
    if (!reader.Ok() || count > 0xFFFFu) {
        return;
    }
    const std::vector<std::string> entries = GetEntries();
    for (uint32_t c = 0; c < count && reader.Ok(); ++c) {
        const uint32_t index = reader.ReadU32();
        const nlohmann::json value = reader.ReadJson();
        if (!reader.Ok() || index >= entries.size()) {
            continue;
        }
        ResolvedProperty resolved = Resolve(entries[index]);
        if (resolved.component == nullptr) {
            continue;
        }
        core::ComponentProperty* prop =
            resolved.component->GetPropertyRegistry().GetProperty(resolved.propertyName);
        if (prop == nullptr) {
            continue;
        }
        prop->SetValueFromJson(value);
        // Let the target component mirror the value into its internal state / visuals.
        resolved.component->OnPropertyChanged(resolved.propertyName, value);
    }
}

} // namespace components
} // namespace lupine

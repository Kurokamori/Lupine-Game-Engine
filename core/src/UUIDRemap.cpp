#include "lupine/core/UUIDRemap.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"

namespace lupine {
namespace core {
namespace uuidremap {

void ReassignRecursive(const std::shared_ptr<Node>& node, UUIDRemapTable& outRemap) {
    if (!node) return;

    const std::string oldNodeUuid = node->GetUUID().ToString();
    const core::UUID newNodeUuid;
    const_cast<core::UUID&>(node->GetUUID()) = newNodeUuid;
    outRemap[oldNodeUuid] = newNodeUuid.ToString();

    for (const std::shared_ptr<Component>& component : node->GetComponents()) {
        if (!component) continue;
        const std::string oldCompUuid = component->GetUUID().ToString();
        const core::UUID newCompUuid;
        const_cast<core::UUID&>(component->GetUUID()) = newCompUuid;
        outRemap[oldCompUuid] = newCompUuid.ToString();
    }

    for (const std::shared_ptr<Node>& child : node->GetChildren()) {
        ReassignRecursive(child, outRemap);
    }
}

void RemapConnectionTargetsRecursive(const std::shared_ptr<Node>& node, const UUIDRemapTable& remap) {
    if (!node) return;

    node->RemapPendingConnectionTargets(remap);

    for (const std::shared_ptr<Component>& component : node->GetComponents()) {
        if (component) {
            component->RemapPendingConnectionTargets(remap);
        }
    }

    for (const std::shared_ptr<Node>& child : node->GetChildren()) {
        RemapConnectionTargetsRecursive(child, remap);
    }
}

void RegenerateSubtree(const std::shared_ptr<Node>& node) {
    if (!node) return;

    UUIDRemapTable remap;
    ReassignRecursive(node, remap);
    RemapConnectionTargetsRecursive(node, remap);
}

void RegenerateAdoptedSubtree(const std::shared_ptr<Node>& node, const std::string& sourceRootUuid) {
    if (!node) return;

    UUIDRemapTable remap;

    // The instance keeps its own UUID, so the source root's UUID maps onto it rather than onto a
    // fresh one. Connections authored inside the source that pointed at its root then land on the
    // instance instead of dangling.
    if (!sourceRootUuid.empty()) {
        remap[sourceRootUuid] = node->GetUUID().ToString();
    }

    for (const std::shared_ptr<Component>& component : node->GetComponents()) {
        if (!component) continue;
        const std::string oldCompUuid = component->GetUUID().ToString();
        const core::UUID newCompUuid;
        const_cast<core::UUID&>(component->GetUUID()) = newCompUuid;
        remap[oldCompUuid] = newCompUuid.ToString();
    }

    for (const std::shared_ptr<Node>& child : node->GetChildren()) {
        ReassignRecursive(child, remap);
    }

    RemapConnectionTargetsRecursive(node, remap);
}

} // namespace uuidremap
} // namespace core
} // namespace lupine

#include "lupine/components/CheckList.hpp"
#include "lupine/components/Checkbox.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/OBB.hpp"
#include "lupine/math/Quat.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

CheckList::CheckList()
    : UIControl("CheckList")
    , m_LayoutDirty(true)
    , m_AutoCreateCheckboxes(false)
{
}

CheckList::CheckList(const std::string& name)
    : UIControl(name)
    , m_LayoutDirty(true)
    , m_AutoCreateCheckboxes(false)
{
}

CheckList::~CheckList() {
    DestroyAutoCreatedCheckboxes();
}

void CheckList::DefineProperties() {
    // Shared UIControl layout properties (anchors/size-flags/useUISpace + width/height).
    DefineUIControlProperties(100.0f, 100.0f, "useUISpace", "Layout");

    // Group
    DefineProperty(PROPERTY_DEFAULT_GROUP(groupName, String, std::string("default"), "Group"));

    // Layout
    DefineProperty(PROPERTY_ENUM_GROUP(orientation, 0, "Layout", Vertical, Horizontal));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(spacing, 10.0f, 0.0f, 200.0f, 1.0f, "Layout"));

    // Rendering
    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));

    // Items
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoCreateCheckboxes, Bool, false, "Items"));
    // Items stored as JSON array - no special macro needed
}

void CheckList::OnAwake() {
    m_GroupName = GetPropertyValue<std::string>("groupName");
    m_AutoCreateCheckboxes = GetPropertyValue<bool>("autoCreateCheckboxes");

    // Load items
    const ComponentProperty* itemsProp = m_CustomProperties.GetProperty("items");
    if (itemsProp) {
        nlohmann::json itemsJson = itemsProp->GetValueAsJson();
        if (itemsJson.is_array()) {
            m_Items.clear();
            for (const auto& item : itemsJson) {
                if (item.is_string()) {
                    m_Items.push_back(item.get<std::string>());
                }
            }
            m_ItemChecked.assign(m_Items.size(), false);
        }
    }

    m_LayoutDirty = true;
}

void CheckList::OnReady() {
    // Create checkboxes from items if auto-create is enabled
    if (m_AutoCreateCheckboxes && !m_Items.empty()) {
        CreateCheckboxesFromItems();
    }

    // Find and register existing checkboxes with matching group name
    if (GetOwner()) {
        auto children = GetOwner()->GetChildren();
        for (auto& child : children) {
            auto checkbox = child->GetComponent<Checkbox>();
            if (checkbox && checkbox->GetGroupName() == m_GroupName) {
                RegisterCheckbox(checkbox.get());
            }
        }
    }

    m_LayoutDirty = true;
}

void CheckList::OnUpdate(float deltaTime) {
    if (!IsEnabled()) {
        return;
    }

    UIControl::OnUpdate(deltaTime);

    if (m_LayoutDirty) {
        UpdateLayout();
        m_LayoutDirty = false;
    }
}

void CheckList::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    UIControl::OnPropertyChanged(propertyName, newValue);

    if (propertyName == "groupName") {
        m_GroupName = newValue.get<std::string>();
        // Re-register checkboxes with new group name
        m_Checkboxes.clear();
        if (GetOwner()) {
            auto children = GetOwner()->GetChildren();
            for (auto& child : children) {
                auto checkbox = child->GetComponent<Checkbox>();
                if (checkbox && checkbox->GetGroupName() == m_GroupName) {
                    RegisterCheckbox(checkbox.get());
                }
            }
        }
    } else if (propertyName == "orientation" || propertyName == "spacing") {
        m_LayoutDirty = true;
    } else if (propertyName == "autoCreateCheckboxes") {
        bool newAutoCreate = newValue.get<bool>();
        if (newAutoCreate != m_AutoCreateCheckboxes) {
            m_AutoCreateCheckboxes = newAutoCreate;
            if (m_AutoCreateCheckboxes && !m_Items.empty()) {
                CreateCheckboxesFromItems();
            } else if (!m_AutoCreateCheckboxes) {
                DestroyAutoCreatedCheckboxes();
            }
        }
    } else if (propertyName == "items") {
        if (newValue.is_array()) {
            m_Items.clear();
            for (const auto& item : newValue) {
                if (item.is_string()) {
                    m_Items.push_back(item.get<std::string>());
                }
            }
            m_ItemChecked.assign(m_Items.size(), false);
            if (m_AutoCreateCheckboxes) {
                DestroyAutoCreatedCheckboxes();
                CreateCheckboxesFromItems();
            }
        }
    }
}

// ===== Property Accessors =====

std::string CheckList::GetGroupName() const {
    return GetPropertyValue<std::string>("groupName");
}

void CheckList::SetGroupName(const std::string& groupName) {
    m_GroupName = groupName;
    SetPropertyValue<std::string>("groupName", groupName);
}

CheckListOrientation CheckList::GetOrientation() const {
    return static_cast<CheckListOrientation>(GetPropertyValue<int>("orientation"));
}

void CheckList::SetOrientation(CheckListOrientation orientation) {
    SetPropertyValue<int>("orientation", static_cast<int>(orientation));
    m_LayoutDirty = true;
}

float CheckList::GetSpacing() const {
    return GetPropertyValue<float>("spacing");
}

void CheckList::SetSpacing(float spacing) {
    SetPropertyValue<float>("spacing", spacing);
    m_LayoutDirty = true;
}

int CheckList::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void CheckList::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int CheckList::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void CheckList::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

// GetUseUISpace/SetUseUISpace are provided by the UIControl base class.

// ===== Checkbox Registration =====

void CheckList::RegisterCheckbox(Checkbox* checkbox) {
    if (!checkbox) {
        return;
    }

    // Check if already registered
    auto it = std::find(m_Checkboxes.begin(), m_Checkboxes.end(), checkbox);
    if (it != m_Checkboxes.end()) {
        return;
    }

    m_Checkboxes.push_back(checkbox);
    checkbox->SetCheckList(this);
    m_LayoutDirty = true;

}

void CheckList::UnregisterCheckbox(Checkbox* checkbox) {
    if (!checkbox) {
        return;
    }

    auto it = std::find(m_Checkboxes.begin(), m_Checkboxes.end(), checkbox);
    if (it != m_Checkboxes.end()) {
        m_Checkboxes.erase(it);
        checkbox->SetCheckList(nullptr);
        m_LayoutDirty = true;

    }
}

void CheckList::OnCheckboxStateChanged(Checkbox*) {
    NotifyGroupStateChanged();
}

// ===== Group Operations =====

void CheckList::CheckAll() {
    for (Checkbox* checkbox : m_Checkboxes) {
        if (checkbox && checkbox->IsButtonEnabled()) {
            checkbox->SetChecked(true);
        }
    }
    std::fill(m_ItemChecked.begin(), m_ItemChecked.end(), true);
    NotifyGroupStateChanged();
}

void CheckList::UncheckAll() {
    for (Checkbox* checkbox : m_Checkboxes) {
        if (checkbox && checkbox->IsButtonEnabled()) {
            checkbox->SetChecked(false);
        }
    }
    std::fill(m_ItemChecked.begin(), m_ItemChecked.end(), false);
    NotifyGroupStateChanged();
}

void CheckList::ToggleAll() {
    for (Checkbox* checkbox : m_Checkboxes) {
        if (checkbox && checkbox->IsButtonEnabled()) {
            checkbox->Toggle();
        }
    }
    for (size_t i = 0; i < m_ItemChecked.size(); ++i) {
        m_ItemChecked[i] = !m_ItemChecked[i];
    }
    NotifyGroupStateChanged();
}

// ===== State Queries =====

bool CheckList::AreAllChecked() const {
    if (m_Checkboxes.empty()) {
        if (m_ItemChecked.empty()) {
            return false;
        }
        for (bool checked : m_ItemChecked) {
            if (!checked) {
                return false;
            }
        }
        return true;
    }

    for (const Checkbox* checkbox : m_Checkboxes) {
        if (checkbox && !checkbox->IsChecked()) {
            return false;
        }
    }
    return true;
}

bool CheckList::AreAllUnchecked() const {
    if (m_Checkboxes.empty()) {
        for (bool checked : m_ItemChecked) {
            if (checked) {
                return false;
            }
        }
        return true;
    }

    for (const Checkbox* checkbox : m_Checkboxes) {
        if (checkbox && checkbox->IsChecked()) {
            return false;
        }
    }
    return true;
}

bool CheckList::IsAnyChecked() const {
    if (m_Checkboxes.empty()) {
        for (bool checked : m_ItemChecked) {
            if (checked) {
                return true;
            }
        }
        return false;
    }

    for (const Checkbox* checkbox : m_Checkboxes) {
        if (checkbox && checkbox->IsChecked()) {
            return true;
        }
    }
    return false;
}

int CheckList::GetCheckedCount() const {
    if (m_Checkboxes.empty()) {
        int count = 0;
        for (bool checked : m_ItemChecked) {
            if (checked) {
                ++count;
            }
        }
        return count;
    }

    int count = 0;
    for (const Checkbox* checkbox : m_Checkboxes) {
        if (checkbox && checkbox->IsChecked()) {
            ++count;
        }
    }
    return count;
}

int CheckList::GetTotalCount() const {
    if (m_Checkboxes.empty()) {
        return static_cast<int>(m_Items.size());
    }
    return static_cast<int>(m_Checkboxes.size());
}

std::vector<Checkbox*> CheckList::GetCheckedCheckboxes() const {
    std::vector<Checkbox*> result;
    for (Checkbox* checkbox : m_Checkboxes) {
        if (checkbox && checkbox->IsChecked()) {
            result.push_back(checkbox);
        }
    }
    return result;
}

std::vector<Checkbox*> CheckList::GetUncheckedCheckboxes() const {
    std::vector<Checkbox*> result;
    for (Checkbox* checkbox : m_Checkboxes) {
        if (checkbox && !checkbox->IsChecked()) {
            result.push_back(checkbox);
        }
    }
    return result;
}

std::vector<int> CheckList::GetCheckedValues() const {
    std::vector<int> result;
    for (const Checkbox* checkbox : m_Checkboxes) {
        if (checkbox && checkbox->IsChecked()) {
            result.push_back(checkbox->GetCheckboxValue());
        }
    }
    return result;
}

// ===== Item Management =====

void CheckList::AddItem(const std::string& text, int value) {
    m_Items.push_back(text);
    m_ItemChecked.push_back(false);

    // Update property
    nlohmann::json itemsJson = nlohmann::json::array();
    for (const auto& item : m_Items) {
        itemsJson.push_back(item);
    }
    SetPropertyValue<nlohmann::json>("items", itemsJson);

    if (m_AutoCreateCheckboxes) {
        // Create a new checkbox for this item
        if (GetOwner()) {
            auto checkboxNode = std::make_shared<Node2D>(text);
            auto checkbox = std::make_shared<Checkbox>(text);
            checkbox->SetText(text);
            checkbox->SetGroupName(m_GroupName);
            checkbox->SetCheckboxValue(value >= 0 ? value : static_cast<int>(m_Items.size() - 1));

            checkboxNode->AddComponent(checkbox);
            m_AutoCreatedNodes.push_back(checkboxNode);
            GetOwner()->AddChild(checkboxNode);

            RegisterCheckbox(checkbox.get());
        }
    }

    m_LayoutDirty = true;
}

void CheckList::RemoveItem(int index) {
    if (index < 0 || index >= static_cast<int>(m_Items.size())) {
        return;
    }

    m_Items.erase(m_Items.begin() + index);
    if (index < static_cast<int>(m_ItemChecked.size())) {
        m_ItemChecked.erase(m_ItemChecked.begin() + index);
    }

    // Update property
    nlohmann::json itemsJson = nlohmann::json::array();
    for (const auto& item : m_Items) {
        itemsJson.push_back(item);
    }
    SetPropertyValue<nlohmann::json>("items", itemsJson);

    if (m_AutoCreateCheckboxes && index < static_cast<int>(m_AutoCreatedNodes.size())) {
        auto& nodeToRemove = m_AutoCreatedNodes[index];
        if (nodeToRemove) {
            // Unregister checkbox
            auto checkbox = nodeToRemove->GetComponent<Checkbox>();
            if (checkbox) {
                UnregisterCheckbox(checkbox.get());
            }

            // Remove from parent
            if (nodeToRemove->GetParent()) {
                nodeToRemove->GetParent()->RemoveChild(nodeToRemove);
            }
        }
        m_AutoCreatedNodes.erase(m_AutoCreatedNodes.begin() + index);
    }

    m_LayoutDirty = true;
}

void CheckList::ClearItems() {
    m_Items.clear();
    m_ItemChecked.clear();

    // Update property
    SetPropertyValue<nlohmann::json>("items", nlohmann::json::array());

    if (m_AutoCreateCheckboxes) {
        DestroyAutoCreatedCheckboxes();
    }

    m_LayoutDirty = true;
}

int CheckList::GetItemCount() const {
    return static_cast<int>(m_Items.size());
}

std::vector<std::string> CheckList::GetItems() const {
    return m_Items;
}

void CheckList::SetItems(const std::vector<std::string>& items) {
    m_Items = items;
    m_ItemChecked.assign(m_Items.size(), false);

    // Update property
    nlohmann::json itemsJson = nlohmann::json::array();
    for (const auto& item : m_Items) {
        itemsJson.push_back(item);
    }
    SetPropertyValue<nlohmann::json>("items", itemsJson);

    if (m_AutoCreateCheckboxes) {
        DestroyAutoCreatedCheckboxes();
        CreateCheckboxesFromItems();
    }

    m_LayoutDirty = true;
}

bool CheckList::GetAutoCreateCheckboxes() const {
    return GetPropertyValue<bool>("autoCreateCheckboxes");
}

void CheckList::SetAutoCreateCheckboxes(bool autoCreate) {
    m_AutoCreateCheckboxes = autoCreate;
    SetPropertyValue<bool>("autoCreateCheckboxes", autoCreate);

    if (autoCreate && !m_Items.empty()) {
        CreateCheckboxesFromItems();
    } else if (!autoCreate) {
        DestroyAutoCreatedCheckboxes();
    }
}

void CheckList::SetOnGroupStateChangedCallback(GroupStateCallback callback) {
    m_OnGroupStateChangedCallback = callback;
}

// ===== Private Methods =====

void CheckList::UpdateLayout() {
    if (!GetOwner() || m_Checkboxes.empty()) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(GetOwner());
    if (!node2D) {
        return;
    }

    Vec2 basePosition = node2D->GetGlobalPosition();
    CheckListOrientation orientation = GetOrientation();
    float spacing = GetSpacing();

    float currentOffset = 0.0f;

    for (size_t i = 0; i < m_Checkboxes.size(); ++i) {
        Checkbox* checkbox = m_Checkboxes[i];
        if (!checkbox || !checkbox->GetOwner()) {
            continue;
        }

        Node2D* checkboxNode = dynamic_cast<Node2D*>(checkbox->GetOwner());
        if (!checkboxNode) {
            continue;
        }

        Vec2 checkboxSize = checkbox->CalculateTotalSize();
        Vec2 newPosition;

        if (orientation == CheckListOrientation::Vertical) {
            newPosition.x = basePosition.x;
            newPosition.y = basePosition.y + currentOffset + checkboxSize.y * 0.5f;
            currentOffset += checkboxSize.y + spacing;
        } else {
            newPosition.x = basePosition.x + currentOffset + checkboxSize.x * 0.5f;
            newPosition.y = basePosition.y;
            currentOffset += checkboxSize.x + spacing;
        }

        // Set local position relative to parent
        if (checkboxNode->GetParent() == GetOwner()) {
            Vec2 localPos;
            if (orientation == CheckListOrientation::Vertical) {
                localPos.x = 0.0f;
                localPos.y = currentOffset - checkboxSize.y * 0.5f - spacing;
            } else {
                localPos.x = currentOffset - checkboxSize.x * 0.5f - spacing;
                localPos.y = 0.0f;
            }
            checkboxNode->SetPosition(localPos);
        }
    }
}

void CheckList::CreateCheckboxesFromItems() {
    if (!GetOwner()) {
        return;
    }

    DestroyAutoCreatedCheckboxes();

    for (size_t i = 0; i < m_Items.size(); ++i) {
        const std::string& itemText = m_Items[i];

        auto checkboxNode = std::make_shared<Node2D>(itemText);
        auto checkbox = std::make_shared<Checkbox>(itemText);
        checkbox->SetText(itemText);
        checkbox->SetGroupName(m_GroupName);
        checkbox->SetCheckboxValue(static_cast<int>(i));

        checkboxNode->AddComponent(checkbox);
        m_AutoCreatedNodes.push_back(checkboxNode);
        GetOwner()->AddChild(checkboxNode);

        RegisterCheckbox(checkbox.get());
    }

    m_LayoutDirty = true;
}

void CheckList::DestroyAutoCreatedCheckboxes() {
    for (auto& node : m_AutoCreatedNodes) {
        if (node) {
            // Unregister checkbox
            auto checkbox = node->GetComponent<Checkbox>();
            if (checkbox) {
                UnregisterCheckbox(checkbox.get());
            }

            // Remove from parent
            if (node->GetParent()) {
                node->GetParent()->RemoveChild(node);
            }
        }
    }
    m_AutoCreatedNodes.clear();
}

void CheckList::NotifyGroupStateChanged() {
    if (m_OnGroupStateChangedCallback) {
        m_OnGroupStateChangedCallback(GetCheckedCount(), GetTotalCount());
    }
    Emit("group_changed", { GetCheckedCount(), GetTotalCount() });
}

void CheckList::DefineSignals() {
    RegisterSignal({"group_changed",
                    {{"checked", core::PropertyValueType::Int}, {"total", core::PropertyValueType::Int}},
                    "Emitted when the number of checked items in the group changes."});
}

// ===== IRenderableComponent Implementation =====

void CheckList::buildDrawCommands(RenderContext&) {
    // CheckList itself doesn't render anything; the checkboxes render themselves.
}

AABB CheckList::getWorldBounds() const {
    if (!GetOwner() || m_Checkboxes.empty()) {
        return AABB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(GetOwner());
    if (!node2D) {
        return AABB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    CheckListOrientation orientation = GetOrientation();
    float spacing = GetSpacing();

    float totalWidth = 0.0f;
    float totalHeight = 0.0f;
    float maxWidth = 0.0f;
    float maxHeight = 0.0f;

    for (const Checkbox* checkbox : m_Checkboxes) {
        if (!checkbox) continue;

        Vec2 size = checkbox->CalculateTotalSize();

        if (orientation == CheckListOrientation::Vertical) {
            totalHeight += size.y + spacing;
            maxWidth = std::max(maxWidth, size.x);
        } else {
            totalWidth += size.x + spacing;
            maxHeight = std::max(maxHeight, size.y);
        }
    }

    if (orientation == CheckListOrientation::Vertical) {
        totalHeight -= spacing; // Remove last spacing
        totalWidth = maxWidth;
    } else {
        totalWidth -= spacing;
        totalHeight = maxHeight;
    }

    Vec2 halfSize(totalWidth * 0.5f, totalHeight * 0.5f);

    return AABB(
        Vec3(position.x - halfSize.x, position.y - halfSize.y, -0.5f),
        Vec3(position.x + halfSize.x, position.y + halfSize.y, 0.5f)
    );
}

RenderLayer CheckList::getRenderLayer() const {
    return RenderLayer::UI;
}

SpatialType CheckList::getSpatialType() const {
    return GetUISpatialType();
}

OBB CheckList::getOrientedBounds() const {
    AABB aabb = getWorldBounds();
    Vec3 center = aabb.GetCenter();
    Vec3 extents = aabb.GetExtents();

    return OBB(center, extents, Quat::Identity());
}

bool CheckList::IntersectRay(const Ray& ray, float& outDistance) const {
    OBB obb = getOrientedBounds();

    Vec3 localRayOrigin = obb.rotation.Inverse() * (ray.origin - obb.center);
    Vec3 localRayDir = obb.rotation.Inverse() * ray.direction;

    AABB localAABB(
        Vec3(-obb.extents.x, -obb.extents.y, -obb.extents.z),
        Vec3(obb.extents.x, obb.extents.y, obb.extents.z)
    );

    Ray localRay(localRayOrigin, localRayDir);
    return localRay.IntersectAABB(localAABB, outDistance);
}

} // namespace components
} // namespace lupine


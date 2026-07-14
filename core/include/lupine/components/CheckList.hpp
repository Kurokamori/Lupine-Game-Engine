#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <string>
#include <vector>
#include <functional>

namespace lupine {
namespace components {

// Forward declaration
class Checkbox;

/**
 * Layout orientation for CheckList
 */
enum class CheckListOrientation {
    Vertical = 0,
    Horizontal = 1
};

/**
 * CheckList Component
 *
 * A container component that manages a group of Checkbox components.
 * Provides:
 * - Group management (check all, uncheck all, toggle all)
 * - Layout management (vertical/horizontal orientation)
 * - State queries (all checked, all unchecked, any checked)
 * - Callbacks for group state changes
 * - Auto-creation of checkboxes from item list
 *
 * Checkboxes register with CheckList based on matching group names.
 */
class CheckList : public UIControl, public IRenderableComponent {
public:
    CheckList();
    explicit CheckList(const std::string& name);
    virtual ~CheckList();

    // ISerializable interface
    std::string GetTypeName() const override { return "CheckList"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnUpdate(float deltaTime) override;

    // Property change notification
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ===== Group Properties =====

    std::string GetGroupName() const;
    void SetGroupName(const std::string& groupName);

    // ===== Layout Properties =====

    CheckListOrientation GetOrientation() const;
    void SetOrientation(CheckListOrientation orientation);

    float GetSpacing() const;
    void SetSpacing(float spacing);

    // ===== Rendering Properties =====

    int GetLayer() const;
    void SetLayer(int layer);

    int GetSortingOrder() const;
    void SetSortingOrder(int order);

    // GetUseUISpace/SetUseUISpace are provided by the UIControl base class.

    // ===== Checkbox Registration =====

    void RegisterCheckbox(Checkbox* checkbox);
    void UnregisterCheckbox(Checkbox* checkbox);
    void OnCheckboxStateChanged(Checkbox* checkbox);

    // ===== Group Operations =====

    void CheckAll();
    void UncheckAll();
    void ToggleAll();

    // ===== State Queries =====

    bool AreAllChecked() const;
    bool AreAllUnchecked() const;
    bool IsAnyChecked() const;
    int GetCheckedCount() const;
    int GetTotalCount() const;

    std::vector<Checkbox*> GetCheckedCheckboxes() const;
    std::vector<Checkbox*> GetUncheckedCheckboxes() const;
    std::vector<int> GetCheckedValues() const;

    // ===== Item Management =====

    void AddItem(const std::string& text, int value = -1);
    void RemoveItem(int index);
    void ClearItems();
    int GetItemCount() const;

    std::vector<std::string> GetItems() const;
    void SetItems(const std::vector<std::string>& items);

    bool GetAutoCreateCheckboxes() const;
    void SetAutoCreateCheckboxes(bool autoCreate);

    // ===== Callbacks =====

    using GroupStateCallback = std::function<void(int checkedCount, int totalCount)>;
    void SetOnGroupStateChangedCallback(GroupStateCallback callback);

    // ===== IRenderableComponent Implementation =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

    math::OBB getOrientedBounds() const override;
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;

private:
    // Update layout of registered checkboxes
    void UpdateLayout();

    // Create checkboxes from items list
    void CreateCheckboxesFromItems();

    // Destroy auto-created checkboxes
    void DestroyAutoCreatedCheckboxes();

    // Notify callback of state change
    void NotifyGroupStateChanged();

    // Group name for matching checkboxes
    std::string m_GroupName;

    // Registered checkboxes
    std::vector<Checkbox*> m_Checkboxes;

    // Auto-created checkbox nodes (for cleanup)
    std::vector<std::shared_ptr<core::Node>> m_AutoCreatedNodes;

    // Items list for auto-creation
    std::vector<std::string> m_Items;

    // Per-item checked state, authoritative when no checkbox child nodes are
    // registered (data-only mode); kept in sync with the item list.
    std::vector<bool> m_ItemChecked;

    // Callback
    GroupStateCallback m_OnGroupStateChangedCallback;

    // Flags
    bool m_LayoutDirty{true};
    bool m_AutoCreateCheckboxes{false};
};

} // namespace components
} // namespace lupine

#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace lupine {
namespace components {

// Forward declaration
class RadioButton;

/**
 * RadioList Component
 *
 * A container component that manages a group of radio buttons.
 * Ensures mutual exclusion - only one radio button can be selected at a time.
 *
 * Features:
 * - Automatic RadioButton child management
 * - Group name for identifying radio button groups
 * - Selection tracking and callbacks
 * - Vertical or horizontal layout orientation
 * - Configurable spacing between radio buttons
 * - Dynamic item list management
 *
 * Usage:
 * 1. Add RadioList component to a node
 * 2. Set group name to identify the radio group
 * 3. Add RadioButton components as children with matching group name
 * 4. RadioList will automatically manage mutual exclusion
 */
class RadioList : public core::Component {
public:
    RadioList();
    explicit RadioList(const std::string& name);
    virtual ~RadioList();

    // ISerializable interface
    std::string GetTypeName() const override { return "RadioList"; }
    void DefineProperties() override;
    void DefineSignals() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnUpdate(float deltaTime) override;

    // ===== Group Management =====
    
    std::string GetGroupName() const;
    void SetGroupName(const std::string& groupName);

    // ===== Selection Management =====
    
    int GetSelectedIndex() const;
    void SetSelectedIndex(int index);

    int GetSelectedValue() const;
    void SetSelectedValue(int value);

    // ===== Layout Properties =====
    
    enum class Orientation {
        Vertical = 0,
        Horizontal = 1
    };

    Orientation GetOrientation() const;
    void SetOrientation(Orientation orientation);

    float GetSpacing() const;
    void SetSpacing(float spacing);

    // ===== Item Management =====
    
    std::vector<std::string> GetItems() const;
    void SetItems(const std::vector<std::string>& items);

    void AddItem(const std::string& item);
    void RemoveItem(int index);
    void ClearItems();

    int GetItemCount() const;

    // ===== Auto-Create RadioButtons =====

    bool GetAutoCreateButtons() const;
    void SetAutoCreateButtons(bool autoCreate);

    // ===== Layout Management =====

    bool GetManageLayout() const;
    void SetManageLayout(bool manage);

    void RegenerateRadioButtons();

    // ===== Selection Callback =====
    
    using SelectionChangedCallback = std::function<void(int, int)>; // (index, value)
    void SetOnSelectionChangedCallback(SelectionChangedCallback callback);

    // ===== RadioButton Registration (called by RadioButton components) =====
    
    void RegisterRadioButton(RadioButton* radioButton);
    void UnregisterRadioButton(RadioButton* radioButton);

    // Called when a radio button is selected
    void OnRadioButtonSelected(RadioButton* selectedButton);

private:
    // Sync internal state from properties
    void SyncFromProperties();

    // Update layout of radio buttons
    void UpdateLayout();

    // Deselect all radio buttons except the specified one
    void DeselectAllExcept(RadioButton* exceptButton);

    // Find radio button by index
    RadioButton* FindRadioButtonByIndex(int index) const;

    // Find radio button by value
    RadioButton* FindRadioButtonByValue(int value) const;

    // Group name
    std::string m_GroupName;

    // Selection state
    int m_SelectedIndex{-1};
    int m_SelectedValue{-1};

    // Layout
    Orientation m_Orientation{Orientation::Vertical};
    float m_Spacing{10.0f};

    // Items
    std::vector<std::string> m_Items;
    bool m_AutoCreateButtons{true};
    bool m_ManageLayout{true};
    bool m_ItemsChanged{false};
    bool m_LayoutDirty{false};

    // Registered radio buttons
    std::vector<RadioButton*> m_RegisteredButtons;

    // Selection callback
    SelectionChangedCallback m_OnSelectionChangedCallback;
};

} // namespace components
} // namespace lupine


#include "lupine/components/RadioList.hpp"
#include "lupine/components/RadioButton.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace components {

using namespace core;

RadioList::RadioList()
    : Component("RadioList")
    , m_SelectedIndex(-1)
    , m_SelectedValue(-1)
    , m_Orientation(Orientation::Vertical)
    , m_Spacing(10.0f)
    , m_AutoCreateButtons(true)
    , m_ItemsChanged(false)
    , m_LayoutDirty(false)
{
}

RadioList::RadioList(const std::string& name)
    : Component(name)
    , m_SelectedIndex(-1)
    , m_SelectedValue(-1)
    , m_Orientation(Orientation::Vertical)
    , m_Spacing(10.0f)
    , m_AutoCreateButtons(true)
    , m_ItemsChanged(false)
    , m_LayoutDirty(false)
{
}

RadioList::~RadioList() {
    m_RegisteredButtons.clear();
}

void RadioList::DefineProperties() {
    // Group
    DefineProperty(PROPERTY_DEFAULT_GROUP(groupName, String, std::string("default"), "Group"));

    // Selection
    DefineProperty(PROPERTY_INT_RANGE_GROUP(selectedIndex, -1, -1, 1000, 1, "Selection"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(selectedValue, -1, -1, 1000, 1, "Selection"));

    // Layout
    DefineProperty(PROPERTY_ENUM_GROUP(orientation, 0, "Layout", Vertical, Horizontal));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(spacing, 10.0f, 0.0f, 1000.0f, 1.0f, "Layout"));

    // Items
    DefineProperty(PROPERTY_DEFAULT_GROUP(items, StringArray, std::vector<std::string>(), "Items"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(autoCreateButtons, Bool, true, "Items"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(manageLayout, Bool, true, "Layout"));
}

void RadioList::OnAwake() {
    m_GroupName = GetPropertyValue<std::string>("groupName");
    m_SelectedIndex = GetPropertyValue<int>("selectedIndex");
    m_SelectedValue = GetPropertyValue<int>("selectedValue");
    m_Orientation = static_cast<Orientation>(GetPropertyValue<int>("orientation"));
    m_Spacing = GetPropertyValue<float>("spacing");
    m_Items = GetPropertyValue<std::vector<std::string>>("items");
    m_AutoCreateButtons = GetPropertyValue<bool>("autoCreateButtons");
    m_ManageLayout = GetPropertyValue<bool>("manageLayout");
}

void RadioList::OnReady() {
    // If auto-create is enabled and we have items, create radio buttons
    if (m_AutoCreateButtons && !m_Items.empty()) {
        RegenerateRadioButtons();
    }

    // Update layout
    m_LayoutDirty = true;
}

void RadioList::OnUpdate(float) {
    SyncFromProperties();

    // If items changed and auto-create is enabled, regenerate buttons
    if (m_ItemsChanged && m_AutoCreateButtons) {
        RegenerateRadioButtons();
        m_ItemsChanged = false;
    }

    // Update layout only if dirty and layout management is enabled
    if (m_LayoutDirty && m_ManageLayout) {
        UpdateLayout();
        m_LayoutDirty = false;
    } else if (m_LayoutDirty) {
        // Clear the dirty flag even if we're not managing layout
        m_LayoutDirty = false;
    }
}

// ===== Property Accessors =====

std::string RadioList::GetGroupName() const {
    return GetPropertyValue<std::string>("groupName");
}

void RadioList::SetGroupName(const std::string& groupName) {
    m_GroupName = groupName;
    SetPropertyValue<std::string>("groupName", groupName);

    // Update all registered radio buttons
    for (RadioButton* btn : m_RegisteredButtons) {
        if (btn) {
            btn->SetGroupName(groupName);
        }
    }
}

int RadioList::GetSelectedIndex() const {
    return GetPropertyValue<int>("selectedIndex");
}

void RadioList::SetSelectedIndex(int index) {
    int itemCount = !m_RegisteredButtons.empty()
        ? static_cast<int>(m_RegisteredButtons.size())
        : static_cast<int>(m_Items.size());
    if (index < -1 || index >= itemCount) {
        return;
    }

    m_SelectedIndex = index;
    SetPropertyValue<int>("selectedIndex", index);

    if (index >= 0 && index < static_cast<int>(m_RegisteredButtons.size())) {
        RadioButton* btn = m_RegisteredButtons[index];
        if (btn) {
            btn->SetSelected(true);
            m_SelectedValue = btn->GetRadioValue();
            SetPropertyValue<int>("selectedValue", m_SelectedValue);
        }
    } else if (index < 0) {
        // Deselect all
        DeselectAllExcept(nullptr);
        m_SelectedValue = -1;
        SetPropertyValue<int>("selectedValue", -1);
    }
}

int RadioList::GetSelectedValue() const {
    return GetPropertyValue<int>("selectedValue");
}

void RadioList::SetSelectedValue(int value) {
    m_SelectedValue = value;
    SetPropertyValue<int>("selectedValue", value);

    RadioButton* btn = FindRadioButtonByValue(value);
    if (btn) {
        btn->SetSelected(true);
    }
}

RadioList::Orientation RadioList::GetOrientation() const {
    return static_cast<Orientation>(GetPropertyValue<int>("orientation"));
}

void RadioList::SetOrientation(Orientation orientation) {
    m_Orientation = orientation;
    SetPropertyValue<int>("orientation", static_cast<int>(orientation));
    m_LayoutDirty = true;
}

float RadioList::GetSpacing() const {
    return GetPropertyValue<float>("spacing");
}

void RadioList::SetSpacing(float spacing) {
    m_Spacing = spacing;
    SetPropertyValue<float>("spacing", spacing);
    m_LayoutDirty = true;
}

std::vector<std::string> RadioList::GetItems() const {
    return GetPropertyValue<std::vector<std::string>>("items");
}

void RadioList::SetItems(const std::vector<std::string>& items) {
    m_Items = items;
    SetPropertyValue<std::vector<std::string>>("items", items);
    m_ItemsChanged = true;
}

void RadioList::AddItem(const std::string& item) {
    m_Items.push_back(item);
    SetPropertyValue<std::vector<std::string>>("items", m_Items);
    m_ItemsChanged = true;
}

void RadioList::RemoveItem(int index) {
    if (index >= 0 && index < static_cast<int>(m_Items.size())) {
        m_Items.erase(m_Items.begin() + index);
        SetPropertyValue<std::vector<std::string>>("items", m_Items);
        m_ItemsChanged = true;
    }
}

void RadioList::ClearItems() {
    m_Items.clear();
    SetPropertyValue<std::vector<std::string>>("items", m_Items);
    m_ItemsChanged = true;
}

int RadioList::GetItemCount() const {
    return static_cast<int>(m_Items.size());
}

bool RadioList::GetAutoCreateButtons() const {
    return GetPropertyValue<bool>("autoCreateButtons");
}

void RadioList::SetAutoCreateButtons(bool autoCreate) {
    m_AutoCreateButtons = autoCreate;
    SetPropertyValue<bool>("autoCreateButtons", autoCreate);

    if (autoCreate && !m_Items.empty()) {
        RegenerateRadioButtons();
    }
}

bool RadioList::GetManageLayout() const {
    return GetPropertyValue<bool>("manageLayout");
}

void RadioList::SetManageLayout(bool manage) {
    m_ManageLayout = manage;
    SetPropertyValue<bool>("manageLayout", manage);
    if (manage) {
        m_LayoutDirty = true;
    }
}

void RadioList::RegenerateRadioButtons() {
    if (!GetOwner()) {
        return;
    }

    // Remove all existing auto-created radio button child nodes
    // (We identify them by checking if they have RadioButton component with matching group name)
    auto children = GetOwner()->GetChildren();
    for (auto& child : children) {
        auto radioBtn = child->GetComponent<RadioButton>();
        if (radioBtn && radioBtn->GetGroupName() == m_GroupName) {
            GetOwner()->RemoveChild(child);
        }
    }

    // Clear registered buttons
    m_RegisteredButtons.clear();

    // Create new radio button nodes for each item
    for (size_t i = 0; i < m_Items.size(); ++i) {
        std::string itemName = "RadioButton_" + std::to_string(i);
        auto radioNode = std::make_shared<Node2D>(itemName);

        auto radioBtn = std::make_shared<RadioButton>();
        radioBtn->SetText(m_Items[i]);
        radioBtn->SetGroupName(m_GroupName);
        radioBtn->SetRadioValue(static_cast<int>(i));

        radioNode->AddComponent(radioBtn);
        GetOwner()->AddChild(radioNode);

        RegisterRadioButton(radioBtn.get());
    }

    // Mark layout as dirty
    m_LayoutDirty = true;

    // Restore selection if valid
    if (m_SelectedIndex >= 0 && m_SelectedIndex < static_cast<int>(m_RegisteredButtons.size())) {
        m_RegisteredButtons[m_SelectedIndex]->SetSelected(true);
    }
}

void RadioList::SetOnSelectionChangedCallback(SelectionChangedCallback callback) {
    m_OnSelectionChangedCallback = callback;
}

// ===== RadioButton Registration =====

void RadioList::RegisterRadioButton(RadioButton* radioButton) {
    if (!radioButton) {
        return;
    }

    // Check if already registered
    for (RadioButton* btn : m_RegisteredButtons) {
        if (btn == radioButton) {
            return;
        }
    }

    m_RegisteredButtons.push_back(radioButton);
    radioButton->SetRadioList(this);

    // Mark layout as dirty when a new button is registered
    m_LayoutDirty = true;
}

void RadioList::UnregisterRadioButton(RadioButton* radioButton) {
    if (!radioButton) {
        return;
    }

    for (auto it = m_RegisteredButtons.begin(); it != m_RegisteredButtons.end(); ++it) {
        if (*it == radioButton) {
            m_RegisteredButtons.erase(it);
            // Mark layout as dirty when a button is unregistered
            m_LayoutDirty = true;
            break;
        }
    }
}

void RadioList::OnRadioButtonSelected(RadioButton* selectedButton) {
    if (!selectedButton) {
        return;
    }

    // Deselect all other buttons
    DeselectAllExcept(selectedButton);

    // Update selection state
    int newIndex = -1;
    for (size_t i = 0; i < m_RegisteredButtons.size(); ++i) {
        if (m_RegisteredButtons[i] == selectedButton) {
            newIndex = static_cast<int>(i);
            break;
        }
    }

    int newValue = selectedButton->GetRadioValue();

    bool selectionChanged = (newIndex != m_SelectedIndex) || (newValue != m_SelectedValue);

    m_SelectedIndex = newIndex;
    m_SelectedValue = newValue;

    SetPropertyValue<int>("selectedIndex", m_SelectedIndex);
    SetPropertyValue<int>("selectedValue", m_SelectedValue);

    // Invoke callback
    if (selectionChanged && m_OnSelectionChangedCallback) {
        m_OnSelectionChangedCallback(m_SelectedIndex, m_SelectedValue);
    }
    if (selectionChanged) {
        Emit("selection_changed", { m_SelectedIndex, m_SelectedValue });
    }
}

void RadioList::DefineSignals() {
    RegisterSignal({"selection_changed",
                    {{"index", core::PropertyValueType::Int}, {"value", core::PropertyValueType::Int}},
                    "Emitted when the selected radio item changes."});
}

// ===== Private Methods =====

void RadioList::SyncFromProperties() {
    m_GroupName = GetPropertyValue<std::string>("groupName");
    m_SelectedIndex = GetPropertyValue<int>("selectedIndex");
    m_SelectedValue = GetPropertyValue<int>("selectedValue");

    Orientation newOrientation = static_cast<Orientation>(GetPropertyValue<int>("orientation"));
    if (newOrientation != m_Orientation) {
        m_Orientation = newOrientation;
        m_LayoutDirty = true;
    }

    float newSpacing = GetPropertyValue<float>("spacing");
    if (newSpacing != m_Spacing) {
        m_Spacing = newSpacing;
        m_LayoutDirty = true;
    }

    std::vector<std::string> newItems = GetPropertyValue<std::vector<std::string>>("items");
    if (newItems != m_Items) {
        m_Items = newItems;
        m_ItemsChanged = true;
    }

    m_AutoCreateButtons = GetPropertyValue<bool>("autoCreateButtons");
    m_ManageLayout = GetPropertyValue<bool>("manageLayout");
}

void RadioList::UpdateLayout() {
    if (!GetOwner()) {
        return;
    }

    if (m_RegisteredButtons.empty()) {
        return;
    }

    // Calculate total size first
    float totalExtent = 0.0f;
    std::vector<math::Vec2> buttonSizes;

    // Minimum button size to prevent layout collapse when properties aren't ready
    const float MIN_BUTTON_SIZE = 20.0f;

    for (size_t i = 0; i < m_RegisteredButtons.size(); ++i) {
        RadioButton* btn = m_RegisteredButtons[i];
        if (!btn || !btn->GetOwner()) {
            buttonSizes.push_back(math::Vec2(MIN_BUTTON_SIZE, MIN_BUTTON_SIZE));
            if (m_Orientation == Orientation::Vertical) {
                totalExtent += MIN_BUTTON_SIZE;
            } else {
                totalExtent += MIN_BUTTON_SIZE;
            }
            if (i < m_RegisteredButtons.size() - 1) {
                totalExtent += m_Spacing;
            }
            continue;
        }

        math::Vec2 totalSize = btn->CalculateTotalSize();

        // Ensure minimum size
        if (totalSize.x < MIN_BUTTON_SIZE) totalSize.x = MIN_BUTTON_SIZE;
        if (totalSize.y < MIN_BUTTON_SIZE) totalSize.y = MIN_BUTTON_SIZE;

        buttonSizes.push_back(totalSize);

        if (m_Orientation == Orientation::Vertical) {
            totalExtent += totalSize.y;
        } else {
            totalExtent += totalSize.x;
        }

        if (i < m_RegisteredButtons.size() - 1) {
            totalExtent += m_Spacing;
        }
    }

    // Start from 0 offset (top-left alignment) instead of centering
    // This makes positioning more predictable - the RadioList node position is the starting point
    float currentOffset = 0.0f;

    for (size_t i = 0; i < m_RegisteredButtons.size(); ++i) {
        RadioButton* btn = m_RegisteredButtons[i];
        if (!btn || !btn->GetOwner()) {
            // Still advance the offset for missing buttons
            if (m_Orientation == Orientation::Vertical) {
                currentOffset += buttonSizes[i].y + m_Spacing;
            } else {
                currentOffset += buttonSizes[i].x + m_Spacing;
            }
            continue;
        }

        Node2D* btnNode = dynamic_cast<Node2D*>(btn->GetOwner());
        if (!btnNode) {
            continue;
        }

        math::Vec2 totalSize = buttonSizes[i];

        if (m_Orientation == Orientation::Vertical) {
            // Vertical layout: stack buttons vertically downward from the RadioList position
            btnNode->SetPosition(math::Vec2(0.0f, currentOffset));
            currentOffset += totalSize.y + m_Spacing;
        } else {
            // Horizontal layout: stack buttons horizontally to the right from the RadioList position
            btnNode->SetPosition(math::Vec2(currentOffset, 0.0f));
            currentOffset += totalSize.x + m_Spacing;
        }
    }
}

void RadioList::DeselectAllExcept(RadioButton* exceptButton) {
    for (RadioButton* btn : m_RegisteredButtons) {
        if (btn && btn != exceptButton) {
            btn->SetSelected(false);
        }
    }
}

RadioButton* RadioList::FindRadioButtonByIndex(int index) const {
    if (index >= 0 && index < static_cast<int>(m_RegisteredButtons.size())) {
        return m_RegisteredButtons[index];
    }
    return nullptr;
}

RadioButton* RadioList::FindRadioButtonByValue(int value) const {
    for (RadioButton* btn : m_RegisteredButtons) {
        if (btn && btn->GetRadioValue() == value) {
            return btn;
        }
    }
    return nullptr;
}

} // namespace components
} // namespace lupine


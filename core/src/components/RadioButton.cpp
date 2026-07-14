#include "lupine/components/RadioButton.hpp"
#include "lupine/components/RadioList.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/DrawCommand.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/input/InputManager.hpp"
#include "lupine/rendering/ViewportUtils.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/math/OBB.hpp"
#include "lupine/math/Quat.hpp"

namespace lupine {
namespace components {

using namespace core;
using namespace math;

RadioButton::RadioButton()
    : UIControl("RadioButton")
    , m_ButtonEnabled(true)
    , m_IsSelected(false)
    , m_CurrentState(RadioButtonState::Normal)
    , m_PreviousState(RadioButtonState::Normal)
    , m_RadioValue(0)
    , m_RadioList(nullptr)
    , m_TextMeshNeedsRegeneration(true)
    , m_MouseWasOver(false)
{
    // Initialize default state modulation colors
    m_StateStyles[static_cast<int>(RadioButtonState::Normal)].modulationColor = Color::White();
    m_StateStyles[static_cast<int>(RadioButtonState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateStyles[static_cast<int>(RadioButtonState::Pressed)].modulationColor = Color(0.9f, 0.9f, 0.9f, 1.0f);
    m_StateStyles[static_cast<int>(RadioButtonState::Selected)].modulationColor = Color(0.3f, 0.7f, 1.0f, 1.0f);
    m_StateStyles[static_cast<int>(RadioButtonState::SelectedHover)].modulationColor = Color(0.4f, 0.8f, 1.0f, 1.0f);
    m_StateStyles[static_cast<int>(RadioButtonState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

RadioButton::RadioButton(const std::string& name)
    : UIControl(name)
    , m_ButtonEnabled(true)
    , m_IsSelected(false)
    , m_CurrentState(RadioButtonState::Normal)
    , m_PreviousState(RadioButtonState::Normal)
    , m_RadioValue(0)
    , m_RadioList(nullptr)
    , m_TextMeshNeedsRegeneration(true)
    , m_MouseWasOver(false)
{
    // Initialize default state modulation colors
    m_StateStyles[static_cast<int>(RadioButtonState::Normal)].modulationColor = Color::White();
    m_StateStyles[static_cast<int>(RadioButtonState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateStyles[static_cast<int>(RadioButtonState::Pressed)].modulationColor = Color(0.9f, 0.9f, 0.9f, 1.0f);
    m_StateStyles[static_cast<int>(RadioButtonState::Selected)].modulationColor = Color(0.3f, 0.7f, 1.0f, 1.0f);
    m_StateStyles[static_cast<int>(RadioButtonState::SelectedHover)].modulationColor = Color(0.4f, 0.8f, 1.0f, 1.0f);
    m_StateStyles[static_cast<int>(RadioButtonState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

RadioButton::~RadioButton() {
}

void RadioButton::DefineProperties() {
    // Shared UIControl layout properties (anchors/size-flags/useUISpace + width/height).
    DefineUIControlProperties(20.0f, 20.0f, "useUISpace", "Layout");

    // Indicator
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(indicatorSize, 20.0f, 5.0f, 100.0f, 1.0f, "Indicator"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(outerCircleColor, Color, Color(0.7f, 0.7f, 0.7f, 1.0f), "Indicator"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.2f, 0.2f, 0.2f, 1.0f), "Indicator"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(innerCircleColor, Color, Color(0.3f, 0.7f, 1.0f, 1.0f), "Indicator"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(innerCircleScale, 0.5f, 0.1f, 0.9f, 0.05f, "Indicator"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(borderWidth, 2.0f, 0.0f, 10.0f, 0.5f, "Indicator"));

    // Rendering
    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));
    // Button
    DefineProperty(PROPERTY_DEFAULT_GROUP(buttonEnabled, Bool, true, "Button"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(isSelected, Bool, false, "Button"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(groupName, String, std::string(""), "Button"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(radioValue, 0, 0, 1000, 1, "Button"));

    // Text
    DefineProperty(PROPERTY_DEFAULT_GROUP(text, String, std::string("Radio Button"), "Text"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(textOffset, 10.0f, 0.0f, 100.0f, 1.0f, "Text"));

    // State modulation colors
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalModulation, Color, Color::White(), "StateModulation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverModulation, Color, Color(1.2f, 1.2f, 1.2f, 1.0f), "StateModulation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedModulation, Color, Color(0.9f, 0.9f, 0.9f, 1.0f), "StateModulation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(selectedModulation, Color, Color(0.3f, 0.7f, 1.0f, 1.0f), "StateModulation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(selectedHoverModulation, Color, Color(0.4f, 0.8f, 1.0f, 1.0f), "StateModulation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledModulation, Color, Color(0.5f, 0.5f, 0.5f, 0.5f), "StateModulation"));

    // Audio
    DefineProperty(PROPERTY_FILE_GROUP(selectSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "Audio"));
}

void RadioButton::OnAwake() {
    m_ButtonEnabled = GetPropertyValue<bool>("buttonEnabled");
    m_IsSelected = GetPropertyValue<bool>("isSelected");
    m_GroupName = GetPropertyValue<std::string>("groupName");
    m_RadioValue = GetPropertyValue<int>("radioValue");

    // Load state modulation colors, honoring theme bindings and per-instance overrides
    LoadThemedStateData();

    // Load audio
    std::string selectSoundPath = GetPropertyValue<std::string>("selectSoundPath");
    if (!selectSoundPath.empty()) {
        SetSelectSoundPath(selectSoundPath);
    }

    // Load font
    std::string fontPath = GetFontPath();
    if (!fontPath.empty()) {
        m_CurrentFontPath = fontPath;
    }

    // Set initial state
    if (!m_ButtonEnabled) {
        m_CurrentState = RadioButtonState::Disabled;
    } else if (m_IsSelected) {
        m_CurrentState = RadioButtonState::Selected;
    } else {
        m_CurrentState = RadioButtonState::Normal;
    }
}

void RadioButton::OnReady() {
    m_TextMeshNeedsRegeneration = true;

    // Find and register with RadioList if group name is set
    if (!m_GroupName.empty() && GetOwner()) {
        NotifyRadioList();

        // If this radio button is selected, notify the RadioList to enforce mutual exclusion
        if (m_IsSelected && m_RadioList) {
            m_RadioList->OnRadioButtonSelected(this);
        }
    }
}

void RadioButton::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    UIControl::OnPropertyChanged(propertyName, newValue);

    // Sync member variables when properties are changed in the editor

    if (propertyName == "groupName") {
        m_GroupName = newValue.get<std::string>();
        
        // Re-register with RadioList if group name changed
        if (GetOwner()) {
            NotifyRadioList();
        }
    } else if (propertyName == "radioValue") {
        m_RadioValue = newValue.get<int>();
    } else if (propertyName == "buttonEnabled") {
        bool newEnabled = newValue.get<bool>();
        if (m_ButtonEnabled != newEnabled) {
            m_ButtonEnabled = newEnabled;
            if (!m_ButtonEnabled) {
                TransitionToState(RadioButtonState::Disabled);
            } else {
                TransitionToState(m_IsSelected ? RadioButtonState::Selected : RadioButtonState::Normal);
            }
        }
    } else if (propertyName == "text") {
        m_TextMeshNeedsRegeneration = true;
    } else if (propertyName == "fontSize" || propertyName == "fontPath") {
        m_TextMeshNeedsRegeneration = true;
    }
}

void RadioButton::OnInput(float deltaTime) {
    if (!IsEnabled()) {
        return;
    }

    UpdateMouseInteraction(deltaTime);
}

void RadioButton::OnUpdate(float deltaTime) {
    if (!IsEnabled()) {
        return;
    }

    UIControl::OnUpdate(deltaTime);
    SyncFromProperties();
}

bool RadioButton::OnGizmoScale(float scaleDelta, int, bool is3D) {
    if (!is3D) {
        // Scale both indicator size and font size
        float currentIndicatorSize = GetIndicatorSize();
        float currentFontSize = GetFontSize();

        float newIndicatorSize = std::max(5.0f, currentIndicatorSize + scaleDelta * currentIndicatorSize);
        float newFontSize = std::max(1.0f, std::min(256.0f, currentFontSize + scaleDelta * currentFontSize));

        SetIndicatorSize(newIndicatorSize);
        SetFontSize(newFontSize);

        m_TextMeshNeedsRegeneration = true;

        return true;
    }
    return false;
}

// ===== Property Accessors =====

float RadioButton::GetIndicatorSize() const {
    return GetPropertyValue<float>("indicatorSize");
}

void RadioButton::SetIndicatorSize(float size) {
    SetPropertyValue<float>("indicatorSize", size);
}

int RadioButton::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void RadioButton::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int RadioButton::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void RadioButton::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

// GetUseUISpace/SetUseUISpace are provided by the UIControl base class.

void RadioButton::SetEnabled(bool enabled) {
    m_ButtonEnabled = enabled;
    SetPropertyValue<bool>("buttonEnabled", enabled);

    if (!enabled && m_CurrentState != RadioButtonState::Disabled) {
        TransitionToState(RadioButtonState::Disabled);
    } else if (enabled && m_CurrentState == RadioButtonState::Disabled) {
        TransitionToState(m_IsSelected ? RadioButtonState::Selected : RadioButtonState::Normal);
    }
}

void RadioButton::SetSelected(bool selected) {
    if (m_IsSelected == selected) {
        return;
    }

    m_IsSelected = selected;
    SetPropertyValue<bool>("isSelected", selected);

    if (!m_ButtonEnabled) {
        return;
    }

    if (selected) {
        TransitionToState(RadioButtonState::Selected);
        PlaySelectionSound();

        // Notify radio list to deselect others
        if (m_RadioList) {
            m_RadioList->OnRadioButtonSelected(this);
        } else {
            // If no RadioList, manually deselect other radio buttons with the same group name
            DeselectOtherRadioButtons();
        }

        // Invoke callback
        if (m_OnSelectedCallback) {
            m_OnSelectedCallback(m_RadioValue);
        }
        Emit("selected", { m_RadioValue });
    } else {
        TransitionToState(RadioButtonState::Normal);
    }
}

std::string RadioButton::GetGroupName() const {
    return GetPropertyValue<std::string>("groupName");
}

void RadioButton::SetGroupName(const std::string& groupName) {
    m_GroupName = groupName;
    SetPropertyValue<std::string>("groupName", groupName);

    // Re-register with RadioList
    if (GetOwner()) {
        NotifyRadioList();
    }
}

int RadioButton::GetRadioValue() const {
    return GetPropertyValue<int>("radioValue");
}

void RadioButton::SetRadioValue(int value) {
    m_RadioValue = value;
    SetPropertyValue<int>("radioValue", value);
}

const std::vector<UIControl::ThemeBinding>& RadioButton::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = []() {
        std::vector<ThemeBinding> b = {
            { "outerCircleColor", "border_color",       ThemeBinding::Kind::Color },
            { "backgroundColor",  "background",          ThemeBinding::Kind::Color },
            { "innerCircleColor", "inner_circle_color",  ThemeBinding::Kind::Color },
            { "fontColor",        "font_color",          ThemeBinding::Kind::Color },
            { "fontPath",         "font",                ThemeBinding::Kind::Font },
            { "fontSize",         "font_size",           ThemeBinding::Kind::Constant },
            { "borderWidth",      "border_width",        ThemeBinding::Kind::Constant }
        };
        const char* states[] = { "normal", "hover", "pressed", "selected", "selectedHover", "disabled" };
        for (const char* s : states) {
            std::string p(s);
            b.push_back({ p + "Modulation", p + "Modulation", ThemeBinding::Kind::Color });
        }
        return b;
    }();
    return kBindings;
}

Color RadioButton::GetOuterCircleColor() const {
    return ResolveThemedColor("outerCircleColor", "border_color");
}

void RadioButton::SetOuterCircleColor(const Color& color) {
    SetThemedProperty<Color>("outerCircleColor", color);
}

Color RadioButton::GetBackgroundColor() const {
    return ResolveThemedColor("backgroundColor", "background");
}

void RadioButton::SetBackgroundColor(const Color& color) {
    SetThemedProperty<Color>("backgroundColor", color);
}

Color RadioButton::GetInnerCircleColor() const {
    return ResolveThemedColor("innerCircleColor", "inner_circle_color");
}

void RadioButton::SetInnerCircleColor(const Color& color) {
    SetThemedProperty<Color>("innerCircleColor", color);
}

float RadioButton::GetInnerCircleScale() const {
    return GetPropertyValue<float>("innerCircleScale");
}

void RadioButton::SetInnerCircleScale(float scale) {
    SetPropertyValue<float>("innerCircleScale", scale);
}

float RadioButton::GetBorderWidth() const {
    return ResolveThemedConstant("borderWidth", "border_width");
}

void RadioButton::SetBorderWidth(float width) {
    SetThemedProperty<float>("borderWidth", width);
}

std::string RadioButton::GetText() const {
    return GetPropertyValue<std::string>("text");
}

void RadioButton::SetText(const std::string& text) {
    SetPropertyValue<std::string>("text", text);
    m_TextMeshNeedsRegeneration = true;
}

std::string RadioButton::GetFontPath() const {
    return ResolveThemedFontPath("fontPath", "font");
}

void RadioButton::SetFontPath(const std::string& path) {
    SetThemedProperty<std::string>("fontPath", path);
    m_CurrentFontPath = path;
    m_TextMeshNeedsRegeneration = true;
}

float RadioButton::GetFontSize() const {
    return ResolveThemedFontSize("fontSize", "font_size", "font");
}

void RadioButton::SetFontSize(float size) {
    SetThemedProperty<float>("fontSize", size);
    m_CurrentFontSize = size;
    m_TextMeshNeedsRegeneration = true;
}

Color RadioButton::GetFontColor() const {
    return ResolveThemedColor("fontColor", "font_color");
}

void RadioButton::SetFontColor(const Color& color) {
    SetThemedProperty<Color>("fontColor", color);
    m_TextMeshNeedsRegeneration = true;
}

float RadioButton::GetTextOffset() const {
    return GetPropertyValue<float>("textOffset");
}

void RadioButton::SetTextOffset(float offset) {
    SetPropertyValue<float>("textOffset", offset);
}

const Color& RadioButton::GetStateModulation(RadioButtonState state) const {
    return m_StateStyles[static_cast<int>(state)].modulationColor;
}

void RadioButton::SetStateModulation(RadioButtonState state, const Color& color) {
    m_StateStyles[static_cast<int>(state)].modulationColor = color;

    std::string propName;
    switch (state) {
        case RadioButtonState::Normal: propName = "normalModulation"; break;
        case RadioButtonState::Hover: propName = "hoverModulation"; break;
        case RadioButtonState::Pressed: propName = "pressedModulation"; break;
        case RadioButtonState::Selected: propName = "selectedModulation"; break;
        case RadioButtonState::SelectedHover: propName = "selectedHoverModulation"; break;
        case RadioButtonState::Disabled: propName = "disabledModulation"; break;
        default: return;
    }
    SetPropertyValue<Color>(propName, color);
}

std::string RadioButton::GetSelectSoundPath() const {
    return GetPropertyValue<std::string>("selectSoundPath");
}

void RadioButton::SetSelectSoundPath(const std::string& path) {
    m_SelectSoundPath = path;
    SetPropertyValue<std::string>("selectSoundPath", path);

    if (!path.empty()) {
        m_SelectSoundAsset = asset::AssetRef<asset::AudioAsset>(new asset::AudioAsset());
        bool loaded = m_SelectSoundAsset->LoadFromFile(path);
        if (!loaded) {
            m_SelectSoundAsset.Reset();
        }
    } else {
        m_SelectSoundAsset.Reset();
    }
}

void RadioButton::SetOnSelectedCallback(SelectionCallback callback) {
    m_OnSelectedCallback = callback;
}

void RadioButton::DefineSignals() {
    RegisterSignal({"selected",
                    {{"value", core::PropertyValueType::Int}},
                    "Emitted when this radio button becomes selected."});
}

void RadioButton::NotifyRadioList() {
    if (!GetOwner()) {
        return;
    }

    // Clear existing RadioList reference first
    m_RadioList = nullptr;

    // Search for RadioList component in parent hierarchy
    Node* current = GetOwner();
    while (current) {
        auto radioList = current->GetComponent<RadioList>();
        if (radioList && radioList->GetGroupName() == m_GroupName) {
            SetRadioList(radioList.get());
            radioList->RegisterRadioButton(this);
            return;
        }
        current = current->GetParent();
    }
}

void RadioButton::SetRadioList(RadioList* radioList) {
    m_RadioList = radioList;
}

// ===== Private Methods =====

Color RadioButton::GetSafeColorProperty(const std::string& propName, const Color& defaultColor) const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty(propName);
    if (prop && !prop->GetValueAsJson().is_null()) {
        return prop->GetValue<Color>();
    }
    return defaultColor;
}

void RadioButton::SyncFromProperties() {
    bool wasEnabled = m_ButtonEnabled;

    m_ButtonEnabled = GetPropertyValue<bool>("buttonEnabled");
    m_GroupName = GetPropertyValue<std::string>("groupName");
    m_RadioValue = GetPropertyValue<int>("radioValue");

    // NOTE: We intentionally do NOT read isSelected from properties here.
    // m_IsSelected is the source of truth and is only modified via SetSelected().
    // This prevents the property system from overwriting runtime selection state.
    // The property is still updated when SetSelected() is called for serialization.

    // Sync state modulation colors, honoring theme bindings and per-instance overrides
    LoadThemedStateData();

    // Sync visual state with enabled property changes
    if (!m_ButtonEnabled && m_CurrentState != RadioButtonState::Disabled) {
        TransitionToState(RadioButtonState::Disabled);
    } else if (m_ButtonEnabled && wasEnabled != m_ButtonEnabled) {
        // Re-enabled: update to appropriate state
        if (m_IsSelected) {
            TransitionToState(RadioButtonState::Selected);
        } else {
            TransitionToState(RadioButtonState::Normal);
        }
    }
}

void RadioButton::LoadThemedStateData() {
    struct StateMap { RadioButtonState state; const char* prefix; };
    static const StateMap kStates[] = {
        { RadioButtonState::Normal,        "normal" },
        { RadioButtonState::Hover,         "hover" },
        { RadioButtonState::Pressed,       "pressed" },
        { RadioButtonState::Selected,      "selected" },
        { RadioButtonState::SelectedHover, "selectedHover" },
        { RadioButtonState::Disabled,      "disabled" },
    };
    for (const StateMap& sm : kStates) {
        int idx = static_cast<int>(sm.state);
        std::string p(sm.prefix);
        m_StateStyles[idx].modulationColor = ResolveThemedColor(p + "Modulation", p + "Modulation");
    }
}

void RadioButton::UpdateMouseInteraction(float) {
    if (!m_ButtonEnabled) {
        if (m_CurrentState != RadioButtonState::Disabled) {
            TransitionToState(RadioButtonState::Disabled);
        }
        return;
    }

    input::InputManager& inputMgr = input::InputManager::Get();

    // Mouse in the centered canvas space the control rects live in, so hit-testing
    // matches rendering on every backend (shared helper).
    Vec2 mousePos = GetCanvasMousePosition();

    bool isOver = IsMouseOver(mousePos) && IsTopPointerTarget(mousePos);
    bool leftPressed = inputMgr.IsMouseButtonPressed(input::MouseButton::Left);

    // Track when press starts inside button
    if (leftPressed && isOver && !m_PressedInside) {
        // Only set if we're transitioning into pressed state
        if (m_CurrentState != RadioButtonState::Pressed) {
            m_PressedInside = true;
        }
    }

    // Check if we were in a pressed state and now releasing while over
    bool wasInPressedState = (m_CurrentState == RadioButtonState::Pressed);

    // Select when releasing from pressed state while still over the button
    // Radio buttons cannot be deselected by clicking them again - only by selecting another radio in the group
    if (wasInPressedState && !leftPressed && isOver && m_PressedInside) {
        if (!m_IsSelected) {
            SetSelected(true);
        }
        m_PressedInside = false;
    }

    // Reset pressed tracking if mouse released or moved off
    if (!leftPressed || !isOver) {
        m_PressedInside = false;
    }

    // Determine new state based on mouse interaction and selection state
    RadioButtonState newState = m_CurrentState;

    if (m_IsSelected) {
        // Selected states
        if (isOver && leftPressed) {
            newState = RadioButtonState::Pressed;
        } else if (isOver) {
            newState = RadioButtonState::SelectedHover;
        } else {
            newState = RadioButtonState::Selected;
        }
    } else {
        // Normal states
        if (isOver && leftPressed) {
            newState = RadioButtonState::Pressed;
        } else if (isOver) {
            newState = RadioButtonState::Hover;
        } else {
            newState = RadioButtonState::Normal;
        }
    }

    if (newState != m_CurrentState) {
        TransitionToState(newState);
    }

    m_MouseWasOver = isOver;
}

bool RadioButton::IsMouseOver(const Vec2& mousePos) const {
    if (!GetOwner()) {
        return false;
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return false;
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 totalSize = CalculateTotalSize();

    // Position is center-pivot, calculate bounds
    Vec2 halfSize = totalSize * 0.5f;
    return mousePos.x >= position.x - halfSize.x && mousePos.x <= position.x + halfSize.x &&
           mousePos.y >= position.y - halfSize.y && mousePos.y <= position.y + halfSize.y;
}

void RadioButton::TransitionToState(RadioButtonState newState) {
    if (newState == m_CurrentState) {
        return;
    }

    m_PreviousState = m_CurrentState;
    m_CurrentState = newState;
}

void RadioButton::PlaySelectionSound() {
    if (m_SelectSoundPath.empty() || !m_SelectSoundAsset.IsValid() || !m_SelectSoundAsset->IsLoaded()) {
        return;
    }

    audio::AudioManager& audioMgr = audio::AudioManager::GetInstance();
    audioMgr.Play(m_SelectSoundAsset, "SFX", audio::PlaybackMode::OneShot, 1.0f);
}

void RadioButton::DeselectOtherRadioButtons() {
    if (m_GroupName.empty() || !GetOwner()) {
        return;
    }

    // Search for sibling radio buttons with the same group name
    Node* parent = GetOwner()->GetParent();
    if (!parent) {
        return;
    }

    auto children = parent->GetChildren();
    for (auto& child : children) {
        if (child.get() == GetOwner()) {
            continue; // Skip self
        }

        auto radioBtn = child->GetComponent<RadioButton>();
        if (radioBtn && radioBtn->GetGroupName() == m_GroupName) {
            radioBtn->SetSelected(false);
        }
    }
}

Vec2 RadioButton::CalculateTextSize() const {
    std::string text = GetText();
    float fontSize = GetFontSize();

    if (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {
        // Estimate text size if font isn't loaded yet (for layout purposes)
        // Use a rough estimate: 0.6 * fontSize per character width, fontSize for height
        if (text.empty()) {
            return Vec2(0.0f, 0.0f);
        }
        float estimatedWidth = text.length() * fontSize * 0.6f;
        return Vec2(estimatedWidth, fontSize);
    }

    return m_FontAsset->MeasureText(text, fontSize);
}

Vec2 RadioButton::CalculateTotalSize() const {
    float indicatorSize = GetIndicatorSize();
    float textOffset = GetTextOffset();
    Vec2 textSize = CalculateTextSize();

    // Ensure minimum indicator size if property isn't initialized yet
    if (indicatorSize <= 0.0f) {
        indicatorSize = 20.0f;  // Default from property definition
    }
    if (textOffset < 0.0f) {
        textOffset = 10.0f;  // Default from property definition
    }

    float totalWidth = indicatorSize + textOffset + textSize.x;
    float totalHeight = std::max(indicatorSize, textSize.y);

    // Ensure minimum size
    if (totalWidth < indicatorSize) totalWidth = indicatorSize;
    if (totalHeight < indicatorSize) totalHeight = indicatorSize;

    return Vec2(totalWidth, totalHeight);
}

Vec2 RadioButton::GetContentMinSize() const {
    return CalculateTotalSize();
}

Color RadioButton::GetEffectiveColor(const Color& baseColor) const {
    const Color& modulation = m_StateStyles[static_cast<int>(m_CurrentState)].modulationColor;
    return Color(
        baseColor.r * modulation.r,
        baseColor.g * modulation.g,
        baseColor.b * modulation.b,
        baseColor.a * modulation.a
    );
}

// ===== Rendering Methods =====

void RadioButton::buildDrawCommands(RenderContext& ctx) {
    if (!GetOwner() || !IsEnabled()) {
        return;
    }


    if (!ctx.getDevice()) {
        return;
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return;
    }

    // A resolution change re-bakes the font atlas at a new oversampling density,
    // which repacks glyph UVs; regenerate the cached text mesh so it matches.
    if (ConsumeFontOversampleChanged()) {
        m_TextMeshNeedsRegeneration = true;
    }

    // Check if indicator size is valid (properties are ready)
    float indicatorSize = GetIndicatorSize();
    if (indicatorSize <= 0.0f) {
        return;
    }

    Vec2 position = node2D->GetGlobalPosition();
    float rotation = node2D->GetGlobalRotation();

    // Handle font loading
    std::string fontPath = GetFontPath();
    float currentFontSize = GetFontSize();

    bool fontPathChanged = (fontPath != m_CurrentFontPath);
    bool fontSizeChanged = (currentFontSize != m_CurrentFontSize);

    bool needsLoad = fontPathChanged || fontSizeChanged || (!m_FontAsset.IsValid() && !fontPath.empty());

    if (needsLoad) {
        m_CurrentFontPath = fontPath;
        m_CurrentFontSize = currentFontSize;

        if (m_FontHandle.isValid()) {
            m_FontHandle = FontHandle();
        }

        m_FontAsset.Reset();

        if (!fontPath.empty()) {
            m_FontAsset = asset::AssetRef<asset::FontAsset>(new asset::FontAsset());

            uint32_t minAtlasSize = static_cast<uint32_t>(currentFontSize * 10.0f);
            uint32_t atlasSize = 512;
            while (atlasSize < minAtlasSize && atlasSize < 4096) {
                atlasSize *= 2;
            }

            bool loaded = m_FontAsset->LoadFromFile(fontPath, currentFontSize, atlasSize, atlasSize);
            if (!loaded) {
                m_FontAsset.Reset();
            } else {
                // The label is measured with this font, which loads lazily on the first
                // draw -- after any parent container measured this control without it.
                NotifyContentSizeChanged();
            }
        }
    }

    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded() && !m_FontHandle.isValid()) {
        FontDesc fontDesc;
        // Use GetPhysicalPath() to resolve res:// path to filesystem path for file loading
        fontDesc.fontPath = m_FontAsset->GetPhysicalPath();
        fontDesc.fontSize = currentFontSize;
        fontDesc.atlasWidth = m_FontAsset->GetAtlasWidth();
        fontDesc.atlasHeight = m_FontAsset->GetAtlasHeight();

        IGfxDevice* device = ctx.getDevice();
        if (device) {
            m_FontHandle = device->createFontAtlas(fontDesc);
        }
        m_TextMeshNeedsRegeneration = true;
    }

    // Check for text changes that require mesh regeneration
    std::string currentText = GetText();
    Color currentFontColor = GetEffectiveColor(GetFontColor());

    if (currentText != m_CachedText || currentFontSize != m_CachedFontSize) {
        m_TextMeshNeedsRegeneration = true;
    }
    if (currentFontColor != m_CachedFontColor) {
        m_TextMeshNeedsRegeneration = true;
    }
    if (position != m_CachedPosition || std::abs(rotation - m_CachedRotation) > 0.0001f) {
        m_TextMeshNeedsRegeneration = true;
    }

    // Call render methods
    RenderIndicator(ctx, position, rotation);
    RenderText(ctx, position, rotation);
}

void RadioButton::RenderIndicator(RenderContext& ctx, const Vec2& position, float rotation) {
    if (!ctx.getDevice()) {
        return;
    }

    float indicatorSize = GetIndicatorSize();

    if (indicatorSize <= 0.0f) {
        return;
    }

    Vec2 totalSize = CalculateTotalSize();
    float borderWidth = GetBorderWidth();
    float innerCircleScale = GetInnerCircleScale();

    // Calculate indicator position (left side of total bounds, centered vertically)
    Vec2 indicatorLocalOffset;
    indicatorLocalOffset.x = -totalSize.x * 0.5f + indicatorSize * 0.5f;
    indicatorLocalOffset.y = 0.0f;

    // Apply rotation to offset if needed
    Vec2 indicatorOffset = indicatorLocalOffset;
    if (std::abs(rotation) > 0.0001f) {
        float cosR = std::cos(rotation);
        float sinR = std::sin(rotation);
        indicatorOffset.x = indicatorLocalOffset.x * cosR - indicatorLocalOffset.y * sinR;
        indicatorOffset.y = indicatorLocalOffset.x * sinR + indicatorLocalOffset.y * cosR;
    }

    Vec2 indicatorPos = position + indicatorOffset;

    // Use corner radius equal to half size for perfect circle
    float circleRadius = indicatorSize * 0.5f;
    Vec4 cornerRadius(circleRadius, circleRadius, circleRadius, circleRadius);

    // Get colors with state modulation applied
    Color outerColor = GetEffectiveColor(GetOuterCircleColor());
    Color bgColor = GetEffectiveColor(GetBackgroundColor());

    // Draw outer circle (border) if border width > 0
    if (borderWidth > 0.0f) {
        Vec2 outerSize(indicatorSize + borderWidth * 2.0f, indicatorSize + borderWidth * 2.0f);
        float outerRadius = (indicatorSize + borderWidth * 2.0f) * 0.5f;
        Vec4 outerCornerRadius(outerRadius, outerRadius, outerRadius, outerRadius);
        Vec4 borderWidthVec(borderWidth, borderWidth, borderWidth, borderWidth);

        if (std::abs(rotation) > 0.0001f) {
            ctx.drawRoundedRectBorder(
                indicatorPos,
                outerSize,
                outerCornerRadius,
                borderWidthVec,
                outerColor,
                rotation
            );
        } else {
            // Convert center-pivot to top-left for non-rotated
            Vec2 outerTopLeft(indicatorPos.x - outerSize.x * 0.5f, indicatorPos.y - outerSize.y * 0.5f);
            ctx.drawRoundedRectBorder(
                outerTopLeft,
                outerSize,
                outerCornerRadius,
                borderWidthVec,
                outerColor
            );
        }
    }

    // Draw background circle
    Vec2 bgSize(indicatorSize, indicatorSize);
    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRoundedRect(
            indicatorPos,
            bgSize,
            cornerRadius,
            bgColor,
            rotation,
            0
        );
    } else {
        // Convert center-pivot to top-left for non-rotated
        Vec2 bgTopLeft(indicatorPos.x - indicatorSize * 0.5f, indicatorPos.y - indicatorSize * 0.5f);
        ctx.drawRoundedRect(
            bgTopLeft,
            bgSize,
            cornerRadius,
            bgColor,
            0
        );
    }

    // Draw inner circle if selected - use base color without state modulation for clear visibility
    if (m_IsSelected) {
        float innerSize = indicatorSize * innerCircleScale;
        float innerRadius = innerSize * 0.5f;
        Vec4 innerCornerRadius(innerRadius, innerRadius, innerRadius, innerRadius);
        Vec2 innerSizeVec(innerSize, innerSize);

        // Use the inner circle color directly without state modulation
        // This ensures the selection indicator is always clearly visible
        Color innerColorDirect = GetInnerCircleColor();

        if (std::abs(rotation) > 0.0001f) {
            ctx.drawRoundedRect(
                indicatorPos,
                innerSizeVec,
                innerCornerRadius,
                innerColorDirect,
                rotation,
                0
            );
        } else {
            // Convert center-pivot to top-left for non-rotated
            Vec2 innerTopLeft(indicatorPos.x - innerSize * 0.5f, indicatorPos.y - innerSize * 0.5f);
            ctx.drawRoundedRect(
                innerTopLeft,
                innerSizeVec,
                innerCornerRadius,
                innerColorDirect,
                0
            );
        }
    }
}

void RadioButton::RegenerateTextMesh(RenderContext& ctx, const Vec2& position, float rotation) {
    IGfxDevice* device = ctx.getDevice();
    if (!device) {
        return;
    }

    std::string text = GetText();
    float fontSize = GetFontSize();

    if (text.empty()) {
        if (m_TextMesh.isValid()) {
            device->destroyMesh(m_TextMesh);
            m_TextMesh = MeshHandle();
        }
        m_CachedText = "";
        m_CachedFontSize = 0.0f;
        m_CachedPosition = position;
        m_CachedRotation = rotation;
        return;
    }

    if (!m_FontHandle.isValid()) {
        return;
    }

    const FontAtlas* fontAtlas = device->getFontAtlas(m_FontHandle);
    if (!fontAtlas || fontAtlas->fontSize <= 0.0f) {
        return;
    }

    // Calculate text position (right of indicator)
    float indicatorSize = GetIndicatorSize();
    float textOffset = GetTextOffset();
    Vec2 totalSize = CalculateTotalSize();
    Vec2 textSize = CalculateTextSize();

    Vec2 textLocalOffset;
    textLocalOffset.x = -totalSize.x * 0.5f + indicatorSize + textOffset;

    if (m_FontAsset.IsValid() && m_FontAsset->IsLoaded()) {
        float scaleFactor = fontSize / m_FontAsset->GetFontSize();
        float ascent = m_FontAsset->GetAscent() * scaleFactor;
        float descent = m_FontAsset->GetDescent() * scaleFactor;
        textLocalOffset.y = -(ascent + descent) * 0.5f;
    } else {
        textLocalOffset.y = 0.0f;
    }

    // Apply rotation to text offset
    Vec2 rotatedTextOffset = textLocalOffset;
    if (std::abs(rotation) > 0.0001f) {
        float cosR = std::cos(rotation);
        float sinR = std::sin(rotation);
        rotatedTextOffset.x = textLocalOffset.x * cosR - textLocalOffset.y * sinR;
        rotatedTextOffset.y = textLocalOffset.x * sinR + textLocalOffset.y * cosR;
    }

    Vec2 textPos = position + rotatedTextOffset;

    float scale = fontSize / fontAtlas->fontSize;
    MeshData textMeshData;
    Vec2 cursor = textPos;
    uint32_t vertexOffset = 0;

    Color fontColor = GetEffectiveColor(GetFontColor());

    // Lambda to rotate vertex around button position (like Label.cpp does)
    auto rotateVertex = [&](float x, float y) -> Vec3 {
        if (std::abs(rotation) > 0.0001f) {
            float relX = x - position.x;
            float relY = y - position.y;
            float cosR = std::cos(rotation);
            float sinR = std::sin(rotation);
            float rotX = relX * cosR - relY * sinR;
            float rotY = relX * sinR + relY * cosR;
            return Vec3(rotX + position.x, rotY + position.y, 0.0f);
        }
        return Vec3(x, y, 0.0f);
    };

    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        uint32_t codepoint = static_cast<uint32_t>(c);

        if (c == '\n') {
            cursor.x = textPos.x;
            cursor.y += fontAtlas->lineHeight * scale;
            continue;
        }

        const Glyph* glyph = fontAtlas->getGlyph(codepoint);
        if (!glyph) {
            if (c == ' ') {
                cursor.x += fontAtlas->fontSize * 0.25f * scale;
            }
            continue;
        }

        Vec2 glyphPos;
        glyphPos.x = cursor.x + glyph->bearing.x * scale;
        glyphPos.y = cursor.y - glyph->bearing.y * scale;
        Vec2 glyphSize = glyph->size * scale;

        // Create vertices with proper structure and rotation
        Vertex v0, v1, v2, v3;

        // v0: bottom-left
        v0.position = rotateVertex(glyphPos.x, glyphPos.y - glyphSize.y);
        v0.normal = Vec3(0, 0, 1);
        v0.texCoord = Vec2(glyph->uvMin.x, glyph->uvMax.y);
        v0.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        // v1: bottom-right
        v1.position = rotateVertex(glyphPos.x + glyphSize.x, glyphPos.y - glyphSize.y);
        v1.normal = Vec3(0, 0, 1);
        v1.texCoord = Vec2(glyph->uvMax.x, glyph->uvMax.y);
        v1.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        // v2: top-right
        v2.position = rotateVertex(glyphPos.x + glyphSize.x, glyphPos.y);
        v2.normal = Vec3(0, 0, 1);
        v2.texCoord = Vec2(glyph->uvMax.x, glyph->uvMin.y);
        v2.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        // v3: top-left
        v3.position = rotateVertex(glyphPos.x, glyphPos.y);
        v3.normal = Vec3(0, 0, 1);
        v3.texCoord = Vec2(glyph->uvMin.x, glyph->uvMin.y);
        v3.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        textMeshData.vertices.push_back(v0);
        textMeshData.vertices.push_back(v1);
        textMeshData.vertices.push_back(v2);
        textMeshData.vertices.push_back(v3);

        // Indices
        textMeshData.indices.push_back(vertexOffset + 0);
        textMeshData.indices.push_back(vertexOffset + 1);
        textMeshData.indices.push_back(vertexOffset + 2);
        textMeshData.indices.push_back(vertexOffset + 0);
        textMeshData.indices.push_back(vertexOffset + 2);
        textMeshData.indices.push_back(vertexOffset + 3);

        vertexOffset += 4;
        cursor.x += glyph->advance * scale;
    }

    if (m_TextMesh.isValid()) {
        device->destroyMesh(m_TextMesh);
        m_TextMesh = MeshHandle();
    }

    if (!textMeshData.vertices.empty()) {
        textMeshData.calculateBounds();
        m_TextMesh = device->createMesh(textMeshData);
    }

    m_CachedText = text;
    m_CachedFontSize = fontSize;
    m_CachedPosition = position;
    m_CachedRotation = rotation;
    m_CachedFontColor = fontColor;
}

void RadioButton::RenderText(RenderContext& ctx, const Vec2& position, float rotation) {
    IGfxDevice* device = ctx.getDevice();
    if (!device) {
        return;
    }

    std::string text = GetText();
    if (text.empty()) {
        return;
    }

    if (!m_FontHandle.isValid()) {
        return;
    }

    float fontSize = GetFontSize();
    Color fontColor = GetEffectiveColor(GetFontColor());

    // Check if we need to regenerate the text mesh
    if (text != m_CachedText || fontSize != m_CachedFontSize ||
        position != m_CachedPosition || std::abs(rotation - m_CachedRotation) > 0.0001f ||
        fontColor != m_CachedFontColor || m_TextMeshNeedsRegeneration || !m_TextMesh.isValid()) {
        RegenerateTextMesh(ctx, position, rotation);
        m_TextMeshNeedsRegeneration = false;
    }

    if (!m_TextMesh.isValid()) {
        return;
    }

    const FontAtlas* fontAtlas = device->getFontAtlas(m_FontHandle);
    if (!fontAtlas || !fontAtlas->texture.isValid()) {
        return;
    }

    MaterialHandle textMaterial = ctx.getDefaultTextMaterial();
    if (!textMaterial.isValid()) {
        return;
    }

    MaterialPropertyBlock overrides;
    overrides.setTexture("u_FontAtlas", fontAtlas->texture);
    overrides.setColor("u_TextColor", fontColor);

    Mat4 transform = Mat4::Identity();
    ctx.drawMesh(m_TextMesh, textMaterial, transform, overrides);
}

// ===== IRenderableComponent Implementation =====

AABB RadioButton::getWorldBounds() const {
    if (!GetOwner()) {
        return AABB();
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return AABB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 totalSize = CalculateTotalSize();

    // Ensure we have valid bounds
    if (totalSize.x <= 0.0f) totalSize.x = 20.0f;
    if (totalSize.y <= 0.0f) totalSize.y = 20.0f;

    Vec2 halfSize = totalSize * 0.5f;

    AABB bounds;
    bounds.min = Vec3(position.x - halfSize.x, position.y - halfSize.y, 0.0f);
    bounds.max = Vec3(position.x + halfSize.x, position.y + halfSize.y, 0.0f);
    return bounds;
}

RenderLayer RadioButton::getRenderLayer() const {
    int layer = GetLayer();
    int sortingOrder = GetSortingOrder();

    return static_cast<RenderLayer>(layer * 1000 + sortingOrder);
}

SpatialType RadioButton::getSpatialType() const {
    return GetUISpatialType();
}

math::OBB RadioButton::getOrientedBounds() const {
    if (!GetOwner()) {
        return math::OBB();
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return math::OBB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 totalSize = CalculateTotalSize();

    // Ensure we have valid bounds
    if (totalSize.x <= 0.0f) totalSize.x = 20.0f;
    if (totalSize.y <= 0.0f) totalSize.y = 20.0f;

    math::OBB obb;
    obb.center = Vec3(position.x, position.y, 0.0f);
    obb.extents = Vec3(totalSize.x * 0.5f, totalSize.y * 0.5f, 0.1f);
    obb.rotation = Quat::Identity();
    return obb;
}

bool RadioButton::IntersectRay(const math::Ray& ray, float& outDistance) const {
    // Simple AABB ray intersection for 2D UI elements
    AABB bounds = getWorldBounds();
    return bounds.IntersectRay(ray, outDistance);
}

bool RadioButton::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
    std::string fontPath = GetFontPath();
    if (fontPath.empty()) return false;

    auto& assetDb = asset::AssetDatabase::GetInstance();
    std::string resolvedFontPath;
    if (assetDb.IsInitialized()) {
        resolvedFontPath = assetDb.ResolveAsset(fontPath);
    }

    bool matches = (fontPath == changedPath) ||
                   (!resolvedFontPath.empty() && !resolvedChangedPath.empty() &&
                    resolvedFontPath == resolvedChangedPath);

    if (matches) {
        
        m_FontHandle = FontHandle();
        m_FontAsset.Reset();
        m_CurrentFontPath.clear();
        m_TextMeshNeedsRegeneration = true;
        return true;
    }

    return false;
}

} // namespace components
} // namespace lupine


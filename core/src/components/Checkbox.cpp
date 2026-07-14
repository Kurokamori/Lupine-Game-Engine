#include "lupine/components/Checkbox.hpp"
#include "lupine/components/CheckList.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/DrawCommand.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/rendering/TextureUpload.hpp"
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

Checkbox::Checkbox()
    : UIControl("Checkbox")
    , m_ButtonEnabled(true)
    , m_IsChecked(false)
    , m_CurrentState(CheckboxState::Normal)
    , m_PreviousState(CheckboxState::Normal)
    , m_CheckboxValue(0)
    , m_CheckList(nullptr)
    , m_TextMeshNeedsRegeneration(true)
    , m_MouseWasOver(false)
{
    // Initialize default state modulation colors
    m_StateStyles[static_cast<int>(CheckboxState::Normal)].modulationColor = Color::White();
    m_StateStyles[static_cast<int>(CheckboxState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateStyles[static_cast<int>(CheckboxState::Pressed)].modulationColor = Color(0.9f, 0.9f, 0.9f, 1.0f);
    m_StateStyles[static_cast<int>(CheckboxState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

Checkbox::Checkbox(const std::string& name)
    : UIControl(name)
    , m_ButtonEnabled(true)
    , m_IsChecked(false)
    , m_CurrentState(CheckboxState::Normal)
    , m_PreviousState(CheckboxState::Normal)
    , m_CheckboxValue(0)
    , m_CheckList(nullptr)
    , m_TextMeshNeedsRegeneration(true)
    , m_MouseWasOver(false)
{
    // Initialize default state modulation colors
    m_StateStyles[static_cast<int>(CheckboxState::Normal)].modulationColor = Color::White();
    m_StateStyles[static_cast<int>(CheckboxState::Hover)].modulationColor = Color(1.2f, 1.2f, 1.2f, 1.0f);
    m_StateStyles[static_cast<int>(CheckboxState::Pressed)].modulationColor = Color(0.9f, 0.9f, 0.9f, 1.0f);
    m_StateStyles[static_cast<int>(CheckboxState::Disabled)].modulationColor = Color(0.5f, 0.5f, 0.5f, 0.5f);
}

Checkbox::~Checkbox() {
}

void Checkbox::DefineProperties() {
    // Shared UIControl layout properties (anchors/size-flags/useUISpace + width/height).
    DefineUIControlProperties(20.0f, 20.0f, "useUISpace", "Layout");

    // Box
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(boxSize, 20.0f, 5.0f, 100.0f, 1.0f, "Box"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(borderColor, Color, Color(0.7f, 0.7f, 0.7f, 1.0f), "Box"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(backgroundColor, Color, Color(0.2f, 0.2f, 0.2f, 1.0f), "Box"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(checkmarkColor, Color, Color(0.3f, 0.7f, 1.0f, 1.0f), "Box"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(cornerRadius, 3.0f, 0.0f, 50.0f, 0.5f, "Box"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(borderWidth, 2.0f, 0.0f, 10.0f, 0.5f, "Box"));

    // Textures
    DefineProperty(PROPERTY_DEFAULT_GROUP(useTextures, Bool, false, "Textures"));
    DefineProperty(PROPERTY_FILE_GROUP(uncheckedTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Textures"));
    DefineProperty(PROPERTY_FILE_GROUP(checkedTexturePath, std::string(""), "*.png,*.jpg,*.jpeg,*.bmp,*.tga", "Textures"));

    // Rendering
    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(sortingOrder, 0, -1000, 1000, 1, "Rendering"));
    // Checkbox State
    DefineProperty(PROPERTY_DEFAULT_GROUP(buttonEnabled, Bool, true, "Checkbox"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(isChecked, Bool, false, "Checkbox"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(groupName, String, std::string(""), "Checkbox"));
    DefineProperty(PROPERTY_INT_RANGE_GROUP(checkboxValue, 0, 0, 1000, 1, "Checkbox"));

    // Text
    DefineProperty(PROPERTY_DEFAULT_GROUP(text, String, std::string("Checkbox"), "Text"));
    DefineProperty(PROPERTY_FILE_GROUP(fontPath, std::string(""), "*.ttf,*.otf", "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(fontSize, 16.0f, 1.0f, 256.0f, 1.0f, "Text"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(fontColor, Color, Color::White(), "Text"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(textOffset, 10.0f, 0.0f, 100.0f, 1.0f, "Text"));

    // State modulation colors
    DefineProperty(PROPERTY_DEFAULT_GROUP(normalModulation, Color, Color::White(), "StateModulation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(hoverModulation, Color, Color(1.2f, 1.2f, 1.2f, 1.0f), "StateModulation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(pressedModulation, Color, Color(0.9f, 0.9f, 0.9f, 1.0f), "StateModulation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(checkedModulation, Color, Color::White(), "StateModulation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(checkedHoverModulation, Color, Color(1.2f, 1.2f, 1.2f, 1.0f), "StateModulation"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(disabledModulation, Color, Color(0.5f, 0.5f, 0.5f, 0.5f), "StateModulation"));

    // Audio
    DefineProperty(PROPERTY_FILE_GROUP(checkSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "Audio"));
    DefineProperty(PROPERTY_FILE_GROUP(uncheckSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "Audio"));
    DefineProperty(PROPERTY_FILE_GROUP(hoverSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "Audio"));
    DefineProperty(PROPERTY_FILE_GROUP(pressSoundPath, std::string(""), "*.wav,*.ogg,*.mp3", "Audio"));
}

void Checkbox::OnAwake() {
    m_ButtonEnabled = GetPropertyValue<bool>("buttonEnabled");
    m_IsChecked = GetPropertyValue<bool>("isChecked");
    m_GroupName = GetPropertyValue<std::string>("groupName");
    m_CheckboxValue = GetPropertyValue<int>("checkboxValue");

    LoadThemedStateData();

    // Load audio
    std::string checkSoundPath = GetPropertyValue<std::string>("checkSoundPath");
    if (!checkSoundPath.empty()) {
        SetCheckSoundPath(checkSoundPath);
    }
    std::string uncheckSoundPath = GetPropertyValue<std::string>("uncheckSoundPath");
    if (!uncheckSoundPath.empty()) {
        SetUncheckSoundPath(uncheckSoundPath);
    }
    std::string hoverSoundPath = GetPropertyValue<std::string>("hoverSoundPath");
    if (!hoverSoundPath.empty()) {
        SetHoverSoundPath(hoverSoundPath);
    }
    std::string pressSoundPath = GetPropertyValue<std::string>("pressSoundPath");
    if (!pressSoundPath.empty()) {
        SetPressSoundPath(pressSoundPath);
    }

    // Load font
    std::string fontPath = GetFontPath();
    if (!fontPath.empty()) {
        m_CurrentFontPath = fontPath;
    }

    // Set initial state
    if (!m_ButtonEnabled) {
        m_CurrentState = CheckboxState::Disabled;
    } else if (m_IsChecked) {
        m_CurrentState = CheckboxState::Checked;
    } else {
        m_CurrentState = CheckboxState::Normal;
    }
}

void Checkbox::OnReady() {
    m_TextMeshNeedsRegeneration = true;

    // Find and register with CheckList if group name is set
    if (!m_GroupName.empty() && GetOwner()) {
        NotifyCheckList();
    }
}

void Checkbox::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    UIControl::OnPropertyChanged(propertyName, newValue);

    if (propertyName == "groupName") {
        m_GroupName = newValue.get<std::string>();
        if (GetOwner()) {
            NotifyCheckList();
        }
    } else if (propertyName == "checkboxValue") {
        m_CheckboxValue = newValue.get<int>();
    } else if (propertyName == "buttonEnabled") {
        bool newEnabled = newValue.get<bool>();
        if (m_ButtonEnabled != newEnabled) {
            m_ButtonEnabled = newEnabled;
            if (!m_ButtonEnabled) {
                TransitionToState(CheckboxState::Disabled);
            } else {
                TransitionToState(CheckboxState::Normal);
            }
        }
    } else if (propertyName == "text") {
        m_TextMeshNeedsRegeneration = true;
    } else if (propertyName == "fontSize" || propertyName == "fontPath") {
        m_TextMeshNeedsRegeneration = true;
    } else if (propertyName == "isChecked") {
        m_IsChecked = newValue.get<bool>();
    }
}

void Checkbox::OnInput(float deltaTime) {
    if (!IsEnabled()) {
        return;
    }

    UpdateMouseInteraction(deltaTime);
}

void Checkbox::OnUpdate(float deltaTime) {
    if (!IsEnabled()) {
        return;
    }

    UIControl::OnUpdate(deltaTime);
    SyncFromProperties();
}

bool Checkbox::OnGizmoScale(float scaleDelta, int, bool is3D) {
    if (!is3D) {
        // Scale both box size and font size
        float currentBoxSize = GetBoxSize();
        float currentFontSize = GetFontSize();

        float newBoxSize = std::max(5.0f, currentBoxSize + scaleDelta * currentBoxSize);
        float newFontSize = std::max(1.0f, std::min(256.0f, currentFontSize + scaleDelta * currentFontSize));

        SetBoxSize(newBoxSize);
        SetFontSize(newFontSize);

        m_TextMeshNeedsRegeneration = true;

        return true;
    }
    return false;
}

// ===== Property Accessors =====

float Checkbox::GetBoxSize() const {
    return GetPropertyValue<float>("boxSize");
}

void Checkbox::SetBoxSize(float size) {
    SetPropertyValue<float>("boxSize", size);
}

int Checkbox::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void Checkbox::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

int Checkbox::GetSortingOrder() const {
    return GetPropertyValue<int>("sortingOrder");
}

void Checkbox::SetSortingOrder(int order) {
    SetPropertyValue<int>("sortingOrder", order);
}

// GetUseUISpace/SetUseUISpace are provided by the UIControl base class.

void Checkbox::SetEnabled(bool enabled) {
    m_ButtonEnabled = enabled;
    SetPropertyValue<bool>("buttonEnabled", enabled);

    if (!enabled && m_CurrentState != CheckboxState::Disabled) {
        TransitionToState(CheckboxState::Disabled);
    } else if (enabled && m_CurrentState == CheckboxState::Disabled) {
        TransitionToState(m_IsChecked ? CheckboxState::Checked : CheckboxState::Normal);
    }
}

void Checkbox::SetChecked(bool checked) {

    if (m_IsChecked == checked) {
        
        return;
    }

    m_IsChecked = checked;
    SetPropertyValue<bool>("isChecked", checked);

    if (!m_ButtonEnabled) {
        return;
    }

    if (checked) {
        
        PlayCheckSound();
        TransitionToState(CheckboxState::Checked);
    } else {
        
        PlayUncheckSound();
        TransitionToState(CheckboxState::Normal);
    }

    // Notify CheckList of state change
    if (m_CheckList) {
        m_CheckList->OnCheckboxStateChanged(this);
    }

    // Invoke callback
    if (m_OnCheckedCallback) {
        m_OnCheckedCallback(m_IsChecked);
    }
    Emit("toggled", { m_IsChecked });
}

void Checkbox::DefineSignals() {
    RegisterSignal({"toggled",
                    {{"checked", core::PropertyValueType::Bool}},
                    "Emitted when the checkbox checked state changes."});
}

void Checkbox::Toggle() {
    
    SetChecked(!m_IsChecked);
}

std::string Checkbox::GetGroupName() const {
    return GetPropertyValue<std::string>("groupName");
}

void Checkbox::SetGroupName(const std::string& groupName) {
    m_GroupName = groupName;
    SetPropertyValue<std::string>("groupName", groupName);

    // Re-register with CheckList
    if (GetOwner()) {
        NotifyCheckList();
    }
}

int Checkbox::GetCheckboxValue() const {
    return GetPropertyValue<int>("checkboxValue");
}

void Checkbox::SetCheckboxValue(int value) {
    m_CheckboxValue = value;
    SetPropertyValue<int>("checkboxValue", value);
}

const std::vector<UIControl::ThemeBinding>& Checkbox::GetThemeBindings() const {
    static const std::vector<ThemeBinding> kBindings = []() {
        std::vector<ThemeBinding> b = {
            { "borderColor",     "border_color",   ThemeBinding::Kind::Color },
            { "backgroundColor", "background",      ThemeBinding::Kind::Color },
            { "checkmarkColor",  "checkmark_color", ThemeBinding::Kind::Color },
            { "fontColor",       "font_color",      ThemeBinding::Kind::Color },
            { "fontPath",        "font",            ThemeBinding::Kind::Font },
            { "fontSize",        "font_size",       ThemeBinding::Kind::Constant },
            { "cornerRadius",    "corner_radius",   ThemeBinding::Kind::Constant },
            { "borderWidth",     "border_width",    ThemeBinding::Kind::Constant }
        };
        const char* states[] = { "normal", "hover", "pressed", "checked", "checkedHover", "disabled" };
        for (const char* s : states) {
            std::string p(s);
            b.push_back({ p + "Modulation", p + "Modulation", ThemeBinding::Kind::Color });
        }
        return b;
    }();
    return kBindings;
}

Color Checkbox::GetBorderColor() const {
    return ResolveThemedColor("borderColor", "border_color");
}

void Checkbox::SetBorderColor(const Color& color) {
    SetThemedProperty<Color>("borderColor", color);
}

Color Checkbox::GetBackgroundColor() const {
    return ResolveThemedColor("backgroundColor", "background");
}

void Checkbox::SetBackgroundColor(const Color& color) {
    SetThemedProperty<Color>("backgroundColor", color);
}

Color Checkbox::GetCheckmarkColor() const {
    return ResolveThemedColor("checkmarkColor", "checkmark_color");
}

void Checkbox::SetCheckmarkColor(const Color& color) {
    SetThemedProperty<Color>("checkmarkColor", color);
}

float Checkbox::GetCornerRadius() const {
    return ResolveThemedConstant("cornerRadius", "corner_radius");
}

void Checkbox::SetCornerRadius(float radius) {
    SetThemedProperty<float>("cornerRadius", radius);
}

float Checkbox::GetBorderWidth() const {
    return ResolveThemedConstant("borderWidth", "border_width");
}

void Checkbox::SetBorderWidth(float width) {
    SetThemedProperty<float>("borderWidth", width);
}

std::string Checkbox::GetUncheckedTexturePath() const {
    return GetPropertyValue<std::string>("uncheckedTexturePath");
}

void Checkbox::SetUncheckedTexturePath(const std::string& path) {
    SetPropertyValue<std::string>("uncheckedTexturePath", path);
}

std::string Checkbox::GetCheckedTexturePath() const {
    return GetPropertyValue<std::string>("checkedTexturePath");
}

void Checkbox::SetCheckedTexturePath(const std::string& path) {
    SetPropertyValue<std::string>("checkedTexturePath", path);
}

bool Checkbox::GetUseTextures() const {
    return GetPropertyValue<bool>("useTextures");
}

void Checkbox::SetUseTextures(bool use) {
    SetPropertyValue<bool>("useTextures", use);
}

std::string Checkbox::GetText() const {
    return GetPropertyValue<std::string>("text");
}

void Checkbox::SetText(const std::string& text) {
    SetPropertyValue<std::string>("text", text);
    m_TextMeshNeedsRegeneration = true;
}

std::string Checkbox::GetFontPath() const {
    return ResolveThemedFontPath("fontPath", "font");
}

void Checkbox::SetFontPath(const std::string& path) {
    SetThemedProperty<std::string>("fontPath", path);
    m_CurrentFontPath = path;
    m_TextMeshNeedsRegeneration = true;
}

float Checkbox::GetFontSize() const {
    return ResolveThemedFontSize("fontSize", "font_size", "font");
}

void Checkbox::SetFontSize(float size) {
    SetThemedProperty<float>("fontSize", size);
    m_CurrentFontSize = size;
    m_TextMeshNeedsRegeneration = true;
}

Color Checkbox::GetFontColor() const {
    return ResolveThemedColor("fontColor", "font_color");
}

void Checkbox::SetFontColor(const Color& color) {
    SetThemedProperty<Color>("fontColor", color);
    m_TextMeshNeedsRegeneration = true;
}

float Checkbox::GetTextOffset() const {
    return GetPropertyValue<float>("textOffset");
}

void Checkbox::SetTextOffset(float offset) {
    SetPropertyValue<float>("textOffset", offset);
}

const Color& Checkbox::GetStateModulation(CheckboxState state) const {
    return m_StateStyles[static_cast<int>(state)].modulationColor;
}

void Checkbox::SetStateModulation(CheckboxState state, const Color& color) {
    m_StateStyles[static_cast<int>(state)].modulationColor = color;

    std::string propName;
    switch (state) {
        case CheckboxState::Normal: propName = "normalModulation"; break;
        case CheckboxState::Hover: propName = "hoverModulation"; break;
        case CheckboxState::Pressed: propName = "pressedModulation"; break;
        case CheckboxState::Checked: propName = "checkedModulation"; break;
        case CheckboxState::CheckedHover: propName = "checkedHoverModulation"; break;
        case CheckboxState::Disabled: propName = "disabledModulation"; break;
        default: return;
    }
    SetPropertyValue<Color>(propName, color);
}

std::string Checkbox::GetCheckSoundPath() const {
    return GetPropertyValue<std::string>("checkSoundPath");
}

void Checkbox::SetCheckSoundPath(const std::string& path) {
    m_CheckSoundPath = path;
    SetPropertyValue<std::string>("checkSoundPath", path);

    if (!path.empty()) {
        m_CheckSoundAsset = asset::AssetRef<asset::AudioAsset>(new asset::AudioAsset());
        bool loaded = m_CheckSoundAsset->LoadFromFile(path);
        if (!loaded) {
            m_CheckSoundAsset.Reset();
        }
    } else {
        m_CheckSoundAsset.Reset();
    }
}

std::string Checkbox::GetUncheckSoundPath() const {
    return GetPropertyValue<std::string>("uncheckSoundPath");
}

void Checkbox::SetUncheckSoundPath(const std::string& path) {
    m_UncheckSoundPath = path;
    SetPropertyValue<std::string>("uncheckSoundPath", path);

    if (!path.empty()) {
        m_UncheckSoundAsset = asset::AssetRef<asset::AudioAsset>(new asset::AudioAsset());
        bool loaded = m_UncheckSoundAsset->LoadFromFile(path);
        if (!loaded) {
            m_UncheckSoundAsset.Reset();
        }
    } else {
        m_UncheckSoundAsset.Reset();
    }
}

std::string Checkbox::GetHoverSoundPath() const {
    return GetPropertyValue<std::string>("hoverSoundPath");
}

void Checkbox::SetHoverSoundPath(const std::string& path) {
    m_HoverSoundPath = path;
    SetPropertyValue<std::string>("hoverSoundPath", path);

    if (!path.empty()) {
        m_HoverSoundAsset = asset::AssetRef<asset::AudioAsset>(new asset::AudioAsset());
        bool loaded = m_HoverSoundAsset->LoadFromFile(path);
        if (!loaded) {
            m_HoverSoundAsset.Reset();
        }
    } else {
        m_HoverSoundAsset.Reset();
    }
}

std::string Checkbox::GetPressSoundPath() const {
    return GetPropertyValue<std::string>("pressSoundPath");
}

void Checkbox::SetPressSoundPath(const std::string& path) {
    m_PressSoundPath = path;
    SetPropertyValue<std::string>("pressSoundPath", path);

    if (!path.empty()) {
        m_PressSoundAsset = asset::AssetRef<asset::AudioAsset>(new asset::AudioAsset());
        bool loaded = m_PressSoundAsset->LoadFromFile(path);
        if (!loaded) {
            m_PressSoundAsset.Reset();
        }
    } else {
        m_PressSoundAsset.Reset();
    }
}

void Checkbox::SetOnCheckedCallback(CheckCallback callback) {
    m_OnCheckedCallback = callback;
}

void Checkbox::NotifyCheckList() {
    if (!GetOwner()) {
        return;
    }

    // Clear existing CheckList reference first
    m_CheckList = nullptr;

    // Search for CheckList component in parent hierarchy
    Node* current = GetOwner();
    while (current) {
        auto checkList = current->GetComponent<CheckList>();
        if (checkList && checkList->GetGroupName() == m_GroupName) {
            SetCheckList(checkList.get());
            checkList->RegisterCheckbox(this);
            return;
        }
        current = current->GetParent();
    }
}

void Checkbox::SetCheckList(CheckList* checkList) {
    m_CheckList = checkList;
}

// ===== Private Methods =====

Color Checkbox::GetSafeColorProperty(const std::string& propName, const Color& defaultColor) const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty(propName);
    if (prop && !prop->GetValueAsJson().is_null()) {
        return prop->GetValue<Color>();
    }
    return defaultColor;
}

void Checkbox::SyncFromProperties() {
    bool wasEnabled = m_ButtonEnabled;

    m_ButtonEnabled = GetPropertyValue<bool>("buttonEnabled");
    m_GroupName = GetPropertyValue<std::string>("groupName");
    m_CheckboxValue = GetPropertyValue<int>("checkboxValue");

    LoadThemedStateData();

    // Sync visual state with enabled property changes
    if (!m_ButtonEnabled && m_CurrentState != CheckboxState::Disabled) {
        TransitionToState(CheckboxState::Disabled);
    } else if (m_ButtonEnabled && wasEnabled != m_ButtonEnabled) {
        // Re-enabled: update to appropriate state
        if (m_IsChecked) {
            TransitionToState(CheckboxState::Checked);
        } else {
            TransitionToState(CheckboxState::Normal);
        }
    }
}

void Checkbox::LoadThemedStateData() {
    struct StateMap { CheckboxState state; const char* prefix; };
    static const StateMap kStates[] = {
        { CheckboxState::Normal,       "normal" },
        { CheckboxState::Hover,        "hover" },
        { CheckboxState::Pressed,      "pressed" },
        { CheckboxState::Checked,      "checked" },
        { CheckboxState::CheckedHover, "checkedHover" },
        { CheckboxState::Disabled,     "disabled" },
    };
    for (const StateMap& sm : kStates) {
        int idx = static_cast<int>(sm.state);
        std::string p(sm.prefix);
        m_StateStyles[idx].modulationColor = ResolveThemedColor(p + "Modulation", p + "Modulation");
    }
}

void Checkbox::UpdateMouseInteraction(float) {
    if (!m_ButtonEnabled) {
        if (m_CurrentState != CheckboxState::Disabled) {
            TransitionToState(CheckboxState::Disabled);
        }
        return;
    }

    input::InputManager& inputMgr = input::InputManager::Get();

    // Mouse in the centered canvas space the control rects live in, so hit-testing
    // matches rendering on every backend (shared helper).
    Vec2 mousePos = GetCanvasMousePosition();

    bool isOver = IsMouseOver(mousePos) && IsTopPointerTarget(mousePos);
    bool leftPressed = inputMgr.IsMouseButtonPressed(input::MouseButton::Left);

    // Play hover sound when entering
    if (isOver && !m_MouseWasOver) {
        PlayHoverSound();
    }

    // Track when press starts inside checkbox
    if (leftPressed && isOver && !m_PressedInside) {
        // Only set if we're transitioning into pressed state
        if (m_CurrentState != CheckboxState::Pressed) {
            m_PressedInside = true;
        }
    }

    // Check if we were in a pressed state and now releasing while over
    bool wasInPressedState = (m_CurrentState == CheckboxState::Pressed);

    // Toggle when releasing from pressed state while still over the checkbox
    if (wasInPressedState && !leftPressed && isOver && m_PressedInside) {
        Toggle();
    }

    // Reset pressed tracking if mouse released or moved off
    if (!leftPressed || !isOver) {
        m_PressedInside = false;
    }

    // Determine new state based on mouse interaction and checked state
    CheckboxState newState = m_CurrentState;

    if (m_IsChecked) {
        // Checked states
        if (isOver && leftPressed) {
            newState = CheckboxState::Pressed;
        } else if (isOver) {
            newState = CheckboxState::CheckedHover;
        } else {
            newState = CheckboxState::Checked;
        }
    } else {
        // Normal states
        if (isOver && leftPressed) {
            newState = CheckboxState::Pressed;
        } else if (isOver) {
            newState = CheckboxState::Hover;
        } else {
            newState = CheckboxState::Normal;
        }
    }

    if (newState != m_CurrentState) {
        TransitionToState(newState);
    }

    m_MouseWasOver = isOver;
}

bool Checkbox::IsMouseOver(const Vec2& mousePos) const {
    if (!GetOwner()) {
        return false;
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return false;
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 totalSize = CalculateTotalSize();

    Vec2 halfSize = totalSize * 0.5f;
    return mousePos.x >= position.x - halfSize.x && mousePos.x <= position.x + halfSize.x &&
           mousePos.y >= position.y - halfSize.y && mousePos.y <= position.y + halfSize.y;
}

void Checkbox::TransitionToState(CheckboxState newState) {
    if (newState == m_CurrentState) {
        return;
    }

    m_PreviousState = m_CurrentState;
    m_CurrentState = newState;
}

void Checkbox::PlayCheckSound() {
    if (m_CheckSoundPath.empty() || !m_CheckSoundAsset.IsValid() || !m_CheckSoundAsset->IsLoaded()) {
        return;
    }

    audio::AudioManager& audioMgr = audio::AudioManager::GetInstance();
    audioMgr.Play(m_CheckSoundAsset, "SFX", audio::PlaybackMode::OneShot, 1.0f);
}

void Checkbox::PlayUncheckSound() {
    if (m_UncheckSoundPath.empty() || !m_UncheckSoundAsset.IsValid() || !m_UncheckSoundAsset->IsLoaded()) {
        return;
    }

    audio::AudioManager& audioMgr = audio::AudioManager::GetInstance();
    audioMgr.Play(m_UncheckSoundAsset, "SFX", audio::PlaybackMode::OneShot, 1.0f);
}

void Checkbox::PlayHoverSound() {
    if (m_HoverSoundPath.empty() || !m_HoverSoundAsset.IsValid() || !m_HoverSoundAsset->IsLoaded()) {
        return;
    }

    audio::AudioManager& audioMgr = audio::AudioManager::GetInstance();
    audioMgr.Play(m_HoverSoundAsset, "SFX", audio::PlaybackMode::OneShot, 1.0f);
}

void Checkbox::PlayPressSound() {
    if (m_PressSoundPath.empty() || !m_PressSoundAsset.IsValid() || !m_PressSoundAsset->IsLoaded()) {
        return;
    }

    audio::AudioManager& audioMgr = audio::AudioManager::GetInstance();
    audioMgr.Play(m_PressSoundAsset, "SFX", audio::PlaybackMode::OneShot, 1.0f);
}

Vec2 Checkbox::CalculateTextSize() const {
    std::string text = GetText();
    float fontSize = GetFontSize();

    if (!m_FontAsset.IsValid() || !m_FontAsset->IsLoaded()) {
        if (text.empty()) {
            return Vec2(0.0f, 0.0f);
        }
        float estimatedWidth = text.length() * fontSize * 0.6f;
        return Vec2(estimatedWidth, fontSize);
    }

    return m_FontAsset->MeasureText(text, fontSize);
}

Vec2 Checkbox::CalculateTotalSize() const {
    float boxSize = GetBoxSize();
    float textOffset = GetTextOffset();
    Vec2 textSize = CalculateTextSize();

    if (boxSize <= 0.0f) {
        boxSize = 20.0f;
    }
    if (textOffset < 0.0f) {
        textOffset = 10.0f;
    }

    float totalWidth = boxSize + textOffset + textSize.x;
    float totalHeight = std::max(boxSize, textSize.y);

    if (totalWidth < boxSize) totalWidth = boxSize;
    if (totalHeight < boxSize) totalHeight = boxSize;

    return Vec2(totalWidth, totalHeight);
}

Vec2 Checkbox::GetContentMinSize() const {
    return CalculateTotalSize();
}

Color Checkbox::GetEffectiveColor(const Color& baseColor) const {
    const Color& modulation = m_StateStyles[static_cast<int>(m_CurrentState)].modulationColor;
    return Color(
        baseColor.r * modulation.r,
        baseColor.g * modulation.g,
        baseColor.b * modulation.b,
        baseColor.a * modulation.a
    );
}

// ===== Rendering Methods =====

void Checkbox::buildDrawCommands(RenderContext& ctx) {
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

    float boxSize = GetBoxSize();
    if (boxSize <= 0.0f) {
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
    RenderBox(ctx, position, rotation);
    RenderText(ctx, position, rotation);
}

void Checkbox::EnsureBoxTextures(RenderContext& ctx) {
    if (!GetUseTextures()) {
        return;
    }

    IGfxDevice* device = ctx.getDevice();
    if (!device) {
        return;
    }

    // Reload whichever path changed since last frame, dropping the stale GPU handle.
    const std::string uncheckedPath = GetUncheckedTexturePath();
    if (uncheckedPath != m_CurrentUncheckedTexturePath) {
        if (m_UncheckedTextureHandle.isValid()) {
            device->destroyTexture(m_UncheckedTextureHandle);
            m_UncheckedTextureHandle = TextureHandle();
        }
        m_UncheckedTextureAsset.Reset();
        if (!uncheckedPath.empty()) {
            m_UncheckedTextureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
            if (!m_UncheckedTextureAsset->LoadFromFile(uncheckedPath, true, asset::ImageColorSpace::sRGB)) {
                m_UncheckedTextureAsset.Reset();
            }
        }
        m_CurrentUncheckedTexturePath = uncheckedPath;
    }

    const std::string checkedPath = GetCheckedTexturePath();
    if (checkedPath != m_CurrentCheckedTexturePath) {
        if (m_CheckedTextureHandle.isValid()) {
            device->destroyTexture(m_CheckedTextureHandle);
            m_CheckedTextureHandle = TextureHandle();
        }
        m_CheckedTextureAsset.Reset();
        if (!checkedPath.empty()) {
            m_CheckedTextureAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
            if (!m_CheckedTextureAsset->LoadFromFile(checkedPath, true, asset::ImageColorSpace::sRGB)) {
                m_CheckedTextureAsset.Reset();
            }
        }
        m_CurrentCheckedTexturePath = checkedPath;
    }

    // Upload any loaded asset that does not yet have a GPU handle.
    if (!m_UncheckedTextureHandle.isValid() && m_UncheckedTextureAsset.IsValid() && m_UncheckedTextureAsset->IsLoaded()) {
        m_UncheckedTextureHandle = lupine::CreateTexture2DFromImage(device, *m_UncheckedTextureAsset, TextureFormat::RGBA8_UNORM);
    }
    if (!m_CheckedTextureHandle.isValid() && m_CheckedTextureAsset.IsValid() && m_CheckedTextureAsset->IsLoaded()) {
        m_CheckedTextureHandle = lupine::CreateTexture2DFromImage(device, *m_CheckedTextureAsset, TextureFormat::RGBA8_UNORM);
    }
}

void Checkbox::RenderBox(RenderContext& ctx, const Vec2& position, float rotation) {
    if (!ctx.getDevice()) {
        return;
    }

    float boxSize = GetBoxSize();
    if (boxSize <= 0.0f) {
        return;
    }

    Vec2 totalSize = CalculateTotalSize();
    float borderWidth = GetBorderWidth();
    float cornerRadius = GetCornerRadius();

    EnsureBoxTextures(ctx);

    // Calculate box position (left side of total bounds, centered vertically)
    Vec2 boxLocalOffset;
    boxLocalOffset.x = -totalSize.x * 0.5f + boxSize * 0.5f;
    boxLocalOffset.y = 0.0f;

    // Apply rotation to offset if needed
    Vec2 boxOffset = boxLocalOffset;
    if (std::abs(rotation) > 0.0001f) {
        float cosR = std::cos(rotation);
        float sinR = std::sin(rotation);
        boxOffset.x = boxLocalOffset.x * cosR - boxLocalOffset.y * sinR;
        boxOffset.y = boxLocalOffset.x * sinR + boxLocalOffset.y * cosR;
    }

    Vec2 boxPos = position + boxOffset;

    // Get colors with state modulation applied
    Color borderColor = GetEffectiveColor(GetBorderColor());
    Color bgColor = GetEffectiveColor(GetBackgroundColor());

    Vec4 corners(cornerRadius, cornerRadius, cornerRadius, cornerRadius);

    // Textured mode: the checked/unchecked image is the complete box visual, so it
    // replaces the flat background + checkmark fill (state modulation still tints it).
    // Falls through to the flat path when the active state has no valid texture.
    if (GetUseTextures()) {
        TextureHandle boxTexture = m_IsChecked ? m_CheckedTextureHandle : m_UncheckedTextureHandle;
        if (boxTexture.isValid()) {
            Color tint = GetEffectiveColor(Color::White());
            Vec2 texSize(boxSize, boxSize);
            if (std::abs(rotation) > 0.0001f) {
                ctx.drawRoundedRect(boxPos, texSize, corners, tint, boxTexture, rotation);
            } else {
                Vec2 texTopLeft(boxPos.x - boxSize * 0.5f, boxPos.y - boxSize * 0.5f);
                ctx.drawRoundedRect(texTopLeft, texSize, corners, tint, boxTexture);
            }
            return;
        }
    }

    // Draw border if border width > 0
    if (borderWidth > 0.0f) {
        Vec2 outerSize(boxSize + borderWidth * 2.0f, boxSize + borderWidth * 2.0f);
        float outerRadius = std::min(cornerRadius + borderWidth, (boxSize + borderWidth * 2.0f) * 0.5f);
        Vec4 outerCorners(outerRadius, outerRadius, outerRadius, outerRadius);
        Vec4 borderWidthVec(borderWidth, borderWidth, borderWidth, borderWidth);

        if (std::abs(rotation) > 0.0001f) {
            ctx.drawRoundedRectBorder(
                boxPos,
                outerSize,
                outerCorners,
                borderWidthVec,
                borderColor,
                rotation
            );
        } else {
            Vec2 outerTopLeft(boxPos.x - outerSize.x * 0.5f, boxPos.y - outerSize.y * 0.5f);
            ctx.drawRoundedRectBorder(
                outerTopLeft,
                outerSize,
                outerCorners,
                borderWidthVec,
                borderColor
            );
        }
    }

    // Draw background
    Vec2 bgSize(boxSize, boxSize);
    if (std::abs(rotation) > 0.0001f) {
        ctx.drawRoundedRect(
            boxPos,
            bgSize,
            corners,
            bgColor,
            rotation,
            0
        );
    } else {
        Vec2 bgTopLeft(boxPos.x - boxSize * 0.5f, boxPos.y - boxSize * 0.5f);
        ctx.drawRoundedRect(
            bgTopLeft,
            bgSize,
            corners,
            bgColor,
            0
        );
    }

    // Draw filled rectangle if checked (similar to radio button's filled circle)
    if (m_IsChecked) {
        Color checkmarkColor = GetCheckmarkColor();

        // Draw filled rounded rectangle with padding
        float fillPadding = boxSize * 0.25f;
        float fillSize = boxSize - (fillPadding * 2.0f);
        Vec2 fillRectSize(fillSize, fillSize);

        if (std::abs(rotation) > 0.0001f) {
            ctx.drawRoundedRect(
                boxPos,
                fillRectSize,
                corners * 0.5f,  // Smaller corner radius for the fill
                checkmarkColor,
                rotation,
                0
            );
        } else {
            Vec2 fillTopLeft(boxPos.x - fillSize * 0.5f, boxPos.y - fillSize * 0.5f);
            ctx.drawRoundedRect(
                fillTopLeft,
                fillRectSize,
                corners * 0.5f,  // Smaller corner radius for the fill
                checkmarkColor,
                0
            );
        }
    }
}

void Checkbox::RegenerateTextMesh(RenderContext& ctx, const Vec2& position, float rotation) {
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

    // Calculate text position (right of box)
    float boxSize = GetBoxSize();
    float textOffset = GetTextOffset();
    Vec2 totalSize = CalculateTotalSize();

    Vec2 textLocalOffset;
    textLocalOffset.x = -totalSize.x * 0.5f + boxSize + textOffset;

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

        Vertex v0, v1, v2, v3;

        v0.position = rotateVertex(glyphPos.x, glyphPos.y - glyphSize.y);
        v0.normal = Vec3(0, 0, 1);
        v0.texCoord = Vec2(glyph->uvMin.x, glyph->uvMax.y);
        v0.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        v1.position = rotateVertex(glyphPos.x + glyphSize.x, glyphPos.y - glyphSize.y);
        v1.normal = Vec3(0, 0, 1);
        v1.texCoord = Vec2(glyph->uvMax.x, glyph->uvMax.y);
        v1.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        v2.position = rotateVertex(glyphPos.x + glyphSize.x, glyphPos.y);
        v2.normal = Vec3(0, 0, 1);
        v2.texCoord = Vec2(glyph->uvMax.x, glyph->uvMin.y);
        v2.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        v3.position = rotateVertex(glyphPos.x, glyphPos.y);
        v3.normal = Vec3(0, 0, 1);
        v3.texCoord = Vec2(glyph->uvMin.x, glyph->uvMin.y);
        v3.color = Vec4(fontColor.r, fontColor.g, fontColor.b, fontColor.a);

        textMeshData.vertices.push_back(v0);
        textMeshData.vertices.push_back(v1);
        textMeshData.vertices.push_back(v2);
        textMeshData.vertices.push_back(v3);

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

void Checkbox::RenderText(RenderContext& ctx, const Vec2& position, float rotation) {
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

// ===== IRenderableComponent Interface =====

AABB Checkbox::getWorldBounds() const {
    if (!GetOwner()) {
        return AABB();
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return AABB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    Vec2 totalSize = CalculateTotalSize();
    Vec2 halfSize = totalSize * 0.5f;

    return AABB(
        Vec3(position.x - halfSize.x, position.y - halfSize.y, -0.5f),
        Vec3(position.x + halfSize.x, position.y + halfSize.y, 0.5f)
    );
}

RenderLayer Checkbox::getRenderLayer() const {
    return RenderLayer::UI;
}

SpatialType Checkbox::getSpatialType() const {
    return GetUISpatialType();
}

OBB Checkbox::getOrientedBounds() const {
    if (!GetOwner()) {
        return OBB();
    }

    core::Node2D* node2D = dynamic_cast<core::Node2D*>(GetOwner());
    if (!node2D) {
        return OBB();
    }

    Vec2 position = node2D->GetGlobalPosition();
    float rotation = node2D->GetGlobalRotation();
    Vec2 totalSize = CalculateTotalSize();

    Vec3 center(position.x, position.y, 0.0f);
    Vec3 extents(totalSize.x * 0.5f, totalSize.y * 0.5f, 0.5f);

    Quat orientation = Quat::FromAxisAngle(Vec3(0, 0, 1), rotation);

    return OBB(center, extents, orientation);
}

bool Checkbox::IntersectRay(const Ray& ray, float& outDistance) const {
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

bool Checkbox::OnAssetFileChanged(const std::string& changedPath, const std::string& resolvedChangedPath) {
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


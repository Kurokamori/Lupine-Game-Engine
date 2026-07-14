/**
 * @file UIBasicComponentTests.cpp
 * @brief Headless console tests for the basic UI widget components.
 *
 * These tests drive the pure data/state surface of the UI widgets WITHOUT any
 * live GPU device, window, font atlas, or texture files. Every component is
 * constructed via std::make_shared and its property registry is populated by a
 * direct DefineProperties() call (the same registration the engine performs
 * lazily through RegisterProperties()/Deserialize()). After that, the
 * setter/getter round-trips, enum/mode selections, per-state style data, value
 * clamping and flag toggles all run against in-memory state only.
 *
 * Anything that needs an owner Node, the renderer, or real assets (draw,
 * buildDrawCommands, GPU upload, ray intersection against meshes) is avoided.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/components/Label.hpp"
#include "lupine/components/Button.hpp"
#include "lupine/components/ColorRect.hpp"
#include "lupine/components/Image2D.hpp"
#include "lupine/components/Panel.hpp"
#include "lupine/components/NineSlicePanel.hpp"
#include "lupine/components/ProgressBar.hpp"
#include "lupine/components/TextureButton.hpp"
#include "lupine/components/ToggleButton.hpp"
#include "lupine/components/TextureToggleButton.hpp"
#include "lupine/components/StyleBox.hpp"
#include "lupine/components/UIImageDraw.hpp"
#include "lupine/core/LinkedProperty.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <cmath>
#include <string>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

bool FloatEq(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) < tol;
}

bool ColorEq(const math::Color& c, float r, float g, float b, float a, float tol = 1e-4f) {
    return FloatEq(c.r, r, tol) && FloatEq(c.g, g, tol) &&
           FloatEq(c.b, b, tol) && FloatEq(c.a, a, tol);
}

/**
 * Construct a UI component and populate its property registry, mirroring what the
 * engine does the first time properties are touched. Without this the
 * property-backed setters are no-ops and getters return default-constructed values.
 */
template <typename T>
std::shared_ptr<T> MakeUI() {
    std::shared_ptr<T> comp = std::make_shared<T>();
    comp->DefineProperties();
    return comp;
}

bool TestLabel() {
    TEST_SECTION("Label: Text / Font / Alignment");

    std::shared_ptr<Label> label = MakeUI<Label>();

    label->SetText("Hello, Lupine");
    TEST_ASSERT(label->GetText() == "Hello, Lupine", "Label text round-trips");

    label->SetFontPath("res://fonts/default.ttf");
    TEST_ASSERT(label->GetFontPath() == "res://fonts/default.ttf", "Label font path round-trips");

    label->SetFontSize(24.0f);
    TEST_ASSERT(FloatEq(label->GetFontSize(), 24.0f), "Label font size round-trips");

    label->SetColor(math::Color(0.2f, 0.4f, 0.6f, 0.8f));
    TEST_ASSERT(ColorEq(label->GetColor(), 0.2f, 0.4f, 0.6f, 0.8f), "Label color round-trips");

    label->SetCentered(true);
    TEST_ASSERT(label->GetCentered(), "Label centered flag set");
    label->SetCentered(false);
    TEST_ASSERT(!label->GetCentered(), "Label centered flag cleared");

    label->SetOffset(math::Vec2(12.0f, -5.0f));
    TEST_ASSERT(FloatEq(label->GetOffset().x, 12.0f) && FloatEq(label->GetOffset().y, -5.0f),
                "Label offset round-trips");

    label->SetHorizontalAlign(TextHAlign::Right);
    TEST_ASSERT(label->GetHorizontalAlign() == TextHAlign::Right, "Label horizontal align = Right");
    label->SetHorizontalAlign(TextHAlign::Center);
    TEST_ASSERT(label->GetHorizontalAlign() == TextHAlign::Center, "Label horizontal align = Center");
    label->SetHorizontalAlign(TextHAlign::Fill);
    TEST_ASSERT(label->GetHorizontalAlign() == TextHAlign::Fill, "Label horizontal align = Fill");

    label->SetVerticalAlign(TextVAlign::Bottom);
    TEST_ASSERT(label->GetVerticalAlign() == TextVAlign::Bottom, "Label vertical align = Bottom");
    label->SetVerticalAlign(TextVAlign::Center);
    TEST_ASSERT(label->GetVerticalAlign() == TextVAlign::Center, "Label vertical align = Center");

    label->SetWordWrap(true);
    TEST_ASSERT(label->GetWordWrap(), "Label word wrap enabled");
    label->SetWordWrap(false);
    TEST_ASSERT(!label->GetWordWrap(), "Label word wrap disabled");

    label->SetMultiline(false);
    TEST_ASSERT(!label->GetMultiline(), "Label multiline disabled");
    label->SetMultiline(true);
    TEST_ASSERT(label->GetMultiline(), "Label multiline enabled");

    label->SetLineSpacing(1.5f);
    TEST_ASSERT(FloatEq(label->GetLineSpacing(), 1.5f), "Label line spacing round-trips");

    label->SetOutlineWidth(2.0f);
    TEST_ASSERT(FloatEq(label->GetOutlineWidth(), 2.0f), "Label outline width round-trips");

    label->SetOutlineColor(math::Color(1.0f, 0.0f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(label->GetOutlineColor(), 1.0f, 0.0f, 0.0f, 1.0f), "Label outline color round-trips");

    TEST_ASSERT(label->GetTypeName() == "Label", "Label type name is Label");

    return true;
}

bool TestButtonStateAndModes() {
    TEST_SECTION("Button: State / Modes / Sizing");

    std::shared_ptr<Button> button = MakeUI<Button>();

    TEST_ASSERT(button->GetTypeName() == "Button", "Button type name is Button");
    TEST_ASSERT(button->GetCurrentState() == ButtonState::Normal, "Button defaults to Normal state");
    TEST_ASSERT(button->IsButtonEnabled(), "Button enabled by default");
    TEST_ASSERT(button->GetStyleMode() == ButtonStyleMode::Automatic, "Button defaults to Automatic style mode");
    TEST_ASSERT(button->GetScaleMode() == ButtonScaleMode::Fixed, "Button defaults to Fixed scale mode");
    TEST_ASSERT(FloatEq(button->GetTextPadding().x, 10.0f) && FloatEq(button->GetTextPadding().y, 5.0f),
                "Button default text padding is (10, 5)");

    button->SetEnabled(false);
    TEST_ASSERT(!button->IsButtonEnabled(), "Button disabled via SetEnabled");
    button->SetEnabled(true);
    TEST_ASSERT(button->IsButtonEnabled(), "Button re-enabled via SetEnabled");

    button->SetStyleMode(ButtonStyleMode::Manual);
    TEST_ASSERT(button->GetStyleMode() == ButtonStyleMode::Manual, "Button style mode = Manual");
    button->SetStyleMode(ButtonStyleMode::Automatic);
    TEST_ASSERT(button->GetStyleMode() == ButtonStyleMode::Automatic, "Button style mode = Automatic");

    button->SetScaleMode(ButtonScaleMode::FitToText);
    TEST_ASSERT(button->GetScaleMode() == ButtonScaleMode::FitToText, "Button scale mode = FitToText");
    button->SetScaleMode(ButtonScaleMode::FitToTextWidth);
    TEST_ASSERT(button->GetScaleMode() == ButtonScaleMode::FitToTextWidth, "Button scale mode = FitToTextWidth");
    button->SetScaleMode(ButtonScaleMode::Fixed);
    TEST_ASSERT(button->GetScaleMode() == ButtonScaleMode::Fixed, "Button scale mode back to Fixed");

    button->SetTextPadding(math::Vec2(20.0f, 8.0f));
    TEST_ASSERT(FloatEq(button->GetTextPadding().x, 20.0f) && FloatEq(button->GetTextPadding().y, 8.0f),
                "Button text padding round-trips");

    button->SetWidth(200.0f);
    TEST_ASSERT(FloatEq(button->GetWidth(), 200.0f), "Button width round-trips");
    button->SetHeight(48.0f);
    TEST_ASSERT(FloatEq(button->GetHeight(), 48.0f), "Button height round-trips");

    button->SetLayer(3);
    TEST_ASSERT(button->GetLayer() == 3, "Button layer round-trips");
    button->SetSortingOrder(-7);
    TEST_ASSERT(button->GetSortingOrder() == -7, "Button sorting order round-trips");

    return true;
}

bool TestButtonTextAndColors() {
    TEST_SECTION("Button: Text / Background / Colors");

    std::shared_ptr<Button> button = MakeUI<Button>();

    button->SetText("Click Me");
    TEST_ASSERT(button->GetText() == "Click Me", "Button text round-trips");

    button->SetFontPath("res://fonts/ui.ttf");
    TEST_ASSERT(button->GetFontPath() == "res://fonts/ui.ttf", "Button font path round-trips");

    button->SetFontSize(18.0f);
    TEST_ASSERT(FloatEq(button->GetFontSize(), 18.0f), "Button font size round-trips");

    button->SetFontColor(math::Color(0.9f, 0.9f, 0.1f, 1.0f));
    TEST_ASSERT(ColorEq(button->GetFontColor(), 0.9f, 0.9f, 0.1f, 1.0f), "Button font color round-trips");

    button->SetWordWrap(true);
    TEST_ASSERT(button->GetWordWrap(), "Button word wrap enabled");
    button->SetWordWrap(false);
    TEST_ASSERT(!button->GetWordWrap(), "Button word wrap disabled");

    button->SetBackgroundColor(math::Color(0.1f, 0.2f, 0.3f, 1.0f));
    TEST_ASSERT(ColorEq(button->GetBackgroundColor(), 0.1f, 0.2f, 0.3f, 1.0f), "Button background color round-trips");

    button->SetOpacity(0.5f);
    TEST_ASSERT(FloatEq(button->GetOpacity(), 0.5f), "Button opacity round-trips");

    button->SetBorderColor(math::Color(0.0f, 0.0f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(button->GetBorderColor(), 0.0f, 0.0f, 0.0f, 1.0f), "Button border color round-trips");

    button->SetBorderEnabled(true);
    TEST_ASSERT(button->GetBorderEnabled(), "Button border enabled");
    button->SetBorderEnabled(false);
    TEST_ASSERT(!button->GetBorderEnabled(), "Button border disabled");

    return true;
}

bool TestButtonBordersAndCorners() {
    TEST_SECTION("Button: Border Widths / Corner Radii");

    std::shared_ptr<Button> button = MakeUI<Button>();

    button->SetBorderWidthLinked(false);
    TEST_ASSERT(!button->GetBorderWidthLinked(), "Button border width unlinked");
    button->SetBorderWidthTop(1.0f);
    button->SetBorderWidthRight(2.0f);
    button->SetBorderWidthBottom(3.0f);
    button->SetBorderWidthLeft(4.0f);
    TEST_ASSERT(FloatEq(button->GetBorderWidthTop(), 1.0f), "Button border width top round-trips");
    TEST_ASSERT(FloatEq(button->GetBorderWidthRight(), 2.0f), "Button border width right round-trips");
    TEST_ASSERT(FloatEq(button->GetBorderWidthBottom(), 3.0f), "Button border width bottom round-trips");
    TEST_ASSERT(FloatEq(button->GetBorderWidthLeft(), 4.0f), "Button border width left round-trips");

    button->SetBorderWidthLinked(true);
    TEST_ASSERT(button->GetBorderWidthLinked(), "Button border width re-linked");

    button->SetCornerRadiusLinked(false);
    TEST_ASSERT(!button->GetCornerRadiusLinked(), "Button corner radius unlinked");
    button->SetCornerRadiusTopLeft(5.0f);
    button->SetCornerRadiusTopRight(6.0f);
    button->SetCornerRadiusBottomLeft(7.0f);
    button->SetCornerRadiusBottomRight(8.0f);
    TEST_ASSERT(FloatEq(button->GetCornerRadiusTopLeft(), 5.0f), "Button corner radius top-left round-trips");
    TEST_ASSERT(FloatEq(button->GetCornerRadiusTopRight(), 6.0f), "Button corner radius top-right round-trips");
    TEST_ASSERT(FloatEq(button->GetCornerRadiusBottomLeft(), 7.0f), "Button corner radius bottom-left round-trips");
    TEST_ASSERT(FloatEq(button->GetCornerRadiusBottomRight(), 8.0f), "Button corner radius bottom-right round-trips");

    button->SetCornerRadiusLinked(true);
    TEST_ASSERT(button->GetCornerRadiusLinked(), "Button corner radius re-linked");

    return true;
}

bool TestButtonPerStateData() {
    TEST_SECTION("Button: Per-State Modulation / Style / Sound / Tween");

    std::shared_ptr<Button> button = MakeUI<Button>();

    button->SetStateModulation(ButtonState::Normal, math::Color(1.0f, 1.0f, 1.0f, 1.0f));
    button->SetStateModulation(ButtonState::Hover, math::Color(1.2f, 1.2f, 1.2f, 1.0f));
    button->SetStateModulation(ButtonState::Pressed, math::Color(0.8f, 0.8f, 0.8f, 1.0f));
    button->SetStateModulation(ButtonState::Disabled, math::Color(0.5f, 0.5f, 0.5f, 0.5f));
    TEST_ASSERT(ColorEq(button->GetStateModulation(ButtonState::Normal), 1.0f, 1.0f, 1.0f, 1.0f),
                "Button Normal modulation round-trips");
    TEST_ASSERT(ColorEq(button->GetStateModulation(ButtonState::Hover), 1.2f, 1.2f, 1.2f, 1.0f),
                "Button Hover modulation round-trips");
    TEST_ASSERT(ColorEq(button->GetStateModulation(ButtonState::Pressed), 0.8f, 0.8f, 0.8f, 1.0f),
                "Button Pressed modulation round-trips");
    TEST_ASSERT(ColorEq(button->GetStateModulation(ButtonState::Disabled), 0.5f, 0.5f, 0.5f, 0.5f),
                "Button Disabled modulation round-trips");

    std::shared_ptr<StyleBoxFlat> hoverStyle = std::make_shared<StyleBoxFlat>();
    hoverStyle->SetBackgroundColor(math::Color(0.3f, 0.3f, 0.3f, 1.0f));
    button->SetStateStyleBox(ButtonState::Hover, hoverStyle);
    TEST_ASSERT(button->GetStateStyleBox(ButtonState::Hover) == hoverStyle,
                "Button Hover style box round-trips");
    TEST_ASSERT(button->GetStateStyleBox(ButtonState::Normal) == nullptr,
                "Button Normal style box default is null");

    const ButtonStateSound& defaultSound = button->GetStateSound(ButtonState::Pressed);
    TEST_ASSERT(defaultSound.busName == "SFX", "Button default per-state sound bus is SFX");
    TEST_ASSERT(FloatEq(defaultSound.volume, 1.0f), "Button default per-state sound volume is 1.0");

    button->SetStateSoundPath(ButtonState::Pressed, "res://sfx/click.wav");
    TEST_ASSERT(button->GetStateSound(ButtonState::Pressed).audioPath == "res://sfx/click.wav",
                "Button per-state sound path round-trips");

    ButtonStateSound custom;
    custom.audioPath = "res://sfx/hover.wav";
    custom.busName = "UI";
    custom.volume = 0.6f;
    button->SetStateSound(ButtonState::Hover, custom);
    TEST_ASSERT(button->GetStateSound(ButtonState::Hover).busName == "UI" &&
                FloatEq(button->GetStateSound(ButtonState::Hover).volume, 0.6f),
                "Button per-state sound struct round-trips");

    ButtonStateTween tween;
    tween.enabled = true;
    tween.scaleOffset = math::Vec2(0.1f, 0.1f);
    tween.rotationOffset = 5.0f;
    tween.positionOffset = math::Vec2(2.0f, -2.0f);
    tween.duration = 0.35f;
    button->SetStateTween(ButtonState::Pressed, tween);
    const ButtonStateTween& gotTween = button->GetStateTween(ButtonState::Pressed);
    TEST_ASSERT(gotTween.enabled, "Button per-state tween enabled round-trips");
    TEST_ASSERT(FloatEq(gotTween.scaleOffset.x, 0.1f) && FloatEq(gotTween.rotationOffset, 5.0f),
                "Button per-state tween offsets round-trip");
    TEST_ASSERT(FloatEq(gotTween.duration, 0.35f), "Button per-state tween duration round-trips");

    int callbackCount = 0;
    button->SetStateCallback(ButtonState::Normal, [&callbackCount]() { ++callbackCount; });
    button->ClearStateCallback(ButtonState::Normal);
    TEST_ASSERT(callbackCount == 0, "Button state callback set/clear leaves state untouched");

    return true;
}

bool TestButtonBackgroundImage() {
    TEST_SECTION("Button: Background Image / Nine-Slice");

    std::shared_ptr<Button> button = MakeUI<Button>();

    button->SetBackgroundImagePath("res://ui/panel.png");
    TEST_ASSERT(button->GetBackgroundImagePath() == "res://ui/panel.png",
                "Button background image path round-trips");

    button->SetBackgroundImageStretchMode(UIImageStretchMode::NineSlice);
    TEST_ASSERT(button->GetBackgroundImageStretchMode() == UIImageStretchMode::NineSlice,
                "Button background image stretch mode = NineSlice");
    button->SetBackgroundImageStretchMode(UIImageStretchMode::KeepCentered);
    TEST_ASSERT(button->GetBackgroundImageStretchMode() == UIImageStretchMode::KeepCentered,
                "Button background image stretch mode = KeepCentered");

    button->SetNineSliceMargins(math::Vec4(4.0f, 8.0f, 12.0f, 16.0f));
    math::Vec4 margins = button->GetNineSliceMargins();
    TEST_ASSERT(FloatEq(margins.x, 4.0f) && FloatEq(margins.y, 8.0f) &&
                FloatEq(margins.z, 12.0f) && FloatEq(margins.w, 16.0f),
                "Button nine-slice margins round-trip");

    button->SetNineSliceAxisHorizontal(UINineSliceAxisMode::Tile);
    TEST_ASSERT(button->GetNineSliceAxisHorizontal() == UINineSliceAxisMode::Tile,
                "Button nine-slice horizontal axis = Tile");
    button->SetNineSliceAxisVertical(UINineSliceAxisMode::Stretch);
    TEST_ASSERT(button->GetNineSliceAxisVertical() == UINineSliceAxisMode::Stretch,
                "Button nine-slice vertical axis = Stretch");

    button->SetNineSliceDrawCenter(false);
    TEST_ASSERT(!button->GetNineSliceDrawCenter(), "Button nine-slice draw center disabled");
    button->SetNineSliceDrawCenter(true);
    TEST_ASSERT(button->GetNineSliceDrawCenter(), "Button nine-slice draw center enabled");

    return true;
}

bool TestColorRect() {
    TEST_SECTION("ColorRect: Color / Border / Corner / Blend");

    std::shared_ptr<ColorRect> rect = MakeUI<ColorRect>();

    TEST_ASSERT(rect->GetTypeName() == "ColorRect", "ColorRect type name is ColorRect");

    rect->SetColor(math::Color(0.25f, 0.5f, 0.75f, 1.0f));
    TEST_ASSERT(ColorEq(rect->GetColor(), 0.25f, 0.5f, 0.75f, 1.0f), "ColorRect color round-trips");

    rect->SetLayer(5);
    TEST_ASSERT(rect->GetLayer() == 5, "ColorRect layer round-trips");
    rect->SetSortingOrder(42);
    TEST_ASSERT(rect->GetSortingOrder() == 42, "ColorRect sorting order round-trips");

    rect->SetCornerRadiusAll(10.0f);
    TEST_ASSERT(FloatEq(rect->GetCornerRadius().Get(0), 10.0f) &&
                FloatEq(rect->GetCornerRadius().Get(3), 10.0f),
                "ColorRect corner radius all round-trips");

    rect->SetCornerRadiusLinked(false);
    TEST_ASSERT(!rect->IsCornerRadiusLinked(), "ColorRect corner radius unlinked");
    rect->SetCornerRadiusIndividual(2, 3.0f);
    TEST_ASSERT(FloatEq(rect->GetCornerRadius().Get(2), 3.0f),
                "ColorRect individual corner radius round-trips");

    rect->SetBorderEnabled(true);
    TEST_ASSERT(rect->GetBorderEnabled(), "ColorRect border enabled");

    rect->SetBorderColor(math::Color(0.0f, 1.0f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(rect->GetBorderColor(), 0.0f, 1.0f, 0.0f, 1.0f), "ColorRect border color round-trips");

    rect->SetBorderWidthAll(2.0f);
    TEST_ASSERT(FloatEq(rect->GetBorderWidth().Get(0), 2.0f), "ColorRect border width all round-trips");
    rect->SetBorderWidthLinked(false);
    TEST_ASSERT(!rect->IsBorderWidthLinked(), "ColorRect border width unlinked");
    rect->SetBorderWidthIndividual(1, 5.0f);
    TEST_ASSERT(FloatEq(rect->GetBorderWidth().Get(1), 5.0f), "ColorRect individual border width round-trips");

    rect->SetBlendMode(BlendMode::Additive);
    TEST_ASSERT(rect->GetBlendMode() == BlendMode::Additive, "ColorRect blend mode = Additive");
    rect->SetBlendMode(BlendMode::Multiply);
    TEST_ASSERT(rect->GetBlendMode() == BlendMode::Multiply, "ColorRect blend mode = Multiply");

    TEST_ASSERT(rect->IntToBlendMode(rect->BlendModeToInt(BlendMode::Overlay)) == BlendMode::Overlay,
                "ColorRect blend mode int round-trips");

    rect->SetMaterialOverride("res://mat/custom.mat");
    TEST_ASSERT(rect->GetMaterialOverride() == "res://mat/custom.mat", "ColorRect material override round-trips");

    rect->SetShader("res://shaders/glow.lsh");
    TEST_ASSERT(rect->GetShader() == "res://shaders/glow.lsh", "ColorRect shader path round-trips");

    rect->SetShaderParameters("{\"u_Intensity\":{\"type\":\"float\",\"value\":1.0}}");
    TEST_ASSERT(rect->GetShaderParameters().find("u_Intensity") != std::string::npos,
                "ColorRect shader parameters round-trip");

    return true;
}

bool TestImage2D() {
    TEST_SECTION("Image2D: Texture / Aspect / Flip / Blend / Mouse");

    std::shared_ptr<Image2D> image = MakeUI<Image2D>();

    TEST_ASSERT(image->GetTypeName() == "Image2D", "Image2D type name is Image2D");

    image->SetTexturePath("res://textures/icon.png");
    TEST_ASSERT(image->GetTexturePath() == "res://textures/icon.png", "Image2D texture path round-trips");

    image->SetColor(math::Color(1.0f, 0.5f, 0.5f, 0.9f));
    TEST_ASSERT(ColorEq(image->GetColor(), 1.0f, 0.5f, 0.5f, 0.9f), "Image2D color round-trips");

    image->SetPreserveAspect(true);
    TEST_ASSERT(image->GetPreserveAspect(), "Image2D preserve aspect enabled");
    image->SetPreserveAspect(false);
    TEST_ASSERT(!image->GetPreserveAspect(), "Image2D preserve aspect disabled");

    image->SetAspectMode(AspectMode::Fill);
    TEST_ASSERT(image->GetAspectMode() == AspectMode::Fill, "Image2D aspect mode = Fill");
    image->SetAspectMode(AspectMode::Stretch);
    TEST_ASSERT(image->GetAspectMode() == AspectMode::Stretch, "Image2D aspect mode = Stretch");
    image->SetAspectMode(AspectMode::Fit);
    TEST_ASSERT(image->GetAspectMode() == AspectMode::Fit, "Image2D aspect mode = Fit");

    image->SetFlipH(true);
    TEST_ASSERT(image->GetFlipH(), "Image2D flip horizontal enabled");
    image->SetFlipV(true);
    TEST_ASSERT(image->GetFlipV(), "Image2D flip vertical enabled");
    image->SetFlipH(false);
    image->SetFlipV(false);
    TEST_ASSERT(!image->GetFlipH() && !image->GetFlipV(), "Image2D flip flags cleared");

    image->SetBlendMode(BlendMode::Additive);
    TEST_ASSERT(image->GetBlendMode() == BlendMode::Additive, "Image2D blend mode = Additive");

    image->SetMouseBehaviour(MouseBehaviour::Stop);
    TEST_ASSERT(image->GetMouseBehaviour() == MouseBehaviour::Stop, "Image2D mouse behaviour = Stop");
    image->SetMouseBehaviour(MouseBehaviour::Ignore);
    TEST_ASSERT(image->GetMouseBehaviour() == MouseBehaviour::Ignore, "Image2D mouse behaviour = Ignore");

    image->SetCornerRadiusAll(6.0f);
    TEST_ASSERT(FloatEq(image->GetCornerRadius().Get(0), 6.0f), "Image2D corner radius all round-trips");
    image->SetCornerRadiusLinked(false);
    TEST_ASSERT(!image->IsCornerRadiusLinked(), "Image2D corner radius unlinked");

    image->SetBorderEnabled(true);
    TEST_ASSERT(image->GetBorderEnabled(), "Image2D border enabled");
    image->SetBorderColor(math::Color(0.0f, 0.0f, 1.0f, 1.0f));
    TEST_ASSERT(ColorEq(image->GetBorderColor(), 0.0f, 0.0f, 1.0f, 1.0f), "Image2D border color round-trips");
    image->SetBorderWidthAll(3.0f);
    TEST_ASSERT(FloatEq(image->GetBorderWidth().Get(0), 3.0f), "Image2D border width all round-trips");

    image->SetMaterialOverride("res://mat/img.mat");
    TEST_ASSERT(image->GetMaterialOverride() == "res://mat/img.mat", "Image2D material override round-trips");

    image->SetShader("res://shaders/img.lsh");
    TEST_ASSERT(image->GetShader() == "res://shaders/img.lsh", "Image2D shader path round-trips");

    return true;
}

bool TestPanel() {
    TEST_SECTION("Panel: Background / Border / Corner / Shadow");

    std::shared_ptr<Panel> panel = MakeUI<Panel>();

    TEST_ASSERT(panel->GetTypeName() == "Panel", "Panel type name is Panel");
    TEST_ASSERT(panel->GetStyleBox() != nullptr, "Panel constructs a default StyleBox");

    panel->SetLayer(2);
    TEST_ASSERT(panel->GetLayer() == 2, "Panel layer round-trips");
    panel->SetSortingOrder(11);
    TEST_ASSERT(panel->GetSortingOrder() == 11, "Panel sorting order round-trips");

    panel->SetBackgroundColor(math::Color(0.4f, 0.4f, 0.45f, 1.0f));
    TEST_ASSERT(ColorEq(panel->GetBackgroundColor(), 0.4f, 0.4f, 0.45f, 1.0f), "Panel background color round-trips");

    panel->SetOpacity(0.75f);
    TEST_ASSERT(FloatEq(panel->GetOpacity(), 0.75f), "Panel opacity round-trips");

    panel->SetBorderEnabled(true);
    TEST_ASSERT(panel->GetBorderEnabled(), "Panel border enabled");
    panel->SetBorderColor(math::Color(0.1f, 0.1f, 0.1f, 1.0f));
    TEST_ASSERT(ColorEq(panel->GetBorderColor(), 0.1f, 0.1f, 0.1f, 1.0f), "Panel border color round-trips");

    panel->SetBorderWidthLinked(false);
    TEST_ASSERT(!panel->GetBorderWidthLinked(), "Panel border width unlinked");
    panel->SetBorderWidthLeft(1.0f);
    panel->SetBorderWidthRight(2.0f);
    panel->SetBorderWidthTop(3.0f);
    panel->SetBorderWidthBottom(4.0f);
    TEST_ASSERT(FloatEq(panel->GetBorderWidthLeft(), 1.0f) && FloatEq(panel->GetBorderWidthRight(), 2.0f) &&
                FloatEq(panel->GetBorderWidthTop(), 3.0f) && FloatEq(panel->GetBorderWidthBottom(), 4.0f),
                "Panel per-edge border widths round-trip");

    panel->SetCornerRadiusLinked(false);
    TEST_ASSERT(!panel->GetCornerRadiusLinked(), "Panel corner radius unlinked");
    panel->SetCornerRadiusTopLeft(5.0f);
    panel->SetCornerRadiusTopRight(6.0f);
    panel->SetCornerRadiusBottomLeft(7.0f);
    panel->SetCornerRadiusBottomRight(8.0f);
    TEST_ASSERT(FloatEq(panel->GetCornerRadiusTopLeft(), 5.0f) && FloatEq(panel->GetCornerRadiusTopRight(), 6.0f) &&
                FloatEq(panel->GetCornerRadiusBottomLeft(), 7.0f) && FloatEq(panel->GetCornerRadiusBottomRight(), 8.0f),
                "Panel per-corner radii round-trip");

    panel->SetCornerDetail(16);
    TEST_ASSERT(panel->GetCornerDetail() == 16, "Panel corner detail round-trips");

    panel->SetAntiAliasing(false);
    TEST_ASSERT(!panel->GetAntiAliasing(), "Panel anti-aliasing disabled");
    panel->SetAntiAliasingSize(2.0f);
    TEST_ASSERT(FloatEq(panel->GetAntiAliasingSize(), 2.0f), "Panel anti-aliasing size round-trips");

    panel->SetShadowEnabled(true);
    TEST_ASSERT(panel->GetShadowEnabled(), "Panel shadow enabled");
    panel->SetShadowColor(math::Color(0.0f, 0.0f, 0.0f, 0.4f));
    TEST_ASSERT(ColorEq(panel->GetShadowColor(), 0.0f, 0.0f, 0.0f, 0.4f), "Panel shadow color round-trips");
    panel->SetShadowSize(6.0f);
    TEST_ASSERT(FloatEq(panel->GetShadowSize(), 6.0f), "Panel shadow size round-trips");
    panel->SetShadowOffset(math::Vec2(3.0f, 3.0f));
    TEST_ASSERT(FloatEq(panel->GetShadowOffset().x, 3.0f) && FloatEq(panel->GetShadowOffset().y, 3.0f),
                "Panel shadow offset round-trips");

    panel->SetStylePath("res://styles/panel.style");
    TEST_ASSERT(panel->GetStylePath() == "res://styles/panel.style", "Panel style path round-trips");

    panel->SetBackgroundImagePath("res://ui/bg.png");
    TEST_ASSERT(panel->GetBackgroundImagePath() == "res://ui/bg.png", "Panel background image path round-trips");
    panel->SetBackgroundImageStretchMode(UIImageStretchMode::NineSlice);
    TEST_ASSERT(panel->GetBackgroundImageStretchMode() == UIImageStretchMode::NineSlice,
                "Panel background image stretch mode = NineSlice");
    panel->SetNineSliceMargins(math::Vec4(2.0f, 4.0f, 6.0f, 8.0f));
    TEST_ASSERT(FloatEq(panel->GetNineSliceMargins().x, 2.0f) && FloatEq(panel->GetNineSliceMargins().w, 8.0f),
                "Panel nine-slice margins round-trip");
    panel->SetNineSliceDrawCenter(false);
    TEST_ASSERT(!panel->GetNineSliceDrawCenter(), "Panel nine-slice draw center disabled");

    panel->SetShader("res://shaders/panel.lsh");
    TEST_ASSERT(panel->GetShader() == "res://shaders/panel.lsh", "Panel shader path round-trips");

    return true;
}

bool TestNineSlicePanel() {
    TEST_SECTION("NineSlicePanel: Texture / Margins / Modulate");

    std::shared_ptr<NineSlicePanel> panel = MakeUI<NineSlicePanel>();

    TEST_ASSERT(panel->GetTypeName() == "NineSlicePanel", "NineSlicePanel type name is NineSlicePanel");

    panel->SetTexturePath("res://ui/frame.png");
    TEST_ASSERT(panel->GetTexturePath() == "res://ui/frame.png", "NineSlicePanel texture path round-trips");

    panel->SetModulate(math::Color(0.8f, 0.9f, 1.0f, 1.0f));
    TEST_ASSERT(ColorEq(panel->GetModulate(), 0.8f, 0.9f, 1.0f, 1.0f), "NineSlicePanel modulate round-trips");

    panel->SetMarginLeft(10.0f);
    panel->SetMarginRight(12.0f);
    panel->SetMarginTop(14.0f);
    panel->SetMarginBottom(16.0f);
    TEST_ASSERT(FloatEq(panel->GetMarginLeft(), 10.0f), "NineSlicePanel margin left round-trips");
    TEST_ASSERT(FloatEq(panel->GetMarginRight(), 12.0f), "NineSlicePanel margin right round-trips");
    TEST_ASSERT(FloatEq(panel->GetMarginTop(), 14.0f), "NineSlicePanel margin top round-trips");
    TEST_ASSERT(FloatEq(panel->GetMarginBottom(), 16.0f), "NineSlicePanel margin bottom round-trips");

    panel->SetNineSliceAxisHorizontal(UINineSliceAxisMode::Tile);
    TEST_ASSERT(panel->GetNineSliceAxisHorizontal() == UINineSliceAxisMode::Tile,
                "NineSlicePanel horizontal axis = Tile");
    panel->SetNineSliceAxisVertical(UINineSliceAxisMode::Tile);
    TEST_ASSERT(panel->GetNineSliceAxisVertical() == UINineSliceAxisMode::Tile,
                "NineSlicePanel vertical axis = Tile");

    panel->SetNineSliceDrawCenter(false);
    TEST_ASSERT(!panel->GetNineSliceDrawCenter(), "NineSlicePanel draw center disabled");
    panel->SetNineSliceDrawCenter(true);
    TEST_ASSERT(panel->GetNineSliceDrawCenter(), "NineSlicePanel draw center enabled");

    panel->SetLayer(1);
    TEST_ASSERT(panel->GetLayer() == 1, "NineSlicePanel layer round-trips");
    panel->SetSortingOrder(9);
    TEST_ASSERT(panel->GetSortingOrder() == 9, "NineSlicePanel sorting order round-trips");

    return true;
}

bool TestProgressBar() {
    TEST_SECTION("ProgressBar: Value Clamping / Orientation / Colors");

    std::shared_ptr<ProgressBar> bar = MakeUI<ProgressBar>();

    TEST_ASSERT(bar->GetTypeName() == "ProgressBar", "ProgressBar type name is ProgressBar");

    bar->SetStep(0.0f);
    bar->SetMinValue(0.0f);
    bar->SetMaxValue(100.0f);
    TEST_ASSERT(FloatEq(bar->GetMinValue(), 0.0f), "ProgressBar min value round-trips");
    TEST_ASSERT(FloatEq(bar->GetMaxValue(), 100.0f), "ProgressBar max value round-trips");

    bar->SetValue(50.0f);
    TEST_ASSERT(FloatEq(bar->GetValue(), 50.0f), "ProgressBar value in range round-trips");

    bar->SetValue(150.0f);
    TEST_ASSERT(FloatEq(bar->GetValue(), 100.0f), "ProgressBar value clamps to max");

    bar->SetValue(-25.0f);
    TEST_ASSERT(FloatEq(bar->GetValue(), 0.0f), "ProgressBar value clamps to min");

    bar->SetStep(10.0f);
    TEST_ASSERT(FloatEq(bar->GetStep(), 10.0f), "ProgressBar step round-trips");
    bar->SetValue(53.0f);
    TEST_ASSERT(FloatEq(bar->GetValue(), 50.0f), "ProgressBar value snaps to step");
    bar->SetStep(0.0f);

    bar->SetSmooth(true);
    TEST_ASSERT(bar->GetSmooth(), "ProgressBar smooth enabled");
    bar->SetSmoothSpeed(8.0f);
    TEST_ASSERT(FloatEq(bar->GetSmoothSpeed(), 8.0f), "ProgressBar smooth speed round-trips");

    bar->SetOrientation(1);
    TEST_ASSERT(bar->GetOrientation() == 1, "ProgressBar orientation = Vertical");
    bar->SetOrientation(0);
    TEST_ASSERT(bar->GetOrientation() == 0, "ProgressBar orientation = Horizontal");

    bar->SetFillDirection(3);
    TEST_ASSERT(bar->GetFillDirection() == 3, "ProgressBar fill direction = DownToUp");
    bar->SetFillDirection(0);
    TEST_ASSERT(bar->GetFillDirection() == 0, "ProgressBar fill direction = LeftToRight");

    bar->SetBackgroundTexturePath("res://ui/bar_bg.png");
    TEST_ASSERT(bar->GetBackgroundTexturePath() == "res://ui/bar_bg.png", "ProgressBar background texture path round-trips");
    bar->SetBackgroundColor(math::Color(0.2f, 0.2f, 0.2f, 1.0f));
    TEST_ASSERT(ColorEq(bar->GetBackgroundColor(), 0.2f, 0.2f, 0.2f, 1.0f), "ProgressBar background color round-trips");

    bar->SetFillTexturePath("res://ui/bar_fill.png");
    TEST_ASSERT(bar->GetFillTexturePath() == "res://ui/bar_fill.png", "ProgressBar fill texture path round-trips");
    bar->SetFillColor(math::Color(0.0f, 0.8f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(bar->GetFillColor(), 0.0f, 0.8f, 0.0f, 1.0f), "ProgressBar fill color round-trips");

    bar->SetBorderTexturePath("res://ui/bar_border.png");
    TEST_ASSERT(bar->GetBorderTexturePath() == "res://ui/bar_border.png", "ProgressBar border texture path round-trips");
    bar->SetBorderColor(math::Color(1.0f, 1.0f, 1.0f, 1.0f));
    TEST_ASSERT(ColorEq(bar->GetBorderColor(), 1.0f, 1.0f, 1.0f, 1.0f), "ProgressBar border color round-trips");

    bar->SetShowValue(true);
    TEST_ASSERT(bar->GetShowValue(), "ProgressBar show value enabled");
    bar->SetValueFontPath("res://fonts/value.ttf");
    TEST_ASSERT(bar->GetValueFontPath() == "res://fonts/value.ttf", "ProgressBar value font path round-trips");
    bar->SetValueFontSize(14.0f);
    TEST_ASSERT(FloatEq(bar->GetValueFontSize(), 14.0f), "ProgressBar value font size round-trips");
    bar->SetValueColor(math::Color(1.0f, 1.0f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(bar->GetValueColor(), 1.0f, 1.0f, 0.0f, 1.0f), "ProgressBar value color round-trips");

    bar->SetCornerRadiusAll(4.0f);
    TEST_ASSERT(FloatEq(bar->GetCornerRadius().Get(0), 4.0f), "ProgressBar corner radius all round-trips");
    bar->SetBorderWidthAll(1.0f);
    TEST_ASSERT(FloatEq(bar->GetBorderWidth().Get(0), 1.0f), "ProgressBar border width all round-trips");

    return true;
}

bool TestTextureButton() {
    TEST_SECTION("TextureButton: State / Toggle / Stretch / Mask / Text");

    std::shared_ptr<TextureButton> button = MakeUI<TextureButton>();

    TEST_ASSERT(button->GetTypeName() == "TextureButton", "TextureButton type name is TextureButton");
    TEST_ASSERT(button->GetCurrentState() == TextureButtonState::Normal, "TextureButton defaults to Normal state");
    TEST_ASSERT(button->IsButtonEnabled(), "TextureButton enabled by default");
    TEST_ASSERT(button->GetStyleMode() == TextureButtonStyleMode::Automatic, "TextureButton defaults to Automatic mode");

    button->SetEnabled(false);
    TEST_ASSERT(!button->IsButtonEnabled(), "TextureButton disabled via SetEnabled");
    button->SetEnabled(true);

    button->SetStyleMode(TextureButtonStyleMode::Manual);
    TEST_ASSERT(button->GetStyleMode() == TextureButtonStyleMode::Manual, "TextureButton style mode = Manual");

    button->SetStretchMode(TextureButtonStretchMode::NineSlice);
    TEST_ASSERT(button->GetStretchMode() == TextureButtonStretchMode::NineSlice, "TextureButton stretch mode = NineSlice");
    button->SetStretchMode(TextureButtonStretchMode::KeepCentered);
    TEST_ASSERT(button->GetStretchMode() == TextureButtonStretchMode::KeepCentered, "TextureButton stretch mode = KeepCentered");

    button->SetUseStretch(false);
    TEST_ASSERT(!button->GetUseStretch(), "TextureButton use stretch disabled");
    button->SetUseStretch(true);
    TEST_ASSERT(button->GetUseStretch(), "TextureButton use stretch enabled");

    button->SetIsToggle(true);
    TEST_ASSERT(button->GetIsToggle(), "TextureButton toggle mode enabled");
    button->SetIsChecked(true);
    TEST_ASSERT(button->GetIsChecked(), "TextureButton checked state set");
    button->SetIsChecked(false);
    TEST_ASSERT(!button->GetIsChecked(), "TextureButton checked state cleared");

    button->SetClickMaskMode(ClickMaskMode::AlphaThreshold);
    TEST_ASSERT(button->GetClickMaskMode() == ClickMaskMode::AlphaThreshold, "TextureButton click mask = AlphaThreshold");
    button->SetAlphaThreshold(0.25f);
    TEST_ASSERT(FloatEq(button->GetAlphaThreshold(), 0.25f), "TextureButton alpha threshold round-trips");

    button->SetMouseButton(MouseButtonType::Right);
    TEST_ASSERT(button->GetMouseButton() == MouseButtonType::Right, "TextureButton mouse button = Right");
    button->SetMouseButton(MouseButtonType::Left);
    TEST_ASSERT(button->GetMouseButton() == MouseButtonType::Left, "TextureButton mouse button = Left");

    button->SetTexturePath("res://ui/btn.png");
    TEST_ASSERT(button->GetTexturePath() == "res://ui/btn.png", "TextureButton automatic texture path round-trips");

    button->SetStateTexturePath(TextureButtonState::Hover, "res://ui/btn_hover.png");
    TEST_ASSERT(button->GetStateTexturePath(TextureButtonState::Hover) == "res://ui/btn_hover.png",
                "TextureButton per-state texture path round-trips");

    button->SetStateModulation(TextureButtonState::Pressed, math::Color(0.7f, 0.7f, 0.7f, 1.0f));
    TEST_ASSERT(ColorEq(button->GetStateModulation(TextureButtonState::Pressed), 0.7f, 0.7f, 0.7f, 1.0f),
                "TextureButton per-state modulation round-trips");

    button->SetStateSoundPath(TextureButtonState::Pressed, "res://sfx/tbtn.wav");
    TEST_ASSERT(button->GetStateSound(TextureButtonState::Pressed).audioPath == "res://sfx/tbtn.wav",
                "TextureButton per-state sound path round-trips");

    TextureButtonStateTween tween;
    tween.enabled = true;
    tween.scaleOffset = math::Vec2(0.05f, 0.05f);
    tween.duration = 0.15f;
    button->SetStateTween(TextureButtonState::Hover, tween);
    TEST_ASSERT(button->GetStateTween(TextureButtonState::Hover).enabled &&
                FloatEq(button->GetStateTween(TextureButtonState::Hover).duration, 0.15f),
                "TextureButton per-state tween round-trips");

    button->SetText("Play");
    TEST_ASSERT(button->GetText() == "Play", "TextureButton text round-trips");
    button->SetFontPath("res://fonts/btn.ttf");
    TEST_ASSERT(button->GetFontPath() == "res://fonts/btn.ttf", "TextureButton font path round-trips");
    button->SetFontSize(20.0f);
    TEST_ASSERT(FloatEq(button->GetFontSize(), 20.0f), "TextureButton font size round-trips");
    button->SetFontColor(math::Color(0.95f, 0.95f, 0.95f, 1.0f));
    TEST_ASSERT(ColorEq(button->GetFontColor(), 0.95f, 0.95f, 0.95f, 1.0f), "TextureButton font color round-trips");

    bool toggled = false;
    button->SetOnToggledCallback([&toggled](bool checked) { toggled = checked; });
    button->SetIsChecked(true);
    TEST_ASSERT(toggled, "TextureButton on-toggled callback fires on check");

    return true;
}

bool TestToggleButton() {
    TEST_SECTION("ToggleButton: Toggle State / Styles / Text");

    std::shared_ptr<ToggleButton> button = MakeUI<ToggleButton>();

    TEST_ASSERT(button->GetTypeName() == "ToggleButton", "ToggleButton type name is ToggleButton");
    TEST_ASSERT(button->IsButtonEnabled(), "ToggleButton enabled by default");
    TEST_ASSERT(!button->IsToggled(), "ToggleButton defaults to untoggled");

    button->SetEnabled(false);
    TEST_ASSERT(!button->IsButtonEnabled(), "ToggleButton disabled via SetEnabled");
    button->SetEnabled(true);

    button->SetStyleMode(ToggleButtonStyleMode::Manual);
    TEST_ASSERT(button->GetStyleMode() == ToggleButtonStyleMode::Manual, "ToggleButton style mode = Manual");
    button->SetStyleMode(ToggleButtonStyleMode::Automatic);
    TEST_ASSERT(button->GetStyleMode() == ToggleButtonStyleMode::Automatic, "ToggleButton style mode = Automatic");

    bool toggledCallbackValue = false;
    button->SetOnToggledCallback([&toggledCallbackValue](bool v) { toggledCallbackValue = v; });
    button->SetToggled(true);
    TEST_ASSERT(button->IsToggled(), "ToggleButton toggled on");
    button->SetToggled(false);
    TEST_ASSERT(!button->IsToggled(), "ToggleButton toggled off");

    button->SetStateModulation(ToggleButtonState::Toggled, math::Color(0.2f, 0.9f, 0.2f, 1.0f));
    TEST_ASSERT(ColorEq(button->GetStateModulation(ToggleButtonState::Toggled), 0.2f, 0.9f, 0.2f, 1.0f),
                "ToggleButton Toggled-state modulation round-trips");
    button->SetStateModulation(ToggleButtonState::ToggledHover, math::Color(0.3f, 1.0f, 0.3f, 1.0f));
    TEST_ASSERT(ColorEq(button->GetStateModulation(ToggleButtonState::ToggledHover), 0.3f, 1.0f, 0.3f, 1.0f),
                "ToggleButton ToggledHover-state modulation round-trips");

    ToggleButtonStateStyle style;
    style.modulationColor = math::Color(0.5f, 0.5f, 1.0f, 1.0f);
    std::shared_ptr<StyleBoxFlat> sbox = std::make_shared<StyleBoxFlat>();
    style.styleBox = sbox;
    button->SetStateStyle(ToggleButtonState::Pressed, style);
    TEST_ASSERT(button->GetStateStyle(ToggleButtonState::Pressed).styleBox == sbox,
                "ToggleButton per-state style box round-trips");

    button->SetStateSoundPath(ToggleButtonState::Toggled, "res://sfx/toggle.wav");
    TEST_ASSERT(button->GetStateSound(ToggleButtonState::Toggled).audioPath == "res://sfx/toggle.wav",
                "ToggleButton per-state sound path round-trips");

    ToggleButtonStateTween tween;
    tween.enabled = true;
    tween.rotationOffset = 3.0f;
    button->SetStateTween(ToggleButtonState::Hover, tween);
    TEST_ASSERT(button->GetStateTween(ToggleButtonState::Hover).enabled &&
                FloatEq(button->GetStateTween(ToggleButtonState::Hover).rotationOffset, 3.0f),
                "ToggleButton per-state tween round-trips");

    button->SetText("On / Off");
    TEST_ASSERT(button->GetText() == "On / Off", "ToggleButton text round-trips");
    button->SetFontPath("res://fonts/toggle.ttf");
    TEST_ASSERT(button->GetFontPath() == "res://fonts/toggle.ttf", "ToggleButton font path round-trips");
    button->SetFontSize(16.0f);
    TEST_ASSERT(FloatEq(button->GetFontSize(), 16.0f), "ToggleButton font size round-trips");
    button->SetFontColor(math::Color(0.1f, 0.1f, 0.1f, 1.0f));
    TEST_ASSERT(ColorEq(button->GetFontColor(), 0.1f, 0.1f, 0.1f, 1.0f), "ToggleButton font color round-trips");

    button->SetBackgroundColor(math::Color(0.6f, 0.6f, 0.6f, 1.0f));
    TEST_ASSERT(ColorEq(button->GetBackgroundColor(), 0.6f, 0.6f, 0.6f, 1.0f), "ToggleButton background color round-trips");
    button->SetOpacity(0.9f);
    TEST_ASSERT(FloatEq(button->GetOpacity(), 0.9f), "ToggleButton opacity round-trips");
    button->SetBorderEnabled(true);
    TEST_ASSERT(button->GetBorderEnabled(), "ToggleButton border enabled");
    button->SetBorderColor(math::Color(0.0f, 0.0f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(button->GetBorderColor(), 0.0f, 0.0f, 0.0f, 1.0f), "ToggleButton border color round-trips");

    button->SetLayer(4);
    TEST_ASSERT(button->GetLayer() == 4, "ToggleButton layer round-trips");
    button->SetSortingOrder(2);
    TEST_ASSERT(button->GetSortingOrder() == 2, "ToggleButton sorting order round-trips");

    return true;
}

bool TestTextureToggleButton() {
    TEST_SECTION("TextureToggleButton: Toggle / Textures / Nine-Slice");

    std::shared_ptr<TextureToggleButton> button = MakeUI<TextureToggleButton>();

    TEST_ASSERT(button->GetTypeName() == "TextureToggleButton", "TextureToggleButton type name is TextureToggleButton");
    TEST_ASSERT(button->IsButtonEnabled(), "TextureToggleButton enabled by default");
    TEST_ASSERT(!button->IsToggled(), "TextureToggleButton defaults to untoggled");

    button->SetEnabled(false);
    TEST_ASSERT(!button->IsButtonEnabled(), "TextureToggleButton disabled via SetEnabled");
    button->SetEnabled(true);

    button->SetToggled(true);
    TEST_ASSERT(button->IsToggled(), "TextureToggleButton toggled on");
    button->SetToggled(false);
    TEST_ASSERT(!button->IsToggled(), "TextureToggleButton toggled off");

    button->SetStateTexturePath(TextureToggleButtonState::Normal, "res://ui/tog_normal.png");
    TEST_ASSERT(button->GetStateTexture(TextureToggleButtonState::Normal).texturePath == "res://ui/tog_normal.png",
                "TextureToggleButton per-state texture path round-trips");

    TextureToggleButtonStateTexture tex;
    tex.texturePath = "res://ui/tog_toggled.png";
    tex.modulationColor = math::Color(0.9f, 0.9f, 0.9f, 1.0f);
    button->SetStateTexture(TextureToggleButtonState::Toggled, tex);
    TEST_ASSERT(button->GetStateTexture(TextureToggleButtonState::Toggled).texturePath == "res://ui/tog_toggled.png",
                "TextureToggleButton per-state texture struct path round-trips");

    button->SetStateModulation(TextureToggleButtonState::Hover, math::Color(1.1f, 1.1f, 1.1f, 1.0f));
    TEST_ASSERT(ColorEq(button->GetStateModulation(TextureToggleButtonState::Hover), 1.1f, 1.1f, 1.1f, 1.0f),
                "TextureToggleButton per-state modulation round-trips");

    button->SetStateSoundPath(TextureToggleButtonState::Toggled, "res://sfx/tog.wav");
    TEST_ASSERT(button->GetStateSound(TextureToggleButtonState::Toggled).audioPath == "res://sfx/tog.wav",
                "TextureToggleButton per-state sound path round-trips");

    TextureToggleButtonStateTween tween;
    tween.enabled = true;
    tween.positionOffset = math::Vec2(1.0f, 1.0f);
    tween.duration = 0.25f;
    button->SetStateTween(TextureToggleButtonState::Pressed, tween);
    TEST_ASSERT(button->GetStateTween(TextureToggleButtonState::Pressed).enabled &&
                FloatEq(button->GetStateTween(TextureToggleButtonState::Pressed).duration, 0.25f),
                "TextureToggleButton per-state tween round-trips");

    button->SetUseNineSlice(true);
    TEST_ASSERT(button->GetUseNineSlice(), "TextureToggleButton nine-slice enabled");
    button->SetMarginLeft(5.0f);
    button->SetMarginRight(6.0f);
    button->SetMarginTop(7.0f);
    button->SetMarginBottom(8.0f);
    TEST_ASSERT(FloatEq(button->GetMarginLeft(), 5.0f) && FloatEq(button->GetMarginRight(), 6.0f) &&
                FloatEq(button->GetMarginTop(), 7.0f) && FloatEq(button->GetMarginBottom(), 8.0f),
                "TextureToggleButton nine-slice margins round-trip");
    button->SetUseNineSlice(false);
    TEST_ASSERT(!button->GetUseNineSlice(), "TextureToggleButton nine-slice disabled");

    bool toggledValue = false;
    button->SetOnToggledCallback([&toggledValue](bool v) { toggledValue = v; });
    button->SetToggled(true);
    TEST_ASSERT(button->IsToggled(), "TextureToggleButton re-toggled on with callback set");

    button->SetLayer(6);
    TEST_ASSERT(button->GetLayer() == 6, "TextureToggleButton layer round-trips");
    button->SetSortingOrder(-3);
    TEST_ASSERT(button->GetSortingOrder() == -3, "TextureToggleButton sorting order round-trips");

    return true;
}

} // namespace

void RunUIBasicComponentTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "UI BASIC COMPONENT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("UIBasicComponents");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestLabel();
    allPassed &= TestButtonStateAndModes();
    allPassed &= TestButtonTextAndColors();
    allPassed &= TestButtonBordersAndCorners();
    allPassed &= TestButtonPerStateData();
    allPassed &= TestButtonBackgroundImage();
    allPassed &= TestColorRect();
    allPassed &= TestImage2D();
    allPassed &= TestPanel();
    allPassed &= TestNineSlicePanel();
    allPassed &= TestProgressBar();
    allPassed &= TestTextureButton();
    allPassed &= TestToggleButton();
    allPassed &= TestTextureToggleButton();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL UI BASIC COMPONENT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}

/**
 * @file UI3DComponentTests.cpp
 * @brief Headless behavioral tests for the 3D UI components
 *        (Label3D, Button3D, ProgressBar3D, Panel3D).
 *
 * These components inherit from core::Component and IRenderableComponent. Their
 * rendering paths require a live GPU/renderer/window plus real font, texture and
 * style assets, so those are intentionally NOT exercised here. Instead the tests
 * focus on the pure data layer: property setter/getter round-trips, enum state,
 * per-state configuration and value clamping — the parts a project configures and
 * that must be correct independently of any rendering device.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/components/Label3D.hpp"
#include "lupine/components/Button3D.hpp"
#include "lupine/components/ProgressBar3D.hpp"
#include "lupine/components/Panel3D.hpp"
#include "lupine/components/Sprite3D.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <cmath>

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

bool TestLabel3D() {
    TEST_SECTION("UI3D: Label3D Properties");

    std::shared_ptr<Label3D> label = std::make_shared<Label3D>();
    label->RegisterProperties();
    TEST_ASSERT(label != nullptr, "Label3D constructs via make_shared");
    TEST_ASSERT(label->GetTypeName() == "Label3D", "Label3D reports its type name");

    label->SetText("Hello 3D World");
    TEST_ASSERT(label->GetText() == "Hello 3D World", "Label3D text round-trips");

    label->SetText("");
    TEST_ASSERT(label->GetText().empty(), "Label3D empty text round-trips");

    label->SetFontPath("res://fonts/default.ttf");
    TEST_ASSERT(label->GetFontPath() == "res://fonts/default.ttf", "Label3D font path round-trips");

    label->SetFontSize(42.0f);
    TEST_ASSERT(FloatEq(label->GetFontSize(), 42.0f), "Label3D font size round-trips");

    label->SetColor(math::Color(0.25f, 0.5f, 0.75f, 0.9f));
    TEST_ASSERT(ColorEq(label->GetColor(), 0.25f, 0.5f, 0.75f, 0.9f), "Label3D color round-trips");

    label->SetCentered(true);
    TEST_ASSERT(label->GetCentered(), "Label3D centered flag true round-trips");
    label->SetCentered(false);
    TEST_ASSERT(!label->GetCentered(), "Label3D centered flag false round-trips");

    label->SetOffset(math::Vec2(3.0f, -7.0f));
    TEST_ASSERT(FloatEq(label->GetOffset().x, 3.0f) && FloatEq(label->GetOffset().y, -7.0f),
                "Label3D offset round-trips");

    label->SetBillboardMode(BillboardMode::Enabled);
    TEST_ASSERT(label->GetBillboardMode() == BillboardMode::Enabled,
                "Label3D billboard Enabled round-trips");
    label->SetBillboardMode(BillboardMode::YAxisOnly);
    TEST_ASSERT(label->GetBillboardMode() == BillboardMode::YAxisOnly,
                "Label3D billboard YAxisOnly round-trips");
    label->SetBillboardMode(BillboardMode::Disabled);
    TEST_ASSERT(label->GetBillboardMode() == BillboardMode::Disabled,
                "Label3D billboard Disabled round-trips");

    label->SetPixelSize(0.01f);
    TEST_ASSERT(FloatEq(label->GetPixelSize(), 0.01f), "Label3D pixel size round-trips");

    std::shared_ptr<Label3D> named = std::make_shared<Label3D>(std::string("MyLabel"));
    named->RegisterProperties();
    TEST_ASSERT(named != nullptr, "Label3D named constructor works");

    return true;
}

bool TestButton3DBasics() {
    TEST_SECTION("UI3D: Button3D Size / Rendering / Layer");

    std::shared_ptr<Button3D> button = std::make_shared<Button3D>();
    button->RegisterProperties();
    TEST_ASSERT(button != nullptr, "Button3D constructs via make_shared");
    TEST_ASSERT(button->GetTypeName() == "Button3D", "Button3D reports its type name");

    button->SetWidth(4.5f);
    TEST_ASSERT(FloatEq(button->GetWidth(), 4.5f), "Button3D width round-trips");
    button->SetHeight(2.25f);
    TEST_ASSERT(FloatEq(button->GetHeight(), 2.25f), "Button3D height round-trips");

    button->SetBillboardMode(Button3DBillboardMode::Enabled);
    TEST_ASSERT(button->GetBillboardMode() == Button3DBillboardMode::Enabled,
                "Button3D billboard Enabled round-trips");
    button->SetBillboardMode(Button3DBillboardMode::YAxisOnly);
    TEST_ASSERT(button->GetBillboardMode() == Button3DBillboardMode::YAxisOnly,
                "Button3D billboard YAxisOnly round-trips");
    button->SetBillboardMode(Button3DBillboardMode::Disabled);
    TEST_ASSERT(button->GetBillboardMode() == Button3DBillboardMode::Disabled,
                "Button3D billboard Disabled round-trips");

    button->SetDoubleSided(true);
    TEST_ASSERT(button->GetDoubleSided(), "Button3D double-sided round-trips");
    button->SetDoubleSided(false);
    TEST_ASSERT(!button->GetDoubleSided(), "Button3D double-sided clears");

    button->SetCastShadow(true);
    TEST_ASSERT(button->GetCastShadow(), "Button3D cast shadow round-trips");
    button->SetReceiveShadow(true);
    TEST_ASSERT(button->GetReceiveShadow(), "Button3D receive shadow round-trips");

    button->SetLayer(5);
    TEST_ASSERT(button->GetLayer() == 5, "Button3D layer round-trips");
    button->SetSortingOrder(12);
    TEST_ASSERT(button->GetSortingOrder() == 12, "Button3D sorting order round-trips");

    return true;
}

bool TestButton3DState() {
    TEST_SECTION("UI3D: Button3D State / Modes");

    std::shared_ptr<Button3D> button = std::make_shared<Button3D>();
    button->RegisterProperties();

    TEST_ASSERT(button->GetCurrentState() == Button3DState::Normal,
                "Button3D defaults to Normal state");
    TEST_ASSERT(button->IsButtonEnabled(), "Button3D defaults to enabled");

    button->SetEnabled(false);
    TEST_ASSERT(!button->IsButtonEnabled(), "Button3D disable round-trips");
    button->SetEnabled(true);
    TEST_ASSERT(button->IsButtonEnabled(), "Button3D re-enable round-trips");

    button->SetStyleMode(Button3DStyleMode::Manual);
    TEST_ASSERT(button->GetStyleMode() == Button3DStyleMode::Manual,
                "Button3D manual style mode round-trips");
    button->SetStyleMode(Button3DStyleMode::Automatic);
    TEST_ASSERT(button->GetStyleMode() == Button3DStyleMode::Automatic,
                "Button3D automatic style mode round-trips");

    button->SetScaleMode(Button3DScaleMode::FitToText);
    TEST_ASSERT(button->GetScaleMode() == Button3DScaleMode::FitToText,
                "Button3D FitToText scale mode round-trips");
    button->SetScaleMode(Button3DScaleMode::FitToTextWidth);
    TEST_ASSERT(button->GetScaleMode() == Button3DScaleMode::FitToTextWidth,
                "Button3D FitToTextWidth scale mode round-trips");
    button->SetScaleMode(Button3DScaleMode::Fixed);
    TEST_ASSERT(button->GetScaleMode() == Button3DScaleMode::Fixed,
                "Button3D Fixed scale mode round-trips");

    button->SetTextPadding(math::Vec2(1.5f, 0.75f));
    TEST_ASSERT(FloatEq(button->GetTextPadding().x, 1.5f) && FloatEq(button->GetTextPadding().y, 0.75f),
                "Button3D text padding round-trips");

    return true;
}

bool TestButton3DStyleAndText() {
    TEST_SECTION("UI3D: Button3D Style / Border / Text");

    std::shared_ptr<Button3D> button = std::make_shared<Button3D>();
    button->RegisterProperties();

    button->SetBackgroundColor(math::Color(0.1f, 0.2f, 0.3f, 0.4f));
    TEST_ASSERT(ColorEq(button->GetBackgroundColor(), 0.1f, 0.2f, 0.3f, 0.4f),
                "Button3D background color round-trips");

    button->SetOpacity(0.6f);
    TEST_ASSERT(FloatEq(button->GetOpacity(), 0.6f), "Button3D opacity round-trips");

    button->SetBorderEnabled(true);
    TEST_ASSERT(button->GetBorderEnabled(), "Button3D border enabled round-trips");

    button->SetBorderColor(math::Color(1.0f, 0.0f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(button->GetBorderColor(), 1.0f, 0.0f, 0.0f, 1.0f),
                "Button3D border color round-trips");

    button->SetBorderWidthLinked(false);
    TEST_ASSERT(!button->GetBorderWidthLinked(), "Button3D border width unlinked round-trips");
    button->SetBorderWidthLeft(1.0f);
    button->SetBorderWidthRight(2.0f);
    button->SetBorderWidthTop(3.0f);
    button->SetBorderWidthBottom(4.0f);
    TEST_ASSERT(FloatEq(button->GetBorderWidthLeft(), 1.0f), "Button3D border width left round-trips");
    TEST_ASSERT(FloatEq(button->GetBorderWidthRight(), 2.0f), "Button3D border width right round-trips");
    TEST_ASSERT(FloatEq(button->GetBorderWidthTop(), 3.0f), "Button3D border width top round-trips");
    TEST_ASSERT(FloatEq(button->GetBorderWidthBottom(), 4.0f), "Button3D border width bottom round-trips");

    button->SetCornerRadiusLinked(false);
    TEST_ASSERT(!button->GetCornerRadiusLinked(), "Button3D corner radius unlinked round-trips");
    button->SetCornerRadiusTopLeft(5.0f);
    button->SetCornerRadiusTopRight(6.0f);
    button->SetCornerRadiusBottomLeft(7.0f);
    button->SetCornerRadiusBottomRight(8.0f);
    TEST_ASSERT(FloatEq(button->GetCornerRadiusTopLeft(), 5.0f), "Button3D corner TL round-trips");
    TEST_ASSERT(FloatEq(button->GetCornerRadiusTopRight(), 6.0f), "Button3D corner TR round-trips");
    TEST_ASSERT(FloatEq(button->GetCornerRadiusBottomLeft(), 7.0f), "Button3D corner BL round-trips");
    TEST_ASSERT(FloatEq(button->GetCornerRadiusBottomRight(), 8.0f), "Button3D corner BR round-trips");

    button->SetText("Click Me");
    TEST_ASSERT(button->GetText() == "Click Me", "Button3D text round-trips");
    button->SetFontPath("res://fonts/ui.ttf");
    TEST_ASSERT(button->GetFontPath() == "res://fonts/ui.ttf", "Button3D font path round-trips");
    button->SetFontSize(20.0f);
    TEST_ASSERT(FloatEq(button->GetFontSize(), 20.0f), "Button3D font size round-trips");
    button->SetFontColor(math::Color(0.9f, 0.8f, 0.7f, 1.0f));
    TEST_ASSERT(ColorEq(button->GetFontColor(), 0.9f, 0.8f, 0.7f, 1.0f),
                "Button3D font color round-trips");
    button->SetWordWrap(true);
    TEST_ASSERT(button->GetWordWrap(), "Button3D word wrap round-trips");

    return true;
}

bool TestButton3DPerState() {
    TEST_SECTION("UI3D: Button3D Per-State Data");

    std::shared_ptr<Button3D> button = std::make_shared<Button3D>();
    button->RegisterProperties();

    const Button3DState states[4] = {
        Button3DState::Normal, Button3DState::Hover,
        Button3DState::Pressed, Button3DState::Disabled
    };

    for (int i = 0; i < 4; ++i) {
        Button3DState state = states[i];
        float v = 0.2f + 0.1f * static_cast<float>(i);
        button->SetStateModulation(state, math::Color(v, v, v, 1.0f));
        TEST_ASSERT(ColorEq(button->GetStateModulation(state), v, v, v, 1.0f),
                    "Button3D per-state modulation round-trips");
    }

    for (int i = 0; i < 4; ++i) {
        Button3DState state = states[i];
        button->SetStateSoundPath(state, "res://sfx/click.wav");
        TEST_ASSERT(button->GetStateSound(state).audioPath == "res://sfx/click.wav",
                    "Button3D per-state sound path round-trips");
    }

    Button3DStateSound sound;
    sound.audioPath = "res://sfx/hover.wav";
    sound.busName = "UI";
    sound.volume = 0.5f;
    button->SetStateSound(Button3DState::Hover, sound);
    TEST_ASSERT(button->GetStateSound(Button3DState::Hover).busName == "UI",
                "Button3D per-state sound bus round-trips");
    TEST_ASSERT(FloatEq(button->GetStateSound(Button3DState::Hover).volume, 0.5f),
                "Button3D per-state sound volume round-trips");

    Button3DStateTween tween;
    tween.enabled = true;
    tween.scaleOffset = math::Vec2(0.1f, 0.2f);
    tween.rotationOffset = 15.0f;
    tween.positionOffset = math::Vec2(0.3f, 0.4f);
    tween.duration = 0.5f;
    button->SetStateTween(Button3DState::Pressed, tween);
    const Button3DStateTween& gotTween = button->GetStateTween(Button3DState::Pressed);
    TEST_ASSERT(gotTween.enabled, "Button3D per-state tween enabled round-trips");
    TEST_ASSERT(FloatEq(gotTween.scaleOffset.x, 0.1f) && FloatEq(gotTween.scaleOffset.y, 0.2f),
                "Button3D per-state tween scale round-trips");
    TEST_ASSERT(FloatEq(gotTween.rotationOffset, 15.0f),
                "Button3D per-state tween rotation round-trips");
    TEST_ASSERT(FloatEq(gotTween.positionOffset.x, 0.3f) && FloatEq(gotTween.positionOffset.y, 0.4f),
                "Button3D per-state tween position round-trips");
    TEST_ASSERT(FloatEq(gotTween.duration, 0.5f),
                "Button3D per-state tween duration round-trips");

    bool callbackFired = false;
    button->SetStateCallback(Button3DState::Normal, [&callbackFired]() { callbackFired = true; });
    button->ClearStateCallback(Button3DState::Normal);
    TEST_ASSERT(!callbackFired, "Button3D state callback set/clear does not auto-fire");

    return true;
}

bool TestProgressBar3DValues() {
    TEST_SECTION("UI3D: ProgressBar3D Value / Clamping");

    std::shared_ptr<ProgressBar3D> bar = std::make_shared<ProgressBar3D>();
    bar->RegisterProperties();
    TEST_ASSERT(bar != nullptr, "ProgressBar3D constructs via make_shared");
    TEST_ASSERT(bar->GetTypeName() == "ProgressBar3D", "ProgressBar3D reports its type name");

    bar->SetMinValue(0.0f);
    bar->SetMaxValue(100.0f);
    TEST_ASSERT(FloatEq(bar->GetMinValue(), 0.0f), "ProgressBar3D min value round-trips");
    TEST_ASSERT(FloatEq(bar->GetMaxValue(), 100.0f), "ProgressBar3D max value round-trips");

    bar->SetValue(50.0f);
    TEST_ASSERT(FloatEq(bar->GetValue(), 50.0f), "ProgressBar3D in-range value round-trips");

    bar->SetValue(150.0f);
    TEST_ASSERT(bar->GetValue() <= bar->GetMaxValue() + 1e-3f,
                "ProgressBar3D value clamps to max");
    bar->SetValue(-25.0f);
    TEST_ASSERT(bar->GetValue() >= bar->GetMinValue() - 1e-3f,
                "ProgressBar3D value clamps to min");

    bar->SetValue(0.0f);
    TEST_ASSERT(FloatEq(bar->GetValue(), 0.0f), "ProgressBar3D value at min round-trips");

    bar->SetStep(5.0f);
    TEST_ASSERT(FloatEq(bar->GetStep(), 5.0f), "ProgressBar3D step round-trips");

    bar->SetSmooth(true);
    TEST_ASSERT(bar->GetSmooth(), "ProgressBar3D smooth flag round-trips");
    bar->SetSmoothSpeed(8.0f);
    TEST_ASSERT(FloatEq(bar->GetSmoothSpeed(), 8.0f), "ProgressBar3D smooth speed round-trips");

    return true;
}

bool TestProgressBar3DLayout() {
    TEST_SECTION("UI3D: ProgressBar3D Size / Orientation / 3D");

    std::shared_ptr<ProgressBar3D> bar = std::make_shared<ProgressBar3D>();
    bar->RegisterProperties();

    bar->SetWidth(6.0f);
    TEST_ASSERT(FloatEq(bar->GetWidth(), 6.0f), "ProgressBar3D width round-trips");
    bar->SetHeight(1.0f);
    TEST_ASSERT(FloatEq(bar->GetHeight(), 1.0f), "ProgressBar3D height round-trips");

    bar->SetOrientation(1);
    TEST_ASSERT(bar->GetOrientation() == 1, "ProgressBar3D vertical orientation round-trips");
    bar->SetOrientation(0);
    TEST_ASSERT(bar->GetOrientation() == 0, "ProgressBar3D horizontal orientation round-trips");

    bar->SetFillDirection(3);
    TEST_ASSERT(bar->GetFillDirection() == 3, "ProgressBar3D fill direction round-trips");
    bar->SetFillDirection(0);
    TEST_ASSERT(bar->GetFillDirection() == 0, "ProgressBar3D fill direction reset round-trips");

    bar->SetBillboardMode(ProgressBar3DBillboardMode::Enabled);
    TEST_ASSERT(bar->GetBillboardMode() == ProgressBar3DBillboardMode::Enabled,
                "ProgressBar3D billboard Enabled round-trips");
    bar->SetBillboardMode(ProgressBar3DBillboardMode::YAxisOnly);
    TEST_ASSERT(bar->GetBillboardMode() == ProgressBar3DBillboardMode::YAxisOnly,
                "ProgressBar3D billboard YAxisOnly round-trips");
    bar->SetBillboardMode(ProgressBar3DBillboardMode::Disabled);
    TEST_ASSERT(bar->GetBillboardMode() == ProgressBar3DBillboardMode::Disabled,
                "ProgressBar3D billboard Disabled round-trips");

    bar->SetDoubleSided(true);
    TEST_ASSERT(bar->GetDoubleSided(), "ProgressBar3D double-sided round-trips");
    bar->SetCastShadow(true);
    TEST_ASSERT(bar->GetCastShadow(), "ProgressBar3D cast shadow round-trips");
    bar->SetReceiveShadow(true);
    TEST_ASSERT(bar->GetReceiveShadow(), "ProgressBar3D receive shadow round-trips");

    return true;
}

bool TestProgressBar3DStyle() {
    TEST_SECTION("UI3D: ProgressBar3D Textures / Colors / Value Display");

    std::shared_ptr<ProgressBar3D> bar = std::make_shared<ProgressBar3D>();
    bar->RegisterProperties();

    bar->SetBackgroundTexturePath("res://ui/bg.png");
    TEST_ASSERT(bar->GetBackgroundTexturePath() == "res://ui/bg.png",
                "ProgressBar3D background texture path round-trips");
    bar->SetBackgroundColor(math::Color(0.1f, 0.1f, 0.1f, 1.0f));
    TEST_ASSERT(ColorEq(bar->GetBackgroundColor(), 0.1f, 0.1f, 0.1f, 1.0f),
                "ProgressBar3D background color round-trips");

    bar->SetFillTexturePath("res://ui/fill.png");
    TEST_ASSERT(bar->GetFillTexturePath() == "res://ui/fill.png",
                "ProgressBar3D fill texture path round-trips");
    bar->SetFillColor(math::Color(0.0f, 1.0f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(bar->GetFillColor(), 0.0f, 1.0f, 0.0f, 1.0f),
                "ProgressBar3D fill color round-trips");

    bar->SetBorderTexturePath("res://ui/border.png");
    TEST_ASSERT(bar->GetBorderTexturePath() == "res://ui/border.png",
                "ProgressBar3D border texture path round-trips");
    bar->SetBorderColor(math::Color(0.0f, 0.0f, 0.0f, 1.0f));
    TEST_ASSERT(ColorEq(bar->GetBorderColor(), 0.0f, 0.0f, 0.0f, 1.0f),
                "ProgressBar3D border color round-trips");

    bar->SetShowValue(true);
    TEST_ASSERT(bar->GetShowValue(), "ProgressBar3D show value round-trips");
    bar->SetValueFontPath("res://fonts/value.ttf");
    TEST_ASSERT(bar->GetValueFontPath() == "res://fonts/value.ttf",
                "ProgressBar3D value font path round-trips");
    bar->SetValueFontSize(16.0f);
    TEST_ASSERT(FloatEq(bar->GetValueFontSize(), 16.0f),
                "ProgressBar3D value font size round-trips");
    bar->SetValueColor(math::Color(1.0f, 1.0f, 1.0f, 1.0f));
    TEST_ASSERT(ColorEq(bar->GetValueColor(), 1.0f, 1.0f, 1.0f, 1.0f),
                "ProgressBar3D value color round-trips");

    bar->SetCornerRadiusLinked(false);
    TEST_ASSERT(!bar->IsCornerRadiusLinked(), "ProgressBar3D corner radius unlinked round-trips");
    bar->SetCornerRadiusIndividual(0, 2.0f);
    bar->SetCornerRadiusIndividual(1, 3.0f);
    TEST_ASSERT(FloatEq(bar->GetCornerRadius().Get(0), 2.0f) && FloatEq(bar->GetCornerRadius().Get(1), 3.0f),
                "ProgressBar3D individual corner radius round-trips");
    bar->SetCornerRadiusAll(4.0f);
    TEST_ASSERT(FloatEq(bar->GetCornerRadius().Get(0), 4.0f) && FloatEq(bar->GetCornerRadius().Get(3), 4.0f),
                "ProgressBar3D corner radius all round-trips");

    bar->SetBorderWidthLinked(false);
    TEST_ASSERT(!bar->IsBorderWidthLinked(), "ProgressBar3D border width unlinked round-trips");
    bar->SetBorderWidthIndividual(0, 1.0f);
    bar->SetBorderWidthIndividual(2, 5.0f);
    TEST_ASSERT(FloatEq(bar->GetBorderWidth().Get(0), 1.0f) && FloatEq(bar->GetBorderWidth().Get(2), 5.0f),
                "ProgressBar3D individual border width round-trips");
    bar->SetBorderWidthAll(2.0f);
    TEST_ASSERT(FloatEq(bar->GetBorderWidth().Get(1), 2.0f) && FloatEq(bar->GetBorderWidth().Get(3), 2.0f),
                "ProgressBar3D border width all round-trips");

    return true;
}

bool TestPanel3DBasics() {
    TEST_SECTION("UI3D: Panel3D Size / Rendering / Layer");

    std::shared_ptr<Panel3D> panel = std::make_shared<Panel3D>();
    panel->RegisterProperties();
    TEST_ASSERT(panel != nullptr, "Panel3D constructs via make_shared");
    TEST_ASSERT(panel->GetTypeName() == "Panel3D", "Panel3D reports its type name");

    panel->SetWidth(8.0f);
    TEST_ASSERT(FloatEq(panel->GetWidth(), 8.0f), "Panel3D width round-trips");
    panel->SetHeight(5.0f);
    TEST_ASSERT(FloatEq(panel->GetHeight(), 5.0f), "Panel3D height round-trips");

    panel->SetBillboardMode(Panel3DBillboardMode::Enabled);
    TEST_ASSERT(panel->GetBillboardMode() == Panel3DBillboardMode::Enabled,
                "Panel3D billboard Enabled round-trips");
    panel->SetBillboardMode(Panel3DBillboardMode::YAxisOnly);
    TEST_ASSERT(panel->GetBillboardMode() == Panel3DBillboardMode::YAxisOnly,
                "Panel3D billboard YAxisOnly round-trips");
    panel->SetBillboardMode(Panel3DBillboardMode::Disabled);
    TEST_ASSERT(panel->GetBillboardMode() == Panel3DBillboardMode::Disabled,
                "Panel3D billboard Disabled round-trips");

    panel->SetDoubleSided(true);
    TEST_ASSERT(panel->GetDoubleSided(), "Panel3D double-sided round-trips");
    panel->SetCastShadow(true);
    TEST_ASSERT(panel->GetCastShadow(), "Panel3D cast shadow round-trips");
    panel->SetReceiveShadow(true);
    TEST_ASSERT(panel->GetReceiveShadow(), "Panel3D receive shadow round-trips");

    panel->SetLayer(2);
    TEST_ASSERT(panel->GetLayer() == 2, "Panel3D layer round-trips");
    panel->SetSortingOrder(7);
    TEST_ASSERT(panel->GetSortingOrder() == 7, "Panel3D sorting order round-trips");

    panel->SetStylePath("res://styles/panel.style");
    TEST_ASSERT(panel->GetStylePath() == "res://styles/panel.style",
                "Panel3D style path round-trips");

    return true;
}

bool TestPanel3DStyle() {
    TEST_SECTION("UI3D: Panel3D StyleBox Properties");

    std::shared_ptr<Panel3D> panel = std::make_shared<Panel3D>();
    panel->RegisterProperties();

    panel->SetBackgroundColor(math::Color(0.2f, 0.3f, 0.4f, 0.5f));
    TEST_ASSERT(ColorEq(panel->GetBackgroundColor(), 0.2f, 0.3f, 0.4f, 0.5f),
                "Panel3D background color round-trips");
    panel->SetOpacity(0.75f);
    TEST_ASSERT(FloatEq(panel->GetOpacity(), 0.75f), "Panel3D opacity round-trips");

    panel->SetBorderEnabled(true);
    TEST_ASSERT(panel->GetBorderEnabled(), "Panel3D border enabled round-trips");
    panel->SetBorderColor(math::Color(0.5f, 0.5f, 0.5f, 1.0f));
    TEST_ASSERT(ColorEq(panel->GetBorderColor(), 0.5f, 0.5f, 0.5f, 1.0f),
                "Panel3D border color round-trips");

    panel->SetBorderWidthLinked(false);
    TEST_ASSERT(!panel->GetBorderWidthLinked(), "Panel3D border width unlinked round-trips");
    panel->SetBorderWidthLeft(1.0f);
    panel->SetBorderWidthRight(2.0f);
    panel->SetBorderWidthTop(3.0f);
    panel->SetBorderWidthBottom(4.0f);
    TEST_ASSERT(FloatEq(panel->GetBorderWidthLeft(), 1.0f), "Panel3D border width left round-trips");
    TEST_ASSERT(FloatEq(panel->GetBorderWidthRight(), 2.0f), "Panel3D border width right round-trips");
    TEST_ASSERT(FloatEq(panel->GetBorderWidthTop(), 3.0f), "Panel3D border width top round-trips");
    TEST_ASSERT(FloatEq(panel->GetBorderWidthBottom(), 4.0f), "Panel3D border width bottom round-trips");

    panel->SetCornerRadiusLinked(false);
    TEST_ASSERT(!panel->GetCornerRadiusLinked(), "Panel3D corner radius unlinked round-trips");
    panel->SetCornerRadiusTopLeft(5.0f);
    panel->SetCornerRadiusTopRight(6.0f);
    panel->SetCornerRadiusBottomLeft(7.0f);
    panel->SetCornerRadiusBottomRight(8.0f);
    TEST_ASSERT(FloatEq(panel->GetCornerRadiusTopLeft(), 5.0f), "Panel3D corner TL round-trips");
    TEST_ASSERT(FloatEq(panel->GetCornerRadiusTopRight(), 6.0f), "Panel3D corner TR round-trips");
    TEST_ASSERT(FloatEq(panel->GetCornerRadiusBottomLeft(), 7.0f), "Panel3D corner BL round-trips");
    TEST_ASSERT(FloatEq(panel->GetCornerRadiusBottomRight(), 8.0f), "Panel3D corner BR round-trips");

    panel->SetCornerDetail(16);
    TEST_ASSERT(panel->GetCornerDetail() == 16, "Panel3D corner detail round-trips");

    panel->SetAntiAliasing(true);
    TEST_ASSERT(panel->GetAntiAliasing(), "Panel3D anti-aliasing round-trips");
    panel->SetAntiAliasingSize(1.5f);
    TEST_ASSERT(FloatEq(panel->GetAntiAliasingSize(), 1.5f),
                "Panel3D anti-aliasing size round-trips");

    panel->SetShadowEnabled(true);
    TEST_ASSERT(panel->GetShadowEnabled(), "Panel3D shadow enabled round-trips");
    panel->SetShadowColor(math::Color(0.0f, 0.0f, 0.0f, 0.5f));
    TEST_ASSERT(ColorEq(panel->GetShadowColor(), 0.0f, 0.0f, 0.0f, 0.5f),
                "Panel3D shadow color round-trips");
    panel->SetShadowSize(4.0f);
    TEST_ASSERT(FloatEq(panel->GetShadowSize(), 4.0f), "Panel3D shadow size round-trips");
    panel->SetShadowOffset(math::Vec2(2.0f, -3.0f));
    TEST_ASSERT(FloatEq(panel->GetShadowOffset().x, 2.0f) && FloatEq(panel->GetShadowOffset().y, -3.0f),
                "Panel3D shadow offset round-trips");

    panel->SetCustomShaderPath("res://shaders/panel.lsh");
    TEST_ASSERT(panel->GetCustomShaderPath() == "res://shaders/panel.lsh",
                "Panel3D custom shader path round-trips");

    return true;
}

} // namespace

void RunUI3DComponentTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "UI 3D COMPONENT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("UI3DComponents");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestLabel3D();
    allPassed &= TestButton3DBasics();
    allPassed &= TestButton3DState();
    allPassed &= TestButton3DStyleAndText();
    allPassed &= TestButton3DPerState();
    allPassed &= TestProgressBar3DValues();
    allPassed &= TestProgressBar3DLayout();
    allPassed &= TestProgressBar3DStyle();
    allPassed &= TestPanel3DBasics();
    allPassed &= TestPanel3DStyle();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL UI 3D COMPONENT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}

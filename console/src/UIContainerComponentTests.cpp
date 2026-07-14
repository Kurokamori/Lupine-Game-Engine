/**
 * @file UIContainerComponentTests.cpp
 * @brief Headless behavioral tests for the UI container family and the shared
 *        UIControl base, plus the StyleBox resource and UIImageDraw helpers.
 *
 * These components are configured through plain property getters/setters whose
 * state survives independently of the renderer, window, and owning Node tree.
 * The tests therefore construct each component in isolation (no owner Node) and
 * verify property round-trips, default values, enum mappings, and serialization.
 *
 * Methods that require a live GPU/renderer, a real owner Node2D, or computed
 * child layout (CalculateLayout / ForceLayoutUpdate / ResolveLayout / SetRect /
 * buildDrawCommands / DrawUIImage / GetChildren enumeration) are intentionally
 * not driven here; they need infrastructure that is unavailable in the headless
 * console harness. Configuration state is the focus.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/components/Container.hpp"
#include "lupine/components/PaddingContainer.hpp"
#include "lupine/components/CenterContainer.hpp"
#include "lupine/components/HorizontalContainer.hpp"
#include "lupine/components/VerticalContainer.hpp"
#include "lupine/components/GridContainer.hpp"
#include "lupine/components/DockContainer.hpp"
#include "lupine/components/Stack.hpp"
#include "lupine/components/Wrap.hpp"
#include "lupine/components/Spacer.hpp"
#include "lupine/components/ScrollContainer.hpp"
#include "lupine/components/SplitContainer.hpp"
#include "lupine/components/AspectRatioContainer.hpp"
#include "lupine/components/LayoutSlot.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/components/StyleBox.hpp"
#include "lupine/components/UIImageDraw.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <cmath>
#include <cstring>
#include <vector>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

bool FloatEq(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) < tol;
}

bool Vec2Eq(const math::Vec2& a, const math::Vec2& b, float tol = 1e-4f) {
    return FloatEq(a.x, b.x, tol) && FloatEq(a.y, b.y, tol);
}

bool Vec4Eq(const math::Vec4& a, const math::Vec4& b, float tol = 1e-4f) {
    return FloatEq(a.x, b.x, tol) && FloatEq(a.y, b.y, tol) &&
           FloatEq(a.z, b.z, tol) && FloatEq(a.w, b.w, tol);
}

bool ColorEq(const math::Color& a, const math::Color& b, float tol = 1e-4f) {
    return FloatEq(a.r, b.r, tol) && FloatEq(a.g, b.g, tol) &&
           FloatEq(a.b, b.b, tol) && FloatEq(a.a, b.a, tol);
}

// ============================================================
// UIControl - shared base class
// ============================================================

bool TestUIControlSize() {
    TEST_SECTION("UIControl: Size");

    std::shared_ptr<UIControl> control = std::make_shared<UIControl>();
    control->RegisterProperties();

    control->SetWidth(320.0f);
    TEST_ASSERT(FloatEq(control->GetWidth(), 320.0f), "Width round-trips");

    control->SetHeight(180.0f);
    TEST_ASSERT(FloatEq(control->GetHeight(), 180.0f), "Height round-trips");

    control->SetSize(math::Vec2(640.0f, 480.0f));
    TEST_ASSERT(Vec2Eq(control->GetSize(), math::Vec2(640.0f, 480.0f)), "Size round-trips");
    TEST_ASSERT(FloatEq(control->GetWidth(), 640.0f), "SetSize updates width");
    TEST_ASSERT(FloatEq(control->GetHeight(), 480.0f), "SetSize updates height");

    control->SetCustomMinSize(math::Vec2(40.0f, 24.0f));
    TEST_ASSERT(Vec2Eq(control->GetCustomMinSize(), math::Vec2(40.0f, 24.0f)),
                "Custom min size round-trips");

    control->SetCustomMaxSize(math::Vec2(800.0f, 600.0f));
    TEST_ASSERT(Vec2Eq(control->GetCustomMaxSize(), math::Vec2(800.0f, 600.0f)),
                "Custom max size round-trips");

    return true;
}

bool TestUIControlMinMaxSize() {
    TEST_SECTION("UIControl: Effective Min/Max Size");

    std::shared_ptr<UIControl> control = std::make_shared<UIControl>();
    control->RegisterProperties();

    TEST_ASSERT(Vec2Eq(control->GetContentMinSize(), math::Vec2(0.0f, 0.0f)),
                "Base content min size is zero");

    control->SetCustomMinSize(math::Vec2(50.0f, 60.0f));
    TEST_ASSERT(Vec2Eq(control->GetMinSize(), math::Vec2(50.0f, 60.0f)),
                "Effective min size is the custom min when content is zero");

    control->SetCustomMaxSize(math::Vec2(200.0f, 150.0f));
    TEST_ASSERT(Vec2Eq(control->GetMaxSize(), math::Vec2(200.0f, 150.0f)),
                "Effective max size reports the custom max");

    return true;
}

bool TestUIControlLayoutMode() {
    TEST_SECTION("UIControl: Layout Mode");

    std::shared_ptr<UIControl> control = std::make_shared<UIControl>();
    control->RegisterProperties();

    control->SetLayoutMode(LayoutMode::Position);
    TEST_ASSERT(control->GetLayoutMode() == LayoutMode::Position, "Position layout mode round-trips");

    control->SetLayoutMode(LayoutMode::Anchors);
    TEST_ASSERT(control->GetLayoutMode() == LayoutMode::Anchors, "Anchors layout mode round-trips");

    return true;
}

bool TestUIControlAnchorsOffsets() {
    TEST_SECTION("UIControl: Anchors & Offsets");

    std::shared_ptr<UIControl> control = std::make_shared<UIControl>();
    control->RegisterProperties();

    control->SetAnchorMin(math::Vec2(0.1f, 0.2f));
    TEST_ASSERT(Vec2Eq(control->GetAnchorMin(), math::Vec2(0.1f, 0.2f)), "Anchor min round-trips");

    control->SetAnchorMax(math::Vec2(0.8f, 0.9f));
    TEST_ASSERT(Vec2Eq(control->GetAnchorMax(), math::Vec2(0.8f, 0.9f)), "Anchor max round-trips");

    control->SetOffsetMin(math::Vec2(5.0f, 7.0f));
    TEST_ASSERT(Vec2Eq(control->GetOffsetMin(), math::Vec2(5.0f, 7.0f)), "Offset min round-trips");

    control->SetOffsetMax(math::Vec2(-3.0f, -4.0f));
    TEST_ASSERT(Vec2Eq(control->GetOffsetMax(), math::Vec2(-3.0f, -4.0f)), "Offset max round-trips");

    TEST_ASSERT(control->GetAnchorPreset() == AnchorPreset::Custom,
                "Editing anchors directly switches the preset to Custom");

    return true;
}

bool TestUIControlAnchorPresets() {
    TEST_SECTION("UIControl: Anchor Presets");

    std::shared_ptr<UIControl> control = std::make_shared<UIControl>();
    control->RegisterProperties();

    control->SetAnchorPreset(AnchorPreset::Center);
    TEST_ASSERT(control->GetAnchorPreset() == AnchorPreset::Center, "Anchor preset round-trips");

    control->ApplyAnchorPreset(AnchorPreset::FullRect);
    TEST_ASSERT(control->GetAnchorPreset() == AnchorPreset::FullRect,
                "ApplyAnchorPreset records the applied preset");
    TEST_ASSERT(control->GetLayoutMode() == LayoutMode::Anchors,
                "ApplyAnchorPreset switches to Anchors layout mode");
    TEST_ASSERT(Vec2Eq(control->GetAnchorMin(), math::Vec2(0.0f, 0.0f)),
                "FullRect preset anchors min to (0,0)");
    TEST_ASSERT(Vec2Eq(control->GetAnchorMax(), math::Vec2(1.0f, 1.0f)),
                "FullRect preset anchors max to (1,1)");

    math::Vec2 outMin;
    math::Vec2 outMax;
    UIControl::PresetToAnchors(AnchorPreset::FullRect, outMin, outMax);
    TEST_ASSERT(Vec2Eq(outMin, math::Vec2(0.0f, 0.0f)) && Vec2Eq(outMax, math::Vec2(1.0f, 1.0f)),
                "PresetToAnchors resolves FullRect corners");

    UIControl::PresetToAnchors(AnchorPreset::TopLeft, outMin, outMax);
    TEST_ASSERT(Vec2Eq(outMin, math::Vec2(0.0f, 0.0f)) && Vec2Eq(outMax, math::Vec2(0.0f, 0.0f)),
                "PresetToAnchors resolves TopLeft corners");

    UIControl::PresetToAnchors(AnchorPreset::Center, outMin, outMax);
    TEST_ASSERT(Vec2Eq(outMin, math::Vec2(0.5f, 0.5f)) && Vec2Eq(outMax, math::Vec2(0.5f, 0.5f)),
                "PresetToAnchors resolves Center corners");

    UIControl::PresetToAnchors(AnchorPreset::BottomRight, outMin, outMax);
    TEST_ASSERT(Vec2Eq(outMin, math::Vec2(1.0f, 1.0f)) && Vec2Eq(outMax, math::Vec2(1.0f, 1.0f)),
                "PresetToAnchors resolves BottomRight corners");

    UIControl::PresetToAnchors(AnchorPreset::TopWide, outMin, outMax);
    TEST_ASSERT(Vec2Eq(outMin, math::Vec2(0.0f, 0.0f)) && Vec2Eq(outMax, math::Vec2(1.0f, 0.0f)),
                "PresetToAnchors resolves TopWide corners");

    return true;
}

bool TestUIControlPivotGrow() {
    TEST_SECTION("UIControl: Grow Direction");

    std::shared_ptr<UIControl> control = std::make_shared<UIControl>();
    control->RegisterProperties();

    // Pivot was removed: the property was never read by the solver, and asserting that a
    // dead field round-trips the value it was handed is precisely the kind of test that let
    // it ship. Every control is center-pivoted by WriteRectToNode.

    control->SetGrowDirectionH(GrowDirection::Both);
    TEST_ASSERT(control->GetGrowDirectionH() == GrowDirection::Both,
                "Horizontal grow direction round-trips");

    control->SetGrowDirectionV(GrowDirection::End);
    TEST_ASSERT(control->GetGrowDirectionV() == GrowDirection::End,
                "Vertical grow direction round-trips");

    return true;
}

bool TestUIControlSizeFlags() {
    TEST_SECTION("UIControl: Container Size Flags");

    std::shared_ptr<UIControl> control = std::make_shared<UIControl>();
    control->RegisterProperties();

    int combined = SizeFlagBits::Fill | SizeFlagBits::Expand;
    control->SetSizeFlagsHorizontal(combined);
    TEST_ASSERT(control->GetSizeFlagsHorizontal() == combined,
                "Horizontal size flags (Fill|Expand) round-trip");

    control->SetSizeFlagsVertical(SizeFlagBits::ShrinkCenter);
    TEST_ASSERT(control->GetSizeFlagsVertical() == SizeFlagBits::ShrinkCenter,
                "Vertical size flags round-trip");

    control->SetSizeFlagsStretchRatio(2.5f);
    TEST_ASSERT(FloatEq(control->GetSizeFlagsStretchRatio(), 2.5f),
                "Size flags stretch ratio round-trips");

    TEST_ASSERT((control->GetSizeFlagsHorizontal() & SizeFlagBits::Expand) != 0,
                "Expand bit is set in horizontal flags");
    TEST_ASSERT((control->GetSizeFlagsHorizontal() & SizeFlagBits::Fill) != 0,
                "Fill bit is set in horizontal flags");

    return true;
}

bool TestUIControlUISpace() {
    TEST_SECTION("UIControl: UI Space Flag");

    std::shared_ptr<UIControl> control = std::make_shared<UIControl>();
    control->RegisterProperties();

    control->SetUISpace(true);
    TEST_ASSERT(control->GetUISpace(), "UI space flag set round-trips");
    TEST_ASSERT(control->GetUseUISpace(), "Legacy GetUseUISpace alias agrees");

    control->SetUseUISpace(false);
    TEST_ASSERT(!control->GetUISpace(), "Legacy SetUseUISpace alias clears the flag");
    TEST_ASSERT(!control->GetUseUISpace(), "Legacy GetUseUISpace alias reflects cleared flag");

    return true;
}

bool TestUIControlRectState() {
    TEST_SECTION("UIControl: Rect & Container-Driven State");

    std::shared_ptr<UIControl> control = std::make_shared<UIControl>();
    control->RegisterProperties();

    TEST_ASSERT(!control->IsContainerDriven(), "Control is not container-driven by default");
    control->SetContainerDriven(true);
    TEST_ASSERT(control->IsContainerDriven(), "Container-driven flag round-trips");
    control->SetContainerDriven(false);
    TEST_ASSERT(!control->IsContainerDriven(), "Container-driven flag clears");

    TEST_ASSERT(!control->IsLayoutContainer(), "Base UIControl is not a layout container");
    TEST_ASSERT(!control->ClipsDescendants(), "Base UIControl does not clip descendants by default");

    control->SetSize(math::Vec2(100.0f, 50.0f));
    TEST_ASSERT(Vec2Eq(control->GetBoundsSize(), math::Vec2(100.0f, 50.0f)),
                "Unresolved control reports its requested size as bounds size");

    const math::Rect& resolved = control->GetResolvedRect();
    TEST_ASSERT(Vec2Eq(resolved.size, math::Vec2(0.0f, 0.0f)),
                "Resolved rect defaults to zero size before layout");

    return true;
}

bool TestUIControlTheme() {
    TEST_SECTION("UIControl: Theme Configuration");

    std::shared_ptr<UIControl> control = std::make_shared<UIControl>();
    control->RegisterProperties();

    TEST_ASSERT(control->GetThemeTypeName() == "UIControl",
                "Theme type name defaults to the component type name");

    control->SetThemeTypeVariation("DangerPanel");
    TEST_ASSERT(control->GetThemeTypeVariation() == "DangerPanel",
                "Theme type variation round-trips");

    uint64_t genBefore = UIControl::GetThemeOwnerGeneration();
    control->SetThemePath("res://themes/dark.uitheme");
    TEST_ASSERT(control->GetThemePath() == "res://themes/dark.uitheme",
                "Theme path round-trips");
    TEST_ASSERT(UIControl::GetThemeOwnerGeneration() > genBefore,
                "Setting a theme path advances the theme owner generation");

    control->SetThemeOverride("background_color", true);
    TEST_ASSERT(control->IsThemeOverridden("background_color"),
                "Theme override flag is recorded");
    TEST_ASSERT(control->GetThemeOverrides().count("background_color") == 1,
                "Theme override appears in the override set");

    control->SetThemeOverride("background_color", false);
    TEST_ASSERT(!control->IsThemeOverridden("background_color"),
                "Theme override flag clears");

    control->SetThemeOverride("font_color", true);
    control->ClearThemeOverrides();
    TEST_ASSERT(control->GetThemeOverrides().empty(),
                "ClearThemeOverrides empties the override set");

    return true;
}

bool TestUIControlFocus() {
    TEST_SECTION("UIControl: Keyboard Focus");

    UIControl::ClearFocus();
    TEST_ASSERT(UIControl::GetFocusedControl() == nullptr, "No control is focused after ClearFocus");

    std::shared_ptr<UIControl> a = std::make_shared<UIControl>();
    a->RegisterProperties();
    std::shared_ptr<UIControl> b = std::make_shared<UIControl>();
    b->RegisterProperties();

    a->GrabFocus();
    TEST_ASSERT(a->HasFocus(), "Control reports focus after GrabFocus");
    TEST_ASSERT(UIControl::GetFocusedControl() == a.get(), "Focused control pointer matches");
    TEST_ASSERT(!b->HasFocus(), "Unfocused control does not report focus");

    b->GrabFocus();
    TEST_ASSERT(b->HasFocus() && !a->HasFocus(), "Focus transfers to the most recent grabber");

    b->ReleaseFocus();
    TEST_ASSERT(!b->HasFocus(), "ReleaseFocus clears focus from the holder");
    TEST_ASSERT(UIControl::GetFocusedControl() == nullptr, "No control is focused after ReleaseFocus");

    return true;
}

bool TestUIControlSerialization() {
    TEST_SECTION("UIControl: Serialization Round-Trip");

    std::shared_ptr<UIControl> control = std::make_shared<UIControl>();
    control->RegisterProperties();
    control->SetSize(math::Vec2(256.0f, 128.0f));
    control->SetLayoutMode(LayoutMode::Anchors);
    control->SetThemeOverride("border_color", true);

    nlohmann::json json = control->Serialize();
    TEST_ASSERT(json.is_object(), "Serialize produces a JSON object");

    std::shared_ptr<UIControl> restored = std::make_shared<UIControl>();
    restored->RegisterProperties();
    restored->Deserialize(json);
    TEST_ASSERT(restored->IsThemeOverridden("border_color"),
                "Deserialize restores the theme-override set");

    return true;
}

// ============================================================
// Container - shared container base
// ============================================================

bool TestContainerSize() {
    TEST_SECTION("Container: Size & Size Modes");

    std::shared_ptr<Container> container = std::make_shared<Container>();
    container->RegisterProperties();

    container->SetWidth(500.0f);
    TEST_ASSERT(FloatEq(container->GetWidth(), 500.0f), "Container width round-trips");
    container->SetHeight(300.0f);
    TEST_ASSERT(FloatEq(container->GetHeight(), 300.0f), "Container height round-trips");
    container->SetSize(math::Vec2(800.0f, 600.0f));
    TEST_ASSERT(Vec2Eq(container->GetSize(), math::Vec2(800.0f, 600.0f)), "Container size round-trips");

    container->SetMinSize(math::Vec2(100.0f, 80.0f));
    TEST_ASSERT(Vec2Eq(container->GetMinSize(), math::Vec2(100.0f, 80.0f)),
                "Container min size constraint round-trips");
    container->SetMaxSize(math::Vec2(1200.0f, 900.0f));
    TEST_ASSERT(Vec2Eq(container->GetMaxSize(), math::Vec2(1200.0f, 900.0f)),
                "Container max size constraint round-trips");

    container->SetHorizontalSizeMode(Container::SizeMode::FitChildren);
    TEST_ASSERT(container->GetHorizontalSizeMode() == Container::SizeMode::FitChildren,
                "Horizontal size mode round-trips");
    container->SetVerticalSizeMode(Container::SizeMode::Expand);
    TEST_ASSERT(container->GetVerticalSizeMode() == Container::SizeMode::Expand,
                "Vertical size mode round-trips");

    return true;
}

bool TestContainerPadding() {
    TEST_SECTION("Container: Padding");

    std::shared_ptr<Container> container = std::make_shared<Container>();
    container->RegisterProperties();

    container->SetPaddingLinked(false);
    TEST_ASSERT(!container->GetPaddingLinked(), "Padding linked flag clears");

    container->SetPaddingLeft(4.0f);
    container->SetPaddingRight(8.0f);
    container->SetPaddingTop(12.0f);
    container->SetPaddingBottom(16.0f);
    TEST_ASSERT(FloatEq(container->GetPaddingLeft(), 4.0f), "Padding left round-trips");
    TEST_ASSERT(FloatEq(container->GetPaddingRight(), 8.0f), "Padding right round-trips");
    TEST_ASSERT(FloatEq(container->GetPaddingTop(), 12.0f), "Padding top round-trips");
    TEST_ASSERT(FloatEq(container->GetPaddingBottom(), 16.0f), "Padding bottom round-trips");

    math::Vec4 padding(20.0f, 21.0f, 22.0f, 23.0f);
    container->SetPadding(padding);
    TEST_ASSERT(Vec4Eq(container->GetPadding(), padding), "Vec4 padding round-trips");

    container->SetPaddingLinked(true);
    TEST_ASSERT(container->GetPaddingLinked(), "Padding linked flag sets");

    return true;
}

bool TestContainerMargin() {
    TEST_SECTION("Container: Margin");

    std::shared_ptr<Container> container = std::make_shared<Container>();
    container->RegisterProperties();

    container->SetMarginLinked(false);
    TEST_ASSERT(!container->GetMarginLinked(), "Margin linked flag clears");

    container->SetMarginLeft(1.0f);
    container->SetMarginRight(2.0f);
    container->SetMarginTop(3.0f);
    container->SetMarginBottom(4.0f);
    TEST_ASSERT(FloatEq(container->GetMarginLeft(), 1.0f), "Margin left round-trips");
    TEST_ASSERT(FloatEq(container->GetMarginRight(), 2.0f), "Margin right round-trips");
    TEST_ASSERT(FloatEq(container->GetMarginTop(), 3.0f), "Margin top round-trips");
    TEST_ASSERT(FloatEq(container->GetMarginBottom(), 4.0f), "Margin bottom round-trips");

    math::Vec4 margin(30.0f, 31.0f, 32.0f, 33.0f);
    container->SetMargin(margin);
    TEST_ASSERT(Vec4Eq(container->GetMargin(), margin), "Vec4 margin round-trips");

    return true;
}

bool TestContainerAppearance() {
    TEST_SECTION("Container: Visual Appearance");

    std::shared_ptr<Container> container = std::make_shared<Container>();
    container->RegisterProperties();

    math::Color bg(0.2f, 0.4f, 0.6f, 0.8f);
    container->SetBackgroundColor(bg);
    TEST_ASSERT(ColorEq(container->GetBackgroundColor(), bg), "Background color round-trips");

    container->SetOpacity(0.5f);
    TEST_ASSERT(FloatEq(container->GetOpacity(), 0.5f), "Opacity round-trips");

    container->SetDrawBackground(false);
    TEST_ASSERT(!container->GetDrawBackground(), "Draw-background flag clears");
    container->SetDrawBackground(true);
    TEST_ASSERT(container->GetDrawBackground(), "Draw-background flag sets");

    return true;
}

bool TestContainerBorder() {
    TEST_SECTION("Container: Border");

    std::shared_ptr<Container> container = std::make_shared<Container>();
    container->RegisterProperties();

    container->SetBorderEnabled(true);
    TEST_ASSERT(container->GetBorderEnabled(), "Border enabled flag round-trips");

    container->SetBorderWidthLinked(false);
    TEST_ASSERT(!container->GetBorderWidthLinked(), "Border width linked flag clears");

    container->SetBorderWidthLeft(1.5f);
    container->SetBorderWidthRight(2.5f);
    container->SetBorderWidthTop(3.5f);
    container->SetBorderWidthBottom(4.5f);
    TEST_ASSERT(FloatEq(container->GetBorderWidthLeft(), 1.5f), "Border width left round-trips");
    TEST_ASSERT(FloatEq(container->GetBorderWidthRight(), 2.5f), "Border width right round-trips");
    TEST_ASSERT(FloatEq(container->GetBorderWidthTop(), 3.5f), "Border width top round-trips");
    TEST_ASSERT(FloatEq(container->GetBorderWidthBottom(), 4.5f), "Border width bottom round-trips");

    math::Color borderColor(1.0f, 0.0f, 0.0f, 1.0f);
    container->SetBorderColor(borderColor);
    TEST_ASSERT(ColorEq(container->GetBorderColor(), borderColor), "Border color round-trips");

    return true;
}

bool TestContainerCornerRadius() {
    TEST_SECTION("Container: Corner Radius");

    std::shared_ptr<Container> container = std::make_shared<Container>();
    container->RegisterProperties();

    container->SetCornerRadiusLinked(false);
    TEST_ASSERT(!container->GetCornerRadiusLinked(), "Corner radius linked flag clears");

    container->SetCornerRadiusTopLeft(5.0f);
    container->SetCornerRadiusTopRight(6.0f);
    container->SetCornerRadiusBottomLeft(7.0f);
    container->SetCornerRadiusBottomRight(8.0f);
    TEST_ASSERT(FloatEq(container->GetCornerRadiusTopLeft(), 5.0f), "Corner radius top-left round-trips");
    TEST_ASSERT(FloatEq(container->GetCornerRadiusTopRight(), 6.0f), "Corner radius top-right round-trips");
    TEST_ASSERT(FloatEq(container->GetCornerRadiusBottomLeft(), 7.0f), "Corner radius bottom-left round-trips");
    TEST_ASSERT(FloatEq(container->GetCornerRadiusBottomRight(), 8.0f), "Corner radius bottom-right round-trips");

    return true;
}

bool TestContainerLayoutProps() {
    TEST_SECTION("Container: Layout & Rendering Properties");

    std::shared_ptr<Container> container = std::make_shared<Container>();
    container->RegisterProperties();

    container->SetClipChildren(true);
    TEST_ASSERT(container->GetClipChildren(), "Clip children flag round-trips");
    TEST_ASSERT(container->ClipsDescendants(), "ClipsDescendants follows the clip-children flag");

    container->SetSeparation(14.0f);
    TEST_ASSERT(FloatEq(container->GetSeparation(), 14.0f), "Separation round-trips");

    container->SetLayer(3);
    TEST_ASSERT(container->GetLayer() == 3, "Layer round-trips");
    container->SetSortingOrder(7);
    TEST_ASSERT(container->GetSortingOrder() == 7, "Sorting order round-trips");

    TEST_ASSERT(container->IsLayoutContainer(), "Container reports itself as a layout container");

    container->InvalidateLayout();
    TEST_ASSERT(container->GetLayoutDirty(), "InvalidateLayout marks the layout dirty");

    return true;
}

// ============================================================
// PaddingContainer
// ============================================================

bool TestPaddingContainer() {
    TEST_SECTION("PaddingContainer");

    std::shared_ptr<PaddingContainer> pad = std::make_shared<PaddingContainer>();
    pad->RegisterProperties();

    pad->SetAutoFitChildren(true);
    TEST_ASSERT(pad->GetAutoFitChildren(), "AutoFitChildren round-trips");
    pad->SetAutoFitChildren(false);
    TEST_ASSERT(!pad->GetAutoFitChildren(), "AutoFitChildren clears");

    pad->SetMaintainAspectRatio(true);
    TEST_ASSERT(pad->GetMaintainAspectRatio(), "MaintainAspectRatio round-trips");

    pad->SetChildAlignment(PaddingContainer::Alignment::Center);
    TEST_ASSERT(pad->GetChildAlignment() == PaddingContainer::Alignment::Center,
                "Child alignment Center round-trips");
    pad->SetChildAlignment(PaddingContainer::Alignment::BottomRight);
    TEST_ASSERT(pad->GetChildAlignment() == PaddingContainer::Alignment::BottomRight,
                "Child alignment BottomRight round-trips");

    TEST_ASSERT(pad->GetTypeName() == "PaddingContainer", "Type name is PaddingContainer");
    TEST_ASSERT(pad->IsLayoutContainer(), "PaddingContainer is a layout container");

    return true;
}

// ============================================================
// CenterContainer
// ============================================================

bool TestCenterContainer() {
    TEST_SECTION("CenterContainer");

    std::shared_ptr<CenterContainer> center = std::make_shared<CenterContainer>();
    center->RegisterProperties();

    center->SetAutoFitChild(true);
    TEST_ASSERT(center->GetAutoFitChild(), "AutoFitChild round-trips");
    center->SetAutoFitChild(false);
    TEST_ASSERT(!center->GetAutoFitChild(), "AutoFitChild clears");

    center->SetMaintainAspectRatio(true);
    TEST_ASSERT(center->GetMaintainAspectRatio(), "MaintainAspectRatio round-trips");

    center->SetStackChildren(true);
    TEST_ASSERT(center->GetStackChildren(), "StackChildren round-trips");
    center->SetStackChildren(false);
    TEST_ASSERT(!center->GetStackChildren(), "StackChildren clears");

    TEST_ASSERT(center->GetTypeName() == "CenterContainer", "Type name is CenterContainer");

    return true;
}

// ============================================================
// HorizontalContainer
// ============================================================

bool TestHorizontalContainer() {
    TEST_SECTION("HorizontalContainer");

    std::shared_ptr<HorizontalContainer> hbox = std::make_shared<HorizontalContainer>();
    hbox->RegisterProperties();

    hbox->SetVerticalAlignment(HorizontalContainer::VerticalAlignment::Center);
    TEST_ASSERT(hbox->GetVerticalAlignment() == HorizontalContainer::VerticalAlignment::Center,
                "Vertical alignment Center round-trips");
    hbox->SetVerticalAlignment(HorizontalContainer::VerticalAlignment::Fill);
    TEST_ASSERT(hbox->GetVerticalAlignment() == HorizontalContainer::VerticalAlignment::Fill,
                "Vertical alignment Fill round-trips");

    hbox->SetHorizontalAlignment(HorizontalContainer::HorizontalAlignment::End);
    TEST_ASSERT(hbox->GetHorizontalAlignment() == HorizontalContainer::HorizontalAlignment::End,
                "Horizontal alignment End round-trips");

    hbox->SetSeparation(9.0f);
    TEST_ASSERT(FloatEq(hbox->GetSeparation(), 9.0f), "Inherited separation round-trips");

    TEST_ASSERT(hbox->GetTypeName() == "HorizontalContainer", "Type name is HorizontalContainer");

    return true;
}

// ============================================================
// VerticalContainer
// ============================================================

bool TestVerticalContainer() {
    TEST_SECTION("VerticalContainer");

    std::shared_ptr<VerticalContainer> vbox = std::make_shared<VerticalContainer>();
    vbox->RegisterProperties();

    vbox->SetHorizontalAlignment(VerticalContainer::HorizontalAlignment::Center);
    TEST_ASSERT(vbox->GetHorizontalAlignment() == VerticalContainer::HorizontalAlignment::Center,
                "Horizontal alignment Center round-trips");
    vbox->SetHorizontalAlignment(VerticalContainer::HorizontalAlignment::Fill);
    TEST_ASSERT(vbox->GetHorizontalAlignment() == VerticalContainer::HorizontalAlignment::Fill,
                "Horizontal alignment Fill round-trips");

    vbox->SetVerticalAlignment(VerticalContainer::VerticalAlignment::End);
    TEST_ASSERT(vbox->GetVerticalAlignment() == VerticalContainer::VerticalAlignment::End,
                "Vertical alignment End round-trips");

    TEST_ASSERT(vbox->GetTypeName() == "VerticalContainer", "Type name is VerticalContainer");

    return true;
}

// ============================================================
// GridContainer
// ============================================================

bool TestGridContainer() {
    TEST_SECTION("GridContainer");

    std::shared_ptr<GridContainer> grid = std::make_shared<GridContainer>();
    grid->RegisterProperties();

    grid->SetUseRows(true);
    TEST_ASSERT(grid->GetUseRows(), "UseRows round-trips");
    grid->SetUseRows(false);
    TEST_ASSERT(!grid->GetUseRows(), "UseRows clears");

    grid->SetRowCount(5);
    TEST_ASSERT(grid->GetRowCount() == 5, "Row count round-trips");
    grid->SetColumnCount(4);
    TEST_ASSERT(grid->GetColumnCount() == 4, "Column count round-trips");

    grid->SetCellSizingMode(GridContainer::CellSizingMode::Fixed);
    TEST_ASSERT(grid->GetCellSizingMode() == GridContainer::CellSizingMode::Fixed,
                "Cell sizing mode Fixed round-trips");
    grid->SetCellSizingMode(GridContainer::CellSizingMode::AutoWidthFixedHeight);
    TEST_ASSERT(grid->GetCellSizingMode() == GridContainer::CellSizingMode::AutoWidthFixedHeight,
                "Cell sizing mode AutoWidthFixedHeight round-trips");

    grid->SetFixedCellWidth(64.0f);
    TEST_ASSERT(FloatEq(grid->GetFixedCellWidth(), 64.0f), "Fixed cell width round-trips");
    grid->SetFixedCellHeight(48.0f);
    TEST_ASSERT(FloatEq(grid->GetFixedCellHeight(), 48.0f), "Fixed cell height round-trips");

    grid->SetHorizontalSpacing(6.0f);
    TEST_ASSERT(FloatEq(grid->GetHorizontalSpacing(), 6.0f), "Horizontal spacing round-trips");
    grid->SetVerticalSpacing(10.0f);
    TEST_ASSERT(FloatEq(grid->GetVerticalSpacing(), 10.0f), "Vertical spacing round-trips");

    grid->SetFlowDirection(GridContainer::FlowDirection::TopToBottom);
    TEST_ASSERT(grid->GetFlowDirection() == GridContainer::FlowDirection::TopToBottom,
                "Flow direction TopToBottom round-trips");
    grid->SetFlowDirection(GridContainer::FlowDirection::RightToLeft);
    TEST_ASSERT(grid->GetFlowDirection() == GridContainer::FlowDirection::RightToLeft,
                "Flow direction RightToLeft round-trips");

    grid->SetHomogeneousCells(true);
    TEST_ASSERT(grid->GetHomogeneousCells(), "Homogeneous cells round-trips");

    grid->SetSizeChildren(true);
    TEST_ASSERT(grid->GetSizeChildren(), "Size children round-trips");

    grid->SetClipChildren(true);
    TEST_ASSERT(grid->GetClipChildren(), "Clip children round-trips");

    TEST_ASSERT(grid->GetTypeName() == "GridContainer", "Type name is GridContainer");

    return true;
}

// ============================================================
// DockContainer
// ============================================================

bool TestDockContainer() {
    TEST_SECTION("DockContainer");

    std::shared_ptr<DockContainer> dock = std::make_shared<DockContainer>();
    dock->RegisterProperties();

    dock->SetDockSpacing(7.5f);
    TEST_ASSERT(FloatEq(dock->GetDockSpacing(), 7.5f), "Dock spacing round-trips");

    dock->SetDefaultDockSide(DockContainer::DockSide::Center);
    TEST_ASSERT(dock->GetDefaultDockSide() == DockContainer::DockSide::Center,
                "Default dock side Center round-trips");
    dock->SetDefaultDockSide(DockContainer::DockSide::Right);
    TEST_ASSERT(dock->GetDefaultDockSide() == DockContainer::DockSide::Right,
                "Default dock side Right round-trips");

    TEST_ASSERT(dock->GetTypeName() == "DockContainer", "Type name is DockContainer");

    return true;
}

// ============================================================
// Stack
// ============================================================

bool TestStack() {
    TEST_SECTION("Stack");

    std::shared_ptr<Stack> stack = std::make_shared<Stack>();
    stack->RegisterProperties();

    stack->SetDefaultAlignment(Stack::Alignment::Center);
    TEST_ASSERT(stack->GetDefaultAlignment() == Stack::Alignment::Center,
                "Default alignment Center round-trips");
    stack->SetDefaultAlignment(Stack::Alignment::BottomRight);
    TEST_ASSERT(stack->GetDefaultAlignment() == Stack::Alignment::BottomRight,
                "Default alignment BottomRight round-trips");

    stack->SetSortByZIndex(true);
    TEST_ASSERT(stack->GetSortByZIndex(), "Sort by z-index round-trips");
    stack->SetSortByZIndex(false);
    TEST_ASSERT(!stack->GetSortByZIndex(), "Sort by z-index clears");

    TEST_ASSERT(stack->GetTypeName() == "Stack", "Type name is Stack");

    return true;
}

// ============================================================
// Wrap
// ============================================================

bool TestWrap() {
    TEST_SECTION("Wrap");

    std::shared_ptr<Wrap> wrap = std::make_shared<Wrap>();
    wrap->RegisterProperties();

    wrap->SetSpacingX(11.0f);
    TEST_ASSERT(FloatEq(wrap->GetSpacingX(), 11.0f), "Spacing X round-trips");
    wrap->SetSpacingY(13.0f);
    TEST_ASSERT(FloatEq(wrap->GetSpacingY(), 13.0f), "Spacing Y round-trips");

    wrap->SetLineAlignment(Wrap::LineAlignment::Center);
    TEST_ASSERT(wrap->GetLineAlignment() == Wrap::LineAlignment::Center,
                "Line alignment Center round-trips");
    wrap->SetLineAlignment(Wrap::LineAlignment::Justify);
    TEST_ASSERT(wrap->GetLineAlignment() == Wrap::LineAlignment::Justify,
                "Line alignment Justify round-trips");

    wrap->SetMaxLines(3);
    TEST_ASSERT(wrap->GetMaxLines() == 3, "Max lines round-trips");
    wrap->SetMaxLines(0);
    TEST_ASSERT(wrap->GetMaxLines() == 0, "Max lines unlimited (0) round-trips");

    wrap->SetWrapDirection(Wrap::WrapDirection::Vertical);
    TEST_ASSERT(wrap->GetWrapDirection() == Wrap::WrapDirection::Vertical,
                "Wrap direction Vertical round-trips");

    TEST_ASSERT(wrap->GetTypeName() == "Wrap", "Type name is Wrap");

    return true;
}

// ============================================================
// Spacer
// ============================================================

bool TestSpacer() {
    TEST_SECTION("Spacer");

    std::shared_ptr<Spacer> spacer = std::make_shared<Spacer>();
    spacer->RegisterProperties();

    spacer->SetMinSize(math::Vec2(50.0f, 20.0f));
    TEST_ASSERT(Vec2Eq(spacer->GetMinSize(), math::Vec2(50.0f, 20.0f)), "Spacer min size round-trips");
    TEST_ASSERT(Vec2Eq(spacer->GetContentMinSize(), math::Vec2(50.0f, 20.0f)),
                "Spacer content min size follows its min size constraint");

    spacer->SetMaxSize(math::Vec2(200.0f, 100.0f));
    TEST_ASSERT(Vec2Eq(spacer->GetMaxSize(), math::Vec2(200.0f, 100.0f)), "Spacer max size round-trips");

    spacer->SetExpand(true);
    TEST_ASSERT(spacer->GetExpand(), "Spacer expand flag round-trips");
    spacer->SetExpand(false);
    TEST_ASSERT(!spacer->GetExpand(), "Spacer expand flag clears");

    spacer->SetSize(math::Vec2(80.0f, 30.0f));
    TEST_ASSERT(Vec2Eq(spacer->GetSize(), math::Vec2(80.0f, 30.0f)),
                "Spacer inherits UIControl size round-trip");

    TEST_ASSERT(spacer->GetTypeName() == "Spacer", "Type name is Spacer");
    TEST_ASSERT(!spacer->IsLayoutContainer(), "Spacer is not a layout container");

    return true;
}

// ============================================================
// ScrollContainer
// ============================================================

bool TestScrollContainer() {
    TEST_SECTION("ScrollContainer");

    std::shared_ptr<ScrollContainer> scroll = std::make_shared<ScrollContainer>();
    scroll->RegisterProperties();

    TEST_ASSERT(scroll->ClipsDescendants(), "ScrollContainer always clips descendants");

    scroll->SetHScrollEnabled(true);
    TEST_ASSERT(scroll->GetHScrollEnabled(), "Horizontal scroll enabled round-trips");
    scroll->SetVScrollEnabled(false);
    TEST_ASSERT(!scroll->GetVScrollEnabled(), "Vertical scroll enabled clears");

    scroll->SetScrollSpeed(45.0f);
    TEST_ASSERT(FloatEq(scroll->GetScrollSpeed(), 45.0f), "Scroll speed round-trips");

    scroll->SetScrollBarWidth(18.0f);
    TEST_ASSERT(FloatEq(scroll->GetScrollBarWidth(), 18.0f), "Scrollbar width round-trips");

    scroll->SetHScrollBarVisibility(ScrollContainer::ScrollBarVisibility::AlwaysShow);
    TEST_ASSERT(scroll->GetHScrollBarVisibility() == ScrollContainer::ScrollBarVisibility::AlwaysShow,
                "Horizontal scrollbar visibility AlwaysShow round-trips");
    scroll->SetVScrollBarVisibility(ScrollContainer::ScrollBarVisibility::NeverShow);
    TEST_ASSERT(scroll->GetVScrollBarVisibility() == ScrollContainer::ScrollBarVisibility::NeverShow,
                "Vertical scrollbar visibility NeverShow round-trips");

    math::Color barColor(0.3f, 0.3f, 0.3f, 1.0f);
    scroll->SetScrollBarColor(barColor);
    TEST_ASSERT(ColorEq(scroll->GetScrollBarColor(), barColor), "Scrollbar color round-trips");

    math::Color barBg(0.1f, 0.1f, 0.1f, 0.5f);
    scroll->SetScrollBarBackgroundColor(barBg);
    TEST_ASSERT(ColorEq(scroll->GetScrollBarBackgroundColor(), barBg),
                "Scrollbar background color round-trips");

    scroll->SetHScroll(25.0f);
    TEST_ASSERT(FloatEq(scroll->GetHScroll(), 25.0f) || FloatEq(scroll->GetHScroll(), 0.0f),
                "Horizontal scroll offset is stored or clamped to a valid range");
    scroll->SetVScroll(0.0f);
    TEST_ASSERT(FloatEq(scroll->GetVScroll(), 0.0f), "Vertical scroll offset of zero round-trips");

    TEST_ASSERT(Vec2Eq(scroll->GetContentSize(), math::Vec2(0.0f, 0.0f)),
                "Content size defaults to zero before layout");

    TEST_ASSERT(scroll->GetTypeName() == "ScrollContainer", "Type name is ScrollContainer");

    return true;
}

// ============================================================
// StyleBox / StyleBoxFlat
// ============================================================

bool TestStyleBoxContentMargins() {
    TEST_SECTION("StyleBox: Content Margins");

    std::shared_ptr<StyleBoxFlat> style = std::make_shared<StyleBoxFlat>();

    style->SetContentMarginLeft(2.0f);
    style->SetContentMarginRight(3.0f);
    style->SetContentMarginTop(4.0f);
    style->SetContentMarginBottom(5.0f);
    TEST_ASSERT(FloatEq(style->GetContentMarginLeft(), 2.0f), "Content margin left round-trips");
    TEST_ASSERT(FloatEq(style->GetContentMarginRight(), 3.0f), "Content margin right round-trips");
    TEST_ASSERT(FloatEq(style->GetContentMarginTop(), 4.0f), "Content margin top round-trips");
    TEST_ASSERT(FloatEq(style->GetContentMarginBottom(), 5.0f), "Content margin bottom round-trips");

    style->SetContentMarginAll(9.0f);
    TEST_ASSERT(FloatEq(style->GetContentMarginLeft(), 9.0f) &&
                FloatEq(style->GetContentMarginRight(), 9.0f) &&
                FloatEq(style->GetContentMarginTop(), 9.0f) &&
                FloatEq(style->GetContentMarginBottom(), 9.0f),
                "SetContentMarginAll sets all four margins");

    return true;
}

bool TestStyleBoxFlatBackground() {
    TEST_SECTION("StyleBoxFlat: Background");

    std::shared_ptr<StyleBoxFlat> style = std::make_shared<StyleBoxFlat>();

    TEST_ASSERT(style->GetTypeName() == "StyleBoxFlat", "Type name is StyleBoxFlat");

    math::Color bg(0.1f, 0.2f, 0.3f, 0.4f);
    style->SetBackgroundColor(bg);
    TEST_ASSERT(ColorEq(style->GetBackgroundColor(), bg), "Background color round-trips");

    style->SetOpacity(0.5f);
    TEST_ASSERT(FloatEq(style->GetOpacity(), 0.5f), "Opacity round-trips");
    style->SetOpacity(2.0f);
    TEST_ASSERT(FloatEq(style->GetOpacity(), 1.0f), "Opacity clamps to 1.0 above range");
    style->SetOpacity(-1.0f);
    TEST_ASSERT(FloatEq(style->GetOpacity(), 0.0f), "Opacity clamps to 0.0 below range");

    return true;
}

bool TestStyleBoxFlatBorder() {
    TEST_SECTION("StyleBoxFlat: Border Width & Color");

    std::shared_ptr<StyleBoxFlat> style = std::make_shared<StyleBoxFlat>();

    style->SetBorderWidthLinked(false);
    TEST_ASSERT(!style->GetBorderWidthLinked(), "Border width linked flag clears");

    style->SetBorderWidthLeft(1.0f);
    style->SetBorderWidthRight(2.0f);
    style->SetBorderWidthTop(3.0f);
    style->SetBorderWidthBottom(4.0f);
    TEST_ASSERT(FloatEq(style->GetBorderWidthLeft(), 1.0f), "Border width left round-trips");
    TEST_ASSERT(FloatEq(style->GetBorderWidthRight(), 2.0f), "Border width right round-trips");
    TEST_ASSERT(FloatEq(style->GetBorderWidthTop(), 3.0f), "Border width top round-trips");
    TEST_ASSERT(FloatEq(style->GetBorderWidthBottom(), 4.0f), "Border width bottom round-trips");

    style->SetBorderWidthAll(6.0f);
    TEST_ASSERT(FloatEq(style->GetBorderWidthLeft(), 6.0f) &&
                FloatEq(style->GetBorderWidthBottom(), 6.0f),
                "SetBorderWidthAll sets all border widths");

    style->SetBorderColorLinked(false);
    TEST_ASSERT(!style->GetBorderColorLinked(), "Border color linked flag clears");

    math::Color cl(1.0f, 0.0f, 0.0f, 1.0f);
    math::Color cr(0.0f, 1.0f, 0.0f, 1.0f);
    math::Color ct(0.0f, 0.0f, 1.0f, 1.0f);
    math::Color cb(1.0f, 1.0f, 0.0f, 1.0f);
    style->SetBorderColorLeft(cl);
    style->SetBorderColorRight(cr);
    style->SetBorderColorTop(ct);
    style->SetBorderColorBottom(cb);
    TEST_ASSERT(ColorEq(style->GetBorderColorLeft(), cl), "Border color left round-trips");
    TEST_ASSERT(ColorEq(style->GetBorderColorRight(), cr), "Border color right round-trips");
    TEST_ASSERT(ColorEq(style->GetBorderColorTop(), ct), "Border color top round-trips");
    TEST_ASSERT(ColorEq(style->GetBorderColorBottom(), cb), "Border color bottom round-trips");

    math::Color all(0.5f, 0.5f, 0.5f, 1.0f);
    style->SetBorderColorAll(all);
    TEST_ASSERT(ColorEq(style->GetBorderColorLeft(), all) && ColorEq(style->GetBorderColorBottom(), all),
                "SetBorderColorAll sets all border colors");

    return true;
}

bool TestStyleBoxFlatCorners() {
    TEST_SECTION("StyleBoxFlat: Corner Radius & Detail");

    std::shared_ptr<StyleBoxFlat> style = std::make_shared<StyleBoxFlat>();

    style->SetCornerRadiusLinked(false);
    TEST_ASSERT(!style->GetCornerRadiusLinked(), "Corner radius linked flag clears");

    style->SetCornerRadiusTopLeft(5.0f);
    style->SetCornerRadiusTopRight(6.0f);
    style->SetCornerRadiusBottomLeft(7.0f);
    style->SetCornerRadiusBottomRight(8.0f);
    TEST_ASSERT(FloatEq(style->GetCornerRadiusTopLeft(), 5.0f), "Corner radius top-left round-trips");
    TEST_ASSERT(FloatEq(style->GetCornerRadiusTopRight(), 6.0f), "Corner radius top-right round-trips");
    TEST_ASSERT(FloatEq(style->GetCornerRadiusBottomLeft(), 7.0f), "Corner radius bottom-left round-trips");
    TEST_ASSERT(FloatEq(style->GetCornerRadiusBottomRight(), 8.0f), "Corner radius bottom-right round-trips");

    style->SetCornerRadiusAll(12.0f);
    TEST_ASSERT(FloatEq(style->GetCornerRadiusTopLeft(), 12.0f) &&
                FloatEq(style->GetCornerRadiusBottomRight(), 12.0f),
                "SetCornerRadiusAll sets all corner radii");

    style->SetCornerDetail(16);
    TEST_ASSERT(style->GetCornerDetail() == 16, "Corner detail round-trips");
    style->SetCornerDetail(100);
    TEST_ASSERT(style->GetCornerDetail() == 32, "Corner detail clamps to 32 above range");
    style->SetCornerDetail(0);
    TEST_ASSERT(style->GetCornerDetail() == 1, "Corner detail clamps to 1 below range");

    style->SetAntiAliasing(false);
    TEST_ASSERT(!style->GetAntiAliasing(), "Anti-aliasing flag clears");
    style->SetAntiAliasingSize(2.0f);
    TEST_ASSERT(FloatEq(style->GetAntiAliasingSize(), 2.0f), "Anti-aliasing size round-trips");

    return true;
}

bool TestStyleBoxFlatExpandAndShadow() {
    TEST_SECTION("StyleBoxFlat: Expand Margins & Shadow");

    std::shared_ptr<StyleBoxFlat> style = std::make_shared<StyleBoxFlat>();

    style->SetExpandMarginLeft(1.0f);
    style->SetExpandMarginRight(2.0f);
    style->SetExpandMarginTop(3.0f);
    style->SetExpandMarginBottom(4.0f);
    TEST_ASSERT(FloatEq(style->GetExpandMarginLeft(), 1.0f), "Expand margin left round-trips");
    TEST_ASSERT(FloatEq(style->GetExpandMarginRight(), 2.0f), "Expand margin right round-trips");
    TEST_ASSERT(FloatEq(style->GetExpandMarginTop(), 3.0f), "Expand margin top round-trips");
    TEST_ASSERT(FloatEq(style->GetExpandMarginBottom(), 4.0f), "Expand margin bottom round-trips");

    style->SetExpandMarginAll(10.0f);
    TEST_ASSERT(FloatEq(style->GetExpandMarginLeft(), 10.0f) &&
                FloatEq(style->GetExpandMarginBottom(), 10.0f),
                "SetExpandMarginAll sets all expand margins");

    style->SetShadowEnabled(true);
    TEST_ASSERT(style->GetShadowEnabled(), "Shadow enabled flag round-trips");

    math::Color shadow(0.0f, 0.0f, 0.0f, 0.5f);
    style->SetShadowColor(shadow);
    TEST_ASSERT(ColorEq(style->GetShadowColor(), shadow), "Shadow color round-trips");

    style->SetShadowSize(8.0f);
    TEST_ASSERT(FloatEq(style->GetShadowSize(), 8.0f), "Shadow size round-trips");

    style->SetShadowOffset(math::Vec2(3.0f, -2.0f));
    TEST_ASSERT(Vec2Eq(style->GetShadowOffset(), math::Vec2(3.0f, -2.0f)), "Shadow offset round-trips");

    style->SetCustomShaderPath("res://shaders/panel.lsh");
    TEST_ASSERT(style->GetCustomShaderPath() == "res://shaders/panel.lsh",
                "Custom shader path round-trips");

    return true;
}

bool TestStyleBoxFlatCloneAndSerialize() {
    TEST_SECTION("StyleBoxFlat: Clone & Serialization");

    std::shared_ptr<StyleBoxFlat> style = std::make_shared<StyleBoxFlat>();
    style->SetBackgroundColor(math::Color(0.7f, 0.6f, 0.5f, 0.9f));
    style->SetCornerRadiusLinked(false);
    style->SetCornerRadiusTopLeft(11.0f);
    style->SetBorderWidthLinked(false);
    style->SetBorderWidthTop(2.0f);
    style->SetShadowEnabled(true);

    std::shared_ptr<StyleBox> clone = style->Clone();
    TEST_ASSERT(clone != nullptr, "Clone returns a non-null StyleBox");
    TEST_ASSERT(clone->GetTypeName() == "StyleBoxFlat", "Clone preserves the type name");
    std::shared_ptr<StyleBoxFlat> flatClone = std::dynamic_pointer_cast<StyleBoxFlat>(clone);
    TEST_ASSERT(flatClone != nullptr, "Clone is a StyleBoxFlat");
    TEST_ASSERT(ColorEq(flatClone->GetBackgroundColor(), math::Color(0.7f, 0.6f, 0.5f, 0.9f)),
                "Clone preserves background color");
    TEST_ASSERT(FloatEq(flatClone->GetCornerRadiusTopLeft(), 11.0f),
                "Clone preserves corner radius");

    nlohmann::json json = style->Serialize();
    TEST_ASSERT(json.is_object(), "Serialize produces a JSON object");

    std::shared_ptr<StyleBoxFlat> restored = std::make_shared<StyleBoxFlat>();
    restored->Deserialize(json);
    TEST_ASSERT(ColorEq(restored->GetBackgroundColor(), math::Color(0.7f, 0.6f, 0.5f, 0.9f)),
                "Deserialize restores background color");
    TEST_ASSERT(FloatEq(restored->GetCornerRadiusTopLeft(), 11.0f),
                "Deserialize restores corner radius top-left");
    TEST_ASSERT(FloatEq(restored->GetBorderWidthTop(), 2.0f),
                "Deserialize restores border width top");
    TEST_ASSERT(restored->GetShadowEnabled(), "Deserialize restores shadow enabled flag");

    return true;
}

// ============================================================
// UIImageDraw helpers
// ============================================================

bool TestUIImageEnums() {
    TEST_SECTION("UIImageDraw: Enum Conversions & Defaults");

    TEST_ASSERT(UIImageStretchModeFromInt(0) == UIImageStretchMode::Stretch,
                "Stretch mode int 0 maps to Stretch");
    TEST_ASSERT(UIImageStretchModeFromInt(1) == UIImageStretchMode::KeepCentered,
                "Stretch mode int 1 maps to KeepCentered");
    TEST_ASSERT(UIImageStretchModeFromInt(2) == UIImageStretchMode::NineSlice,
                "Stretch mode int 2 maps to NineSlice");

    TEST_ASSERT(std::strcmp(UIImageStretchModeToString(UIImageStretchMode::NineSlice), "") != 0,
                "Stretch mode to-string yields a non-empty label");

    TEST_ASSERT(UINineSliceAxisModeFromInt(0) == UINineSliceAxisMode::Stretch,
                "Axis mode int 0 maps to Stretch");
    TEST_ASSERT(UINineSliceAxisModeFromInt(1) == UINineSliceAxisMode::Tile,
                "Axis mode int 1 maps to Tile");

    TEST_ASSERT(std::strcmp(UINineSliceAxisModeToString(UINineSliceAxisMode::Tile), "") != 0,
                "Axis mode to-string yields a non-empty label");

    UINineSlice slice;
    TEST_ASSERT(FloatEq(slice.marginLeft, 0.0f) && FloatEq(slice.marginTop, 0.0f) &&
                FloatEq(slice.marginRight, 0.0f) && FloatEq(slice.marginBottom, 0.0f),
                "UINineSlice margins default to zero");
    TEST_ASSERT(slice.axisHorizontal == UINineSliceAxisMode::Stretch &&
                slice.axisVertical == UINineSliceAxisMode::Stretch,
                "UINineSlice axis modes default to Stretch");
    TEST_ASSERT(slice.drawCenter, "UINineSlice draws the center region by default");

    slice.marginLeft = 8.0f;
    slice.axisHorizontal = UINineSliceAxisMode::Tile;
    slice.drawCenter = false;
    TEST_ASSERT(FloatEq(slice.marginLeft, 8.0f) &&
                slice.axisHorizontal == UINineSliceAxisMode::Tile && !slice.drawCenter,
                "UINineSlice fields are mutable and round-trip");

    return true;
}

// ============================================================
// Layout arrangement (the tests that would have caught the whole family)
// ============================================================
//
// Everything above this point is a property round-trip. That is exactly why an entire
// generation of layout bugs shipped: the containers stored what they were told and then
// arranged their children upside down, and no test ever looked at where a child ended up.
//
// These build a real Node2D tree, run the layout, and assert the CHILD RECTS.
//
// The single most important assertion in this file is "the first child is at the TOP".
// The 2D/UI canvas is Y-UP: math::Rect::position is the MINIMUM corner, so position.y is the
// BOTTOM edge, and half the container family was written as though it were the top. Every
// container therefore gets a first-child-is-at-the-top test.

// Build `count` children under `parent`, each a UIControl of the given size.
std::vector<std::shared_ptr<Node2D>> MakeChildren(const std::shared_ptr<Node2D>& parent,
                                                  int count, float width, float height) {
    std::vector<std::shared_ptr<Node2D>> children;
    for (int i = 0; i < count; ++i) {
        std::shared_ptr<Node2D> child = std::make_shared<Node2D>("child" + std::to_string(i));
        std::shared_ptr<UIControl> control = std::make_shared<UIControl>();
        control->RegisterProperties();
        control->SetWidth(width);
        control->SetHeight(height);
        child->AddComponent(control);
        parent->AddChild(child);
        children.push_back(child);
    }
    return children;
}

math::Rect ChildRect(const std::shared_ptr<Node2D>& child) {
    std::shared_ptr<UIControl> control = child->GetComponent<UIControl>();
    return control ? control->GetRect() : math::Rect();
}

// Attach `container` to a fresh Node2D at the origin, sized width x height.
template <typename ContainerT>
std::shared_ptr<ContainerT> MakeContainerOn(const std::shared_ptr<Node2D>& node,
                                            float width, float height) {
    std::shared_ptr<ContainerT> container = std::make_shared<ContainerT>();
    container->RegisterProperties();
    container->SetWidth(width);
    container->SetHeight(height);
    node->AddComponent(container);
    container->OnAwake();
    return container;
}

bool TestVerticalContainerArrangement() {
    TEST_SECTION("Layout: VerticalContainer arranges top-down");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("vbox");
    std::shared_ptr<VerticalContainer> vbox = MakeContainerOn<VerticalContainer>(node, 200.0f, 300.0f);
    vbox->SetSeparation(10.0f);

    std::vector<std::shared_ptr<Node2D>> children = MakeChildren(node, 3, 100.0f, 50.0f);
    vbox->ForceLayoutUpdate();

    const math::Rect first = ChildRect(children[0]);
    const math::Rect second = ChildRect(children[1]);
    const math::Rect third = ChildRect(children[2]);

    // The container spans y in [-150, +150] around the origin. Y-up: the TOP is +150.
    TEST_ASSERT(FloatEq(first.GetTopEdge(), 150.0f),
                "First child's top edge is the container's TOP edge (not the bottom)");
    TEST_ASSERT(first.GetTopEdge() > second.GetTopEdge() &&
                second.GetTopEdge() > third.GetTopEdge(),
                "Children stack DOWNWARD: each is below the last");
    TEST_ASSERT(FloatEq(second.GetTopEdge(), first.GetBottomEdge() - 10.0f),
                "Separation is applied between children");
    TEST_ASSERT(FloatEq(first.size.y, 50.0f), "Child keeps its authored height");

    return true;
}

bool TestHorizontalContainerArrangement() {
    TEST_SECTION("Layout: HorizontalContainer arranges left-to-right");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("hbox");
    std::shared_ptr<HorizontalContainer> hbox = MakeContainerOn<HorizontalContainer>(node, 500.0f, 200.0f);

    std::vector<std::shared_ptr<Node2D>> children = MakeChildren(node, 2, 100.0f, 30.0f);
    hbox->ForceLayoutUpdate();

    const math::Rect first = ChildRect(children[0]);
    const math::Rect second = ChildRect(children[1]);

    TEST_ASSERT(FloatEq(first.GetLeftEdge(), -250.0f),
                "First child starts at the container's left edge");
    TEST_ASSERT(second.GetLeftEdge() > first.GetLeftEdge(),
                "Children advance rightward");

    // Center alignment on the main axis used to be a silent no-op: the free space was
    // subtracted twice and the bracket came out identically zero, so two 100px buttons in a
    // 500px HBox stayed flush left.
    hbox->SetHorizontalAlignment(HorizontalContainer::HorizontalAlignment::Center);
    hbox->ForceLayoutUpdate();
    const math::Rect centered = ChildRect(children[0]);
    TEST_ASSERT(FloatEq(centered.GetLeftEdge(), -100.0f),
                "Center alignment actually centers the row (300px of slack, half on each side)");

    return true;
}

bool TestHorizontalContainerCrossAxisTopBottom() {
    TEST_SECTION("Layout: HorizontalContainer cross-axis Top really is the top");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("hbox");
    std::shared_ptr<HorizontalContainer> hbox = MakeContainerOn<HorizontalContainer>(node, 400.0f, 200.0f);

    std::vector<std::shared_ptr<Node2D>> children = MakeChildren(node, 1, 50.0f, 40.0f);

    // The child must not fill, or there is no cross-axis slack to reveal the flip.
    children[0]->GetComponent<UIControl>()->SetSizeFlagsVertical(SizeFlagBits::ShrinkCenter);

    hbox->SetVerticalAlignment(HorizontalContainer::VerticalAlignment::Top);
    hbox->ForceLayoutUpdate();
    // The container spans y in [-100, +100]. Top is +100.
    // With ShrinkCenter the CHILD's own flag wins over the container-wide fallback, so this
    // asserts the flag, then the fallback is checked with a flag-less child below.
    const math::Rect centered = ChildRect(children[0]);
    TEST_ASSERT(FloatEq(centered.GetCenter().y, 0.0f),
                "A child's own ShrinkCenter flag centers it on the cross axis");

    // A child expressing no preference falls back to the container-wide alignment. Godot's
    // reading-order naming: ShrinkBegin on the VERTICAL axis means the TOP.
    children[0]->GetComponent<UIControl>()->SetSizeFlagsVertical(SizeFlagBits::ShrinkBegin);
    hbox->ForceLayoutUpdate();
    const math::Rect top = ChildRect(children[0]);
    TEST_ASSERT(FloatEq(top.GetTopEdge(), 100.0f),
                "ShrinkBegin on the vertical axis pins the child to the TOP (not the bottom)");

    children[0]->GetComponent<UIControl>()->SetSizeFlagsVertical(SizeFlagBits::ShrinkEnd);
    hbox->ForceLayoutUpdate();
    const math::Rect bottom = ChildRect(children[0]);
    TEST_ASSERT(FloatEq(bottom.GetBottomEdge(), -100.0f),
                "ShrinkEnd on the vertical axis pins the child to the BOTTOM");

    return true;
}

bool TestContainerPaddingTopInsetsTheTop() {
    TEST_SECTION("Layout: paddingTop insets the TOP edge");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("vbox");
    std::shared_ptr<VerticalContainer> vbox = MakeContainerOn<VerticalContainer>(node, 200.0f, 300.0f);

    // paddingTop = 40 must put the gap ABOVE the first child. GetContentRect used to compute
    // its "top-left" as position - size/2 -- the BOTTOM-left on a Y-up canvas -- so paddingTop
    // inset the bottom edge and paddingBottom the top.
    vbox->SetPaddingTop(40.0f);

    std::vector<std::shared_ptr<Node2D>> children = MakeChildren(node, 1, 100.0f, 50.0f);
    vbox->ForceLayoutUpdate();

    const math::Rect first = ChildRect(children[0]);
    TEST_ASSERT(FloatEq(first.GetTopEdge(), 150.0f - 40.0f),
                "paddingTop pushes the first child DOWN from the top edge");

    return true;
}

bool TestGridContainerArrangement() {
    TEST_SECTION("Layout: GridContainer fills from the top-left");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("grid");
    std::shared_ptr<GridContainer> grid = MakeContainerOn<GridContainer>(node, 400.0f, 400.0f);
    grid->SetColumnCount(2);
    grid->SetHorizontalSpacing(0.0f);
    grid->SetVerticalSpacing(0.0f);
    grid->SetSizeChildren(true);

    std::vector<std::shared_ptr<Node2D>> children = MakeChildren(node, 4, 100.0f, 100.0f);
    grid->ForceLayoutUpdate();

    const math::Rect topLeft = ChildRect(children[0]);
    const math::Rect topRight = ChildRect(children[1]);
    const math::Rect bottomLeft = ChildRect(children[2]);

    TEST_ASSERT(FloatEq(topLeft.GetTopEdge(), 200.0f),
                "Cell (0,0) is at the container's TOP edge");
    TEST_ASSERT(FloatEq(topLeft.GetLeftEdge(), -200.0f),
                "Cell (0,0) is at the container's LEFT edge");
    TEST_ASSERT(topRight.GetLeftEdge() > topLeft.GetLeftEdge(),
                "Column 1 is to the right of column 0");
    TEST_ASSERT(bottomLeft.GetTopEdge() < topLeft.GetTopEdge(),
                "Row 1 is BELOW row 0");

    return true;
}

bool TestWrapArrangement() {
    TEST_SECTION("Layout: Wrap lines march DOWN, and maxLines is not off by one");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("wrap");
    std::shared_ptr<Wrap> wrap = MakeContainerOn<Wrap>(node, 250.0f, 400.0f);
    wrap->SetSpacingX(0.0f);
    wrap->SetSpacingY(0.0f);

    // Three 100px children in a 250px-wide box: two fit on the first line, the third wraps.
    std::vector<std::shared_ptr<Node2D>> children = MakeChildren(node, 3, 100.0f, 50.0f);
    wrap->ForceLayoutUpdate();

    const math::Rect first = ChildRect(children[0]);
    const math::Rect second = ChildRect(children[1]);
    const math::Rect third = ChildRect(children[2]);

    TEST_ASSERT(FloatEq(first.GetTopEdge(), 200.0f),
                "The first line starts at the container's TOP edge");
    TEST_ASSERT(FloatEq(second.GetTopEdge(), first.GetTopEdge()),
                "The second child shares the first line");
    // The old code started at the BOTTOM and then ADDED the line height to advance, so line 2
    // marched UP and out through the top of the container.
    TEST_ASSERT(third.GetTopEdge() < first.GetTopEdge(),
                "The wrapped line is BELOW the first, not above it");

    // maxLines = 1 must yield exactly one line. The old code checked the cap before pushing
    // the in-progress line and then pushed it anyway, so maxLines = N produced N + 1 lines.
    wrap->SetMaxLines(1);
    wrap->ForceLayoutUpdate();
    const math::Rect capped = ChildRect(children[2]);
    TEST_ASSERT(FloatEq(capped.size.x, 0.0f) && FloatEq(capped.size.y, 0.0f),
                "maxLines = 1 keeps exactly one line; the overflow child is collapsed");

    return true;
}

bool TestDockContainerArrangement() {
    TEST_SECTION("Layout: DockContainer Top docks along the TOP edge");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("dock");
    std::shared_ptr<DockContainer> dock = MakeContainerOn<DockContainer>(node, 400.0f, 400.0f);
    dock->SetDockSpacing(0.0f);

    std::vector<std::shared_ptr<Node2D>> children = MakeChildren(node, 1, 100.0f, 60.0f);

    // Per-child dock side, through the LayoutSlot attached property. GetChildDockSide used to
    // ignore the child entirely and return the container-wide default for every one of them.
    std::shared_ptr<LayoutSlot> slot = std::make_shared<LayoutSlot>();
    slot->RegisterProperties();
    slot->SetDockSide(LayoutSlot::DockSide::Top);
    children[0]->AddComponent(slot);

    dock->ForceLayoutUpdate();

    const math::Rect docked = ChildRect(children[0]);
    TEST_ASSERT(FloatEq(docked.GetTopEdge(), 200.0f),
                "DockSide::Top places the child at the container's TOP edge");
    TEST_ASSERT(docked.size.x > 0.0f && docked.size.y > 0.0f,
                "A docked child has a positive rect (the available rect never goes negative)");

    return true;
}

bool TestExpandDoesNotRatchet() {
    TEST_SECTION("Layout: an expanding child can shrink again (no ratchet)");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("vbox");
    std::shared_ptr<VerticalContainer> vbox = MakeContainerOn<VerticalContainer>(node, 200.0f, 600.0f);

    std::vector<std::shared_ptr<Node2D>> children = MakeChildren(node, 1, 100.0f, 100.0f);
    std::shared_ptr<UIControl> child = children[0]->GetComponent<UIControl>();
    child->SetSizeFlagsVertical(SizeFlagBits::Expand | SizeFlagBits::Fill);

    vbox->ForceLayoutUpdate();
    TEST_ASSERT(FloatEq(ChildRect(children[0]).size.y, 600.0f),
                "The expanding child fills the 600px container");

    // The authored height must be untouched: the arranged size used to be written back into
    // the child's height PROPERTY, so it was re-read as the base size next frame (the child
    // could grow but never shrink) and the transient layout output was serialized into the
    // saved scene.
    TEST_ASSERT(FloatEq(child->GetHeight(), 100.0f),
                "The arranged size is NOT written back into the authored height property");

    // Shrink the container. The child must follow it down, not stay inflated.
    vbox->SetHeight(200.0f);
    vbox->ForceLayoutUpdate();
    TEST_ASSERT(FloatEq(ChildRect(children[0]).size.y, 200.0f),
                "Shrinking the container shrinks the expanded child back down");

    return true;
}

bool TestExpandRatioSplitsTotal() {
    TEST_SECTION("Layout: expand ratios split the TOTAL, Godot-style");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("hbox");
    std::shared_ptr<HorizontalContainer> hbox = MakeContainerOn<HorizontalContainer>(node, 400.0f, 100.0f);

    std::vector<std::shared_ptr<Node2D>> children = MakeChildren(node, 2, 100.0f, 50.0f);
    for (int i = 0; i < 2; ++i) {
        std::shared_ptr<UIControl> control = children[i]->GetComponent<UIControl>();
        control->SetSizeFlagsHorizontal(SizeFlagBits::Expand | SizeFlagBits::Fill);
    }
    children[1]->GetComponent<UIControl>()->SetSizeFlagsStretchRatio(3.0f);

    hbox->ForceLayoutUpdate();

    // Two children with a desired 100 and ratios 1:3 in a 400px box. Godot's model splits the
    // POOL (their own extents plus the free space) by ratio: 100/300. Splitting only the
    // leftover -- which is what the old code did -- would give 150/250.
    TEST_ASSERT(FloatEq(ChildRect(children[0]).size.x, 100.0f),
                "Ratio 1 of 1:3 in a 400px row gets 100px");
    TEST_ASSERT(FloatEq(ChildRect(children[1]).size.x, 300.0f),
                "Ratio 3 of 1:3 in a 400px row gets 300px");

    return true;
}

bool TestSizeFlagsFillIsHonored() {
    TEST_SECTION("Layout: per-child size flags are honored on the cross axis");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("hbox");
    std::shared_ptr<HorizontalContainer> hbox = MakeContainerOn<HorizontalContainer>(node, 400.0f, 200.0f);

    std::vector<std::shared_ptr<Node2D>> children = MakeChildren(node, 1, 100.0f, 30.0f);
    std::shared_ptr<UIControl> child = children[0]->GetComponent<UIControl>();

    // sizeFlagsVertical = Fill on a child in an HBox. ChildWantsFill() existed but had ZERO
    // call sites repo-wide, so this did nothing and the only way to fill was a container-wide
    // enum that then force-filled every child alike.
    child->SetSizeFlagsVertical(SizeFlagBits::Fill);
    hbox->ForceLayoutUpdate();

    TEST_ASSERT(FloatEq(ChildRect(children[0]).size.y, 200.0f),
                "A child with sizeFlagsVertical = Fill fills the row's height");

    child->SetSizeFlagsVertical(SizeFlagBits::ShrinkCenter);
    hbox->ForceLayoutUpdate();
    TEST_ASSERT(FloatEq(ChildRect(children[0]).size.y, 30.0f),
                "ShrinkCenter keeps the child's own height");

    return true;
}

bool TestDistributedSpacing() {
    TEST_SECTION("Layout: space-between / space-around / space-evenly");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("hbox");
    std::shared_ptr<HorizontalContainer> hbox = MakeContainerOn<HorizontalContainer>(node, 400.0f, 100.0f);

    // Two 100px children in a 400px row: 200px of slack.
    std::vector<std::shared_ptr<Node2D>> children = MakeChildren(node, 2, 100.0f, 50.0f);

    hbox->SetHorizontalAlignment(HorizontalContainer::HorizontalAlignment::SpaceBetween);
    hbox->ForceLayoutUpdate();
    TEST_ASSERT(FloatEq(ChildRect(children[0]).GetLeftEdge(), -200.0f) &&
                FloatEq(ChildRect(children[1]).GetRightEdge(), 200.0f),
                "SpaceBetween pins the first child left and the last child right");

    hbox->SetHorizontalAlignment(HorizontalContainer::HorizontalAlignment::SpaceEvenly);
    hbox->ForceLayoutUpdate();
    // Three equal gaps of 200/3 each.
    TEST_ASSERT(FloatEq(ChildRect(children[0]).GetLeftEdge(), -200.0f + 200.0f / 3.0f),
                "SpaceEvenly makes the end gaps equal to the inter-child gap");

    hbox->SetHorizontalAlignment(HorizontalContainer::HorizontalAlignment::SpaceAround);
    hbox->ForceLayoutUpdate();
    // Each child gets 100 of space around it, so the end gaps are 50.
    TEST_ASSERT(FloatEq(ChildRect(children[0]).GetLeftEdge(), -150.0f),
                "SpaceAround gives the ends half the inter-child gap");

    return true;
}

bool TestSplitContainerArrangement() {
    TEST_SECTION("Layout: SplitContainer splits into two panes");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("split");
    std::shared_ptr<SplitContainer> split = MakeContainerOn<SplitContainer>(node, 400.0f, 200.0f);
    split->SetSplitterWidth(10.0f);
    split->SetOrientation(SplitContainer::Orientation::Horizontal);

    std::vector<std::shared_ptr<Node2D>> children = MakeChildren(node, 2, 10.0f, 10.0f);
    split->ForceLayoutUpdate();

    const math::Rect left = ChildRect(children[0]);
    const math::Rect right = ChildRect(children[1]);

    TEST_ASSERT(FloatEq(left.GetLeftEdge(), -200.0f), "The first pane starts at the left edge");
    TEST_ASSERT(FloatEq(right.GetRightEdge(), 200.0f), "The second pane ends at the right edge");
    TEST_ASSERT(FloatEq(right.GetLeftEdge() - left.GetRightEdge(), 10.0f),
                "The splitter occupies exactly splitterWidth between the panes");

    return true;
}

bool TestAspectRatioContainerArrangement() {
    TEST_SECTION("Layout: AspectRatioContainer holds its ratio");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("aspect");
    std::shared_ptr<AspectRatioContainer> aspect =
        MakeContainerOn<AspectRatioContainer>(node, 400.0f, 400.0f);
    aspect->SetRatio(2.0f);   // 2:1
    aspect->SetStretchMode(AspectRatioContainer::StretchMode::Fit);

    std::vector<std::shared_ptr<Node2D>> children = MakeChildren(node, 1, 50.0f, 50.0f);
    aspect->ForceLayoutUpdate();

    const math::Rect fitted = ChildRect(children[0]);
    TEST_ASSERT(FloatEq(fitted.size.x, 400.0f) && FloatEq(fitted.size.y, 200.0f),
                "Fit produces the largest 2:1 box inside a 400x400 area");
    TEST_ASSERT(FloatEq(fitted.GetCenter().y, 0.0f),
                "The fitted box is centered vertically (letterboxed)");

    return true;
}

bool TestChildMarginReservesSpace() {
    TEST_SECTION("Layout: a child container's margin reserves outer space");

    std::shared_ptr<Node2D> node = std::make_shared<Node2D>("vbox");
    std::shared_ptr<VerticalContainer> vbox = MakeContainerOn<VerticalContainer>(node, 200.0f, 300.0f);

    // The child is itself a Container, because `margin` is a Container property.
    std::shared_ptr<Node2D> childNode = std::make_shared<Node2D>("panel");
    std::shared_ptr<Container> child = std::make_shared<Container>();
    child->RegisterProperties();
    child->SetWidth(100.0f);
    child->SetHeight(50.0f);
    child->SetMarginTop(20.0f);
    childNode->AddComponent(child);
    vbox->GetOwner()->AddChild(childNode);

    vbox->ForceLayoutUpdate();

    // margin used to affect NOTHING in layout: it only inflated getWorldBounds(), which made
    // the editor's selection box bigger than the drawn panel and picked up stray clicks.
    TEST_ASSERT(FloatEq(child->GetRect().GetTopEdge(), 150.0f - 20.0f),
                "marginTop pushes the child down from the container's top edge");

    return true;
}

} // namespace

void RunUIContainerComponentTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "UI CONTAINER COMPONENT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("UIContainerComponents");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestUIControlSize();
    allPassed &= TestUIControlMinMaxSize();
    allPassed &= TestUIControlLayoutMode();
    allPassed &= TestUIControlAnchorsOffsets();
    allPassed &= TestUIControlAnchorPresets();
    allPassed &= TestUIControlPivotGrow();
    allPassed &= TestUIControlSizeFlags();
    allPassed &= TestUIControlUISpace();
    allPassed &= TestUIControlRectState();
    allPassed &= TestUIControlTheme();
    allPassed &= TestUIControlFocus();
    allPassed &= TestUIControlSerialization();
    allPassed &= TestContainerSize();
    allPassed &= TestContainerPadding();
    allPassed &= TestContainerMargin();
    allPassed &= TestContainerAppearance();
    allPassed &= TestContainerBorder();
    allPassed &= TestContainerCornerRadius();
    allPassed &= TestContainerLayoutProps();
    allPassed &= TestPaddingContainer();
    allPassed &= TestCenterContainer();
    allPassed &= TestHorizontalContainer();
    allPassed &= TestVerticalContainer();
    allPassed &= TestGridContainer();
    allPassed &= TestDockContainer();
    allPassed &= TestStack();
    allPassed &= TestWrap();
    allPassed &= TestSpacer();
    allPassed &= TestScrollContainer();
    allPassed &= TestStyleBoxContentMargins();
    allPassed &= TestStyleBoxFlatBackground();
    allPassed &= TestStyleBoxFlatBorder();
    allPassed &= TestStyleBoxFlatCorners();
    allPassed &= TestStyleBoxFlatExpandAndShadow();
    allPassed &= TestStyleBoxFlatCloneAndSerialize();
    allPassed &= TestUIImageEnums();

    // Layout ARRANGEMENT -- these drive a real node tree and assert where children land.
    allPassed &= TestVerticalContainerArrangement();
    allPassed &= TestHorizontalContainerArrangement();
    allPassed &= TestHorizontalContainerCrossAxisTopBottom();
    allPassed &= TestContainerPaddingTopInsetsTheTop();
    allPassed &= TestGridContainerArrangement();
    allPassed &= TestWrapArrangement();
    allPassed &= TestDockContainerArrangement();
    allPassed &= TestExpandDoesNotRatchet();
    allPassed &= TestExpandRatioSplitsTotal();
    allPassed &= TestSizeFlagsFillIsHonored();
    allPassed &= TestDistributedSpacing();
    allPassed &= TestSplitContainerArrangement();
    allPassed &= TestAspectRatioContainerArrangement();
    allPassed &= TestChildMarginReservesSpace();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL UI CONTAINER COMPONENT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}

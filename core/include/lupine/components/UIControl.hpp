#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/math/Rect.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/rendering/DrawCommand.hpp"
#include "lupine/input/InputCodes.hpp"
#include <string>
#include <vector>
#include <set>
#include <memory>
#include <unordered_map>
#include <cstdint>

namespace lupine {
namespace ui {
class ThemeAsset;
struct ThemeImage;
}
namespace components {

class StyleBox;

/**
 * Anchor presets, mirroring Godot's Control anchor presets. Selecting a preset
 * writes anchorMin/anchorMax (and offsets) and switches the control into
 * Anchors layout mode. "Custom" is set automatically when anchors are edited
 * directly and is never applied as a preset.
 */
enum class AnchorPreset {
    TopLeft = 0,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    LeftWide,
    TopWide,
    RightWide,
    BottomWide,
    VCenterWide,
    HCenterWide,
    FullRect,
    Custom
};

/**
 * How a control's rect is determined.
 *  - Position: the rect follows the owning Node2D's position (free placement).
 *    This is the legacy/default behaviour and reproduces pre-UIControl scenes.
 *  - Anchors: the rect is computed from anchorMin/anchorMax + offsetMin/offsetMax
 *    relative to the parent control's rect (or the canvas at the root).
 * A control whose immediate parent is a layout container is "container-driven":
 * the container assigns its rect and both modes above are bypassed.
 */
enum class LayoutMode {
    Position = 0,
    Anchors
};

/**
 * Direction in which a control grows when its minimum size exceeds the space
 * granted by its anchors/offsets (per axis).
 */
enum class GrowDirection {
    Begin = 0,
    End,
    Both
};

/**
 * How a control participates in keyboard/gamepad focus (Godot `Control.FocusMode`
 * parity).
 *  - None: the control can never hold focus (decorative panels, labels). It is
 *    skipped as a navigation target and never returned by neighbor/next/prev queries.
 *  - Click: the control accepts focus when clicked (and can be focused
 *    programmatically) but is skipped by directional/tab traversal.
 *  - All: the control accepts focus from clicks AND from directional neighbor /
 *    next/previous traversal.
 */
enum class FocusMode {
    None = 0,
    Click,
    All
};

/**
 * How a control reacts to the mouse cursor (Godot `Control.MouseFilter` parity).
 *  - Stop: the control handles the mouse itself (hover/press/click signals) AND is
 *    opaque to the cursor: it takes part in pointer arbitration, so controls behind
 *    it never see the click.
 *  - Propagate: the control handles the mouse itself but is NOT opaque: controls
 *    behind it still receive the same click ("pass" semantics).
 *  - Ignore: the control is invisible to the cursor. It reports no mouse state, emits
 *    no mouse signals, and never blocks controls behind it.
 *
 * Only controls that register the property (DefineMouseFilterProperty) are governed
 * by it; controls that hard-code ConsumesPointerInput() (the button family) keep
 * their own behaviour. Controls without the property read as Ignore.
 */
enum class MouseFilter {
    Stop = 0,
    Propagate,
    Ignore
};

/**
 * The four directional sides used by neighbor focus traversal. The integer values
 * match the order the scripting/C-API surfaces expect (left, top, right, bottom).
 */
enum class FocusSide {
    Left = 0,
    Top,
    Right,
    Bottom
};

/**
 * Container size-flag bits (Godot semantics). Stored as an int bitmask so
 * combinations such as Fill|Expand serialize cleanly and remain scriptable.
 * A value of 0 means "shrink to minimum, aligned to the begin edge".
 */
namespace SizeFlagBits {
    enum : int {
        ShrinkBegin  = 0,
        Fill         = 1 << 0,
        Expand       = 1 << 1,
        ShrinkCenter = 1 << 2,
        ShrinkEnd    = 1 << 3
    };
}

/**
 * UIControl - centralized base class for every UI element (the Godot `Control`
 * analog). It owns size, the unified Godot/Unity anchor model (anchorMin/Max +
 * offsetMin/Max + presets + pivot + grow direction), container size flags, the
 * UI-space flag, and the layout solver that resolves a control's on-screen rect
 * from its parent's rect.
 *
 * Concrete UI components derive from this and additionally implement
 * IRenderableComponent. Containers derive from this too (and override
 * IsLayoutContainer()/OnChildLayoutChanged()) so they are controls that also
 * arrange their children.
 *
 * Backward compatibility: the shared properties are registered through
 * DefineUIControlProperties(), which is parameterized with each component's
 * historical width/height defaults and its UI-space property name ("uiSpace" or
 * "useUISpace"), so existing scenes load byte-identically. The additive anchor
 * properties default to Position layout mode, which reproduces current placement.
 */
class UIControl : public core::Component {
public:
    UIControl();
    explicit UIControl(const std::string& name);
    virtual ~UIControl();

    std::string GetTypeName() const override { return "UIControl"; }

    // ========================================
    // Size
    // ========================================
    // GetWidth/GetHeight/GetSize are virtual so containers can report an
    // auto-computed size (e.g. SizeMode::FitChildren) for their own rect.
    virtual float GetWidth() const;
    void SetWidth(float width);
    virtual float GetHeight() const;
    void SetHeight(float height);
    virtual math::Vec2 GetSize() const;
    void SetSize(const math::Vec2& size);
    math::Vec2 GetCustomMinSize() const;
    void SetCustomMinSize(const math::Vec2& size);
    // Custom maximum size. A component of 0 (or less) means "no maximum" on that
    // axis; a positive value clamps the resolved size so the control never grows
    // past it (floored to the minimum size if the two conflict).
    math::Vec2 GetCustomMaxSize() const;
    void SetCustomMaxSize(const math::Vec2& size);

    // ========================================
    // Layout mode
    // ========================================
    LayoutMode GetLayoutMode() const;
    void SetLayoutMode(LayoutMode mode);

    // ========================================
    // Anchors / offsets (unified Godot + Unity model)
    // ========================================
    math::Vec2 GetAnchorMin() const;
    void SetAnchorMin(const math::Vec2& anchor);
    math::Vec2 GetAnchorMax() const;
    void SetAnchorMax(const math::Vec2& anchor);
    math::Vec2 GetOffsetMin() const;
    void SetOffsetMin(const math::Vec2& offset);
    math::Vec2 GetOffsetMax() const;
    void SetOffsetMax(const math::Vec2& offset);

    AnchorPreset GetAnchorPreset() const;
    void SetAnchorPreset(AnchorPreset preset);

    /**
     * Apply an anchor preset. When keepOffsets is true the current on-screen rect
     * is preserved (offsets recomputed); otherwise offsets snap so the control
     * occupies the preset's natural region (fill for wide presets, flush/centered
     * for point presets) using the current width/height. Switches to Anchors mode.
     */
    void ApplyAnchorPreset(AnchorPreset preset, bool keepOffsets = false);

    // ========================================
    // Content margins (the one text-inset model)
    // ========================================

    /**
     * How far a control's TEXT/content is inset from its own rect, per side, as
     * (top, right, bottom, left).
     *
     * There used to be three incompatible padding models and one of them was entirely dead:
     *   - Label had NO padding property at all;
     *   - Button had a Vec2 `textPadding` used only for auto-sizing, never to inset the
     *     drawn text;
     *   - LineEdit/TextEdit had a single SCALAR `padding` that was honored;
     *   - and StyleBox's four per-side content margins were serialized, cloned, unit-tested
     *     and editable in the theme editor, but had ZERO consumers -- authoring
     *     contentMarginLeft = 20 on a theme moved nothing.
     *
     * All four collapse here. The effective inset is the component-wise MAXIMUM of the
     * control's own padding property (whichever spelling it uses) and the content margins of
     * its themed StyleBox, so authoring either one works and neither silently zeroes the
     * other.
     *
     * `styleboxEntry` selects which themed stylebox to read (e.g. "normal" for a Button).
     */
    math::Vec4 GetContentMargins(const std::string& styleboxEntry = "normal") const;

    /**
     * `box` inset by GetContentMargins(). Canvas-oriented, so `top` really does come off the
     * top edge. Clamped to zero extent rather than inverting when the insets exceed the box.
     */
    math::Rect GetContentInsetRect(const math::Rect& box,
                                   const std::string& styleboxEntry = "normal") const;

    // ========================================
    // Which property actually drives an axis
    // ========================================

    /**
     * What a resize of this control on the given axis must actually write to.
     *
     * The three cases are genuinely different properties, and getting this wrong is the
     * single most confusing class of bug in the layout system: the editor accepts the drag
     * and NOTHING happens.
     *
     *   WidthHeight     -- the width/height property (Position mode, or a point-anchored
     *                      axis where anchorMin == anchorMax).
     *   Offsets         -- offsetMin/offsetMax. On an anchor-STRETCHED axis the extent is
     *                      (anchor span across the parent) + offsets, and ResolveLayout
     *                      never reads width/height at all. Dragging the scale handle on a
     *                      FullRect control therefore moved the handle and did nothing.
     *   ContainerDriven -- neither: a parent container owns the rect and overwrites it on
     *                      the next layout pass. Nothing the user edits here can stick.
     */
    enum class AxisDriver {
        WidthHeight,
        Offsets,
        ContainerDriven
    };

    AxisDriver GetAxisDriver(bool vertical) const;

    /**
     * Resize this control on one axis to `extent` pixels by writing whichever property
     * actually drives it. Returns false when the axis is not editable at all (the control
     * is container-driven), so callers can refuse the interaction instead of silently
     * swallowing it.
     */
    bool SetAxisExtent(bool vertical, float extent);

    /**
     * Gizmo scale for every UIControl. Resizes through SetAxisExtent, so it works on
     * anchor-stretched axes and correctly refuses when a container owns the rect.
     */
    bool OnGizmoScale(float scaleDelta, int axis, bool is3D) override;

    // ========================================
    // Grow direction
    // ========================================
    // There is no pivot: every UI control is center-pivoted by WriteRectToNode, and the
    // "pivot" property that used to sit here was never read by anything. See the note in
    // UIControl::DefineUIControlProperties.
    GrowDirection GetGrowDirectionH() const;
    void SetGrowDirectionH(GrowDirection dir);
    GrowDirection GetGrowDirectionV() const;
    void SetGrowDirectionV(GrowDirection dir);

    // ========================================
    // Container size flags
    // ========================================
    int GetSizeFlagsHorizontal() const;
    void SetSizeFlagsHorizontal(int flags);
    int GetSizeFlagsVertical() const;
    void SetSizeFlagsVertical(int flags);
    float GetSizeFlagsStretchRatio() const;
    void SetSizeFlagsStretchRatio(float ratio);

    // ========================================
    // UI space (CameraUI vs Camera2D render pass)
    // ========================================
    bool GetUISpace() const;
    void SetUISpace(bool uiSpace);
    // Backward-compatible aliases for components that historically used "useUISpace".
    bool GetUseUISpace() const { return GetUISpace(); }
    void SetUseUISpace(bool uiSpace) { SetUISpace(uiSpace); }

    // ========================================
    // Minimum size
    // ========================================
    /**
     * Content-driven minimum size (e.g. measured text). Override in derived
     * components. The default returns zero (no content contribution).
     *
     * Implementations must be pure with respect to layout: this is the MEASURE query
     * and a container calls it before arranging, so it must never depend on a rect the
     * container previously assigned (that is what creates a growth ratchet).
     */
    virtual math::Vec2 GetContentMinSize() const { return math::Vec2(0.0f, 0.0f); }

    /**
     * True if changing `propertyName` can change this control's CONTENT minimum size --
     * a Label's text or font, a Button's label, a nested container's padding, and so on.
     *
     * A parent container's arrangement depends on what its children MEASURE, so any such
     * property has to re-run the parent's layout. Only the pure *layout* properties
     * (width/height/offsets/size flags) used to do that, so a Label whose text changed at
     * runtime never re-laid-out its VerticalContainer and the row heights kept their
     * pre-change values indefinitely.
     *
     * The base implementation answers for the union of the content properties across the
     * control family. A name that a given control does not define simply never arrives
     * here, so the shared list costs nothing and cannot go stale the way sixteen per-class
     * lists would. Override to extend it for a control with a bespoke content property.
     *
     * See also NotifyContentSizeChanged(), for content changes that do not come through a
     * property at all -- an asynchronous font load completing being the important one.
     */
    virtual bool IsContentSizeProperty(const std::string& propertyName) const;

    /**
     * Announce that this control's content size changed for a reason other than a property
     * edit -- most importantly, an asynchronous font load completing, which changes every
     * measurement the control has already reported.
     */
    void NotifyContentSizeChanged() { InvalidateParentContainerLayout(); }

    /**
     * Content-driven minimum height when the control is constrained to `availableWidth`
     * (height-for-width). Text controls that wrap override this so a container can size
     * a row to the wrapped line count instead of a single line. The default ignores the
     * width and returns GetContentMinSize().y, which is correct for any control whose
     * height does not depend on its width.
     */
    virtual float GetContentMinHeightForWidth(float availableWidth) const {
        (void)availableWidth;
        return GetContentMinSize().y;
    }

    /**
     * Effective minimum size: component-wise max of customMinSize and content min.
     * Virtual so containers (whose minimum is driven by their children) report one
     * consistent number to both the anchor solver and a parent container.
     */
    virtual math::Vec2 GetMinSize() const;

    /**
     * Effective maximum size (the customMaxSize property). A component <= 0 means
     * "unbounded" on that axis. The solver and GetBoundsSize() clamp the resolved
     * size to this, but never below GetMinSize() (the minimum always wins).
     */
    virtual math::Vec2 GetMaxSize() const;

    /**
     * The control's INTRINSIC (desired) size: the authored width/height floored to
     * GetMinSize() and clamped to GetMaxSize(). This is the measure-pass query a
     * container uses to ask "how big do you want to be?".
     *
     * Deliberately independent of m_ResolvedRect: a container must never measure a
     * child by the size it previously arranged it to, or the child ratchets larger
     * every frame and can never shrink again.
     */
    math::Vec2 GetDesiredSize() const;

    // ========================================
    // Rect
    // ========================================
    /**
     * The control's resolved global rect in logical-canvas coordinates.
     *
     * The canvas is Y-UP and centered on the world origin: a WxH canvas spans
     * [-W/2, +W/2] x [-H/2, +H/2], anchor.y = 0 is the TOP edge, and
     * math::Rect::position is the MINIMUM corner -- i.e. the BOTTOM-left, not the
     * top-left. Use rect.GetTopEdge() / GetBottomEdge() / GetTopLeft() rather than
     * open-coding position.y arithmetic; see math/Rect.hpp.
     */
    math::Rect GetRect() const { return m_ResolvedRect; }
    const math::Rect& GetResolvedRect() const { return m_ResolvedRect; }

    /**
     * Effective size for world bounds / picking: the resolved (possibly
     * anchor-stretched) size, floored to the minimum size. Unlike GetSize() (the
     * requested width/height) this reflects anchor stretching and min/content size,
     * so selection boxes match what is actually rendered. Falls back to the requested
     * size if the control has not been resolved yet.
     */
    math::Vec2 GetBoundsSize() const;

    /**
     * Assign a global rect directly. Used by layout containers to place children:
     * marks the control container-driven, stores the rect as the resolved rect, and
     * updates the owning Node2D position.
     *
     * This deliberately does NOT write the width/height properties. Those are the
     * control's AUTHORED request and are the input to the next measure pass; writing
     * arranged output back into them makes a child grow every frame and never shrink
     * (a ratchet), and serializes transient layout output into the saved scene. The
     * arranged size lives in the resolved rect -- read it via GetRect()/GetBoundsSize().
     */
    void SetRect(const math::Rect& globalRect);

    void SetContainerDriven(bool driven) { m_ContainerDriven = driven; }
    bool IsContainerDriven() const { return m_ContainerDriven; }

    /**
     * True for container components that arrange their children. Containers
     * override this so children detect that they are container-driven.
     */
    virtual bool IsLayoutContainer() const { return false; }

    /**
     * Notification hook invoked on a parent control when a child's layout-relevant
     * property changes. Containers override to invalidate their layout.
     */
    virtual void OnChildLayoutChanged() {}

    // ========================================
    // Clipping (scissor)
    // ========================================
    /**
     * True if this control clips its descendants to GetClipRect(). The render
     * traversal pushes the clip while gathering this node's children, so the
     * control clips its descendants (not its own draws). Default: no clipping.
     */
    virtual bool ClipsDescendants() const { return false; }

    /**
     * The rectangle descendants are clipped to when ClipsDescendants() is true, in
     * global logical-canvas coordinates (Y-up, origin at the canvas center -- see
     * GetRect()). Defaults to the resolved rect; clipping containers typically
     * return their content rect.
     */
    virtual math::Rect GetClipRect() const { return m_ResolvedRect; }

    // ========================================
    // Solver
    // ========================================
    /**
     * Compute and apply this control's rect given its parent rect and the canvas
     * size. Container-driven controls are skipped (the container owns their rect).
     */
    void ResolveLayout(const math::Rect& parentRect, const math::Vec2& canvasSize);

    /**
     * Determine the parent rect (nearest ancestor UIControl, else the canvas) and
     * resolve. Re-evaluates container-driven status each call so re-parented
     * controls recover.
     */
    void ResolveSelf();

    /**
     * Bring this control's layout up to date. The base resolves the control's own rect;
     * containers additionally arrange their children.
     *
     * Invoked by LayoutPass() (and by OnUpdate at runtime). Never call this from
     * buildDrawCommands: layout writes node transforms, and doing that during rendering
     * is what let a child resolve against its parent's previous-frame rect.
     */
    virtual void PerformLayout();

    /**
     * Run the layout pass over a subtree, in PRE-ORDER so a parent's rect is final before
     * any of its children resolve against it. This is the single, explicit point at which
     * UI layout happens each frame.
     *
     * It runs from both the runtime update and the renderer's gather (which is what makes
     * the editor viewport lay out correctly -- the editor does not tick OnUpdate, and
     * layout used to be smuggled into buildDrawCommands to compensate). Idempotent:
     * containers only re-arrange when their layout is dirty.
     */
    static void LayoutPass(core::Node* root);

    // ========================================
    // Lifecycle
    // ========================================
    void OnUpdate(float deltaTime) override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    /**
     * Map the UI-space flag to a render spatial type. Derived components return
     * this from getSpatialType().
     */
    SpatialType GetUISpatialType() const;

    /**
     * Mouse position in this control's coordinate space (logical canvas units, the
     * same space node global positions use). Convenience for hit-testing and drag
     * handling in interactive UI components.
     */
    math::Vec2 GetCanvasMousePosition() const;

    // ========================================
    // Keyboard focus
    // ========================================
    /**
     * A single UIControl holds keyboard focus across the whole UI. Text-input
     * controls (LineEdit/TextEdit/SpinBox) grab focus on click so only the focused
     * control consumes typed text. Focus is cleared automatically if the holder is
     * destroyed.
     */
    void GrabFocus();
    void ReleaseFocus();
    bool HasFocus() const;
    static UIControl* GetFocusedControl();
    static void ClearFocus();

    // ----------------------------------------
    // Focus mode + neighbor navigation
    // ----------------------------------------
    /**
     * How this control participates in focus. None controls are never selected by
     * any traversal; Click controls accept programmatic/click focus but are skipped
     * by directional/tab traversal; All controls participate everywhere. GrabFocus()
     * itself is NOT gated by this (it always forces focus, matching Godot) — the mode
     * only governs which controls the navigation queries below consider.
     */
    FocusMode GetFocusMode() const;
    void SetFocusMode(FocusMode mode);

    /**
     * True when this control is a valid target for directional/tab traversal: its
     * focus mode is All and its owning node is visible and active in the hierarchy.
     */
    bool CanGrabFocus() const;

    /**
     * Per-side explicit neighbor override, stored as a NodePath relative to the
     * owning node (empty means "fall back to geometry"). Resolving prefers the
     * explicit path; when it is empty (or unresolvable) FindValidFocusNeighbor falls
     * back to the nearest focusable control in that screen direction.
     */
    std::string GetFocusNeighbor(FocusSide side) const;
    void SetFocusNeighbor(FocusSide side, const std::string& nodePath);

    /**
     * Explicit next/previous focus override (NodePath, empty = fall back to scene
     * tree order). Used by FindNextValidFocus / FindPrevValidFocus.
     */
    std::string GetFocusNext() const;
    void SetFocusNext(const std::string& nodePath);
    std::string GetFocusPrevious() const;
    void SetFocusPrevious(const std::string& nodePath);

    /**
     * Resolve the focusable control reached by traversing toward `side`. Prefers the
     * explicit focusNeighbor override for that side; otherwise picks the nearest
     * focusable control whose center lies in that screen direction. Returns nullptr
     * when none qualifies.
     */
    UIControl* FindValidFocusNeighbor(FocusSide side) const;

    /**
     * Resolve the next / previous focusable control in tab order. Prefers the
     * explicit focusNext / focusPrevious override; otherwise walks scene-tree
     * pre-order across all focusable controls, wrapping around. Returns nullptr when
     * this is the only focusable control.
     */
    UIControl* FindNextValidFocus() const;
    UIControl* FindPrevValidFocus() const;

    /**
     * Convenience: resolve the neighbor / next / previous control and, if found,
     * grab focus on it. Returns the control that received focus, or nullptr when no
     * valid target exists (focus is left unchanged in that case).
     */
    UIControl* GrabFocusNeighbor(FocusSide side);
    UIControl* GrabFocusNext();
    UIControl* GrabFocusPrevious();

    /**
     * Scriptable surface for the focus + neighbor-navigation API. Dispatches the
     * snake_case method names used by Lua / mruby / Python (and the C-API) to the
     * methods above. All UI components derive from UIControl and none override
     * CallMethod, so every control gains the focus API for free.
     */
    nlohmann::json CallMethod(const std::string& method, const nlohmann::json& args) override;

    // ========================================
    // Pointer (mouse) input arbitration
    // ========================================
    /**
     * In the polling input model every control samples the mouse independently, so
     * two overlapping controls would both react to a single click. A control that
     * consumes the pointer gates its hit-test on IsTopPointerTarget() so only the
     * front-most consumer under the cursor handles the press.
     *
     * ConsumesPointerInput: whether this control is opaque to the cursor (a click on
     * it must not pass through to controls behind it). The base derives this from the
     * mouse filter, so a control is opaque only when its filter is Stop. Controls that
     * are unconditionally interactive (the button family) override it to return true;
     * controls with no mouseFilter property read as Ignore and never block.
     */
    virtual bool ConsumesPointerInput() const { return GetMouseFilter() == MouseFilter::Stop; }

    // ========================================
    // Mouse filter (opt-in mouse handling for otherwise decorative controls)
    // ========================================
    /**
     * How this control reacts to the cursor. Controls that registered the property via
     * DefineMouseFilterProperty() return their serialized value; every other control
     * returns Ignore (its historical behaviour: decorative, never blocking).
     */
    MouseFilter GetMouseFilter() const;
    void SetMouseFilter(MouseFilter filter);

    /**
     * Current mouse state, maintained by UpdateMouseFilterState(). Always false while
     * the filter is Ignore.
     */
    bool IsMouseHovered() const { return m_MouseHovered; }
    bool IsMousePressed(input::MouseButton button = input::MouseButton::Left) const;

    /**
     * Whether `point` (logical-canvas space, the space GetCanvasMousePosition()
     * returns) lies within this control's interactive area. The base tests the
     * resolved rect (top-left position + GetBoundsSize()); controls whose hit area
     * differs (e.g. center-pivot buttons) override it so the arbiter's containment
     * test matches the control's own.
     */
    virtual bool ContainsCanvasPoint(const math::Vec2& point) const;

    /**
     * True when this control is the front-most pointer-consuming control under
     * `point` this frame. The topmost is the consumer with the highest render
     * priority (layer*1000 + sortingOrder), ties broken by scene-tree draw order
     * (later/deeper draws on top). The result is cached per (input frame, point), so
     * every control querying the same cursor position in a frame triggers a single
     * tree scan. Returns false when another consumer is on top, so the caller skips
     * its click handling.
     */
    bool IsTopPointerTarget(const math::Vec2& point) const;

    /**
     * The front-most pointer-consuming control under `point` (logical-canvas space)
     * within `sceneContext`'s scene, or nullptr when none covers the point. Exposed
     * for callers that need the winner itself rather than a self comparison.
     */
    static UIControl* GetTopPointerTarget(const math::Vec2& point, const core::Node* sceneContext);

    /**
     * Resolve a preset to its normalized anchor corners. Public so the editor and
     * gizmo can preview presets without mutating the control.
     */
    static void PresetToAnchors(AnchorPreset preset, math::Vec2& outMin, math::Vec2& outMax);

    // ========================================
    // Theme
    // ========================================
    /**
     * The theme "type name" used to look up this control's themed entries
     * (colours/constants/styleboxes/fonts). Defaults to the component's type name
     * (e.g. "Button"); override to map a component onto a different theme type.
     */
    virtual std::string GetThemeTypeName() const { return GetTypeName(); }

    /**
     * Optional theme type variation (a type that 'extends' the base type, e.g.
     * "DangerButton"). Empty means "use the base type".
     */
    std::string GetThemeTypeVariation() const;
    void SetThemeTypeVariation(const std::string& variation);

    /**
     * Per-subtree theme override: a res:// path to a .uitheme. Empty means the
     * control inherits the nearest ancestor's theme, falling back to the project
     * default (ThemeManager active theme).
     */
    std::string GetThemePath() const;
    void SetThemePath(const std::string& resPath);

    /**
     * Resolve the theme that applies to this control: its own 'theme' if set, else
     * the nearest ancestor UIControl's 'theme', else the active project theme.
     * Returns nullptr when no theme is loaded (callers then use their own default).
     *
     * The result is memoized per control. The walk up the ancestor chain runs only
     * when one of its inputs changes: the node tree structure (a reparent, via
     * Node::GetTreeStructureVersion), any control's theme path (s_ThemeOwnerGeneration),
     * or the ThemeManager state (active theme / token edits, via ThemeManager::Version).
     * A steady frame therefore resolves through three integer comparisons with no
     * traversal, while every real change still invalidates the cache deterministically.
     */
    const ui::ThemeAsset* GetEffectiveTheme() const;

    /**
     * Process-wide generation counter that advances whenever any control's theme
     * path ('theme' property) changes, through either the SetThemePath() / scripting
     * route or the inspector's OnPropertyChanged() route. Effective-theme caches key
     * on it so a theme override added or removed anywhere invalidates the affected
     * descendants without per-control notification wiring.
     */
    static uint64_t GetThemeOwnerGeneration() { return s_ThemeOwnerGeneration; }

    /**
     * Maps one of this component's properties to the theme entry it is driven by.
     */
    struct ThemeBinding {
        std::string property;            // component property name
        std::string entry;               // theme entry name
        enum class Kind { Color, Constant, Font, Vec2, Bool, Image } kind = Kind::Color;
    };

    /**
     * Declares which of this component's properties are theme-driven and the theme
     * entry each maps to. The base returns an empty list; concrete components
     * override this. Used to mark per-instance overrides and (in the editor) to
     * present themed properties.
     */
    virtual const std::vector<ThemeBinding>& GetThemeBindings() const;

    /**
     * Override model: a themeable property is theme-driven unless it has been
     * explicitly overridden (edited in the inspector / set as an override). When
     * overridden, the control's own property value is used (legacy behaviour).
     */
    bool IsThemeableProperty(const std::string& property) const;
    bool IsThemeOverridden(const std::string& property) const;
    void SetThemeOverride(const std::string& property, bool overridden);
    void ClearThemeOverrides();
    const std::set<std::string>& GetThemeOverrides() const { return m_ThemeOverrides; }

    /**
     * Resolve a themed colour/constant for the given property + theme entry. If the
     * property is overridden (or no theme provides the entry) the control's own
     * property value is returned, so behaviour is identical when no theme is loaded.
     */
    math::Color ResolveThemedColor(const std::string& property, const std::string& entry) const;
    float ResolveThemedConstant(const std::string& property, const std::string& entry) const;

    /**
     * Resolve a themed Vec2 / bool for a property + theme entry. Used for per-state
     * tween offsets (Vec2: scale/position) and flags (Bool: enabled). Returns the
     * control's own value when the property is overridden or the theme has no such
     * entry, so behaviour is unchanged with no theme.
     */
    math::Vec2 ResolveThemedVec2(const std::string& property, const std::string& entry) const;
    bool ResolveThemedBool(const std::string& property, const std::string& entry) const;

    /**
     * Resolve a themed font face (path) for a font property + theme font entry. Returns
     * the control's own font path when the property is overridden or the theme provides
     * no (non-empty) font entry, so behaviour is unchanged with no theme.
     */
    std::string ResolveThemedFontPath(const std::string& pathProperty, const std::string& fontEntry) const;

    /**
     * Resolve a themed image/texture path for an image property + theme image entry.
     * Returns the control's own path when the property is overridden or the theme
     * provides no (non-empty) image entry, so behaviour is unchanged with no theme.
     */
    std::string ResolveThemedImage(const std::string& pathProperty, const std::string& imageEntry) const;

    /**
     * Resolve a themed image entry into its path AND its optional stretch/nine-slice
     * configuration (out.hasStretch tells the caller whether the theme dictates HOW
     * the image is fitted, so a textured control can take its nine-slice margins from
     * the theme instead of its own properties). Returns false (caller keeps its own
     * image + nine-slice) when the path property is a local override or the theme
     * defines no non-empty image entry.
     */
    bool ResolveThemedImageEx(const std::string& pathProperty, const std::string& imageEntry,
                              ui::ThemeImage& out) const;

    /**
     * Resolve a themed StyleBox for this control's type + the given stylebox entry
     * (e.g. "panel", "normal"). Returns the resolved StyleBox of its declared
     * concrete subtype (StyleBoxFlat/Texture/Line/Empty), or nullptr when the
     * effective theme defines no such entry (so the caller keeps its own style).
     * Use components::DrawStyleBox() to paint the result.
     */
    std::shared_ptr<components::StyleBox> ResolveThemedStyleBox(const std::string& styleboxEntry) const;

    /**
     * Resolve a themed font size: the type's 'sizeEntry' constant if the theme defines
     * it, else the font entry's role/size if the theme defines a font entry, else the
     * control's own size (and always its own size when the size property is overridden).
     */
    float ResolveThemedFontSize(const std::string& sizeProperty, const std::string& sizeEntry,
                                const std::string& fontEntry) const;

    /**
     * Returns true (once) when the active theme's version has changed since the
     * last call, updating the cached version. Components call this in their draw
     * step to invalidate cached meshes/styles when the theme or palette is edited.
     */
    bool ConsumeThemeVersionChanged();

    // ISerializable: extend the base to persist the theme-override set.
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

protected:
    /**
     * Register the shared layout property set. Call FIRST from each derived
     * DefineProperties(). uiSpacePropName preserves each component's historical
     * UI-space property name; defaultWidth/defaultHeight/defaultUISpace preserve
     * historical defaults so existing scenes are unaffected.
     */
    void DefineUIControlProperties(float defaultWidth, float defaultHeight,
                                   const char* uiSpacePropName = "uiSpace",
                                   const char* sizeGroup = "Layout",
                                   bool defaultUISpace = true);

    /**
     * Register the "mouseFilter" enum property. Call from DefineProperties() in any
     * control that should expose Stop/Propagate/Ignore mouse handling. Controls that
     * never call it read as Ignore, so their behaviour is unchanged.
     */
    void DefineMouseFilterProperty(MouseFilter defaultFilter = MouseFilter::Ignore,
                                   const char* group = "Mouse");

    /**
     * Register the mouse signals driven by UpdateMouseFilterState(). Call from
     * DefineSignals() in any control that calls DefineMouseFilterProperty().
     */
    void DefineMouseFilterSignals();

    /**
     * Sample the cursor and advance this control's hover/press state, emitting
     * mouse_entered / mouse_exited / mouse_pressed / mouse_released / clicked. Call
     * from OnInput(). A Stop control gates its hit-test on IsTopPointerTarget() so a
     * control behind it never reacts to the same click; a Propagate control does not
     * gate (controls behind it react too). An Ignore control resets its state and
     * emits nothing.
     */
    void UpdateMouseFilterState();

    /**
     * Set a themeable property and mark it a local override, so an explicit value
     * set from code (or a gizmo) wins over the theme. Component setters for themed
     * properties should call this instead of SetPropertyValue.
     */
    template <typename T>
    void SetThemedProperty(const std::string& property, const T& value) {
        SetPropertyValue<T>(property, value);
        SetThemeOverride(property, true);
    }

    // Floor a size to GetMinSize() then clamp it to GetMaxSize() (0 = unbounded on that
    // axis). Shared by GetBoundsSize() (arranged size) and GetDesiredSize() (intrinsic).
    math::Vec2 ClampToMinMax(const math::Vec2& size) const;

    // Write a resolved global rect to the owner Node2D (as a local center position).
    void WriteRectToNode(const math::Rect& globalRect);

    // Build the resolved rect from the current Node2D global position (Position mode).
    math::Rect ComputeRectFromNode() const;

    // Walk ancestors for the nearest UIControl rect; fall back to the canvas rect.
    math::Rect ResolveParentRect(const math::Vec2& canvasSize) const;

    // Notify the parent container (if any) that this control's layout changed.
    void InvalidateParentContainerLayout();

    /**
     * Run the side effects for `propertyName` exactly as an inspector edit would, by
     * re-entering OnPropertyChanged with the property's current value.
     *
     * The layout side effects -- switching layoutMode to Anchors, marking the preset
     * Custom, invalidating the parent container -- live only in OnPropertyChanged, but
     * Component::SetPropertyValue never calls it. So every typed setter was a bare
     * SetPropertyValue that skipped them: lc_uicontrol_set_anchor_min() on a default
     * control was a SILENT NO-OP (layoutMode stays Position, and the solver returns before
     * it ever reads the anchors), and set_width() on a container child never dirtied the
     * parent. Routing the setters through here gives the C API, native code, the editor
     * and the scripting layer one path with one behaviour.
     */
    void NotifyLayoutPropertyChanged(const std::string& propertyName);

    std::string m_UISpacePropName = "uiSpace";
    math::Rect  m_ResolvedRect;
    bool        m_ContainerDriven = false;

    // ---- Mouse-filter state (see UpdateMouseFilterState) ----
    // m_MousePressedMask is a bitmask over input::MouseButton indices: a bit is set
    // while that button is held after being pressed ON this control, so a press that
    // began elsewhere never releases/clicks here.
    bool     m_MouseHovered = false;
    uint32_t m_MousePressedMask = 0;

    // Themeable properties that are local overrides (use the control's own value
    // instead of the theme). Persisted in the scene; populated to "all themeable"
    // for legacy scenes that predate the theme system so their look is preserved.
    std::set<std::string> m_ThemeOverrides;

    // Last active-theme version this control resolved against (for cache invalidation).
    uint64_t m_ThemeVersionSeen = 0;

    // ---- Effective-theme cache (see GetEffectiveTheme) ----
    // Memoizes the resolved theme pointer plus the three monotonic versions it was
    // derived from. mutable: GetEffectiveTheme() is const but fills the cache lazily.
    mutable const ui::ThemeAsset* m_CachedEffectiveTheme = nullptr;
    mutable bool     m_EffectiveThemeCacheValid = false;
    mutable uint64_t m_CachedTreeStructVersion = 0;
    mutable uint64_t m_CachedThemeOwnerGen = 0;
    mutable uint64_t m_CachedThemeMgrVersion = 0;

    // ---- Resolved themed-stylebox cache (see ResolveThemedStyleBox) ----
    // Keyed by entry name; refreshed when the theme pointer or theme version
    // changes, so a textured stylebox uploads to the GPU once rather than every
    // frame. Caches nullptr too (absent entries do not re-resolve). mutable:
    // resolution is const but fills the cache lazily.
    mutable std::unordered_map<std::string, std::shared_ptr<StyleBox>> m_ThemedStyleBoxCache;
    mutable const ui::ThemeAsset* m_ThemedStyleBoxCacheTheme = nullptr;
    mutable uint64_t m_ThemedStyleBoxCacheVersion = 0;

private:
    // Resolve a NodePath (relative to the owning node) to the UIControl it carries,
    // or nullptr when the path is empty/unresolvable or the node has no UIControl.
    UIControl* ResolveNeighborControl(const std::string& nodePath) const;

    // Gather every focusable control (CanGrabFocus()) in the owning scene in tree
    // pre-order. Used for tab-order next/previous traversal and geometry neighbor search.
    void CollectFocusableControls(std::vector<UIControl*>& out) const;

    // Nearest focusable control whose center lies toward `side` of this control's
    // center, or nullptr when none qualifies. Geometry fallback for FindValidFocusNeighbor.
    UIControl* FindGeometricNeighbor(FocusSide side) const;

    // Property name backing the explicit neighbor override for a side.
    static const char* FocusNeighborPropertyName(FocusSide side);

    // Render-order key used to pick the front-most pointer consumer: layer*1000 +
    // sortingOrder (reads those properties when present, else 0). Higher is on top.
    int GetPointerSortKey() const;

    // ---- Topmost-pointer-target cache (see IsTopPointerTarget) ----
    // Rebuilt at most once per input frame per cursor position; shared across every
    // control so a frame's controls resolve the same winner from one tree scan.
    static uint64_t s_PointerCacheFrame;
    static math::Vec2 s_PointerCachePoint;
    static UIControl* s_PointerCacheTarget;
    static bool s_PointerCacheValid;

    // Advanced by SetThemePath() and OnPropertyChanged("theme") whenever a control's
    // theme path changes; read by every effective-theme cache via GetThemeOwnerGeneration().
    static uint64_t s_ThemeOwnerGeneration;
};

} // namespace components
} // namespace lupine

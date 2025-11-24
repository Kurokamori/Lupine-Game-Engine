#pragma once

#include "lupine/core/Core.hpp"
#include "lupine/math/Math.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>

namespace lupine {
namespace components {

/**
 * StyleBox - Base class for visual styles
 *
 * Similar to Godot's StyleBox, this is the base class for all style boxes
 * that define visual appearance of UI elements like panels, buttons, etc.
 *
 * Supports:
 * - Content margins (padding)
 * - Serialization to/from .style files
 * - Cloning for creating instances
 */
class StyleBox {
public:
    virtual ~StyleBox() = default;

    /**
     * Get the type name of this StyleBox
     */
    virtual std::string GetTypeName() const = 0;

    /**
     * Clone this StyleBox
     */
    virtual std::shared_ptr<StyleBox> Clone() const = 0;

    /**
     * Serialize to JSON
     */
    virtual nlohmann::json Serialize() const;

    /**
     * Deserialize from JSON
     */
    virtual void Deserialize(const nlohmann::json& json);

    /**
     * Save to .style file
     */
    bool SaveToFile(const std::string& filepath) const;

    /**
     * Load from .style file
     */
    static std::shared_ptr<StyleBox> LoadFromFile(const std::string& filepath);

    // ===== Content Margins (Padding) =====
    
    float GetContentMarginLeft() const { return m_ContentMarginLeft; }
    void SetContentMarginLeft(float margin) { m_ContentMarginLeft = margin; }

    float GetContentMarginRight() const { return m_ContentMarginRight; }
    void SetContentMarginRight(float margin) { m_ContentMarginRight = margin; }

    float GetContentMarginTop() const { return m_ContentMarginTop; }
    void SetContentMarginTop(float margin) { m_ContentMarginTop = margin; }

    float GetContentMarginBottom() const { return m_ContentMarginBottom; }
    void SetContentMarginBottom(float margin) { m_ContentMarginBottom = margin; }

    void SetContentMarginAll(float margin) {
        m_ContentMarginLeft = margin;
        m_ContentMarginRight = margin;
        m_ContentMarginTop = margin;
        m_ContentMarginBottom = margin;
    }

protected:
    // Content margins (padding inside the style box)
    float m_ContentMarginLeft = 0.0f;
    float m_ContentMarginRight = 0.0f;
    float m_ContentMarginTop = 0.0f;
    float m_ContentMarginBottom = 0.0f;
};

/**
 * StyleBoxFlat - A flat colored style box with borders and corners
 *
 * Similar to Godot's StyleBoxFlat, provides:
 * - Background color with opacity
 * - Per-border width control (or linked)
 * - Per-border color control (or linked)
 * - Per-corner radius control (or linked)
 * - Corner detail settings
 * - Anti-aliasing support
 * - Shadow support
 * - Expand margins
 */
class StyleBoxFlat : public StyleBox {
public:
    StyleBoxFlat();
    virtual ~StyleBoxFlat() = default;

    // StyleBox interface
    std::string GetTypeName() const override { return "StyleBoxFlat"; }
    std::shared_ptr<StyleBox> Clone() const override;
    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;

    // ===== Background =====
    
    const math::Color& GetBackgroundColor() const { return m_BackgroundColor; }
    void SetBackgroundColor(const math::Color& color) { m_BackgroundColor = color; }

    float GetOpacity() const { return m_Opacity; }
    void SetOpacity(float opacity) { m_Opacity = math::Clamp(opacity, 0.0f, 1.0f); }

    // ===== Border Width =====
    
    bool GetBorderWidthLinked() const { return m_BorderWidthLinked; }
    void SetBorderWidthLinked(bool linked) { m_BorderWidthLinked = linked; }

    float GetBorderWidthLeft() const { return m_BorderWidthLeft; }
    void SetBorderWidthLeft(float width) { m_BorderWidthLeft = width; }

    float GetBorderWidthRight() const { return m_BorderWidthRight; }
    void SetBorderWidthRight(float width) { m_BorderWidthRight = width; }

    float GetBorderWidthTop() const { return m_BorderWidthTop; }
    void SetBorderWidthTop(float width) { m_BorderWidthTop = width; }

    float GetBorderWidthBottom() const { return m_BorderWidthBottom; }
    void SetBorderWidthBottom(float width) { m_BorderWidthBottom = width; }

    void SetBorderWidthAll(float width) {
        m_BorderWidthLeft = width;
        m_BorderWidthRight = width;
        m_BorderWidthTop = width;
        m_BorderWidthBottom = width;
    }

    // ===== Border Color =====
    
    bool GetBorderColorLinked() const { return m_BorderColorLinked; }
    void SetBorderColorLinked(bool linked) { m_BorderColorLinked = linked; }

    const math::Color& GetBorderColorLeft() const { return m_BorderColorLeft; }
    void SetBorderColorLeft(const math::Color& color) { m_BorderColorLeft = color; }

    const math::Color& GetBorderColorRight() const { return m_BorderColorRight; }
    void SetBorderColorRight(const math::Color& color) { m_BorderColorRight = color; }

    const math::Color& GetBorderColorTop() const { return m_BorderColorTop; }
    void SetBorderColorTop(const math::Color& color) { m_BorderColorTop = color; }

    const math::Color& GetBorderColorBottom() const { return m_BorderColorBottom; }
    void SetBorderColorBottom(const math::Color& color) { m_BorderColorBottom = color; }

    void SetBorderColorAll(const math::Color& color) {
        m_BorderColorLeft = color;
        m_BorderColorRight = color;
        m_BorderColorTop = color;
        m_BorderColorBottom = color;
    }

    // ===== Corner Radius =====
    
    bool GetCornerRadiusLinked() const { return m_CornerRadiusLinked; }
    void SetCornerRadiusLinked(bool linked) { m_CornerRadiusLinked = linked; }

    float GetCornerRadiusTopLeft() const { return m_CornerRadiusTopLeft; }
    void SetCornerRadiusTopLeft(float radius) { m_CornerRadiusTopLeft = radius; }

    float GetCornerRadiusTopRight() const { return m_CornerRadiusTopRight; }
    void SetCornerRadiusTopRight(float radius) { m_CornerRadiusTopRight = radius; }

    float GetCornerRadiusBottomLeft() const { return m_CornerRadiusBottomLeft; }
    void SetCornerRadiusBottomLeft(float radius) { m_CornerRadiusBottomLeft = radius; }

    float GetCornerRadiusBottomRight() const { return m_CornerRadiusBottomRight; }
    void SetCornerRadiusBottomRight(float radius) { m_CornerRadiusBottomRight = radius; }

    void SetCornerRadiusAll(float radius) {
        m_CornerRadiusTopLeft = radius;
        m_CornerRadiusTopRight = radius;
        m_CornerRadiusBottomLeft = radius;
        m_CornerRadiusBottomRight = radius;
    }

    // ===== Corner Detail =====
    
    int GetCornerDetail() const { return m_CornerDetail; }
    void SetCornerDetail(int detail) { m_CornerDetail = math::Clamp(detail, 1, 32); }

    bool GetAntiAliasing() const { return m_AntiAliasing; }
    void SetAntiAliasing(bool aa) { m_AntiAliasing = aa; }

    float GetAntiAliasingSize() const { return m_AntiAliasingSize; }
    void SetAntiAliasingSize(float size) { m_AntiAliasingSize = size; }

    // ===== Expand Margins =====
    
    float GetExpandMarginLeft() const { return m_ExpandMarginLeft; }
    void SetExpandMarginLeft(float margin) { m_ExpandMarginLeft = margin; }

    float GetExpandMarginRight() const { return m_ExpandMarginRight; }
    void SetExpandMarginRight(float margin) { m_ExpandMarginRight = margin; }

    float GetExpandMarginTop() const { return m_ExpandMarginTop; }
    void SetExpandMarginTop(float margin) { m_ExpandMarginTop = margin; }

    float GetExpandMarginBottom() const { return m_ExpandMarginBottom; }
    void SetExpandMarginBottom(float margin) { m_ExpandMarginBottom = margin; }

    void SetExpandMarginAll(float margin) {
        m_ExpandMarginLeft = margin;
        m_ExpandMarginRight = margin;
        m_ExpandMarginTop = margin;
        m_ExpandMarginBottom = margin;
    }

    // ===== Shadow =====
    
    bool GetShadowEnabled() const { return m_ShadowEnabled; }
    void SetShadowEnabled(bool enabled) { m_ShadowEnabled = enabled; }

    const math::Color& GetShadowColor() const { return m_ShadowColor; }
    void SetShadowColor(const math::Color& color) { m_ShadowColor = color; }

    float GetShadowSize() const { return m_ShadowSize; }
    void SetShadowSize(float size) { m_ShadowSize = size; }

    const math::Vec2& GetShadowOffset() const { return m_ShadowOffset; }
    void SetShadowOffset(const math::Vec2& offset) { m_ShadowOffset = offset; }

    // ===== Material Override =====
    
    const std::string& GetCustomShaderPath() const { return m_CustomShaderPath; }
    void SetCustomShaderPath(const std::string& path) { m_CustomShaderPath = path; }

private:
    // Background
    math::Color m_BackgroundColor = math::Color::White();
    float m_Opacity = 1.0f;

    // Border width
    bool m_BorderWidthLinked = true;
    float m_BorderWidthLeft = 0.0f;
    float m_BorderWidthRight = 0.0f;
    float m_BorderWidthTop = 0.0f;
    float m_BorderWidthBottom = 0.0f;

    // Border color
    bool m_BorderColorLinked = true;
    math::Color m_BorderColorLeft = math::Color::Black();
    math::Color m_BorderColorRight = math::Color::Black();
    math::Color m_BorderColorTop = math::Color::Black();
    math::Color m_BorderColorBottom = math::Color::Black();

    // Corner radius
    bool m_CornerRadiusLinked = true;
    float m_CornerRadiusTopLeft = 0.0f;
    float m_CornerRadiusTopRight = 0.0f;
    float m_CornerRadiusBottomLeft = 0.0f;
    float m_CornerRadiusBottomRight = 0.0f;

    // Corner detail
    int m_CornerDetail = 8;
    bool m_AntiAliasing = true;
    float m_AntiAliasingSize = 1.0f;

    // Expand margins
    float m_ExpandMarginLeft = 0.0f;
    float m_ExpandMarginRight = 0.0f;
    float m_ExpandMarginTop = 0.0f;
    float m_ExpandMarginBottom = 0.0f;

    // Shadow
    bool m_ShadowEnabled = false;
    math::Color m_ShadowColor = math::Color(0.0f, 0.0f, 0.0f, 0.6f);
    float m_ShadowSize = 4.0f;
    math::Vec2 m_ShadowOffset = math::Vec2(0.0f, 0.0f);

    // Material override
    std::string m_CustomShaderPath;
};

} // namespace components
} // namespace lupine

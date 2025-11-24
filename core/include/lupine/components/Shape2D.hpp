#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <string>

namespace lupine {
namespace components {

/**
 * Shape2D Component
 *
 * A 2D primitive shape component that can render various geometric shapes:
 * - Circle
 * - Square/Rectangle
 * - Triangle
 * - Pentagon
 * - Hexagon
 *
 * Features:
 * - Multiple shape types
 * - Fill color with alpha support
 * - Border/outline with customizable color and width
 * - Flexible sizing
 * - Layer ordering
 * - UI space selection
 */
class Shape2D : public core::Component, public IRenderableComponent {
public:
    /**
     * Shape types supported by Shape2D
     */
    enum class ShapeType {
        Circle = 0,
        Square = 1,
        Triangle = 2,
        Pentagon = 3,
        Hexagon = 4
    };

    Shape2D();
    explicit Shape2D(const std::string& name);
    virtual ~Shape2D();

    // ISerializable interface
    std::string GetTypeName() const override { return "Shape2D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;

    // ===== Shape Properties =====
    
    /**
     * Get/Set the shape type
     */
    ShapeType GetShapeType() const;
    void SetShapeType(ShapeType type);

    // ===== Appearance Properties =====
    
    /**
     * Get/Set the fill color
     */
    const math::Color& GetColor() const;
    void SetColor(const math::Color& color);
    
    /**
     * Get/Set fill enabled
     */
    bool GetFilled() const;
    void SetFilled(bool filled);

    // ===== Size Properties =====
    
    /**
     * Get/Set size (width and height)
     */
    float GetSize() const;
    void SetSize(float size);
    
    /**
     * Get/Set width (for rectangle/square shapes)
     */
    float GetWidth() const;
    void SetWidth(float width);
    
    /**
     * Get/Set height (for rectangle/square shapes)
     */
    float GetHeight() const;
    void SetHeight(float height);
    
    /**
     * Get/Set radius (for circle and polygon shapes)
     */
    float GetRadius() const;
    void SetRadius(float radius);

    // ===== Border Properties =====
    
    /**
     * Get/Set border enabled state
     */
    bool GetBorderEnabled() const;
    void SetBorderEnabled(bool enabled);
    
    /**
     * Get/Set border color
     */
    const math::Color& GetBorderColor() const;
    void SetBorderColor(const math::Color& color);
    
    /**
     * Get/Set border width
     */
    float GetBorderWidth() const;
    void SetBorderWidth(float width);

    // ===== Detail Properties =====
    
    /**
     * Get/Set circle segments (smoothness)
     */
    int GetCircleSegments() const;
    void SetCircleSegments(int segments);

    // ===== Rendering Properties =====
    
    /**
     * Get/Set render layer
     */
    int GetLayer() const;
    void SetLayer(int layer);
    
    /**
     * Get/Set sorting order within layer
     */
    int GetSortingOrder() const;
    void SetSortingOrder(int order);
    
    /**
     * Get/Set UI space flag
     */
    bool GetUISpace() const;
    void SetUISpace(bool uiSpace);

    // ===== IRenderableComponent Implementation =====
    
    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;

private:
    // Helper methods
    int ShapeTypeToInt(ShapeType type) const;
    ShapeType IntToShapeType(int value) const;

    void RenderCircle(RenderContext& ctx, const math::Vec2& position, float radius, float rotation);
    void RenderSquare(RenderContext& ctx, const math::Vec2& position, const math::Vec2& size, float rotation);
    void RenderTriangle(RenderContext& ctx, const math::Vec2& position, float radius, float rotation);
    void RenderPolygon(RenderContext& ctx, const math::Vec2& position, float radius, int sides, float rotation);
};

} // namespace components
} // namespace lupine

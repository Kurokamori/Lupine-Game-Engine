#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace lupine {
namespace components {

// Forward declarations for SVG data structures
struct SVGPath;
struct SVGShape;
struct SVGDocument;
struct SVGGradient;

/**
 * VectorGraphic2D Component
 *
 * Renders SVG vector graphics with true resolution-independent rendering.
 * Unlike rasterized approaches, this component tessellates SVG paths into
 * triangles at runtime, allowing for infinite scaling without quality loss.
 *
 * Features:
 * - SVG file loading and parsing
 * - True vector rendering (not rasterization)
 * - Resolution-independent scaling
 * - Fill and stroke support
 * - Transform support (translate, rotate, scale)
 * - Color modulation/tint
 * - Multiple blend modes
 * - Path caching for performance
 * - Support for basic SVG elements:
 *   - rect, circle, ellipse, line, polyline, polygon
 *   - path (M, L, H, V, C, S, Q, T, A, Z commands)
 *   - g (groups with transforms)
 *
 * Usage:
 *   auto vectorGraphic = node->AddComponent<VectorGraphic2D>();
 *   vectorGraphic->LoadSVG("assets/icons/logo.svg");
 *   vectorGraphic->SetScale(2.0f);
 *   vectorGraphic->SetTint(math::Color(1.0f, 0.5f, 0.0f, 1.0f));
 */
class VectorGraphic2D : public core::Component, public IRenderableComponent {
public:
    VectorGraphic2D();
    explicit VectorGraphic2D(const std::string& name);
    virtual ~VectorGraphic2D();

    // ISerializable interface
    std::string GetTypeName() const override { return "VectorGraphic2D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnRender() override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ===== SVG Loading =====

    /**
     * Load an SVG file from disk
     * @param filepath Path to the SVG file
     * @return true if loading was successful
     */
    bool LoadSVG(const std::string& filepath);

    /**
     * Load SVG from a string buffer
     * @param svgContent SVG content as a string
     * @return true if parsing was successful
     */
    bool LoadSVGFromString(const std::string& svgContent);

    /**
     * Clear the current SVG data
     */
    void Clear();

    /**
     * Check if an SVG is currently loaded
     */
    bool IsLoaded() const { return m_Document != nullptr; }

    // ===== Properties =====

    // SVG Path
    std::string GetSVGPath() const;
    void SetSVGPath(const std::string& path);

    // Size and transform
    math::Vec2 GetSize() const;
    void SetSize(const math::Vec2& size);

    float GetUniformScale() const;
    void SetUniformScale(float scale);

    bool GetPreserveAspectRatio() const;
    void SetPreserveAspectRatio(bool preserve);

    // Visual properties
    const math::Color& GetTint() const;
    void SetTint(const math::Color& color);

    float GetOpacity() const;
    void SetOpacity(float opacity);

    // Rendering options
    bool GetCentered() const;
    void SetCentered(bool centered);

    math::Vec2 GetOffset() const;
    void SetOffset(const math::Vec2& offset);

    bool GetFlipH() const;
    void SetFlipH(bool flip);

    bool GetFlipV() const;
    void SetFlipV(bool flip);

    int GetTessellationQuality() const;
    void SetTessellationQuality(int quality);

    bool GetUISpace() const;
    void SetUISpace(bool uiSpace);

    int GetLayer() const;
    void SetLayer(int layer);

    // ===== SVG Information =====

    /**
     * Get the original viewport size from the SVG
     */
    math::Vec2 GetOriginalSize() const;

    /**
     * Get the number of paths in the SVG
     */
    size_t GetPathCount() const;

    // ===== IRenderableComponent Implementation =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override;
    bool IntersectRay(const math::Ray& ray, float& outDistance) const override;

private:
    // SVG parsing helpers
    bool ParseSVGDocument(const std::string& content);
    void ParseSVGElement(const std::string& content, size_t& pos);
    // Parses SVG elements with inherited transform and style
    // Transform: ta,tb,tc,td,te,tf (2D affine matrix components)
    // Inherited styles: parentFill, parentStroke, parentStrokeWidth, parentOpacity
    void ParseSVGElementWithTransform(const std::string& content, size_t& pos,
                                       float ta, float tb, float tc, float td, float te, float tf,
                                       const std::string& parentFill, const std::string& parentStroke,
                                       float parentStrokeWidth, float parentOpacity);
    void ParsePath(const std::string& pathData, SVGShape& shape);
    void ParseRect(float x, float y, float width, float height, float rx, float ry, SVGShape& shape);
    void ParseCircle(float cx, float cy, float r, SVGShape& shape);
    void ParseEllipse(float cx, float cy, float rx, float ry, SVGShape& shape);
    void ParsePolygon(const std::string& points, SVGShape& shape);
    void ParsePolyline(const std::string& points, SVGShape& shape);
    void ParseLine(float x1, float y1, float x2, float y2, SVGShape& shape);

    // Path command parsing
    std::vector<math::Vec2> ParsePathCommands(const std::string& pathData);
    void AddBezierCurve(std::vector<math::Vec2>& points,
                        const math::Vec2& p0, const math::Vec2& p1,
                        const math::Vec2& p2, const math::Vec2& p3, int segments);
    void AddQuadraticCurve(std::vector<math::Vec2>& points,
                           const math::Vec2& p0, const math::Vec2& p1,
                           const math::Vec2& p2, int segments);
    void AddArc(std::vector<math::Vec2>& points,
                const math::Vec2& start, float rx, float ry,
                float rotation, bool largeArc, bool sweep,
                const math::Vec2& end, int segments);

    // Tessellation
    void TessellateShape(const SVGShape& shape, float zDepth);
    void TessellatePolygon(const std::vector<math::Vec2>& points,
                           const math::Color& color, float zDepth,
                           std::vector<float>& vertices);
    void TessellatePolygonWithHoles(const std::vector<std::vector<math::Vec2>>& paths,
                                     const math::Color& color, float zDepth,
                                     std::vector<float>& vertices);
    void TessellatePolygonWithGradient(const std::vector<math::Vec2>& points,
                                       const math::Color& color, float zDepth,
                                       std::vector<float>& vertices,
                                       const SVGGradient* gradient,
                                       float boundsMinX, float boundsMinY,
                                       float boundsMaxX, float boundsMaxY,
                                       const math::Color& tint, float opacity);
    void TessellatePolygonWithHolesAndGradient(const std::vector<std::vector<math::Vec2>>& paths,
                                               const math::Color& color, float zDepth,
                                               std::vector<float>& vertices,
                                               const SVGGradient* gradient,
                                               float boundsMinX, float boundsMinY,
                                               float boundsMaxX, float boundsMaxY,
                                               const math::Color& tint, float opacity);
    void TessellateStroke(const std::vector<math::Vec2>& points,
                          float strokeWidth, const math::Color& color,
                          float zDepth, std::vector<float>& vertices,
                          bool closed = false);

    // Rendering helpers
    void RebuildMesh();
    void UpdateTransform();
    math::Vec2 TransformPoint(const math::Vec2& point) const;

    // Mesh management
    void UploadMesh(IGfxDevice* device);
    void DestroyMesh();

    // Member variables
    std::unique_ptr<SVGDocument> m_Document;
    std::string m_CurrentSVGPath;
    bool m_MeshNeedsRebuild;
    bool m_MeshNeedsUpload;

    // Cached mesh data
    std::vector<float> m_VertexData;  // Interleaved: x, y, r, g, b, a
    std::vector<uint32_t> m_IndexData;
    MeshHandle m_MeshHandle;
    MeshHandle m_RenderMesh;  // Persistent mesh for rendering (survives until next frame)
    IGfxDevice* m_CachedDevice = nullptr;  // Device reference for cleanup

    // Original SVG dimensions
    math::Vec2 m_OriginalViewBox;
    math::Vec2 m_OriginalSize;

    // Cached computed values
    mutable math::Color m_CachedTint;
    mutable math::Vec2 m_CachedSize;
    mutable math::Vec2 m_CachedOffset;
};

// ===== SVG Data Structures =====

/**
 * Represents a single path/contour in the SVG
 */
struct SVGPath {
    std::vector<math::Vec2> points;
    bool closed = true;
};

/**
 * Gradient stop (color at a specific offset)
 */
struct SVGGradientStop {
    float offset = 0.0f;      // 0.0 to 1.0
    math::Color color = math::Color::Black();
};

/**
 * Gradient definition (linear or radial)
 */
struct SVGGradient {
    enum class Type { Linear, Radial };

    std::string id;
    Type type = Type::Linear;
    std::vector<SVGGradientStop> stops;

    // Linear gradient: line from (x1,y1) to (x2,y2)
    float x1 = 0.0f, y1 = 0.0f;
    float x2 = 1.0f, y2 = 0.0f;

    // Radial gradient: center (cx,cy), radius r, focal point (fx,fy)
    float cx = 0.5f, cy = 0.5f;
    float r = 0.5f;
    float fx = 0.5f, fy = 0.5f;  // Focal point (defaults to center)

    // Gradient units: true = objectBoundingBox (0-1), false = userSpaceOnUse (SVG coords)
    bool objectBoundingBox = true;

    // Optional transform
    float transformA = 1, transformB = 0, transformC = 0;
    float transformD = 1, transformE = 0, transformF = 0;
    bool hasTransform = false;

    // Sample color at position t (0-1 along gradient)
    math::Color SampleColor(float t) const {
        if (stops.empty()) return math::Color::Black();
        if (stops.size() == 1) return stops[0].color;

        t = std::max(0.0f, std::min(1.0f, t));

        // Handle t before first stop
        if (t <= stops[0].offset) return stops[0].color;

        // Handle t after last stop
        if (t >= stops.back().offset) return stops.back().color;

        // Find stops to interpolate between
        for (size_t i = 0; i < stops.size() - 1; i++) {
            if (t >= stops[i].offset && t <= stops[i + 1].offset) {
                float range = stops[i + 1].offset - stops[i].offset;
                if (range < 0.0001f) return stops[i].color;
                float localT = (t - stops[i].offset) / range;
                return math::Color(
                    stops[i].color.r + (stops[i + 1].color.r - stops[i].color.r) * localT,
                    stops[i].color.g + (stops[i + 1].color.g - stops[i].color.g) * localT,
                    stops[i].color.b + (stops[i + 1].color.b - stops[i].color.b) * localT,
                    stops[i].color.a + (stops[i + 1].color.a - stops[i].color.a) * localT
                );
            }
        }
        return stops.back().color;
    }
};

/**
 * Represents a shape (filled or stroked path) in the SVG
 */
struct SVGShape {
    std::vector<SVGPath> paths;
    math::Color fillColor = math::Color::Black();
    math::Color strokeColor = math::Color::Clear();
    float strokeWidth = 0.0f;
    bool hasFill = true;
    bool hasStroke = false;
    float opacity = 1.0f;
    math::Mat3 transform = math::Mat3::Identity();

    // Gradient fill (empty string = solid color)
    std::string fillGradientId;
    std::string strokeGradientId;
};

/**
 * Represents the entire SVG document
 */
struct SVGDocument {
    std::vector<SVGShape> shapes;
    std::unordered_map<std::string, SVGGradient> gradients;
    math::Vec2 viewBox = math::Vec2(0.0f, 0.0f);
    math::Vec2 size = math::Vec2(100.0f, 100.0f);
};

} // namespace components
} // namespace lupine


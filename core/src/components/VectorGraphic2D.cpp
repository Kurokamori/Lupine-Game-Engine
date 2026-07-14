#include "lupine/components/VectorGraphic2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/asset/Asset.hpp"
#include <cmath>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cctype>
#include <list>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lupine {
namespace components {

using namespace core;
using namespace math;

// ============================================================================
// Constructor / Destructor
// ============================================================================

VectorGraphic2D::VectorGraphic2D()
    : Component("VectorGraphic2D")
    , m_MeshNeedsRebuild(false)
    , m_MeshNeedsUpload(false)
    , m_OriginalViewBox(0.0f, 0.0f)
    , m_OriginalSize(100.0f, 100.0f)
{
}

VectorGraphic2D::VectorGraphic2D(const std::string& name)
    : Component(name)
    , m_MeshNeedsRebuild(false)
    , m_MeshNeedsUpload(false)
    , m_OriginalViewBox(0.0f, 0.0f)
    , m_OriginalSize(100.0f, 100.0f)
{
}

VectorGraphic2D::~VectorGraphic2D() {
    DestroyMesh();
}

// ============================================================================
// Property Definition
// ============================================================================

void VectorGraphic2D::DefineProperties() {
    // SVG source
    DefineProperty(PROPERTY_FILE_GROUP(svgPath, std::string(""), "*.svg", "Source"));

    // Transform
    DefineProperty(PROPERTY_DEFAULT_GROUP(size, Vec2, Vec2(100.0f, 100.0f), "Transform"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(uniformScale, 1.0f, 0.01f, 100.0f, 0.1f, "Transform"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(preserveAspectRatio, Bool, true, "Transform"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(centered, Bool, true, "Transform"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(offset, Vec2, Vec2(0.0f, 0.0f), "Transform"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipH, Bool, false, "Transform"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(flipV, Bool, false, "Transform"));

    // Appearance
    DefineProperty(PROPERTY_DEFAULT_GROUP(tint, Color, Color::White(), "Appearance"));
    DefineProperty(PROPERTY_FLOAT_RANGE_GROUP(opacity, 1.0f, 0.0f, 1.0f, 0.01f, "Appearance"));

    // Quality
    DefineProperty(PROPERTY_INT_RANGE_GROUP(tessellationQuality, 16, 4, 64, 1, "Quality"));

    // Rendering
    DefineProperty(PROPERTY_INT_RANGE_GROUP(layer, 0, -100, 100, 1, "Rendering"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(uiSpace, Bool, false, "Rendering"));
}

// ============================================================================
// Lifecycle Hooks
// ============================================================================

void VectorGraphic2D::OnAwake() {
    std::string svgPath = GetSVGPath();
    if (!svgPath.empty()) {
        LoadSVG(svgPath);
    }
}

void VectorGraphic2D::OnReady() {
    if (m_MeshNeedsRebuild && m_Document) {
        RebuildMesh();
    }
}

void VectorGraphic2D::OnRender() {
    // Rendering happens in buildDrawCommands
}

void VectorGraphic2D::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "svgPath") {
        std::string path = newValue.get<std::string>();
        if (path != m_CurrentSVGPath) {
            LoadSVG(path);
        }
    } else if (propertyName == "tessellationQuality") {
        m_MeshNeedsRebuild = true;
    } else if (propertyName == "size" || propertyName == "uniformScale" ||
               propertyName == "flipH" || propertyName == "flipV") {
        m_MeshNeedsRebuild = true;
    }
}

// ============================================================================
// SVG Loading
// ============================================================================

bool VectorGraphic2D::LoadSVG(const std::string& filepath) {
    if (filepath.empty()) {
        Clear();
        return false;
    }

    // Resolve virtual path (res://, user://, etc.) to physical path
    std::string physicalPath = asset::Asset::ResolveAssetPath(filepath);
    if (physicalPath.empty()) {
        LOG_WARN(LogCategory::Core, "VectorGraphic2D: Failed to resolve SVG path: {}", filepath);
        return false;
    }

    // Read file content
    auto result = platform::FileSystem::ReadFile(physicalPath);
    if (!result.success || result.data.empty()) {
        LOG_WARN(LogCategory::Core, "VectorGraphic2D: Failed to read SVG file: {} (resolved: {})", filepath, physicalPath);
        return false;
    }
    std::string content = result.data;

    bool success = LoadSVGFromString(content);
    if (success) {
        m_CurrentSVGPath = filepath;
        SetSVGPath(filepath);
    }
    return success;
}

bool VectorGraphic2D::LoadSVGFromString(const std::string& svgContent) {
    Clear();

    if (!ParseSVGDocument(svgContent)) {
        LOG_WARN(LogCategory::Core, "VectorGraphic2D: Failed to parse SVG content");
        return false;
    }

    m_MeshNeedsRebuild = true;
    return true;
}

void VectorGraphic2D::Clear() {
    m_Document.reset();
    m_VertexData.clear();
    m_IndexData.clear();
    DestroyMesh();
    m_CurrentSVGPath.clear();
    m_MeshNeedsRebuild = false;
    m_MeshNeedsUpload = false;
}

// ============================================================================
// Property Accessors
// ============================================================================

std::string VectorGraphic2D::GetSVGPath() const {
    return GetPropertyValue<std::string>("svgPath");
}

void VectorGraphic2D::SetSVGPath(const std::string& path) {
    SetPropertyValue<std::string>("svgPath", path);
}

Vec2 VectorGraphic2D::GetSize() const {
    m_CachedSize = GetPropertyValue<Vec2>("size");
    return m_CachedSize;
}

void VectorGraphic2D::SetSize(const Vec2& size) {
    SetPropertyValue<Vec2>("size", size);
    m_MeshNeedsRebuild = true;
}

float VectorGraphic2D::GetUniformScale() const {
    return GetPropertyValue<float>("uniformScale");
}

void VectorGraphic2D::SetUniformScale(float scale) {
    SetPropertyValue<float>("uniformScale", scale);
    m_MeshNeedsRebuild = true;
}

bool VectorGraphic2D::GetPreserveAspectRatio() const {
    return GetPropertyValue<bool>("preserveAspectRatio");
}

void VectorGraphic2D::SetPreserveAspectRatio(bool preserve) {
    SetPropertyValue<bool>("preserveAspectRatio", preserve);
    m_MeshNeedsRebuild = true;
}

const Color& VectorGraphic2D::GetTint() const {
    m_CachedTint = GetPropertyValue<Color>("tint");
    return m_CachedTint;
}

void VectorGraphic2D::SetTint(const Color& color) {
    SetPropertyValue<Color>("tint", color);
}

float VectorGraphic2D::GetOpacity() const {
    return GetPropertyValue<float>("opacity");
}

void VectorGraphic2D::SetOpacity(float opacity) {
    SetPropertyValue<float>("opacity", opacity);
}

bool VectorGraphic2D::GetCentered() const {
    return GetPropertyValue<bool>("centered");
}

void VectorGraphic2D::SetCentered(bool centered) {
    SetPropertyValue<bool>("centered", centered);
}

Vec2 VectorGraphic2D::GetOffset() const {
    m_CachedOffset = GetPropertyValue<Vec2>("offset");
    return m_CachedOffset;
}

void VectorGraphic2D::SetOffset(const Vec2& offset) {
    SetPropertyValue<Vec2>("offset", offset);
}

bool VectorGraphic2D::GetFlipH() const {
    return GetPropertyValue<bool>("flipH");
}

void VectorGraphic2D::SetFlipH(bool flip) {
    SetPropertyValue<bool>("flipH", flip);
    m_MeshNeedsRebuild = true;
}

bool VectorGraphic2D::GetFlipV() const {
    return GetPropertyValue<bool>("flipV");
}

void VectorGraphic2D::SetFlipV(bool flip) {
    SetPropertyValue<bool>("flipV", flip);
    m_MeshNeedsRebuild = true;
}

int VectorGraphic2D::GetTessellationQuality() const {
    return GetPropertyValue<int>("tessellationQuality");
}

void VectorGraphic2D::SetTessellationQuality(int quality) {
    SetPropertyValue<int>("tessellationQuality", quality);
    m_MeshNeedsRebuild = true;
}

bool VectorGraphic2D::GetUISpace() const {
    return GetPropertyValue<bool>("uiSpace");
}

void VectorGraphic2D::SetUISpace(bool uiSpace) {
    SetPropertyValue<bool>("uiSpace", uiSpace);
}

int VectorGraphic2D::GetLayer() const {
    return GetPropertyValue<int>("layer");
}

void VectorGraphic2D::SetLayer(int layer) {
    SetPropertyValue<int>("layer", layer);
}

Vec2 VectorGraphic2D::GetOriginalSize() const {
    return m_OriginalSize;
}

size_t VectorGraphic2D::GetPathCount() const {
    if (!m_Document) return 0;
    size_t count = 0;
    for (const auto& shape : m_Document->shapes) {
        count += shape.paths.size();
    }
    return count;
}

// ============================================================================
// SVG Parsing Helpers
// ============================================================================

namespace {

// Helper to skip whitespace
void SkipWhitespace(const std::string& str, size_t& pos) {
    while (pos < str.size() && std::isspace(str[pos])) pos++;
}

// Helper to parse a number
float ParseNumber(const std::string& str, size_t& pos) {
    SkipWhitespace(str, pos);
    size_t start = pos;
    if (pos < str.size() && (str[pos] == '-' || str[pos] == '+')) pos++;
    while (pos < str.size() && (std::isdigit(str[pos]) || str[pos] == '.')) pos++;
    if (pos < str.size() && (str[pos] == 'e' || str[pos] == 'E')) {
        pos++;
        if (pos < str.size() && (str[pos] == '-' || str[pos] == '+')) pos++;
        while (pos < str.size() && std::isdigit(str[pos])) pos++;
    }
    if (start == pos) return 0.0f;
    return std::stof(str.substr(start, pos - start));
}

// Helper to skip comma/whitespace
void SkipCommaWhitespace(const std::string& str, size_t& pos) {
    SkipWhitespace(str, pos);
    if (pos < str.size() && str[pos] == ',') pos++;
    SkipWhitespace(str, pos);
}

// Helper to parse color from SVG format
// Extract gradient ID from url(#id) format
std::string ExtractGradientId(const std::string& fillStr) {
    if (fillStr.substr(0, 4) == "url(" && fillStr.back() == ')') {
        std::string inside = fillStr.substr(4, fillStr.length() - 5);
        // Remove quotes if present
        if (!inside.empty() && (inside[0] == '"' || inside[0] == '\'')) {
            inside = inside.substr(1, inside.length() - 2);
        }
        // Remove # prefix
        if (!inside.empty() && inside[0] == '#') {
            return inside.substr(1);
        }
        return inside;
    }
    return "";
}

// Check if fill string is a gradient reference
bool IsGradientReference(const std::string& fillStr) {
    return fillStr.substr(0, 4) == "url(";
}

Color ParseColor(const std::string& colorStr) {
    if (colorStr.empty() || colorStr == "none") {
        return Color::Clear();
    }
    // Skip gradient references - they're handled separately
    if (IsGradientReference(colorStr)) {
        return Color::Black(); // Placeholder, will use gradient
    }

    // Handle hex colors
    if (colorStr[0] == '#') {
        unsigned int hex = 0;
        if (colorStr.length() == 4) {
            // #RGB format
            std::stringstream ss;
            ss << std::hex << colorStr.substr(1);
            ss >> hex;
            float r = ((hex >> 8) & 0xF) / 15.0f;
            float g = ((hex >> 4) & 0xF) / 15.0f;
            float b = (hex & 0xF) / 15.0f;
            return Color(r, g, b, 1.0f);
        } else if (colorStr.length() == 7) {
            // #RRGGBB format
            std::stringstream ss;
            ss << std::hex << colorStr.substr(1);
            ss >> hex;
            float r = ((hex >> 16) & 0xFF) / 255.0f;
            float g = ((hex >> 8) & 0xFF) / 255.0f;
            float b = (hex & 0xFF) / 255.0f;
            return Color(r, g, b, 1.0f);
        }
    }

    // Handle rgb() format
    if (colorStr.substr(0, 4) == "rgb(") {
        size_t pos = 4;
        float r = ParseNumber(colorStr, pos) / 255.0f;
        SkipCommaWhitespace(colorStr, pos);
        float g = ParseNumber(colorStr, pos) / 255.0f;
        SkipCommaWhitespace(colorStr, pos);
        float b = ParseNumber(colorStr, pos) / 255.0f;
        return Color(r, g, b, 1.0f);
    }

    // Named colors (common ones)
    static const std::unordered_map<std::string, Color> namedColors = {
        {"black", Color(0, 0, 0, 1)},
        {"white", Color(1, 1, 1, 1)},
        {"red", Color(1, 0, 0, 1)},
        {"green", Color(0, 0.5f, 0, 1)},
        {"blue", Color(0, 0, 1, 1)},
        {"yellow", Color(1, 1, 0, 1)},
        {"cyan", Color(0, 1, 1, 1)},
        {"magenta", Color(1, 0, 1, 1)},
        {"gray", Color(0.5f, 0.5f, 0.5f, 1)},
        {"grey", Color(0.5f, 0.5f, 0.5f, 1)},
        {"orange", Color(1, 0.647f, 0, 1)},
        {"purple", Color(0.5f, 0, 0.5f, 1)},
        {"pink", Color(1, 0.753f, 0.796f, 1)},
        {"brown", Color(0.647f, 0.165f, 0.165f, 1)},
        {"transparent", Color::Clear()}
    };

    auto it = namedColors.find(colorStr);
    if (it != namedColors.end()) {
        return it->second;
    }

    return Color::Black();
}

// Helper to extract attribute value from XML
std::string GetAttribute(const std::string& element, const std::string& attrName) {
    std::string search = attrName + "=\"";
    size_t pos = element.find(search);
    if (pos == std::string::npos) {
        search = attrName + "='";
        pos = element.find(search);
    }
    if (pos == std::string::npos) return "";

    pos += search.length();
    char quote = element[pos - 1];
    size_t end = element.find(quote, pos);
    if (end == std::string::npos) return "";

    return element.substr(pos, end - pos);
}

// Helper to get float attribute
float GetFloatAttribute(const std::string& element, const std::string& attrName, float defaultVal = 0.0f) {
    std::string val = GetAttribute(element, attrName);
    if (val.empty()) return defaultVal;
    try {
        return std::stof(val);
    } catch (...) {
        return defaultVal;
    }
}

// Simple 2D transform matrix (3x3 but we only need 2x3 for affine transforms)
struct Transform2D {
    float a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;  // matrix(a,b,c,d,e,f)

    Vec2 apply(const Vec2& p) const {
        return Vec2(a * p.x + c * p.y + e, b * p.x + d * p.y + f);
    }

    Transform2D operator*(const Transform2D& other) const {
        Transform2D result;
        result.a = a * other.a + c * other.b;
        result.b = b * other.a + d * other.b;
        result.c = a * other.c + c * other.d;
        result.d = b * other.c + d * other.d;
        result.e = a * other.e + c * other.f + e;
        result.f = b * other.e + d * other.f + f;
        return result;
    }

    static Transform2D translate(float tx, float ty) {
        Transform2D t; t.e = tx; t.f = ty; return t;
    }
    static Transform2D scale(float sx, float sy) {
        Transform2D t; t.a = sx; t.d = sy; return t;
    }
    static Transform2D rotate(float angleDeg) {
        float rad = angleDeg * static_cast<float>(M_PI) / 180.0f;
        float c = std::cos(rad), s = std::sin(rad);
        Transform2D t; t.a = c; t.b = s; t.c = -s; t.d = c; return t;
    }
};

// Parse SVG transform attribute
Transform2D ParseTransform(const std::string& transformStr) {
    Transform2D result;
    if (transformStr.empty()) return result;

    size_t pos = 0;
    while (pos < transformStr.size()) {
        // Skip whitespace
        while (pos < transformStr.size() && std::isspace(transformStr[pos])) pos++;
        if (pos >= transformStr.size()) break;

        // Find transform function name
        size_t nameStart = pos;
        while (pos < transformStr.size() && std::isalpha(transformStr[pos])) pos++;
        std::string funcName = transformStr.substr(nameStart, pos - nameStart);

        // Find opening paren
        while (pos < transformStr.size() && transformStr[pos] != '(') pos++;
        if (pos >= transformStr.size()) break;
        pos++; // skip '('

        // Parse arguments
        std::vector<float> args;
        while (pos < transformStr.size() && transformStr[pos] != ')') {
            while (pos < transformStr.size() && (std::isspace(transformStr[pos]) || transformStr[pos] == ',')) pos++;
            if (transformStr[pos] == ')') break;

            size_t numStart = pos;
            if (transformStr[pos] == '-' || transformStr[pos] == '+') pos++;
            while (pos < transformStr.size() && (std::isdigit(transformStr[pos]) || transformStr[pos] == '.')) pos++;
            if (pos > numStart) {
                try {
                    args.push_back(std::stof(transformStr.substr(numStart, pos - numStart)));
                } catch (...) {}
            }
        }
        if (pos < transformStr.size()) pos++; // skip ')'

        // Apply transform
        if (funcName == "translate" && args.size() >= 1) {
            float tx = args[0], ty = args.size() > 1 ? args[1] : 0;
            result = result * Transform2D::translate(tx, ty);
        } else if (funcName == "scale" && args.size() >= 1) {
            float sx = args[0], sy = args.size() > 1 ? args[1] : args[0];
            result = result * Transform2D::scale(sx, sy);
        } else if (funcName == "rotate" && args.size() >= 1) {
            if (args.size() >= 3) {
                // rotate(angle, cx, cy)
                result = result * Transform2D::translate(args[1], args[2]);
                result = result * Transform2D::rotate(args[0]);
                result = result * Transform2D::translate(-args[1], -args[2]);
            } else {
                result = result * Transform2D::rotate(args[0]);
            }
        } else if (funcName == "matrix" && args.size() >= 6) {
            Transform2D m;
            m.a = args[0]; m.b = args[1]; m.c = args[2];
            m.d = args[3]; m.e = args[4]; m.f = args[5];
            result = result * m;
        }
    }
    return result;
}

// Apply transform to all points in a shape
void ApplyTransformToShape(SVGShape& shape, const Transform2D& transform) {
    for (auto& path : shape.paths) {
        for (auto& point : path.points) {
            point = transform.apply(point);
        }
    }
}

// Parse a single gradient element (linearGradient or radialGradient)
SVGGradient ParseGradientElement(const std::string& element, const std::string& tagName, const std::string& fullContent) {
    SVGGradient gradient;
    gradient.id = GetAttribute(element, "id");

    if (tagName == "linearGradient") {
        gradient.type = SVGGradient::Type::Linear;
        gradient.x1 = GetFloatAttribute(element, "x1", 0.0f);
        gradient.y1 = GetFloatAttribute(element, "y1", 0.0f);
        gradient.x2 = GetFloatAttribute(element, "x2", 1.0f);
        gradient.y2 = GetFloatAttribute(element, "y2", 0.0f);
    } else {
        gradient.type = SVGGradient::Type::Radial;
        gradient.cx = GetFloatAttribute(element, "cx", 0.5f);
        gradient.cy = GetFloatAttribute(element, "cy", 0.5f);
        gradient.r = GetFloatAttribute(element, "r", 0.5f);
        gradient.fx = GetFloatAttribute(element, "fx", gradient.cx);
        gradient.fy = GetFloatAttribute(element, "fy", gradient.cy);
    }

    // Check gradientUnits
    std::string units = GetAttribute(element, "gradientUnits");
    gradient.objectBoundingBox = (units != "userSpaceOnUse");

    // Parse gradientTransform
    std::string transformStr = GetAttribute(element, "gradientTransform");
    if (!transformStr.empty()) {
        Transform2D trans = ParseTransform(transformStr);
        gradient.transformA = trans.a;
        gradient.transformB = trans.b;
        gradient.transformC = trans.c;
        gradient.transformD = trans.d;
        gradient.transformE = trans.e;
        gradient.transformF = trans.f;
        gradient.hasTransform = true;
    }

    // Find the gradient's content (stop elements) - need to search in full content
    std::string openTag = "<" + tagName;
    std::string closeTag = "</" + tagName + ">";

    // Find this gradient by id in full content
    std::string searchId = "id=\"" + gradient.id + "\"";
    size_t gradStart = fullContent.find(searchId);
    if (gradStart != std::string::npos) {
        // Find where this gradient element ends
        size_t tagClose = fullContent.find(">", gradStart);
        bool selfClosing = (tagClose > 0 && fullContent[tagClose - 1] == '/');

        if (!selfClosing) {
            // Find closing tag
            size_t closeStart = fullContent.find(closeTag, tagClose);
            if (closeStart != std::string::npos) {
                std::string gradContent = fullContent.substr(tagClose + 1, closeStart - tagClose - 1);

                // Parse stop elements
                size_t stopPos = 0;
                while ((stopPos = gradContent.find("<stop", stopPos)) != std::string::npos) {
                    size_t stopEnd = gradContent.find(">", stopPos);
                    if (stopEnd == std::string::npos) break;

                    std::string stopElement = gradContent.substr(stopPos, stopEnd - stopPos + 1);

                    SVGGradientStop stop;
                    std::string offsetStr = GetAttribute(stopElement, "offset");
                    if (!offsetStr.empty()) {
                        size_t p = 0;
                        stop.offset = ParseNumber(offsetStr, p);
                        // Handle percentage
                        if (offsetStr.find('%') != std::string::npos) {
                            stop.offset /= 100.0f;
                        }
                    }

                    std::string colorStr = GetAttribute(stopElement, "stop-color");
                    if (!colorStr.empty()) {
                        stop.color = ParseColor(colorStr);
                    }

                    // Handle opacity
                    float stopOpacity = GetFloatAttribute(stopElement, "stop-opacity", 1.0f);
                    stop.color.a *= stopOpacity;

                    gradient.stops.push_back(stop);
                    stopPos = stopEnd + 1;
                }
            }
        }
    }

    // Sort stops by offset
    std::sort(gradient.stops.begin(), gradient.stops.end(),
              [](const SVGGradientStop& a, const SVGGradientStop& b) {
                  return a.offset < b.offset;
              });

    LOG_INFO(LogCategory::Core, "VectorGraphic2D: Parsed {} '{}' with {} stops",
             tagName, gradient.id, gradient.stops.size());

    return gradient;
}

// Parse all gradients from the defs section
void ParseGradients(const std::string& content, SVGDocument& doc) {
    // Find <defs> section
    size_t defsStart = content.find("<defs");
    if (defsStart == std::string::npos) return;

    size_t defsEnd = content.find("</defs>", defsStart);
    if (defsEnd == std::string::npos) return;

    std::string defsContent = content.substr(defsStart, defsEnd - defsStart);

    // Parse linearGradient elements
    size_t pos = 0;
    while ((pos = defsContent.find("<linearGradient", pos)) != std::string::npos) {
        size_t tagEnd = defsContent.find(">", pos);
        if (tagEnd == std::string::npos) break;

        std::string element = defsContent.substr(pos, tagEnd - pos + 1);
        SVGGradient gradient = ParseGradientElement(element, "linearGradient", content);
        if (!gradient.id.empty()) {
            doc.gradients[gradient.id] = gradient;
        }
        pos = tagEnd + 1;
    }

    // Parse radialGradient elements
    pos = 0;
    while ((pos = defsContent.find("<radialGradient", pos)) != std::string::npos) {
        size_t tagEnd = defsContent.find(">", pos);
        if (tagEnd == std::string::npos) break;

        std::string element = defsContent.substr(pos, tagEnd - pos + 1);
        SVGGradient gradient = ParseGradientElement(element, "radialGradient", content);
        if (!gradient.id.empty()) {
            doc.gradients[gradient.id] = gradient;
        }
        pos = tagEnd + 1;
    }
}

} // anonymous namespace

bool VectorGraphic2D::ParseSVGDocument(const std::string& content) {
    m_Document = std::make_unique<SVGDocument>();

    // Find SVG element and extract viewBox/size
    size_t svgStart = content.find("<svg");
    if (svgStart == std::string::npos) {
        LOG_WARN(LogCategory::Core, "VectorGraphic2D: No <svg> element found");
        return false;
    }

    size_t svgEnd = content.find(">", svgStart);
    if (svgEnd == std::string::npos) return false;

    std::string svgElement = content.substr(svgStart, svgEnd - svgStart + 1);

    // Parse viewBox
    std::string viewBox = GetAttribute(svgElement, "viewBox");
    if (!viewBox.empty()) {
        size_t pos = 0;
        float minX = ParseNumber(viewBox, pos);
        SkipCommaWhitespace(viewBox, pos);
        float minY = ParseNumber(viewBox, pos);
        SkipCommaWhitespace(viewBox, pos);
        float width = ParseNumber(viewBox, pos);
        SkipCommaWhitespace(viewBox, pos);
        float height = ParseNumber(viewBox, pos);
        m_Document->viewBox = Vec2(minX, minY);
        m_Document->size = Vec2(width, height);
    } else {
        // Try width/height attributes
        float width = GetFloatAttribute(svgElement, "width", 100.0f);
        float height = GetFloatAttribute(svgElement, "height", 100.0f);
        m_Document->size = Vec2(width, height);
    }

    m_OriginalSize = m_Document->size;
    m_OriginalViewBox = m_Document->viewBox;

    // Parse gradient definitions from <defs> section
    ParseGradients(content, *m_Document);

    // Parse shape elements with identity transform and no inherited styles
    size_t pos = svgEnd + 1;
    // Identity transform: a=1, b=0, c=0, d=1, e=0, f=0
    // Empty inherited styles - will use SVG defaults (black fill, no stroke)
    ParseSVGElementWithTransform(content, pos, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                  "", "", 0.0f, 1.0f);

    LOG_INFO(LogCategory::Core, "VectorGraphic2D: Loaded SVG with {} shapes, size {}x{}",
             m_Document->shapes.size(), m_Document->size.x, m_Document->size.y);

    return true;
}

void VectorGraphic2D::ParseSVGElement(const std::string& content, size_t& pos) {
    // Delegate to the helper with identity transform and no inherited styles
    ParseSVGElementWithTransform(content, pos, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                                  "", "", 0.0f, 1.0f);
}

void VectorGraphic2D::ParseSVGElementWithTransform(const std::string& content, size_t& pos,
                                                    float pa, float pb, float pc, float pd, float pe, float pf,
                                                    const std::string& parentFill, const std::string& parentStroke,
                                                    float parentStrokeWidth, float parentOpacity) {
    // Parent transform as matrix components: a,b,c,d,e,f
    // Matrix multiplication: result = parent * element
    // | a c e |   | a' c' e' |   | aa'+cb' ac'+cd' ae'+cf'+e |
    // | b d f | * | b' d' f' | = | ba'+db' bc'+dd' be'+df'+f |
    // | 0 0 1 |   | 0  0  1  |   | 0       0       1         |

    while (pos < content.size()) {
        // Find next element
        size_t tagStart = content.find("<", pos);
        if (tagStart == std::string::npos) break;

        // Skip comments and processing instructions
        if (content.substr(tagStart, 4) == "<!--") {
            pos = content.find("-->", tagStart);
            if (pos != std::string::npos) pos += 3;
            continue;
        }
        if (content.substr(tagStart, 2) == "<?") {
            pos = content.find("?>", tagStart);
            if (pos != std::string::npos) pos += 2;
            continue;
        }

        // Check for closing tag
        if (content[tagStart + 1] == '/') {
            pos = content.find(">", tagStart) + 1;
            return;
        }

        // Find end of tag
        size_t tagEnd = content.find(">", tagStart);
        if (tagEnd == std::string::npos) break;

        std::string element = content.substr(tagStart, tagEnd - tagStart + 1);
        bool selfClosing = (element[element.size() - 2] == '/');

        // Extract tag name
        size_t nameStart = 1;
        while (nameStart < element.size() && std::isspace(element[nameStart])) nameStart++;
        size_t nameEnd = nameStart;
        while (nameEnd < element.size() && !std::isspace(element[nameEnd]) &&
               element[nameEnd] != '>' && element[nameEnd] != '/') nameEnd++;
        std::string tagName = element.substr(nameStart, nameEnd - nameStart);

        // Parse transform attribute and combine with parent transform
        std::string transformStr = GetAttribute(element, "transform");
        Transform2D elementTransform = ParseTransform(transformStr);

        // Combine transforms: combined = parent * element
        float ca = pa * elementTransform.a + pc * elementTransform.b;
        float cb = pb * elementTransform.a + pd * elementTransform.b;
        float cc = pa * elementTransform.c + pc * elementTransform.d;
        float cd = pb * elementTransform.c + pd * elementTransform.d;
        float ce = pa * elementTransform.e + pc * elementTransform.f + pe;
        float cf = pb * elementTransform.e + pd * elementTransform.f + pf;

        // Parse style attributes - inherit from parent if not specified
        std::string fill = GetAttribute(element, "fill");
        std::string stroke = GetAttribute(element, "stroke");
        float strokeWidth = GetFloatAttribute(element, "stroke-width", -1.0f);  // -1 means not specified
        float opacity = GetFloatAttribute(element, "opacity", -1.0f);  // -1 means not specified
        float fillOpacity = GetFloatAttribute(element, "fill-opacity", 1.0f);
        float strokeOpacity = GetFloatAttribute(element, "stroke-opacity", 1.0f);

        // Apply inheritance: use parent value if element doesn't specify its own
        std::string effectiveFill = fill.empty() ? parentFill : fill;
        std::string effectiveStroke = stroke.empty() ? parentStroke : stroke;
        float effectiveStrokeWidth = (strokeWidth < 0) ? parentStrokeWidth : strokeWidth;
        float effectiveOpacity = (opacity < 0) ? parentOpacity : opacity;

        SVGShape shape;
        // Default fill is black if nothing inherited, otherwise use inherited/specified
        if (effectiveFill.empty()) {
            shape.fillColor = Color::Black();
            shape.hasFill = true;
        } else if (effectiveFill == "none") {
            shape.fillColor = Color::Clear();
            shape.hasFill = false;
        } else if (IsGradientReference(effectiveFill)) {
            // Gradient fill - store the ID, will apply during tessellation
            shape.fillGradientId = ExtractGradientId(effectiveFill);
            shape.fillColor = Color::White();  // Placeholder - will be overridden by gradient
            shape.hasFill = true;
        } else {
            shape.fillColor = ParseColor(effectiveFill);
            shape.hasFill = true;
        }

        // Stroke
        if (effectiveStroke.empty() || effectiveStroke == "none") {
            shape.strokeColor = Color::Clear();
            shape.hasStroke = false;
        } else if (IsGradientReference(effectiveStroke)) {
            shape.strokeGradientId = ExtractGradientId(effectiveStroke);
            shape.strokeColor = Color::White();
            shape.hasStroke = (effectiveStrokeWidth > 0);
        } else {
            shape.strokeColor = ParseColor(effectiveStroke);
            shape.hasStroke = (effectiveStrokeWidth > 0);
        }

        shape.strokeWidth = std::max(0.0f, effectiveStrokeWidth);
        shape.opacity = effectiveOpacity;
        shape.fillColor.a *= fillOpacity * effectiveOpacity;
        shape.strokeColor.a *= strokeOpacity * effectiveOpacity;

        bool shapeAdded = false;

        if (tagName == "rect") {
            float x = GetFloatAttribute(element, "x", 0.0f);
            float y = GetFloatAttribute(element, "y", 0.0f);
            float w = GetFloatAttribute(element, "width", 0.0f);
            float h = GetFloatAttribute(element, "height", 0.0f);
            float rx = GetFloatAttribute(element, "rx", 0.0f);
            float ry = GetFloatAttribute(element, "ry", rx);
            ParseRect(x, y, w, h, rx, ry, shape);
            shapeAdded = true;
        }
        else if (tagName == "circle") {
            float cx = GetFloatAttribute(element, "cx", 0.0f);
            float cy = GetFloatAttribute(element, "cy", 0.0f);
            float r = GetFloatAttribute(element, "r", 0.0f);
            ParseCircle(cx, cy, r, shape);
            shapeAdded = true;
        }
        else if (tagName == "ellipse") {
            float cx = GetFloatAttribute(element, "cx", 0.0f);
            float cy = GetFloatAttribute(element, "cy", 0.0f);
            float rx = GetFloatAttribute(element, "rx", 0.0f);
            float ry = GetFloatAttribute(element, "ry", 0.0f);
            ParseEllipse(cx, cy, rx, ry, shape);
            shapeAdded = true;
        }
        else if (tagName == "line") {
            float x1 = GetFloatAttribute(element, "x1", 0.0f);
            float y1 = GetFloatAttribute(element, "y1", 0.0f);
            float x2 = GetFloatAttribute(element, "x2", 0.0f);
            float y2 = GetFloatAttribute(element, "y2", 0.0f);
            ParseLine(x1, y1, x2, y2, shape);
            shape.hasFill = false;
            shape.hasStroke = true;
            if (shape.strokeWidth <= 0) shape.strokeWidth = 1.0f;
            shapeAdded = true;
        }
        else if (tagName == "polyline") {
            std::string points = GetAttribute(element, "points");
            ParsePolyline(points, shape);
            shape.hasFill = false;
            shapeAdded = true;
        }
        else if (tagName == "polygon") {
            std::string points = GetAttribute(element, "points");
            ParsePolygon(points, shape);
            shapeAdded = true;
        }
        else if (tagName == "path") {
            std::string d = GetAttribute(element, "d");
            ParsePath(d, shape);
            shapeAdded = true;
        }
        else if (tagName == "g") {
            // Group - recurse with the combined transform and inherited styles
            // The group's fill/stroke attributes become the parent for children
            pos = tagEnd + 1;
            if (!selfClosing) {
                ParseSVGElementWithTransform(content, pos, ca, cb, cc, cd, ce, cf,
                                              effectiveFill, effectiveStroke,
                                              effectiveStrokeWidth, effectiveOpacity);
            }
            continue;
        }

        if (shapeAdded && !shape.paths.empty()) {
            // Apply combined transform (parent + element) to shape coordinates
            bool hasTransform = (ca != 1.0f || cb != 0.0f || cc != 0.0f || cd != 1.0f || ce != 0.0f || cf != 0.0f);
            if (hasTransform) {
                Transform2D combinedTransform;
                combinedTransform.a = ca;
                combinedTransform.b = cb;
                combinedTransform.c = cc;
                combinedTransform.d = cd;
                combinedTransform.e = ce;
                combinedTransform.f = cf;
                ApplyTransformToShape(shape, combinedTransform);
                // Also scale stroke width by the transform's scale factor
                float scaleAvg = (std::abs(ca) + std::abs(cd)) * 0.5f;
                shape.strokeWidth *= scaleAvg;
            }
            m_Document->shapes.push_back(std::move(shape));
        }

        pos = tagEnd + 1;
        if (!selfClosing) {
            // Skip to closing tag
            std::string closeTag = "</" + tagName + ">";
            size_t closePos = content.find(closeTag, pos);
            if (closePos != std::string::npos) {
                pos = closePos + closeTag.length();
            }
        }
    }
}

// ============================================================================
// Shape Parsing Functions
// ============================================================================

void VectorGraphic2D::ParseRect(float x, float y, float width, float height, float rx, float ry, SVGShape& shape) {
    SVGPath path;

    if (rx <= 0 && ry <= 0) {
        // Simple rectangle
        path.points = {
            Vec2(x, y),
            Vec2(x + width, y),
            Vec2(x + width, y + height),
            Vec2(x, y + height)
        };
    } else {
        // Rounded rectangle - approximate with segments
        int segments = GetTessellationQuality();
        rx = std::min(rx, width / 2.0f);
        ry = std::min(ry, height / 2.0f);

        // Top-right corner
        for (int i = 0; i <= segments; i++) {
            float angle = -static_cast<float>(M_PI) / 2.0f + (static_cast<float>(M_PI) / 2.0f) * i / segments;
            path.points.push_back(Vec2(x + width - rx + rx * std::cos(angle),
                                       y + ry + ry * std::sin(angle)));
        }
        // Bottom-right corner
        for (int i = 0; i <= segments; i++) {
            float angle = (static_cast<float>(M_PI) / 2.0f) * i / segments;
            path.points.push_back(Vec2(x + width - rx + rx * std::cos(angle),
                                       y + height - ry + ry * std::sin(angle)));
        }
        // Bottom-left corner
        for (int i = 0; i <= segments; i++) {
            float angle = static_cast<float>(M_PI) / 2.0f + (static_cast<float>(M_PI) / 2.0f) * i / segments;
            path.points.push_back(Vec2(x + rx + rx * std::cos(angle),
                                       y + height - ry + ry * std::sin(angle)));
        }
        // Top-left corner
        for (int i = 0; i <= segments; i++) {
            float angle = static_cast<float>(M_PI) + (static_cast<float>(M_PI) / 2.0f) * i / segments;
            path.points.push_back(Vec2(x + rx + rx * std::cos(angle),
                                       y + ry + ry * std::sin(angle)));
        }
    }

    path.closed = true;
    shape.paths.push_back(std::move(path));
}

void VectorGraphic2D::ParseCircle(float cx, float cy, float r, SVGShape& shape) {
    SVGPath path;
    int segments = GetTessellationQuality() * 4;

    for (int i = 0; i < segments; i++) {
        float angle = 2.0f * static_cast<float>(M_PI) * i / segments;
        path.points.push_back(Vec2(cx + r * std::cos(angle), cy + r * std::sin(angle)));
    }

    path.closed = true;
    shape.paths.push_back(std::move(path));
}

void VectorGraphic2D::ParseEllipse(float cx, float cy, float rx, float ry, SVGShape& shape) {
    SVGPath path;
    int segments = GetTessellationQuality() * 4;

    for (int i = 0; i < segments; i++) {
        float angle = 2.0f * static_cast<float>(M_PI) * i / segments;
        path.points.push_back(Vec2(cx + rx * std::cos(angle), cy + ry * std::sin(angle)));
    }

    path.closed = true;
    shape.paths.push_back(std::move(path));
}

void VectorGraphic2D::ParseLine(float x1, float y1, float x2, float y2, SVGShape& shape) {
    SVGPath path;
    path.points = { Vec2(x1, y1), Vec2(x2, y2) };
    path.closed = false;
    shape.paths.push_back(std::move(path));
}

void VectorGraphic2D::ParsePolygon(const std::string& points, SVGShape& shape) {
    SVGPath path;
    size_t pos = 0;

    while (pos < points.size()) {
        SkipCommaWhitespace(points, pos);
        if (pos >= points.size()) break;

        float x = ParseNumber(points, pos);
        SkipCommaWhitespace(points, pos);
        float y = ParseNumber(points, pos);
        path.points.push_back(Vec2(x, y));
    }

    path.closed = true;
    shape.paths.push_back(std::move(path));
}

void VectorGraphic2D::ParsePolyline(const std::string& points, SVGShape& shape) {
    SVGPath path;
    size_t pos = 0;

    while (pos < points.size()) {
        SkipCommaWhitespace(points, pos);
        if (pos >= points.size()) break;

        float x = ParseNumber(points, pos);
        SkipCommaWhitespace(points, pos);
        float y = ParseNumber(points, pos);
        path.points.push_back(Vec2(x, y));
    }

    path.closed = false;
    shape.paths.push_back(std::move(path));
}

void VectorGraphic2D::ParsePath(const std::string& pathData, SVGShape& shape) {
    if (pathData.empty()) return;

    SVGPath currentPath;
    Vec2 currentPos(0, 0);
    Vec2 startPos(0, 0);
    Vec2 lastControl(0, 0);
    char lastCommand = 0;

    size_t pos = 0;
    int quality = GetTessellationQuality();

    while (pos < pathData.size()) {
        SkipWhitespace(pathData, pos);
        if (pos >= pathData.size()) break;

        char cmd = pathData[pos];
        bool isCommand = std::isalpha(cmd);

        if (isCommand) {
            pos++;
            lastCommand = cmd;
        } else {
            cmd = lastCommand;
        }

        bool relative = std::islower(static_cast<unsigned char>(cmd)) != 0;
        cmd = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));

        SkipCommaWhitespace(pathData, pos);

        switch (cmd) {
            case 'M': { // MoveTo
                if (!currentPath.points.empty()) {
                    shape.paths.push_back(std::move(currentPath));
                    currentPath = SVGPath();
                }
                float x = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float y = ParseNumber(pathData, pos);
                if (relative) {
                    currentPos.x += x;
                    currentPos.y += y;
                } else {
                    currentPos = Vec2(x, y);
                }
                startPos = currentPos;
                currentPath.points.push_back(currentPos);
                lastCommand = relative ? 'l' : 'L'; // Subsequent coords are LineTo
                break;
            }
            case 'L': { // LineTo
                float x = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float y = ParseNumber(pathData, pos);
                if (relative) {
                    currentPos.x += x;
                    currentPos.y += y;
                } else {
                    currentPos = Vec2(x, y);
                }
                currentPath.points.push_back(currentPos);
                break;
            }
            case 'H': { // Horizontal LineTo
                float x = ParseNumber(pathData, pos);
                if (relative) {
                    currentPos.x += x;
                } else {
                    currentPos.x = x;
                }
                currentPath.points.push_back(currentPos);
                break;
            }
            case 'V': { // Vertical LineTo
                float y = ParseNumber(pathData, pos);
                if (relative) {
                    currentPos.y += y;
                } else {
                    currentPos.y = y;
                }
                currentPath.points.push_back(currentPos);
                break;
            }
            case 'C': { // Cubic Bezier
                float x1 = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float y1 = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float x2 = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float y2 = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float x = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float y = ParseNumber(pathData, pos);

                Vec2 p1(x1, y1), p2(x2, y2), end(x, y);
                if (relative) {
                    p1 = currentPos + p1;
                    p2 = currentPos + p2;
                    end = currentPos + end;
                }

                AddBezierCurve(currentPath.points, currentPos, p1, p2, end, quality);
                lastControl = p2;
                currentPos = end;
                break;
            }
            case 'S': { // Smooth Cubic Bezier
                Vec2 p1 = currentPos * 2.0f - lastControl;
                float x2 = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float y2 = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float x = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float y = ParseNumber(pathData, pos);

                Vec2 p2(x2, y2), end(x, y);
                if (relative) {
                    p2 = currentPos + p2;
                    end = currentPos + end;
                }

                AddBezierCurve(currentPath.points, currentPos, p1, p2, end, quality);
                lastControl = p2;
                currentPos = end;
                break;
            }
            case 'Q': { // Quadratic Bezier
                float x1 = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float y1 = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float x = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float y = ParseNumber(pathData, pos);

                Vec2 p1(x1, y1), end(x, y);
                if (relative) {
                    p1 = currentPos + p1;
                    end = currentPos + end;
                }

                AddQuadraticCurve(currentPath.points, currentPos, p1, end, quality);
                lastControl = p1;
                currentPos = end;
                break;
            }
            case 'T': { // Smooth Quadratic Bezier
                Vec2 p1 = currentPos * 2.0f - lastControl;
                float x = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float y = ParseNumber(pathData, pos);

                Vec2 end(x, y);
                if (relative) {
                    end = currentPos + end;
                }

                AddQuadraticCurve(currentPath.points, currentPos, p1, end, quality);
                lastControl = p1;
                currentPos = end;
                break;
            }
            case 'A': { // Arc
                float rx = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float ry = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float rotation = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float largeArc = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float sweep = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float x = ParseNumber(pathData, pos);
                SkipCommaWhitespace(pathData, pos);
                float y = ParseNumber(pathData, pos);

                Vec2 end(x, y);
                if (relative) {
                    end = currentPos + end;
                }

                AddArc(currentPath.points, currentPos, rx, ry, rotation,
                       largeArc > 0.5f, sweep > 0.5f, end, quality * 2);
                currentPos = end;
                break;
            }
            case 'Z': { // ClosePath
                currentPath.closed = true;
                currentPos = startPos;
                if (!currentPath.points.empty()) {
                    shape.paths.push_back(std::move(currentPath));
                    currentPath = SVGPath();
                }
                break;
            }
            default:
                pos++;
                break;
        }
    }

    if (!currentPath.points.empty()) {
        shape.paths.push_back(std::move(currentPath));
    }

    // Debug: log parsing results
    size_t totalPoints = 0;
    for (const auto& p : shape.paths) {
        totalPoints += p.points.size();
    }
    LOG_INFO(LogCategory::Core, "VectorGraphic2D::ParsePath - parsed {} subpaths with {} total points from pathData length {}",
             shape.paths.size(), totalPoints, pathData.size());
}

// ============================================================================
// Curve Helper Functions
// ============================================================================

void VectorGraphic2D::AddBezierCurve(std::vector<Vec2>& points,
                                      const Vec2& p0, const Vec2& p1,
                                      const Vec2& p2, const Vec2& p3, int baseSegments) {
    // Adaptive subdivision: estimate curve length and add more segments for longer curves
    float chordLen = std::sqrt((p3.x - p0.x) * (p3.x - p0.x) + (p3.y - p0.y) * (p3.y - p0.y));
    float controlLen = std::sqrt((p1.x - p0.x) * (p1.x - p0.x) + (p1.y - p0.y) * (p1.y - p0.y)) +
                       std::sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y)) +
                       std::sqrt((p3.x - p2.x) * (p3.x - p2.x) + (p3.y - p2.y) * (p3.y - p2.y));

    // More segments for curvier paths (where control polygon is much longer than chord)
    float curviness = controlLen / std::max(chordLen, 0.001f);
    int segments = static_cast<int>(baseSegments * std::max(1.0f, curviness * 0.5f));
    segments = std::min(segments, baseSegments * 4); // Cap at 4x base

    for (int i = 1; i <= segments; i++) {
        float t = static_cast<float>(i) / segments;
        float t2 = t * t;
        float t3 = t2 * t;
        float mt = 1.0f - t;
        float mt2 = mt * mt;
        float mt3 = mt2 * mt;

        Vec2 point = p0 * mt3 + p1 * (3.0f * mt2 * t) + p2 * (3.0f * mt * t2) + p3 * t3;
        points.push_back(point);
    }
}

void VectorGraphic2D::AddQuadraticCurve(std::vector<Vec2>& points,
                                         const Vec2& p0, const Vec2& p1,
                                         const Vec2& p2, int baseSegments) {
    // Adaptive subdivision for quadratic curves
    float chordLen = std::sqrt((p2.x - p0.x) * (p2.x - p0.x) + (p2.y - p0.y) * (p2.y - p0.y));
    float controlLen = std::sqrt((p1.x - p0.x) * (p1.x - p0.x) + (p1.y - p0.y) * (p1.y - p0.y)) +
                       std::sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y));

    float curviness = controlLen / std::max(chordLen, 0.001f);
    int segments = static_cast<int>(baseSegments * std::max(1.0f, curviness * 0.5f));
    segments = std::min(segments, baseSegments * 4);

    for (int i = 1; i <= segments; i++) {
        float t = static_cast<float>(i) / segments;
        float mt = 1.0f - t;

        Vec2 point = p0 * (mt * mt) + p1 * (2.0f * mt * t) + p2 * (t * t);
        points.push_back(point);
    }
}

void VectorGraphic2D::AddArc(std::vector<Vec2>& points,
                              const Vec2& start, float rx, float ry,
                              float rotation, bool largeArc, bool sweep,
                              const Vec2& end, int segments) {
    if (rx == 0 || ry == 0) {
        points.push_back(end);
        return;
    }

    // Convert endpoint parameterization to center parameterization
    float phi = rotation * static_cast<float>(M_PI) / 180.0f;
    float cosPhi = std::cos(phi);
    float sinPhi = std::sin(phi);

    // Step 1: Compute (x1', y1')
    float dx = (start.x - end.x) / 2.0f;
    float dy = (start.y - end.y) / 2.0f;
    float x1p = cosPhi * dx + sinPhi * dy;
    float y1p = -sinPhi * dx + cosPhi * dy;

    // Correct radii
    float x1p2 = x1p * x1p;
    float y1p2 = y1p * y1p;
    float rx2 = rx * rx;
    float ry2 = ry * ry;

    float lambda = x1p2 / rx2 + y1p2 / ry2;
    if (lambda > 1.0f) {
        float sqrtLambda = std::sqrt(lambda);
        rx *= sqrtLambda;
        ry *= sqrtLambda;
        rx2 = rx * rx;
        ry2 = ry * ry;
    }

    // Step 2: Compute (cx', cy')
    float sq = (rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2) / (rx2 * y1p2 + ry2 * x1p2);
    sq = std::max(0.0f, sq);
    float coef = std::sqrt(sq) * ((largeArc == sweep) ? -1.0f : 1.0f);
    float cxp = coef * rx * y1p / ry;
    float cyp = -coef * ry * x1p / rx;

    // Step 3: Compute (cx, cy)
    float cx = cosPhi * cxp - sinPhi * cyp + (start.x + end.x) / 2.0f;
    float cy = sinPhi * cxp + cosPhi * cyp + (start.y + end.y) / 2.0f;

    // Step 4: Compute theta1 and dtheta
    auto vectorAngle = [](float ux, float uy, float vx, float vy) -> float {
        float n = std::sqrt(ux * ux + uy * uy) * std::sqrt(vx * vx + vy * vy);
        if (n == 0) return 0;
        float c = (ux * vx + uy * vy) / n;
        c = std::max(-1.0f, std::min(1.0f, c));
        float angle = std::acos(c);
        if (ux * vy - uy * vx < 0) angle = -angle;
        return angle;
    };

    float theta1 = vectorAngle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    float dtheta = vectorAngle((x1p - cxp) / rx, (y1p - cyp) / ry,
                               (-x1p - cxp) / rx, (-y1p - cyp) / ry);

    if (!sweep && dtheta > 0) dtheta -= 2.0f * static_cast<float>(M_PI);
    if (sweep && dtheta < 0) dtheta += 2.0f * static_cast<float>(M_PI);

    // Generate arc points
    for (int i = 1; i <= segments; i++) {
        float t = static_cast<float>(i) / segments;
        float theta = theta1 + t * dtheta;

        float x = rx * std::cos(theta);
        float y = ry * std::sin(theta);

        // Rotate and translate
        float px = cosPhi * x - sinPhi * y + cx;
        float py = sinPhi * x + cosPhi * y + cy;

        points.push_back(Vec2(px, py));
    }
}

// ============================================================================
// Gradient Helpers
// ============================================================================

// Debug counter for gradient sampling
static int s_gradientSampleCount = 0;

// Calculate gradient color for a point
Color SampleGradientAtPoint(const SVGGradient& gradient, const Vec2& point,
                            float minX, float minY, float maxX, float maxY) {
    float t = 0.0f;
    s_gradientSampleCount++;
    bool shouldLog = (s_gradientSampleCount <= 10);  // Log first 10 samples

    if (gradient.type == SVGGradient::Type::Linear) {
        // Linear gradient - project point onto gradient line
        float gx1, gy1, gx2, gy2;

        if (gradient.objectBoundingBox) {
            // Coordinates are in 0-1 range relative to bounding box
            gx1 = minX + gradient.x1 * (maxX - minX);
            gy1 = minY + gradient.y1 * (maxY - minY);
            gx2 = minX + gradient.x2 * (maxX - minX);
            gy2 = minY + gradient.y2 * (maxY - minY);
        } else {
            // Coordinates are in user space (SVG coordinates)
            gx1 = gradient.x1;
            gy1 = gradient.y1;
            gx2 = gradient.x2;
            gy2 = gradient.y2;
        }

        // Apply gradient transform if present
        if (gradient.hasTransform) {
            float tx1 = gradient.transformA * gx1 + gradient.transformC * gy1 + gradient.transformE;
            float ty1 = gradient.transformB * gx1 + gradient.transformD * gy1 + gradient.transformF;
            float tx2 = gradient.transformA * gx2 + gradient.transformC * gy2 + gradient.transformE;
            float ty2 = gradient.transformB * gx2 + gradient.transformD * gy2 + gradient.transformF;
            gx1 = tx1; gy1 = ty1;
            gx2 = tx2; gy2 = ty2;
        }

        // Project point onto gradient vector
        float dx = gx2 - gx1;
        float dy = gy2 - gy1;
        float len2 = dx * dx + dy * dy;

        if (len2 > 0.0001f) {
            t = ((point.x - gx1) * dx + (point.y - gy1) * dy) / len2;
        }
    } else {
        // Radial gradient - distance from center
        float gcx, gcy, gr;

        if (gradient.objectBoundingBox) {
            gcx = minX + gradient.cx * (maxX - minX);
            gcy = minY + gradient.cy * (maxY - minY);
            // Radius is relative to bounding box size
            gr = gradient.r * std::max(maxX - minX, maxY - minY);
        } else {
            gcx = gradient.cx;
            gcy = gradient.cy;
            gr = gradient.r;
        }

        // For transforms, we need to inverse-transform the point to gradient space
        // instead of transforming the gradient to user space
        if (gradient.hasTransform) {
            // Calculate inverse transform
            // For matrix [a c e; b d f; 0 0 1], inverse is:
            // det = a*d - b*c
            // [d/det -c/det (c*f-d*e)/det; -b/det a/det (b*e-a*f)/det; 0 0 1]
            float det = gradient.transformA * gradient.transformD - gradient.transformB * gradient.transformC;
            if (std::abs(det) > 0.0001f) {
                float invA = gradient.transformD / det;
                float invB = -gradient.transformB / det;
                float invC = -gradient.transformC / det;
                float invD = gradient.transformA / det;
                float invE = (gradient.transformC * gradient.transformF - gradient.transformD * gradient.transformE) / det;
                float invF = (gradient.transformB * gradient.transformE - gradient.transformA * gradient.transformF) / det;

                // Transform point to gradient space
                float px = invA * point.x + invC * point.y + invE;
                float py = invB * point.x + invD * point.y + invF;

                // Distance from center in gradient space, normalized by radius
                float dist = std::sqrt((px - gcx) * (px - gcx) + (py - gcy) * (py - gcy));
                t = (gr > 0.0001f) ? (dist / gr) : 0.0f;
            } else {
                t = 0.0f;
            }
        } else {
            // No transform - simple distance calculation
            float dist = std::sqrt((point.x - gcx) * (point.x - gcx) + (point.y - gcy) * (point.y - gcy));
            t = (gr > 0.0001f) ? (dist / gr) : 0.0f;
        }
    }

    Color result = gradient.SampleColor(t);
    if (shouldLog) {
        LOG_INFO(LogCategory::Core, "  GradientSample: point=({},{}), t={}, color=({},{},{},{})",
                 point.x, point.y, t, result.r, result.g, result.b, result.a);
    }
    return result;
}

// Calculate bounding box of points
void CalculateBoundingBox(const std::vector<Vec2>& points, float& minX, float& minY, float& maxX, float& maxY) {
    if (points.empty()) {
        minX = minY = 0.0f;
        maxX = maxY = 1.0f;
        return;
    }
    minX = maxX = points[0].x;
    minY = maxY = points[0].y;
    for (const auto& p : points) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
    }
}

// ============================================================================
// Tessellation Functions
// ============================================================================

void VectorGraphic2D::TessellateShape(const SVGShape& shape, float zDepth) {
    Color tint = GetTint();
    float opacity = GetOpacity();

    static int shapeDebugCount = 0;
    if (shapeDebugCount < 5) {
        LOG_INFO(LogCategory::Core, "VectorGraphic2D: Shape hasFill={}, hasStroke={}, strokeWidth={}, fillColor=({},{},{},{}), strokeColor=({},{},{},{}), paths={}",
                 shape.hasFill, shape.hasStroke, shape.strokeWidth,
                 shape.fillColor.r, shape.fillColor.g, shape.fillColor.b, shape.fillColor.a,
                 shape.strokeColor.r, shape.strokeColor.g, shape.strokeColor.b, shape.strokeColor.a,
                 shape.paths.size());
        shapeDebugCount++;
    }

    // Stroke should be slightly in front of fill (smaller z = closer with GL_LESS)
    float fillZ = zDepth;
    float strokeZ = zDepth - 0.0001f;  // Stroke slightly closer than fill

    // Apply tint and opacity to colors
    Color fillColor = shape.fillColor;
    fillColor.r *= tint.r;
    fillColor.g *= tint.g;
    fillColor.b *= tint.b;
    fillColor.a *= tint.a * opacity;

    Color strokeColor = shape.strokeColor;
    strokeColor.r *= tint.r;
    strokeColor.g *= tint.g;
    strokeColor.b *= tint.b;
    strokeColor.a *= tint.a * opacity;

    // Check for gradient fill
    const SVGGradient* fillGradient = nullptr;
    float boundsMinX = 0, boundsMinY = 0, boundsMaxX = 0, boundsMaxY = 0;

    if (!shape.fillGradientId.empty() && m_Document) {
        LOG_INFO(LogCategory::Core, "  Looking for gradient: '{}'", shape.fillGradientId);
        LOG_INFO(LogCategory::Core, "  Document has {} gradients", m_Document->gradients.size());
        for (const auto& kv : m_Document->gradients) {
            LOG_INFO(LogCategory::Core, "    - '{}' ({} stops)", kv.first, kv.second.stops.size());
        }
        auto it = m_Document->gradients.find(shape.fillGradientId);
        if (it != m_Document->gradients.end()) {
            fillGradient = &it->second;
            LOG_INFO(LogCategory::Core, "  Found gradient '{}' with {} stops",
                     shape.fillGradientId, fillGradient->stops.size());
            for (size_t si = 0; si < fillGradient->stops.size(); si++) {
                LOG_INFO(LogCategory::Core, "    Stop {}: offset={}, color=({},{},{},{})",
                         si, fillGradient->stops[si].offset,
                         fillGradient->stops[si].color.r,
                         fillGradient->stops[si].color.g,
                         fillGradient->stops[si].color.b,
                         fillGradient->stops[si].color.a);
            }
            // Calculate bounding box of all paths for gradient
            bool first = true;
            for (const auto& path : shape.paths) {
                for (const auto& p : path.points) {
                    if (first) {
                        boundsMinX = boundsMaxX = p.x;
                        boundsMinY = boundsMaxY = p.y;
                        first = false;
                    } else {
                        boundsMinX = std::min(boundsMinX, p.x);
                        boundsMinY = std::min(boundsMinY, p.y);
                        boundsMaxX = std::max(boundsMaxX, p.x);
                        boundsMaxY = std::max(boundsMaxY, p.y);
                    }
                }
            }
            LOG_INFO(LogCategory::Core, "  Using gradient '{}' with bounds ({},{}) to ({},{})",
                     shape.fillGradientId, boundsMinX, boundsMinY, boundsMaxX, boundsMaxY);
        }
    }

    // Handle fill - for multi-path shapes, use polygon-with-holes triangulation
    if (shape.hasFill && shape.paths.size() > 0) {
        // Collect all closed paths that can be filled
        std::vector<std::vector<Vec2>> fillablePaths;
        for (size_t pathIdx = 0; pathIdx < shape.paths.size(); pathIdx++) {
            const auto& path = shape.paths[pathIdx];
            if (path.closed && path.points.size() >= 3) {
                fillablePaths.push_back(path.points);
            }
        }

        if (fillablePaths.size() == 1) {
            // Single path - simple polygon fill
            LOG_INFO(LogCategory::Core, "  Fill: single path with {} points", fillablePaths[0].size());
            TessellatePolygonWithGradient(fillablePaths[0], fillColor, fillZ, m_VertexData,
                                          fillGradient, boundsMinX, boundsMinY, boundsMaxX, boundsMaxY, tint, opacity);
        } else if (fillablePaths.size() > 1) {
            // Multiple paths - need to determine which are separate shapes vs holes
            // Use point-in-polygon to check containment relationships
            LOG_INFO(LogCategory::Core, "  Fill: {} paths, analyzing containment...", fillablePaths.size());

            // Helper: point-in-polygon test (ray casting)
            auto pointInPolygon = [](const Vec2& point, const std::vector<Vec2>& poly) -> bool {
                int crossings = 0;
                for (size_t i = 0; i < poly.size(); i++) {
                    size_t j = (i + 1) % poly.size();
                    const Vec2& p1 = poly[i];
                    const Vec2& p2 = poly[j];

                    if ((p1.y <= point.y && p2.y > point.y) ||
                        (p2.y <= point.y && p1.y > point.y)) {
                        float t = (point.y - p1.y) / (p2.y - p1.y);
                        float xIntersect = p1.x + t * (p2.x - p1.x);
                        if (point.x < xIntersect) {
                            crossings++;
                        }
                    }
                }
                return (crossings % 2) == 1;
            };

            // Helper: get a point that's likely inside a polygon (centroid)
            auto getCentroid = [](const std::vector<Vec2>& poly) -> Vec2 {
                Vec2 sum(0, 0);
                for (const auto& p : poly) {
                    sum.x += p.x;
                    sum.y += p.y;
                }
                return Vec2(sum.x / poly.size(), sum.y / poly.size());
            };

            // Helper: calculate signed area
            auto signedArea = [](const std::vector<Vec2>& poly) -> float {
                float area = 0.0f;
                for (size_t i = 0; i < poly.size(); i++) {
                    size_t j = (i + 1) % poly.size();
                    area += poly[i].x * poly[j].y;
                    area -= poly[j].x * poly[i].y;
                }
                return area * 0.5f;
            };

            // Build containment relationships: containedBy[i] = index of polygon containing i, or -1
            std::vector<int> containedBy(fillablePaths.size(), -1);
            std::vector<float> areas(fillablePaths.size());

            for (size_t i = 0; i < fillablePaths.size(); i++) {
                areas[i] = std::abs(signedArea(fillablePaths[i]));
            }

            // Check each polygon against all larger polygons
            for (size_t i = 0; i < fillablePaths.size(); i++) {
                Vec2 testPoint = getCentroid(fillablePaths[i]);
                float smallestContainerArea = 1e30f;
                int smallestContainer = -1;

                for (size_t j = 0; j < fillablePaths.size(); j++) {
                    if (i == j) continue;
                    // Only larger polygons can contain smaller ones
                    if (areas[j] <= areas[i]) continue;

                    if (pointInPolygon(testPoint, fillablePaths[j])) {
                        // j contains i - but we want the smallest container (immediate parent)
                        if (areas[j] < smallestContainerArea) {
                            smallestContainerArea = areas[j];
                            smallestContainer = static_cast<int>(j);
                        }
                    }
                }
                containedBy[i] = smallestContainer;
            }

            // Group into outer polygons and their direct holes
            // A polygon is an "outer" if containedBy[i] == -1 OR if its container's container exists
            // (even-odd rule: depth 0 = fill, depth 1 = hole, depth 2 = fill, etc.)
            std::vector<int> depth(fillablePaths.size(), 0);
            for (size_t i = 0; i < fillablePaths.size(); i++) {
                int d = 0;
                int current = static_cast<int>(i);
                while (containedBy[current] != -1) {
                    d++;
                    current = containedBy[current];
                }
                depth[i] = d;
            }

            // Collect outer polygons (even depth) and their immediate holes (odd depth children)
            for (size_t i = 0; i < fillablePaths.size(); i++) {
                if (depth[i] % 2 == 0) {
                    // This is an outer polygon (or nested fill region)
                    std::vector<std::vector<Vec2>> outerWithHoles;
                    outerWithHoles.push_back(fillablePaths[i]);

                    // Find all immediate children (depth = depth[i] + 1) that are holes
                    for (size_t j = 0; j < fillablePaths.size(); j++) {
                        if (containedBy[j] == static_cast<int>(i) && depth[j] == depth[i] + 1) {
                            outerWithHoles.push_back(fillablePaths[j]);
                        }
                    }

                    LOG_INFO(LogCategory::Core, "    Outer polygon {} (depth {}) with {} holes",
                             i, depth[i], outerWithHoles.size() - 1);

                    if (outerWithHoles.size() == 1) {
                        TessellatePolygonWithGradient(outerWithHoles[0], fillColor, fillZ, m_VertexData,
                                                      fillGradient, boundsMinX, boundsMinY, boundsMaxX, boundsMaxY, tint, opacity);
                    } else {
                        TessellatePolygonWithHolesAndGradient(outerWithHoles, fillColor, fillZ, m_VertexData,
                                                              fillGradient, boundsMinX, boundsMinY, boundsMaxX, boundsMaxY, tint, opacity);
                    }
                }
            }
        }
    }

    // Handle strokes - each path gets its own stroke
    if (shape.hasStroke && shape.strokeWidth > 0) {
        // Calculate the scale factor being applied to geometry
        Vec2 size = GetSize();
        float uniformScale = GetUniformScale();
        Vec2 originalSize = m_Document->size;
        float scaleX = size.x / originalSize.x * uniformScale;
        float scaleY = size.y / originalSize.y * uniformScale;
        if (GetPreserveAspectRatio()) {
            float minScale = std::min(std::abs(scaleX), std::abs(scaleY));
            scaleX = scaleY = minScale;
        }
        // Use average scale for stroke width
        float strokeScale = (std::abs(scaleX) + std::abs(scaleY)) * 0.5f;
        float scaledStrokeWidth = shape.strokeWidth * strokeScale;

        for (size_t pathIdx = 0; pathIdx < shape.paths.size(); pathIdx++) {
            const auto& path = shape.paths[pathIdx];
            if (path.points.size() >= 2) {
                LOG_INFO(LogCategory::Core, "  Stroke: path {} with {} points", pathIdx, path.points.size());
                TessellateStroke(path.points, scaledStrokeWidth, strokeColor, strokeZ, m_VertexData, path.closed);
            }
        }
    }
}

void VectorGraphic2D::TessellatePolygon(const std::vector<Vec2>& points,
                                         const Color& color, float zDepth,
                                         std::vector<float>& vertices) {
    if (points.size() < 3) return;

    // Transform points based on current settings
    std::vector<Vec2> transformedPoints;
    transformedPoints.reserve(points.size());
    for (const auto& p : points) {
        transformedPoints.push_back(TransformPoint(p));
    }

    // Remove duplicate consecutive points
    std::vector<Vec2> cleanPoints;
    cleanPoints.push_back(transformedPoints[0]);
    for (size_t i = 1; i < transformedPoints.size(); i++) {
        Vec2 diff = transformedPoints[i] - cleanPoints.back();
        if (diff.x * diff.x + diff.y * diff.y > 0.0001f) {
            cleanPoints.push_back(transformedPoints[i]);
        }
    }
    // Also check first vs last
    if (cleanPoints.size() > 1) {
        Vec2 diff = cleanPoints.back() - cleanPoints.front();
        if (diff.x * diff.x + diff.y * diff.y < 0.0001f) {
            cleanPoints.pop_back();
        }
    }
    if (cleanPoints.size() < 3) return;

    // Helper: calculate cross product of vectors (b-a) and (c-a)
    auto cross2D = [](const Vec2& a, const Vec2& b, const Vec2& c) -> float {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    };

    // Helper: check if point p is inside triangle abc
    auto pointInTriangle = [&cross2D](const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) -> bool {
        float d1 = cross2D(p, a, b);
        float d2 = cross2D(p, b, c);
        float d3 = cross2D(p, c, a);
        bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        return !(hasNeg && hasPos);
    };

    // Calculate signed area to determine winding order
    float signedArea = 0.0f;
    for (size_t i = 0; i < cleanPoints.size(); i++) {
        size_t j = (i + 1) % cleanPoints.size();
        signedArea += cleanPoints[i].x * cleanPoints[j].y;
        signedArea -= cleanPoints[j].x * cleanPoints[i].y;
    }
    signedArea *= 0.5f;

    // Ensure counter-clockwise winding for ear clipping
    bool ccw = signedArea > 0;
    if (!ccw) {
        std::reverse(cleanPoints.begin(), cleanPoints.end());
    }

    // Ear clipping algorithm
    std::list<size_t> remaining;
    for (size_t i = 0; i < cleanPoints.size(); i++) {
        remaining.push_back(i);
    }

    size_t triangleCount = 0;
    auto addTriangle = [&](const Vec2& a, const Vec2& b, const Vec2& c) {
        triangleCount++;
        // Add vertices with reversed winding for Y-flip
        vertices.push_back(a.x); vertices.push_back(a.y); vertices.push_back(zDepth);
        vertices.push_back(color.r); vertices.push_back(color.g);
        vertices.push_back(color.b); vertices.push_back(color.a);

        vertices.push_back(c.x); vertices.push_back(c.y); vertices.push_back(zDepth);
        vertices.push_back(color.r); vertices.push_back(color.g);
        vertices.push_back(color.b); vertices.push_back(color.a);

        vertices.push_back(b.x); vertices.push_back(b.y); vertices.push_back(zDepth);
        vertices.push_back(color.r); vertices.push_back(color.g);
        vertices.push_back(color.b); vertices.push_back(color.a);
    };

    int maxIterations = static_cast<int>(cleanPoints.size()) * 3;  // Safety limit
    int iterations = 0;
    int forcedRemovals = 0;

    while (remaining.size() > 3 && iterations < maxIterations) {
        iterations++;
        bool earFound = false;

        auto it = remaining.begin();
        while (it != remaining.end()) {
            // Get prev, current, next indices
            auto prevIt = it;
            if (prevIt == remaining.begin()) {
                prevIt = remaining.end();
            }
            --prevIt;

            auto nextIt = it;
            ++nextIt;
            if (nextIt == remaining.end()) {
                nextIt = remaining.begin();
            }

            const Vec2& prev = cleanPoints[*prevIt];
            const Vec2& curr = cleanPoints[*it];
            const Vec2& next = cleanPoints[*nextIt];

            // Check if this is a convex vertex (ear candidate)
            float cross = cross2D(prev, curr, next);
            if (cross > 0.0f) {  // Convex vertex in CCW polygon
                // Check if any other vertex is inside this triangle
                bool isEar = true;
                for (auto checkIt = remaining.begin(); checkIt != remaining.end() && isEar; ++checkIt) {
                    if (checkIt == prevIt || checkIt == it || checkIt == nextIt) continue;
                    if (pointInTriangle(cleanPoints[*checkIt], prev, curr, next)) {
                        isEar = false;
                    }
                }

                if (isEar) {
                    // Found an ear - add triangle and remove vertex
                    addTriangle(prev, curr, next);
                    it = remaining.erase(it);
                    earFound = true;
                    break;
                }
            }
            ++it;
        }

        // If no ear found, the polygon might be degenerate - force remove a vertex
        if (!earFound && remaining.size() > 3) {
            forcedRemovals++;
            auto forcedIt = remaining.begin();
            auto prevIt = remaining.end(); --prevIt;
            auto nextIt = forcedIt; ++nextIt;
            addTriangle(cleanPoints[*prevIt], cleanPoints[*forcedIt], cleanPoints[*nextIt]);
            remaining.erase(forcedIt);
        }
    }

    // Handle the last triangle
    if (remaining.size() == 3) {
        auto it = remaining.begin();
        size_t i0 = *it++;
        size_t i1 = *it++;
        size_t i2 = *it;
        addTriangle(cleanPoints[i0], cleanPoints[i1], cleanPoints[i2]);
    }

    LOG_INFO(LogCategory::Core, "    TessellatePolygon: {} points -> {} triangles (forced: {}, iterations: {})",
             cleanPoints.size(), triangleCount, forcedRemovals, iterations);
}

void VectorGraphic2D::TessellatePolygonWithHoles(const std::vector<std::vector<Vec2>>& paths,
                                                   const Color& color, float zDepth,
                                                   std::vector<float>& vertices) {
    if (paths.empty()) return;

    // Helper: calculate signed area (positive = CCW, negative = CW)
    auto signedArea = [](const std::vector<Vec2>& poly) -> float {
        float area = 0.0f;
        for (size_t i = 0; i < poly.size(); i++) {
            size_t j = (i + 1) % poly.size();
            area += poly[i].x * poly[j].y;
            area -= poly[j].x * poly[i].y;
        }
        return area * 0.5f;
    };

    // Helper: check if polygon is CCW
    auto isCCW = [&signedArea](const std::vector<Vec2>& poly) -> bool {
        return signedArea(poly) > 0;
    };

    // Transform all paths and determine outer vs holes based on winding
    std::vector<std::vector<Vec2>> transformedPaths;
    transformedPaths.reserve(paths.size());

    for (const auto& path : paths) {
        std::vector<Vec2> transformed;
        transformed.reserve(path.size());
        for (const auto& p : path) {
            transformed.push_back(TransformPoint(p));
        }
        // Remove duplicate consecutive points
        std::vector<Vec2> clean;
        clean.push_back(transformed[0]);
        for (size_t i = 1; i < transformed.size(); i++) {
            Vec2 diff = transformed[i] - clean.back();
            if (diff.x * diff.x + diff.y * diff.y > 0.0001f) {
                clean.push_back(transformed[i]);
            }
        }
        if (clean.size() >= 3) {
            transformedPaths.push_back(clean);
        }
    }

    if (transformedPaths.empty()) return;

    // Find the outer contour (largest absolute area) and treat others as holes
    size_t outerIdx = 0;
    float maxArea = 0.0f;
    for (size_t i = 0; i < transformedPaths.size(); i++) {
        float area = std::abs(signedArea(transformedPaths[i]));
        if (area > maxArea) {
            maxArea = area;
            outerIdx = i;
        }
    }

    // Get outer contour (ensure CCW)
    std::vector<Vec2> outer = transformedPaths[outerIdx];
    if (!isCCW(outer)) {
        std::reverse(outer.begin(), outer.end());
    }

    // Collect holes (ensure CW - opposite of outer)
    std::vector<std::vector<Vec2>> holes;
    for (size_t i = 0; i < transformedPaths.size(); i++) {
        if (i != outerIdx) {
            std::vector<Vec2> hole = transformedPaths[i];
            if (isCCW(hole)) {
                std::reverse(hole.begin(), hole.end());  // Make CW
            }
            holes.push_back(hole);
        }
    }

    // If no holes, just tessellate the outer polygon
    if (holes.empty()) {
        TessellatePolygon(paths[outerIdx], color, zDepth, vertices);
        return;
    }

    // Merge holes into outer contour using bridge edges
    // Algorithm: For each hole, find the rightmost point, cast a ray to the right,
    // find where it intersects the outer polygon, and create a bridge

    auto cross2D = [](const Vec2& a, const Vec2& b, const Vec2& c) -> float {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    };

    // Sort holes by their rightmost X coordinate (process right-to-left for stability)
    std::vector<size_t> holeOrder(holes.size());
    for (size_t i = 0; i < holes.size(); i++) holeOrder[i] = i;
    std::sort(holeOrder.begin(), holeOrder.end(), [&holes](size_t a, size_t b) {
        float maxXa = -1e30f, maxXb = -1e30f;
        for (const auto& p : holes[a]) maxXa = std::max(maxXa, p.x);
        for (const auto& p : holes[b]) maxXb = std::max(maxXb, p.x);
        return maxXa > maxXb;  // Descending order
    });

    std::vector<Vec2> merged = outer;

    for (size_t holeIdx : holeOrder) {
        const auto& hole = holes[holeIdx];

        // Find rightmost vertex of the hole
        size_t rightmostIdx = 0;
        float maxX = hole[0].x;
        for (size_t i = 1; i < hole.size(); i++) {
            if (hole[i].x > maxX) {
                maxX = hole[i].x;
                rightmostIdx = i;
            }
        }
        Vec2 holePoint = hole[rightmostIdx];

        // Cast ray to the right from holePoint, find closest intersection with merged polygon
        float minDist = 1e30f;
        size_t bridgeEdgeIdx = 0;
        Vec2 intersectionPoint = holePoint;
        bool foundIntersection = false;

        for (size_t i = 0; i < merged.size(); i++) {
            size_t j = (i + 1) % merged.size();
            Vec2 p1 = merged[i];
            Vec2 p2 = merged[j];

            // Check if edge crosses the horizontal ray from holePoint to the right
            if ((p1.y <= holePoint.y && p2.y > holePoint.y) ||
                (p2.y <= holePoint.y && p1.y > holePoint.y)) {
                // Find X intersection
                float t = (holePoint.y - p1.y) / (p2.y - p1.y);
                float xIntersect = p1.x + t * (p2.x - p1.x);

                if (xIntersect > holePoint.x) {
                    float dist = xIntersect - holePoint.x;
                    if (dist < minDist) {
                        minDist = dist;
                        intersectionPoint = Vec2(xIntersect, holePoint.y);
                        bridgeEdgeIdx = i;
                        foundIntersection = true;
                    }
                }
            }
        }

        if (!foundIntersection) {
            // Fallback: couldn't find intersection, skip this hole
            continue;
        }

        // Find the mutually visible vertex to bridge to
        // For a CCW polygon, when we intersect an edge, the visible vertex depends on edge direction
        size_t nextEdgeIdx = (bridgeEdgeIdx + 1) % merged.size();
        Vec2 edgeV1 = merged[bridgeEdgeIdx];
        Vec2 edgeV2 = merged[nextEdgeIdx];

        // For CCW outer polygon:
        // - If edge goes UP (v1.y < v2.y), visible vertex is v1 (lower vertex)
        // - If edge goes DOWN (v1.y > v2.y), visible vertex is v2 (lower vertex)
        // Actually for a ray going right, we want the vertex on the right side of the ray
        // which is the endpoint with the larger X. But we also need to account for reflex vertices.

        // Start with the endpoint that has larger X (is further right)
        size_t connectIdx = (edgeV1.x >= edgeV2.x) ? bridgeEdgeIdx : nextEdgeIdx;
        Vec2 candidateM = merged[connectIdx];

        // Check if the intersection point IS a vertex (within tolerance)
        const float eps = 0.0001f;
        float distToV1 = std::sqrt((intersectionPoint.x - edgeV1.x) * (intersectionPoint.x - edgeV1.x) +
                                    (intersectionPoint.y - edgeV1.y) * (intersectionPoint.y - edgeV1.y));
        float distToV2 = std::sqrt((intersectionPoint.x - edgeV2.x) * (intersectionPoint.x - edgeV2.x) +
                                    (intersectionPoint.y - edgeV2.y) * (intersectionPoint.y - edgeV2.y));

        if (distToV1 < eps) {
            connectIdx = bridgeEdgeIdx;
        } else if (distToV2 < eps) {
            connectIdx = nextEdgeIdx;
        } else {
            // The intersection is on the edge interior, need to find mutually visible vertex
            // Look for reflex vertices inside the triangle (holePoint, intersectionPoint, candidateM)
            // The search region is the triangle formed by these three points

            // Check if a point is strictly inside a triangle (not on edges)
            auto pointInTriangleStrict = [](const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) -> bool {
                auto sign = [](const Vec2& p1, const Vec2& p2, const Vec2& p3) -> float {
                    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
                };
                float d1 = sign(p, a, b);
                float d2 = sign(p, b, c);
                float d3 = sign(p, c, a);
                // For strict interior, all signs must be same and non-zero
                const float tolerance = 1e-6f;
                if (d1 > tolerance && d2 > tolerance && d3 > tolerance) return true;
                if (d1 < -tolerance && d2 < -tolerance && d3 < -tolerance) return true;
                return false;
            };

            // Find any vertex inside the search triangle that minimizes angle to holePoint
            // The search triangle vertices should be ordered consistently
            Vec2 triA = holePoint;
            Vec2 triB = intersectionPoint;
            Vec2 triC = candidateM;

            float minAngle = 1e30f;
            size_t bestIdx = connectIdx;

            for (size_t i = 0; i < merged.size(); i++) {
                if (i == bridgeEdgeIdx || i == nextEdgeIdx) continue;  // Skip edge vertices
                Vec2 v = merged[i];

                // Must be to the right of holePoint
                if (v.x <= holePoint.x + eps) continue;

                // Check if this vertex is inside the search triangle
                // Try both triangle orientations to handle any winding
                bool inside = pointInTriangleStrict(v, triA, triB, triC) ||
                              pointInTriangleStrict(v, triA, triC, triB);

                if (inside) {
                    // Calculate angle from holePoint to this vertex (relative to horizontal)
                    float dy = v.y - holePoint.y;
                    float dx = v.x - holePoint.x;
                    float angle = std::abs(std::atan2(dy, dx));  // Angle magnitude from horizontal
                    if (angle < minAngle) {
                        minAngle = angle;
                        bestIdx = i;
                    }
                }
            }
            connectIdx = bestIdx;
        }

        // Insert hole into merged polygon at connectIdx
        // The insertion creates: ... merged[connectIdx] -> hole[rightmostIdx] -> ... -> hole[rightmostIdx] -> merged[connectIdx] ...
        std::vector<Vec2> newMerged;
        newMerged.reserve(merged.size() + hole.size() + 2);

        // Add points up to and including connectIdx
        for (size_t i = 0; i <= connectIdx; i++) {
            newMerged.push_back(merged[i]);
        }

        // Add hole points starting from rightmostIdx, going around
        for (size_t i = 0; i < hole.size(); i++) {
            newMerged.push_back(hole[(rightmostIdx + i) % hole.size()]);
        }

        // Add bridge back: hole[rightmostIdx] again and merged[connectIdx] again
        newMerged.push_back(hole[rightmostIdx]);
        newMerged.push_back(merged[connectIdx]);

        // Add remaining points from merged
        for (size_t i = connectIdx + 1; i < merged.size(); i++) {
            newMerged.push_back(merged[i]);
        }

        merged = newMerged;
    }

    // Now tessellate the merged polygon (which is now a simple polygon)
    // Use the existing ear-clipping logic but directly on the merged points
    // Since we already have transformed points, we'll add triangles directly

    if (merged.size() < 3) {
        LOG_INFO(LogCategory::Core, "    TessellatePolygonWithHoles: merged polygon too small ({})", merged.size());
        return;
    }

    LOG_INFO(LogCategory::Core, "    TessellatePolygonWithHoles: merged polygon has {} vertices", merged.size());

    size_t triangleCount = 0;
    auto addTriangle = [&](const Vec2& a, const Vec2& b, const Vec2& c) {
        triangleCount++;
        vertices.push_back(a.x); vertices.push_back(a.y); vertices.push_back(zDepth);
        vertices.push_back(color.r); vertices.push_back(color.g);
        vertices.push_back(color.b); vertices.push_back(color.a);

        vertices.push_back(c.x); vertices.push_back(c.y); vertices.push_back(zDepth);
        vertices.push_back(color.r); vertices.push_back(color.g);
        vertices.push_back(color.b); vertices.push_back(color.a);

        vertices.push_back(b.x); vertices.push_back(b.y); vertices.push_back(zDepth);
        vertices.push_back(color.r); vertices.push_back(color.g);
        vertices.push_back(color.b); vertices.push_back(color.a);
    };

    // Strictly inside test - points on edges return false
    auto pointInTriangleStrict = [&cross2D](const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) -> bool {
        float d1 = cross2D(p, a, b);
        float d2 = cross2D(p, b, c);
        float d3 = cross2D(p, c, a);
        // Require all strictly positive or all strictly negative (not on edge)
        const float eps = 0.0001f;
        if (d1 > eps && d2 > eps && d3 > eps) return true;
        if (d1 < -eps && d2 < -eps && d3 < -eps) return true;
        return false;
    };

    // Ensure CCW winding for ear clipping
    if (!isCCW(merged)) {
        std::reverse(merged.begin(), merged.end());
    }

    // Remove duplicate consecutive vertices (created by bridge insertion)
    std::vector<Vec2> cleanMerged;
    cleanMerged.reserve(merged.size());
    for (size_t i = 0; i < merged.size(); i++) {
        const Vec2& curr = merged[i];
        const Vec2& next = merged[(i + 1) % merged.size()];
        float dx = next.x - curr.x;
        float dy = next.y - curr.y;
        if (dx * dx + dy * dy > 0.0001f) {
            cleanMerged.push_back(curr);
        }
    }
    if (cleanMerged.size() < 3) {
        LOG_INFO(LogCategory::Core, "    TessellatePolygonWithHoles: after cleaning, polygon too small ({})", cleanMerged.size());
        return;
    }
    merged = cleanMerged;
    LOG_INFO(LogCategory::Core, "      After removing duplicates: {} vertices", merged.size());

    // Ear clipping on merged polygon
    std::list<size_t> remaining;
    for (size_t i = 0; i < merged.size(); i++) {
        remaining.push_back(i);
    }

    int maxIterations = static_cast<int>(merged.size()) * 3;
    int iterations = 0;
    int forcedRemovals = 0;

    // Debug: check if polygon is actually CCW by summing cross products
    float totalCross = 0.0f;
    for (size_t i = 0; i < merged.size(); i++) {
        size_t prev = (i + merged.size() - 1) % merged.size();
        size_t next = (i + 1) % merged.size();
        totalCross += cross2D(merged[prev], merged[i], merged[next]);
    }
    LOG_INFO(LogCategory::Core, "      Merged polygon total cross sum: {} (should be positive for CCW)", totalCross);

    while (remaining.size() > 3 && iterations < maxIterations) {
        iterations++;
        bool earFound = false;

        auto it = remaining.begin();
        while (it != remaining.end()) {
            auto prevIt = it;
            if (prevIt == remaining.begin()) prevIt = remaining.end();
            --prevIt;

            auto nextIt = it;
            ++nextIt;
            if (nextIt == remaining.end()) nextIt = remaining.begin();

            const Vec2& prev = merged[*prevIt];
            const Vec2& curr = merged[*it];
            const Vec2& next = merged[*nextIt];

            float cross = cross2D(prev, curr, next);
            // Use small tolerance for convexity check
            if (cross > -0.0001f) {
                bool isEar = true;
                // Only check point-in-triangle if cross > 0 (truly convex)
                if (cross > 0.0001f) {
                    for (auto checkIt = remaining.begin(); checkIt != remaining.end() && isEar; ++checkIt) {
                        if (checkIt == prevIt || checkIt == it || checkIt == nextIt) continue;
                        if (pointInTriangleStrict(merged[*checkIt], prev, curr, next)) {
                            isEar = false;
                        }
                    }
                }

                if (isEar) {
                    addTriangle(prev, curr, next);
                    it = remaining.erase(it);
                    earFound = true;
                    break;
                }
            }
            ++it;
        }

        // If no ear found, force remove the MOST CONVEX vertex to minimize artifacts
        if (!earFound && remaining.size() > 3) {
            forcedRemovals++;

            // Find vertex with largest positive cross product (most convex)
            float maxCross = -1e30f;
            auto bestIt = remaining.begin();

            for (auto scanIt = remaining.begin(); scanIt != remaining.end(); ++scanIt) {
                auto prevIt = scanIt;
                if (prevIt == remaining.begin()) prevIt = remaining.end();
                --prevIt;

                auto nextIt = scanIt;
                ++nextIt;
                if (nextIt == remaining.end()) nextIt = remaining.begin();

                float cross = cross2D(merged[*prevIt], merged[*scanIt], merged[*nextIt]);
                if (cross > maxCross) {
                    maxCross = cross;
                    bestIt = scanIt;
                }
            }

            // Use the most convex vertex
            auto prevIt = bestIt;
            if (prevIt == remaining.begin()) prevIt = remaining.end();
            --prevIt;

            auto nextIt = bestIt;
            ++nextIt;
            if (nextIt == remaining.end()) nextIt = remaining.begin();

            addTriangle(merged[*prevIt], merged[*bestIt], merged[*nextIt]);
            remaining.erase(bestIt);
            earFound = true;  // Continue the loop
        }

        if (!earFound) break;
    }

    if (remaining.size() == 3) {
        auto it = remaining.begin();
        size_t i0 = *it++;
        size_t i1 = *it++;
        size_t i2 = *it;
        addTriangle(merged[i0], merged[i1], merged[i2]);
    }

    size_t expectedTriangles = merged.size() - 2;
    LOG_INFO(LogCategory::Core, "    TessellatePolygonWithHoles: generated {} triangles (expected {}, remaining: {}, forced: {})",
             triangleCount, expectedTriangles, remaining.size(), forcedRemovals);
}

void VectorGraphic2D::TessellatePolygonWithGradient(const std::vector<Vec2>& points,
                                                     const Color& baseColor, float zDepth,
                                                     std::vector<float>& vertices,
                                                     const SVGGradient* gradient,
                                                     float boundsMinX, float boundsMinY,
                                                     float boundsMaxX, float boundsMaxY,
                                                     const Color& tint, float opacity) {
    if (points.size() < 3) return;

    // Transform points based on current settings
    std::vector<Vec2> transformedPoints;
    transformedPoints.reserve(points.size());
    for (const auto& p : points) {
        transformedPoints.push_back(TransformPoint(p));
    }

    // Remove duplicate consecutive points
    std::vector<Vec2> cleanPoints;
    cleanPoints.push_back(transformedPoints[0]);
    for (size_t i = 1; i < transformedPoints.size(); i++) {
        Vec2 diff = transformedPoints[i] - cleanPoints.back();
        if (diff.x * diff.x + diff.y * diff.y > 0.0001f) {
            cleanPoints.push_back(transformedPoints[i]);
        }
    }
    // Also check first vs last
    if (cleanPoints.size() > 1) {
        Vec2 diff = cleanPoints.back() - cleanPoints.front();
        if (diff.x * diff.x + diff.y * diff.y < 0.0001f) {
            cleanPoints.pop_back();
        }
    }
    if (cleanPoints.size() < 3) return;

    // Helper: calculate cross product of vectors (b-a) and (c-a)
    auto cross2D = [](const Vec2& a, const Vec2& b, const Vec2& c) -> float {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    };

    // Helper: check if point p is inside triangle abc
    auto pointInTriangle = [&cross2D](const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) -> bool {
        float d1 = cross2D(p, a, b);
        float d2 = cross2D(p, b, c);
        float d3 = cross2D(p, c, a);
        bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        return !(hasNeg && hasPos);
    };

    // Helper: get color for a point (either from gradient or solid)
    auto getColorAtPoint = [&](const Vec2& p) -> Color {
        Color color;
        if (gradient) {
            // Sample gradient at original (untransformed) point position
            // We need to reverse-transform the point to get SVG coordinates
            color = SampleGradientAtPoint(*gradient, p, boundsMinX, boundsMinY, boundsMaxX, boundsMaxY);
        } else {
            color = baseColor;
        }
        // Apply tint and opacity
        return Color(
            color.r * tint.r,
            color.g * tint.g,
            color.b * tint.b,
            color.a * tint.a * opacity
        );
    };

    // Calculate signed area to determine winding order
    float signedArea = 0.0f;
    for (size_t i = 0; i < cleanPoints.size(); i++) {
        size_t j = (i + 1) % cleanPoints.size();
        signedArea += cleanPoints[i].x * cleanPoints[j].y;
        signedArea -= cleanPoints[j].x * cleanPoints[i].y;
    }
    signedArea *= 0.5f;

    // Ensure counter-clockwise winding for ear clipping
    bool ccw = signedArea > 0;
    if (!ccw) {
        std::reverse(cleanPoints.begin(), cleanPoints.end());
    }

    // Also store original points for gradient sampling (need to track correspondence)
    std::vector<Vec2> originalPoints;
    originalPoints.reserve(points.size());
    for (const auto& p : points) {
        originalPoints.push_back(p);
    }
    // Clean original points the same way
    std::vector<Vec2> cleanOriginal;
    cleanOriginal.push_back(originalPoints[0]);
    for (size_t i = 1; i < originalPoints.size(); i++) {
        Vec2 diff = originalPoints[i] - cleanOriginal.back();
        if (diff.x * diff.x + diff.y * diff.y > 0.0001f) {
            cleanOriginal.push_back(originalPoints[i]);
        }
    }
    if (cleanOriginal.size() > 1) {
        Vec2 diff = cleanOriginal.back() - cleanOriginal.front();
        if (diff.x * diff.x + diff.y * diff.y < 0.0001f) {
            cleanOriginal.pop_back();
        }
    }
    if (!ccw) {
        std::reverse(cleanOriginal.begin(), cleanOriginal.end());
    }

    // Ear clipping algorithm
    std::list<size_t> remaining;
    for (size_t i = 0; i < cleanPoints.size(); i++) {
        remaining.push_back(i);
    }

    size_t triangleCount = 0;
    auto addTriangle = [&](size_t ia, size_t ib, size_t ic) {
        triangleCount++;
        const Vec2& a = cleanPoints[ia];
        const Vec2& b = cleanPoints[ib];
        const Vec2& c = cleanPoints[ic];

        // Get colors at original point positions
        Color colorA = getColorAtPoint(cleanOriginal[ia]);
        Color colorB = getColorAtPoint(cleanOriginal[ib]);
        Color colorC = getColorAtPoint(cleanOriginal[ic]);

        // Add vertices with reversed winding for Y-flip
        vertices.push_back(a.x); vertices.push_back(a.y); vertices.push_back(zDepth);
        vertices.push_back(colorA.r); vertices.push_back(colorA.g);
        vertices.push_back(colorA.b); vertices.push_back(colorA.a);

        vertices.push_back(c.x); vertices.push_back(c.y); vertices.push_back(zDepth);
        vertices.push_back(colorC.r); vertices.push_back(colorC.g);
        vertices.push_back(colorC.b); vertices.push_back(colorC.a);

        vertices.push_back(b.x); vertices.push_back(b.y); vertices.push_back(zDepth);
        vertices.push_back(colorB.r); vertices.push_back(colorB.g);
        vertices.push_back(colorB.b); vertices.push_back(colorB.a);
    };

    int maxIterations = static_cast<int>(cleanPoints.size()) * 3;
    int iterations = 0;
    int forcedRemovals = 0;

    while (remaining.size() > 3 && iterations < maxIterations) {
        iterations++;
        bool earFound = false;

        auto it = remaining.begin();
        while (it != remaining.end()) {
            auto prevIt = it;
            if (prevIt == remaining.begin()) {
                prevIt = remaining.end();
            }
            --prevIt;

            auto nextIt = it;
            ++nextIt;
            if (nextIt == remaining.end()) {
                nextIt = remaining.begin();
            }

            const Vec2& prev = cleanPoints[*prevIt];
            const Vec2& curr = cleanPoints[*it];
            const Vec2& next = cleanPoints[*nextIt];

            float cross = cross2D(prev, curr, next);
            if (cross > 0.0f) {
                bool isEar = true;
                for (auto checkIt = remaining.begin(); checkIt != remaining.end() && isEar; ++checkIt) {
                    if (checkIt == prevIt || checkIt == it || checkIt == nextIt) continue;
                    if (pointInTriangle(cleanPoints[*checkIt], prev, curr, next)) {
                        isEar = false;
                    }
                }

                if (isEar) {
                    addTriangle(*prevIt, *it, *nextIt);
                    it = remaining.erase(it);
                    earFound = true;
                    break;
                }
            }
            ++it;
        }

        if (!earFound && remaining.size() > 3) {
            forcedRemovals++;
            auto forcedIt = remaining.begin();
            auto prevIt = remaining.end(); --prevIt;
            auto nextIt = forcedIt; ++nextIt;
            addTriangle(*prevIt, *forcedIt, *nextIt);
            remaining.erase(forcedIt);
        }
    }

    if (remaining.size() == 3) {
        auto it = remaining.begin();
        size_t i0 = *it++;
        size_t i1 = *it++;
        size_t i2 = *it;
        addTriangle(i0, i1, i2);
    }

    LOG_INFO(LogCategory::Core, "    TessellatePolygonWithGradient: {} points -> {} triangles (gradient: {})",
             cleanPoints.size(), triangleCount, gradient ? "yes" : "no");
}

void VectorGraphic2D::TessellatePolygonWithHolesAndGradient(const std::vector<std::vector<Vec2>>& paths,
                                                             const Color& baseColor, float zDepth,
                                                             std::vector<float>& vertices,
                                                             const SVGGradient* gradient,
                                                             float boundsMinX, float boundsMinY,
                                                             float boundsMaxX, float boundsMaxY,
                                                             const Color& tint, float opacity) {
    if (paths.empty()) return;

    // Helper: get color for a point (either from gradient or solid)
    auto getColorAtPoint = [&](const Vec2& p) -> Color {
        Color color;
        if (gradient) {
            color = SampleGradientAtPoint(*gradient, p, boundsMinX, boundsMinY, boundsMaxX, boundsMaxY);
        } else {
            color = baseColor;
        }
        return Color(
            color.r * tint.r,
            color.g * tint.g,
            color.b * tint.b,
            color.a * tint.a * opacity
        );
    };

    // Helper: calculate signed area
    auto signedArea = [](const std::vector<Vec2>& poly) -> float {
        float area = 0.0f;
        for (size_t i = 0; i < poly.size(); i++) {
            size_t j = (i + 1) % poly.size();
            area += poly[i].x * poly[j].y;
            area -= poly[j].x * poly[i].y;
        }
        return area * 0.5f;
    };

    auto isCCW = [&signedArea](const std::vector<Vec2>& poly) -> bool {
        return signedArea(poly) > 0;
    };

    // Transform all paths (transformed for rendering, originals for gradient)
    std::vector<std::vector<Vec2>> transformedPaths;
    std::vector<std::vector<Vec2>> originalPaths;  // Keep original for gradient sampling
    transformedPaths.reserve(paths.size());
    originalPaths.reserve(paths.size());

    for (const auto& path : paths) {
        std::vector<Vec2> transformed;
        std::vector<Vec2> original;
        transformed.reserve(path.size());
        original.reserve(path.size());
        for (const auto& p : path) {
            transformed.push_back(TransformPoint(p));
            original.push_back(p);
        }
        // Remove duplicate consecutive points
        std::vector<Vec2> cleanTrans, cleanOrig;
        cleanTrans.push_back(transformed[0]);
        cleanOrig.push_back(original[0]);
        for (size_t i = 1; i < transformed.size(); i++) {
            Vec2 diff = transformed[i] - cleanTrans.back();
            if (diff.x * diff.x + diff.y * diff.y > 0.0001f) {
                cleanTrans.push_back(transformed[i]);
                cleanOrig.push_back(original[i]);
            }
        }
        if (cleanTrans.size() >= 3) {
            transformedPaths.push_back(cleanTrans);
            originalPaths.push_back(cleanOrig);
        }
    }

    if (transformedPaths.empty()) return;

    // Find the outer contour (largest absolute area)
    size_t outerIdx = 0;
    float maxArea = 0.0f;
    for (size_t i = 0; i < transformedPaths.size(); i++) {
        float area = std::abs(signedArea(transformedPaths[i]));
        if (area > maxArea) {
            maxArea = area;
            outerIdx = i;
        }
    }

    // Get outer contour (ensure CCW)
    std::vector<Vec2> outer = transformedPaths[outerIdx];
    std::vector<Vec2> outerOrig = originalPaths[outerIdx];
    if (!isCCW(outer)) {
        std::reverse(outer.begin(), outer.end());
        std::reverse(outerOrig.begin(), outerOrig.end());
    }

    // Collect holes (ensure CW)
    std::vector<std::vector<Vec2>> holes;
    std::vector<std::vector<Vec2>> holesOrig;
    for (size_t i = 0; i < transformedPaths.size(); i++) {
        if (i != outerIdx) {
            std::vector<Vec2> hole = transformedPaths[i];
            std::vector<Vec2> holeOrig = originalPaths[i];
            if (isCCW(hole)) {
                std::reverse(hole.begin(), hole.end());
                std::reverse(holeOrig.begin(), holeOrig.end());
            }
            holes.push_back(hole);
            holesOrig.push_back(holeOrig);
        }
    }

    // If no holes, just tessellate the outer polygon
    if (holes.empty()) {
        TessellatePolygonWithGradient(paths[outerIdx], baseColor, zDepth, vertices,
                                      gradient, boundsMinX, boundsMinY, boundsMaxX, boundsMaxY, tint, opacity);
        return;
    }

    auto cross2D = [](const Vec2& a, const Vec2& b, const Vec2& c) -> float {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    };

    // Sort holes by rightmost X
    std::vector<size_t> holeOrder(holes.size());
    for (size_t i = 0; i < holes.size(); i++) holeOrder[i] = i;
    std::sort(holeOrder.begin(), holeOrder.end(), [&holes](size_t a, size_t b) {
        float maxXa = -1e30f, maxXb = -1e30f;
        for (const auto& p : holes[a]) maxXa = std::max(maxXa, p.x);
        for (const auto& p : holes[b]) maxXb = std::max(maxXb, p.x);
        return maxXa > maxXb;
    });

    std::vector<Vec2> merged = outer;
    std::vector<Vec2> mergedOrig = outerOrig;

    for (size_t holeIdx : holeOrder) {
        const auto& hole = holes[holeIdx];
        const auto& holeOrig = holesOrig[holeIdx];

        // Find rightmost vertex of the hole
        size_t rightmostIdx = 0;
        float maxX = hole[0].x;
        for (size_t i = 1; i < hole.size(); i++) {
            if (hole[i].x > maxX) {
                maxX = hole[i].x;
                rightmostIdx = i;
            }
        }
        Vec2 holePoint = hole[rightmostIdx];

        // Cast ray to find intersection
        float minDist = 1e30f;
        size_t bridgeEdgeIdx = 0;
        Vec2 intersectionPoint = holePoint;
        bool foundIntersection = false;

        for (size_t i = 0; i < merged.size(); i++) {
            size_t j = (i + 1) % merged.size();
            Vec2 p1 = merged[i];
            Vec2 p2 = merged[j];

            if ((p1.y <= holePoint.y && p2.y > holePoint.y) ||
                (p2.y <= holePoint.y && p1.y > holePoint.y)) {
                float t = (holePoint.y - p1.y) / (p2.y - p1.y);
                float xIntersect = p1.x + t * (p2.x - p1.x);

                if (xIntersect > holePoint.x) {
                    float dist = xIntersect - holePoint.x;
                    if (dist < minDist) {
                        minDist = dist;
                        intersectionPoint = Vec2(xIntersect, holePoint.y);
                        bridgeEdgeIdx = i;
                        foundIntersection = true;
                    }
                }
            }
        }

        if (!foundIntersection) continue;

        // Find the visible vertex to bridge to
        size_t nextEdgeIdx = (bridgeEdgeIdx + 1) % merged.size();
        Vec2 edgeV1 = merged[bridgeEdgeIdx];
        Vec2 edgeV2 = merged[nextEdgeIdx];

        size_t connectIdx = (edgeV1.x >= edgeV2.x) ? bridgeEdgeIdx : nextEdgeIdx;
        Vec2 candidateM = merged[connectIdx];

        const float eps = 0.0001f;
        float distToV1 = std::sqrt((intersectionPoint.x - edgeV1.x) * (intersectionPoint.x - edgeV1.x) +
                                    (intersectionPoint.y - edgeV1.y) * (intersectionPoint.y - edgeV1.y));
        float distToV2 = std::sqrt((intersectionPoint.x - edgeV2.x) * (intersectionPoint.x - edgeV2.x) +
                                    (intersectionPoint.y - edgeV2.y) * (intersectionPoint.y - edgeV2.y));

        if (distToV1 < eps) {
            connectIdx = bridgeEdgeIdx;
        } else if (distToV2 < eps) {
            connectIdx = nextEdgeIdx;
        } else {
            auto pointInTriangleStrict = [](const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) -> bool {
                auto sign = [](const Vec2& p1, const Vec2& p2, const Vec2& p3) -> float {
                    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
                };
                float d1 = sign(p, a, b);
                float d2 = sign(p, b, c);
                float d3 = sign(p, c, a);
                const float tolerance = 1e-6f;
                if (d1 > tolerance && d2 > tolerance && d3 > tolerance) return true;
                if (d1 < -tolerance && d2 < -tolerance && d3 < -tolerance) return true;
                return false;
            };

            Vec2 triA = holePoint;
            Vec2 triB = intersectionPoint;
            Vec2 triC = candidateM;

            float minAngle = 1e30f;
            size_t bestIdx = connectIdx;

            for (size_t i = 0; i < merged.size(); i++) {
                if (i == bridgeEdgeIdx || i == nextEdgeIdx) continue;
                Vec2 v = merged[i];
                if (v.x <= holePoint.x + eps) continue;
                bool inside = pointInTriangleStrict(v, triA, triB, triC) ||
                              pointInTriangleStrict(v, triA, triC, triB);
                if (inside) {
                    float dy = v.y - holePoint.y;
                    float dx = v.x - holePoint.x;
                    float angle = std::abs(std::atan2(dy, dx));
                    if (angle < minAngle) {
                        minAngle = angle;
                        bestIdx = i;
                    }
                }
            }
            connectIdx = bestIdx;
        }

        // Insert hole into merged polygon
        std::vector<Vec2> newMerged;
        std::vector<Vec2> newMergedOrig;
        newMerged.reserve(merged.size() + hole.size() + 2);
        newMergedOrig.reserve(mergedOrig.size() + holeOrig.size() + 2);

        for (size_t i = 0; i <= connectIdx; i++) {
            newMerged.push_back(merged[i]);
            newMergedOrig.push_back(mergedOrig[i]);
        }

        for (size_t i = 0; i < hole.size(); i++) {
            newMerged.push_back(hole[(rightmostIdx + i) % hole.size()]);
            newMergedOrig.push_back(holeOrig[(rightmostIdx + i) % holeOrig.size()]);
        }

        newMerged.push_back(hole[rightmostIdx]);
        newMerged.push_back(merged[connectIdx]);
        newMergedOrig.push_back(holeOrig[rightmostIdx]);
        newMergedOrig.push_back(mergedOrig[connectIdx]);

        for (size_t i = connectIdx + 1; i < merged.size(); i++) {
            newMerged.push_back(merged[i]);
            newMergedOrig.push_back(mergedOrig[i]);
        }

        merged = newMerged;
        mergedOrig = newMergedOrig;
    }

    if (merged.size() < 3) return;

    LOG_INFO(LogCategory::Core, "    TessellatePolygonWithHolesAndGradient: merged polygon has {} vertices", merged.size());

    size_t triangleCount = 0;
    auto addTriangle = [&](size_t ia, size_t ib, size_t ic) {
        triangleCount++;
        const Vec2& a = merged[ia];
        const Vec2& b = merged[ib];
        const Vec2& c = merged[ic];

        Color colorA = getColorAtPoint(mergedOrig[ia]);
        Color colorB = getColorAtPoint(mergedOrig[ib]);
        Color colorC = getColorAtPoint(mergedOrig[ic]);

        vertices.push_back(a.x); vertices.push_back(a.y); vertices.push_back(zDepth);
        vertices.push_back(colorA.r); vertices.push_back(colorA.g);
        vertices.push_back(colorA.b); vertices.push_back(colorA.a);

        vertices.push_back(c.x); vertices.push_back(c.y); vertices.push_back(zDepth);
        vertices.push_back(colorC.r); vertices.push_back(colorC.g);
        vertices.push_back(colorC.b); vertices.push_back(colorC.a);

        vertices.push_back(b.x); vertices.push_back(b.y); vertices.push_back(zDepth);
        vertices.push_back(colorB.r); vertices.push_back(colorB.g);
        vertices.push_back(colorB.b); vertices.push_back(colorB.a);
    };

    auto pointInTriangleStrict = [&cross2D](const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) -> bool {
        float d1 = cross2D(p, a, b);
        float d2 = cross2D(p, b, c);
        float d3 = cross2D(p, c, a);
        const float eps = 0.0001f;
        if (d1 > eps && d2 > eps && d3 > eps) return true;
        if (d1 < -eps && d2 < -eps && d3 < -eps) return true;
        return false;
    };

    if (!isCCW(merged)) {
        std::reverse(merged.begin(), merged.end());
        std::reverse(mergedOrig.begin(), mergedOrig.end());
    }

    // Remove duplicate consecutive vertices
    std::vector<Vec2> cleanMerged;
    std::vector<Vec2> cleanMergedOrig;
    cleanMerged.reserve(merged.size());
    cleanMergedOrig.reserve(mergedOrig.size());
    for (size_t i = 0; i < merged.size(); i++) {
        const Vec2& curr = merged[i];
        const Vec2& next = merged[(i + 1) % merged.size()];
        float dx = next.x - curr.x;
        float dy = next.y - curr.y;
        if (dx * dx + dy * dy > 0.0001f) {
            cleanMerged.push_back(curr);
            cleanMergedOrig.push_back(mergedOrig[i]);
        }
    }
    if (cleanMerged.size() < 3) return;
    merged = cleanMerged;
    mergedOrig = cleanMergedOrig;

    // Ear clipping
    std::list<size_t> remaining;
    for (size_t i = 0; i < merged.size(); i++) {
        remaining.push_back(i);
    }

    int maxIterations = static_cast<int>(merged.size()) * 3;
    int iterations = 0;
    int forcedRemovals = 0;

    while (remaining.size() > 3 && iterations < maxIterations) {
        iterations++;
        bool earFound = false;

        auto it = remaining.begin();
        while (it != remaining.end()) {
            auto prevIt = it;
            if (prevIt == remaining.begin()) prevIt = remaining.end();
            --prevIt;

            auto nextIt = it;
            ++nextIt;
            if (nextIt == remaining.end()) nextIt = remaining.begin();

            const Vec2& prev = merged[*prevIt];
            const Vec2& curr = merged[*it];
            const Vec2& next = merged[*nextIt];

            float cross = cross2D(prev, curr, next);
            if (cross > -0.0001f) {
                bool isEar = true;
                if (cross > 0.0001f) {
                    for (auto checkIt = remaining.begin(); checkIt != remaining.end() && isEar; ++checkIt) {
                        if (checkIt == prevIt || checkIt == it || checkIt == nextIt) continue;
                        if (pointInTriangleStrict(merged[*checkIt], prev, curr, next)) {
                            isEar = false;
                        }
                    }
                }

                if (isEar) {
                    addTriangle(*prevIt, *it, *nextIt);
                    it = remaining.erase(it);
                    earFound = true;
                    break;
                }
            }
            ++it;
        }

        if (!earFound && remaining.size() > 3) {
            forcedRemovals++;
            float maxCross = -1e30f;
            auto bestIt = remaining.begin();

            for (auto scanIt = remaining.begin(); scanIt != remaining.end(); ++scanIt) {
                auto prevIt = scanIt;
                if (prevIt == remaining.begin()) prevIt = remaining.end();
                --prevIt;

                auto nextIt = scanIt;
                ++nextIt;
                if (nextIt == remaining.end()) nextIt = remaining.begin();

                float cross = cross2D(merged[*prevIt], merged[*scanIt], merged[*nextIt]);
                if (cross > maxCross) {
                    maxCross = cross;
                    bestIt = scanIt;
                }
            }

            auto prevIt = bestIt;
            if (prevIt == remaining.begin()) prevIt = remaining.end();
            --prevIt;

            auto nextIt = bestIt;
            ++nextIt;
            if (nextIt == remaining.end()) nextIt = remaining.begin();

            addTriangle(*prevIt, *bestIt, *nextIt);
            remaining.erase(bestIt);
            earFound = true;
        }

        if (!earFound) break;
    }

    if (remaining.size() == 3) {
        auto it = remaining.begin();
        size_t i0 = *it++;
        size_t i1 = *it++;
        size_t i2 = *it;
        addTriangle(i0, i1, i2);
    }

    LOG_INFO(LogCategory::Core, "    TessellatePolygonWithHolesAndGradient: generated {} triangles (gradient: {}, forced: {})",
             triangleCount, gradient ? "yes" : "no", forcedRemovals);
}

void VectorGraphic2D::TessellateStroke(const std::vector<Vec2>& points,
                                        float strokeWidth, const Color& color,
                                        float zDepth, std::vector<float>& vertices,
                                        bool closed) {
    if (points.size() < 2) return;

    float halfWidth = strokeWidth * 0.5f;

    // Transform points
    std::vector<Vec2> transformedPoints;
    transformedPoints.reserve(points.size());
    for (const auto& p : points) {
        transformedPoints.push_back(TransformPoint(p));
    }

    // Remove duplicate consecutive points
    std::vector<Vec2> cleanPoints;
    cleanPoints.push_back(transformedPoints[0]);
    for (size_t i = 1; i < transformedPoints.size(); i++) {
        Vec2 diff = transformedPoints[i] - cleanPoints.back();
        if (diff.x * diff.x + diff.y * diff.y > 0.0001f) {
            cleanPoints.push_back(transformedPoints[i]);
        }
    }
    if (cleanPoints.size() < 2) return;

    // Helper to add a vertex with z-depth
    auto addVertex = [&](const Vec2& pos) {
        vertices.push_back(pos.x); vertices.push_back(pos.y); vertices.push_back(zDepth);
        vertices.push_back(color.r); vertices.push_back(color.g);
        vertices.push_back(color.b); vertices.push_back(color.a);
    };

    // Helper to add a triangle (reversed winding for Y-flip)
    auto addTriangle = [&](const Vec2& a, const Vec2& b, const Vec2& c) {
        addVertex(a);
        addVertex(c);
        addVertex(b);
    };

    // Calculate perpendicular for a direction
    auto calcPerp = [halfWidth](const Vec2& dir) -> Vec2 {
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len < 0.0001f) return Vec2(0, halfWidth);
        return Vec2(-dir.y / len * halfWidth, dir.x / len * halfWidth);
    };

    // Add round cap at start
    auto addRoundCap = [&](const Vec2& center, const Vec2& dir, bool isEnd) {
        int segments = 8;
        float startAngle = std::atan2(-dir.y, -dir.x);
        if (isEnd) startAngle = std::atan2(dir.y, dir.x);

        Vec2 prevPoint = center + Vec2(std::cos(startAngle - static_cast<float>(M_PI) / 2) * halfWidth,
                                        std::sin(startAngle - static_cast<float>(M_PI) / 2) * halfWidth);
        for (int i = 1; i <= segments; i++) {
            float angle = startAngle - static_cast<float>(M_PI) / 2 + static_cast<float>(M_PI) * i / segments;
            Vec2 point = center + Vec2(std::cos(angle) * halfWidth, std::sin(angle) * halfWidth);
            addTriangle(center, prevPoint, point);
            prevPoint = point;
        }
    };

    // Build stroke with miter joins
    std::vector<Vec2> leftEdge, rightEdge;
    leftEdge.reserve(cleanPoints.size());
    rightEdge.reserve(cleanPoints.size());

    size_t n = cleanPoints.size();
    for (size_t i = 0; i < n; i++) {
        Vec2 perpBefore(0, 0), perpAfter(0, 0);

        // For closed paths, wrap around; for open paths, only compute available edges
        if (closed || i > 0) {
            size_t prev = closed ? (i + n - 1) % n : i - 1;
            Vec2 dir = cleanPoints[i] - cleanPoints[prev];
            perpBefore = calcPerp(dir);
        }
        if (closed || i < n - 1) {
            size_t next = closed ? (i + 1) % n : i + 1;
            Vec2 dir = cleanPoints[next] - cleanPoints[i];
            perpAfter = calcPerp(dir);
        }

        Vec2 perp;
        bool atEndpoint = !closed && (i == 0 || i == n - 1);
        if (atEndpoint && i == 0) {
            perp = perpAfter;
        } else if (atEndpoint && i == n - 1) {
            perp = perpBefore;
        } else {
            // Miter join: average the perpendiculars
            perp = (perpBefore + perpAfter) * 0.5f;
            float perpLen = std::sqrt(perp.x * perp.x + perp.y * perp.y);
            if (perpLen > 0.0001f) {
                // Scale to maintain stroke width
                float dot = (perpBefore.x * perpAfter.x + perpBefore.y * perpAfter.y) / (halfWidth * halfWidth);
                float miterScale = 1.0f / std::max(0.5f, (1.0f + dot) * 0.5f);
                miterScale = std::min(miterScale, 2.0f); // Limit miter length
                perp = perp * (halfWidth / perpLen * miterScale);
            } else {
                perp = perpBefore;
            }
        }

        leftEdge.push_back(cleanPoints[i] - perp);
        rightEdge.push_back(cleanPoints[i] + perp);
    }

    // Only add round caps for open paths
    if (!closed && cleanPoints.size() >= 2) {
        Vec2 startDir = cleanPoints[1] - cleanPoints[0];
        addRoundCap(cleanPoints[0], startDir, false);
    }

    // Generate quad strip from edges
    size_t segmentCount = closed ? n : n - 1;
    for (size_t i = 0; i < segmentCount; i++) {
        size_t next = (i + 1) % n;
        // Quad between segment i and next
        addTriangle(leftEdge[i], rightEdge[i], rightEdge[next]);
        addTriangle(leftEdge[i], rightEdge[next], leftEdge[next]);
    }

    // Only add round cap at end for open paths
    if (!closed && cleanPoints.size() >= 2) {
        Vec2 endDir = cleanPoints[n - 1] - cleanPoints[n - 2];
        addRoundCap(cleanPoints.back(), endDir, true);
    }
}

// ============================================================================
// Transform and Mesh Helpers
// ============================================================================

Vec2 VectorGraphic2D::TransformPoint(const Vec2& point) const {
    if (!m_Document) return point;

    Vec2 size = GetSize();
    float uniformScale = GetUniformScale();
    bool preserveAspect = GetPreserveAspectRatio();
    bool flipH = GetFlipH();
    bool flipV = GetFlipV();
    bool centered = GetCentered();
    Vec2 offset = GetOffset();

    Vec2 originalSize = m_Document->size;
    Vec2 viewBox = m_Document->viewBox;

    // Calculate scale factors
    float scaleX = size.x / originalSize.x * uniformScale;
    float scaleY = size.y / originalSize.y * uniformScale;

    if (preserveAspect) {
        float minScale = std::min(scaleX, scaleY);
        scaleX = scaleY = minScale;
    }

    if (flipH) scaleX = -scaleX;
    if (flipV) scaleY = -scaleY;

    // Transform point from SVG coordinates to engine coordinates
    // SVG: origin at top-left, Y increases downward
    // Engine: origin at center (if centered), Y increases upward
    Vec2 result = point - viewBox;

    // Flip Y to convert from SVG Y-down to engine Y-up
    result.y = originalSize.y - result.y;

    // Apply scaling
    result.x *= scaleX;
    result.y *= scaleY;

    // Center offset
    if (centered) {
        result.x -= (originalSize.x * std::abs(scaleX)) / 2.0f;
        result.y -= (originalSize.y * std::abs(scaleY)) / 2.0f;
    }

    result = result + offset;

    return result;
}

// Forward declare the debug counter
extern int s_gradientSampleCount;

void VectorGraphic2D::RebuildMesh() {
    if (!m_Document) return;

    // Reset gradient sample counter for debug logging
    s_gradientSampleCount = 0;

    m_VertexData.clear();
    m_IndexData.clear();

    // Debug: log transform settings
    Vec2 size = GetSize();
    float uniformScale = GetUniformScale();
    LOG_INFO(LogCategory::Core, "VectorGraphic2D::RebuildMesh - SVG size=({}, {}), target size=({}, {}), uniformScale={}, centered={}",
             m_Document->size.x, m_Document->size.y, size.x, size.y, uniformScale, GetCentered());

    // Tessellate shapes with INCREASING positive Z values
    // For 2D ortho projection with depth test, later shapes need to be "closer"
    // Try positive z where later shapes have smaller z (closer to near plane)
    // Start from a base z and decrement toward 0
    float numShapes = static_cast<float>(m_Document->shapes.size());
    float zDepth = numShapes * 0.001f;  // Start at back (e.g., 0.019 for 19 shapes)
    const float zIncrement = -0.001f;   // Move toward 0 (closer)

    LOG_INFO(LogCategory::Core, "VectorGraphic2D: Tessellating {} shapes with z from {} toward 0",
             m_Document->shapes.size(), zDepth);

    for (size_t i = 0; i < m_Document->shapes.size(); i++) {
        const auto& shape = m_Document->shapes[i];
        size_t vertsBefore = m_VertexData.size() / 7;
        LOG_INFO(LogCategory::Core, "  Shape[{}]: z={}, hasFill={}, hasStroke={}, color=({:.2f},{:.2f},{:.2f}), vertsBefore={}",
                 i, zDepth, shape.hasFill, shape.hasStroke,
                 shape.fillColor.r, shape.fillColor.g, shape.fillColor.b, vertsBefore);
        TessellateShape(shape, zDepth);
        size_t vertsAfter = m_VertexData.size() / 7;
        LOG_INFO(LogCategory::Core, "    -> Added {} vertices (total now: {})", vertsAfter - vertsBefore, vertsAfter);
        zDepth += zIncrement;  // Later shapes get smaller z (closer to camera)
    }

    m_MeshNeedsRebuild = false;
    m_MeshNeedsUpload = true;
}

void VectorGraphic2D::UpdateTransform() {
    // Transform is applied during tessellation
    m_MeshNeedsRebuild = true;
}

void VectorGraphic2D::UploadMesh(IGfxDevice*) {
    // Mesh upload would happen here if using GPU buffers
    // For now, we render directly from vertex data
    m_MeshNeedsUpload = false;
}

void VectorGraphic2D::DestroyMesh() {
    if (m_RenderMesh.isValid() && m_CachedDevice) {
        m_CachedDevice->destroyMesh(m_RenderMesh);
        m_RenderMesh = MeshHandle();
    }
    m_MeshHandle = MeshHandle();
    m_VertexData.clear();
    m_IndexData.clear();
}

// ============================================================================
// IRenderableComponent Implementation
// ============================================================================

void VectorGraphic2D::buildDrawCommands(RenderContext& ctx) {
    static int debugCounter = 0;
    if (debugCounter++ % 60 == 0) {
        LOG_INFO(LogCategory::Core, "VectorGraphic2D::buildDrawCommands called - enabled={}, owner={}, doc={}",
                 IsEnabled(), (m_Owner != nullptr), (m_Document != nullptr));
    }

    if (!IsEnabled() || !m_Owner || !m_Document) {
        return;
    }

    // Rebuild mesh if needed
    if (m_MeshNeedsRebuild) {
        RebuildMesh();
        LOG_INFO(LogCategory::Core, "VectorGraphic2D: Rebuilt mesh with {} vertices from {} shapes",
                 m_VertexData.size() / 7, m_Document->shapes.size());

        // Debug: log first few vertices (format: x, y, z, r, g, b, a = 7 floats)
        if (m_VertexData.size() >= 21) {
            LOG_INFO(LogCategory::Core, "VectorGraphic2D: First vertex: pos=({}, {}, {}), color=({}, {}, {}, {})",
                     m_VertexData[0], m_VertexData[1], m_VertexData[2],
                     m_VertexData[3], m_VertexData[4], m_VertexData[5], m_VertexData[6]);
        }
    }

    if (m_VertexData.empty()) {
        LOG_WARN(LogCategory::Core, "VectorGraphic2D: No vertex data to render (shapes={}, needsRebuild={})",
                 m_Document ? m_Document->shapes.size() : 0, m_MeshNeedsRebuild);
        return;
    }

    // Get world transform
    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 position(0, 0);
    float rotation = 0.0f;
    Vec2 scale(1, 1);

    if (node2D) {
        position = node2D->GetGlobalPosition();
        rotation = node2D->GetGlobalRotation();
        scale = node2D->GetGlobalScale();
    }

    // Build MeshData from vertex data
    // Vertex format: x, y, z, r, g, b, a (7 floats per vertex)
    size_t vertexCount = m_VertexData.size() / 7;
    if (vertexCount == 0) return;

    MeshData meshData;
    meshData.vertices.reserve(vertexCount);
    meshData.indices.reserve(vertexCount);

    // Add the SVG vertex data
    for (size_t i = 0; i < vertexCount; i++) {
        size_t idx = i * 7;
        Vertex v;
        v.position = Vec3(m_VertexData[idx + 0], m_VertexData[idx + 1], m_VertexData[idx + 2]);
        v.normal = Vec3(0.0f, 0.0f, -1.0f);  // Facing camera for 2D
        v.texCoord = Vec2(0.0f, 0.0f);
        v.color = Vec4(m_VertexData[idx + 3], m_VertexData[idx + 4],
                       m_VertexData[idx + 5], m_VertexData[idx + 6]);
        meshData.vertices.push_back(v);
        meshData.indices.push_back(static_cast<uint32_t>(i));
    }

    meshData.calculateBounds();

    // Get device and create/update the persistent render mesh
    IGfxDevice* device = ctx.getDevice();
    if (!device) return;

    // Store device reference for later cleanup
    m_CachedDevice = device;

    // Destroy previous mesh if it exists (from last frame)
    if (m_RenderMesh.isValid()) {
        device->destroyMesh(m_RenderMesh);
        m_RenderMesh = MeshHandle();
    }

    // Create new mesh for this frame
    m_RenderMesh = device->createMesh(meshData);
    if (!m_RenderMesh.isValid()) {
        LOG_WARN(LogCategory::Core, "VectorGraphic2D: Failed to create render mesh");
        return;
    }

    // Build world transform matrix
    Mat4 worldTransform = Mat4::Translate(Vec3(position.x, position.y, 0.0f)) *
                          Mat4::Rotate(rotation, Vec3(0.0f, 0.0f, 1.0f)) *
                          Mat4::Scale(Vec3(scale.x, scale.y, 1.0f));

    // Draw the mesh with vertex colors
    MaterialPropertyBlock overrides;
    // Set tint color to white so vertex colors show through (shader does v_Color * u_TintColor)
    overrides.setColor("u_TintColor", Color::White());
    overrides.setBool("u_UseTexture", false);

    MaterialHandle material = ctx.getColoredMaterialForSpatialType();
    if (!material.isValid()) {
        LOG_WARN(LogCategory::Core, "VectorGraphic2D: Invalid material for spatial type");
        return;
    }

    // Debug: log which material we're using
    static int matDebugCounter = 0;
    if (matDebugCounter++ % 60 == 0) {
        SpatialType spatialType = ctx.getSpatialType();
        LOG_INFO(LogCategory::Core, "VectorGraphic2D: Using material for spatial type {} (0=World3D, 1=World2D, 2=Canvas), material.id={}",
                 static_cast<int>(spatialType), material.id);
    }

    // Use the colored material from context (for 2D rendering)
    ctx.drawMesh(m_RenderMesh, material, worldTransform, overrides);

    static int drawCounter = 0;
    if (drawCounter++ % 60 == 0) {
        LOG_INFO(LogCategory::Core, "VectorGraphic2D: Drew mesh at ({}, {}) with {} verts",
                 position.x, position.y, vertexCount);
        // Log first and last triangle colors to verify order
        if (vertexCount >= 6) {
            LOG_INFO(LogCategory::Core, "  First triangle color: ({:.2f},{:.2f},{:.2f})",
                     m_VertexData[3], m_VertexData[4], m_VertexData[5]);
            size_t lastIdx = (vertexCount - 1) * 7;
            LOG_INFO(LogCategory::Core, "  Last triangle color: ({:.2f},{:.2f},{:.2f})",
                     m_VertexData[lastIdx + 3], m_VertexData[lastIdx + 4], m_VertexData[lastIdx + 5]);
        }
    }

    // NOTE: Do NOT destroy the mesh here - it will be rendered later in the frame
    // The mesh will be destroyed on the next frame before creating a new one
}

AABB VectorGraphic2D::getWorldBounds() const {
    if (!m_Owner || !m_Document) {
        return AABB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    Vec2 position(0, 0);
    Vec2 scale(1, 1);

    if (node2D) {
        position = node2D->GetGlobalPosition();
        scale = node2D->GetGlobalScale();
    }

    Vec2 size = GetSize() * GetUniformScale();
    Vec2 halfSize = size * 0.5f * scale;

    if (GetCentered()) {
        return AABB(
            Vec3(position.x - halfSize.x, position.y - halfSize.y, -0.1f),
            Vec3(position.x + halfSize.x, position.y + halfSize.y, 0.1f)
        );
    } else {
        return AABB(
            Vec3(position.x, position.y, -0.1f),
            Vec3(position.x + size.x * scale.x, position.y + size.y * scale.y, 0.1f)
        );
    }
}

RenderLayer VectorGraphic2D::getRenderLayer() const {
    return RenderLayer::Transparent;
}

SpatialType VectorGraphic2D::getSpatialType() const {
    return GetUISpace() ? SpatialType::Canvas : SpatialType::World2D;
}

bool VectorGraphic2D::IntersectRay(const Ray& ray, float& outDistance) const {
    AABB bounds = getWorldBounds();
    return bounds.IntersectRay(ray, outDistance);
}

} // namespace components
} // namespace lupine

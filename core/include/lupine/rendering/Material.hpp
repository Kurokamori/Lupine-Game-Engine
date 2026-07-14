#pragma once

#include "ResourceHandles.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Vec4.hpp"
#include "lupine/math/Mat4.hpp"
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace lupine {

// Import math types into lupine namespace for rendering
using math::Color;
using math::Vec2;
using math::Vec3;
using math::Vec4;
using math::Mat4;

/**
 * Material property value types
 */
using MaterialPropertyValue = std::variant<
    float,
    int,
    bool,
    Vec2,
    Vec3,
    Vec4,
    Color,
    TextureHandle,
    std::vector<Mat4>  // For uniform arrays (e.g., bone transforms)
>;

/**
 * Material property override block.
 * Allows per-instance property overrides without creating new materials.
 */
class MaterialPropertyBlock {
public:
    void setFloat(const std::string& name, float value);
    void setInt(const std::string& name, int value);
    void setBool(const std::string& name, bool value);
    void setVec2(const std::string& name, const Vec2& value);
    void setVec3(const std::string& name, const Vec3& value);
    void setVec4(const std::string& name, const Vec4& value);
    void setColor(const std::string& name, const Color& value);
    void setTexture(const std::string& name, TextureHandle value);
    void setMat4Array(const std::string& name, const Mat4* values, size_t count);

    bool hasProperty(const std::string& name) const;
    const MaterialPropertyValue* getProperty(const std::string& name) const;
    bool isEmpty() const { return m_properties.empty(); }

    void clear();

    // Get all properties for iteration
    const std::unordered_map<std::string, MaterialPropertyValue>& getProperties() const { return m_properties; }

private:
    std::unordered_map<std::string, MaterialPropertyValue> m_properties;
};

/**
 * Render layer enumeration.
 * Used for sorting and filtering renderables.
 */
enum class RenderLayer : uint32_t {
    Default = 0,
    Background = 100,
    Opaque = 1000,
    Transparent = 2000,
    UI = 3000,
    Overlay = 4000,
    Debug = 5000
};

/**
 * Vertex-layout / host kind for a custom .lsh material. Selects the pipeline vertex
 * input layout and the default render state when the shader declares no #render_mode:
 *   UI2D           - 2D UI / sprite quad (pos/normal/uv/color), no depth, no cull (overlay)
 *   Mesh3D         - 3D mesh (pos/normal/uv/color/tangent), depth test+write, back cull
 *   SkeletalMesh3D - Mesh3D + bone ids/weights and the bone-transform UBO
 */
enum class LshMaterialLayout {
    UI2D,
    Mesh3D,
    SkeletalMesh3D,
    InstancedMesh3D   // 3D mesh (pos/normal/uv/color) + per-instance buffer at binding 1
                      // (a_InstanceModel0..3/Color/Custom, locations 4-9); for MultiMesh.
};

/**
 * Material definition.
 * Describes shaders, textures, and properties for rendering.
 */
struct Material {
    std::string name;
    ShaderHandle vertexShader;
    ShaderHandle fragmentShader;
    PipelineHandle pipeline;

    // Material properties
    std::unordered_map<std::string, MaterialPropertyValue> properties;

    // Textures
    std::unordered_map<std::string, TextureHandle> textures;

    // Render state
    RenderLayer renderLayer = RenderLayer::Opaque;
    bool castsShadows = true;
    bool receivesShadows = true;

    // Alpha/transparency
    bool isTransparent = false;
    float alphaClipThreshold = 0.5f;

    // Feature flags (set from .lsh #feature directives for custom shaders)
    bool usesLighting = false;

    // True when the shader declares a `u_SceneTexture` sampler. The renderer captures the
    // scene color into an offscreen target and binds it here for screen-space post-processing
    // (e.g. a full-screen ColorRect that grades what's behind it).
    bool usesSceneTexture = false;

    // Per-frame engine-fed uniforms for custom .lsh shaders. Set when the shader source
    // references the corresponding built-in (Gap E/F). The renderer fills them in
    // executeBatch so the shader gets a live TIME / texel size / resolution.
    bool usesTime = false;        // shader references TIME -> u_Time
    bool usesScreenSize = false;  // shader references u_TexelSize / u_Resolution

    // Per-texture sampler overrides from .lsh sampler hints (Gap G): (texture slot,
    // sampler) pairs bound after the default sampler so @filter/@repeat take effect.
    std::vector<std::pair<int, SamplerHandle>> customSamplers;
};

/**
 * Material instance with property overrides.
 */
struct MaterialInstance {
    MaterialHandle baseMaterial;
    MaterialPropertyBlock propertyOverrides;
};

} // namespace lupine

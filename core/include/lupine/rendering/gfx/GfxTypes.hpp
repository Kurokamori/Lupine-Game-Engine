#pragma once

#include <cstdint>
#include <string>

namespace lupine {

/**
 * Graphics API backend types
 */
enum class GraphicsBackend {
    OpenGL,
    Vulkan,
    Metal,
    WebGL,
    DirectX11,
    DirectX12,
    None
};

/**
 * Check if a graphics backend uses bottom-left origin for framebuffers/viewports.
 * 
 * Graphics APIs differ in their coordinate system origin:
 * - OpenGL/WebGL: Origin at bottom-left, Y+ points up
 * - Vulkan/DirectX/Metal: Origin at top-left, Y+ points down
 * 
 * This affects:
 * 1. Viewport Y-coordinate (needs flipping for OpenGL/WebGL)
 * 2. UI rendering projection matrices
 * 3. Mouse input coordinate conversion
 * 
 * @param backend The graphics backend to check
 * @return true if backend uses bottom-left origin (OpenGL/WebGL), false otherwise
 */
inline bool IsBottomLeftOrigin(GraphicsBackend backend) {
    return backend == GraphicsBackend::OpenGL || backend == GraphicsBackend::WebGL;
}

/**
 * Check if a graphics backend uses top-left origin for framebuffers/viewports.
 *
 * @param backend The graphics backend to check
 * @return true if backend uses top-left origin (Vulkan/DirectX/Metal), false otherwise
 */
inline bool IsTopLeftOrigin(GraphicsBackend backend) {
    return backend == GraphicsBackend::Vulkan ||
           backend == GraphicsBackend::DirectX11 ||
           backend == GraphicsBackend::DirectX12 ||
           backend == GraphicsBackend::Metal;
}

/**
 * Check if a graphics backend requires Y-flip in the projection matrix.
 *
 * Graphics APIs with top-left origin need Y to be flipped to match OpenGL conventions:
 * - Vulkan: Handles Y-flip via negative viewport height, no projection flip needed
 * - DirectX11/DirectX12/Metal: Don't support negative viewports, need projection Y-flip
 * - OpenGL/WebGL: Already use bottom-left origin, no flip needed
 *
 * @param backend The graphics backend to check
 * @return true if projection matrix needs Y-flip (DX11/DX12/Metal), false otherwise
 */
inline bool NeedsProjectionYFlip(GraphicsBackend backend) {
    return backend == GraphicsBackend::DirectX11 ||
           backend == GraphicsBackend::DirectX12 ||
           backend == GraphicsBackend::Metal;
}

/**
 * Check if a graphics backend uses zero-to-one depth range.
 * OpenGL/WebGL use [-1, 1], while DirectX/Vulkan/Metal use [0, 1].
 *
 * @param backend The graphics backend to check
 * @return true if backend uses [0, 1] depth range, false if [-1, 1]
 */
inline bool IsZeroToOneDepth(GraphicsBackend backend) {
    return backend == GraphicsBackend::Vulkan ||
           backend == GraphicsBackend::DirectX11 ||
           backend == GraphicsBackend::DirectX12 ||
           backend == GraphicsBackend::Metal;
}

/**
 * Texture format enumeration
 */
enum class TextureFormat {
    Unknown,

    // Color formats
    R8_UNORM,
    RG8_UNORM,
    RGBA8_UNORM,
    RGBA8_SRGB,
    BGRA8_UNORM,
    BGRA8_SRGB,

    R16_FLOAT,
    RG16_FLOAT,
    RGBA16_FLOAT,

    R32_FLOAT,
    RG32_FLOAT,
    RGB32_FLOAT,
    RGBA32_FLOAT,

    // Depth/stencil formats
    DEPTH16,
    DEPTH24,
    DEPTH32F,
    DEPTH24_STENCIL8,
    DEPTH32F_STENCIL8,

    // Compressed formats
    BC1_RGB,
    BC1_RGBA,
    BC3_RGBA,
    BC5_RG,
    BC7_RGBA
};

/**
 * Primitive topology
 */
enum class PrimitiveTopology {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip
};

/**
 * Comparison function
 */
enum class CompareFunc {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

/**
 * Blend factor
 */
enum class BlendFactor {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    ConstantColor,
    OneMinusConstantColor
};

/**
 * Blend operation
 */
enum class BlendOp {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max
};

/**
 * Cull mode
 */
enum class CullMode {
    None,
    Front,
    Back
};

/**
 * Fill mode
 */
enum class FillMode {
    Solid,
    Wireframe
};

/**
 * Winding order
 */
enum class WindingOrder {
    Clockwise,
    CounterClockwise
};

/**
 * Texture filtering
 */
enum class FilterMode {
    Nearest,
    Linear
};

/**
 * Texture wrap mode
 */
enum class WrapMode {
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder
};

/**
 * Shader stage
 */
enum class ShaderStage {
    Vertex,
    Fragment,
    Geometry,
    Compute
};

/**
 * Buffer usage flags (can be combined)
 */
enum class BufferUsage : uint32_t {
    None = 0,
    Vertex = 1 << 0,
    Index = 1 << 1,
    Uniform = 1 << 2,
    Storage = 1 << 3,
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5
};

inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline BufferUsage operator&(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/**
 * Texture type
 */
enum class TextureType {
    Texture2D,
    Texture3D,
    TextureCube,
    Texture2DArray,
    TextureCubeArray
};

/**
 * Texture usage flags
 */
enum class TextureUsage : uint32_t {
    None = 0,
    Sampled = 1 << 0,
    Storage = 1 << 1,
    RenderTarget = 1 << 2,
    DepthStencil = 1 << 3,
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5
};

inline TextureUsage operator|(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline TextureUsage operator&(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

/**
 * Load operation for render pass attachments
 */
enum class LoadOp {
    Load,      // Preserve existing contents
    Clear,     // Clear to specified color/depth
    DontCare   // Don't care about previous contents
};

/**
 * Store operation for render pass attachments
 */
enum class StoreOp {
    Store,     // Store the results
    DontCare   // Don't care about storing
};

/**
 * Vertex attribute format
 */
enum class VertexFormat {
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    Int2,
    Int3,
    Int4,
    UInt,
    UInt2,
    UInt3,
    UInt4
};

/**
 * Index format
 */
enum class IndexFormat {
    UInt16,
    UInt32
};

/**
 * Vertex input rate for a vertex buffer binding.
 * Vertex   - attributes advance once per vertex (the normal case).
 * Instance - attributes advance once per instance (GPU instancing); the same
 *            value is fed to every vertex of an instance. The advance step is
 *            controlled by a divisor of 1 (one new value per instance).
 */
enum class VertexInputRate {
    Vertex,
    Instance
};

/**
 * Native window handle for creating swapchains
 */
struct NativeWindowHandle {
    void* platformHandle = nullptr;  // HWND, NSWindow*, X11 Window, Qt winId, etc.
    void* displayHandle = nullptr;   // Optional for GL/Vulkan/Wayland
};

/**
 * Viewport description
 */
struct Viewport {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
};

/**
 * Scissor rectangle
 */
struct ScissorRect {
    int32_t x = 0;
    int32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;

    bool operator==(const ScissorRect& o) const {
        return x == o.x && y == o.y && width == o.width && height == o.height;
    }
    bool operator!=(const ScissorRect& o) const { return !(*this == o); }
};

} // namespace lupine

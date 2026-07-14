#ifdef __EMSCRIPTEN__

#include "lupine/rendering/backends/webgl/WebGLState.hpp"
#include "lupine/logger/Logger.hpp"

#include <GLES3/gl3.h>
#include <emscripten/html5.h>

namespace lupine {

WebGLState::WebGLState() {
    // Initialize texture binding arrays
    boundTextures.resize(16);
    boundSamplers.resize(16);
}

WebGLState::~WebGLState() {
    // Cleanup is handled by GfxDeviceWebGL::shutdown()
}

void WebGLState::bindProgram(GLuint program) {
    if (boundProgram != program) {
        glUseProgram(program);
        boundProgram = program;
    }
}

void WebGLState::bindVAO(GLuint vao) {
    if (boundVAO != vao) {
        glBindVertexArray(vao);
        boundVAO = vao;
    }
}

void WebGLState::bindBuffer(GLenum target, GLuint buffer) {
    if (target == GL_ARRAY_BUFFER) {
        if (boundArrayBuffer != buffer) {
            glBindBuffer(target, buffer);
            boundArrayBuffer = buffer;
        }
    } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
        if (boundElementBuffer != buffer) {
            glBindBuffer(target, buffer);
            boundElementBuffer = buffer;
        }
    } else {
        // Other buffer types (uniform, etc.) - always bind
        glBindBuffer(target, buffer);
    }
}

void WebGLState::bindFramebuffer(GLuint fbo) {
    if (boundFramebuffer != fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        boundFramebuffer = fbo;
    }
}

void WebGLState::bindTexture(uint32_t unit, GLenum target, GLuint texture) {
    if (unit >= boundTextures.size()) {
        boundTextures.resize(unit + 1);
    }

    TextureBinding& binding = boundTextures[unit];

    bool needsRebind = (binding.id != texture) || (binding.target != target);

    if (needsRebind) {
        glActiveTexture(GL_TEXTURE0 + unit);

        // If we're changing texture types on this unit (e.g., 2D to CubeMap),
        // we must unbind the old type first to prevent WebGL errors:
        // "Two textures of different types use the same sampler location"
        if (binding.target != 0 && binding.target != target && binding.id != 0) {
            glBindTexture(binding.target, 0);
        }

        glBindTexture(target, texture);

        binding.id = texture;
        binding.target = target;
    }
}

void WebGLState::bindSampler(uint32_t unit, GLuint sampler) {
    if (unit >= boundSamplers.size()) {
        boundSamplers.resize(unit + 1);
    }

    if (boundSamplers[unit] != sampler) {
        glBindSampler(unit, sampler);
        boundSamplers[unit] = sampler;
    }
}

void WebGLState::resetBindingState() {
    boundProgram = 0;
    boundVAO = 0;
    boundArrayBuffer = 0;
    boundElementBuffer = 0;
    boundFramebuffer = 0;
    currentFboColorTexture = 0;
    currentFboDepthTexture = 0;

    for (auto& binding : boundTextures) {
        binding.id = 0;
        binding.target = 0;
    }
    for (auto& sampler : boundSamplers) {
        sampler = 0;
    }

    viewport = {0, 0, 0, 0, 0, 1};
    scissor = {0, 0, 0, 0, false};
}

void WebGLState::unbindFramebufferTextures() {
    // Prevent feedback loop: unbind any textures that are attached to the current FBO
    // This must be called before draw calls to avoid:
    // "Feedback loop formed between Framebuffer and active Texture"

    if (currentFboColorTexture == 0 && currentFboDepthTexture == 0) {
        return; // Default framebuffer or no attachments tracked
    }

    for (size_t unit = 0; unit < boundTextures.size(); ++unit) {
        TextureBinding& binding = boundTextures[unit];
        if (binding.id != 0) {
            bool needsUnbind = (binding.id == currentFboColorTexture) ||
                               (binding.id == currentFboDepthTexture);
            if (needsUnbind) {
                glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
                glBindTexture(binding.target, 0);
                binding.id = 0;
                // Keep target so we know what type was there
            }
        }
    }
}

// =============================================================================
// WebGL Utility Functions
// =============================================================================

namespace WebGLUtils {

GLenum toWebGLTextureFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_UNORM:           return GL_RED;
        case TextureFormat::RG8_UNORM:          return GL_RG;
        case TextureFormat::RGBA8_UNORM:        return GL_RGBA;
        case TextureFormat::RGBA8_SRGB:         return GL_RGBA;
        case TextureFormat::BGRA8_UNORM:        return GL_RGBA; // WebGL doesn't support BGRA
        case TextureFormat::BGRA8_SRGB:         return GL_RGBA;

        case TextureFormat::R16_FLOAT:          return GL_RED;
        case TextureFormat::RG16_FLOAT:         return GL_RG;
        case TextureFormat::RGBA16_FLOAT:       return GL_RGBA;

        case TextureFormat::R32_FLOAT:          return GL_RED;
        case TextureFormat::RG32_FLOAT:         return GL_RG;
        case TextureFormat::RGB32_FLOAT:        return GL_RGB;
        case TextureFormat::RGBA32_FLOAT:       return GL_RGBA;

        case TextureFormat::DEPTH16:            return GL_DEPTH_COMPONENT;
        case TextureFormat::DEPTH24:            return GL_DEPTH_COMPONENT;
        case TextureFormat::DEPTH32F:           return GL_DEPTH_COMPONENT;
        case TextureFormat::DEPTH24_STENCIL8:   return GL_DEPTH_STENCIL;
        case TextureFormat::DEPTH32F_STENCIL8:  return GL_DEPTH_STENCIL;

        default:
            
            return GL_RGBA;
    }
}

GLenum toWebGLInternalFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_UNORM:           return GL_R8;
        case TextureFormat::RG8_UNORM:          return GL_RG8;
        case TextureFormat::RGBA8_UNORM:        return GL_RGBA8;
        case TextureFormat::RGBA8_SRGB:         return GL_SRGB8_ALPHA8;
        case TextureFormat::BGRA8_UNORM:        return GL_RGBA8; // WebGL doesn't support BGRA
        case TextureFormat::BGRA8_SRGB:         return GL_SRGB8_ALPHA8;

        case TextureFormat::R16_FLOAT:          return GL_R16F;
        case TextureFormat::RG16_FLOAT:         return GL_RG16F;
        case TextureFormat::RGBA16_FLOAT:       return GL_RGBA16F;

        case TextureFormat::R32_FLOAT:          return GL_R32F;
        case TextureFormat::RG32_FLOAT:         return GL_RG32F;
        case TextureFormat::RGB32_FLOAT:        return GL_RGB32F;
        case TextureFormat::RGBA32_FLOAT:       return GL_RGBA32F;

        case TextureFormat::DEPTH16:            return GL_DEPTH_COMPONENT16;
        case TextureFormat::DEPTH24:            return GL_DEPTH_COMPONENT24;
        case TextureFormat::DEPTH32F:           return GL_DEPTH_COMPONENT32F;
        case TextureFormat::DEPTH24_STENCIL8:   return GL_DEPTH24_STENCIL8;
        case TextureFormat::DEPTH32F_STENCIL8:  return GL_DEPTH32F_STENCIL8;

        default:
            
            return GL_RGBA8;
    }
}

GLenum toWebGLDataType(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_UNORM:
        case TextureFormat::RG8_UNORM:
        case TextureFormat::RGBA8_UNORM:
        case TextureFormat::RGBA8_SRGB:
        case TextureFormat::BGRA8_UNORM:
        case TextureFormat::BGRA8_SRGB:
            return GL_UNSIGNED_BYTE;

        case TextureFormat::R16_FLOAT:
        case TextureFormat::RG16_FLOAT:
        case TextureFormat::RGBA16_FLOAT:
            return GL_HALF_FLOAT;

        case TextureFormat::R32_FLOAT:
        case TextureFormat::RG32_FLOAT:
        case TextureFormat::RGB32_FLOAT:
        case TextureFormat::RGBA32_FLOAT:
        case TextureFormat::DEPTH32F:
            return GL_FLOAT;

        case TextureFormat::DEPTH16:
            return GL_UNSIGNED_SHORT;

        case TextureFormat::DEPTH24:
            return GL_UNSIGNED_INT;

        case TextureFormat::DEPTH24_STENCIL8:
            return GL_UNSIGNED_INT_24_8;

        case TextureFormat::DEPTH32F_STENCIL8:
            return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;

        default:
            return GL_UNSIGNED_BYTE;
    }
}

GLenum toWebGLTextureTarget(TextureType type) {
    switch (type) {
        case TextureType::Texture2D:        return GL_TEXTURE_2D;
        case TextureType::Texture3D:        return GL_TEXTURE_3D;
        case TextureType::TextureCube:      return GL_TEXTURE_CUBE_MAP;
        case TextureType::Texture2DArray:   return GL_TEXTURE_2D_ARRAY;
        // WebGL 2.0 doesn't support cube map arrays
        case TextureType::TextureCubeArray:
            
            return GL_TEXTURE_CUBE_MAP;
        default:
            return GL_TEXTURE_2D;
    }
}

GLenum toWebGLTopology(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::PointList:      return GL_POINTS;
        case PrimitiveTopology::LineList:       return GL_LINES;
        case PrimitiveTopology::LineStrip:      return GL_LINE_STRIP;
        case PrimitiveTopology::TriangleList:   return GL_TRIANGLES;
        case PrimitiveTopology::TriangleStrip:  return GL_TRIANGLE_STRIP;
        default:
            return GL_TRIANGLES;
    }
}

GLenum toWebGLCompareFunc(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never:        return GL_NEVER;
        case CompareFunc::Less:         return GL_LESS;
        case CompareFunc::Equal:        return GL_EQUAL;
        case CompareFunc::LessEqual:    return GL_LEQUAL;
        case CompareFunc::Greater:      return GL_GREATER;
        case CompareFunc::NotEqual:     return GL_NOTEQUAL;
        case CompareFunc::GreaterEqual: return GL_GEQUAL;
        case CompareFunc::Always:       return GL_ALWAYS;
        default:
            return GL_LESS;
    }
}

GLenum toWebGLBlendFactor(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero:                 return GL_ZERO;
        case BlendFactor::One:                  return GL_ONE;
        case BlendFactor::SrcColor:             return GL_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor:     return GL_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:             return GL_DST_COLOR;
        case BlendFactor::OneMinusDstColor:     return GL_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha:             return GL_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha:     return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:             return GL_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha:     return GL_ONE_MINUS_DST_ALPHA;
        case BlendFactor::ConstantColor:        return GL_CONSTANT_COLOR;
        case BlendFactor::OneMinusConstantColor: return GL_ONE_MINUS_CONSTANT_COLOR;
        default:
            return GL_ONE;
    }
}

GLenum toWebGLBlendOp(BlendOp op) {
    switch (op) {
        case BlendOp::Add:              return GL_FUNC_ADD;
        case BlendOp::Subtract:         return GL_FUNC_SUBTRACT;
        case BlendOp::ReverseSubtract:  return GL_FUNC_REVERSE_SUBTRACT;
        case BlendOp::Min:              return GL_MIN;
        case BlendOp::Max:              return GL_MAX;
        default:
            return GL_FUNC_ADD;
    }
}

GLenum toWebGLFilterMode(FilterMode mode) {
    switch (mode) {
        case FilterMode::Nearest:   return GL_NEAREST;
        case FilterMode::Linear:    return GL_LINEAR;
        default:
            return GL_LINEAR;
    }
}

GLenum toWebGLWrapMode(WrapMode mode) {
    switch (mode) {
        case WrapMode::Repeat:          return GL_REPEAT;
        case WrapMode::MirroredRepeat:  return GL_MIRRORED_REPEAT;
        case WrapMode::ClampToEdge:     return GL_CLAMP_TO_EDGE;
        // WebGL doesn't support GL_CLAMP_TO_BORDER, use ClampToEdge instead
        case WrapMode::ClampToBorder:
            return GL_CLAMP_TO_EDGE;
        default:
            return GL_REPEAT;
    }
}

GLenum toWebGLShaderStage(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex:   return GL_VERTEX_SHADER;
        case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
        // WebGL 2.0 doesn't support geometry or compute shaders
        case ShaderStage::Geometry:
            
            return GL_VERTEX_SHADER;
        case ShaderStage::Compute:
            
            return GL_FRAGMENT_SHADER;
        default:
            return GL_VERTEX_SHADER;
    }
}

GLuint getVertexFormatSize(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float:   return 4;
        case VertexFormat::Float2:  return 8;
        case VertexFormat::Float3:  return 12;
        case VertexFormat::Float4:  return 16;
        case VertexFormat::Int:     return 4;
        case VertexFormat::Int2:    return 8;
        case VertexFormat::Int3:    return 12;
        case VertexFormat::Int4:    return 16;
        case VertexFormat::UInt:    return 4;
        case VertexFormat::UInt2:   return 8;
        case VertexFormat::UInt3:   return 12;
        case VertexFormat::UInt4:   return 16;
        default:
            return 4;
    }
}

GLenum getVertexFormatType(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float:
        case VertexFormat::Float2:
        case VertexFormat::Float3:
        case VertexFormat::Float4:
            return GL_FLOAT;
        case VertexFormat::Int:
        case VertexFormat::Int2:
        case VertexFormat::Int3:
        case VertexFormat::Int4:
            return GL_INT;
        case VertexFormat::UInt:
        case VertexFormat::UInt2:
        case VertexFormat::UInt3:
        case VertexFormat::UInt4:
            return GL_UNSIGNED_INT;
        default:
            return GL_FLOAT;
    }
}

GLint getVertexFormatCount(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float:
        case VertexFormat::Int:
        case VertexFormat::UInt:
            return 1;
        case VertexFormat::Float2:
        case VertexFormat::Int2:
        case VertexFormat::UInt2:
            return 2;
        case VertexFormat::Float3:
        case VertexFormat::Int3:
        case VertexFormat::UInt3:
            return 3;
        case VertexFormat::Float4:
        case VertexFormat::Int4:
        case VertexFormat::UInt4:
            return 4;
        default:
            return 1;
    }
}

bool isVertexFormatNormalized(VertexFormat format) {
    // None of the formats are normalized by default
    // Integer formats are not normalized
    return false;
}

bool isSamplerUniformType(GLenum uniformType) {
    switch (uniformType) {
        case GL_SAMPLER_2D:
        case GL_SAMPLER_3D:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_2D_SHADOW:
        case GL_SAMPLER_2D_ARRAY:
        case GL_SAMPLER_2D_ARRAY_SHADOW:
        case GL_SAMPLER_CUBE_SHADOW:
        case GL_INT_SAMPLER_2D:
        case GL_INT_SAMPLER_3D:
        case GL_INT_SAMPLER_CUBE:
        case GL_INT_SAMPLER_2D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_UNSIGNED_INT_SAMPLER_3D:
        case GL_UNSIGNED_INT_SAMPLER_CUBE:
        case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
            return true;
        default:
            return false;
    }
}

bool isFormatSupported(TextureFormat format) {
    switch (format) {
        // Supported formats
        case TextureFormat::R8_UNORM:
        case TextureFormat::RG8_UNORM:
        case TextureFormat::RGBA8_UNORM:
        case TextureFormat::RGBA8_SRGB:
        case TextureFormat::R16_FLOAT:
        case TextureFormat::RG16_FLOAT:
        case TextureFormat::RGBA16_FLOAT:
        case TextureFormat::R32_FLOAT:
        case TextureFormat::RG32_FLOAT:
        case TextureFormat::RGBA32_FLOAT:
        case TextureFormat::DEPTH16:
        case TextureFormat::DEPTH24:
        case TextureFormat::DEPTH32F:
        case TextureFormat::DEPTH24_STENCIL8:
        case TextureFormat::DEPTH32F_STENCIL8:
            return true;

        // Unsupported formats in WebGL 2.0
        case TextureFormat::BGRA8_UNORM:
        case TextureFormat::BGRA8_SRGB:
        case TextureFormat::RGB32_FLOAT: // Limited support
        case TextureFormat::BC1_RGB:
        case TextureFormat::BC1_RGBA:
        case TextureFormat::BC3_RGBA:
        case TextureFormat::BC5_RG:
        case TextureFormat::BC7_RGBA:
            return false;

        default:
            return false;
    }
}

TextureFormat getFallbackFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::BGRA8_UNORM:
            return TextureFormat::RGBA8_UNORM;
        case TextureFormat::BGRA8_SRGB:
            return TextureFormat::RGBA8_SRGB;
        case TextureFormat::RGB32_FLOAT:
            return TextureFormat::RGBA32_FLOAT;
        // Compressed formats fall back to uncompressed
        case TextureFormat::BC1_RGB:
        case TextureFormat::BC1_RGBA:
        case TextureFormat::BC3_RGBA:
        case TextureFormat::BC7_RGBA:
            return TextureFormat::RGBA8_UNORM;
        case TextureFormat::BC5_RG:
            return TextureFormat::RG8_UNORM;
        default:
            return format;
    }
}

} // namespace WebGLUtils

} // namespace lupine

#endif // __EMSCRIPTEN__

#include "lupine/rendering/backends/opengl/OpenGLState.hpp"
#include "lupine/logger/Logger.hpp"
#include <GL/glew.h>
#include <cstring>

namespace lupine {

OpenGLState::OpenGLState() {

    boundTextures.resize(32);
    boundSamplers.resize(32, 0);
}

OpenGLState::~OpenGLState() {
}

void OpenGLState::bindProgram(GLuint program) {
    if (boundProgram != program) {
        glUseProgram(program);
        boundProgram = program;
    }
}

void OpenGLState::bindVAO(GLuint vao) {
    if (boundVAO != vao) {

        while (glGetError() != GL_NO_ERROR);

        glBindVertexArray(vao);

        GLenum error = glGetError();
        if (error != GL_NO_ERROR) {

        }

        boundVAO = vao;

        boundElementBuffer = UINT32_MAX;
    }
}

void OpenGLState::bindBuffer(GLenum target, GLuint buffer) {
    if (target == GL_ARRAY_BUFFER) {
        if (boundArrayBuffer != buffer) {
            glBindBuffer(target, buffer);
            boundArrayBuffer = buffer;
        }
    } else if (target == GL_ELEMENT_ARRAY_BUFFER) {

        glBindBuffer(target, buffer);
        boundElementBuffer = buffer;
    } else {
        glBindBuffer(target, buffer);
    }
}

void OpenGLState::bindFramebuffer(GLuint fbo) {
    if (boundFramebuffer != fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        boundFramebuffer = fbo;
    }
}

void OpenGLState::bindTexture(uint32_t unit, GLenum target, GLuint texture) {
    if (unit >= boundTextures.size()) {
        return;
    }

    TextureBinding& binding = boundTextures[unit];

    bool needsRebind = (binding.id != texture) || (binding.target != target);

    if (needsRebind) {
        glActiveTexture(GL_TEXTURE0 + unit);

        if (binding.target != 0 && binding.target != target && binding.id != 0) {
            glBindTexture(binding.target, 0);
        }

        glBindTexture(target, texture);

        binding.id = texture;
        binding.target = target;
    }
}

void OpenGLState::bindSampler(uint32_t unit, GLuint sampler) {
    if (unit < boundSamplers.size() && boundSamplers[unit] != sampler) {
        glBindSampler(unit, sampler);
        boundSamplers[unit] = sampler;
    }
}

void OpenGLState::resetBindingState() {
    // Reset all binding state to force rebinding on next use
    // This is necessary when runtime restarts because OpenGL texture IDs
    // may be reused for different textures
    boundProgram = 0;
    boundVAO = 0;
    boundArrayBuffer = 0;
    boundElementBuffer = 0;
    boundFramebuffer = 0;

    // Clear all texture bindings
    for (auto& binding : boundTextures) {
        binding.id = 0;
        binding.target = 0;
    }

    // Clear all sampler bindings
    std::fill(boundSamplers.begin(), boundSamplers.end(), 0);
}

namespace GLUtils {

GLenum toGLTextureFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_UNORM: return GL_RED;
        case TextureFormat::RG8_UNORM: return GL_RG;
        case TextureFormat::RGBA8_UNORM:
        case TextureFormat::RGBA8_SRGB: return GL_RGBA;
        case TextureFormat::BGRA8_UNORM:
        case TextureFormat::BGRA8_SRGB: return GL_BGRA;
        case TextureFormat::R16_FLOAT: return GL_RED;
        case TextureFormat::RG16_FLOAT: return GL_RG;
        case TextureFormat::RGBA16_FLOAT: return GL_RGBA;
        case TextureFormat::R32_FLOAT: return GL_RED;
        case TextureFormat::RG32_FLOAT: return GL_RG;
        case TextureFormat::RGB32_FLOAT: return GL_RGB;
        case TextureFormat::RGBA32_FLOAT: return GL_RGBA;
        case TextureFormat::DEPTH16:
        case TextureFormat::DEPTH24:
        case TextureFormat::DEPTH32F: return GL_DEPTH_COMPONENT;
        case TextureFormat::DEPTH24_STENCIL8:
        case TextureFormat::DEPTH32F_STENCIL8: return GL_DEPTH_STENCIL;
        default: return GL_RGBA;
    }
}

GLenum toGLInternalFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_UNORM: return GL_R8;
        case TextureFormat::RG8_UNORM: return GL_RG8;
        case TextureFormat::RGBA8_UNORM: return GL_RGBA8;
        case TextureFormat::RGBA8_SRGB: return GL_SRGB8_ALPHA8;
        case TextureFormat::BGRA8_UNORM: return GL_RGBA8;
        case TextureFormat::BGRA8_SRGB: return GL_SRGB8_ALPHA8;
        case TextureFormat::R16_FLOAT: return GL_R16F;
        case TextureFormat::RG16_FLOAT: return GL_RG16F;
        case TextureFormat::RGBA16_FLOAT: return GL_RGBA16F;
        case TextureFormat::R32_FLOAT: return GL_R32F;
        case TextureFormat::RG32_FLOAT: return GL_RG32F;
        case TextureFormat::RGB32_FLOAT: return GL_RGB32F;
        case TextureFormat::RGBA32_FLOAT: return GL_RGBA32F;
        case TextureFormat::DEPTH16: return GL_DEPTH_COMPONENT16;
        case TextureFormat::DEPTH24: return GL_DEPTH_COMPONENT24;
        case TextureFormat::DEPTH32F: return GL_DEPTH_COMPONENT32F;
        case TextureFormat::DEPTH24_STENCIL8: return GL_DEPTH24_STENCIL8;
        case TextureFormat::DEPTH32F_STENCIL8: return GL_DEPTH32F_STENCIL8;
        default: return GL_RGBA8;
    }
}

GLenum toGLDataType(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_UNORM:
        case TextureFormat::RG8_UNORM:
        case TextureFormat::RGBA8_UNORM:
        case TextureFormat::RGBA8_SRGB:
        case TextureFormat::BGRA8_UNORM:
        case TextureFormat::BGRA8_SRGB: return GL_UNSIGNED_BYTE;
        case TextureFormat::R16_FLOAT:
        case TextureFormat::RG16_FLOAT:
        case TextureFormat::RGBA16_FLOAT: return GL_HALF_FLOAT;
        case TextureFormat::R32_FLOAT:
        case TextureFormat::RG32_FLOAT:
        case TextureFormat::RGB32_FLOAT:
        case TextureFormat::RGBA32_FLOAT: return GL_FLOAT;
        case TextureFormat::DEPTH16: return GL_UNSIGNED_SHORT;
        case TextureFormat::DEPTH24: return GL_UNSIGNED_INT;
        case TextureFormat::DEPTH32F: return GL_FLOAT;
        case TextureFormat::DEPTH24_STENCIL8: return GL_UNSIGNED_INT_24_8;
        case TextureFormat::DEPTH32F_STENCIL8: return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
        default: return GL_UNSIGNED_BYTE;
    }
}

GLenum toGLTextureTarget(TextureType type) {
    switch (type) {
        case TextureType::Texture2D: return GL_TEXTURE_2D;
        case TextureType::Texture3D: return GL_TEXTURE_3D;
        case TextureType::TextureCube: return GL_TEXTURE_CUBE_MAP;
        case TextureType::Texture2DArray: return GL_TEXTURE_2D_ARRAY;
        case TextureType::TextureCubeArray: return GL_TEXTURE_CUBE_MAP_ARRAY;
        default: return GL_TEXTURE_2D;
    }
}

GLenum toGLTopology(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::PointList: return GL_POINTS;
        case PrimitiveTopology::LineList: return GL_LINES;
        case PrimitiveTopology::LineStrip: return GL_LINE_STRIP;
        case PrimitiveTopology::TriangleList: return GL_TRIANGLES;
        case PrimitiveTopology::TriangleStrip: return GL_TRIANGLE_STRIP;
        default: return GL_TRIANGLES;
    }
}

GLenum toGLCompareFunc(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never: return GL_NEVER;
        case CompareFunc::Less: return GL_LESS;
        case CompareFunc::Equal: return GL_EQUAL;
        case CompareFunc::LessEqual: return GL_LEQUAL;
        case CompareFunc::Greater: return GL_GREATER;
        case CompareFunc::NotEqual: return GL_NOTEQUAL;
        case CompareFunc::GreaterEqual: return GL_GEQUAL;
        case CompareFunc::Always: return GL_ALWAYS;
        default: return GL_LESS;
    }
}

GLenum toGLBlendFactor(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero: return GL_ZERO;
        case BlendFactor::One: return GL_ONE;
        case BlendFactor::SrcColor: return GL_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor: return GL_DST_COLOR;
        case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha: return GL_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha: return GL_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
        case BlendFactor::ConstantColor: return GL_CONSTANT_COLOR;
        case BlendFactor::OneMinusConstantColor: return GL_ONE_MINUS_CONSTANT_COLOR;
        default: return GL_ONE;
    }
}

GLenum toGLBlendOp(BlendOp op) {
    switch (op) {
        case BlendOp::Add: return GL_FUNC_ADD;
        case BlendOp::Subtract: return GL_FUNC_SUBTRACT;
        case BlendOp::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
        case BlendOp::Min: return GL_MIN;
        case BlendOp::Max: return GL_MAX;
        default: return GL_FUNC_ADD;
    }
}

GLenum toGLFilterMode(FilterMode mode) {
    switch (mode) {
        case FilterMode::Nearest: return GL_NEAREST;
        case FilterMode::Linear: return GL_LINEAR;
        default: return GL_LINEAR;
    }
}

GLenum toGLWrapMode(WrapMode mode) {
    switch (mode) {
        case WrapMode::Repeat: return GL_REPEAT;
        case WrapMode::MirroredRepeat: return GL_MIRRORED_REPEAT;
        case WrapMode::ClampToEdge: return GL_CLAMP_TO_EDGE;
        case WrapMode::ClampToBorder: return GL_CLAMP_TO_BORDER;
        default: return GL_REPEAT;
    }
}

GLenum toGLShaderStage(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex: return GL_VERTEX_SHADER;
        case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
        case ShaderStage::Geometry: return GL_GEOMETRY_SHADER;
        case ShaderStage::Compute: return GL_COMPUTE_SHADER;
        default: return GL_VERTEX_SHADER;
    }
}

GLuint getVertexFormatSize(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float: return sizeof(float);
        case VertexFormat::Float2: return sizeof(float) * 2;
        case VertexFormat::Float3: return sizeof(float) * 3;
        case VertexFormat::Float4: return sizeof(float) * 4;
        case VertexFormat::Int: return sizeof(int32_t);
        case VertexFormat::Int2: return sizeof(int32_t) * 2;
        case VertexFormat::Int3: return sizeof(int32_t) * 3;
        case VertexFormat::Int4: return sizeof(int32_t) * 4;
        case VertexFormat::UInt: return sizeof(uint32_t);
        case VertexFormat::UInt2: return sizeof(uint32_t) * 2;
        case VertexFormat::UInt3: return sizeof(uint32_t) * 3;
        case VertexFormat::UInt4: return sizeof(uint32_t) * 4;
        default: return sizeof(float);
    }
}

GLenum getVertexFormatType(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float:
        case VertexFormat::Float2:
        case VertexFormat::Float3:
        case VertexFormat::Float4: return GL_FLOAT;
        case VertexFormat::Int:
        case VertexFormat::Int2:
        case VertexFormat::Int3:
        case VertexFormat::Int4: return GL_INT;
        case VertexFormat::UInt:
        case VertexFormat::UInt2:
        case VertexFormat::UInt3:
        case VertexFormat::UInt4: return GL_UNSIGNED_INT;
        default: return GL_FLOAT;
    }
}

GLint getVertexFormatCount(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float:
        case VertexFormat::Int:
        case VertexFormat::UInt: return 1;
        case VertexFormat::Float2:
        case VertexFormat::Int2:
        case VertexFormat::UInt2: return 2;
        case VertexFormat::Float3:
        case VertexFormat::Int3:
        case VertexFormat::UInt3: return 3;
        case VertexFormat::Float4:
        case VertexFormat::Int4:
        case VertexFormat::UInt4: return 4;
        default: return 1;
    }
}

bool isVertexFormatNormalized(VertexFormat) {

    return false;
}

}

}

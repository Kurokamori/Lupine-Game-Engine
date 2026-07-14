#include "lupine/rendering/CameraEffectStack.hpp"
#include "lupine/rendering/CameraEffectShaders.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/gfx/GfxCommandList.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/rendering/ShaderTranslator.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/logger/Logger.hpp"

#include <cstddef>

namespace lupine {

namespace {

// Replace every occurrence of `token` in `src` with `value`. Used to substitute the
// per-slot index "%S%" into a pointwise effect's namespaced uniform/function templates.
std::string replaceAll(const std::string& src, const std::string& token, const std::string& value) {
    if (token.empty()) {
        return src;
    }
    std::string out;
    out.reserve(src.size());
    size_t pos = 0;
    for (;;) {
        size_t found = src.find(token, pos);
        if (found == std::string::npos) {
            out.append(src, pos, std::string::npos);
            break;
        }
        out.append(src, pos, found - pos);
        out += value;
        pos = found + token.size();
    }
    return out;
}

} // namespace

// ============================================================================
// Lifecycle
// ============================================================================

CameraEffectStack::~CameraEffectStack() {
    shutdown();
}

bool CameraEffectStack::initialize(IGfxDevice* device) {
    if (m_initialized) {
        return true;
    }
    if (!device) {
        return false;
    }
    m_device = device;
    m_backend = device->getBackend();

    SamplerDesc linear;
    linear.minFilter = FilterMode::Linear;
    linear.magFilter = FilterMode::Linear;
    linear.mipFilter = FilterMode::Linear;
    linear.wrapU = WrapMode::ClampToEdge;
    linear.wrapV = WrapMode::ClampToEdge;
    linear.wrapW = WrapMode::ClampToEdge;
    m_linearClamp = m_device->createSampler(linear);

    SamplerDesc point;
    point.minFilter = FilterMode::Nearest;
    point.magFilter = FilterMode::Nearest;
    point.mipFilter = FilterMode::Nearest;
    point.wrapU = WrapMode::ClampToEdge;
    point.wrapV = WrapMode::ClampToEdge;
    point.wrapW = WrapMode::ClampToEdge;
    m_pointClamp = m_device->createSampler(point);

    if (!ensureFullscreenMesh()) {
        LOG_ERROR(LogCategory::Render, "[CameraEffect] Failed to create full-screen mesh");
        return false;
    }

    m_initialized = true;
    return true;
}

void CameraEffectStack::shutdown() {
    if (!m_device) {
        m_initialized = false;
        return;
    }

    destroyPipelines();
    releaseTargets();

    if (m_fullscreenMesh.isValid()) {
        m_device->destroyMesh(m_fullscreenMesh);
        m_fullscreenMesh = MeshHandle();
    }
    if (m_linearClamp.isValid()) {
        m_device->destroySampler(m_linearClamp);
        m_linearClamp = SamplerHandle();
    }
    if (m_pointClamp.isValid()) {
        m_device->destroySampler(m_pointClamp);
        m_pointClamp = SamplerHandle();
    }

    m_device = nullptr;
    m_initialized = false;
}

// ============================================================================
// Full-screen geometry
// ============================================================================

bool CameraEffectStack::ensureFullscreenMesh() {
    if (m_fullscreenMesh.isValid()) {
        return true;
    }

    MeshData data;
    data.vertices.resize(4);
    data.vertices[0].position = Vec3(-1.0f, -1.0f, 0.0f);
    data.vertices[0].texCoord = Vec2(0.0f, 0.0f);
    data.vertices[1].position = Vec3(1.0f, -1.0f, 0.0f);
    data.vertices[1].texCoord = Vec2(1.0f, 0.0f);
    data.vertices[2].position = Vec3(1.0f, 1.0f, 0.0f);
    data.vertices[2].texCoord = Vec2(1.0f, 1.0f);
    data.vertices[3].position = Vec3(-1.0f, 1.0f, 0.0f);
    data.vertices[3].texCoord = Vec2(0.0f, 1.0f);
    for (auto& v : data.vertices) {
        v.normal = Vec3(0.0f, 0.0f, 1.0f);
        v.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    data.indices = {0, 1, 2, 0, 2, 3};
    data.addSubmesh(6, 4);
    data.calculateBounds();

    m_fullscreenMesh = m_device->createMesh(data);
    return m_fullscreenMesh.isValid();
}

void CameraEffectStack::drawFullscreen(IGfxCommandList* cmd) {
    const GPUMesh* mesh = m_device->getMesh(m_fullscreenMesh);
    if (!mesh) {
        return;
    }
    cmd->bindVertexBuffer(mesh->vertexBuffer, 0);
    cmd->bindIndexBuffer(mesh->indexBuffer, IndexFormat::UInt32);
    cmd->drawIndexed(mesh->indexCount, 1, 0, 0, 0);
}

// ============================================================================
// Pipelines
// ============================================================================

CameraEffectStack::Pipeline* CameraEffectStack::getPipeline(const std::string& key,
                                                            const std::string& lshSource,
                                                            bool alphaBlend) {
    const std::string cacheKey = alphaBlend ? (key + "#b") : key;
    auto it = m_pipelines.find(cacheKey);
    if (it != m_pipelines.end() && it->second.valid()) {
        return &it->second;
    }

    ShaderTranslatorResult translated = ShaderTranslator::translate(lshSource, m_backend);
    if (!translated.success) {
        LOG_ERROR(LogCategory::Render, "[CameraEffect] Translate failed for '{}': {}",
                  key, translated.errorMessage);
        return nullptr;
    }

    const std::string& vertSource = (m_backend == GraphicsBackend::Metal)
        ? translated.combinedSource : translated.vertexSource;
    const std::string& fragSource = (m_backend == GraphicsBackend::Metal)
        ? translated.combinedSource : translated.fragmentSource;

    ShaderDesc vertDesc;
    vertDesc.stage = ShaderStage::Vertex;
    vertDesc.bytecode = vertSource.c_str();
    vertDesc.bytecodeSize = vertSource.size();
    ShaderHandle vs = m_device->createShader(vertDesc);
    if (!vs.isValid()) {
        LOG_ERROR(LogCategory::Render, "[CameraEffect] Vertex compile failed for '{}'", key);
        return nullptr;
    }

    ShaderDesc fragDesc;
    fragDesc.stage = ShaderStage::Fragment;
    fragDesc.bytecode = fragSource.c_str();
    fragDesc.bytecodeSize = fragSource.size();
    ShaderHandle fs = m_device->createShader(fragDesc);
    if (!fs.isValid()) {
        LOG_ERROR(LogCategory::Render, "[CameraEffect] Fragment compile failed for '{}'", key);
        m_device->destroyShader(vs);
        return nullptr;
    }

    VertexBufferLayout layout;
    layout.stride = sizeof(Vertex);
    layout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
    layout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
    layout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
    layout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});

    // Straight-alpha "over" for the final pass so a camera's layer composites onto
    // earlier cameras; opaque overwrite for intermediates / target-owning cameras.
    const BlendState blend = alphaBlend ? BlendState::alphaBlend() : BlendState::opaque();

    PipelineDesc desc;
    desc.shaders = {vs, fs};
    desc.vertexLayout = layout;
    desc.topology = PrimitiveTopology::TriangleList;
    desc.blendState = blend;
    desc.depthStencilState = DepthStencilState::noDepth();
    desc.rasterizerState.cullMode = CullMode::None;
    desc.rasterizerState.fillMode = FillMode::Solid;
    desc.colorFormat = TextureFormat::RGBA16_FLOAT;

    PipelineHandle pipeline = m_device->createPipeline(desc);
    if (!pipeline.isValid()) {
        LOG_ERROR(LogCategory::Render, "[CameraEffect] Pipeline creation failed for '{}'", key);
        m_device->destroyShader(vs);
        m_device->destroyShader(fs);
        return nullptr;
    }

    Pipeline pp;
    pp.vertex = vs;
    pp.fragment = fs;
    pp.pipeline = pipeline;
    m_pipelines[cacheKey] = pp;
    return &m_pipelines[cacheKey];
}

void CameraEffectStack::destroyPipelines() {
    if (!m_device) {
        m_pipelines.clear();
        return;
    }
    for (auto& [key, pp] : m_pipelines) {
        if (pp.pipeline.isValid()) m_device->destroyPipeline(pp.pipeline);
        if (pp.vertex.isValid()) m_device->destroyShader(pp.vertex);
        if (pp.fragment.isValid()) m_device->destroyShader(pp.fragment);
    }
    m_pipelines.clear();
}

// ============================================================================
// Render targets
// ============================================================================

CameraEffectStack::ColorTarget CameraEffectStack::createColorTarget(
    uint32_t width, uint32_t height, TextureFormat format) {
    ColorTarget t;
    RenderTargetDesc desc;
    desc.width = width;
    desc.height = height;
    desc.colorFormat = format;
    desc.hasColor = true;
    desc.hasDepth = false;
    desc.sampleCount = 1;
    t.target = m_device->createRenderTarget(desc);
    if (t.target.isValid()) {
        t.color = m_device->getRenderTargetColorTexture(t.target);
        t.width = width;
        t.height = height;
    }
    return t;
}

void CameraEffectStack::destroyColorTarget(ColorTarget& t) {
    if (t.target.isValid() && m_device) {
        m_device->destroyRenderTarget(t.target);
    }
    t = ColorTarget();
}

void CameraEffectStack::ensureTargets(uint32_t width, uint32_t height) {
    if (width != m_width || height != m_height) {
        releaseTargets();
        m_width = width;
        m_height = height;
    }
    if (!m_pingA.valid()) {
        m_pingA = createColorTarget(width, height, TextureFormat::RGBA16_FLOAT);
    }
    if (!m_pingB.valid()) {
        m_pingB = createColorTarget(width, height, TextureFormat::RGBA16_FLOAT);
    }
}

void CameraEffectStack::releaseTargets() {
    destroyColorTarget(m_pingA);
    destroyColorTarget(m_pingB);
}

// ============================================================================
// Per-pass helpers
// ============================================================================

uint32_t CameraEffectStack::textureSlot(uint32_t index) const {
    return (m_backend == GraphicsBackend::Vulkan) ? (4u + index) : index;
}

void CameraEffectStack::bindInputTexture(IGfxCommandList* cmd, const char* uniformName,
                                         TextureHandle tex, uint32_t index, bool pointSampler) {
    const uint32_t slot = textureSlot(index);
    cmd->bindTexture(tex, slot, 0);
    cmd->setUniformInt(uniformName, static_cast<int>(slot));
    SamplerHandle sampler = pointSampler ? m_pointClamp : m_linearClamp;
    if (sampler.isValid()) {
        cmd->bindSampler(sampler, index);
    }
}

bool CameraEffectStack::resolveFlipV(FlipYMode mode) const {
    switch (mode) {
        case FlipYMode::Off: return false;
        case FlipYMode::On:  return true;
        case FlipYMode::Auto:
        default:
            return IsTopLeftOrigin(m_backend);
    }
}

void CameraEffectStack::applyUniform(IGfxCommandList* cmd, const CameraEffectUniform& u) {
    switch (u.kind) {
        case CameraEffectUniform::Kind::Float: cmd->setUniformFloat(u.name.c_str(), u.fValue); break;
        case CameraEffectUniform::Kind::Int:   cmd->setUniformInt(u.name.c_str(), u.iValue); break;
        case CameraEffectUniform::Kind::Vec2:  cmd->setUniformVec2(u.name.c_str(), u.v2Value); break;
        case CameraEffectUniform::Kind::Vec3:  cmd->setUniformVec3(u.name.c_str(), u.v3Value); break;
        case CameraEffectUniform::Kind::Vec4:  cmd->setUniformVec4(u.name.c_str(), u.v4Value); break;
        case CameraEffectUniform::Kind::Mat4:  cmd->setUniformMat4(u.name.c_str(), u.m4Value); break;
    }
}

// ============================================================================
// Step assembly
// ============================================================================

CameraEffectStack::RenderStep CameraEffectStack::buildCompositeStep(
    const std::vector<const CameraComposeContribution*>& group) {
    RenderStep step;
    std::string ids;
    std::string decls;
    std::string funcs;
    std::string calls;

    for (size_t k = 0; k < group.size(); ++k) {
        const CameraComposeContribution* c = group[k];
        const std::string slot = std::to_string(k);
        ids += c->id;
        ids += "|";
        decls += replaceAll(c->uniformDecls, "%S%", slot);
        decls += "\n";
        funcs += replaceAll(c->applyFunction, "%S%", slot);
        funcs += "\n";
        calls += "        color = fx" + slot + "_apply(color, uv);\n";
        for (CameraEffectUniform u : c->uniforms) {
            u.name = replaceAll(u.name, "%S%", slot);
            step.uniforms.push_back(u);
        }
    }

    std::string fragment = funcs;
    fragment += "    void main() {\n";
    fragment += "        vec2 uv = v_TexCoord;\n";
    fragment += "        vec4 src = SAMPLE(u_SceneTex, uv);\n";
    fragment += "        vec3 color = src.rgb;\n";
    fragment += calls;
    // Preserve the captured alpha so the camera's layer composites correctly over
    // earlier cameras (a UI camera's transparent background must stay transparent).
    fragment += "        FragColor = vec4(color, src.a);\n";
    fragment += "    }\n";

    // Unique, sanitized shader name per composite group (aids debug markers / logs and
    // avoids any chance of a name-based collision in the backend shader compilers).
    std::string shaderName = "CamComposite_";
    for (char c : ids) {
        shaderName += (c == '|') ? '_' : c;
    }

    step.cacheKey = "camcomp:" + ids;
    step.lshSource = CameraEffectShaders::BuildStandaloneShader(shaderName, decls, fragment);
    step.needsDepth = false;
    step.pointSampleScene = false;
    return step;
}

void CameraEffectStack::buildSteps(const CameraEffectChain& chain, std::vector<RenderStep>& out) {
    auto pushPasses = [&out](const ResolvedCameraEffect& e) {
        for (const CameraEffectPass& p : e.passes) {
            RenderStep step;
            step.cacheKey = p.cacheKey;
            step.lshSource = p.lshSource;
            step.uniforms = p.uniforms;
            step.needsDepth = p.needsDepth;
            step.pointSampleScene = p.pointSampleScene;
            out.push_back(std::move(step));
        }
    };

    if (chain.mode == CameraEffectRenderMode::Composite) {
        std::vector<const CameraComposeContribution*> group;
        auto flush = [&]() {
            if (!group.empty()) {
                out.push_back(buildCompositeStep(group));
                group.clear();
            }
        };
        for (const ResolvedCameraEffect& e : chain.effects) {
            if (e.composable) {
                group.push_back(&e.compose);
            } else {
                flush();
                pushPasses(e);
            }
        }
        flush();
    } else {
        for (const ResolvedCameraEffect& e : chain.effects) {
            if (e.composable) {
                std::vector<const CameraComposeContribution*> single{&e.compose};
                out.push_back(buildCompositeStep(single));
            } else {
                pushPasses(e);
            }
        }
    }
}

// ============================================================================
// Execution
// ============================================================================

void CameraEffectStack::runStep(const RenderStep& step, RenderTargetHandle dst,
                                TextureHandle srcColor, TextureHandle srcDepth,
                                uint32_t width, uint32_t height,
                                const CameraEffectChain& chain, bool toFinal) {
    // The final pass alpha-blends onto the real target ONLY for an overlay camera (one
    // that doesn't clear color, so the target already holds an earlier camera's output).
    // A camera that clears/owns its target overwrites it, preserving its own layer alpha.
    // Intermediate passes always overwrite their scratch targets.
    const bool finalBlend = toFinal && !chain.cameraClearsColor;
    Pipeline* pp = getPipeline(step.cacheKey, step.lshSource, finalBlend);
    if (!pp) {
        return;
    }

    auto cmd = m_device->beginFrame(dst);
    if (!cmd) {
        return;
    }

    Viewport vp{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    cmd->setViewport(vp);
    cmd->setScissor(ScissorRect{0, 0, width, height});
    cmd->beginDebugMarker("CameraEffect.Pass");

    // The pass pipelines are created RGBA16F; intermediate targets match while the final
    // target is the swapchain/RT (RGBA8). Bind the variant for the destination's format.
    cmd->bindPipeline(m_device->getColorFormatVariant(pp->pipeline,
                                                      m_device->getRenderTargetColorFormat(dst)));

    cmd->setUniformFloat("u_FlipV", resolveFlipV(chain.flipY) ? 1.0f : 0.0f);
    cmd->setUniformVec2("u_Resolution", Vec2(static_cast<float>(width), static_cast<float>(height)));
    cmd->setUniformVec2("u_TexelSize", Vec2(1.0f / static_cast<float>(width),
                                            1.0f / static_cast<float>(height)));
    cmd->setUniformFloat("u_Time", chain.time);

    for (const CameraEffectUniform& u : step.uniforms) {
        applyUniform(cmd.get(), u);
    }

    bindInputTexture(cmd.get(), "u_SceneTex", srcColor, 0, step.pointSampleScene);
    if (step.needsDepth) {
        TextureHandle depth = srcDepth.isValid() ? srcDepth : srcColor;
        bindInputTexture(cmd.get(), "u_DepthTex", depth, 1, true);
    }

    drawFullscreen(cmd.get());
    cmd->endDebugMarker();
    m_device->submit(std::move(cmd));
}

void CameraEffectStack::execute(RenderTargetHandle finalTarget,
                                TextureHandle sceneColor,
                                TextureHandle sceneDepth,
                                uint32_t width,
                                uint32_t height,
                                const CameraEffectChain& chain) {
    if (!m_initialized || !m_device || !sceneColor.isValid() || width == 0 || height == 0) {
        return;
    }

    std::vector<RenderStep> steps;
    buildSteps(chain, steps);
    if (steps.empty()) {
        return;
    }

    // Intermediate ping-pong targets are only needed when more than one pass runs.
    if (steps.size() >= 2) {
        ensureTargets(width, height);
        if (!m_pingA.valid() || !m_pingB.valid()) {
            return;
        }
    }

    TextureHandle src = sceneColor;
    const size_t count = steps.size();
    for (size_t i = 0; i < count; ++i) {
        const bool toFinal = (i + 1 == count);
        RenderTargetHandle dst;
        if (toFinal) {
            dst = finalTarget;
        } else {
            dst = (i % 2 == 0) ? m_pingA.target : m_pingB.target;
        }
        if (!dst.isValid()) {
            return;
        }
        runStep(steps[i], dst, src, sceneDepth, width, height, chain, toFinal);
        if (!toFinal) {
            src = (i % 2 == 0) ? m_pingA.color : m_pingB.color;
        }
    }
}

} // namespace lupine

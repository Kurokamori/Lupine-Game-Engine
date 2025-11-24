#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/gfx/GfxDeviceFactory.hpp"
#include "lupine/rendering/debug/DebugRendererOpenGL.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/DefaultShaders.hpp"
#include "lupine/rendering/PBRMaterial.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/components/DirectionalLight3D.hpp"
#include "lupine/components/OmniLight3D.hpp"
#include "lupine/components/SpotLight3D.hpp"
#include "lupine/components/WorldEnvironment.hpp"
#include "lupine/math/Camera.hpp"
#include "lupine/logger/Logger.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace lupine {

RenderWorld::RenderWorld()
    : m_device(nullptr)
    , m_nextViewID(1)
    , m_nextMaterialID(1)
    , m_inFrame(false)
    , m_frameNumber(0)
{
}

RenderWorld::~RenderWorld() {
    shutdown();
}

bool RenderWorld::initialize(GraphicsBackend backend) {

    m_device = GfxDeviceFactory::create(backend);
    if (!m_device) {

        return false;
    }

    if (!m_device->initialize()) {

        m_device.reset();
        return false;
    }

    m_device->setDefaultTextureFiltering(m_textureMinFilter, m_textureMagFilter);

    return true;
}

bool RenderWorld::initialize(std::unique_ptr<IGfxDevice> device) {
    if (!device) {

        return false;
    }

    m_device = std::move(device);

    return true;
}

void RenderWorld::shutdown() {
    if (!m_device) {
        return;
    }

    m_renderViews.clear();

    if (m_debugRenderer) {
        m_debugRenderer->shutdown();
        m_debugRenderer.reset();
    }

    m_defaultColoredMaterial = MaterialHandle();
    m_defaultTexturedMaterial = MaterialHandle();
    m_defaultWireframeMaterial = MaterialHandle();
    m_defaultLineMaterial = MaterialHandle();
    m_defaultTextMaterial = MaterialHandle();
    m_defaultPBRMaterial = MaterialHandle();

    m_materials.clear();

    if (m_lightUniformBuffer.isValid()) {
        m_device->destroyUniformBuffer(m_lightUniformBuffer);
        m_lightUniformBuffer = UniformBufferHandle();
    }

    for (auto& shadowMap : m_shadowMaps) {
        if (shadowMap.isValid()) {
            m_device->destroyTexture(shadowMap);
        }
    }
    m_shadowMaps.clear();

    for (auto& shadowFB : m_shadowMapFramebuffers) {
        if (shadowFB.isValid()) {
            m_device->destroyRenderTarget(shadowFB);
        }
    }
    m_shadowMapFramebuffers.clear();

    m_device->shutdown();
    m_device.reset();

}

GraphicsBackend RenderWorld::getBackend() const {
    return m_device ? m_device->getBackend() : GraphicsBackend::None;
}

bool RenderWorld::ensureRenderingResourcesInitialized() {
    if (m_renderingResourcesInitialized) {
        return true;
    }

    if (!createDefaultMaterials()) {

        return false;
    }

    GraphicsBackend backend = getBackend();
    if (backend == GraphicsBackend::OpenGL) {
        m_debugRenderer = std::make_unique<DebugRendererOpenGL>();
        if (!m_debugRenderer->initialize(m_device.get())) {

            m_debugRenderer.reset();
        } else {

        }
    }

    m_renderingResourcesInitialized = true;

    return true;
}

RenderViewID RenderWorld::createRenderView(std::unique_ptr<RenderCamera> camera) {

    if (!ensureRenderingResourcesInitialized()) {

        return 0;
    }

    if (camera) {
        camera->backend = getBackend();
    }

    RenderViewID id = m_nextViewID++;
    auto view = std::make_unique<RenderView>(id, std::move(camera));
    m_renderViews[id] = std::move(view);

    return id;
}

void RenderWorld::destroyRenderView(RenderViewID viewID) {
    auto it = m_renderViews.find(viewID);
    if (it != m_renderViews.end()) {
        m_renderViews.erase(it);

    }
}

RenderView* RenderWorld::getRenderView(RenderViewID viewID) {
    auto it = m_renderViews.find(viewID);
    return (it != m_renderViews.end()) ? it->second.get() : nullptr;
}

std::vector<RenderView*> RenderWorld::getAllRenderViews() {
    std::vector<RenderView*> views;
    views.reserve(m_renderViews.size());
    for (auto& pair : m_renderViews) {
        views.push_back(pair.second.get());
    }
    return views;
}

void RenderWorld::beginFrame() {

    if (!m_inFrame) {
        m_inFrame = true;
        m_frameNumber++;

        m_stats = RenderStats();
        m_stats.renderViews = static_cast<uint32_t>(m_renderViews.size());
    }
}

void RenderWorld::renderAllViews() {

    for (auto& pair : m_renderViews) {
        renderView(pair.first);
    }
}

void RenderWorld::renderView(RenderViewID viewID) {

    RenderView* view = getRenderView(viewID);
    if (!view) {

        return;
    }

    if (!view->hasSwapchain() && !view->hasRenderTarget()) {
        return;
    }

    if (!m_device) {
        return;
    }

    RenderTargetHandle target;
    if (view->hasSwapchain()) {
        target = m_device->getSwapchainBackbuffer(view->getSwapchain());
    } else if (view->hasRenderTarget()) {
        target = view->getRenderTarget();
    } else {
        return;
    }

    auto tempCmd = m_device->beginFrame(target);
    if (!tempCmd) {

        return;
    }

    const Viewport& vp = view->getViewport();
    tempCmd->setViewport(vp);
    tempCmd->setRenderTarget(target);

    m_device->submit(std::move(tempCmd));

    RenderContext ctx(view, m_device.get());

    MaterialHandle texturedMaterialForView = m_defaultTexturedMaterial;
    if (RenderCamera* camera = view->getCamera()) {
        switch (camera->getType()) {
            case CameraType::Camera2D:
            case CameraType::CameraCanvas:
                if (m_defaultTextured2DMaterial.isValid()) {
                    texturedMaterialForView = m_defaultTextured2DMaterial;
                }
                break;
            case CameraType::Camera3D:
            default:
                break;
        }
    }

    ctx.setDefaultMaterials(
        m_defaultColoredMaterial,
        texturedMaterialForView,
        m_defaultWireframeMaterial,
        m_defaultLineMaterial,
        m_defaultTextMaterial
    );
    ctx.setDefaultColoredDoubleSidedMaterial(m_defaultColoredDoubleSidedMaterial);
    ctx.setDefaultTexturedDoubleSidedMaterial(m_defaultTexturedDoubleSidedMaterial);
    ctx.setDefaultColored2DMaterial(m_defaultColored2DMaterial);
    ctx.setDefaultLine2DMaterial(m_defaultLine2DMaterial);
    ctx.setDefaultRoundedRectMaterial(m_defaultRoundedRectMaterial);
    ctx.setRoundedRectAdditiveMaterial(m_roundedRectAdditiveMaterial);
    ctx.setRoundedRectMultiplyMaterial(m_roundedRectMultiplyMaterial);
    ctx.setRoundedRectOpaqueMaterial(m_roundedRectOpaqueMaterial);
    ctx.setRoundedRectOverlayMaterial(m_roundedRectOverlayMaterial);
    ctx.setRoundedRectBorderMaterial(m_roundedRectBorderMaterial);
    ctx.setRoundedRect3DMaterial(m_roundedRect3DMaterial);
    ctx.setRoundedRect3DBorderMaterial(m_roundedRect3DBorderMaterial);
    ctx.setDefaultPBRMaterial(m_defaultPBRMaterial);
    ctx.setDefaultSkeletalMaterial(m_defaultSkeletalMaterial);
    ctx.setDefaultText3DMaterial(m_defaultText3DMaterial);

    ctx.updateCameraMatrices();

    ctx.clear();

    gatherLights(view);

    gatherWorldEnvironment(view);

    gatherRenderables(view, ctx);

    m_renderPasses.clear();
    buildRenderBatches(ctx.getDrawItems(), m_renderPasses);

    renderShadowMaps(view, ctx);

    m_device->updateUniformBuffer(m_lightUniformBuffer, view->m_lightUniformData, sizeof(LightUniformBuffer));

    executeRenderPasses(view, m_renderPasses, ctx);

}

void RenderWorld::endFrame(bool presentAll) {
    if (!m_inFrame) {

        return;
    }

    if (presentAll) {
        for (auto& pair : m_renderViews) {
            RenderView* view = pair.second.get();
            if (view->hasSwapchain()) {
                m_device->present(view->getSwapchain());
            }
        }
    }

    m_inFrame = false;
}

void RenderWorld::presentView(RenderViewID viewID) {
    if (!m_device) {
        return;
    }

    RenderView* view = getRenderView(viewID);
    if (!view) {

        return;
    }

    if (view->hasSwapchain()) {
        m_device->present(view->getSwapchain());
    }
}

MaterialHandle RenderWorld::createMaterial(const Material& material) {
    uint32_t id = m_nextMaterialID++;
    m_materials[id] = material;
    return MaterialHandle(id);
}

const Material* RenderWorld::getMaterial(MaterialHandle handle) const {
    auto it = m_materials.find(handle.id);
    return (it != m_materials.end()) ? &it->second : nullptr;
}

void RenderWorld::updateMaterial(MaterialHandle handle, const Material& material) {
    auto it = m_materials.find(handle.id);
    if (it != m_materials.end()) {
        it->second = material;
    }
}

void RenderWorld::destroyMaterial(MaterialHandle handle) {
    m_materials.erase(handle.id);
}

bool RenderWorld::createDefaultMaterials() {

    auto createStandardVertexLayout = []() -> VertexBufferLayout {
        VertexBufferLayout layout;
        layout.stride = sizeof(Vertex);

        layout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0});
        layout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0});
        layout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0});
        layout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0});

        return layout;
    };

    auto createSpriteVertexLayout = []() -> VertexBufferLayout {
        VertexBufferLayout layout;
        layout.stride = sizeof(Vertex);

        layout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0});
        layout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0});
        layout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0});

        return layout;
    };

    {

        ShaderDesc vertDesc;
        vertDesc.stage = ShaderStage::Vertex;
        vertDesc.bytecode = DefaultShaders::Unlit_Vertex();
        vertDesc.bytecodeSize = std::strlen(DefaultShaders::Unlit_Vertex());
        ShaderHandle vertShader = m_device->createShader(vertDesc);
        if (!vertShader.isValid()) {

            return false;
        }

        ShaderDesc fragDesc;
        fragDesc.stage = ShaderStage::Fragment;
        fragDesc.bytecode = DefaultShaders::Unlit_Fragment();
        fragDesc.bytecodeSize = std::strlen(DefaultShaders::Unlit_Fragment());
        ShaderHandle fragShader = m_device->createShader(fragDesc);
        if (!fragShader.isValid()) {

            m_device->destroyShader(vertShader);
            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {vertShader, fragShader};
        pipelineDesc.vertexLayout = createStandardVertexLayout();
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::opaque();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::Less;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {

            m_device->destroyShader(vertShader);
            m_device->destroyShader(fragShader);
            return false;
        }

        Material coloredMat;
        coloredMat.name = "DefaultColored";
        coloredMat.vertexShader = vertShader;
        coloredMat.fragmentShader = fragShader;
        coloredMat.pipeline = pipeline;
        coloredMat.renderLayer = RenderLayer::Opaque;
        coloredMat.isTransparent = false;

        m_defaultColoredMaterial = createMaterial(coloredMat);

        PipelineDesc pipelineDesc2D = pipelineDesc;
        pipelineDesc2D.depthStencilState.depthTestEnable = false;
        pipelineDesc2D.depthStencilState.depthWriteEnable = false;
        pipelineDesc2D.rasterizerState.cullMode = CullMode::None;

        PipelineHandle pipeline2D = m_device->createPipeline(pipelineDesc2D);
        if (!pipeline2D.isValid()) {

            return false;
        }

        Material colored2DMat;
        colored2DMat.name = "DefaultColored2D";
        colored2DMat.vertexShader = vertShader;
        colored2DMat.fragmentShader = fragShader;
        colored2DMat.pipeline = pipeline2D;
        colored2DMat.renderLayer = RenderLayer::Transparent;
        colored2DMat.isTransparent = true;

        m_defaultColored2DMaterial = createMaterial(colored2DMat);

        PipelineDesc pipelineDescDoubleSided = pipelineDesc;
        pipelineDescDoubleSided.rasterizerState.cullMode = CullMode::None;

        PipelineHandle pipelineDoubleSided = m_device->createPipeline(pipelineDescDoubleSided);
        if (!pipelineDoubleSided.isValid()) {

        } else {
            Material coloredDoubleSidedMat;
            coloredDoubleSidedMat.name = "DefaultColoredDoubleSided";
            coloredDoubleSidedMat.vertexShader = vertShader;
            coloredDoubleSidedMat.fragmentShader = fragShader;
            coloredDoubleSidedMat.pipeline = pipelineDoubleSided;
            coloredDoubleSidedMat.renderLayer = RenderLayer::Opaque;
            coloredDoubleSidedMat.isTransparent = false;

            m_defaultColoredDoubleSidedMaterial = createMaterial(coloredDoubleSidedMat);

        }
    }

    {

        const Material* coloredMat = getMaterial(m_defaultColoredMaterial);
        if (!coloredMat) {

            return false;
        }

        PipelineDesc pipelineDesc3D;
        pipelineDesc3D.shaders = {coloredMat->vertexShader, coloredMat->fragmentShader};
        pipelineDesc3D.vertexLayout = createSpriteVertexLayout();
        pipelineDesc3D.topology = PrimitiveTopology::TriangleList;
        pipelineDesc3D.blendState = BlendState::opaque();
        pipelineDesc3D.depthStencilState.depthTestEnable = true;
        pipelineDesc3D.depthStencilState.depthWriteEnable = true;
        pipelineDesc3D.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc3D.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc3D.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline3D = m_device->createPipeline(pipelineDesc3D);
        if (!pipeline3D.isValid()) {

            return false;
        }

        Material textured3DMat;
        textured3DMat.name = "DefaultTextured";
        textured3DMat.vertexShader = coloredMat->vertexShader;
        textured3DMat.fragmentShader = coloredMat->fragmentShader;
        textured3DMat.pipeline = pipeline3D;
        textured3DMat.renderLayer = RenderLayer::Opaque;
        textured3DMat.isTransparent = true;

        m_defaultTexturedMaterial = createMaterial(textured3DMat);

        PipelineDesc pipelineDesc2D;
        pipelineDesc2D.shaders = {coloredMat->vertexShader, coloredMat->fragmentShader};
        pipelineDesc2D.vertexLayout = createSpriteVertexLayout();
        pipelineDesc2D.topology = PrimitiveTopology::TriangleList;
        pipelineDesc2D.blendState = BlendState::alphaBlend();
        pipelineDesc2D.depthStencilState = DepthStencilState::noDepth();
        pipelineDesc2D.rasterizerState = RasterizerState::noCull();

        PipelineHandle pipeline2D = m_device->createPipeline(pipelineDesc2D);
        if (!pipeline2D.isValid()) {

            return false;
        }

        Material textured2DMat;
        textured2DMat.name = "DefaultTextured2D";
        textured2DMat.vertexShader = coloredMat->vertexShader;
        textured2DMat.fragmentShader = coloredMat->fragmentShader;
        textured2DMat.pipeline = pipeline2D;
        textured2DMat.renderLayer = RenderLayer::Opaque;
        textured2DMat.isTransparent = true;

        m_defaultTextured2DMaterial = createMaterial(textured2DMat);

        PipelineDesc pipelineDescDoubleSided;
        pipelineDescDoubleSided.shaders = {coloredMat->vertexShader, coloredMat->fragmentShader};
        pipelineDescDoubleSided.vertexLayout = createSpriteVertexLayout();
        pipelineDescDoubleSided.topology = PrimitiveTopology::TriangleList;
        pipelineDescDoubleSided.blendState = BlendState::opaque();
        pipelineDescDoubleSided.depthStencilState.depthTestEnable = true;
        pipelineDescDoubleSided.depthStencilState.depthWriteEnable = true;
        pipelineDescDoubleSided.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDescDoubleSided.rasterizerState.cullMode = CullMode::None;
        pipelineDescDoubleSided.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipelineDoubleSided = m_device->createPipeline(pipelineDescDoubleSided);
        if (!pipelineDoubleSided.isValid()) {

            return false;
        }

        Material texturedDoubleSidedMat;
        texturedDoubleSidedMat.name = "DefaultTexturedDoubleSided";
        texturedDoubleSidedMat.vertexShader = coloredMat->vertexShader;
        texturedDoubleSidedMat.fragmentShader = coloredMat->fragmentShader;
        texturedDoubleSidedMat.pipeline = pipelineDoubleSided;
        texturedDoubleSidedMat.renderLayer = RenderLayer::Opaque;
        texturedDoubleSidedMat.isTransparent = true;

        m_defaultTexturedDoubleSidedMaterial = createMaterial(texturedDoubleSidedMat);

    }

    {

        ShaderDesc vertDesc;
        vertDesc.stage = ShaderStage::Vertex;
        vertDesc.bytecode = DefaultShaders::Wireframe_Vertex();
        vertDesc.bytecodeSize = std::strlen(DefaultShaders::Wireframe_Vertex());
        ShaderHandle vertShader = m_device->createShader(vertDesc);
        if (!vertShader.isValid()) {

            return false;
        }

        ShaderDesc fragDesc;
        fragDesc.stage = ShaderStage::Fragment;
        fragDesc.bytecode = DefaultShaders::Wireframe_Fragment();
        fragDesc.bytecodeSize = std::strlen(DefaultShaders::Wireframe_Fragment());
        ShaderHandle fragShader = m_device->createShader(fragDesc);
        if (!fragShader.isValid()) {

            m_device->destroyShader(vertShader);
            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {vertShader, fragShader};
        pipelineDesc.vertexLayout = createStandardVertexLayout();
        pipelineDesc.topology = PrimitiveTopology::LineList;
        pipelineDesc.blendState = BlendState::opaque();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::Less;
        pipelineDesc.rasterizerState.cullMode = CullMode::None;
        pipelineDesc.rasterizerState.fillMode = FillMode::Wireframe;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {

            m_device->destroyShader(vertShader);
            m_device->destroyShader(fragShader);
            return false;
        }

        Material wireframeMat;
        wireframeMat.name = "DefaultWireframe";
        wireframeMat.vertexShader = vertShader;
        wireframeMat.fragmentShader = fragShader;
        wireframeMat.pipeline = pipeline;
        wireframeMat.renderLayer = RenderLayer::Opaque;
        wireframeMat.isTransparent = false;

        m_defaultWireframeMaterial = createMaterial(wireframeMat);

    }

    {

        ShaderDesc vertDesc;
        vertDesc.stage = ShaderStage::Vertex;
        vertDesc.bytecode = DefaultShaders::Line_Vertex();
        vertDesc.bytecodeSize = std::strlen(DefaultShaders::Line_Vertex());
        ShaderHandle vertShader = m_device->createShader(vertDesc);
        if (!vertShader.isValid()) {

            return false;
        }

        ShaderDesc fragDesc;
        fragDesc.stage = ShaderStage::Fragment;
        fragDesc.bytecode = DefaultShaders::Line_Fragment();
        fragDesc.bytecodeSize = std::strlen(DefaultShaders::Line_Fragment());
        ShaderHandle fragShader = m_device->createShader(fragDesc);
        if (!fragShader.isValid()) {

            m_device->destroyShader(vertShader);
            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {vertShader, fragShader};
        pipelineDesc.vertexLayout = createStandardVertexLayout();
        pipelineDesc.topology = PrimitiveTopology::LineList;
        pipelineDesc.blendState = BlendState::opaque();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = false;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::Less;
        pipelineDesc.rasterizerState.cullMode = CullMode::None;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {

            m_device->destroyShader(vertShader);
            m_device->destroyShader(fragShader);
            return false;
        }

        Material lineMat;
        lineMat.name = "DefaultLine";
        lineMat.vertexShader = vertShader;
        lineMat.fragmentShader = fragShader;
        lineMat.pipeline = pipeline;
        lineMat.renderLayer = RenderLayer::Overlay;
        lineMat.isTransparent = false;

        m_defaultLineMaterial = createMaterial(lineMat);

        PipelineDesc pipelineDesc2D = pipelineDesc;
        pipelineDesc2D.depthStencilState.depthTestEnable = false;
        pipelineDesc2D.depthStencilState.depthWriteEnable = false;
        pipelineDesc2D.blendState = BlendState::alphaBlend();

        PipelineHandle pipeline2D = m_device->createPipeline(pipelineDesc2D);
        if (!pipeline2D.isValid()) {

            return false;
        }

        Material line2DMat;
        line2DMat.name = "DefaultLine2D";
        line2DMat.vertexShader = vertShader;
        line2DMat.fragmentShader = fragShader;
        line2DMat.pipeline = pipeline2D;
        line2DMat.renderLayer = RenderLayer::Transparent;
        line2DMat.isTransparent = true;

        m_defaultLine2DMaterial = createMaterial(line2DMat);

    }

    {

        ShaderDesc vertDesc;
        vertDesc.stage = ShaderStage::Vertex;
        vertDesc.bytecode = DefaultShaders::Text_Vertex();
        vertDesc.bytecodeSize = std::strlen(DefaultShaders::Text_Vertex());
        ShaderHandle vertShader = m_device->createShader(vertDesc);
        if (!vertShader.isValid()) {

            return false;
        }

        ShaderDesc fragDesc;
        fragDesc.stage = ShaderStage::Fragment;
        fragDesc.bytecode = DefaultShaders::Text_Fragment();
        fragDesc.bytecodeSize = std::strlen(DefaultShaders::Text_Fragment());
        ShaderHandle fragShader = m_device->createShader(fragDesc);
        if (!fragShader.isValid()) {

            m_device->destroyShader(vertShader);
            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {vertShader, fragShader};
        pipelineDesc.vertexLayout = createStandardVertexLayout();
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::alphaBlend();
        pipelineDesc.depthStencilState = DepthStencilState::noDepth();
        pipelineDesc.rasterizerState.cullMode = CullMode::None;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {

            m_device->destroyShader(vertShader);
            m_device->destroyShader(fragShader);
            return false;
        }

        Material textMat;
        textMat.name = "DefaultText";
        textMat.vertexShader = vertShader;
        textMat.fragmentShader = fragShader;
        textMat.pipeline = pipeline;
        textMat.renderLayer = RenderLayer::UI;
        textMat.isTransparent = true;

        m_defaultTextMaterial = createMaterial(textMat);

    }

    {

        ShaderDesc vertDesc;
        vertDesc.stage = ShaderStage::Vertex;
        vertDesc.bytecode = DefaultShaders::Text3D_Vertex();
        vertDesc.bytecodeSize = std::strlen(DefaultShaders::Text3D_Vertex());
        ShaderHandle vertShader = m_device->createShader(vertDesc);
        if (!vertShader.isValid()) {
            return false;
        }

        ShaderDesc fragDesc;
        fragDesc.stage = ShaderStage::Fragment;
        fragDesc.bytecode = DefaultShaders::Text3D_Fragment();
        fragDesc.bytecodeSize = std::strlen(DefaultShaders::Text3D_Fragment());
        ShaderHandle fragShader = m_device->createShader(fragDesc);
        if (!fragShader.isValid()) {
            m_device->destroyShader(vertShader);
            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {vertShader, fragShader};
        pipelineDesc.vertexLayout = createStandardVertexLayout();
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::alphaBlend();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.rasterizerState.cullMode = CullMode::None;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            m_device->destroyShader(vertShader);
            m_device->destroyShader(fragShader);
            return false;
        }

        Material text3dMat;
        text3dMat.name = "DefaultText3D";
        text3dMat.vertexShader = vertShader;
        text3dMat.fragmentShader = fragShader;
        text3dMat.pipeline = pipeline;
        text3dMat.renderLayer = RenderLayer::Transparent;
        text3dMat.isTransparent = true;

        m_defaultText3DMaterial = createMaterial(text3dMat);
    }

    {

        ShaderDesc vertDesc;
        vertDesc.stage = ShaderStage::Vertex;
        vertDesc.bytecode = DefaultShaders::RoundedRect_Vertex();
        vertDesc.bytecodeSize = std::strlen(DefaultShaders::RoundedRect_Vertex());
        ShaderHandle vertShader = m_device->createShader(vertDesc);
        if (!vertShader.isValid()) {

            return false;
        }

        ShaderDesc fragDesc;
        fragDesc.stage = ShaderStage::Fragment;
        fragDesc.bytecode = DefaultShaders::RoundedRect_Fragment();
        fragDesc.bytecodeSize = std::strlen(DefaultShaders::RoundedRect_Fragment());
        ShaderHandle fragShader = m_device->createShader(fragDesc);
        if (!fragShader.isValid()) {

            m_device->destroyShader(vertShader);
            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {vertShader, fragShader};
        pipelineDesc.vertexLayout = createStandardVertexLayout();
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::alphaBlend();
        pipelineDesc.depthStencilState = DepthStencilState::noDepth();
        pipelineDesc.rasterizerState.cullMode = CullMode::None;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {

            m_device->destroyShader(vertShader);
            m_device->destroyShader(fragShader);
            return false;
        }

        Material roundedRectMat;
        roundedRectMat.name = "DefaultRoundedRect";
        roundedRectMat.vertexShader = vertShader;
        roundedRectMat.fragmentShader = fragShader;
        roundedRectMat.pipeline = pipeline;
        roundedRectMat.renderLayer = RenderLayer::UI;
        roundedRectMat.isTransparent = true;

        m_defaultRoundedRectMaterial = createMaterial(roundedRectMat);

    }

    {

        const Material* defaultMat = getMaterial(m_defaultRoundedRectMaterial);
        if (!defaultMat) {

            return false;
        }

        {
            PipelineDesc pipelineDesc;
            pipelineDesc.shaders = {defaultMat->vertexShader, defaultMat->fragmentShader};
            pipelineDesc.vertexLayout = createStandardVertexLayout();
            pipelineDesc.topology = PrimitiveTopology::TriangleList;
            pipelineDesc.blendState = BlendState::additive();
            pipelineDesc.depthStencilState = DepthStencilState::noDepth();
            pipelineDesc.rasterizerState.cullMode = CullMode::None;
            pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

            PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
            if (!pipeline.isValid()) {

                return false;
            }

            Material additiveMat;
            additiveMat.name = "RoundedRectAdditive";
            additiveMat.vertexShader = defaultMat->vertexShader;
            additiveMat.fragmentShader = defaultMat->fragmentShader;
            additiveMat.pipeline = pipeline;
            additiveMat.renderLayer = RenderLayer::UI;
            additiveMat.isTransparent = true;

            m_roundedRectAdditiveMaterial = createMaterial(additiveMat);
        }

        {
            PipelineDesc pipelineDesc;
            pipelineDesc.shaders = {defaultMat->vertexShader, defaultMat->fragmentShader};
            pipelineDesc.vertexLayout = createStandardVertexLayout();
            pipelineDesc.topology = PrimitiveTopology::TriangleList;
            pipelineDesc.blendState = BlendState::multiply();
            pipelineDesc.depthStencilState = DepthStencilState::noDepth();
            pipelineDesc.rasterizerState.cullMode = CullMode::None;
            pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

            PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
            if (!pipeline.isValid()) {

                return false;
            }

            Material multiplyMat;
            multiplyMat.name = "RoundedRectMultiply";
            multiplyMat.vertexShader = defaultMat->vertexShader;
            multiplyMat.fragmentShader = defaultMat->fragmentShader;
            multiplyMat.pipeline = pipeline;
            multiplyMat.renderLayer = RenderLayer::UI;
            multiplyMat.isTransparent = true;

            m_roundedRectMultiplyMaterial = createMaterial(multiplyMat);
        }

        {
            PipelineDesc pipelineDesc;
            pipelineDesc.shaders = {defaultMat->vertexShader, defaultMat->fragmentShader};
            pipelineDesc.vertexLayout = createStandardVertexLayout();
            pipelineDesc.topology = PrimitiveTopology::TriangleList;
            pipelineDesc.blendState = BlendState::opaque();
            pipelineDesc.depthStencilState = DepthStencilState::noDepth();
            pipelineDesc.rasterizerState.cullMode = CullMode::None;
            pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

            PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
            if (!pipeline.isValid()) {

                return false;
            }

            Material opaqueMat;
            opaqueMat.name = "RoundedRectOpaque";
            opaqueMat.vertexShader = defaultMat->vertexShader;
            opaqueMat.fragmentShader = defaultMat->fragmentShader;
            opaqueMat.pipeline = pipeline;
            opaqueMat.renderLayer = RenderLayer::Opaque;
            opaqueMat.isTransparent = false;

            m_roundedRectOpaqueMaterial = createMaterial(opaqueMat);
        }

        {
            PipelineDesc pipelineDesc;
            pipelineDesc.shaders = {defaultMat->vertexShader, defaultMat->fragmentShader};
            pipelineDesc.vertexLayout = createStandardVertexLayout();
            pipelineDesc.topology = PrimitiveTopology::TriangleList;
            pipelineDesc.blendState = BlendState::overlay();
            pipelineDesc.depthStencilState = DepthStencilState::noDepth();
            pipelineDesc.rasterizerState.cullMode = CullMode::None;
            pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

            PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
            if (!pipeline.isValid()) {

                return false;
            }

            Material overlayMat;
            overlayMat.name = "RoundedRectOverlay";
            overlayMat.vertexShader = defaultMat->vertexShader;
            overlayMat.fragmentShader = defaultMat->fragmentShader;
            overlayMat.pipeline = pipeline;
            overlayMat.renderLayer = RenderLayer::UI;
            overlayMat.isTransparent = true;

            m_roundedRectOverlayMaterial = createMaterial(overlayMat);
        }

    }

    {

        ShaderDesc vertDesc;
        vertDesc.stage = ShaderStage::Vertex;
        vertDesc.bytecode = DefaultShaders::RoundedRectBorder_Vertex();
        vertDesc.bytecodeSize = std::strlen(DefaultShaders::RoundedRectBorder_Vertex());
        ShaderHandle vertShader = m_device->createShader(vertDesc);
        if (!vertShader.isValid()) {

            return false;
        }

        ShaderDesc fragDesc;
        fragDesc.stage = ShaderStage::Fragment;
        fragDesc.bytecode = DefaultShaders::RoundedRectBorder_Fragment();
        fragDesc.bytecodeSize = std::strlen(DefaultShaders::RoundedRectBorder_Fragment());
        ShaderHandle fragShader = m_device->createShader(fragDesc);
        if (!fragShader.isValid()) {

            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {vertShader, fragShader};
        pipelineDesc.vertexLayout = createStandardVertexLayout();
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::alphaBlend();
        pipelineDesc.depthStencilState = DepthStencilState::noDepth();
        pipelineDesc.rasterizerState.cullMode = CullMode::None;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {

            return false;
        }

        Material borderMat;
        borderMat.name = "RoundedRectBorder";
        borderMat.vertexShader = vertShader;
        borderMat.fragmentShader = fragShader;
        borderMat.pipeline = pipeline;
        borderMat.renderLayer = RenderLayer::UI;
        borderMat.isTransparent = true;

        m_roundedRectBorderMaterial = createMaterial(borderMat);

    }

    {

        const Material* defaultRoundedRectMat = getMaterial(m_defaultRoundedRectMaterial);
        if (!defaultRoundedRectMat) {

            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {defaultRoundedRectMat->vertexShader, defaultRoundedRectMat->fragmentShader};
        pipelineDesc.vertexLayout = createStandardVertexLayout();
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::alphaBlend();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.rasterizerState.cullMode = CullMode::None;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {

            return false;
        }

        Material roundedRect3DMat;
        roundedRect3DMat.name = "RoundedRect3D";
        roundedRect3DMat.vertexShader = defaultRoundedRectMat->vertexShader;
        roundedRect3DMat.fragmentShader = defaultRoundedRectMat->fragmentShader;
        roundedRect3DMat.pipeline = pipeline;
        roundedRect3DMat.renderLayer = RenderLayer::Transparent;
        roundedRect3DMat.isTransparent = true;

        m_roundedRect3DMaterial = createMaterial(roundedRect3DMat);

    }

    {

        const Material* borderMat = getMaterial(m_roundedRectBorderMaterial);
        if (!borderMat) {

            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {borderMat->vertexShader, borderMat->fragmentShader};
        pipelineDesc.vertexLayout = createStandardVertexLayout();
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::alphaBlend();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.rasterizerState.cullMode = CullMode::None;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {

            return false;
        }

        Material roundedRect3DBorderMat;
        roundedRect3DBorderMat.name = "RoundedRect3DBorder";
        roundedRect3DBorderMat.vertexShader = borderMat->vertexShader;
        roundedRect3DBorderMat.fragmentShader = borderMat->fragmentShader;
        roundedRect3DBorderMat.pipeline = pipeline;
        roundedRect3DBorderMat.renderLayer = RenderLayer::Transparent;
        roundedRect3DBorderMat.isTransparent = true;

        m_roundedRect3DBorderMaterial = createMaterial(roundedRect3DBorderMat);

    }

    {

        ShaderDesc vertDesc;
        vertDesc.stage = ShaderStage::Vertex;
        vertDesc.bytecode = DefaultShaders::PBR_Vertex();
        vertDesc.bytecodeSize = std::strlen(DefaultShaders::PBR_Vertex());
        ShaderHandle vertShader = m_device->createShader(vertDesc);
        if (!vertShader.isValid()) {

            return false;
        }

        ShaderDesc fragDesc;
        fragDesc.stage = ShaderStage::Fragment;
        fragDesc.bytecode = DefaultShaders::PBR_Fragment();
        fragDesc.bytecodeSize = std::strlen(DefaultShaders::PBR_Fragment());
        ShaderHandle fragShader = m_device->createShader(fragDesc);
        if (!fragShader.isValid()) {

            m_device->destroyShader(vertShader);
            return false;
        }

        VertexBufferLayout pbrVertexLayout;
        pbrVertexLayout.stride = sizeof(Vertex);
        pbrVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0});
        pbrVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0});
        pbrVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0});
        pbrVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {vertShader, fragShader};
        pipelineDesc.vertexLayout = pbrVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::opaque();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {

            m_device->destroyShader(vertShader);
            m_device->destroyShader(fragShader);
            return false;
        }

        Material pbrMat;
        pbrMat.name = "DefaultPBR";
        pbrMat.vertexShader = vertShader;
        pbrMat.fragmentShader = fragShader;
        pbrMat.pipeline = pipeline;
        pbrMat.renderLayer = RenderLayer::Opaque;
        pbrMat.isTransparent = false;

        m_defaultPBRMaterial = createMaterial(pbrMat);

    }

    {
        LOG_INFO(LogCategory::Render, "Initializing skeletal material...");

        ShaderDesc vertDesc;
        vertDesc.stage = ShaderStage::Vertex;
        vertDesc.bytecode = DefaultShaders::Skeletal_Vertex();
        vertDesc.bytecodeSize = std::strlen(DefaultShaders::Skeletal_Vertex());
        ShaderHandle vertShader = m_device->createShader(vertDesc);
        if (!vertShader.isValid()) {
            LOG_ERROR(LogCategory::Render, "Failed to create skeletal vertex shader");
            return false;
        }

        ShaderDesc fragDesc;
        fragDesc.stage = ShaderStage::Fragment;
        fragDesc.bytecode = DefaultShaders::Skeletal_Fragment();
        fragDesc.bytecodeSize = std::strlen(DefaultShaders::Skeletal_Fragment());
        ShaderHandle fragShader = m_device->createShader(fragDesc);
        if (!fragShader.isValid()) {
            LOG_ERROR(LogCategory::Render, "Failed to create skeletal fragment shader");
            m_device->destroyShader(vertShader);
            return false;
        }

        VertexBufferLayout skeletalVertexLayout;
        skeletalVertexLayout.stride = sizeof(Vertex);
        skeletalVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0});
        skeletalVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0});
        skeletalVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0});
        skeletalVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0});
        skeletalVertexLayout.attributes.push_back({"a_Tangent", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent)), 0});
        skeletalVertexLayout.attributes.push_back({"a_BoneIDs", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneIDs)), 0});
        skeletalVertexLayout.attributes.push_back({"a_BoneWeights", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneWeights)), 0});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {vertShader, fragShader};
        pipelineDesc.vertexLayout = skeletalVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::opaque();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            LOG_ERROR(LogCategory::Render, "Failed to create skeletal pipeline");
            m_device->destroyShader(vertShader);
            m_device->destroyShader(fragShader);
            return false;
        }

        Material skeletalMat;
        skeletalMat.name = "DefaultSkeletal";
        skeletalMat.vertexShader = vertShader;
        skeletalMat.fragmentShader = fragShader;
        skeletalMat.pipeline = pipeline;
        skeletalMat.renderLayer = RenderLayer::Opaque;
        skeletalMat.isTransparent = false;

        m_defaultSkeletalMaterial = createMaterial(skeletalMat);
        LOG_INFO(LogCategory::Render, "Skeletal material initialized successfully");

    }

    return true;
}

void RenderWorld::gatherRenderables(RenderView* view, RenderContext& ctx) {
    if (!view->getScene()) {

        return;
    }

    using namespace core;
    Scene* scene = view->getScene();
    auto root = scene->GetRoot();
    if (!root) {

        return;
    }

    DebugDraw::SetCurrentRenderView(view);

    RenderCamera* camera = view->getCamera();
    CameraType cameraType = camera ? camera->getType() : CameraType::Camera3D;

    DebugDraw::SetCurrentCameraType(cameraType);

    std::function<void(std::shared_ptr<Node>)> gatherFromNode = [&](std::shared_ptr<Node> node) {
        if (!node) {
            return;
        }

        // Log skeletal mesh nodes specifically
        auto components = node->GetComponents();
        for (auto& component : components) {
            if (component->GetTypeName() == "SkeletalMesh3D") {
                LOG_INFO(LogCategory::Render, "Found node with SkeletalMesh3D: '{}', active={}, visible={}",
                         node->GetName(), node->IsActiveInHierarchy(), node->IsVisibleInHierarchy());
            }
        }

        if (!node->IsActiveInHierarchy() || !node->IsVisibleInHierarchy()) {

            return;
        }

        for (auto& component : components) {
            auto renderable = dynamic_cast<IRenderableComponent*>(component.get());

            if (renderable) {
                // Log skeletal mesh components specifically
                if (component->GetTypeName() == "SkeletalMesh3D") {
                    LOG_INFO(LogCategory::Render, "Found SkeletalMesh3D component, enabled={}", component->IsEnabled());
                }

                SpatialType spatialType = renderable->getSpatialType();
                bool shouldRender = false;

                switch (cameraType) {
                    case CameraType::Camera2D:

                        if (view->getCamera()->isEditorCamera) {
                            shouldRender = (spatialType == SpatialType::World2D ||
                                          spatialType == SpatialType::Canvas);
                        } else {
                            shouldRender = (spatialType == SpatialType::World2D);
                        }
                        break;
                    case CameraType::Camera3D:

                        shouldRender = (spatialType == SpatialType::World3D);
                        break;
                    case CameraType::CameraCanvas:

                        shouldRender = (spatialType == SpatialType::Canvas);
                        break;
                }

                bool isVisible = ctx.isVisible(renderable->getWorldBounds());

                if (component->GetTypeName() == "SkeletalMesh3D") {
                    LOG_INFO(LogCategory::Render, "SkeletalMesh3D: shouldRender={}, isVisible={}", shouldRender, isVisible);
                }

                if (shouldRender) {

                    if (isVisible) {

                        ctx.setSpatialType(spatialType);

                        // For 2D and Canvas rendering, set Z-index for proper sorting
                        if (spatialType == SpatialType::World2D || spatialType == SpatialType::Canvas) {
                            auto node2D = std::dynamic_pointer_cast<core::Node2D>(node);
                            if (node2D) {
                                ctx.setZIndex(node2D->GetZIndex());
                            } else {
                                ctx.setZIndex(0);
                            }
                        }

                        renderable->buildDrawCommands(ctx);
                        m_stats.renderables++;
                    } else {

                    }
                }
            }
        }

        for (auto& child : node->GetChildren()) {
            gatherFromNode(child);
        }
    };

    gatherFromNode(root);

}

void RenderWorld::gatherLights(RenderView* view) {
    if (!view) {
        return;
    }

    view->m_activeLights.clear();

    if (!view->getScene()) {
        return;
    }

    using namespace core;
    using namespace components;
    Scene* scene = view->getScene();
    auto root = scene->GetRoot();
    if (!root) {
        return;
    }

    std::function<void(std::shared_ptr<Node>)> gatherFromNode = [&](std::shared_ptr<Node> node) {
        if (!node) {
            return;
        }

        if (!node->IsActiveInHierarchy() || !node->IsVisibleInHierarchy()) {
            return;
        }

        auto components = node->GetComponents();
        for (auto& component : components) {

            if (!component->IsEnabled()) {
                continue;
            }

            if (auto dirLight = std::dynamic_pointer_cast<DirectionalLight3D>(component)) {
                if (view->m_activeLights.size() < MAX_LIGHTS_PER_FRAME) {
                    view->m_activeLights.push_back(dirLight->ToLightDescriptor());
                }
            }

            else if (auto omniLight = std::dynamic_pointer_cast<OmniLight3D>(component)) {
                if (view->m_activeLights.size() < MAX_LIGHTS_PER_FRAME) {
                    view->m_activeLights.push_back(omniLight->ToLightDescriptor());
                }
            }

            else if (auto spotLight = std::dynamic_pointer_cast<SpotLight3D>(component)) {
                if (view->m_activeLights.size() < MAX_LIGHTS_PER_FRAME) {
                    view->m_activeLights.push_back(spotLight->ToLightDescriptor());
                }
            }
        }

        for (auto& child : node->GetChildren()) {
            gatherFromNode(child);
        }
    };

    gatherFromNode(root);

    for (size_t i = 0; i < view->m_activeLights.size(); ++i) {
        const auto& light = view->m_activeLights[i];

    }
}

void RenderWorld::gatherWorldEnvironment(RenderView* view) {
    if (!view) {
        return;
    }

    view->m_activeWorldEnvironment = nullptr;

    if (!view->getScene()) {
        return;
    }

    using namespace core;
    using namespace components;
    Scene* scene = view->getScene();
    auto root = scene->GetRoot();
    if (!root) {
        return;
    }

    std::function<void(std::shared_ptr<Node>)> gatherFromNode = [&](std::shared_ptr<Node> node) {
        if (!node) {
            return;
        }

        if (!node->IsActiveInHierarchy() || !node->IsVisibleInHierarchy()) {
            return;
        }

        auto components = node->GetComponents();
        for (auto& component : components) {

            if (!component->IsEnabled()) {
                continue;
            }

            if (auto worldEnv = std::dynamic_pointer_cast<WorldEnvironment>(component)) {

                view->m_activeWorldEnvironment = worldEnv.get();

                return;
            }
        }

        if (!view->m_activeWorldEnvironment) {
            for (auto& child : node->GetChildren()) {
                gatherFromNode(child);
                if (view->m_activeWorldEnvironment) {
                    return;
                }
            }
        }
    };

    gatherFromNode(root);
}

void RenderWorld::renderSkybox(IGfxCommandList* cmd, RenderView* view, const math::Mat4& viewProj) {
    if (!cmd || !view || !view->m_activeWorldEnvironment) {
        return;
    }

    using namespace components;
    WorldEnvironment* worldEnv = view->m_activeWorldEnvironment;

    if (worldEnv->GetSkyboxType() == static_cast<int>(WorldEnvironment::SkyboxType::None)) {
        return;
    }

    worldEnv->EnsureSkyboxResourcesCreated(m_device.get());

    MeshHandle skyboxMesh = worldEnv->GetSkyboxMesh();
    if (!skyboxMesh.isValid()) {

        return;
    }

    const GPUMesh* mesh = m_device->getMesh(skyboxMesh);
    if (!mesh) {

        return;
    }

    const char* vertSource = DefaultShaders::Skybox_Vertex();
    const char* fragSource = DefaultShaders::Skybox_Fragment();

    ShaderDesc vertDesc;
    vertDesc.stage = ShaderStage::Vertex;
    vertDesc.bytecode = vertSource;
    vertDesc.bytecodeSize = std::strlen(vertSource);
    vertDesc.entryPoint = "main";

    ShaderDesc fragDesc;
    fragDesc.stage = ShaderStage::Fragment;
    fragDesc.bytecode = fragSource;
    fragDesc.bytecodeSize = std::strlen(fragSource);
    fragDesc.entryPoint = "main";

    ShaderHandle vertShader = m_device->createShader(vertDesc);
    ShaderHandle fragShader = m_device->createShader(fragDesc);

    if (!vertShader.isValid() || !fragShader.isValid()) {

        if (vertShader.isValid()) m_device->destroyShader(vertShader);
        if (fragShader.isValid()) m_device->destroyShader(fragShader);
        return;
    }

    PipelineDesc pipelineDesc;
    pipelineDesc.shaders.push_back(vertShader);
    pipelineDesc.shaders.push_back(fragShader);

    VertexBufferLayout layout;
    layout.stride = sizeof(Vertex);
    layout.attributes.push_back({"a_Position", VertexFormat::Float3, 0, 0});
    layout.attributes.push_back({"a_Normal", VertexFormat::Float3, 12, 0});
    layout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, 24, 0});
    pipelineDesc.vertexLayout = layout;

    pipelineDesc.depthStencilState.depthTestEnable = true;
    pipelineDesc.depthStencilState.depthWriteEnable = false;
    pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;

    pipelineDesc.rasterizerState.cullMode = CullMode::Front;
    pipelineDesc.rasterizerState.frontFace = WindingOrder::CounterClockwise;
    pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

    pipelineDesc.blendState.blendEnable = false;

    PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
    if (!pipeline.isValid()) {

        m_device->destroyShader(vertShader);
        m_device->destroyShader(fragShader);
        return;
    }

    cmd->bindPipeline(pipeline);

    RenderCamera* camera = view->getCamera();
    if (camera) {

        math::Mat4 viewMatrix = camera->getViewMatrix();
        math::Mat4 viewNoTranslation = viewMatrix;
        viewNoTranslation[3][0] = 0.0f;
        viewNoTranslation[3][1] = 0.0f;
        viewNoTranslation[3][2] = 0.0f;

        float aspectRatio = view->getAspectRatio();
        math::Mat4 projection = camera->getProjectionMatrix(aspectRatio);
        math::Mat4 skyboxViewProj = projection * viewNoTranslation;

        cmd->setUniformMat4("u_ViewProjection", skyboxViewProj);
        cmd->setUniformMat4("u_View", viewNoTranslation);
    }

    int skyboxType = worldEnv->GetSkyboxType();

    // Check if texture-based skybox has valid texture, otherwise render as black
    bool hasValidTexture = true;
    if (skyboxType == static_cast<int>(WorldEnvironment::SkyboxType::Cubemap) ||
        skyboxType == static_cast<int>(WorldEnvironment::SkyboxType::Panoramic)) {
        TextureHandle skyboxTexture = worldEnv->GetSkyboxTexture();
        hasValidTexture = skyboxTexture.isValid();
    }

    // If texture-based skybox but no valid texture, render as black
    if (!hasValidTexture && (skyboxType == static_cast<int>(WorldEnvironment::SkyboxType::Cubemap) ||
                             skyboxType == static_cast<int>(WorldEnvironment::SkyboxType::Panoramic))) {
        cmd->setUniformInt("u_SkyboxType", static_cast<int>(WorldEnvironment::SkyboxType::Color));
        cmd->setUniformVec4("u_SkyboxColor", math::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    } else {
        cmd->setUniformInt("u_SkyboxType", skyboxType);

        switch (static_cast<WorldEnvironment::SkyboxType>(skyboxType)) {
            case WorldEnvironment::SkyboxType::Color: {
                math::Color color = worldEnv->GetSkyboxColor();
                cmd->setUniformVec4("u_SkyboxColor", math::Vec4(color.r, color.g, color.b, color.a));
                break;
            }
            case WorldEnvironment::SkyboxType::Procedural: {
                math::Color topColor = worldEnv->GetSkyTopColor();
                math::Color horizonColor = worldEnv->GetSkyHorizonColor();
                math::Color bottomColor = worldEnv->GetSkyBottomColor();
                cmd->setUniformVec4("u_SkyTopColor", math::Vec4(topColor.r, topColor.g, topColor.b, topColor.a));
                cmd->setUniformVec4("u_SkyHorizonColor", math::Vec4(horizonColor.r, horizonColor.g, horizonColor.b, horizonColor.a));
                cmd->setUniformVec4("u_SkyBottomColor", math::Vec4(bottomColor.r, bottomColor.g, bottomColor.b, bottomColor.a));
                break;
            }
            case WorldEnvironment::SkyboxType::Cubemap: {
                TextureHandle cubemapTexture = worldEnv->GetSkyboxTexture();
                if (cubemapTexture.isValid()) {
                    cmd->bindTexture(cubemapTexture, 0);
                    cmd->setUniformInt("u_CubemapTexture", 0);
                }
                break;
            }
            case WorldEnvironment::SkyboxType::Panoramic: {
                TextureHandle panoramicTexture = worldEnv->GetSkyboxTexture();
                if (panoramicTexture.isValid()) {
                    cmd->bindTexture(panoramicTexture, 0);
                    cmd->setUniformInt("u_PanoramicTexture", 0);
                }
                break;
            }
            default:
                break;
        }
    }

    cmd->bindVertexBuffer(mesh->vertexBuffer, 0);
    cmd->bindIndexBuffer(mesh->indexBuffer, IndexFormat::UInt32);
    cmd->drawIndexed(mesh->indexCount, 1, 0, 0, 0);

    m_device->destroyPipeline(pipeline);
    m_device->destroyShader(vertShader);
    m_device->destroyShader(fragShader);

}

void RenderWorld::buildRenderBatches(
    const std::vector<DrawItem>& drawItems,
    std::vector<RenderPass>& outPasses)
{

    RenderPass opaque3DPass;
    opaque3DPass.type = RenderPassType::Opaque3D;

    RenderPass transparent3DPass;
    transparent3DPass.type = RenderPassType::Transparent3D;

    RenderPass world2DPass;
    world2DPass.type = RenderPassType::World2D;

    RenderPass canvasPass;
    canvasPass.type = RenderPassType::Canvas;

    std::vector<const DrawItem*> opaqueItems;
    std::vector<const DrawItem*> transparentItems;
    std::vector<const DrawItem*> world2DItems;
    std::vector<const DrawItem*> canvasItems;

    for (const auto& item : drawItems) {
        if (!item.isVisible) continue;

        switch (item.spatialType) {
            case SpatialType::World2D:
                world2DItems.push_back(&item);
                break;
            case SpatialType::Canvas:
                canvasItems.push_back(&item);
                break;
            case SpatialType::World3D:

                switch (item.renderLayer) {
                    case RenderLayer::Opaque:
                    case RenderLayer::Background:
                        opaqueItems.push_back(&item);
                        break;
                    case RenderLayer::Transparent:
                    case RenderLayer::Default:
                        transparentItems.push_back(&item);
                        break;
                    case RenderLayer::UI:
                    case RenderLayer::Overlay:
                        canvasItems.push_back(&item);
                        break;
                    default:
                        opaqueItems.push_back(&item);
                        break;
                }
                break;
        }
    }

    sortDrawItems(opaqueItems, RenderPassType::Opaque3D);
    sortDrawItems(transparentItems, RenderPassType::Transparent3D);
    sortDrawItems(world2DItems, RenderPassType::World2D);
    sortDrawItems(canvasItems, RenderPassType::Canvas);

    auto createBatch = [this](const DrawItem* item) -> RenderBatch {
        RenderBatch batch;
        batch.material = item->material;
        batch.mesh = item->mesh;
        batch.submeshIndex = item->submeshIndex;
        batch.items.push_back(item);

        const Material* material = getMaterial(item->material);
        if (material) {
            batch.pipeline = material->pipeline;
        }

        return batch;
    };

    for (const DrawItem* item : opaqueItems) {
        opaque3DPass.batches.push_back(createBatch(item));
    }

    for (const DrawItem* item : transparentItems) {
        transparent3DPass.batches.push_back(createBatch(item));
    }

    for (const DrawItem* item : world2DItems) {
        world2DPass.batches.push_back(createBatch(item));
    }

    for (const DrawItem* item : canvasItems) {
        canvasPass.batches.push_back(createBatch(item));
    }

    if (!opaque3DPass.batches.empty()) outPasses.push_back(opaque3DPass);
    if (!transparent3DPass.batches.empty()) outPasses.push_back(transparent3DPass);
    if (!world2DPass.batches.empty()) outPasses.push_back(world2DPass);
    if (!canvasPass.batches.empty()) outPasses.push_back(canvasPass);

}

void RenderWorld::uploadLightData(IGfxCommandList* cmd, RenderView* view) {
    if (!cmd || !view || !view->m_lightUniformData) {
        return;
    }

    for (size_t i = 0; i < view->m_activeLights.size() && i < MAX_LIGHTS_PER_FRAME; ++i) {

    }

    if (!m_lightUniformBuffer.isValid()) {
        m_lightUniformBuffer = m_device->createUniformBuffer(sizeof(LightUniformBuffer));

        if (!m_lightUniformBuffer.isValid()) {

            return;
        }

    }

    int numShadowMaps = static_cast<int>(view->m_lightUniformData->lightCounts.y);
    for (int i = 0; i < numShadowMaps; ++i) {

    }

    if (view->m_activeWorldEnvironment) {
        using namespace components;
        WorldEnvironment* worldEnv = view->m_activeWorldEnvironment;

        if (worldEnv->GetAmbientLightEnabled()) {
            math::Color ambientColor = worldEnv->GetAmbientLightColor();
            float ambientIntensity = worldEnv->GetAmbientLightIntensity();
            view->m_lightUniformData->ambientLight = Vec4(
                ambientColor.r * ambientIntensity,
                ambientColor.g * ambientIntensity,
                ambientColor.b * ambientIntensity,
                ambientIntensity
            );
        } else {

            view->m_lightUniformData->ambientLight = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        }

        if (worldEnv->GetFogEnabled()) {
            math::Color fogColor = worldEnv->GetFogColor();
            view->m_lightUniformData->fogColor = Vec4(fogColor.r, fogColor.g, fogColor.b, 1.0f);

            float density = worldEnv->GetFogDensity();
            float start = worldEnv->GetFogStart();
            float end = worldEnv->GetFogEnd();
            float mode = static_cast<float>(worldEnv->GetFogMode());
            view->m_lightUniformData->fogParams = Vec4(density, start, end, mode);

        } else {

            view->m_lightUniformData->fogColor.w = 0.0f;
        }
    } else {

        view->m_lightUniformData->ambientLight = Vec4(0.1f, 0.1f, 0.1f, 1.0f);
        view->m_lightUniformData->fogColor.w = 0.0f;
    }

}

void RenderWorld::renderShadowMaps(RenderView* view, RenderContext& ctx) {
    if (!view || !view->m_lightUniformData) {

        return;
    }

    view->m_lightUniformData->lightCounts.x = static_cast<float>(view->m_activeLights.size());

    if (view->m_activeLights.empty()) {
        view->m_lightUniformData->lightCounts.y = 0.0f;
        view->m_lightUniformData->lightCounts.z = 0.0f;
        m_device->unbindFramebuffer();
        return;
    }

    for (size_t i = 0; i < view->m_activeLights.size() && i < MAX_LIGHTS_PER_FRAME; ++i) {
        view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(-1);

    }

    if (!m_shadowMapMaterial.isValid()) {

        ShaderDesc vertDesc;
        vertDesc.stage = ShaderStage::Vertex;
        vertDesc.bytecode = DefaultShaders::ShadowMap_Vertex();
        vertDesc.bytecodeSize = std::strlen(DefaultShaders::ShadowMap_Vertex());
        ShaderHandle vertShader = m_device->createShader(vertDesc);

        ShaderDesc fragDesc;
        fragDesc.stage = ShaderStage::Fragment;
        fragDesc.bytecode = DefaultShaders::ShadowMap_Fragment();
        fragDesc.bytecodeSize = std::strlen(DefaultShaders::ShadowMap_Fragment());
        ShaderHandle fragShader = m_device->createShader(fragDesc);

        if (!vertShader.isValid() || !fragShader.isValid()) {

            return;
        }

        VertexBufferLayout shadowVertexLayout;
        shadowVertexLayout.stride = sizeof(Vertex);
        shadowVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0});
        shadowVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0});
        shadowVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0});
        shadowVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders.push_back(vertShader);
        pipelineDesc.shaders.push_back(fragShader);
        pipelineDesc.vertexLayout = shadowVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::Less;
        pipelineDesc.blendState = BlendState::opaque();

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {

            m_device->destroyShader(vertShader);
            m_device->destroyShader(fragShader);
            return;
        }

        Material shadowMat;
        shadowMat.name = "ShadowMap";
        shadowMat.vertexShader = vertShader;
        shadowMat.fragmentShader = fragShader;
        shadowMat.pipeline = pipeline;
        shadowMat.renderLayer = RenderLayer::Opaque;
        shadowMat.isTransparent = false;

        m_shadowMapMaterial = createMaterial(shadowMat);

    }

    if (!m_shadowCubeMaterial.isValid()) {

        ShaderDesc vertDesc;
        vertDesc.stage = ShaderStage::Vertex;
        vertDesc.bytecode = DefaultShaders::ShadowCube_Vertex();
        vertDesc.bytecodeSize = std::strlen(DefaultShaders::ShadowCube_Vertex());
        ShaderHandle vertShader = m_device->createShader(vertDesc);

        ShaderDesc fragDesc;
        fragDesc.stage = ShaderStage::Fragment;
        fragDesc.bytecode = DefaultShaders::ShadowCube_Fragment();
        fragDesc.bytecodeSize = std::strlen(DefaultShaders::ShadowCube_Fragment());
        ShaderHandle fragShader = m_device->createShader(fragDesc);

        if (!vertShader.isValid() || !fragShader.isValid()) {

            return;
        }

        VertexBufferLayout shadowVertexLayout;
        shadowVertexLayout.stride = sizeof(Vertex);
        shadowVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0});
        shadowVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0});
        shadowVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0});
        shadowVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders.push_back(vertShader);
        pipelineDesc.shaders.push_back(fragShader);
        pipelineDesc.vertexLayout = shadowVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::Less;
        pipelineDesc.blendState = BlendState::opaque();

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {

            m_device->destroyShader(vertShader);
            m_device->destroyShader(fragShader);
            return;
        }

        Material shadowCubeMat;
        shadowCubeMat.name = "ShadowCube";
        shadowCubeMat.vertexShader = vertShader;
        shadowCubeMat.fragmentShader = fragShader;
        shadowCubeMat.pipeline = pipeline;
        shadowCubeMat.renderLayer = RenderLayer::Opaque;
        shadowCubeMat.isTransparent = false;

        m_shadowCubeMaterial = createMaterial(shadowCubeMat);

    }

    // Create skeletal shadow map material if needed
    if (!m_shadowMapSkeletalMaterial.isValid()) {

        ShaderDesc vertDesc;
        vertDesc.stage = ShaderStage::Vertex;
        vertDesc.bytecode = DefaultShaders::ShadowMapSkeletal_Vertex();
        vertDesc.bytecodeSize = std::strlen(DefaultShaders::ShadowMapSkeletal_Vertex());
        ShaderHandle vertShader = m_device->createShader(vertDesc);

        ShaderDesc fragDesc;
        fragDesc.stage = ShaderStage::Fragment;
        fragDesc.bytecode = DefaultShaders::ShadowMap_Fragment();
        fragDesc.bytecodeSize = std::strlen(DefaultShaders::ShadowMap_Fragment());
        ShaderHandle fragShader = m_device->createShader(fragDesc);

        if (!vertShader.isValid() || !fragShader.isValid()) {

            return;
        }

        VertexBufferLayout skeletalVertexLayout;
        skeletalVertexLayout.stride = sizeof(Vertex);
        skeletalVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0});
        skeletalVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0});
        skeletalVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0});
        skeletalVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0});
        skeletalVertexLayout.attributes.push_back({"a_Tangent", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent)), 0});
        skeletalVertexLayout.attributes.push_back({"a_BoneIDs", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneIDs)), 0});
        skeletalVertexLayout.attributes.push_back({"a_BoneWeights", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneWeights)), 0});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders.push_back(vertShader);
        pipelineDesc.shaders.push_back(fragShader);
        pipelineDesc.vertexLayout = skeletalVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::Less;
        pipelineDesc.blendState = BlendState::opaque();

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {

            m_device->destroyShader(vertShader);
            m_device->destroyShader(fragShader);
            return;
        }

        Material shadowSkeletalMat;
        shadowSkeletalMat.name = "ShadowMapSkeletal";
        shadowSkeletalMat.vertexShader = vertShader;
        shadowSkeletalMat.fragmentShader = fragShader;
        shadowSkeletalMat.pipeline = pipeline;
        shadowSkeletalMat.renderLayer = RenderLayer::Opaque;
        shadowSkeletalMat.isTransparent = false;

        m_shadowMapSkeletalMaterial = createMaterial(shadowSkeletalMat);

    }

    int shadowMapIndex = 0;
    int cascadedShadowMapIndex = 0;
    int cubeMapIndex = 0;

    for (size_t i = 0; i < view->m_activeLights.size(); ++i) {
        const auto& light = view->m_activeLights[i];

        if (!light.castShadows) {

            view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(-1);

            continue;
        }

        if (m_enableCascadedShadowMaps &&
            light.type == LightType::Directional && light.shadowCascades > 1) {
            if (cascadedShadowMapIndex >= MAX_SHADOW_MAPS) {

                continue;
            }

            RenderCamera* camera = view->getCamera();
            Camera3D* camera3D = dynamic_cast<Camera3D*>(camera);
            if (!camera3D) {

                continue;
            }

            uint32_t cascadeCount = std::min(light.shadowCascades, MAX_CASCADES);
            float cascadeSplits[MAX_CASCADES + 1];
            calculateCascadeSplits(camera3D->nearPlane, camera3D->farPlane, cascadeCount, light.cascadeSplitLambda, cascadeSplits);

            CascadedShadowMapData& csmData = view->m_lightUniformData->cascadedShadowMaps[cascadedShadowMapIndex];
            csmData.cascadeParams.x = static_cast<float>(cascadeCount);
            csmData.cascadeParams.y = 0.001f;
            csmData.cascadeParams.z = 0.0f;
            csmData.cascadeParams.w = static_cast<float>(shadowMapIndex);

            for (uint32_t c = 0; c < std::min(cascadeCount, 4u); ++c) {
                csmData.cascadeSplits[c] = cascadeSplits[c + 1];
            }
            for (uint32_t c = 4; c < cascadeCount && c < 8; ++c) {
                csmData.cascadeSplits2[c - 4] = cascadeSplits[c + 1];
            }

            Vec3 lightDir = light.direction.Normalized();

            for (uint32_t c = 0; c < cascadeCount; ++c) {

                Mat4 cascadeMatrix = calculateCascadeLightSpaceMatrix(view, lightDir, cascadeSplits[c], cascadeSplits[c + 1]);
                csmData.cascadeMatrices[c] = cascadeMatrix;

                if (c == 0) {

                }

                int cascadeShadowMapIndex = shadowMapIndex + c;

                if (cascadeShadowMapIndex >= m_shadowMapFramebuffers.size()) {
                    RenderTargetDesc rtDesc;
                    rtDesc.width = light.shadowResolution;
                    rtDesc.height = light.shadowResolution;
                    rtDesc.hasColor = false;
                    rtDesc.depthFormat = TextureFormat::DEPTH32F;
                    rtDesc.hasDepth = true;
                    rtDesc.sampleCount = 1;

                    RenderTargetHandle shadowFB = m_device->createRenderTarget(rtDesc);
                    if (!shadowFB.isValid()) {

                        continue;
                    }

                    m_shadowMapFramebuffers.push_back(shadowFB);

                    TextureHandle depthTex = m_device->getRenderTargetDepthTexture(shadowFB);
                    if (!depthTex.isValid()) {

                        continue;
                    }

                    m_shadowMaps.push_back(depthTex);

                }

                renderShadowMapForLight(view, ctx, cascadeShadowMapIndex, cascadeMatrix, light.shadowResolution);
            }

            view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(-(cascadedShadowMapIndex + 1));

            shadowMapIndex += cascadeCount;
            cascadedShadowMapIndex++;

        }

        else {

            if (shadowMapIndex >= MAX_SHADOW_MAPS) {

                view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(-1);

                continue;
            }

            if (light.type == LightType::Point) {

                bool needsRecreate = false;
                RenderTargetHandle oldCubeRT;

                if (cubeMapIndex >= m_shadowCubeMapFramebuffers.size()) {
                    needsRecreate = true;
                } else {

                    uint32_t currentResolution = (cubeMapIndex < m_shadowCubeMapResolutions.size())
                        ? m_shadowCubeMapResolutions[cubeMapIndex] : 0;

                    if (currentResolution != light.shadowResolution) {

                        needsRecreate = true;

                        oldCubeRT = m_shadowCubeMapFramebuffers[cubeMapIndex];
                    }
                }

                if (needsRecreate) {

                    RenderTargetDesc cubeDesc;
                    cubeDesc.width = light.shadowResolution;
                    cubeDesc.height = light.shadowResolution;
                    cubeDesc.hasColor = true;
                    cubeDesc.colorFormat = TextureFormat::R32_FLOAT;
                    cubeDesc.depthFormat = TextureFormat::DEPTH32F;
                    cubeDesc.hasDepth = true;
                    cubeDesc.sampleCount = 1;
                    cubeDesc.isCubeMap = true;
                    cubeDesc.cubeMapFace = 0;

                    RenderTargetHandle cubeRT = m_device->createRenderTarget(cubeDesc);
                    if (!cubeRT.isValid()) {

                        view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(-1);
                        continue;
                    }

                    TextureHandle colorCubeMap = m_device->getRenderTargetColorTexture(cubeRT);
                    if (!colorCubeMap.isValid()) {

                        view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(-1);
                        m_device->destroyRenderTarget(cubeRT);
                        continue;
                    }

                    if (cubeMapIndex >= m_shadowCubeMapFramebuffers.size()) {
                        m_shadowCubeMapFramebuffers.push_back(cubeRT);
                        m_shadowCubeMaps.push_back(colorCubeMap);
                        m_shadowCubeMapResolutions.push_back(light.shadowResolution);
                    } else {
                        m_shadowCubeMapFramebuffers[cubeMapIndex] = cubeRT;
                        m_shadowCubeMaps[cubeMapIndex] = colorCubeMap;
                        m_shadowCubeMapResolutions[cubeMapIndex] = light.shadowResolution;
                    }

                    if (oldCubeRT.isValid()) {
                        m_device->destroyRenderTarget(oldCubeRT);
                    }
                }
            }

            else {

                bool needsRecreate = false;
                RenderTargetHandle oldShadowFB;

                if (shadowMapIndex >= m_shadowMapFramebuffers.size()) {
                    needsRecreate = true;
                } else {

                    uint32_t currentResolution = (shadowMapIndex < m_shadowMapResolutions.size())
                        ? m_shadowMapResolutions[shadowMapIndex] : 0;

                    if (currentResolution != light.shadowResolution) {

                        needsRecreate = true;

                        oldShadowFB = m_shadowMapFramebuffers[shadowMapIndex];
                    }
                }

                if (needsRecreate) {
                    RenderTargetDesc rtDesc;
                    rtDesc.width = light.shadowResolution;
                    rtDesc.height = light.shadowResolution;
                    rtDesc.hasColor = false;
                    rtDesc.depthFormat = TextureFormat::DEPTH32F;
                    rtDesc.hasDepth = true;
                    rtDesc.sampleCount = 1;

                    RenderTargetHandle shadowFB = m_device->createRenderTarget(rtDesc);
                    if (!shadowFB.isValid()) {

                        view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(-1);
                        continue;
                    }

                    TextureHandle depthTex = m_device->getRenderTargetDepthTexture(shadowFB);
                    if (!depthTex.isValid()) {

                        view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(-1);
                        continue;
                    }

                    if (shadowMapIndex >= m_shadowMapFramebuffers.size()) {
                        m_shadowMapFramebuffers.push_back(shadowFB);
                        m_shadowMaps.push_back(depthTex);
                        m_shadowMapResolutions.push_back(light.shadowResolution);
                    } else {
                        m_shadowMapFramebuffers[shadowMapIndex] = shadowFB;
                        m_shadowMaps[shadowMapIndex] = depthTex;
                        m_shadowMapResolutions[shadowMapIndex] = light.shadowResolution;
                    }

                    if (oldShadowFB.isValid()) {
                        m_device->destroyRenderTarget(oldShadowFB);
                    }
                }
            }

            Mat4 lightSpaceMatrix = Mat4::Identity();
            bool validMatrix = false;

            if (light.type == LightType::Directional) {

                RenderCamera* camera = view->getCamera();
                Camera3D* camera3D = dynamic_cast<Camera3D*>(camera);
                if (!camera3D) {

                    view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(-1);
                    continue;
                }

                Vec3 lightDir = light.direction.Normalized();

                float shadowDistance = std::min(camera3D->farPlane, 100.0f);

                lightSpaceMatrix = calculateCascadeLightSpaceMatrix(view, lightDir, camera3D->nearPlane, shadowDistance);
                validMatrix = true;

            } else if (light.type == LightType::Point) {

                Vec3 lightPos = light.position;

                view->m_lightUniformData->shadowMaps[shadowMapIndex].lightSpaceMatrix = Mat4::Identity();
                view->m_lightUniformData->shadowMaps[shadowMapIndex].shadowParams = Vec4(light.shadowBias, 0.0001f, light.shadowBlur, light.shadowOpacity);
                view->m_lightUniformData->shadowMaps[shadowMapIndex].shadowParams2 = Vec4(static_cast<float>(light.shadowResolution), 1.0f, light.range, 0.0f);

                view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(shadowMapIndex);

                Vec3 faceDirections[6] = {
                    Vec3(1, 0, 0),
                    Vec3(-1, 0, 0),
                    Vec3(0, 1, 0),
                    Vec3(0, -1, 0),
                    Vec3(0, 0, 1),
                    Vec3(0, 0, -1)
                };

                Vec3 faceUps[6] = {
                    Vec3(0, -1, 0),
                    Vec3(0, -1, 0),
                    Vec3(0, 0, 1),
                    Vec3(0, 0, -1),
                    Vec3(0, -1, 0),
                    Vec3(0, -1, 0)
                };

                Mat4 lightProj = math::Camera::Perspective(1.5708f, 1.0f, 0.1f, light.range);

                for (int face = 0; face < 6; ++face) {
                    Vec3 target = lightPos + faceDirections[face];
                    Mat4 lightView = math::Camera::LookAt(lightPos, target, faceUps[face]);
                    Mat4 faceMatrix = lightProj * lightView;

                    renderShadowCubeMapFace(view, ctx, cubeMapIndex, face, faceMatrix, light.shadowResolution, lightPos, light.range);
                }

                m_device->unbindFramebuffer();

                shadowMapIndex++;
                cubeMapIndex++;
                continue;
            } else if (light.type == LightType::Spot) {

                Vec3 lightPos = light.position;
                Vec3 lightDir = light.direction.Normalized();
                Vec3 target = lightPos + lightDir;

                Vec3 up = Vec3(0, 1, 0);

                if (std::abs(lightDir.y) > 0.99f) {
                    up = Vec3(1, 0, 0);
                }

                Mat4 lightView = math::Camera::LookAt(lightPos, target, up);

                float fov = light.outerConeAngle * 2.0f;
                fov = std::min(fov, 3.14159f);

                float nearPlane = 0.1f;
                float farPlane = light.range;
                Mat4 lightProj = math::Camera::Perspective(fov, 1.0f, nearPlane, farPlane);

                lightSpaceMatrix = lightProj * lightView;
                validMatrix = true;

            }

            if (!validMatrix) {

                view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(-1);
                continue;
            }

            view->m_lightUniformData->shadowMaps[shadowMapIndex].lightSpaceMatrix = lightSpaceMatrix;

            float shadowBias = 0.0005f;
            float normalBias = 0.0f;
            view->m_lightUniformData->shadowMaps[shadowMapIndex].shadowParams = Vec4(shadowBias, normalBias, light.shadowBlur, light.shadowOpacity);

            float isCubeMap = (light.type == LightType::Point) ? 1.0f : 0.0f;
            float lightRange = (light.type == LightType::Point) ? light.range : 0.0f;
            view->m_lightUniformData->shadowMaps[shadowMapIndex].shadowParams2 = Vec4(static_cast<float>(light.shadowResolution), isCubeMap, lightRange, 0.0f);

            renderShadowMapForLight(view, ctx, shadowMapIndex, lightSpaceMatrix, light.shadowResolution);

            view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(shadowMapIndex);

            shadowMapIndex++;
        }
    }

    view->m_lightUniformData->lightCounts.y = static_cast<float>(shadowMapIndex);
    view->m_lightUniformData->lightCounts.z = static_cast<float>(cascadedShadowMapIndex);

    m_device->unbindFramebuffer();
}

void RenderWorld::renderShadowMapForLight(RenderView* view, RenderContext& ctx, int shadowMapIndex, const Mat4& lightSpaceMatrix, int shadowResolution) {
    if (shadowMapIndex < 0 || shadowMapIndex >= m_shadowMapFramebuffers.size()) {
        return;
    }

    RenderTargetHandle shadowFB = m_shadowMapFramebuffers[shadowMapIndex];
    if (!shadowFB.isValid()) {
        return;
    }

    auto cmd = m_device->beginFrame(shadowFB);
    if (!cmd) {

        return;
    }

    cmd->clearDepth(1.0f);

    const Material* shadowMaterial = getMaterial(m_shadowMapMaterial);
    const Material* shadowSkeletalMaterial = getMaterial(m_shadowMapSkeletalMaterial);
    if (!shadowMaterial) {

        m_device->submit(std::move(cmd));
        return;
    }

    Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(shadowResolution);
    viewport.height = static_cast<float>(shadowResolution);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cmd->setViewport(viewport);

    ScissorRect scissor;
    scissor.x = 0;
    scissor.y = 0;
    scissor.width = shadowResolution;
    scissor.height = shadowResolution;
    cmd->setScissor(scissor);

    const auto& drawItems = ctx.getDrawItems();
    int renderedCount = 0;

    for (const auto& item : drawItems) {

        if (item.spatialType != SpatialType::World3D || item.renderLayer != RenderLayer::Opaque) {
            continue;
        }

        if (!item.castShadow) {
            continue;
        }

        const GPUMesh* mesh = m_device->getMesh(item.mesh);
        if (!mesh) {
            continue;
        }

        // Check if this is a skeletal mesh by looking for bone transform data in property overrides
        bool isSkeletal = false;
        const MaterialPropertyValue* useSkinningProp = item.propertyOverrides.getProperty("u_UseSkinning");
        if (useSkinningProp && std::holds_alternative<bool>(*useSkinningProp)) {
            isSkeletal = std::get<bool>(*useSkinningProp);
        }

        // Use appropriate shadow material based on mesh type
        const Material* currentShadowMaterial = isSkeletal && shadowSkeletalMaterial ? shadowSkeletalMaterial : shadowMaterial;
        cmd->bindPipeline(currentShadowMaterial->pipeline);

        cmd->setUniformMat4("u_LightSpaceMatrix", lightSpaceMatrix);
        cmd->setUniformMat4("u_Model", item.worldTransform);

        // If skeletal, set bone transforms and skinning flag
        if (isSkeletal) {
            cmd->setUniformBool("u_UseSkinning", true);

            const MaterialPropertyValue* boneTransformsProp = item.propertyOverrides.getProperty("u_BoneTransforms");
            if (boneTransformsProp && std::holds_alternative<std::vector<Mat4>>(*boneTransformsProp)) {
                const auto& boneTransforms = std::get<std::vector<Mat4>>(*boneTransformsProp);
                if (!boneTransforms.empty()) {
                    cmd->setUniformMat4Array("u_BoneTransforms", boneTransforms.data(), boneTransforms.size());
                }
            }
        } else {
            cmd->setUniformBool("u_UseSkinning", false);
        }

        float alphaCutoff = 0.5f;
        int hasAlbedoTexture = 0;
        TextureHandle albedoTexture;

        const MaterialPropertyValue* alphaCutoffProp = item.propertyOverrides.getProperty("u_AlphaCutoff");
        if (alphaCutoffProp && std::holds_alternative<float>(*alphaCutoffProp)) {
            alphaCutoff = std::get<float>(*alphaCutoffProp);
        } else {

            const MaterialPropertyValue* params2Prop = item.propertyOverrides.getProperty("u_MaterialParams2");
            if (params2Prop && std::holds_alternative<Vec4>(*params2Prop)) {
                Vec4 params2 = std::get<Vec4>(*params2Prop);
                alphaCutoff = params2.x;
            }
        }

        const MaterialPropertyValue* albedoTexProp = item.propertyOverrides.getProperty("u_AlbedoTexture");
        if (albedoTexProp && std::holds_alternative<TextureHandle>(*albedoTexProp)) {
            albedoTexture = std::get<TextureHandle>(*albedoTexProp);
            if (albedoTexture.isValid()) {
                hasAlbedoTexture = 1;
            }
        }

        cmd->setUniformFloat("u_AlphaCutoff", alphaCutoff);
        cmd->setUniformInt("u_HasAlbedoTexture", hasAlbedoTexture);

        if (hasAlbedoTexture && albedoTexture.isValid()) {
            cmd->bindTexture(albedoTexture, 0, 0);
            cmd->setUniformInt("u_AlbedoTexture", 0);
        }

        cmd->bindVertexBuffer(mesh->vertexBuffer, 0, 0);
        cmd->bindIndexBuffer(mesh->indexBuffer, IndexFormat::UInt32, 0);

        if (item.submeshIndex < mesh->submeshes.size()) {
            const auto& submesh = mesh->submeshes[item.submeshIndex];
            cmd->drawIndexed(
                submesh.indexCount,
                1,
                submesh.indexOffset,
                submesh.vertexOffset,
                0
            );
        } else {
            cmd->drawIndexed(
                mesh->indexCount,
                1,
                0,
                0,
                0
            );
        }

        renderedCount++;
    }

    ScissorRect disableScissor{0, 0, 0, 0};
    cmd->setScissor(disableScissor);

    m_device->submit(std::move(cmd));

    m_device->unbindFramebuffer();
}

void RenderWorld::renderShadowCubeMapFace(RenderView* view, RenderContext& ctx, int cubeMapIndex, int faceIndex, const Mat4& lightSpaceMatrix, int shadowResolution, const Vec3& lightPos, float lightRange) {
    if (cubeMapIndex < 0 || cubeMapIndex >= m_shadowCubeMapFramebuffers.size()) {
        return;
    }

    if (faceIndex < 0 || faceIndex >= 6) {
        return;
    }

    RenderTargetHandle cubeRT = m_shadowCubeMapFramebuffers[cubeMapIndex];

    m_device->attachCubeMapFace(cubeRT, faceIndex);

    auto cmd = m_device->beginFrame(cubeRT);
    if (!cmd) {

        return;
    }

    cmd->clearColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    cmd->clearDepth(1.0f);

    const Material* shadowMaterial = getMaterial(m_shadowCubeMaterial);
    if (!shadowMaterial) {

        m_device->submit(std::move(cmd));
        return;
    }

    cmd->bindPipeline(shadowMaterial->pipeline);

    cmd->setUniformVec3("u_LightPos", lightPos);
    cmd->setUniformFloat("u_LightRange", lightRange);

    Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(shadowResolution);
    viewport.height = static_cast<float>(shadowResolution);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cmd->setViewport(viewport);

    ScissorRect scissor;
    scissor.x = 0;
    scissor.y = 0;
    scissor.width = shadowResolution;
    scissor.height = shadowResolution;
    cmd->setScissor(scissor);

    const auto& drawItems = ctx.getDrawItems();
    int renderedCount = 0;

    for (const auto& item : drawItems) {

        if (item.spatialType != SpatialType::World3D || item.renderLayer != RenderLayer::Opaque) {
            continue;
        }

        if (!item.castShadow) {
            continue;
        }

        const GPUMesh* mesh = m_device->getMesh(item.mesh);
        if (!mesh) {
            continue;
        }

        cmd->setUniformMat4("u_LightSpaceMatrix", lightSpaceMatrix);

        cmd->setUniformMat4("u_Model", item.worldTransform);

        float alphaCutoff = 0.5f;
        int hasAlbedoTexture = 0;
        TextureHandle albedoTexture;

        const MaterialPropertyValue* alphaCutoffProp = item.propertyOverrides.getProperty("u_AlphaCutoff");
        if (alphaCutoffProp && std::holds_alternative<float>(*alphaCutoffProp)) {
            alphaCutoff = std::get<float>(*alphaCutoffProp);
        } else {
            const MaterialPropertyValue* params2Prop = item.propertyOverrides.getProperty("u_MaterialParams2");
            if (params2Prop && std::holds_alternative<Vec4>(*params2Prop)) {
                Vec4 params2 = std::get<Vec4>(*params2Prop);
                alphaCutoff = params2.x;
            }
        }

        const MaterialPropertyValue* albedoTexProp = item.propertyOverrides.getProperty("u_AlbedoTexture");
        if (albedoTexProp && std::holds_alternative<TextureHandle>(*albedoTexProp)) {
            albedoTexture = std::get<TextureHandle>(*albedoTexProp);
            if (albedoTexture.isValid()) {
                hasAlbedoTexture = 1;
            }
        }

        cmd->setUniformFloat("u_AlphaCutoff", alphaCutoff);
        cmd->setUniformInt("u_HasAlbedoTexture", hasAlbedoTexture);

        if (hasAlbedoTexture && albedoTexture.isValid()) {
            cmd->bindTexture(albedoTexture, 0, 0);
            cmd->setUniformInt("u_AlbedoTexture", 0);
        }

        cmd->bindVertexBuffer(mesh->vertexBuffer, 0, 0);
        cmd->bindIndexBuffer(mesh->indexBuffer, IndexFormat::UInt32, 0);

        if (item.submeshIndex < mesh->submeshes.size()) {
            const auto& submesh = mesh->submeshes[item.submeshIndex];
            cmd->drawIndexed(
                submesh.indexCount,
                1,
                submesh.indexOffset,
                submesh.vertexOffset,
                0
            );
        } else {
            cmd->drawIndexed(
                mesh->indexCount,
                1,
                0,
                0,
                0
            );
        }
        renderedCount++;
    }

    ScissorRect disableScissor{0, 0, 0, 0};
    cmd->setScissor(disableScissor);

    m_device->submit(std::move(cmd));
}

void RenderWorld::calculateCascadeSplits(float nearPlane, float farPlane, uint32_t cascadeCount, float lambda, float* outSplits) {

    outSplits[0] = nearPlane;

    for (uint32_t i = 1; i < cascadeCount; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(cascadeCount);

        float uniformSplit = nearPlane + (farPlane - nearPlane) * t;

        float logSplit = nearPlane * std::pow(farPlane / nearPlane, t);

        outSplits[i] = lambda * logSplit + (1.0f - lambda) * uniformSplit;
    }

    outSplits[cascadeCount] = farPlane;
}

math::Mat4 RenderWorld::calculateCascadeLightSpaceMatrix(const RenderView* view, const math::Vec3& lightDir, float nearPlane, float farPlane) {

    RenderCamera* camera = view->getCamera();
    Camera3D* camera3D = dynamic_cast<Camera3D*>(camera);

    if (!camera3D) {
        Vec3 lightPos = -lightDir * 100.0f;
        Vec3 up = std::abs(lightDir.y) > 0.99f ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
        Mat4 lightView = math::Camera::LookAt(lightPos, Vec3(0, 0, 0), up);
        float orthoSize = 100.0f;
        Mat4 lightProj = math::Camera::Orthographic(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 300.0f);
        return lightProj * lightView;
    }

    float aspectRatio = view->getAspectRatio();

    Vec3 cameraPos = camera3D->position;
    Mat4 cameraView = camera3D->getViewMatrix();

    Vec3 viewDir = Vec3(-cameraView[0][2], -cameraView[1][2], -cameraView[2][2]).Normalized();
    Vec3 right = Vec3(cameraView[0][0], cameraView[1][0], cameraView[2][0]).Normalized();
    Vec3 up = Vec3(cameraView[0][1], cameraView[1][1], cameraView[2][1]).Normalized();

    float fovY = camera3D->fov * (3.14159265359f / 180.0f);
    float nearHeight = 2.0f * std::tan(fovY * 0.5f) * nearPlane;
    float nearWidth = nearHeight * aspectRatio;
    float farHeight = 2.0f * std::tan(fovY * 0.5f) * farPlane;
    float farWidth = farHeight * aspectRatio;

    Vec3 nearCenter = cameraPos + viewDir * nearPlane;
    Vec3 farCenter = cameraPos + viewDir * farPlane;

    math::Vec3 frustumCorners[8];

    frustumCorners[0] = nearCenter - right * (nearWidth * 0.5f) - up * (nearHeight * 0.5f);
    frustumCorners[1] = nearCenter + right * (nearWidth * 0.5f) - up * (nearHeight * 0.5f);
    frustumCorners[2] = nearCenter + right * (nearWidth * 0.5f) + up * (nearHeight * 0.5f);
    frustumCorners[3] = nearCenter - right * (nearWidth * 0.5f) + up * (nearHeight * 0.5f);

    frustumCorners[4] = farCenter - right * (farWidth * 0.5f) - up * (farHeight * 0.5f);
    frustumCorners[5] = farCenter + right * (farWidth * 0.5f) - up * (farHeight * 0.5f);
    frustumCorners[6] = farCenter + right * (farWidth * 0.5f) + up * (farHeight * 0.5f);
    frustumCorners[7] = farCenter - right * (farWidth * 0.5f) + up * (farHeight * 0.5f);

    math::Vec3 center(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 8; ++i) {
        center = center + frustumCorners[i];
    }
    center = center / 8.0f;

    Vec3 upVec = std::abs(lightDir.y) > 0.99f ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(0.0f, 1.0f, 0.0f);

    math::Vec3 lightPos = center - lightDir * 500.0f;
    math::Mat4 lightView = math::Camera::LookAt(lightPos, center, upVec);

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();

    for (int i = 0; i < 8; ++i) {
        math::Vec4 cornerLS = lightView * math::Vec4(frustumCorners[i], 1.0f);
        minX = std::min(minX, cornerLS.x);
        maxX = std::max(maxX, cornerLS.x);
        minY = std::min(minY, cornerLS.y);
        maxY = std::max(maxY, cornerLS.y);
        minZ = std::min(minZ, cornerLS.z);
        maxZ = std::max(maxZ, cornerLS.z);
    }

    float zExtension = (maxZ - minZ) * 2.0f;
    minZ -= zExtension;

    float orthoNear = -maxZ;
    float orthoFar = -minZ;

    if (orthoNear > orthoFar) {
        std::swap(orthoNear, orthoFar);
    }

    float worldUnitsPerTexel = (maxX - minX) / 1024.0f;

    minX = std::floor(minX / worldUnitsPerTexel) * worldUnitsPerTexel;
    maxX = std::floor(maxX / worldUnitsPerTexel) * worldUnitsPerTexel;
    minY = std::floor(minY / worldUnitsPerTexel) * worldUnitsPerTexel;
    maxY = std::floor(maxY / worldUnitsPerTexel) * worldUnitsPerTexel;

    math::Mat4 lightProj = math::Camera::Orthographic(minX, maxX, minY, maxY, orthoNear, orthoFar);

    Mat4 lightSpaceMatrix = lightProj * lightView;

    return lightSpaceMatrix;
}

void RenderWorld::sortDrawItems(std::vector<const DrawItem*>& items, RenderPassType passType) {
    switch (passType) {
        case RenderPassType::Opaque3D:

            std::sort(items.begin(), items.end(), [](const DrawItem* a, const DrawItem* b) {
                if (a->material.id != b->material.id) {
                    return a->material.id < b->material.id;
                }
                if (a->mesh.id != b->mesh.id) {
                    return a->mesh.id < b->mesh.id;
                }
                return a->distanceToCamera < b->distanceToCamera;
            });
            break;

        case RenderPassType::Transparent3D:

            std::sort(items.begin(), items.end(), [](const DrawItem* a, const DrawItem* b) {
                return a->distanceToCamera > b->distanceToCamera;
            });
            break;

        case RenderPassType::World2D:
        case RenderPassType::Canvas:

            std::sort(items.begin(), items.end(), [](const DrawItem* a, const DrawItem* b) {
                if (a->renderLayer != b->renderLayer) {
                    return static_cast<uint32_t>(a->renderLayer) < static_cast<uint32_t>(b->renderLayer);
                }
                return a->sortKey < b->sortKey;
            });
            break;

        default:
            break;
    }
}

void RenderWorld::executeRenderPasses(
    RenderView* view,
    const std::vector<RenderPass>& passes,
    RenderContext& ctx)
{
    if (!m_device) {
        return;
    }

    RenderTargetHandle target;
    if (view->hasSwapchain()) {
        target = m_device->getSwapchainBackbuffer(view->getSwapchain());
    } else if (view->hasRenderTarget()) {
        target = view->getRenderTarget();
    } else {
        return;
    }

    m_device->unbindFramebuffer();

    auto cmd = m_device->beginFrame(target);
    if (!cmd) {

        return;
    }

    const Viewport& vp = view->getViewport();

    cmd->setViewport(vp);

    cmd->setScissor(view->getScissor());

    RenderCamera* camera = view->getCamera();
    if (camera) {
        if (static_cast<uint32_t>(camera->clearFlags) & static_cast<uint32_t>(CameraClearFlags::Color)) {
            cmd->clearColor(camera->clearColor);
        }
        if (static_cast<uint32_t>(camera->clearFlags) & static_cast<uint32_t>(CameraClearFlags::Depth)) {
            cmd->clearDepth(camera->clearDepth);
        }
        if (static_cast<uint32_t>(camera->clearFlags) & static_cast<uint32_t>(CameraClearFlags::Stencil)) {
            cmd->clearStencil(camera->clearStencil);
        }
    }

    Mat4 viewProj = Mat4::Identity();
    if (camera) {
        float aspectRatio = view->getAspectRatio();
        Mat4 viewMatrix = camera->getViewMatrix();
        Mat4 proj = camera->getProjectionMatrix(aspectRatio);
        viewProj = proj * viewMatrix;
    }

    uploadLightData(cmd.get(), view);

    renderSkybox(cmd.get(), view, viewProj);

    bool debugEnabled = m_debugRenderingEnabled && view->isDebugRenderingEnabled() && m_debugRenderer;
    bool gridEnabled = debugEnabled && view->isGridRenderingEnabled();
    if (gridEnabled) {
        m_debugRenderer->beginFrame();
        renderDebugGrid(cmd.get(), view, viewProj);
        m_debugRenderer->endFrame();
        m_debugRenderer->render(cmd.get(), viewProj, 2.5f);

    }

    for (const auto& pass : passes) {
        cmd->beginDebugMarker(getPassName(pass.type));

        for (const auto& batch : pass.batches) {
            executeBatch(cmd.get(), batch, viewProj, camera, view);
        }

        cmd->endDebugMarker();
    }

    const auto& customMeshes = view->getCustomDrawMeshes();
    if (!customMeshes.empty()) {
        static bool logged = false;
        if (!logged) {

            logged = true;
        }

        cmd->beginDebugMarker("Custom Draw Meshes");

        for (const auto& customMesh : customMeshes) {
            if (!customMesh.mesh.isValid()) {
                continue;
            }

            const GPUMesh* gpuMesh = m_device->getMesh(customMesh.mesh);
            if (!gpuMesh) {
                continue;
            }

            MaterialHandle materialToUse = customMesh.material.isValid() ? customMesh.material : m_defaultColoredMaterial;
            const Material* material = getMaterial(materialToUse);
            if (!material) {
                continue;
            }

            if (!material->pipeline.isValid()) {
                continue;
            }

            cmd->bindPipeline(material->pipeline);

            Mat4 mvp = viewProj * customMesh.transform;
            cmd->setUniformMat4("u_ModelViewProjection", mvp);
            cmd->setUniformMat4("u_Model", customMesh.transform);
            cmd->setUniformMat4("u_ViewProjection", viewProj);

            cmd->setUniformVec4("u_TintColor", Vec4(1.0f, 1.0f, 1.0f, 1.0f));
            cmd->setUniformBool("u_UseTexture", false);
            cmd->setUniformVec4("u_UVRect", Vec4(0.0f, 0.0f, 1.0f, 1.0f));

            cmd->bindVertexBuffer(gpuMesh->vertexBuffer, 0);
            cmd->bindIndexBuffer(gpuMesh->indexBuffer, IndexFormat::UInt32);

            cmd->drawIndexed(gpuMesh->indexCount, 1, 0, 0, 0);
        }

        cmd->endDebugMarker();

        view->clearCustomDrawMeshes();
    }

    if (debugEnabled) {
        m_debugRenderer->beginFrame();
        renderDebugOverlayPostSprites(cmd.get(), view, viewProj);
        m_debugRenderer->endFrame();
        m_debugRenderer->render(cmd.get(), viewProj, 4.0f);

        cmd->setRenderTarget(target);
    }

    m_device->submit(std::move(cmd));
}

void RenderWorld::executeBatch(
    IGfxCommandList* cmd,
    const RenderBatch& batch,
    const Mat4& viewProj,
    RenderCamera* camera,
    RenderView* view)
{
    if (!cmd || batch.items.empty() || !view) {

        return;
    }

    cmd->bindPipeline(batch.pipeline);

    const Material* material = getMaterial(batch.material);
    if (!material) {
        return;
    }

    bool isPBRMaterial = (material->name == "DefaultPBR" || material->name == "DefaultSkeletal");
    if (isPBRMaterial && m_lightUniformBuffer.isValid()) {
        cmd->bindUniformBuffer(m_lightUniformBuffer, 0, 0);

    }

    if (isPBRMaterial && camera) {
        Camera3D* camera3D = dynamic_cast<Camera3D*>(camera);
        if (camera3D) {
            cmd->setUniformVec3("u_CameraPosition", camera3D->position);
            cmd->setUniformMat4("u_View", camera3D->getViewMatrix());

        }
    }

    uint32_t textureUnit = 0;
    if (!isPBRMaterial) {
        for (const auto& texturePair : material->textures) {
            cmd->bindTexture(texturePair.second, textureUnit, 0);

            cmd->setUniformInt(texturePair.first.c_str(), static_cast<int>(textureUnit));
            ++textureUnit;
        }
    }

    if (material->name == "DefaultPBR") {
        const uint32_t shadowMapBaseUnit = 8;
        const size_t maxShadowMapsToBind = 8;
        int boundShadowMaps = 0;

        for (size_t i = 0; i < m_shadowMaps.size() && i < maxShadowMapsToBind; ++i) {
            if (m_shadowMaps[i].isValid()) {
                uint32_t unit = shadowMapBaseUnit + static_cast<uint32_t>(i);
                cmd->bindTexture(m_shadowMaps[i], unit, 0);
                std::string uniformName = "u_ShadowMap" + std::to_string(i);
                cmd->setUniformInt(uniformName.c_str(), static_cast<int>(unit));
                ++boundShadowMaps;

            }
        }

        if (boundShadowMaps > 0) {

        } else {

        }

        if (m_shadowMaps.size() > maxShadowMapsToBind) {

        }

        const uint32_t shadowCubeMapBaseUnit = 16;
        const size_t maxShadowCubeMapsToBind = 8;
        int boundShadowCubeMaps = 0;

        for (size_t i = 0; i < m_shadowCubeMaps.size() && i < maxShadowCubeMapsToBind; ++i) {
            if (m_shadowCubeMaps[i].isValid()) {
                uint32_t unit = shadowCubeMapBaseUnit + static_cast<uint32_t>(i);
                cmd->bindTexture(m_shadowCubeMaps[i], unit, 0);
                std::string uniformName = "u_ShadowCubeMap" + std::to_string(i);
                cmd->setUniformInt(uniformName.c_str(), static_cast<int>(unit));
                ++boundShadowCubeMaps;

            }
        }

        if (boundShadowCubeMaps > 0) {

        }

        if (m_shadowCubeMaps.size() > maxShadowCubeMapsToBind) {

        }
    }

    const GPUMesh* mesh = m_device->getMesh(batch.mesh);
    if (!mesh) {
        return;
    }

    const Submesh* submesh = nullptr;
    if (batch.submeshIndex < mesh->submeshes.size()) {
        submesh = &mesh->submeshes[batch.submeshIndex];
    }

    for (const DrawItem* item : batch.items) {
        if (!item) {
            continue;
        }

        struct PerObjectUniforms {
            Mat4 viewProjection;
            Mat4 model;
            Mat4 normalMatrix;
            Vec4 tintColor;
        } uniforms;

        uniforms.viewProjection = viewProj;
        uniforms.model = item->worldTransform;
        uniforms.normalMatrix = item->perObject.normalMatrix;
        uniforms.tintColor = Vec4(
            item->perObject.tintColor.r,
            item->perObject.tintColor.g,
            item->perObject.tintColor.b,
            item->perObject.tintColor.a
        );

        auto col3 = item->worldTransform[3];

        cmd->pushConstants(ShaderStage::Vertex, &uniforms, sizeof(PerObjectUniforms), 0);

        if (!item->propertyOverrides.isEmpty()) {
            applyPropertyOverrides(cmd, item->propertyOverrides);
        }

        cmd->bindVertexBuffer(mesh->vertexBuffer, 0, 0);
        cmd->bindIndexBuffer(mesh->indexBuffer, IndexFormat::UInt32, 0);

        if (submesh) {

            cmd->drawIndexed(
                submesh->indexCount,
                1,
                submesh->indexOffset,
                submesh->vertexOffset,
                0
            );
        } else {

            cmd->drawIndexed(
                mesh->indexCount,
                1,
                0,
                0,
                0
            );
        }

        m_stats.drawCalls++;
    }
}

const char* RenderWorld::getPassName(RenderPassType type) const {
    switch (type) {
        case RenderPassType::Opaque3D: return "Opaque3D";
        case RenderPassType::Transparent3D: return "Transparent3D";
        case RenderPassType::World2D: return "World2D";
        case RenderPassType::Canvas: return "Canvas";
        case RenderPassType::Shadow: return "Shadow";
        case RenderPassType::PostProcess: return "PostProcess";
        default: return "Unknown";
    }
}

void RenderWorld::renderDebugGrid(IGfxCommandList* cmd, RenderView* view, const Mat4& viewProj) {
    if (!m_debugRenderer || !view || !cmd) {
        return;
    }

    RenderCamera* camera = view->getCamera();
    if (!camera) {
        return;
    }

    Camera2D* camera2D = dynamic_cast<Camera2D*>(camera);
    Camera3D* camera3D = dynamic_cast<Camera3D*>(camera);

    if (camera2D) {

        float cellSize = 100.0f;
        int cellCount = 100;

        DebugGrid grid2D;
        grid2D.cellSize = cellSize;
        grid2D.cellCount = cellCount;
        grid2D.center = math::Vec3(0.0f, 0.0f, 0.0f);
        grid2D.gridColor = math::Color(0.35f, 0.35f, 0.35f, 1.0f);
        grid2D.axisColor = math::Color(0.4f, 0.4f, 0.4f, 1.0f);
        grid2D.is3D = false;

        m_debugRenderer->draw2DGrid(grid2D);

    } else if (camera3D) {

        math::Color skyInfluence(0.5f, 0.7f, 1.0f, 1.0f);

        if (view->m_activeWorldEnvironment) {
            using namespace components;
            WorldEnvironment* worldEnv = view->m_activeWorldEnvironment;

            int skyboxType = worldEnv->GetSkyboxType();
            switch (static_cast<WorldEnvironment::SkyboxType>(skyboxType)) {
                case WorldEnvironment::SkyboxType::Color:
                    skyInfluence = worldEnv->GetSkyboxColor();
                    break;
                case WorldEnvironment::SkyboxType::Procedural:

                    skyInfluence = worldEnv->GetSkyHorizonColor();
                    break;
                case WorldEnvironment::SkyboxType::Cubemap:
                case WorldEnvironment::SkyboxType::Panoramic:

                    skyInfluence = math::Color(0.6f, 0.6f, 0.7f, 1.0f);
                    break;
                case WorldEnvironment::SkyboxType::None:
                default:

                    break;
            }
        }

        auto makeGridColor = [](const math::Color& skyColor, float brightness) -> math::Color {

            float gray = (skyColor.r + skyColor.g + skyColor.b) / 3.0f;
            math::Color desaturated(
                skyColor.r * 0.3f + gray * 0.7f,
                skyColor.g * 0.3f + gray * 0.7f,
                skyColor.b * 0.3f + gray * 0.7f,
                1.0f
            );

            return math::Color(
                desaturated.r * brightness,
                desaturated.g * brightness,
                desaturated.b * brightness,
                1.0f
            );
        };

        math::Color gridColor = makeGridColor(skyInfluence, 0.35f);
        math::Color axisColor = makeGridColor(skyInfluence, 0.55f);

        float cellSize = 1.0f;
        int cellCount = 1000;

        DebugGrid grid3D;
        grid3D.cellSize = cellSize;
        grid3D.cellCount = cellCount;
        grid3D.center = math::Vec3(0.0f, 0.0f, 0.0f);
        grid3D.gridColor = gridColor;
        grid3D.axisColor = axisColor;
        grid3D.is3D = true;

        m_debugRenderer->draw3DGrid(grid3D);
    }
}

void RenderWorld::renderDebugOverlayPostSprites(IGfxCommandList* cmd, RenderView* view, const Mat4& viewProj) {
    if (!m_debugRenderer || !view || !cmd) {

        return;
    }

    RenderCamera* camera = view->getCamera();
    if (!camera) {

        return;
    }

    DebugDraw::SetRenderer(m_debugRenderer.get());

    Camera2D* camera2D = dynamic_cast<Camera2D*>(camera);
    Camera3D* camera3D = dynamic_cast<Camera3D*>(camera);

    DebugDraw::SetCurrentCameraType(camera->getType());

    if (camera2D) {

        if (view->isCameraPreviewEnabled()) {
            float halfWidth = static_cast<float>(m_projectWindowWidth) * 0.5f;
            float halfHeight = static_cast<float>(m_projectWindowHeight) * 0.5f;
            math::AABB cameraBounds(
                math::Vec3(-halfWidth, -halfHeight, 0.0f),
                math::Vec3(halfWidth, halfHeight, 0.0f)
            );
            math::Color boundsColor(1.0f, 0.5f, 0.0f, 1.0f);
            m_debugRenderer->draw2DBoundingBox(cameraBounds, boundsColor, 0.0f);
        }

    } else if (camera3D) {

        m_debugRenderer->drawAxes(math::Vec3(0.0f, 0.0f, 0.0f), 2.0f);
    }

    std::shared_ptr<core::Node> selectedNode = view->getSelectedNode();

    if (selectedNode && selectedNode->IsActiveInHierarchy() && selectedNode->IsVisibleInHierarchy()) {
        math::Color selectionColor(1.0f, 0.6f, 0.0f, 1.0f);

        if (auto node2D = std::dynamic_pointer_cast<core::Node2D>(selectedNode)) {

            if (camera2D) {

                const auto& components = selectedNode->GetComponents();
                for (const auto& component : components) {
                    if (auto renderable = std::dynamic_pointer_cast<IRenderableComponent>(component)) {

                        float rotation = node2D->GetGlobalRotation();
                        if (std::abs(rotation) > 0.001f) {

                            math::OBB obb = renderable->getOrientedBounds();
                            if (obb.extents.x > 0.0f || obb.extents.y > 0.0f) {
                                math::Vec2 center2D(obb.center.x, obb.center.y);
                                math::Vec2 size2D(obb.extents.x * 2.0f, obb.extents.y * 2.0f);
                                m_debugRenderer->draw2DOrientedBoundingBox(center2D, size2D, rotation, selectionColor, 0.0f);
                                break;
                            }
                        } else {

                            math::AABB worldBounds = renderable->getWorldBounds();
                            math::Vec3 boundsSize = worldBounds.GetSize();

                            if (boundsSize.x > 0.0f || boundsSize.y > 0.0f) {

                                m_debugRenderer->draw2DBoundingBox(worldBounds, selectionColor, 0.0f);
                                break;
                            }
                        }
                    }
                }
            }

        } else if (auto node3D = std::dynamic_pointer_cast<core::Node3D>(selectedNode)) {

            if (camera3D) {

                const auto& components = selectedNode->GetComponents();
                bool drewBounds = false;

                for (const auto& component : components) {
                    if (auto renderable = std::dynamic_pointer_cast<IRenderableComponent>(component)) {

                        math::OBB orientedBounds = renderable->getOrientedBounds();

                        if (orientedBounds.extents.x > 0.0f || orientedBounds.extents.y > 0.0f || orientedBounds.extents.z > 0.0f) {
                            m_debugRenderer->draw3DOrientedBoundingBox(orientedBounds, selectionColor, 0.0f);
                            drewBounds = true;
                            break;
                        }
                    }
                }

                if (!drewBounds) {
                    math::Vec3 position = node3D->GetGlobalPosition();
                    math::Vec3 size(1.0f, 1.0f, 1.0f);
                    math::AABB defaultBounds(position - size * 0.5f, position + size * 0.5f);
                    m_debugRenderer->draw3DBoundingBox(defaultBounds, selectionColor, 0.0f);
                }
            }
        }

        if (view->isGizmoEnabled()) {
            DebugGizmo gizmo;
            gizmo.type = view->getGizmoType();

            gizmo.highlightAxis = view->isGizmoDragging() ? view->getGizmoDragAxis() : GizmoAxis::None;

            const auto& selectedNodes = view->getSelectedNodes();
            std::vector<std::shared_ptr<core::Node>> nodesToConsider;

            if (!selectedNodes.empty()) {
                nodesToConsider = selectedNodes;
            } else if (selectedNode) {
                nodesToConsider.push_back(selectedNode);
            }

            TransformSpace transformSpace = view->getTransformSpace();

            if (!nodesToConsider.empty()) {

                bool is2DMode = std::dynamic_pointer_cast<core::Node2D>(nodesToConsider[0]) != nullptr;

                if (is2DMode && camera2D) {

                    math::Vec2 avgPos2D(0.0f, 0.0f);
                    int validNodeCount = 0;

                    for (const auto& node : nodesToConsider) {
                        if (auto node2D = std::dynamic_pointer_cast<core::Node2D>(node)) {
                            math::Vec2 nodePos = node2D->GetGlobalPosition();

                            const auto& components = node->GetComponents();
                            for (const auto& component : components) {
                                if (auto renderable = std::dynamic_pointer_cast<IRenderableComponent>(component)) {
                                    math::AABB worldBounds = renderable->getWorldBounds();
                                    math::Vec3 boundsSize = worldBounds.GetSize();
                                    if (boundsSize.x > 0.0f || boundsSize.y > 0.0f) {
                                        math::Vec3 center = worldBounds.GetCenter();
                                        nodePos = math::Vec2(center.x, center.y);
                                        break;
                                    }
                                }
                            }

                            avgPos2D = avgPos2D + nodePos;
                            validNodeCount++;
                        }
                    }

                    if (validNodeCount > 0) {
                        avgPos2D = avgPos2D / static_cast<float>(validNodeCount);
                        gizmo.position = math::Vec3(avgPos2D.x, avgPos2D.y, 0.0f);
                        gizmo.size = 50.0f / camera2D->zoom;

                        if (nodesToConsider.size() == 1) {
                            auto& node = nodesToConsider[0];
                            if (transformSpace == TransformSpace::Local) {

                                if (auto node2D = std::dynamic_pointer_cast<core::Node2D>(node)) {
                                    gizmo.rotation2D = node2D->GetGlobalRotation();
                                }
                            } else if (transformSpace == TransformSpace::Parent) {

                                if (auto parent = node->GetParent()) {
                                    if (auto parent2D = dynamic_cast<core::Node2D*>(parent)) {
                                        gizmo.rotation2D = parent2D->GetGlobalRotation();
                                    }
                                }
                            }

                        }

                        m_debugRenderer->draw2DGizmo(gizmo);
                    }

                } else if (!is2DMode && camera3D) {

                    math::Vec3 avgPos3D(0.0f, 0.0f, 0.0f);
                    int validNodeCount = 0;

                    for (const auto& node : nodesToConsider) {
                        if (auto node3D = std::dynamic_pointer_cast<core::Node3D>(node)) {
                            avgPos3D = avgPos3D + node3D->GetGlobalPosition();
                            validNodeCount++;
                        }
                    }

                    if (validNodeCount > 0) {
                        avgPos3D = avgPos3D / static_cast<float>(validNodeCount);
                        gizmo.position = avgPos3D;

                        math::Vec3 cameraPos = camera3D->position;
                        float distanceToCamera = (gizmo.position - cameraPos).Length();
                        gizmo.size = distanceToCamera * 0.1f;

                        if (nodesToConsider.size() == 1) {
                            auto& node = nodesToConsider[0];
                            if (transformSpace == TransformSpace::Local) {

                                if (auto node3D = std::dynamic_pointer_cast<core::Node3D>(node)) {
                                    gizmo.rotation3D = node3D->GetGlobalRotation();
                                }
                            } else if (transformSpace == TransformSpace::Parent) {

                                if (auto parent = node->GetParent()) {
                                    if (auto parent3D = dynamic_cast<core::Node3D*>(parent)) {
                                        gizmo.rotation3D = parent3D->GetGlobalRotation();
                                    }
                                }
                            }

                        }

                        m_debugRenderer->draw3DGizmo(gizmo);
                    }
                }
            }
        }
    }

    const auto& selectedNodes = view->getSelectedNodes();
    if (!selectedNodes.empty()) {
        math::Color multiSelectionColor(0.5f, 0.8f, 1.0f, 1.0f);

        for (const auto& node : selectedNodes) {
            if (!node || node == selectedNode) continue;
            if (!node->IsActiveInHierarchy() || !node->IsVisibleInHierarchy()) continue;

            if (auto node2D = std::dynamic_pointer_cast<core::Node2D>(node)) {

                if (camera2D) {
                    const auto& components = node->GetComponents();
                    for (const auto& component : components) {
                        if (auto renderable = std::dynamic_pointer_cast<IRenderableComponent>(component)) {

                            float rotation = node2D->GetGlobalRotation();
                            if (std::abs(rotation) > 0.001f) {

                                math::OBB obb = renderable->getOrientedBounds();
                                if (obb.extents.x > 0.0f || obb.extents.y > 0.0f) {
                                    math::Vec2 center2D(obb.center.x, obb.center.y);
                                    math::Vec2 size2D(obb.extents.x * 2.0f, obb.extents.y * 2.0f);
                                    m_debugRenderer->draw2DOrientedBoundingBox(center2D, size2D, rotation, multiSelectionColor, 0.0f);
                                    break;
                                }
                            } else {

                                math::AABB worldBounds = renderable->getWorldBounds();
                                math::Vec3 boundsSize = worldBounds.GetSize();
                                if (boundsSize.x > 0.0f || boundsSize.y > 0.0f) {
                                    m_debugRenderer->draw2DBoundingBox(worldBounds, multiSelectionColor, 0.0f);
                                    break;
                                }
                            }
                        }
                    }
                }
            } else if (auto node3D = std::dynamic_pointer_cast<core::Node3D>(node)) {

                if (camera3D) {
                    math::AABB combinedBounds;
                    bool hasBounds = false;

                    const auto& components = node->GetComponents();
                    for (const auto& component : components) {
                        if (auto renderable = std::dynamic_pointer_cast<IRenderableComponent>(component)) {
                            math::AABB worldBounds = renderable->getWorldBounds();
                            math::Vec3 boundsSize = worldBounds.GetSize();
                            if (boundsSize.x > 0.0f || boundsSize.y > 0.0f || boundsSize.z > 0.0f) {
                                if (!hasBounds) {
                                    combinedBounds = worldBounds;
                                    hasBounds = true;
                                } else {
                                    combinedBounds.Encapsulate(worldBounds);
                                }
                            }
                        }
                    }

                    if (hasBounds) {
                        m_debugRenderer->draw3DBoundingBox(combinedBounds, multiSelectionColor, 0.0f);
                    }
                }
            }
        }
    }

    core::Scene* scene = view->getScene();
    if (scene) {
        scene->Render();
    }

    DebugDraw::ClearRenderer();
}

void RenderWorld::renderDebugOverlay(IGfxCommandList* cmd, RenderView* view, const Mat4& viewProj) {

    renderDebugGrid(cmd, view, viewProj);
    renderDebugOverlayPostSprites(cmd, view, viewProj);
}

void RenderWorld::applyPropertyOverrides(IGfxCommandList* cmd, const MaterialPropertyBlock& overrides) {
    if (!cmd || overrides.isEmpty()) {
        return;
    }

    for (const auto& [name, value] : overrides.getProperties()) {

        if (std::holds_alternative<TextureHandle>(value)) {
            TextureHandle texture = std::get<TextureHandle>(value);
            if (texture.isValid()) {

                uint32_t textureUnit = 0;
                if (name == "u_Texture" || name == "u_MainTexture" || name == "u_AlbedoTexture") {
                    textureUnit = 0;
                } else if (name == "u_NormalMap" || name == "u_NormalTexture") {
                    textureUnit = 1;
                } else if (name == "u_MetallicRoughness" || name == "u_MetallicRoughnessTexture") {
                    textureUnit = 2;
                } else if (name == "u_EmissiveTexture") {
                    textureUnit = 3;
                } else if (name == "u_AOTexture") {
                    textureUnit = 4;
                }
                cmd->bindTexture(texture, textureUnit, 0);

                cmd->setUniformInt(name.c_str(), static_cast<int>(textureUnit));
            }
        }

        else if (std::holds_alternative<Color>(value)) {
            Color color = std::get<Color>(value);
            cmd->setUniformColor(name.c_str(), color);

        }

        else if (std::holds_alternative<Vec4>(value)) {
            Vec4 vec = std::get<Vec4>(value);
            cmd->setUniformVec4(name.c_str(), vec);

        }

        else if (std::holds_alternative<Vec3>(value)) {
            Vec3 vec = std::get<Vec3>(value);
            cmd->setUniformVec3(name.c_str(), vec);
        }

        else if (std::holds_alternative<Vec2>(value)) {
            Vec2 vec = std::get<Vec2>(value);
            cmd->setUniformVec2(name.c_str(), vec);
        }

        else if (std::holds_alternative<bool>(value)) {
            bool bval = std::get<bool>(value);
            cmd->setUniformBool(name.c_str(), bval);

        }

        else if (std::holds_alternative<int>(value)) {
            int ival = std::get<int>(value);
            cmd->setUniformInt(name.c_str(), ival);
        }

        else if (std::holds_alternative<float>(value)) {
            float fval = std::get<float>(value);
            cmd->setUniformFloat(name.c_str(), fval);
        }

        else if (std::holds_alternative<std::vector<Mat4>>(value)) {
            const auto& matArray = std::get<std::vector<Mat4>>(value);
            if (!matArray.empty()) {
                cmd->setUniformMat4Array(name.c_str(), matArray.data(), matArray.size());
            }
        }
    }
}

}


#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/profiling/Profiler.hpp"
#include "lupine/rendering/PostProcessStack.hpp"
#include "lupine/rendering/CameraEffectStack.hpp"
#include "lupine/rendering/gfx/GfxDeviceFactory.hpp"
#include "lupine/rendering/debug/DebugRenderer.hpp"
#include "lupine/rendering/GizmoUtils.hpp"
#include "lupine/rendering/debug/DebugDrawQueue.hpp"
#include "lupine/rendering/debug/DebugDraw.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/DefaultShaders.hpp"
#include "lupine/rendering/ShaderTranslator.hpp"
#include "lupine/rendering/PBRMaterial.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/UILayerNode.hpp"
#include "lupine/core/Component.hpp"
#include "lupine/components/DirectionalLight3D.hpp"
#include "lupine/components/OmniLight3D.hpp"
#include "lupine/components/SpotLight3D.hpp"
#include "lupine/components/WorldEnvironment.hpp"
#include "lupine/components/SubViewport.hpp"
#include "lupine/components/UIControl.hpp"
#include "lupine/math/Camera.hpp"
#include "lupine/logger/Logger.hpp"

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <unordered_map>

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

    m_preparedScenes.clear();
    m_renderViews.clear();

    // Free all per-view geometry-batch buffer pools while the device is still alive.
    for (auto& [poolViewID, pool] : m_geometryBatchPools) {
        (void)poolViewID;
        for (GeometryBatchBuffers& slot : pool.slots) {
            if (slot.vertexBuffer.isValid()) {
                m_device->destroyBuffer(slot.vertexBuffer);
            }
            if (slot.indexBuffer.isValid()) {
                m_device->destroyBuffer(slot.indexBuffer);
            }
        }
    }
    m_geometryBatchPools.clear();
    m_drawListCache.clear();

    if (m_postProcess) {
        m_postProcess->shutdown();
        m_postProcess.reset();
    }
    if (m_cameraEffects) {
        m_cameraEffects->shutdown();
        m_cameraEffects.reset();
    }
    if (m_effectInputTarget.isValid()) {
        m_device->destroyRenderTarget(m_effectInputTarget);
        m_effectInputTarget = RenderTargetHandle();
        m_effectInputWidth = 0;
        m_effectInputHeight = 0;
    }
    if (m_hdrSceneTarget.isValid()) {
        m_device->destroyRenderTarget(m_hdrSceneTarget);
        m_hdrSceneTarget = RenderTargetHandle();
        m_hdrSceneWidth = 0;
        m_hdrSceneHeight = 0;
    }

    if (m_frameTarget.isValid()) {
        m_device->destroyRenderTarget(m_frameTarget);
        m_frameTarget = RenderTargetHandle();
        m_frameTargetWidth = 0;
        m_frameTargetHeight = 0;
    }

    if (m_sceneCaptureTarget.isValid()) {
        m_device->destroyRenderTarget(m_sceneCaptureTarget);
        m_sceneCaptureTarget = RenderTargetHandle();
        m_sceneCaptureWidth = 0;
        m_sceneCaptureHeight = 0;
    }
    m_postProcessSceneTexture = TextureHandle();

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
    m_defaultPBRInstancedMaterial = MaterialHandle();

    m_materials.clear();

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (m_lightUniformBuffers[i].isValid()) {
            m_device->destroyUniformBuffer(m_lightUniformBuffers[i]);
            m_lightUniformBuffers[i] = UniformBufferHandle();
        }
    }

    // Clean up per-view shadow maps from all views
    for (auto& [viewID, view] : m_renderViews) {
        if (view) {
            // Clean up per-view shadow maps
            for (auto& shadowMap : view->m_shadowMaps) {
                if (shadowMap.isValid()) {
                    m_device->destroyTexture(shadowMap);
                }
            }
            view->m_shadowMaps.clear();

            for (auto& shadowFB : view->m_shadowMapFramebuffers) {
                if (shadowFB.isValid()) {
                    m_device->destroyRenderTarget(shadowFB);
                }
            }
            view->m_shadowMapFramebuffers.clear();

            // Clean up per-view cube shadow maps
            for (auto& cubeMap : view->m_shadowCubeMaps) {
                if (cubeMap.isValid()) {
                    m_device->destroyTexture(cubeMap);
                }
            }
            view->m_shadowCubeMaps.clear();

            for (auto& cubeFB : view->m_shadowCubeMapFramebuffers) {
                if (cubeFB.isValid()) {
                    m_device->destroyRenderTarget(cubeFB);
                }
            }
            view->m_shadowCubeMapFramebuffers.clear();
        }
    }

    // Clean up legacy shared shadow maps (should be empty, but clean up just in case)
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

    // Clean up default sampler
    if (m_defaultSampler.isValid()) {
        m_device->destroySampler(m_defaultSampler);
        m_defaultSampler = SamplerHandle();
    }

    // Clean up shadow sampler
    if (m_shadowSampler.isValid()) {
        m_device->destroySampler(m_shadowSampler);
        m_shadowSampler = SamplerHandle();
    }

    // Clean up .lsh sampler-hint samplers (Gap G)
    for (auto& kv : m_lshSamplerCache) {
        if (kv.second.isValid()) m_device->destroySampler(kv.second);
    }
    m_lshSamplerCache.clear();

    // Clean up the fullscreen-blit resources + the mid-scene capture pong target (Gap C)
    if (m_blitPipeline.isValid()) { m_device->destroyPipeline(m_blitPipeline); m_blitPipeline = PipelineHandle(); }
    if (m_blitVertexShader.isValid()) { m_device->destroyShader(m_blitVertexShader); m_blitVertexShader = ShaderHandle(); }
    if (m_blitFragmentShader.isValid()) { m_device->destroyShader(m_blitFragmentShader); m_blitFragmentShader = ShaderHandle(); }
    if (m_blitMesh.isValid()) { m_device->destroyMesh(m_blitMesh); m_blitMesh = MeshHandle(); }
    if (m_sceneCapturePong.isValid()) { m_device->destroyRenderTarget(m_sceneCapturePong); m_sceneCapturePong = RenderTargetHandle(); }

    m_device->shutdown();
    m_device.reset();

}

GraphicsBackend RenderWorld::getBackend() const {
    return m_device ? m_device->getBackend() : GraphicsBackend::None;
}

// ===== Shader Loading Helpers =====

RenderWorld::ShaderLoadResult RenderWorld::loadShaders(const char* shaderName) {
    ShaderLoadResult result;
    result.success = false;

    if (!m_device || !shaderName) {
        return result;
    }

    GraphicsBackend backend = getBackend();

    // Try to get shader data (precompiled bytecode or source)
    // getShaderData automatically returns bytecode for backends that support it
    DefaultShaders::ShaderDataPair shaderData;
    if (!DefaultShaders::getShaderData(shaderName, backend, &shaderData)) {
        LOG_ERROR(LogCategory::Render, "Failed to get shader data for '{}'", shaderName);
        return result;
    }

    // Debug: Log shader data info (using INFO level to ensure visibility)

    // Verify shader source looks valid (should start with # for GLSL)
    if (shaderData.vertex.data && shaderData.vertex.size > 0 && !shaderData.vertex.isPrecompiled) {
        const char* src = static_cast<const char*>(shaderData.vertex.data);
        // Log first 50 chars of shader to verify it's valid
        std::string preview(src, std::min(size_t(50), shaderData.vertex.size));

    }

    // Create vertex shader
    ShaderDesc vertDesc;
    vertDesc.stage = ShaderStage::Vertex;
    vertDesc.bytecode = shaderData.vertex.data;
    vertDesc.bytecodeSize = shaderData.vertex.size;
    vertDesc.entryPoint = DefaultShaders::getVertexEntryPoint(backend);

    result.vertex = m_device->createShader(vertDesc);
    if (!result.vertex.isValid()) {
        LOG_ERROR(LogCategory::Render, "Failed to create vertex shader for '{}'", shaderName);
        return result;
    }

    // Create fragment shader
    ShaderDesc fragDesc;
    fragDesc.stage = ShaderStage::Fragment;
    fragDesc.bytecode = shaderData.fragment.data;
    fragDesc.bytecodeSize = shaderData.fragment.size;
    fragDesc.entryPoint = DefaultShaders::getFragmentEntryPoint(backend);

    result.fragment = m_device->createShader(fragDesc);
    if (!result.fragment.isValid()) {
        LOG_ERROR(LogCategory::Render, "Failed to create fragment shader for '{}'", shaderName);
        m_device->destroyShader(result.vertex);
        result.vertex = ShaderHandle();
        return result;
    }

    // Create geometry shader if available (not supported on Metal)
    if (shaderData.hasGeometry() && backend != GraphicsBackend::Metal) {
        ShaderDesc geomDesc;
        geomDesc.stage = ShaderStage::Geometry;
        geomDesc.bytecode = shaderData.geometry.data;
        geomDesc.bytecodeSize = shaderData.geometry.size;
        geomDesc.entryPoint = "main";

        result.geometry = m_device->createShader(geomDesc);
        if (!result.geometry.isValid()) {
            
            // Don't fail - geometry shader is optional
        }
    }

    result.success = true;

    if (shaderData.vertex.isPrecompiled) {
        
    } else {
        
    }

    return result;
}

void RenderWorld::destroyLoadedShaders(ShaderLoadResult& result) {
    if (!m_device) return;

    if (result.vertex.isValid()) {
        m_device->destroyShader(result.vertex);
        result.vertex = ShaderHandle();
    }
    if (result.fragment.isValid()) {
        m_device->destroyShader(result.fragment);
        result.fragment = ShaderHandle();
    }
    if (result.geometry.isValid()) {
        m_device->destroyShader(result.geometry);
        result.geometry = ShaderHandle();
    }
    result.success = false;
}

uint32_t RenderWorld::getTextureBinding(const std::string& textureName) const {
    GraphicsBackend backend = getBackend();

    // Vulkan uses specific binding slots defined in shader layouts
    // OpenGL/WebGL/DirectX use sequential slots starting at 0
    bool isVulkan = (backend == GraphicsBackend::Vulkan);

    // Albedo/main texture
    if (textureName == "u_Texture" || textureName == "u_MainTexture" ||
        textureName == "u_AlbedoTexture" || textureName == "u_FontAtlas" ||
        textureName == "albedo" || textureName == "diffuse") {
        return isVulkan ? 4 : 0;
    }
    // Metallic roughness
    if (textureName == "u_MetallicRoughness" || textureName == "u_MetallicRoughnessTexture" ||
        textureName == "metallicRoughness") {
        return isVulkan ? 5 : 1;
    }
    // Normal map
    if (textureName == "u_NormalMap" || textureName == "u_NormalTexture" ||
        textureName == "normal") {
        return isVulkan ? 6 : 2;
    }
    // Emissive
    if (textureName == "u_EmissiveTexture" || textureName == "emissive") {
        return isVulkan ? 7 : 3;
    }
    // Ambient occlusion
    if (textureName == "u_AOTexture" || textureName == "ao") {
        return isVulkan ? 10 : 4;
    }
    // Scene-capture texture for post-process UI shaders. Post-process ColorRect shaders keep
    // u_Texture (slot 0) first and declare u_SceneTexture second, so it maps to the next slot.
    if (textureName == "u_SceneTexture") {
        return isVulkan ? 5 : 1;
    }

    // Default to albedo binding for unknown textures
    return isVulkan ? 4 : 0;
}

uint32_t RenderWorld::getShadowMapBaseBinding() const {
    GraphicsBackend backend = getBackend();
    // Vulkan: shadow maps start at binding 8
    // DirectX11/12: shadow maps start at register t4 (see pbr.frag: Texture2D u_ShadowMaps[MAX_SHADOW_MAPS] : register(t4))
    // OpenGL: shadow maps start at binding 5 (after material textures 0-4)
    if (backend == GraphicsBackend::Vulkan) {
        return 8;
    } else if (backend == GraphicsBackend::DirectX11 || backend == GraphicsBackend::DirectX12) {
        return 4;  // DX11/DX12 uses t4-t11 for shadow maps
    } else {
        return 5;  // OpenGL
    }
}

uint32_t RenderWorld::getShadowCubeMapBaseBinding() const {
    GraphicsBackend backend = getBackend();
    // Vulkan: shadow cube maps use bindings 16-23 which get mapped to array binding 9
    // (see GfxCommandListVulkan::updateDescriptorSets for the mapping logic)
    // DirectX11/12: shadow cube maps start at register t12 (see pbr.frag: TextureCube u_ShadowCubeMaps[MAX_SHADOW_MAPS] : register(t12))
    // OpenGL: shadow cube maps start at binding 13 (5 + 8 shadow maps)
    // WebGL: shadow cube maps start at unit 9 (5 material + 4 shadow maps); 4 cube maps
    //        occupy units 9-12, keeping every bound unit within the WebGL2 0-15 range.
    if (backend == GraphicsBackend::Vulkan) {
        return 16;
    } else if (backend == GraphicsBackend::DirectX11 || backend == GraphicsBackend::DirectX12) {
        return 12;  // DX11/DX12 uses t12-t19 for shadow cube maps
    } else if (backend == GraphicsBackend::WebGL) {
        return 9;
    } else {
        return 13;  // OpenGL
    }
}

bool RenderWorld::ensureRenderingResourcesInitialized() {
    if (m_renderingResourcesInitialized) {
        return true;
    }

    if (!createDefaultMaterials()) {

        return false;
    }

    GraphicsBackend backend = getBackend();
    m_debugRenderer = createDebugRenderer(backend);
    if (m_debugRenderer) {
        if (!m_debugRenderer->initialize(m_device.get())) {

            m_debugRenderer.reset();
        }
    }

    // Initialize the WorldEnvironment-driven post-processing stack. Failure is
    // non-fatal: rendering proceeds without post-processing.
    m_postProcess = std::make_unique<PostProcessStack>();
    if (!m_postProcess->initialize(m_device.get())) {
        LOG_WARN(LogCategory::Render, "[RenderWorld] Post-process stack failed to initialize; effects disabled");
        m_postProcess.reset();
    }

    // Initialize the per-camera stackable effect stack (runs after post-processing).
    // Non-fatal on failure: cameras simply render without their effect chains.
    m_cameraEffects = std::make_unique<CameraEffectStack>();
    if (!m_cameraEffects->initialize(m_device.get())) {
        LOG_WARN(LogCategory::Render, "[RenderWorld] Camera effect stack failed to initialize; camera effects disabled");
        m_cameraEffects.reset();
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
        RenderView* view = it->second.get();
        if (view && m_device) {
            // Clean up per-view light uniform buffers
            for (uint32_t i = 0; i < RenderView::MAX_LIGHT_UBO_FRAMES; ++i) {
                if (view->m_lightUniformBuffers[i].isValid()) {
                    m_device->destroyUniformBuffer(view->m_lightUniformBuffers[i]);
                    view->m_lightUniformBuffers[i] = UniformBufferHandle();
                }
            }

            // Clean up per-view shadow maps
            for (auto& shadowMap : view->m_shadowMaps) {
                if (shadowMap.isValid()) {
                    m_device->destroyTexture(shadowMap);
                }
            }
            view->m_shadowMaps.clear();

            for (auto& shadowFB : view->m_shadowMapFramebuffers) {
                if (shadowFB.isValid()) {
                    m_device->destroyRenderTarget(shadowFB);
                }
            }
            view->m_shadowMapFramebuffers.clear();
            view->m_shadowMapResolutions.clear();

            // Clean up per-view cube shadow maps
            for (auto& cubeMap : view->m_shadowCubeMaps) {
                if (cubeMap.isValid()) {
                    m_device->destroyTexture(cubeMap);
                }
            }
            view->m_shadowCubeMaps.clear();

            for (auto& cubeFB : view->m_shadowCubeMapFramebuffers) {
                if (cubeFB.isValid()) {
                    m_device->destroyRenderTarget(cubeFB);
                }
            }
            view->m_shadowCubeMapFramebuffers.clear();
            view->m_shadowCubeMapResolutions.clear();
        }

        m_renderViews.erase(it);
    }

    // Drop any cached draw list for this view so a recycled view id can't reuse it.
    m_drawListCache.erase(viewID);

    // Free this view's geometry-batch buffer pool.
    auto poolIt = m_geometryBatchPools.find(viewID);
    if (poolIt != m_geometryBatchPools.end()) {
        if (m_device) {
            for (GeometryBatchBuffers& slot : poolIt->second.slots) {
                if (slot.vertexBuffer.isValid()) {
                    m_device->destroyBuffer(slot.vertexBuffer);
                }
                if (slot.indexBuffer.isValid()) {
                    m_device->destroyBuffer(slot.indexBuffer);
                }
            }
        }
        m_geometryBatchPools.erase(poolIt);
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

        // Clear active frame tracking from previous frame
        m_viewsWithActiveFrame.clear();

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
        LOG_ERROR(LogCategory::Render, "[RenderWorld] renderView: view not found for id={}", viewID);
        return;
    }

    if (!view->hasSwapchain() && !view->hasRenderTarget()) {
        LOG_ERROR(LogCategory::Render, "[RenderWorld] renderView: view {} has no swapchain and no render target", viewID);
        return;
    }

    if (!m_device) {
        LOG_ERROR(LogCategory::Render, "[RenderWorld] renderView: no device!");
        return;
    }

    // Check if graphics context is valid (handles WebGL context loss gracefully)
    if (!m_device->isContextValid()) {
        LOG_WARN(LogCategory::Render, "[RenderWorld] renderView: context not valid, skipping frame");
        return;
    }

    // Pre-upload GPU resources disabled - with NodeScatter each instance has its own
    // handles so this just moves work around rather than reducing it. The real fix
    // requires proper asset/handle sharing across instances.
    // if (view->getScene()) {
    //     prepareScene(view->getScene());
    // }

    RenderTargetHandle target;
    if (view->hasSwapchain()) {
        SwapchainHandle sc = view->getSwapchain();
        
        target = m_device->getSwapchainBackbuffer(sc);
        
    } else if (view->hasRenderTarget()) {
        target = view->getRenderTarget();
    } else {
        return;
    }

    // Track that this view will have an active frame
    // The actual beginFrame/submit happens in executeRenderPasses() to avoid
    // double beginFrame/submit which causes issues on Metal (presents empty frame)
    m_viewsWithActiveFrame.insert(viewID);

    RenderContext ctx(view, m_device.get());
    ctx.setRenderWorld(this);

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
    ctx.setTexturedAdditiveMaterial(m_defaultTextured2DAdditiveMaterial);
    ctx.setColored2DAdditiveMaterial(m_defaultColored2DAdditiveMaterial);
    ctx.setParticle3DMaterials(m_particle3DAlphaMaterial, m_particle3DAdditiveMaterial);
    ctx.setDefaultLine2DMaterial(m_defaultLine2DMaterial);
    ctx.setDefaultRoundedRectMaterial(m_defaultRoundedRectMaterial);
    ctx.setRoundedRectAdditiveMaterial(m_roundedRectAdditiveMaterial);
    ctx.setRoundedRectMultiplyMaterial(m_roundedRectMultiplyMaterial);
    ctx.setRoundedRectOpaqueMaterial(m_roundedRectOpaqueMaterial);
    ctx.setRoundedRectOverlayMaterial(m_roundedRectOverlayMaterial);
    ctx.setRoundedRectBorderMaterial(m_roundedRectBorderMaterial);
    ctx.setRoundedRect3DMaterial(m_roundedRect3DMaterial);
    ctx.setRoundedRect3DBorderMaterial(m_roundedRect3DBorderMaterial);
    ctx.setRadialGradientMaterial(m_radialGradientMaterial);
    ctx.setRadialGradientAdditiveMaterial(m_radialGradientAdditiveMaterial);
    ctx.setRadialGradientMultiplyMaterial(m_radialGradientMultiplyMaterial);
    ctx.setRadialGradientOpaqueMaterial(m_radialGradientOpaqueMaterial);
    ctx.setPolygonMaterial(m_polygonMaterial);
    ctx.setPolygonAdditiveMaterial(m_polygonAdditiveMaterial);
    ctx.setPolygonMultiplyMaterial(m_polygonMultiplyMaterial);
    ctx.setPolygonOpaqueMaterial(m_polygonOpaqueMaterial);
    ctx.setDefaultPBRMaterial(m_defaultPBRMaterial);
    ctx.setDefaultPBRInstancedMaterial(m_defaultPBRInstancedMaterial);
    ctx.setDefaultSkeletalMaterial(m_defaultSkeletalMaterial);
    ctx.setDefaultToonMaterial(m_defaultToonMaterial);
    ctx.setDefaultSkeletalToonMaterial(m_defaultSkeletalToonMaterial);
    ctx.setDefaultStylizedMaterial(m_defaultStylizedMaterial);
    ctx.setDefaultSkeletalStylizedMaterial(m_defaultSkeletalStylizedMaterial);
    ctx.setDefaultTransparentMaterial(m_defaultTransparentMaterial);
    ctx.setDefaultGlowMaterial(m_defaultGlowMaterial);
    ctx.setDefaultText3DMaterial(m_defaultText3DMaterial);

    // Register materials in the shader type registry for flexible material lookup
    // Static mesh materials
    ctx.registerMaterial(ShaderType::PBR, m_defaultPBRMaterial, false);
    ctx.registerMaterial(ShaderType::Toon, m_defaultToonMaterial, false);
    ctx.registerMaterial(ShaderType::Stylized, m_defaultStylizedMaterial, false);
    ctx.registerMaterial(ShaderType::Transparent, m_defaultTransparentMaterial, false);
    ctx.registerMaterial(ShaderType::Glow, m_defaultGlowMaterial, false);
    ctx.registerMaterial(ShaderType::Unlit, m_defaultColoredMaterial, false);
    ctx.registerMaterial(ShaderType::Standard3D, m_defaultPBRMaterial, false);

    // Skeletal mesh materials
    ctx.registerMaterial(ShaderType::PBR, m_defaultSkeletalMaterial, true);
    ctx.registerMaterial(ShaderType::Toon, m_defaultSkeletalToonMaterial, true);
    ctx.registerMaterial(ShaderType::Stylized, m_defaultSkeletalStylizedMaterial, true);
    ctx.registerMaterial(ShaderType::Transparent, m_defaultTransparentMaterial, true);  // No skeletal variant yet
    ctx.registerMaterial(ShaderType::Glow, m_defaultGlowMaterial, true);  // No skeletal variant yet
    ctx.registerMaterial(ShaderType::Unlit, m_defaultColoredMaterial, true);
    ctx.registerMaterial(ShaderType::Standard3D, m_defaultSkeletalMaterial, true);

    ctx.updateCameraMatrices();

    ctx.clear();

    // Render any embedded SubViewports within this view's scope into their own
    // off-screen targets first, so their textures are ready when this view samples
    // them. No-op when the scene contains no SubViewport components.
    renderEmbeddedSubViewports(view);

    gatherLights(view);

    gatherWorldEnvironment(view);

    // Draw-list cache: reuse the previously gathered + batched passes for this view while
    // nothing that affects them changed (render epoch, tree-structure version, camera
    // view-projection). On a hit the full scene-tree walk + RTTI + sort/merge are skipped.
    {
        const uint64_t dlEpoch = core::Node::GetRenderEpoch();
        const uint64_t dlTreeVersion = core::Node::GetTreeStructureVersion();
        const math::Mat4 dlCameraVP = ctx.getViewProjectionMatrix();
        DrawListCache& dlCache = m_drawListCache[viewID];

        const bool reuseDrawList = dlCache.valid &&
                                   dlCache.epoch == dlEpoch &&
                                   dlCache.treeVersion == dlTreeVersion &&
                                   dlCache.cameraVP == dlCameraVP;

        if (reuseDrawList) {
            // The cached batches' const DrawItem* reference dlCache.drawItems (this
            // RenderWorld member's stable heap storage), so reusing them never dangles.
            m_renderPasses = dlCache.passes;
            // A list is only cached when the gather found no dynamic renderable, so the
            // shadow cache (which reads this flag) is correct to treat the view as static.
            m_AnyDynamicRenderableGathered = false;
        } else {
            gatherRenderables(view, ctx);

            m_renderPasses.clear();
            buildRenderBatches(ctx.getDrawItems(), m_renderPasses, &m_geometryBatchPools[viewID]);

            if (!m_AnyDynamicRenderableGathered) {
                // RenderBatch::items point into ctx's per-frame DrawItem storage, which is
                // freed when this frame's RenderContext is destroyed. Copy the DrawItems by
                // value (DrawItem is self-contained) and rebase every batch pointer onto the
                // cache's own storage so a future reuse frame doesn't dereference freed memory.
                const std::vector<DrawItem>& srcItems = ctx.getDrawItems();
                dlCache.drawItems = srcItems;
                const DrawItem* srcBase = srcItems.data();
                const DrawItem* dstBase = dlCache.drawItems.data();
                const size_t itemCount = dlCache.drawItems.size();

                dlCache.passes = m_renderPasses;
                bool rebaseOk = true;
                for (RenderPass& pass : dlCache.passes) {
                    for (RenderBatch& batch : pass.batches) {
                        for (const DrawItem*& itemPtr : batch.items) {
                            if (!itemPtr) {
                                continue;
                            }
                            const size_t idx = static_cast<size_t>(itemPtr - srcBase);
                            if (idx >= itemCount) {
                                rebaseOk = false;  // pointer outside ctx storage - unexpected
                                break;
                            }
                            itemPtr = dstBase + idx;
                        }
                        if (!rebaseOk) break;
                    }
                    if (!rebaseOk) break;
                }

                if (rebaseOk) {
                    // Re-sample AFTER the gather so any lazy GPU upload done in
                    // buildDrawCommands (which would advance the epoch) is captured.
                    dlCache.epoch = core::Node::GetRenderEpoch();
                    dlCache.treeVersion = core::Node::GetTreeStructureVersion();
                    dlCache.cameraVP = dlCameraVP;
                    dlCache.valid = true;
                } else {
                    // Don't cache rather than risk a dangling pointer; re-gather next frame.
                    dlCache.valid = false;
                    dlCache.passes.clear();
                    dlCache.drawItems.clear();
                }
            } else {
                dlCache.valid = false;
            }
        }
    }

    {
        size_t totalBatchItems = 0;
        size_t totalBatches = 0;
        for (const auto& pass : m_renderPasses) {
            totalBatches += pass.batches.size();
            for (const auto& batch : pass.batches) {
                totalBatchItems += batch.items.size();
            }
        }
    }

    // Early-out if GPU hung during a previous frame — isContextValid() returns false
    if (!m_device->isContextValid()) {
        return;
    }

    renderShadowMaps(view, ctx);

    static int renderViewCount = 0;
    renderViewCount++;
    bool debugRenderView = renderViewCount <= 5;

    if (debugRenderView) {

    }

    // Set ambient light and fog from WorldEnvironment BEFORE uploading UBO
    // This was previously only done in uploadLightData() which wasn't called from this path
    if (view->m_lightUniformData) {
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
            // Default ambient when no WorldEnvironment is present
            view->m_lightUniformData->ambientLight = Vec4(0.1f, 0.1f, 0.1f, 1.0f);
            view->m_lightUniformData->fogColor.w = 0.0f;
        }
    }

    // Create per-view ring-buffered light UBOs if not yet created
    // Each view needs its own GPU buffers to prevent conflicts when multiple scenes render concurrently
    if (!view->m_lightUniformBuffers[0].isValid()) {
        for (uint32_t i = 0; i < RenderView::MAX_LIGHT_UBO_FRAMES; ++i) {
            view->m_lightUniformBuffers[i] = m_device->createUniformBuffer(sizeof(LightUniformBuffer));
            if (!view->m_lightUniformBuffers[i].isValid()) {
                
            }
        }
    }

    if (debugRenderView) {

    }

    // Update the current frame's light buffer and advance the ring buffer index
    // This ensures CPU updates don't race with GPU reads from previous frames
    // Using per-view buffers prevents conflicts between concurrent scenes
    UniformBufferHandle currentLightBuffer = view->m_lightUniformBuffers[view->m_currentLightBufferIndex];
    if (currentLightBuffer.isValid() && view->m_lightUniformData) {
        m_device->updateUniformBuffer(currentLightBuffer, view->m_lightUniformData, sizeof(LightUniformBuffer));
    }

    if (debugRenderView) {

    }

    // WorldEnvironment-driven post-processing: if the view's active WorldEnvironment
    // requests any effect, render the scene into an HDR off-screen target and run the
    // composable post-process chain (bloom/tonemap/grading/SSAO/vignette/overlay/...)
    // into the real target. Otherwise fall back to the scene-grab path (u_SceneTexture
    // UI shaders) or a plain direct render.
    PostProcessSettings ppSettings;
    bool ppActive = m_postProcess &&
                    gatherPostProcessSettings(view, ppSettings) &&
                    m_postProcess->isActive(ppSettings);

    // Per-camera stackable effects run after the WorldEnvironment post-process, using
    // the same HDR scene capture. When both are active the post-process writes an HDR
    // intermediate that the effect chain then consumes into the real target.
    CameraEffectChain effectChain;
    bool effectsActive = m_cameraEffects &&
                         gatherCameraEffects(view, effectChain) &&
                         m_cameraEffects->isActive(effectChain);

    bool ppHandled = false;
    if (ppActive || effectsActive) {
        const Viewport& ppVp = view->getViewport();
        uint32_t ppW = static_cast<uint32_t>(ppVp.width);
        uint32_t ppH = static_cast<uint32_t>(ppVp.height);
        if (ppW > 0 && ppH > 0 && ensureHdrSceneTarget(ppW, ppH)) {
            // For camera effects the HDR target is an isolated capture of THIS camera's
            // layer. Non-first cameras clear depth-only (they composite onto earlier
            // cameras), so their color is never cleared by the scene pass — pre-clear it
            // to transparent here so the layer's empty regions carry alpha 0 and blend
            // through to the cameras beneath. First cameras re-clear to the opaque clear
            // color during the scene pass, so this is harmless for them.
            if (effectsActive) {
                if (auto clr = m_device->beginFrame(m_hdrSceneTarget)) {
                    clr->setViewport(Viewport{0.0f, 0.0f, static_cast<float>(ppW), static_cast<float>(ppH), 0.0f, 1.0f});
                    clr->setScissor(ScissorRect{0, 0, ppW, ppH});
                    clr->clearColor(math::Color(0.0f, 0.0f, 0.0f, 0.0f));
                    clr->clearDepth(1.0f);
                    m_device->submit(std::move(clr));
                }
            }

            renderSceneInto(view, ctx, m_hdrSceneTarget);

            RenderTargetHandle finalTarget;
            if (view->hasSwapchain()) {
                finalTarget = m_device->getSwapchainBackbuffer(view->getSwapchain());
            } else {
                finalTarget = view->getRenderTarget();
            }

            TextureHandle hdrColor = m_device->getRenderTargetColorTexture(m_hdrSceneTarget);
            TextureHandle hdrDepth = m_device->getRenderTargetDepthTexture(m_hdrSceneTarget);

            if (ppActive && effectsActive && ensureEffectInputTarget(ppW, ppH)) {
                m_postProcess->execute(m_effectInputTarget, hdrColor, hdrDepth, ppW, ppH, ppSettings);
                TextureHandle ppColor = m_device->getRenderTargetColorTexture(m_effectInputTarget);
                m_cameraEffects->execute(finalTarget, ppColor, hdrDepth, ppW, ppH, effectChain);
            } else if (ppActive) {
                m_postProcess->execute(finalTarget, hdrColor, hdrDepth, ppW, ppH, ppSettings);
            } else {
                m_cameraEffects->execute(finalTarget, hdrColor, hdrDepth, ppW, ppH, effectChain);
            }
            ppHandled = true;
        }
    }

    if (!ppHandled) {
        // If any draw uses a post-process (u_SceneTexture) material, capture the scene first and
        // composite; otherwise render normally.
        if (!renderViewPostProcess(view, ctx)) {
            executeRenderPasses(view, m_renderPasses, ctx);
        }
    }

    if (debugRenderView) {

    }

    // Advance to next buffer for next frame (per-view to prevent cross-scene conflicts)
    view->m_currentLightBufferIndex = (view->m_currentLightBufferIndex + 1) % RenderView::MAX_LIGHT_UBO_FRAMES;
}

bool RenderWorld::ensureSceneCaptureTarget(uint32_t width, uint32_t height) {
    if (!m_device || width == 0 || height == 0) {
        return false;
    }
    if (m_sceneCaptureTarget.isValid() && m_sceneCaptureWidth == width && m_sceneCaptureHeight == height) {
        return true;
    }
    if (m_sceneCaptureTarget.isValid()) {
        m_device->destroyRenderTarget(m_sceneCaptureTarget);
        m_sceneCaptureTarget = RenderTargetHandle();
    }

    RenderTargetDesc desc;
    desc.width = width;
    desc.height = height;
    // Defaults: RGBA8 color + depth/stencil, single sample. The scene (incl. 3D) renders here,
    // so depth is required.
    m_sceneCaptureTarget = m_device->createRenderTarget(desc);
    if (!m_sceneCaptureTarget.isValid()) {
        m_sceneCaptureWidth = 0;
        m_sceneCaptureHeight = 0;
        return false;
    }
    m_sceneCaptureWidth = width;
    m_sceneCaptureHeight = height;
    return true;
}

bool RenderWorld::ensureSceneCapturePong(uint32_t width, uint32_t height) {
    if (!m_device || width == 0 || height == 0) {
        return false;
    }
    if (m_sceneCapturePong.isValid() && m_sceneCapturePongWidth == width && m_sceneCapturePongHeight == height) {
        return true;
    }
    if (m_sceneCapturePong.isValid()) {
        m_device->destroyRenderTarget(m_sceneCapturePong);
        m_sceneCapturePong = RenderTargetHandle();
    }
    RenderTargetDesc desc;
    desc.width = width;
    desc.height = height;
    desc.hasColor = true;
    desc.hasDepth = false;  // snapshot is color-only; sampled as u_SceneTexture
    desc.sampleCount = 1;
    m_sceneCapturePong = m_device->createRenderTarget(desc);
    if (!m_sceneCapturePong.isValid()) {
        m_sceneCapturePongWidth = 0;
        m_sceneCapturePongHeight = 0;
        return false;
    }
    m_sceneCapturePongWidth = width;
    m_sceneCapturePongHeight = height;
    return true;
}

// Embedded copy shader for blitTexture (translated at runtime like the post-process
// shaders). u_Texture maps to the standard first texture slot via getTextureBinding.
static const char* kBlitCopyLsh = R"LSH(
#shader "LshBlitCopy"
#description "Fullscreen texture copy (blit)"

#begin properties
    uniform float u_FlipV = 0.0;
    uniform sampler2D u_Texture;
#end properties

#begin vertex
    #input vec3 a_Position : POSITION 0
    #input vec3 a_Normal : NORMAL 1
    #input vec2 a_TexCoord : TEXCOORD0 2
    #input vec4 a_Color : COLOR 3
    #output vec2 v_TexCoord
    void main() {
        vec2 uv = a_Position.xy * 0.5 + 0.5;
        uv.y = mix(uv.y, 1.0 - uv.y, u_FlipV);
        v_TexCoord = uv;
        VERTEX_OUTPUT = vec4(a_Position.x, a_Position.y, 0.0, 1.0);
    }
#end vertex

#begin fragment
    #output vec4 FragColor
    void main() {
        FragColor = SAMPLE(u_Texture, v_TexCoord);
    }
#end fragment
)LSH";

bool RenderWorld::ensureBlitResources() {
    if (!m_device) {
        return false;
    }
    if (!m_blitMesh.isValid()) {
        MeshData data;
        data.vertices.resize(4);
        data.vertices[0].position = Vec3(-1.0f, -1.0f, 0.0f); data.vertices[0].texCoord = Vec2(0.0f, 0.0f);
        data.vertices[1].position = Vec3( 1.0f, -1.0f, 0.0f); data.vertices[1].texCoord = Vec2(1.0f, 0.0f);
        data.vertices[2].position = Vec3( 1.0f,  1.0f, 0.0f); data.vertices[2].texCoord = Vec2(1.0f, 1.0f);
        data.vertices[3].position = Vec3(-1.0f,  1.0f, 0.0f); data.vertices[3].texCoord = Vec2(0.0f, 1.0f);
        for (auto& v : data.vertices) { v.normal = Vec3(0.0f, 0.0f, 1.0f); v.color = Vec4(1.0f, 1.0f, 1.0f, 1.0f); }
        data.indices = {0, 1, 2, 0, 2, 3};
        data.addSubmesh(6, 4);
        data.calculateBounds();
        m_blitMesh = m_device->createMesh(data);
        if (!m_blitMesh.isValid()) return false;
    }
    if (!m_blitPipeline.isValid()) {
        ShaderTranslatorResult tr = ShaderTranslator::translate(kBlitCopyLsh, getBackend());
        if (!tr.success) return false;
        const std::string& vsSrc = (getBackend() == GraphicsBackend::Metal) ? tr.combinedSource : tr.vertexSource;
        const std::string& fsSrc = (getBackend() == GraphicsBackend::Metal) ? tr.combinedSource : tr.fragmentSource;
        ShaderDesc vd; vd.stage = ShaderStage::Vertex; vd.bytecode = vsSrc.c_str(); vd.bytecodeSize = vsSrc.size();
        m_blitVertexShader = m_device->createShader(vd);
        ShaderDesc fd; fd.stage = ShaderStage::Fragment; fd.bytecode = fsSrc.c_str(); fd.bytecodeSize = fsSrc.size();
        m_blitFragmentShader = m_device->createShader(fd);
        if (!m_blitVertexShader.isValid() || !m_blitFragmentShader.isValid()) return false;

        VertexBufferLayout layout;
        layout.stride = sizeof(Vertex);
        layout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        layout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        layout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        layout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});

        PipelineDesc desc;
        desc.shaders = {m_blitVertexShader, m_blitFragmentShader};
        desc.vertexLayout = layout;
        desc.topology = PrimitiveTopology::TriangleList;
        desc.blendState = BlendState::opaque();
        desc.depthStencilState = DepthStencilState::noDepth();
        desc.rasterizerState.cullMode = CullMode::None;
        desc.rasterizerState.fillMode = FillMode::Solid;
        m_blitPipeline = m_device->createPipeline(desc);
        if (!m_blitPipeline.isValid()) return false;
    }
    return true;
}

void RenderWorld::blitTexture(TextureHandle src, RenderTargetHandle dst, uint32_t width,
                              uint32_t height, bool flipV) {
    if (!m_device || !src.isValid() || !dst.isValid() || width == 0 || height == 0) {
        return;
    }
    if (!ensureBlitResources()) {
        return;
    }
    auto cmd = m_device->beginFrame(dst);
    if (!cmd) {
        return;
    }
    Viewport vp{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    cmd->setViewport(vp);
    cmd->setScissor(ScissorRect{0, 0, width, height});
    cmd->beginDebugMarker("RenderWorld.Blit");
    cmd->bindPipeline(m_device->getColorFormatVariant(
        m_blitPipeline, m_device->getRenderTargetColorFormat(dst)));
    const uint32_t slot = getTextureBinding("u_Texture");
    if (m_defaultSampler.isValid()) {
        cmd->bindSampler(m_defaultSampler, slot);
    }
    cmd->bindTexture(src, slot, 0);
    cmd->setUniformInt("u_Texture", static_cast<int>(slot));
    cmd->setUniformFloat("u_FlipV", flipV ? 1.0f : 0.0f);
    const GPUMesh* mesh = m_device->getMesh(m_blitMesh);
    if (mesh) {
        cmd->bindVertexBuffer(mesh->vertexBuffer, 0);
        cmd->bindIndexBuffer(mesh->indexBuffer, IndexFormat::UInt32);
        cmd->drawIndexed(mesh->indexCount, 1, 0, 0, 0);
    }
    cmd->endDebugMarker();
    m_device->submit(std::move(cmd));
}

bool RenderWorld::renderViewPostProcess(RenderView* view, RenderContext& ctx) {
    if (!m_device || !view) {
        return false;
    }

    // Detect any post-process (u_SceneTexture) material in this view's batches.
    bool hasPostProcess = false;
    for (const auto& pass : m_renderPasses) {
        for (const auto& batch : pass.batches) {
            const Material* mat = getMaterial(batch.material);
            if (mat && mat->usesSceneTexture) {
                hasPostProcess = true;
                break;
            }
        }
        if (hasPostProcess) break;
    }
    if (!hasPostProcess) {
        return false;  // Nothing to capture; caller uses the normal path.
    }

    const Viewport& vp = view->getViewport();
    uint32_t width = static_cast<uint32_t>(vp.width);
    uint32_t height = static_cast<uint32_t>(vp.height);
    if (!ensureSceneCaptureTarget(width, height)) {
        return false;  // Couldn't make a capture target; fall back to normal rendering.
    }

    // ----- Phase 1: render the scene (excluding grab draws) into the capture target -----
    // Also collect the grab batches (those that sample u_SceneTexture) in draw order so the
    // ordered path can replay them one at a time.
    struct GrabEntry { RenderPassType type; RenderBatch batch; };
    std::vector<RenderPass> scenePasses;
    std::vector<GrabEntry> grabBatches;
    scenePasses.reserve(m_renderPasses.size());
    for (const auto& pass : m_renderPasses) {
        RenderPass filtered;
        filtered.type = pass.type;
        for (const auto& batch : pass.batches) {
            const Material* mat = getMaterial(batch.material);
            if (mat && mat->usesSceneTexture) {
                grabBatches.push_back({pass.type, batch});  // replayed in order below
                continue;
            }
            filtered.batches.push_back(batch);
        }
        scenePasses.push_back(std::move(filtered));
    }

    // NOTE: setSwapchain() and setRenderTarget() are mutually exclusive — each clears the other.
    // So we point the view at the capture target (which clears the swapchain), then restore ONLY
    // the originally-active backing afterwards.
    SwapchainHandle savedSwapchain = view->getSwapchain();
    RenderTargetHandle savedTarget = view->getRenderTarget();
    auto restoreBacking = [&]() {
        if (savedSwapchain.isValid()) {
            view->setSwapchain(savedSwapchain);
        } else {
            view->setRenderTarget(savedTarget);
        }
    };

    view->setRenderTarget(m_sceneCaptureTarget);    // also clears the swapchain -> RT path
    executeRenderPasses(view, scenePasses, ctx);

    // ----- Phase 2 -----
    // Fast path (0 or 1 grab "layer"): re-render the full scene + grab draws into the real
    // target with the captured scene bound as u_SceneTexture. Identical to the original
    // behaviour, and the common case (a single refraction/distortion object or independent
    // grabbers that only need to see the scene behind them).
    if (grabBatches.size() <= 1 || !ensureSceneCapturePong(width, height)) {
        restoreBacking();
        m_postProcessSceneTexture = m_device->getRenderTargetColorTexture(m_sceneCaptureTarget);
        executeRenderPasses(view, m_renderPasses, ctx);
        m_postProcessSceneTexture = TextureHandle();
        return true;
    }

    // Ordered path (2+ grab layers): the capture target already holds the scene without any
    // grab draws. Draw the grab batches onto it ONE AT A TIME, each sampling a snapshot of
    // everything rendered before it (scene + earlier grab objects), so later grabbers refract
    // earlier ones. Finally blit the accumulated result to the real backing.
    for (const GrabEntry& ge : grabBatches) {
        // Snapshot of the scene-so-far. The capture<->pong blit is an identity RT copy
        // (flipV=false), so the grab shader samples the snapshot exactly as it would the
        // capture target directly (the fast path's behaviour).
        blitTexture(m_device->getRenderTargetColorTexture(m_sceneCaptureTarget),
                    m_sceneCapturePong, width, height, false);
        m_postProcessSceneTexture = m_device->getRenderTargetColorTexture(m_sceneCapturePong);

        RenderPass grabPass;
        grabPass.type = ge.type;
        grabPass.batches.push_back(ge.batch);
        std::vector<RenderPass> grabOnly{std::move(grabPass)};

        view->setRenderTarget(m_sceneCaptureTarget);
        executeRenderPasses(view, grabOnly, ctx, /*skipClear*/ true);  // draw on top
        m_postProcessSceneTexture = TextureHandle();
    }

    // Present the accumulated capture to the real backing. NDC y=-1 maps to the framebuffer
    // bottom while RT texel (0,0) is at the backend's origin, so a presenting blit flips on
    // top-left-origin backends — exactly NeedsProjectionYFlip.
    restoreBacking();
    RenderTargetHandle realBacking = savedSwapchain.isValid()
        ? m_device->getSwapchainBackbuffer(savedSwapchain)
        : savedTarget;
    blitTexture(m_device->getRenderTargetColorTexture(m_sceneCaptureTarget), realBacking,
                width, height, NeedsProjectionYFlip(getBackend()));
    return true;
}

bool RenderWorld::ensureEffectInputTarget(uint32_t width, uint32_t height) {
    if (!m_device || width == 0 || height == 0) {
        return false;
    }
    if (m_effectInputTarget.isValid() && m_effectInputWidth == width && m_effectInputHeight == height) {
        return true;
    }
    if (m_effectInputTarget.isValid()) {
        m_device->destroyRenderTarget(m_effectInputTarget);
        m_effectInputTarget = RenderTargetHandle();
    }

    RenderTargetDesc desc;
    desc.width = width;
    desc.height = height;
    // HDR color (no depth needed) so the post-process result feeds the effect chain
    // without clamping intermediate values.
    desc.colorFormat = TextureFormat::RGBA16_FLOAT;
    desc.hasColor = true;
    desc.hasDepth = false;
    desc.sampleCount = 1;
    m_effectInputTarget = m_device->createRenderTarget(desc);
    if (!m_effectInputTarget.isValid()) {
        m_effectInputWidth = 0;
        m_effectInputHeight = 0;
        return false;
    }
    m_effectInputWidth = width;
    m_effectInputHeight = height;
    return true;
}

bool RenderWorld::ensureHdrSceneTarget(uint32_t width, uint32_t height) {
    if (!m_device || width == 0 || height == 0) {
        return false;
    }
    if (m_hdrSceneTarget.isValid() && m_hdrSceneWidth == width && m_hdrSceneHeight == height) {
        return true;
    }
    if (m_hdrSceneTarget.isValid()) {
        m_device->destroyRenderTarget(m_hdrSceneTarget);
        m_hdrSceneTarget = RenderTargetHandle();
    }

    RenderTargetDesc desc;
    desc.width = width;
    desc.height = height;
    // HDR color so bloom/tonemapping see values above 1.0, plus a sampleable depth
    // attachment so SSAO can reconstruct view-space positions from scene depth.
    desc.colorFormat = TextureFormat::RGBA16_FLOAT;
    desc.hasColor = true;
    desc.hasDepth = true;
    desc.sampleCount = 1;
    m_hdrSceneTarget = m_device->createRenderTarget(desc);
    if (!m_hdrSceneTarget.isValid()) {
        m_hdrSceneWidth = 0;
        m_hdrSceneHeight = 0;
        return false;
    }
    m_hdrSceneWidth = width;
    m_hdrSceneHeight = height;
    return true;
}

bool RenderWorld::ensureFrameTarget(uint32_t width, uint32_t height) {
    if (!m_device || width == 0 || height == 0) {
        return false;
    }
    if (m_frameTarget.isValid() && m_frameTargetWidth == width && m_frameTargetHeight == height) {
        return true;
    }
    if (m_frameTarget.isValid()) {
        m_device->destroyRenderTarget(m_frameTarget);
        m_frameTarget = RenderTargetHandle();
    }

    RenderTargetDesc desc;
    desc.width = width;
    desc.height = height;
    // Plain UNORM colour (see m_frameTargetFormat note): the scene is stored verbatim so
    // the present blit reproduces the direct-render result without a second sRGB encode.
    // A depth attachment lets the world passes depth-test as they would against the
    // swapchain.
    desc.colorFormat = m_frameTargetFormat;
    desc.hasColor = true;
    desc.hasDepth = true;
    desc.sampleCount = 1;
    m_frameTarget = m_device->createRenderTarget(desc);
    if (!m_frameTarget.isValid()) {
        m_frameTargetWidth = 0;
        m_frameTargetHeight = 0;
        return false;
    }
    m_frameTargetWidth = width;
    m_frameTargetHeight = height;
    return true;
}

void RenderWorld::presentFrameTargetScaled(SwapchainHandle swapchain, const Viewport& dstViewport,
                                           uint32_t fullWidth, uint32_t fullHeight) {
    if (!m_device || !m_frameTarget.isValid() || !swapchain.isValid() ||
        fullWidth == 0 || fullHeight == 0) {
        return;
    }
    if (!ensureBlitResources()) {
        return;
    }
    RenderTargetHandle backbuffer = m_device->getSwapchainBackbuffer(swapchain);
    if (!backbuffer.isValid()) {
        return;
    }
    TextureHandle src = m_device->getRenderTargetColorTexture(m_frameTarget);
    if (!src.isValid()) {
        return;
    }

    auto cmd = m_device->beginFrame(backbuffer);
    if (!cmd) {
        return;
    }

    // Clear the whole backbuffer first so any letterbox bars are black.
    cmd->setViewport(Viewport{0.0f, 0.0f, static_cast<float>(fullWidth),
                              static_cast<float>(fullHeight), 0.0f, 1.0f});
    cmd->setScissor(ScissorRect{0, 0, fullWidth, fullHeight});
    cmd->clearColor(math::Color(0.0f, 0.0f, 0.0f, 1.0f));

    // Downscale the supersampled frame into the destination (letterbox) viewport with
    // linear filtering. NDC y=-1 maps to the framebuffer bottom while RT texel (0,0) is
    // at the backend origin, so a presenting blit flips on top-left-origin backends -
    // matching the post-process present path.
    cmd->setViewport(dstViewport);
    cmd->setScissor(ScissorRect{
        static_cast<int32_t>(dstViewport.x),
        static_cast<int32_t>(dstViewport.y),
        static_cast<uint32_t>(dstViewport.width),
        static_cast<uint32_t>(dstViewport.height)});
    cmd->beginDebugMarker("RenderWorld.PresentScaled");
    cmd->bindPipeline(m_device->getColorFormatVariant(
        m_blitPipeline, m_device->getRenderTargetColorFormat(backbuffer)));
    const uint32_t slot = getTextureBinding("u_Texture");
    if (m_defaultSampler.isValid()) {
        cmd->bindSampler(m_defaultSampler, slot);
    }
    cmd->bindTexture(src, slot, 0);
    cmd->setUniformInt("u_Texture", static_cast<int>(slot));
    cmd->setUniformFloat("u_FlipV", NeedsProjectionYFlip(getBackend()) ? 1.0f : 0.0f);
    const GPUMesh* mesh = m_device->getMesh(m_blitMesh);
    if (mesh) {
        cmd->bindVertexBuffer(mesh->vertexBuffer, 0);
        cmd->bindIndexBuffer(mesh->indexBuffer, IndexFormat::UInt32);
        cmd->drawIndexed(mesh->indexCount, 1, 0, 0, 0);
    }
    cmd->endDebugMarker();
    m_device->submit(std::move(cmd));
}

void RenderWorld::renderSceneInto(RenderView* view, RenderContext& ctx, RenderTargetHandle target) {
    if (!m_device || !view || !target.isValid()) {
        return;
    }

    // Detect a screen-grab (u_SceneTexture) material so those UI shaders keep working
    // even when the full post-process stack is active.
    bool hasGrab = false;
    for (const auto& pass : m_renderPasses) {
        for (const auto& batch : pass.batches) {
            const Material* mat = getMaterial(batch.material);
            if (mat && mat->usesSceneTexture) {
                hasGrab = true;
                break;
            }
        }
        if (hasGrab) break;
    }

    // setSwapchain()/setRenderTarget() are mutually exclusive; save the original backing
    // and restore exactly it afterward.
    SwapchainHandle savedSwapchain = view->getSwapchain();
    RenderTargetHandle savedTarget = view->getRenderTarget();
    auto restoreBacking = [&]() {
        if (savedSwapchain.isValid()) {
            view->setSwapchain(savedSwapchain);
        } else {
            view->setRenderTarget(savedTarget);
        }
    };

    const Viewport& vp = view->getViewport();
    uint32_t width = static_cast<uint32_t>(vp.width);
    uint32_t height = static_cast<uint32_t>(vp.height);

    if (hasGrab && ensureSceneCaptureTarget(width, height)) {
        // Phase 1: scene minus the grab draws -> scene-capture target.
        std::vector<RenderPass> scenePasses;
        scenePasses.reserve(m_renderPasses.size());
        for (const auto& pass : m_renderPasses) {
            RenderPass filtered;
            filtered.type = pass.type;
            for (const auto& batch : pass.batches) {
                const Material* mat = getMaterial(batch.material);
                if (mat && mat->usesSceneTexture) {
                    continue;
                }
                filtered.batches.push_back(batch);
            }
            scenePasses.push_back(std::move(filtered));
        }

        view->setRenderTarget(m_sceneCaptureTarget);
        executeRenderPasses(view, scenePasses, ctx);

        // Phase 2: full scene + grab draws -> requested target, with the captured
        // scene bound as u_SceneTexture.
        view->setRenderTarget(target);
        m_postProcessSceneTexture = m_device->getRenderTargetColorTexture(m_sceneCaptureTarget);
        executeRenderPasses(view, m_renderPasses, ctx);
        m_postProcessSceneTexture = TextureHandle();
    } else {
        view->setRenderTarget(target);
        executeRenderPasses(view, m_renderPasses, ctx);
    }

    restoreBacking();
}

bool RenderWorld::gatherPostProcessSettings(RenderView* view, PostProcessSettings& out) {
    if (!view) {
        return false;
    }
    using namespace components;
    WorldEnvironment* we = view->m_activeWorldEnvironment;
    if (!we || !we->GetPostProcessingEnabled()) {
        return false;
    }

    out = PostProcessSettings();
    out.enabled = true;

    // Tonemapping / exposure
    out.tonemap = static_cast<TonemapMode>(we->GetTonemapMode());
    out.exposure = we->GetExposure();
    out.whitePoint = we->GetWhitePoint();

    // Bloom
    out.bloomEnabled = we->GetBloomEnabled();
    out.bloomThreshold = we->GetBloomThreshold();
    out.bloomSoftKnee = we->GetBloomSoftKnee();
    out.bloomIntensity = we->GetBloomIntensity();
    out.bloomIterations = we->GetBloomIterations();

    // SSAO
    out.ssaoEnabled = we->GetSSAOEnabled();
    out.ssaoRadius = we->GetSSAORadius();
    out.ssaoIntensity = we->GetSSAOIntensity();
    out.ssaoBias = we->GetSSAOBias();
    out.ssaoSamples = we->GetSSAOSamples();
    out.ssaoPower = we->GetSSAOPower();

    // Color grading
    out.colorGradingEnabled = we->GetColorGradingEnabled();
    out.contrast = we->GetContrast();
    out.saturation = we->GetSaturation();
    out.brightness = we->GetBrightnessAdjust();
    out.temperature = we->GetTemperature();
    out.tint = we->GetTintAdjust();
    {
        math::Color cf = we->GetColorFilter();
        out.colorFilter = Vec4(cf.r, cf.g, cf.b, cf.a);
        math::Color lift = we->GetColorLift();
        out.lift = Vec3(lift.r, lift.g, lift.b);
        math::Color gamma = we->GetColorGamma();
        out.gamma = Vec3(gamma.r, gamma.g, gamma.b);
        math::Color gain = we->GetColorGain();
        out.gain = Vec3(gain.r, gain.g, gain.b);
    }

    // Vignette
    out.vignetteEnabled = we->GetVignetteEnabled();
    {
        math::Color vc = we->GetVignetteColor();
        out.vignetteColor = Vec4(vc.r, vc.g, vc.b, vc.a);
    }
    out.vignetteIntensity = we->GetVignetteIntensity();
    out.vignetteSmoothness = we->GetVignetteSmoothness();
    out.vignetteRoundness = we->GetVignetteRoundness();
    out.vignetteCenter = Vec2(we->GetVignetteCenterX(), we->GetVignetteCenterY());

    // Chromatic aberration
    out.chromaticAberrationEnabled = we->GetChromaticAberrationEnabled();
    out.chromaticAberrationAmount = we->GetChromaticAberrationAmount();

    // Film grain
    out.grainEnabled = we->GetFilmGrainEnabled();
    out.grainIntensity = we->GetFilmGrainIntensity();
    out.grainSize = we->GetFilmGrainSize();

    // Overlay
    out.overlayBlend = static_cast<OverlayBlendMode>(we->GetOverlayBlendMode());
    out.overlayOpacity = we->GetOverlayOpacity();
    we->EnsurePostProcessResourcesCreated(m_device.get());
    out.overlayTexture = we->GetOverlayTextureHandle();

    // Backend orientation override
    out.flipY = static_cast<FlipYMode>(we->GetPostFlipYMode());

    // Renderer-owned data (camera + timing).
    out.time = static_cast<float>(m_frameNumber) * 0.0166667f;
    if (RenderCamera* cam = view->getCamera()) {
        out.cameraPerspective = (cam->getType() == CameraType::Camera3D);
        float aspect = view->getAspectRatio();
        out.projection = cam->getProjectionMatrix(aspect);
        out.invProjection = out.projection.Inverse();
        if (Camera3D* cam3d = dynamic_cast<Camera3D*>(cam)) {
            out.cameraNear = cam3d->nearPlane;
            out.cameraFar = cam3d->farPlane;
        }
    }

    return true;
}

void RenderWorld::renderTexturedQuad(
    TextureHandle texture,
    SwapchainHandle swapchain,
    const math::Color& tint,
    const math::Vec2& position,
    const math::Vec2& size,
    int windowWidth,
    int windowHeight)
{
    if (!m_device || !texture.isValid() || !swapchain.isValid()) {
        return;
    }

    // Create a temporary canvas camera for 2D rendering
    auto camera = std::make_unique<CameraCanvas>();
    camera->canvasSize = Vec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    camera->clearFlags = CameraClearFlags::All;
    camera->clearColor = math::Color(0.0f, 0.0f, 0.0f, 1.0f);

    // Create render view
    RenderViewID viewID = createRenderView(std::move(camera));
    if (viewID == 0) {
        return;
    }

    RenderView* view = getRenderView(viewID);
    if (!view) {
        destroyRenderView(viewID);
        return;
    }

    // Configure the view
    view->setSwapchain(swapchain);
    Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(windowWidth);
    viewport.height = static_cast<float>(windowHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    view->setViewport(viewport);

    // Track that this view will have an active frame
    m_viewsWithActiveFrame.insert(viewID);

    // Create render context
    RenderContext ctx(view, m_device.get());
    ctx.setRenderWorld(this);

    // Set up materials (like renderView does)
    ctx.setDefaultMaterials(
        m_defaultColoredMaterial,
        m_defaultTextured2DMaterial,
        m_defaultWireframeMaterial,
        m_defaultLineMaterial,
        m_defaultTextMaterial
    );
    ctx.setDefaultColoredDoubleSidedMaterial(m_defaultColoredDoubleSidedMaterial);
    ctx.setDefaultTexturedDoubleSidedMaterial(m_defaultTexturedDoubleSidedMaterial);
    ctx.setDefaultColored2DMaterial(m_defaultColored2DMaterial);
    ctx.setDefaultLine2DMaterial(m_defaultLine2DMaterial);

    ctx.updateCameraMatrices();
    ctx.clear();

    // Calculate centered sprite position
    float centerX = position.x + size.x * 0.5f;
    float centerY = position.y + size.y * 0.5f;

    // Draw the textured quad as a sprite
    SpriteDrawData sprite;
    sprite.texture = texture;
    sprite.position = Vec2(centerX, centerY);
    sprite.size = size;
    sprite.pivot = Vec2(0.5f, 0.5f);
    sprite.rotation = 0.0f;
    sprite.tint = tint;
    sprite.uvMin = Vec2(0.0f, 0.0f);
    sprite.uvMax = Vec2(1.0f, 1.0f);
    sprite.layer = 0;

    ctx.drawSprite(sprite);

    // Build and execute render passes
    m_renderPasses.clear();
    buildRenderBatches(ctx.getDrawItems(), m_renderPasses);
    executeRenderPasses(view, m_renderPasses, ctx);

    // Clean up
    destroyRenderView(viewID);

    // Present
    m_device->present(swapchain);
}

void RenderWorld::renderTexturedQuadFromImageData(
    const uint8_t* imageData,
    uint32_t imageWidth,
    uint32_t imageHeight,
    SwapchainHandle swapchain,
    const math::Color& tint,
    const math::Vec2& position,
    const math::Vec2& size,
    int windowWidth,
    int windowHeight)
{
    static int callCount = 0;
    callCount++;
    bool debug = (callCount <= 3);

    if (debug) {
        
    }

    if (!m_device || !imageData || imageWidth == 0 || imageHeight == 0 || !swapchain.isValid()) {
        LOG_ERROR(LogCategory::Render, "renderTexturedQuadFromImageData: invalid params");
        return;
    }

    // Create a temporary canvas camera for 2D rendering
    auto camera = std::make_unique<CameraCanvas>();
    camera->canvasSize = Vec2(static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    camera->clearFlags = CameraClearFlags::All;
    camera->clearColor = math::Color(0.0f, 0.0f, 0.0f, 1.0f);

    // Create render view
    RenderViewID viewID = createRenderView(std::move(camera));
    if (viewID == 0) {
        return;
    }

    RenderView* view = getRenderView(viewID);
    if (!view) {
        destroyRenderView(viewID);
        return;
    }

    // Configure the view
    view->setSwapchain(swapchain);
    Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(windowWidth);
    viewport.height = static_cast<float>(windowHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    view->setViewport(viewport);

    // Track that this view will have an active frame
    m_viewsWithActiveFrame.insert(viewID);

    // Begin frame to make the graphics context current (required for texture creation)
    RenderTargetHandle target = m_device->getSwapchainBackbuffer(swapchain);
    auto cmd = m_device->beginFrame(target);
    if (!cmd) {
        destroyRenderView(viewID);
        return;
    }

    // Now that the context is current, create the texture
    TextureDesc texDesc;
    texDesc.type = TextureType::Texture2D;
    texDesc.width = imageWidth;
    texDesc.height = imageHeight;
    texDesc.mipLevels = 1;
    texDesc.format = TextureFormat::RGBA8_UNORM;
    texDesc.usage = TextureUsage::Sampled;
    texDesc.initialData = imageData;

    TextureHandle texture = m_device->createTexture(texDesc);
    if (!texture.isValid()) {
        LOG_ERROR(LogCategory::Render, "renderTexturedQuadFromImageData: Failed to create texture ({}x{})",
            imageWidth, imageHeight);
        m_device->submit(std::move(cmd));
        destroyRenderView(viewID);
        return;
    }

    if (debug) {
        
    }

    // Set up viewport and clear
    const Viewport& vp = view->getViewport();
    cmd->setViewport(vp);
    cmd->setScissor(view->getScissor());

    RenderCamera* renderCamera = view->getCamera();
    if (renderCamera) {
        if (static_cast<uint32_t>(renderCamera->clearFlags) & static_cast<uint32_t>(CameraClearFlags::Color)) {
            cmd->clearColor(renderCamera->clearColor);
        }
        if (static_cast<uint32_t>(renderCamera->clearFlags) & static_cast<uint32_t>(CameraClearFlags::Depth)) {
            cmd->clearDepth(renderCamera->clearDepth);
        }
    }

    // Create render context for drawing
    RenderContext ctx(view, m_device.get());
    ctx.setRenderWorld(this);

    ctx.setDefaultMaterials(
        m_defaultColoredMaterial,
        m_defaultTextured2DMaterial,
        m_defaultWireframeMaterial,
        m_defaultLineMaterial,
        m_defaultTextMaterial
    );
    ctx.setDefaultColoredDoubleSidedMaterial(m_defaultColoredDoubleSidedMaterial);
    ctx.setDefaultTexturedDoubleSidedMaterial(m_defaultTexturedDoubleSidedMaterial);
    ctx.setDefaultColored2DMaterial(m_defaultColored2DMaterial);
    ctx.setDefaultLine2DMaterial(m_defaultLine2DMaterial);

    ctx.updateCameraMatrices();

    // Set spatial type to Canvas for proper 2D rendering
    ctx.setSpatialType(SpatialType::Canvas);

    // Calculate centered sprite position
    float centerX = position.x + size.x * 0.5f;
    float centerY = position.y + size.y * 0.5f;

    // Draw the textured quad as a sprite
    SpriteDrawData sprite;
    sprite.texture = texture;
    sprite.position = Vec2(centerX, centerY);
    sprite.size = size;
    sprite.pivot = Vec2(0.5f, 0.5f);
    sprite.rotation = 0.0f;
    sprite.tint = tint;
    sprite.uvMin = Vec2(0.0f, 0.0f);
    sprite.uvMax = Vec2(1.0f, 1.0f);
    sprite.layer = 0;

    ctx.drawSprite(sprite);

    // Build render batches
    m_renderPasses.clear();
    buildRenderBatches(ctx.getDrawItems(), m_renderPasses);

    if (debug) {
        size_t totalBatches = 0;
        for (const auto& pass : m_renderPasses) {
            totalBatches += pass.batches.size();
        }

        // Check material validity
        if (!m_defaultTextured2DMaterial.isValid()) {
            LOG_ERROR(LogCategory::Render, "renderTexturedQuadFromImageData: m_defaultTextured2DMaterial is invalid!");
        } else if (!getMaterial(m_defaultTextured2DMaterial)) {
            LOG_ERROR(LogCategory::Render, "renderTexturedQuadFromImageData: material lookup failed!");
        }
    }

    // Get camera for viewProj calculation
    RenderCamera* renderCam = view->getCamera();
    Mat4 viewProj = Mat4::Identity();
    if (renderCam) {
        float aspectRatio = view->getAspectRatio();
        Mat4 viewMatrix = renderCam->getViewMatrix();
        Mat4 proj = renderCam->getProjectionMatrix(aspectRatio);
        viewProj = proj * viewMatrix;
    }

    // Execute render batches
    for (const auto& pass : m_renderPasses) {
        for (const auto& batch : pass.batches) {
            executeBatch(cmd.get(), batch, viewProj, renderCam, view);
        }
    }

    // Submit the command list
    m_device->submit(std::move(cmd));

    // Destroy the temporary texture
    m_device->destroyTexture(texture);

    // Clean up the render view
    destroyRenderView(viewID);

    // Present
    m_device->present(swapchain);
}

void RenderWorld::endFrame(bool presentAll) {
    if (!m_inFrame) {

        return;
    }

    if (presentAll) {
        // Only present views that had a successful beginFrame() this frame
        for (RenderViewID viewID : m_viewsWithActiveFrame) {
            RenderView* view = getRenderView(viewID);
            if (view && view->hasSwapchain()) {
                m_device->present(view->getSwapchain());
            }
        }
        // Clear the active frame tracking after batch present
        m_viewsWithActiveFrame.clear();
    }
    // When presentAll=false, individual presentView() calls will clear entries
    // from m_viewsWithActiveFrame as they present

    // Publish GPU submission stats into the profiler's open frame. The renderer
    // runs inside RuntimeApp's "Render" zone, so the frame record is still open.
    profiling::Profiler& profiler = profiling::Profiler::Get();
    if (profiler.IsEnabled()) {
        profiler.SetCounter("gpu.drawCalls", static_cast<double>(m_stats.drawCalls));
        profiler.SetCounter("gpu.triangles", static_cast<double>(m_stats.triangles));
        profiler.SetCounter("gpu.renderables", static_cast<double>(m_stats.renderables));
        profiler.SetCounter("gpu.renderViews", static_cast<double>(m_stats.renderViews));
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
        // Only present if this view had a successful beginFrame() this frame
        // This prevents calling present when beginFrame failed (e.g., swapchain being destroyed)
        if (m_viewsWithActiveFrame.find(viewID) != m_viewsWithActiveFrame.end()) {
            m_device->present(view->getSwapchain());
            // Remove from active set after presenting
            m_viewsWithActiveFrame.erase(viewID);
        }
    }
}

void RenderWorld::prepareScene(core::Scene* scene) {
    if (!scene || !m_device) {
        return;
    }

    // Check if already prepared
    if (m_preparedScenes.find(scene) != m_preparedScenes.end()) {
        return;
    }

    using namespace core;
    auto root = scene->GetRoot();
    if (!root) {
        m_preparedScenes.insert(scene);
        return;
    }

    // Recursively prepare all renderable components
    std::function<void(std::shared_ptr<Node>)> prepareNode = [&](std::shared_ptr<Node> node) {
        if (!node) {
            return;
        }

        auto components = node->GetComponents();
        for (auto& component : components) {
            auto renderable = dynamic_cast<IRenderableComponent*>(component.get());
            if (renderable) {
                renderable->prepareGPUResources(m_device.get());
            }
        }

        for (auto& child : node->GetChildren()) {
            prepareNode(child);
        }
    };

    prepareNode(root);
    m_preparedScenes.insert(scene);
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

MaterialHandle RenderWorld::getOrCreateCustomMaterial(const std::string& vertPath, const std::string& fragPath, bool isSkeletal) {
    // At least one path must be provided
    if (vertPath.empty() && fragPath.empty()) {
        
        return MaterialHandle();
    }

    // If only one path provided, use it for both (supports single-file shaders like HLSL)
    const std::string& effectiveVertPath = vertPath.empty() ? fragPath : vertPath;
    const std::string& effectiveFragPath = fragPath.empty() ? vertPath : fragPath;

    // Create cache key (use original paths for cache to differentiate single-file vs two-file)
    std::string cacheKey = effectiveVertPath + "|" + effectiveFragPath + "|" + (isSkeletal ? "skeletal" : "static");

    // Check cache first
    auto it = m_customShaderCache.find(cacheKey);
    if (it != m_customShaderCache.end() && it->second.isValid()) {
        return it->second;
    }

    // Load vertex shader source
    std::string vertSource;
    auto& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(effectiveVertPath)) {
        vertSource = packFS.readFileAsString(effectiveVertPath);
        if (vertSource.empty()) {
            
            return MaterialHandle();
        }
    } else {
        auto vertResult = platform::FileSystem::ReadFile(effectiveVertPath);
        if (!vertResult.success) {
            
            return MaterialHandle();
        }
        vertSource = std::move(vertResult.data);
    }

    // Load fragment shader source (may be same file as vertex for single-file shaders)
    std::string fragSource;
    if (effectiveFragPath == effectiveVertPath) {
        // Same file - reuse the already loaded content
        fragSource = vertSource;
    } else {
        if (packFS.isPackMode() && packFS.exists(effectiveFragPath)) {
            fragSource = packFS.readFileAsString(effectiveFragPath);
            if (fragSource.empty()) {
                
                return MaterialHandle();
            }
        } else {
            auto fragResult = platform::FileSystem::ReadFile(effectiveFragPath);
            if (!fragResult.success) {
                
                return MaterialHandle();
            }
            fragSource = std::move(fragResult.data);
        }
    }

    // Compile vertex shader
    ShaderDesc vertDesc;
    vertDesc.stage = ShaderStage::Vertex;
    vertDesc.bytecode = vertSource.c_str();
    vertDesc.bytecodeSize = vertSource.size();
    ShaderHandle vertShader = m_device->createShader(vertDesc);
    if (!vertShader.isValid()) {
        
        return MaterialHandle();
    }

    // Compile fragment shader
    ShaderDesc fragDesc;
    fragDesc.stage = ShaderStage::Fragment;
    fragDesc.bytecode = fragSource.c_str();
    fragDesc.bytecodeSize = fragSource.size();
    ShaderHandle fragShader = m_device->createShader(fragDesc);
    if (!fragShader.isValid()) {
        
        m_device->destroyShader(vertShader);
        return MaterialHandle();
    }

    // Create vertex layout based on skeletal vs static
    // {name, format, offset, binding, location}
    VertexBufferLayout vertexLayout;
    vertexLayout.stride = sizeof(Vertex);
    vertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
    vertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
    vertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
    vertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});
    vertexLayout.attributes.push_back({"a_Tangent", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent)), 0, 4});

    if (isSkeletal) {
        vertexLayout.attributes.push_back({"a_BoneIDs", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneIDs)), 0, 5});
        vertexLayout.attributes.push_back({"a_BoneWeights", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneWeights)), 0, 6});
    }

    // Create pipeline
    PipelineDesc pipelineDesc;
    pipelineDesc.shaders = {vertShader, fragShader};
    pipelineDesc.vertexLayout = vertexLayout;
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
        return MaterialHandle();
    }

    // Create material
    Material customMat;
    customMat.name = "Custom_" + effectiveVertPath + (effectiveVertPath != effectiveFragPath ? "_" + effectiveFragPath : "");
    customMat.vertexShader = vertShader;
    customMat.fragmentShader = fragShader;
    customMat.pipeline = pipeline;
    customMat.renderLayer = RenderLayer::Opaque;
    customMat.isTransparent = false;
    // A raw custom shader that samples u_SceneTexture is a mid-scene grab-pass too (Gap C):
    // the renderer captures the scene-so-far and binds it before this material draws,
    // exactly as for .lsh materials. Mirrors the detection in getOrCreateLshMaterial.
    customMat.usesSceneTexture = (vertSource.find("u_SceneTexture") != std::string::npos
                                  || fragSource.find("u_SceneTexture") != std::string::npos);

    MaterialHandle materialHandle = createMaterial(customMat);

    // Cache the material
    m_customShaderCache[cacheKey] = materialHandle;

    return materialHandle;
}

void RenderWorld::clearCustomShaderCache() {
    m_customShaderCache.clear();
}

MaterialHandle RenderWorld::getOrCreateLshMaterial(const std::string& lshPath, int blendMode,
                                                   LshMaterialLayout layout) {
    if (lshPath.empty() || !m_device) {
        return MaterialHandle();
    }

    const std::string cacheKey = lshPath + "|" + std::to_string(blendMode)
        + "|" + std::to_string(static_cast<int>(layout));

    // ----- Resolve the physical path and compute a cheap change-fingerprint -----
    // This avoids reading the whole file on a warm cache: only the last-write-time is
    // stat'd each frame, and the source is read solely on a cache miss or change.
    auto& packFS = platform::PackFileSystem::Instance();
    const bool packMode = packFS.isPackMode() && packFS.exists(lshPath);

    std::string physicalPath = lshPath;
    if (!packMode) {
        auto& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            std::string resolved = assetDb.ResolveAsset(lshPath);
            if (!resolved.empty()) {
                physicalPath = resolved;
            }
        }
    }

    std::string fingerprint;  // empty in pack mode (immutable assets, cache by path only)
    if (!packMode) {
        std::error_code ec;
        auto writeTime = std::filesystem::last_write_time(physicalPath, ec);
        if (!ec) {
            fingerprint = std::to_string(static_cast<long long>(writeTime.time_since_epoch().count()));
        }
    }

    // ----- Cache lookup; release stale GPU resources when the source changed -----
    auto cacheIt = m_lshMaterialCache.find(cacheKey);
    if (cacheIt != m_lshMaterialCache.end()) {
        if (cacheIt->second.material.isValid() && cacheIt->second.sourceFingerprint == fingerprint) {
            return cacheIt->second.material;
        }
        const LshMaterialEntry& stale = cacheIt->second;
        if (stale.pipeline.isValid()) m_device->destroyPipeline(stale.pipeline);
        if (stale.vertexShader.isValid()) m_device->destroyShader(stale.vertexShader);
        if (stale.fragmentShader.isValid()) m_device->destroyShader(stale.fragmentShader);
        if (stale.material.isValid()) destroyMaterial(stale.material);
        m_lshMaterialCache.erase(cacheIt);
    }

    // ----- Read the source (only on a cache miss or change) -----
    std::string source;
    if (packMode) {
        source = packFS.readFileAsString(lshPath);
    } else {
        auto readResult = platform::FileSystem::ReadFile(physicalPath);
        if (!readResult.success) {
            return MaterialHandle();
        }
        source = std::move(readResult.data);
    }

    if (source.empty()) {
        return MaterialHandle();
    }

    // ----- Translate the .lsh to the active backend -----
    const GraphicsBackend backend = getBackend();
    ShaderTranslatorResult translated = ShaderTranslator::translate(source, backend);
    if (!translated.success) {
        LOG_WARN(LogCategory::Render, "getOrCreateLshMaterial: failed to translate '{}': {}",
                 lshPath, translated.errorMessage);
        return MaterialHandle();
    }

    const std::string& vertSource = (backend == GraphicsBackend::Metal)
        ? translated.combinedSource : translated.vertexSource;
    const std::string& fragSource = (backend == GraphicsBackend::Metal)
        ? translated.combinedSource : translated.fragmentSource;

    // ----- Compile the vertex and fragment stages -----
    ShaderDesc vertDesc;
    vertDesc.stage = ShaderStage::Vertex;
    vertDesc.bytecode = vertSource.c_str();
    vertDesc.bytecodeSize = vertSource.size();
    ShaderHandle vertShader = m_device->createShader(vertDesc);
    if (!vertShader.isValid()) {
        LOG_WARN(LogCategory::Render, "getOrCreateLshMaterial: vertex shader compile failed for '{}'", lshPath);
        return MaterialHandle();
    }

    ShaderDesc fragDesc;
    fragDesc.stage = ShaderStage::Fragment;
    fragDesc.bytecode = fragSource.c_str();
    fragDesc.bytecodeSize = fragSource.size();
    ShaderHandle fragShader = m_device->createShader(fragDesc);
    if (!fragShader.isValid()) {
        LOG_WARN(LogCategory::Render, "getOrCreateLshMaterial: fragment shader compile failed for '{}'", lshPath);
        m_device->destroyShader(vertShader);
        return MaterialHandle();
    }

    // ----- Vertex layout per host kind -----
    // UI quads and 3D meshes share the standard Vertex struct; non-instanced 3D adds
    // tangent (location 4), skeletal adds bone ids/weights. Instanced 3D omits tangent —
    // locations 4-9 belong to the per-instance buffer at binding 1 — and matches the
    // built-in instanced shaders (pbr_instanced.lsh): a_Position/Normal/TexCoord/Color only.
    const bool is3D = (layout != LshMaterialLayout::UI2D);
    const bool isInstanced = (layout == LshMaterialLayout::InstancedMesh3D);
    VertexBufferLayout vertexLayout;
    vertexLayout.stride = sizeof(Vertex);
    vertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
    vertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
    vertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
    vertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});
    if (is3D && !isInstanced) {
        vertexLayout.attributes.push_back({"a_Tangent", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent)), 0, 4});
    }
    if (layout == LshMaterialLayout::SkeletalMesh3D) {
        vertexLayout.attributes.push_back({"a_BoneIDs", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneIDs)), 0, 5});
        vertexLayout.attributes.push_back({"a_BoneWeights", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneWeights)), 0, 6});
    }

    // ----- Pipeline render state -----
    // A `#render_mode` in the shader overrides blend/cull/depth; otherwise the host kind
    // selects sensible defaults (2D-UI: blend-per-mode, no depth, no cull; 3D mesh: opaque,
    // depth test+write, back-face cull — Godot spatial defaults).
    const LshRenderMode& rm = translated.renderMode;
    PipelineDesc pipelineDesc;
    pipelineDesc.shaders = {vertShader, fragShader};
    pipelineDesc.vertexLayout = vertexLayout;
    pipelineDesc.topology = PrimitiveTopology::TriangleList;

    // Instanced host: per-instance transform/color/custom at binding 1 (matches the
    // engine's InstanceVertexData and the built-in pbr_instanced.lsh inputs at 4-9).
    if (isInstanced) {
        VertexBufferLayout instLayout;
        instLayout.stride = sizeof(InstanceVertexData);
        instLayout.inputRate = VertexInputRate::Instance;
        instLayout.attributes.push_back({"a_InstanceModel0", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol0)), 1, 4});
        instLayout.attributes.push_back({"a_InstanceModel1", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol1)), 1, 5});
        instLayout.attributes.push_back({"a_InstanceModel2", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol2)), 1, 6});
        instLayout.attributes.push_back({"a_InstanceModel3", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol3)), 1, 7});
        instLayout.attributes.push_back({"a_InstanceColor", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, color)), 1, 8});
        instLayout.attributes.push_back({"a_InstanceCustom", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, customData)), 1, 9});
        pipelineDesc.extraVertexBuffers = { instLayout };
    }

    bool opaqueBlend = false;
    if (rm.specified) {
        if (rm.blend == "add") { pipelineDesc.blendState = BlendState::additive(); }
        else if (rm.blend == "mul") { pipelineDesc.blendState = BlendState::multiply(); }
        else if (rm.blend == "opaque") { pipelineDesc.blendState = BlendState::opaque(); opaqueBlend = true; }
        else if (rm.blend == "premul") {
            BlendState bs; bs.blendEnable = true;
            bs.srcColorBlend = BlendFactor::One; bs.dstColorBlend = BlendFactor::OneMinusSrcAlpha;
            bs.srcAlphaBlend = BlendFactor::One; bs.dstAlphaBlend = BlendFactor::OneMinusSrcAlpha;
            pipelineDesc.blendState = bs;
        } else if (rm.blend == "sub") {
            BlendState bs = BlendState::additive();
            bs.colorBlendOp = BlendOp::ReverseSubtract; bs.alphaBlendOp = BlendOp::ReverseSubtract;
            pipelineDesc.blendState = bs;
        } else { pipelineDesc.blendState = BlendState::alphaBlend(); }  // mix
    } else {
        switch (blendMode) {
            case 1: pipelineDesc.blendState = BlendState::additive(); break;
            case 2: pipelineDesc.blendState = BlendState::multiply(); break;
            case 3: pipelineDesc.blendState = BlendState::opaque(); opaqueBlend = true; break;
            case 4: pipelineDesc.blendState = BlendState::overlay(); break;
            default: pipelineDesc.blendState = BlendState::alphaBlend(); break;
        }
    }

    if (rm.specified) {
        pipelineDesc.rasterizerState.cullMode =
            (rm.cull == "disabled") ? CullMode::None
            : (rm.cull == "front") ? CullMode::Front : CullMode::Back;
    } else {
        pipelineDesc.rasterizerState.cullMode = is3D ? CullMode::Back : CullMode::None;
    }
    pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

    if (rm.specified) {
        DepthStencilState ds;
        ds.depthTestEnable = rm.depthTest;
        ds.depthWriteEnable = rm.depthWrite;
        ds.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.depthStencilState = ds;
    } else if (is3D) {
        DepthStencilState ds;
        ds.depthTestEnable = true;
        ds.depthWriteEnable = opaqueBlend;  // transparent 3D writes color but not depth
        ds.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.depthStencilState = ds;
    } else {
        pipelineDesc.depthStencilState = DepthStencilState::noDepth();
    }

    PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
    if (!pipeline.isValid()) {
        LOG_WARN(LogCategory::Render, "getOrCreateLshMaterial: pipeline creation failed for '{}'", lshPath);
        m_device->destroyShader(vertShader);
        m_device->destroyShader(fragShader);
        return MaterialHandle();
    }

    // ----- Create the material -----
    Material mat;
    mat.name = "Lsh_" + lshPath + "_blend" + std::to_string(blendMode);
    mat.vertexShader = vertShader;
    mat.fragmentShader = fragShader;
    mat.pipeline = pipeline;
    mat.isTransparent = !opaqueBlend;
    mat.renderLayer = is3D ? (opaqueBlend ? RenderLayer::Opaque : RenderLayer::Transparent)
                           : RenderLayer::UI;
    mat.usesLighting = translated.usesLighting;
    // A shader that samples u_SceneTexture is a screen-space post-process: the renderer must
    // capture the scene color and bind it before this material draws.
    mat.usesSceneTexture = (source.find("u_SceneTexture") != std::string::npos);
    // Per-frame engine-fed built-ins (Gap E/F): filled in executeBatch when present.
    mat.usesTime = translated.usesTime;
    mat.usesScreenSize = (source.find("u_TexelSize") != std::string::npos
                          || source.find("u_Resolution") != std::string::npos);

    // Per-texture sampler overrides from @filter/@repeat hints (Gap G). source_color is
    // handled in-shader, so only filter/repeat need a sampler object here.
    for (const LshSamplerConfig& sc : translated.samplerConfigs) {
        if (sc.slot < 0 || (sc.filter.empty() && sc.repeat.empty())) continue;
        const FilterMode fm = (sc.filter == "nearest") ? FilterMode::Nearest : FilterMode::Linear;
        WrapMode wm = WrapMode::Repeat;
        if (sc.repeat == "clamp") wm = WrapMode::ClampToEdge;
        else if (sc.repeat == "mirror") wm = WrapMode::MirroredRepeat;
        SamplerHandle samp = getOrCreateLshSampler(fm, wm);
        if (samp.isValid()) mat.customSamplers.emplace_back(sc.slot, samp);
    }

    MaterialHandle materialHandle = createMaterial(mat);

    LshMaterialEntry entry;
    entry.material = materialHandle;
    entry.vertexShader = vertShader;
    entry.fragmentShader = fragShader;
    entry.pipeline = pipeline;
    entry.sourceFingerprint = fingerprint;
    m_lshMaterialCache[cacheKey] = entry;

    return materialHandle;
}

SamplerHandle RenderWorld::getOrCreateLshSampler(FilterMode filter, WrapMode wrap) {
    if (!m_device) return SamplerHandle();
    const std::string key = std::to_string(static_cast<int>(filter)) + "|"
        + std::to_string(static_cast<int>(wrap));
    auto it = m_lshSamplerCache.find(key);
    if (it != m_lshSamplerCache.end()) return it->second;
    SamplerDesc desc;
    desc.minFilter = filter;
    desc.magFilter = filter;
    desc.mipFilter = filter;
    desc.wrapU = wrap;
    desc.wrapV = wrap;
    desc.wrapW = wrap;
    SamplerHandle samp = m_device->createSampler(desc);
    m_lshSamplerCache[key] = samp;
    return samp;
}

void RenderWorld::clearLshMaterialCache(const std::string& lshPath) {
    if (!m_device) {
        m_lshMaterialCache.clear();
        return;
    }

    const std::string prefix = lshPath.empty() ? std::string() : (lshPath + "|");
    for (auto it = m_lshMaterialCache.begin(); it != m_lshMaterialCache.end(); ) {
        const bool match = lshPath.empty() || (it->first.rfind(prefix, 0) == 0);
        if (match) {
            const LshMaterialEntry& entry = it->second;
            if (entry.pipeline.isValid()) m_device->destroyPipeline(entry.pipeline);
            if (entry.vertexShader.isValid()) m_device->destroyShader(entry.vertexShader);
            if (entry.fragmentShader.isValid()) m_device->destroyShader(entry.fragmentShader);
            if (entry.material.isValid()) destroyMaterial(entry.material);
            it = m_lshMaterialCache.erase(it);
        } else {
            ++it;
        }
    }
}

bool RenderWorld::createDefaultMaterials() {

    auto createStandardVertexLayout = []() -> VertexBufferLayout {
        VertexBufferLayout layout;
        layout.stride = sizeof(Vertex);

        // {name, format, offset, binding, location}
        layout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        layout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        layout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        layout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});

        return layout;
    };

    // Per-instance vertex buffer layout (binding 1, instance step rate) used by
    // GPU-instanced pipelines (MultiMesh). Matches InstanceVertexData exactly:
    // model matrix columns at locations 4-7, color at 8, custom data at 9.
    auto createInstanceVertexLayout = []() -> VertexBufferLayout {
        VertexBufferLayout layout;
        layout.stride = sizeof(InstanceVertexData);
        layout.inputRate = VertexInputRate::Instance;

        // {name, format, offset, binding, location}
        layout.attributes.push_back({"a_InstanceModel0", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol0)), 1, 4});
        layout.attributes.push_back({"a_InstanceModel1", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol1)), 1, 5});
        layout.attributes.push_back({"a_InstanceModel2", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol2)), 1, 6});
        layout.attributes.push_back({"a_InstanceModel3", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol3)), 1, 7});
        layout.attributes.push_back({"a_InstanceColor", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, color)), 1, 8});
        layout.attributes.push_back({"a_InstanceCustom", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, customData)), 1, 9});

        return layout;
    };

    // Note: Sprite layout now uses standard layout to match Unlit shader expectations
    // The Vertex struct has all fields, so we include Normal even though sprites don't use it
    auto createSpriteVertexLayout = []() -> VertexBufferLayout {
        VertexBufferLayout layout;
        layout.stride = sizeof(Vertex);

        // {name, format, offset, binding, location}
        layout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        layout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        layout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        layout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});

        return layout;
    };

    {
        auto shaders = loadShaders("Unlit");
        if (!shaders.success) {
            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
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
            destroyLoadedShaders(shaders);
            return false;
        }

        Material coloredMat;
        coloredMat.name = "DefaultColored";
        coloredMat.vertexShader = shaders.vertex;
        coloredMat.fragmentShader = shaders.fragment;
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
        colored2DMat.vertexShader = shaders.vertex;
        colored2DMat.fragmentShader = shaders.fragment;
        colored2DMat.pipeline = pipeline2D;
        colored2DMat.renderLayer = RenderLayer::Transparent;
        colored2DMat.isTransparent = true;

        m_defaultColored2DMaterial = createMaterial(colored2DMat);
        LOG_INFO(LogCategory::Render, "Created DefaultColored2D material with id={}, pipeline.id={}, depthTestEnable=false, depthWriteEnable=false",
                 m_defaultColored2DMaterial.id, pipeline2D.id);

        // 2D colored material with additive blending (textureless additive quads).
        PipelineDesc pipelineDesc2DAdditive = pipelineDesc2D;
        pipelineDesc2DAdditive.blendState = BlendState::additive();

        PipelineHandle pipeline2DColoredAdditive = m_device->createPipeline(pipelineDesc2DAdditive);
        if (!pipeline2DColoredAdditive.isValid()) {

            return false;
        }

        Material colored2DAdditiveMat;
        colored2DAdditiveMat.name = "Colored2DAdditive";
        colored2DAdditiveMat.vertexShader = shaders.vertex;
        colored2DAdditiveMat.fragmentShader = shaders.fragment;
        colored2DAdditiveMat.pipeline = pipeline2DColoredAdditive;
        colored2DAdditiveMat.renderLayer = RenderLayer::Transparent;
        colored2DAdditiveMat.isTransparent = true;

        m_defaultColored2DAdditiveMaterial = createMaterial(colored2DAdditiveMat);

        PipelineDesc pipelineDescDoubleSided = pipelineDesc;
        pipelineDescDoubleSided.rasterizerState.cullMode = CullMode::None;

        PipelineHandle pipelineDoubleSided = m_device->createPipeline(pipelineDescDoubleSided);
        if (!pipelineDoubleSided.isValid()) {

        } else {
            Material coloredDoubleSidedMat;
            coloredDoubleSidedMat.name = "DefaultColoredDoubleSided";
            coloredDoubleSidedMat.vertexShader = shaders.vertex;
            coloredDoubleSidedMat.fragmentShader = shaders.fragment;
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

        // 2D textured material with additive blending (sprites/particles glow).
        PipelineDesc pipelineDesc2DAdditive;
        pipelineDesc2DAdditive.shaders = {coloredMat->vertexShader, coloredMat->fragmentShader};
        pipelineDesc2DAdditive.vertexLayout = createSpriteVertexLayout();
        pipelineDesc2DAdditive.topology = PrimitiveTopology::TriangleList;
        pipelineDesc2DAdditive.blendState = BlendState::additive();
        pipelineDesc2DAdditive.depthStencilState = DepthStencilState::noDepth();
        pipelineDesc2DAdditive.rasterizerState = RasterizerState::noCull();

        PipelineHandle pipeline2DAdditive = m_device->createPipeline(pipelineDesc2DAdditive);
        if (!pipeline2DAdditive.isValid()) {

            return false;
        }

        Material textured2DAdditiveMat;
        textured2DAdditiveMat.name = "Textured2DAdditive";
        textured2DAdditiveMat.vertexShader = coloredMat->vertexShader;
        textured2DAdditiveMat.fragmentShader = coloredMat->fragmentShader;
        textured2DAdditiveMat.pipeline = pipeline2DAdditive;
        textured2DAdditiveMat.renderLayer = RenderLayer::Transparent;
        textured2DAdditiveMat.isTransparent = true;

        m_defaultTextured2DAdditiveMaterial = createMaterial(textured2DAdditiveMat);

        // 3D billboard particle materials. These test the scene depth buffer (so
        // particles are occluded by geometry) but do not write depth (so a cloud
        // of overlapping particles blends correctly regardless of draw order).
        PipelineDesc pipelineDescParticle3D;
        pipelineDescParticle3D.shaders = {coloredMat->vertexShader, coloredMat->fragmentShader};
        pipelineDescParticle3D.vertexLayout = createSpriteVertexLayout();
        pipelineDescParticle3D.topology = PrimitiveTopology::TriangleList;
        pipelineDescParticle3D.blendState = BlendState::alphaBlend();
        pipelineDescParticle3D.depthStencilState = DepthStencilState::depthReadOnly();
        pipelineDescParticle3D.rasterizerState = RasterizerState::noCull();

        PipelineHandle pipelineParticle3DAlpha = m_device->createPipeline(pipelineDescParticle3D);
        if (!pipelineParticle3DAlpha.isValid()) {

            return false;
        }

        Material particle3DAlphaMat;
        particle3DAlphaMat.name = "Particle3DAlpha";
        particle3DAlphaMat.vertexShader = coloredMat->vertexShader;
        particle3DAlphaMat.fragmentShader = coloredMat->fragmentShader;
        particle3DAlphaMat.pipeline = pipelineParticle3DAlpha;
        particle3DAlphaMat.renderLayer = RenderLayer::Transparent;
        particle3DAlphaMat.isTransparent = true;

        m_particle3DAlphaMaterial = createMaterial(particle3DAlphaMat);

        PipelineDesc pipelineDescParticle3DAdditive = pipelineDescParticle3D;
        pipelineDescParticle3DAdditive.blendState = BlendState::additive();

        PipelineHandle pipelineParticle3DAdditive = m_device->createPipeline(pipelineDescParticle3DAdditive);
        if (!pipelineParticle3DAdditive.isValid()) {

            return false;
        }

        Material particle3DAdditiveMat;
        particle3DAdditiveMat.name = "Particle3DAdditive";
        particle3DAdditiveMat.vertexShader = coloredMat->vertexShader;
        particle3DAdditiveMat.fragmentShader = coloredMat->fragmentShader;
        particle3DAdditiveMat.pipeline = pipelineParticle3DAdditive;
        particle3DAdditiveMat.renderLayer = RenderLayer::Transparent;
        particle3DAdditiveMat.isTransparent = true;

        m_particle3DAdditiveMaterial = createMaterial(particle3DAdditiveMat);

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
        auto shaders = loadShaders("Wireframe");
        if (!shaders.success) {
            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
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
            destroyLoadedShaders(shaders);
            return false;
        }

        Material wireframeMat;
        wireframeMat.name = "DefaultWireframe";
        wireframeMat.vertexShader = shaders.vertex;
        wireframeMat.fragmentShader = shaders.fragment;
        wireframeMat.pipeline = pipeline;
        wireframeMat.renderLayer = RenderLayer::Opaque;
        wireframeMat.isTransparent = false;

        m_defaultWireframeMaterial = createMaterial(wireframeMat);

    }

    {
        auto shaders = loadShaders("Line");
        if (!shaders.success) {
            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
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
            destroyLoadedShaders(shaders);
            return false;
        }

        Material lineMat;
        lineMat.name = "DefaultLine";
        lineMat.vertexShader = shaders.vertex;
        lineMat.fragmentShader = shaders.fragment;
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
        line2DMat.vertexShader = shaders.vertex;
        line2DMat.fragmentShader = shaders.fragment;
        line2DMat.pipeline = pipeline2D;
        line2DMat.renderLayer = RenderLayer::Transparent;
        line2DMat.isTransparent = true;

        m_defaultLine2DMaterial = createMaterial(line2DMat);

    }

    {
        auto shaders = loadShaders("Text");
        if (!shaders.success) {
            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
        pipelineDesc.vertexLayout = createStandardVertexLayout();
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::alphaBlend();
        pipelineDesc.depthStencilState = DepthStencilState::noDepth();
        pipelineDesc.rasterizerState.cullMode = CullMode::None;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            destroyLoadedShaders(shaders);
            return false;
        }

        Material textMat;
        textMat.name = "DefaultText";
        textMat.vertexShader = shaders.vertex;
        textMat.fragmentShader = shaders.fragment;
        textMat.pipeline = pipeline;
        textMat.renderLayer = RenderLayer::UI;
        textMat.isTransparent = true;

        m_defaultTextMaterial = createMaterial(textMat);

    }

    {
        auto shaders = loadShaders("Text3D");
        if (!shaders.success) {
            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
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
            destroyLoadedShaders(shaders);
            return false;
        }

        Material text3dMat;
        text3dMat.name = "DefaultText3D";
        text3dMat.vertexShader = shaders.vertex;
        text3dMat.fragmentShader = shaders.fragment;
        text3dMat.pipeline = pipeline;
        text3dMat.renderLayer = RenderLayer::Transparent;
        text3dMat.isTransparent = true;

        m_defaultText3DMaterial = createMaterial(text3dMat);
    }

    {
        auto shaders = loadShaders("RoundedRect");
        if (!shaders.success) {
            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
        pipelineDesc.vertexLayout = createStandardVertexLayout();
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::alphaBlend();
        pipelineDesc.depthStencilState = DepthStencilState::noDepth();
        pipelineDesc.rasterizerState.cullMode = CullMode::None;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            destroyLoadedShaders(shaders);
            return false;
        }

        Material roundedRectMat;
        roundedRectMat.name = "DefaultRoundedRect";
        roundedRectMat.vertexShader = shaders.vertex;
        roundedRectMat.fragmentShader = shaders.fragment;
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
        auto shaders = loadShaders("RoundedRectBorder");
        if (!shaders.success) {
            return false;
        }

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
        pipelineDesc.vertexLayout = createStandardVertexLayout();
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::alphaBlend();
        pipelineDesc.depthStencilState = DepthStencilState::noDepth();
        pipelineDesc.rasterizerState.cullMode = CullMode::None;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            destroyLoadedShaders(shaders);
            return false;
        }

        Material borderMat;
        borderMat.name = "RoundedRectBorder";
        borderMat.vertexShader = shaders.vertex;
        borderMat.fragmentShader = shaders.fragment;
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

    // Create radial gradient materials
    {
        auto shaders = loadShaders("RadialGradient");
        if (!shaders.success) {
            // Radial gradient is optional, continue without it
            LOG_WARN(LogCategory::Render, "Failed to load RadialGradient shaders");
        } else {
            PipelineDesc pipelineDesc;
            pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
            pipelineDesc.vertexLayout = createStandardVertexLayout();
            pipelineDesc.topology = PrimitiveTopology::TriangleList;
            pipelineDesc.blendState = BlendState::alphaBlend();
            pipelineDesc.depthStencilState = DepthStencilState::noDepth();
            pipelineDesc.rasterizerState.cullMode = CullMode::None;
            pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

            PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
            if (pipeline.isValid()) {
                Material radialGradientMat;
                radialGradientMat.name = "RadialGradient";
                radialGradientMat.vertexShader = shaders.vertex;
                radialGradientMat.fragmentShader = shaders.fragment;
                radialGradientMat.pipeline = pipeline;
                radialGradientMat.renderLayer = RenderLayer::UI;
                radialGradientMat.isTransparent = true;

                m_radialGradientMaterial = createMaterial(radialGradientMat);

                // Create additive blend variant
                {
                    PipelineDesc additivePipelineDesc = pipelineDesc;
                    additivePipelineDesc.blendState = BlendState::additive();
                    PipelineHandle additivePipeline = m_device->createPipeline(additivePipelineDesc);
                    if (additivePipeline.isValid()) {
                        Material additiveMat;
                        additiveMat.name = "RadialGradientAdditive";
                        additiveMat.vertexShader = shaders.vertex;
                        additiveMat.fragmentShader = shaders.fragment;
                        additiveMat.pipeline = additivePipeline;
                        additiveMat.renderLayer = RenderLayer::UI;
                        additiveMat.isTransparent = true;
                        m_radialGradientAdditiveMaterial = createMaterial(additiveMat);
                    }
                }

                // Create multiply blend variant
                {
                    PipelineDesc multiplyPipelineDesc = pipelineDesc;
                    multiplyPipelineDesc.blendState = BlendState::multiply();
                    PipelineHandle multiplyPipeline = m_device->createPipeline(multiplyPipelineDesc);
                    if (multiplyPipeline.isValid()) {
                        Material multiplyMat;
                        multiplyMat.name = "RadialGradientMultiply";
                        multiplyMat.vertexShader = shaders.vertex;
                        multiplyMat.fragmentShader = shaders.fragment;
                        multiplyMat.pipeline = multiplyPipeline;
                        multiplyMat.renderLayer = RenderLayer::UI;
                        multiplyMat.isTransparent = true;
                        m_radialGradientMultiplyMaterial = createMaterial(multiplyMat);
                    }
                }

                // Create opaque variant
                {
                    PipelineDesc opaquePipelineDesc = pipelineDesc;
                    opaquePipelineDesc.blendState = BlendState::opaque();
                    PipelineHandle opaquePipeline = m_device->createPipeline(opaquePipelineDesc);
                    if (opaquePipeline.isValid()) {
                        Material opaqueMat;
                        opaqueMat.name = "RadialGradientOpaque";
                        opaqueMat.vertexShader = shaders.vertex;
                        opaqueMat.fragmentShader = shaders.fragment;
                        opaqueMat.pipeline = opaquePipeline;
                        opaqueMat.renderLayer = RenderLayer::Opaque;
                        opaqueMat.isTransparent = false;
                        m_radialGradientOpaqueMaterial = createMaterial(opaqueMat);
                    }
                }
            }
        }
    }

    // Create polygon materials
    {
        
        auto shaders = loadShaders("Polygon");
        if (!shaders.success) {
            // Polygon is optional, continue without it
            LOG_ERROR(LogCategory::Render, "Failed to load Polygon shaders!");
        } else {
            
            PipelineDesc pipelineDesc;
            pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
            pipelineDesc.vertexLayout = createStandardVertexLayout();
            pipelineDesc.topology = PrimitiveTopology::TriangleList;
            pipelineDesc.blendState = BlendState::alphaBlend();
            pipelineDesc.depthStencilState = DepthStencilState::noDepth();
            pipelineDesc.rasterizerState.cullMode = CullMode::None;
            pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

            PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
            if (pipeline.isValid()) {
                Material polygonMat;
                polygonMat.name = "Polygon";
                polygonMat.vertexShader = shaders.vertex;
                polygonMat.fragmentShader = shaders.fragment;
                polygonMat.pipeline = pipeline;
                polygonMat.renderLayer = RenderLayer::UI;
                polygonMat.isTransparent = true;

                m_polygonMaterial = createMaterial(polygonMat);

                // Create additive blend variant
                {
                    PipelineDesc additivePipelineDesc = pipelineDesc;
                    additivePipelineDesc.blendState = BlendState::additive();
                    PipelineHandle additivePipeline = m_device->createPipeline(additivePipelineDesc);
                    if (additivePipeline.isValid()) {
                        Material additiveMat;
                        additiveMat.name = "PolygonAdditive";
                        additiveMat.vertexShader = shaders.vertex;
                        additiveMat.fragmentShader = shaders.fragment;
                        additiveMat.pipeline = additivePipeline;
                        additiveMat.renderLayer = RenderLayer::UI;
                        additiveMat.isTransparent = true;
                        m_polygonAdditiveMaterial = createMaterial(additiveMat);
                    }
                }

                // Create multiply blend variant
                {
                    PipelineDesc multiplyPipelineDesc = pipelineDesc;
                    multiplyPipelineDesc.blendState = BlendState::multiply();
                    PipelineHandle multiplyPipeline = m_device->createPipeline(multiplyPipelineDesc);
                    if (multiplyPipeline.isValid()) {
                        Material multiplyMat;
                        multiplyMat.name = "PolygonMultiply";
                        multiplyMat.vertexShader = shaders.vertex;
                        multiplyMat.fragmentShader = shaders.fragment;
                        multiplyMat.pipeline = multiplyPipeline;
                        multiplyMat.renderLayer = RenderLayer::UI;
                        multiplyMat.isTransparent = true;
                        m_polygonMultiplyMaterial = createMaterial(multiplyMat);
                    }
                }

                // Create opaque variant
                {
                    PipelineDesc opaquePipelineDesc = pipelineDesc;
                    opaquePipelineDesc.blendState = BlendState::opaque();
                    PipelineHandle opaquePipeline = m_device->createPipeline(opaquePipelineDesc);
                    if (opaquePipeline.isValid()) {
                        Material opaqueMat;
                        opaqueMat.name = "PolygonOpaque";
                        opaqueMat.vertexShader = shaders.vertex;
                        opaqueMat.fragmentShader = shaders.fragment;
                        opaqueMat.pipeline = opaquePipeline;
                        opaqueMat.renderLayer = RenderLayer::Opaque;
                        opaqueMat.isTransparent = false;
                        m_polygonOpaqueMaterial = createMaterial(opaqueMat);
                    }
                }
            }
        }
    }

    {
        auto shaders = loadShaders("PBR");
        if (!shaders.success) {
            return false;
        }

        VertexBufferLayout pbrVertexLayout;
        pbrVertexLayout.stride = sizeof(Vertex);
        pbrVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        pbrVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        pbrVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        pbrVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});
        pbrVertexLayout.attributes.push_back({"a_Tangent", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent)), 0, 4});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
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
            destroyLoadedShaders(shaders);
            return false;
        }

        Material pbrMat;
        pbrMat.name = "DefaultPBR";
        pbrMat.vertexShader = shaders.vertex;
        pbrMat.fragmentShader = shaders.fragment;
        pbrMat.pipeline = pipeline;
        pbrMat.renderLayer = RenderLayer::Opaque;
        pbrMat.isTransparent = false;

        m_defaultPBRMaterial = createMaterial(pbrMat);

    }

    {
        // Instanced PBR: same lighting/material model as DefaultPBR, but the
        // vertex stage reads the per-instance world transform, color and custom
        // data from a second vertex buffer (binding 1, instance step rate)
        // instead of the u_Model push constant. Used by MultiMesh for true GPU
        // instancing of large instance counts in a single draw call.
        auto shaders = loadShaders("PBRInstanced");
        if (!shaders.success) {
            return false;
        }

        VertexBufferLayout pbrVertexLayout;
        pbrVertexLayout.stride = sizeof(Vertex);
        pbrVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        pbrVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        pbrVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        pbrVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
        pipelineDesc.vertexLayout = pbrVertexLayout;
        pipelineDesc.extraVertexBuffers = { createInstanceVertexLayout() };
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::opaque();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            destroyLoadedShaders(shaders);
            return false;
        }

        Material pbrInstancedMat;
        pbrInstancedMat.name = "DefaultPBRInstanced";
        pbrInstancedMat.vertexShader = shaders.vertex;
        pbrInstancedMat.fragmentShader = shaders.fragment;
        pbrInstancedMat.pipeline = pipeline;
        pbrInstancedMat.renderLayer = RenderLayer::Opaque;
        pbrInstancedMat.isTransparent = false;
        pbrInstancedMat.usesLighting = true;

        m_defaultPBRInstancedMaterial = createMaterial(pbrInstancedMat);
    }

    {
        auto shaders = loadShaders("Skeletal");
        if (!shaders.success) {
            return false;
        }

        VertexBufferLayout skeletalVertexLayout;
        skeletalVertexLayout.stride = sizeof(Vertex);
        skeletalVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        skeletalVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        skeletalVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        skeletalVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});
        skeletalVertexLayout.attributes.push_back({"a_Tangent", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent)), 0, 4});
        skeletalVertexLayout.attributes.push_back({"a_BoneIDs", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneIDs)), 0, 5});
        skeletalVertexLayout.attributes.push_back({"a_BoneWeights", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneWeights)), 0, 6});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
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
            destroyLoadedShaders(shaders);
            return false;
        }

        Material skeletalMat;
        skeletalMat.name = "DefaultSkeletal";
        skeletalMat.vertexShader = shaders.vertex;
        skeletalMat.fragmentShader = shaders.fragment;
        skeletalMat.pipeline = pipeline;
        skeletalMat.renderLayer = RenderLayer::Opaque;
        skeletalMat.isTransparent = false;

        m_defaultSkeletalMaterial = createMaterial(skeletalMat);

    }

    // Create Toon material (cel shading)
    {
        auto shaders = loadShaders("Toon");
        if (!shaders.success) {
            return false;
        }

        VertexBufferLayout toonVertexLayout;
        toonVertexLayout.stride = sizeof(Vertex);
        toonVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        toonVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        toonVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        toonVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});
        toonVertexLayout.attributes.push_back({"a_Tangent", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent)), 0, 4});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
        pipelineDesc.vertexLayout = toonVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::opaque();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            destroyLoadedShaders(shaders);
            return false;
        }

        Material toonMat;
        toonMat.name = "DefaultToon";
        toonMat.vertexShader = shaders.vertex;
        toonMat.fragmentShader = shaders.fragment;
        toonMat.pipeline = pipeline;
        toonMat.renderLayer = RenderLayer::Opaque;
        toonMat.isTransparent = false;

        m_defaultToonMaterial = createMaterial(toonMat);
    }

    // Create Skeletal Toon material (cel shading with skeletal animation)
    {
        auto shaders = loadShaders("SkeletalToon");
        if (!shaders.success) {
            return false;
        }

        VertexBufferLayout skeletalToonVertexLayout;
        skeletalToonVertexLayout.stride = sizeof(Vertex);
        skeletalToonVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        skeletalToonVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        skeletalToonVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        skeletalToonVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});
        skeletalToonVertexLayout.attributes.push_back({"a_Tangent", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent)), 0, 4});
        skeletalToonVertexLayout.attributes.push_back({"a_BoneIDs", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneIDs)), 0, 5});
        skeletalToonVertexLayout.attributes.push_back({"a_BoneWeights", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneWeights)), 0, 6});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
        pipelineDesc.vertexLayout = skeletalToonVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::opaque();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            destroyLoadedShaders(shaders);
            return false;
        }

        Material skeletalToonMat;
        skeletalToonMat.name = "DefaultSkeletalToon";
        skeletalToonMat.vertexShader = shaders.vertex;
        skeletalToonMat.fragmentShader = shaders.fragment;
        skeletalToonMat.pipeline = pipeline;
        skeletalToonMat.renderLayer = RenderLayer::Opaque;
        skeletalToonMat.isTransparent = false;

        m_defaultSkeletalToonMaterial = createMaterial(skeletalToonMat);
    }

    // Create Stylized material (fantasy-style soft shading with shadow ramps)
    {
        auto shaders = loadShaders("Stylized");
        if (!shaders.success) {
            return false;
        }

        VertexBufferLayout stylizedVertexLayout;
        stylizedVertexLayout.stride = sizeof(Vertex);
        stylizedVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        stylizedVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        stylizedVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        stylizedVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});
        stylizedVertexLayout.attributes.push_back({"a_Tangent", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent)), 0, 4});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
        pipelineDesc.vertexLayout = stylizedVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::opaque();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            destroyLoadedShaders(shaders);
            return false;
        }

        Material stylizedMat;
        stylizedMat.name = "DefaultStylized";
        stylizedMat.vertexShader = shaders.vertex;
        stylizedMat.fragmentShader = shaders.fragment;
        stylizedMat.pipeline = pipeline;
        stylizedMat.renderLayer = RenderLayer::Opaque;
        stylizedMat.isTransparent = false;

        m_defaultStylizedMaterial = createMaterial(stylizedMat);
    }

    // Create Skeletal Stylized material (stylized rendering with skeletal animation)
    {
        auto shaders = loadShaders("SkeletalStylized");
        if (!shaders.success) {
            return false;
        }

        VertexBufferLayout skeletalStylizedVertexLayout;
        skeletalStylizedVertexLayout.stride = sizeof(Vertex);
        skeletalStylizedVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        skeletalStylizedVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        skeletalStylizedVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        skeletalStylizedVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});
        skeletalStylizedVertexLayout.attributes.push_back({"a_Tangent", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent)), 0, 4});
        skeletalStylizedVertexLayout.attributes.push_back({"a_BoneIDs", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneIDs)), 0, 5});
        skeletalStylizedVertexLayout.attributes.push_back({"a_BoneWeights", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneWeights)), 0, 6});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
        pipelineDesc.vertexLayout = skeletalStylizedVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::opaque();
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            destroyLoadedShaders(shaders);
            return false;
        }

        Material skeletalStylizedMat;
        skeletalStylizedMat.name = "DefaultSkeletalStylized";
        skeletalStylizedMat.vertexShader = shaders.vertex;
        skeletalStylizedMat.fragmentShader = shaders.fragment;
        skeletalStylizedMat.pipeline = pipeline;
        skeletalStylizedMat.renderLayer = RenderLayer::Opaque;
        skeletalStylizedMat.isTransparent = false;

        m_defaultSkeletalStylizedMaterial = createMaterial(skeletalStylizedMat);
    }

    // Create Transparent/Glass material (refraction and fresnel effects)
    {
        auto shaders = loadShaders("Transparent");
        if (!shaders.success) {
            return false;
        }

        VertexBufferLayout transparentVertexLayout;
        transparentVertexLayout.stride = sizeof(Vertex);
        transparentVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        transparentVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        transparentVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        transparentVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});
        transparentVertexLayout.attributes.push_back({"a_Tangent", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent)), 0, 4});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
        pipelineDesc.vertexLayout = transparentVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::alphaBlend();  // Transparent needs alpha blending
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = false;  // Don't write depth for transparent
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            destroyLoadedShaders(shaders);
            return false;
        }

        Material transparentMat;
        transparentMat.name = "DefaultTransparent";
        transparentMat.vertexShader = shaders.vertex;
        transparentMat.fragmentShader = shaders.fragment;
        transparentMat.pipeline = pipeline;
        transparentMat.renderLayer = RenderLayer::Transparent;
        transparentMat.isTransparent = true;

        m_defaultTransparentMaterial = createMaterial(transparentMat);
    }

    // Create Glow/Emissive material (for stars, lights, magic effects)
    {
        auto shaders = loadShaders("Glow");
        if (!shaders.success) {
            return false;
        }

        VertexBufferLayout glowVertexLayout;
        glowVertexLayout.stride = sizeof(Vertex);
        glowVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        glowVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        glowVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        glowVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});
        glowVertexLayout.attributes.push_back({"a_Tangent", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent)), 0, 4});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders = {shaders.vertex, shaders.fragment};
        pipelineDesc.vertexLayout = glowVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.blendState = BlendState::alphaBlend();  // Use alpha blend for glow with depth
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;  // Write depth for proper occlusion
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            destroyLoadedShaders(shaders);
            return false;
        }

        Material glowMat;
        glowMat.name = "DefaultGlow";
        glowMat.vertexShader = shaders.vertex;
        glowMat.fragmentShader = shaders.fragment;
        glowMat.pipeline = pipeline;
        glowMat.renderLayer = RenderLayer::Transparent;  // Glow renders in transparent pass
        glowMat.isTransparent = true;

        m_defaultGlowMaterial = createMaterial(glowMat);
    }

    // Create default sampler for material rendering
    // Uses Linear filtering and Wrap mode for proper texture sampling
    {
        SamplerDesc samplerDesc;
        samplerDesc.minFilter = FilterMode::Linear;
        samplerDesc.magFilter = FilterMode::Linear;
        samplerDesc.mipFilter = FilterMode::Linear;
        samplerDesc.wrapU = WrapMode::Repeat;
        samplerDesc.wrapV = WrapMode::Repeat;
        samplerDesc.wrapW = WrapMode::Repeat;
        samplerDesc.maxAnisotropy = 16.0f;
        samplerDesc.compareEnable = false;

        m_defaultSampler = m_device->createSampler(samplerDesc);
        if (!m_defaultSampler.isValid()) {
            
        }
    }

    // Create shadow sampler for shadow map sampling
    // Uses Clamp-to-edge addressing and nearest filtering for depth values
    {
        SamplerDesc shadowSamplerDesc;
        shadowSamplerDesc.minFilter = FilterMode::Nearest;
        shadowSamplerDesc.magFilter = FilterMode::Nearest;
        shadowSamplerDesc.mipFilter = FilterMode::Nearest;
        shadowSamplerDesc.wrapU = WrapMode::ClampToEdge;
        shadowSamplerDesc.wrapV = WrapMode::ClampToEdge;
        shadowSamplerDesc.wrapW = WrapMode::ClampToEdge;
        shadowSamplerDesc.maxAnisotropy = 1.0f;
        shadowSamplerDesc.compareEnable = false;  // Metal doesn't use comparison sampler like DX11

        m_shadowSampler = m_device->createSampler(shadowSamplerDesc);
        if (!m_shadowSampler.isValid()) {
            
        }
    }

    return true;
}

void RenderWorld::renderEmbeddedSubViewports(RenderView* view) {
    if (!view || !view->getScene()) {
        return;
    }

    using namespace core;

    Scene* scene = view->getScene();
    auto root = scene->GetRoot();
    if (!root) {
        return;
    }

    Node* renderRoot = view->getRenderRootNode();
    std::shared_ptr<Node> startNode = root;
    if (renderRoot) {
        startNode = renderRoot->shared_from_this();
        if (!startNode) {
            return;
        }
    }

    std::function<void(std::shared_ptr<Node>)> visit = [&](std::shared_ptr<Node> node) {
        if (!node) {
            return;
        }
        if (!node->IsActiveInHierarchy()) {
            return;
        }

        if (node.get() != renderRoot) {
            auto subViewport = node->GetComponent<lupine::components::SubViewport>();
            if (subViewport) {
                if (subViewport->IsEnabled()) {
                    // Synchronize the SubViewport's off-screen rendering against the
                    // containing view's swapchain. Required for correct Vulkan sync in
                    // multi-view frames; a no-op on backends that synchronize off-screen
                    // work implicitly (DX11/OpenGL/WebGL). The SubViewport's own view has
                    // no swapchain, so without this the hint would otherwise go stale.
                    if (view->hasSwapchain()) {
                        m_device->setSwapchainHintForOffscreen(view->getSwapchain());
                    }
                    subViewport->RenderEmbedded(this, scene);
                }
                // Do not descend: the SubViewport renders its own subtree (including
                // any nested SubViewports) into its own off-screen target.
                return;
            }
        }

        for (auto& child : node->GetChildren()) {
            visit(child);
        }
    };

    visit(startNode);
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

    // When the view is scoped to a SubViewport subtree, gather from that node
    // instead of the scene root.
    Node* renderRoot = view->getRenderRootNode();
    std::shared_ptr<Node> startNode = root;
    if (renderRoot) {
        startNode = renderRoot->shared_from_this();
        if (!startNode) {
            return;
        }
    }

    DebugDraw::SetCurrentRenderView(view);

    // Resolve UI layout for this subtree BEFORE gathering any draw commands. Layout is a
    // discrete top-down pass: it must be complete before anything reads a control's rect.
    //
    // Components used to each call ResolveSelf() from their own buildDrawCommands, which
    // made the draw pass write node transforms as a side effect. This is also what drives
    // layout in the editor viewport, which does not tick OnUpdate.
    components::UIControl::LayoutPass(startNode.get());

    // Reset the per-gather "dynamic renderable present" flag; gatherFromNode sets it
    // when a gathered renderable reports dynamic content (used by the shadow cache).
    m_AnyDynamicRenderableGathered = false;

    RenderCamera* camera = view->getCamera();
    CameraType cameraType = camera ? camera->getType() : CameraType::Camera3D;

    DebugDraw::SetCurrentCameraType(cameraType);

    std::function<void(std::shared_ptr<Node>)> gatherFromNode = [&](std::shared_ptr<Node> node) {
        if (!node) {
            return;
        }

        // Bind by const-reference: GetComponents() returns a reference to the node's
        // own vector, so a plain `auto` here would heap-copy the vector (and bump every
        // component's shared_ptr refcount) for every node, every frame.
        const auto& components = node->GetComponents();

        if (!node->IsActiveInHierarchy() || !node->IsVisibleInHierarchy()) {

            return;
        }

        // A UILayer scopes its entire subtree onto a single canvas layer so HUDs,
        // overlays and transitions stack deterministically. Save the enclosing
        // layer and restore it once this subtree is fully traversed (nested
        // UILayers override their ancestor for their own subtree).
        int previousCanvasLayer = ctx.getCanvasLayer();
        bool scopedCanvasLayer = false;
        if (auto uiLayer = std::dynamic_pointer_cast<core::UILayer>(node)) {
            ctx.setCanvasLayer(uiLayer->GetLayer());
            scopedCanvasLayer = true;
        }

        for (auto& component : components) {
            auto renderable = dynamic_cast<IRenderableComponent*>(component.get());

            if (renderable) {
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
                        if (renderable->isRenderContentDynamic()) {
                            m_AnyDynamicRenderableGathered = true;
                        }
                        renderable->buildDrawCommands(ctx);
                        m_stats.renderables++;
                    }
                }
            }
        }

        // A SubViewport owns its descendants as a separate render world. Render the
        // SubViewport node's own components (handled above, e.g. its display quad)
        // but do not descend into its subtree here - that content is rendered into
        // the SubViewport's own off-screen target. The SubViewport that scopes this
        // view (the render root) is excluded so its own content is gathered.
        if (node.get() != renderRoot && node->GetComponent<lupine::components::SubViewport>()) {
            if (scopedCanvasLayer) {
                ctx.setCanvasLayer(previousCanvasLayer);
            }
            return;
        }

        // If this node clips its descendants (a ScrollContainer, or any Container
        // with clipChildren enabled), push its clip rect as a scissor for the
        // duration of the child traversal so descendants are clipped to it. The
        // node's own draws were already recorded above and are intentionally not
        // clipped by its own clip rect (a control clips its children, not itself).
        bool pushedClip = false;
        for (auto& component : components) {
            auto* uiControl = dynamic_cast<components::UIControl*>(component.get());
            if (uiControl && uiControl->ClipsDescendants()) {
                // Already resolved by the LayoutPass above; do not re-resolve here.
                math::Rect clip = uiControl->GetClipRect();
                ctx.pushClipRect(clip.position, clip.size);
                pushedClip = true;
                break;
            }
        }

        for (auto& child : node->GetChildren()) {
            gatherFromNode(child);
        }

        if (pushedClip) {
            ctx.popClipRect();
        }

        if (scopedCanvasLayer) {
            ctx.setCanvasLayer(previousCanvasLayer);
        }
    };

    gatherFromNode(startNode);

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

    Node* renderRoot = view->getRenderRootNode();
    std::shared_ptr<Node> startNode = root;
    if (renderRoot) {
        startNode = renderRoot->shared_from_this();
        if (!startNode) {
            return;
        }
    }

    std::function<void(std::shared_ptr<Node>)> gatherFromNode = [&](std::shared_ptr<Node> node) {
        if (!node) {
            return;
        }

        if (!node->IsActiveInHierarchy() || !node->IsVisibleInHierarchy()) {
            return;
        }

        const auto& components = node->GetComponents();
        for (const auto& component : components) {

            if (!component->IsEnabled()) {
                continue;
            }

            if (auto dirLight = std::dynamic_pointer_cast<DirectionalLight3D>(component)) {
                view->m_activeLights.push_back(dirLight->ToLightDescriptor());
            }

            else if (auto omniLight = std::dynamic_pointer_cast<OmniLight3D>(component)) {
                view->m_activeLights.push_back(omniLight->ToLightDescriptor());
            }

            else if (auto spotLight = std::dynamic_pointer_cast<SpotLight3D>(component)) {
                view->m_activeLights.push_back(spotLight->ToLightDescriptor());
            }
        }

        // Lights inside a SubViewport belong to that SubViewport's own render world.
        if (node.get() != renderRoot && node->GetComponent<lupine::components::SubViewport>()) {
            return;
        }

        for (auto& child : node->GetChildren()) {
            gatherFromNode(child);
        }
    };

    gatherFromNode(startNode);

    // When over budget, keep the most important lights instead of whichever
    // ones the scene-graph traversal happened to find first.  Directional
    // lights are global and always kept; local lights rank by estimated
    // contribution at the camera position.
    if (view->m_activeLights.size() > MAX_LIGHTS_PER_FRAME) {
        Vec3 camPos(0.0f, 0.0f, 0.0f);
        if (Camera3D* camera3D = dynamic_cast<Camera3D*>(view->getCamera())) {
            camPos = camera3D->position;
        }

        auto importance = [&camPos](const LightDescriptor& light) -> float {
            if (light.type == LightType::Directional) {
                return std::numeric_limits<float>::max();
            }
            Vec3 toLight = light.position - camPos;
            float distSq = toLight.x * toLight.x + toLight.y * toLight.y + toLight.z * toLight.z;
            float rangeSq = std::max(light.range * light.range, 0.0001f);
            return light.intensity * rangeSq / (1.0f + distSq);
        };

        std::stable_sort(view->m_activeLights.begin(), view->m_activeLights.end(),
            [&importance](const LightDescriptor& a, const LightDescriptor& b) {
                return importance(a) > importance(b);
            });

        view->m_activeLights.resize(MAX_LIGHTS_PER_FRAME);
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

    Node* renderRoot = view->getRenderRootNode();
    std::shared_ptr<Node> startNode = root;
    if (renderRoot) {
        startNode = renderRoot->shared_from_this();
        if (!startNode) {
            return;
        }
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

        // A SubViewport's WorldEnvironment applies only to its own render world.
        if (node.get() != renderRoot && node->GetComponent<lupine::components::SubViewport>()) {
            return;
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

    gatherFromNode(startNode);
}

void RenderWorld::renderSkybox(IGfxCommandList* cmd, RenderView* view, const math::Mat4&) {
    static int skyboxCount = 0;
    skyboxCount++;
    bool debugSkybox = skyboxCount <= 5;

    if (debugSkybox) {

    }

    if (!cmd || !view || !view->m_activeWorldEnvironment) {
        if (debugSkybox) {

        }
        return;
    }

    // Skip skybox rendering for 2D and Canvas cameras - skybox is only for 3D viewports
    RenderCamera* camera = view->getCamera();
    if (camera) {
        CameraType cameraType = camera->getType();
        if (cameraType == CameraType::Camera2D || cameraType == CameraType::CameraCanvas) {
            if (debugSkybox) {

            }
            return;
        }
    }

    if (debugSkybox) {

    }

    using namespace components;
    WorldEnvironment* worldEnv = view->m_activeWorldEnvironment;

    if (worldEnv->GetSkyboxType() == static_cast<int>(WorldEnvironment::SkyboxType::None)) {
        if (debugSkybox) {

        }
        return;
    }

    if (debugSkybox) {

    }

    worldEnv->EnsureSkyboxResourcesCreated(m_device.get());

    if (debugSkybox) {

    }

    MeshHandle skyboxMesh = worldEnv->GetSkyboxMesh();
    if (!skyboxMesh.isValid()) {
        if (debugSkybox) {

        }
        return;
    }

    if (debugSkybox) {

    }

    const GPUMesh* mesh = m_device->getMesh(skyboxMesh);
    if (!mesh) {
        if (debugSkybox) {

        }
        return;
    }

    if (debugSkybox) {

    }

    auto shaders = loadShaders("Skybox");
    if (!shaders.success) {
        if (debugSkybox) {

        }
        return;
    }

    if (debugSkybox) {

    }

    PipelineDesc pipelineDesc;
    pipelineDesc.shaders.push_back(shaders.vertex);
    pipelineDesc.shaders.push_back(shaders.fragment);

    VertexBufferLayout layout;
    layout.stride = sizeof(Vertex);
    layout.attributes.push_back({"a_Position", VertexFormat::Float3, 0, 0, 0});
    layout.attributes.push_back({"a_Normal", VertexFormat::Float3, 12, 0, 1});
    layout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, 24, 0, 2});
    layout.attributes.push_back({"a_Color", VertexFormat::Float4, 32, 0, 3});
    pipelineDesc.vertexLayout = layout;

    pipelineDesc.depthStencilState.depthTestEnable = true;
    pipelineDesc.depthStencilState.depthWriteEnable = false;
    pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::LessEqual;

    // Disable culling for skybox - we're inside the cube looking out
    // and different backends may have different winding conventions
    pipelineDesc.rasterizerState.cullMode = CullMode::None;
    pipelineDesc.rasterizerState.frontFace = WindingOrder::CounterClockwise;
    pipelineDesc.rasterizerState.fillMode = FillMode::Solid;

    pipelineDesc.blendState.blendEnable = false;

    PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
    if (!pipeline.isValid()) {
        if (debugSkybox) {

        }
        destroyLoadedShaders(shaders);
        return;
    }

    if (debugSkybox) {

    }

    cmd->bindPipeline(pipeline);

    // Bind default sampler for skybox rendering.
    // This ensures the skybox has a valid sampler state with proper settings (Linear, Wrap/Clamp).
    // Without this, the skybox would use whatever sampler was previously bound, which could cause
    // undefined behavior or incorrect texture sampling.
    if (m_defaultSampler.isValid()) {
        cmd->bindSampler(m_defaultSampler, 0);
    }

    // camera was already retrieved at the start of this function for type checking
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

    // The skybox shader declares both a samplerCube and a sampler2D. WebGL/ANGLE fails the
    // draw ("Two textures of different types use the same sampler location") if two samplers
    // of different types resolve to the same texture unit, and every sampler defaults to unit
    // 0. Assign BOTH to their distinct units up front, whatever the skybox type, so the one
    // that goes unused this frame never collides with the one that is bound below.
    const GraphicsBackend skyboxBackend = getBackend();
    const uint32_t cubemapBinding = (skyboxBackend == GraphicsBackend::Vulkan) ? 24 : 0;
    const uint32_t panoramicBinding = (skyboxBackend == GraphicsBackend::Vulkan) ? 25 : 1;
    cmd->setUniformInt("u_CubemapTexture", static_cast<int>(cubemapBinding));
    cmd->setUniformInt("u_PanoramicTexture", static_cast<int>(panoramicBinding));

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
                    cmd->bindTexture(cubemapTexture, cubemapBinding);
                }
                break;
            }
            case WorldEnvironment::SkyboxType::Panoramic: {
                TextureHandle panoramicTexture = worldEnv->GetSkyboxTexture();
                if (panoramicTexture.isValid()) {
                    cmd->bindTexture(panoramicTexture, panoramicBinding);
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

    // Unbind skybox textures to prevent them from bleeding into subsequent draw calls.
    // Binding an invalid handle will bind a dummy texture in DX11 or unbind in OpenGL.
    cmd->bindTexture(TextureHandle(), cubemapBinding);
    cmd->bindTexture(TextureHandle(), panoramicBinding);

    // Reset sampler state to default after skybox rendering.
    // This ensures subsequent material rendering uses the correct sampler settings
    // (Linear filtering, Wrap mode) instead of whatever the skybox may have left behind.
    if (m_defaultSampler.isValid()) {
        cmd->bindSampler(m_defaultSampler, 0);
    }

    m_device->destroyPipeline(pipeline);
    m_device->destroyShader(shaders.vertex);
    m_device->destroyShader(shaders.fragment);

}

void RenderWorld::buildRenderBatches(
    const std::vector<DrawItem>& drawItems,
    std::vector<RenderPass>& outPasses,
    GeometryBatchPool* geomPool)
{
    // Hand out geometry-batch buffer slots from index 0 for this build. Slots are
    // recycled (their dynamic buffers updated in place); on a draw-list cache hit this
    // function is skipped entirely, so the cached batches keep referencing valid slots.
    if (geomPool) {
        geomPool->used = 0;
    }

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

    // Merge DrawItems that share the same pipeline+material+mesh+submesh into a
    // single RenderBatch.  This dramatically reduces per-batch overhead (pipeline
    // re-binds, sampler re-binds, SRV table allocations, shadow-map re-binds) on
    // every backend, and is the single biggest factor in DX12 SRV-pool exhaustion.
    //
    // Key: (pipeline.id, material.id, mesh.id, submeshIndex)
    struct BatchKey {
        uint32_t pipelineId;
        uint32_t materialId;
        uint32_t meshId;
        uint32_t submeshIndex;

        bool operator==(const BatchKey& o) const {
            return pipelineId == o.pipelineId && materialId == o.materialId &&
                   meshId == o.meshId && submeshIndex == o.submeshIndex;
        }
    };
    struct BatchKeyHash {
        size_t operator()(const BatchKey& k) const {
            // Simple hash combine
            size_t h = std::hash<uint32_t>()(k.pipelineId);
            h ^= std::hash<uint32_t>()(k.materialId) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<uint32_t>()(k.meshId)     + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<uint32_t>()(k.submeshIndex)+ 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    // UI clip rectangle, part of the consecutive-merge key for ordered passes.
    // executeRenderPasses applies scissor per batch from items[0]'s clip rect, so
    // DrawItems under different clip rects must never share a batch.
    struct ClipKey {
        bool enabled;
        int32_t x;
        int32_t y;
        uint32_t width;
        uint32_t height;

        bool operator==(const ClipKey& o) const {
            if (enabled != o.enabled) return false;
            if (!enabled) return true;  // clip rect irrelevant when disabled
            return x == o.x && y == o.y && width == o.width && height == o.height;
        }
        bool operator!=(const ClipKey& o) const { return !(*this == o); }
    };

    auto mergeBatches = [this](const std::vector<const DrawItem*>& items,
                               std::vector<RenderBatch>& outBatches) {
        std::unordered_map<BatchKey, size_t, BatchKeyHash> batchMap;
        batchMap.reserve(items.size());

        for (const DrawItem* item : items) {
            const Material* material = getMaterial(item->material);
            PipelineHandle pipeline;
            if (material) {
                pipeline = material->pipeline;
            }

            BatchKey key{pipeline.id, item->material.id, item->mesh.id, item->submeshIndex};
            auto it = batchMap.find(key);
            if (it != batchMap.end()) {
                outBatches[it->second].items.push_back(item);
            } else {
                size_t idx = outBatches.size();
                RenderBatch batch;
                batch.pipeline = pipeline;
                batch.material = item->material;
                batch.mesh = item->mesh;
                batch.submeshIndex = item->submeshIndex;
                batch.items.push_back(item);
                outBatches.push_back(std::move(batch));
                batchMap[key] = idx;
            }
        }
    };

    // Helper: merge only CONSECUTIVE DrawItems that share the same batch state AND
    // the same UI clip rect. Unlike mergeBatches (which groups globally and is only
    // valid when draw order is irrelevant), this preserves the exact submission order,
    // so it is safe for z-ordered 2D/Canvas passes and back-to-front transparent
    // passes while still collapsing runs of same-material/same-mesh items (tilemaps,
    // repeated sprites, particles, ColorRects) into a single batch that binds the
    // pipeline / samplers / textures once instead of once per item. Per-item state
    // (transform, tint, UV rect, texture overrides) is still applied per item inside
    // executeBatch, so merging changes only the number of redundant state binds.
    auto mergeConsecutiveBatches = [this](const std::vector<const DrawItem*>& items,
                                          std::vector<RenderBatch>& outBatches) {
        bool haveCurrent = false;
        BatchKey currentKey{};
        ClipKey currentClip{};
        for (const DrawItem* item : items) {
            const Material* material = getMaterial(item->material);
            PipelineHandle pipeline;
            if (material) {
                pipeline = material->pipeline;
            }

            BatchKey key{pipeline.id, item->material.id, item->mesh.id, item->submeshIndex};
            ClipKey clip{item->clipEnabled, item->clipX, item->clipY,
                         item->clipWidth, item->clipHeight};

            if (haveCurrent && key == currentKey && clip == currentClip) {
                outBatches.back().items.push_back(item);
            } else {
                RenderBatch batch;
                batch.pipeline = pipeline;
                batch.material = item->material;
                batch.mesh = item->mesh;
                batch.submeshIndex = item->submeshIndex;
                batch.items.push_back(item);
                outBatches.push_back(std::move(batch));
                currentKey = key;
                currentClip = clip;
                haveCurrent = true;
            }
        }
    };

    // Opaque: order doesn't matter for correctness — merge aggressively (global).
    mergeBatches(opaqueItems, opaque3DPass.batches);
    // Transparent: must preserve back-to-front order — merge only consecutive runs.
    mergeConsecutiveBatches(transparentItems, transparent3DPass.batches);
    // 2D/Canvas: order determined by Z-index. Combine bakeable quad runs into single-draw
    // geometry batches (when a pool is supplied) and consecutive state-merge the rest;
    // both preserve exact submission order and per-item clip rects.
    buildOrderedBatches2D(world2DItems, world2DPass.batches, geomPool);
    buildOrderedBatches2D(canvasItems, canvasPass.batches, geomPool);

    if (!opaque3DPass.batches.empty()) outPasses.push_back(opaque3DPass);
    if (!transparent3DPass.batches.empty()) outPasses.push_back(transparent3DPass);
    if (!world2DPass.batches.empty()) outPasses.push_back(world2DPass);
    if (!canvasPass.batches.empty()) outPasses.push_back(canvasPass);

    // Debug: Log batch counts
    static int batchLogCount = 0;
    if (batchLogCount < 10) {

        batchLogCount++;
    }

}

bool RenderWorld::is2DBakeableMaterial(MaterialHandle material) const {
    if (!material.isValid()) {
        return false;
    }
    // Only the four built-in default 2D materials use the Unlit shader whose entire
    // per-object state (model transform, tint, uv rect, single texture) folds cleanly
    // into per-vertex data. Custom .lsh / shape materials (rounded rect, gradient,
    // polygon, text) carry shape parameters that cannot be baked and are excluded.
    return material.id == m_defaultTextured2DMaterial.id ||
           material.id == m_defaultTextured2DAdditiveMaterial.id ||
           material.id == m_defaultColored2DMaterial.id ||
           material.id == m_defaultColored2DAdditiveMaterial.id;
}

void RenderWorld::buildOrderedBatches2D(
    const std::vector<const DrawItem*>& items,
    std::vector<RenderBatch>& outBatches,
    GeometryBatchPool* geomPool)
{
    const size_t n = items.size();
    if (n == 0) {
        return;
    }

    auto pipelineOf = [this](const DrawItem* item) -> PipelineHandle {
        const Material* m = getMaterial(item->material);
        return m ? m->pipeline : PipelineHandle();
    };
    auto extractTexture = [](const DrawItem* item) -> TextureHandle {
        const MaterialPropertyValue* p = item->propertyOverrides.getProperty("u_Texture");
        if (p && std::holds_alternative<TextureHandle>(*p)) {
            return std::get<TextureHandle>(*p);
        }
        return TextureHandle();
    };
    auto extractUseTexture = [](const DrawItem* item, TextureHandle tex) -> bool {
        const MaterialPropertyValue* p = item->propertyOverrides.getProperty("u_UseTexture");
        if (p && std::holds_alternative<bool>(*p)) {
            return std::get<bool>(*p);
        }
        return tex.isValid();
    };
    auto clipEqual = [](const DrawItem* a, const DrawItem* b) -> bool {
        if (a->clipEnabled != b->clipEnabled) {
            return false;
        }
        if (!a->clipEnabled) {
            return true;
        }
        return a->clipX == b->clipX && a->clipY == b->clipY &&
               a->clipWidth == b->clipWidth && a->clipHeight == b->clipHeight;
    };

    // Precompute per-item grouping attributes once.
    std::vector<bool> bakeable(n);
    std::vector<PipelineHandle> pipe(n);
    std::vector<TextureHandle> tex(n);
    std::vector<bool> useTex(n);
    for (size_t i = 0; i < n; ++i) {
        bakeable[i] = (geomPool != nullptr) && is2DBakeableMaterial(items[i]->material);
        pipe[i] = pipelineOf(items[i]);
        tex[i] = extractTexture(items[i]);
        useTex[i] = extractUseTexture(items[i], tex[i]);
    }

    // Two items can share ONE combined-geometry draw when their pipeline, material,
    // texture, useTexture flag and clip rect all match (transform/tint/uv differ but
    // are baked per-vertex).
    auto geomCompatible = [&](size_t a, size_t b) -> bool {
        return bakeable[a] && bakeable[b] &&
               items[a]->material.id == items[b]->material.id &&
               pipe[a].id == pipe[b].id &&
               tex[a].id == tex[b].id &&
               useTex[a] == useTex[b] &&
               clipEqual(items[a], items[b]);
    };
    // Two items can share ONE state-merge batch (bind once, draw per item) when their
    // pipeline, material, mesh, submesh and clip rect match.
    auto stateCompatible = [&](size_t a, size_t b) -> bool {
        return pipe[a].id == pipe[b].id &&
               items[a]->material.id == items[b]->material.id &&
               items[a]->mesh.id == items[b]->mesh.id &&
               items[a]->submeshIndex == items[b]->submeshIndex &&
               clipEqual(items[a], items[b]);
    };
    // A geometry batch of >= 2 items begins exactly at index k.
    auto geomRunStartsAt = [&](size_t k) -> bool {
        return bakeable[k] && (k + 1 < n) && geomCompatible(k, k + 1);
    };

    auto emitStateMerge = [this](const std::vector<const DrawItem*>& src,
                                 size_t begin, size_t end,
                                 std::vector<RenderBatch>& out) {
        RenderBatch batch;
        const DrawItem* first = src[begin];
        const Material* material = getMaterial(first->material);
        batch.pipeline = material ? material->pipeline : PipelineHandle();
        batch.material = first->material;
        batch.mesh = first->mesh;
        batch.submeshIndex = first->submeshIndex;
        batch.items.reserve(end - begin);
        for (size_t i = begin; i < end; ++i) {
            batch.items.push_back(src[i]);
        }
        out.push_back(std::move(batch));
    };

    size_t i = 0;
    while (i < n) {
        // Geometry batching takes priority over state merging where it applies.
        if (geomRunStartsAt(i)) {
            size_t j = i + 1;
            while (j < n && geomCompatible(i, j)) {
                ++j;
            }
            outBatches.push_back(
                buildGeometryBatch(items, i, j, pipe[i], tex[i], useTex[i], *geomPool));
            i = j;
            continue;
        }
        // Otherwise emit one consecutive state-merge batch, stopping before any geometry
        // run so the next iteration can claim it.
        size_t k = i + 1;
        while (k < n && stateCompatible(i, k) && !geomRunStartsAt(k)) {
            ++k;
        }
        emitStateMerge(items, i, k, outBatches);
        i = k;
    }
}

RenderBatch RenderWorld::buildGeometryBatch(
    const std::vector<const DrawItem*>& items,
    size_t begin,
    size_t end,
    PipelineHandle pipeline,
    TextureHandle texture,
    bool useTexture,
    GeometryBatchPool& pool)
{
    RenderBatch batch;
    batch.isGeometryBatched = true;
    batch.pipeline = pipeline;
    batch.material = items[begin]->material;
    batch.mesh = items[begin]->mesh;
    batch.submeshIndex = items[begin]->submeshIndex;
    batch.batchedTexture = texture;
    batch.batchedUseTexture = useTexture;

    const size_t quadCount = end - begin;

    // Unit-quad local geometry, matching MeshBuilder::createQuad(1,1). The 2D pipelines
    // use CullMode::None, so winding does not matter; two triangles (0,1,2)+(0,2,3).
    static const struct { float px, py, u, v; } CORNERS[4] = {
        {-0.5f, -0.5f, 0.0f, 0.0f},
        {-0.5f,  0.5f, 0.0f, 1.0f},
        { 0.5f,  0.5f, 1.0f, 1.0f},
        { 0.5f, -0.5f, 1.0f, 0.0f},
    };

    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;
    verts.reserve(quadCount * 4);
    indices.reserve(quadCount * 6);

    for (size_t it = begin; it < end; ++it) {
        const DrawItem* item = items[it];
        // Retained for per-batch scissor (items[0]), draw-list-cache pointer rebasing
        // and stats; the combined draw does not iterate these.
        batch.items.push_back(item);

        // Per-item tint folds into vertex color, uv rect folds into texcoords. The batch
        // then draws with model=identity, u_TintColor=white, u_UVRect=identity so the
        // Unlit shader reproduces the per-item result exactly. Mirror executeBatch's tint
        // resolution: base from perObject.tintColor, overridden by a u_TintColor Color.
        Vec4 tint(item->perObject.tintColor.r, item->perObject.tintColor.g,
                  item->perObject.tintColor.b, item->perObject.tintColor.a);
        const MaterialPropertyValue* tintProp = item->propertyOverrides.getProperty("u_TintColor");
        if (tintProp && std::holds_alternative<Color>(*tintProp)) {
            Color c = std::get<Color>(*tintProp);
            tint = Vec4(c.r, c.g, c.b, c.a);
        }

        Vec4 uvRect(0.0f, 0.0f, 1.0f, 1.0f);
        const MaterialPropertyValue* uvProp = item->propertyOverrides.getProperty("u_UVRect");
        if (uvProp && std::holds_alternative<Vec4>(*uvProp)) {
            uvRect = std::get<Vec4>(*uvProp);
        }

        const uint32_t base = static_cast<uint32_t>(verts.size());
        for (const auto& c : CORNERS) {
            Vec4 world = item->worldTransform * Vec4(c.px, c.py, 0.0f, 1.0f);
            Vertex v;
            v.position = Vec3(world.x, world.y, world.z);
            v.normal = Vec3(0.0f, 0.0f, -1.0f);
            v.texCoord = Vec2(uvRect.x + (uvRect.z - uvRect.x) * c.u,
                              uvRect.y + (uvRect.w - uvRect.y) * c.v);
            v.color = tint;
            verts.push_back(v);
        }
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    batch.geomIndexCount = static_cast<uint32_t>(indices.size());

    // Acquire (and grow as needed) a pooled dynamic buffer slot, then upload.
    const size_t slotIndex = pool.used++;
    if (slotIndex >= pool.slots.size()) {
        pool.slots.emplace_back();
    }
    GeometryBatchBuffers& slot = pool.slots[slotIndex];

    const uint32_t vtxCount = static_cast<uint32_t>(verts.size());
    const uint32_t idxCount = static_cast<uint32_t>(indices.size());

    if (!slot.vertexBuffer.isValid() || slot.vertexCapacity < vtxCount) {
        if (slot.vertexBuffer.isValid()) {
            m_device->destroyBuffer(slot.vertexBuffer);
            slot.vertexBuffer = BufferHandle();
        }
        // Grow in a 64-bit accumulator so the doubling can never wrap a uint32 to
        // 0 (which would allocate a tiny buffer that the upload then overruns).
        uint64_t cap64 = slot.vertexCapacity > 0 ? slot.vertexCapacity : 256u;
        while (cap64 < vtxCount) {
            cap64 *= 2u;
        }
        const uint32_t cap = static_cast<uint32_t>(std::min<uint64_t>(cap64, 0xFFFFFFFFull));
        BufferDesc desc;
        desc.size = static_cast<uint64_t>(cap) * sizeof(Vertex);
        desc.usage = BufferUsage::Vertex | BufferUsage::TransferDst;
        desc.dynamic = true;
        slot.vertexBuffer = m_device->createBuffer(desc);
        slot.vertexCapacity = slot.vertexBuffer.isValid() ? cap : 0u;
    }
    if (!slot.indexBuffer.isValid() || slot.indexCapacity < idxCount) {
        if (slot.indexBuffer.isValid()) {
            m_device->destroyBuffer(slot.indexBuffer);
            slot.indexBuffer = BufferHandle();
        }
        // 64-bit accumulator: see the vertex-capacity note above.
        uint64_t cap64 = slot.indexCapacity > 0 ? slot.indexCapacity : 384u;
        while (cap64 < idxCount) {
            cap64 *= 2u;
        }
        const uint32_t cap = static_cast<uint32_t>(std::min<uint64_t>(cap64, 0xFFFFFFFFull));
        BufferDesc desc;
        desc.size = static_cast<uint64_t>(cap) * sizeof(uint32_t);
        desc.usage = BufferUsage::Index | BufferUsage::TransferDst;
        desc.dynamic = true;
        slot.indexBuffer = m_device->createBuffer(desc);
        slot.indexCapacity = slot.indexBuffer.isValid() ? cap : 0u;
    }

    if (slot.vertexBuffer.isValid() && slot.indexBuffer.isValid()) {
        m_device->updateBuffer(slot.vertexBuffer, verts.data(),
                               static_cast<uint64_t>(vtxCount) * sizeof(Vertex), 0);
        m_device->updateBuffer(slot.indexBuffer, indices.data(),
                               static_cast<uint64_t>(idxCount) * sizeof(uint32_t), 0);
        batch.geomVertexBuffer = slot.vertexBuffer;
        batch.geomIndexBuffer = slot.indexBuffer;
    } else {
        // Allocation failed: degrade to a normal state-merge batch (executeBatch draws
        // the items individually). Correctness is preserved; only the optimization is lost.
        batch.isGeometryBatched = false;
        batch.geomIndexCount = 0;
    }

    return batch;
}

void RenderWorld::uploadLightData(IGfxCommandList* cmd, RenderView* view) {
    if (!cmd || !view || !view->m_lightUniformData) {
        return;
    }

    // Create per-view ring-buffered light UBOs if not yet created
    // Each view needs its own GPU buffers to prevent conflicts when multiple scenes render concurrently
    if (!view->m_lightUniformBuffers[0].isValid()) {
        for (uint32_t i = 0; i < RenderView::MAX_LIGHT_UBO_FRAMES; ++i) {
            view->m_lightUniformBuffers[i] = m_device->createUniformBuffer(sizeof(LightUniformBuffer));
            if (!view->m_lightUniformBuffers[i].isValid()) {
                
                return;
            }
        }
    }

    // Set ambient light and fog from WorldEnvironment
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

    // Update the current frame's light buffer with the latest data
    // This is critical for DX11 - the buffer must be updated before binding
    UniformBufferHandle currentLightBuffer = view->m_lightUniformBuffers[view->m_currentLightBufferIndex];
    if (currentLightBuffer.isValid()) {
        // Debug: Log light data being uploaded
        
        m_device->updateUniformBuffer(currentLightBuffer, view->m_lightUniformData, sizeof(LightUniformBuffer));
    }
}

// Maximum shadow map resolution to prevent GPU hangs from corrupt/uninitialized values.
static constexpr int MAX_SHADOW_RESOLUTION = 4096;

// ============================================================================
// Shadow frustum culling helpers
// ============================================================================

// A frustum defined by 6 planes (normals point inward).
// Extracted from a view-projection matrix using the Gribb-Hartmann method.
struct ShadowFrustum {
    // Each plane: (a, b, c, d) where ax + by + cz + d >= 0 is inside
    Vec4 planes[6];

    static ShadowFrustum fromMatrix(const Mat4& vp, bool zeroToOneDepth) {
        ShadowFrustum f;
        // glm::mat4 is column-major: g[col][row]
        const glm::mat4& g = vp.GetGLM();
        // Left:   row3 + row0
        f.planes[0] = Vec4(g[0][3] + g[0][0],
                            g[1][3] + g[1][0],
                            g[2][3] + g[2][0],
                            g[3][3] + g[3][0]);
        // Right:  row3 - row0
        f.planes[1] = Vec4(g[0][3] - g[0][0],
                            g[1][3] - g[1][0],
                            g[2][3] - g[2][0],
                            g[3][3] - g[3][0]);
        // Bottom: row3 + row1
        f.planes[2] = Vec4(g[0][3] + g[0][1],
                            g[1][3] + g[1][1],
                            g[2][3] + g[2][1],
                            g[3][3] + g[3][1]);
        // Top:    row3 - row1
        f.planes[3] = Vec4(g[0][3] - g[0][1],
                            g[1][3] - g[1][1],
                            g[2][3] - g[2][1],
                            g[3][3] - g[3][1]);
        // Near plane depends on the clip-space depth convention of the projection:
        //   Zero-to-one depth (DirectX/Vulkan/Metal): near = row2
        //   Negative-one-to-one depth (OpenGL/WebGL):  near = row3 + row2
        // Using the zero-to-one form on a [-1,1] matrix places the near plane at the
        // middle of the depth range, which wrongly culls roughly the near half of all
        // shadow casters and leaves the shadow map empty where the camera looks.
        if (zeroToOneDepth) {
            f.planes[4] = Vec4(g[0][2],
                                g[1][2],
                                g[2][2],
                                g[3][2]);
        } else {
            f.planes[4] = Vec4(g[0][3] + g[0][2],
                                g[1][3] + g[1][2],
                                g[2][3] + g[2][2],
                                g[3][3] + g[3][2]);
        }
        // Far:    row3 - row2
        f.planes[5] = Vec4(g[0][3] - g[0][2],
                            g[1][3] - g[1][2],
                            g[2][3] - g[2][2],
                            g[3][3] - g[3][2]);

        // Normalize each plane
        for (int i = 0; i < 6; ++i) {
            float len = std::sqrt(f.planes[i].x * f.planes[i].x +
                                  f.planes[i].y * f.planes[i].y +
                                  f.planes[i].z * f.planes[i].z);
            if (len > 0.0001f) {
                f.planes[i].x /= len;
                f.planes[i].y /= len;
                f.planes[i].z /= len;
                f.planes[i].w /= len;
            }
        }
        return f;
    }

    // Test an AABB against the frustum.  Returns true if the box is at least
    // partially inside (i.e. should be rendered).
    bool testAABB(const AABB& box) const {
        for (int i = 0; i < 6; ++i) {
            // Find the AABB vertex most in the direction of the plane normal
            // (the "positive vertex").  If it's outside, the whole box is outside.
            Vec3 pVertex(
                planes[i].x >= 0.0f ? box.max.x : box.min.x,
                planes[i].y >= 0.0f ? box.max.y : box.min.y,
                planes[i].z >= 0.0f ? box.max.z : box.min.z
            );

            float dist = planes[i].x * pVertex.x +
                         planes[i].y * pVertex.y +
                         planes[i].z * pVertex.z +
                         planes[i].w;

            if (dist < 0.0f) {
                return false;  // Entirely outside this plane
            }
        }
        return true;
    }
};

void RenderWorld::renderShadowMaps(RenderView* view, RenderContext& ctx) {
    if (!view || !view->m_lightUniformData || !m_device->isContextValid()) {
        return;
    }

    // Per-view shadow maps: Each view now has its own shadow map textures and framebuffers
    // This prevents GPU hangs in Vulkan when multiple scenes with lights render concurrently

    view->m_lightUniformData->lightCounts.x = static_cast<float>(view->m_activeLights.size());

    if (view->m_activeLights.empty()) {
        view->m_lightUniformData->lightCounts.y = 0.0f;
        view->m_lightUniformData->lightCounts.z = 0.0f;
        m_device->unbindFramebuffer();
        return;
    }

    // Static-shadow cache: this view's shadow maps and the per-light shadow matrices in
    // m_lightUniformData persist across frames, so when nothing that affects them has
    // changed we skip the entire (expensive) depth-map rasterization and reuse the
    // previous frame's results. Inputs: the render epoch (caster/light transform,
    // visibility, component content), the tree-structure version (caster/light add or
    // remove), whether a dynamic caster is present (its silhouette can change without
    // advancing the epoch), and - only when cascaded shadow maps are enabled, since
    // those cascades are fit to the camera - the camera view-projection.
    {
        const uint64_t shadowEpoch = core::Node::GetRenderEpoch();
        const uint64_t shadowTreeVersion = core::Node::GetTreeStructureVersion();
        const math::Mat4 shadowCameraVP = ctx.getViewProjectionMatrix();

        bool unchanged = view->m_HasRenderedShadows &&
                         shadowEpoch == view->m_LastShadowEpoch &&
                         shadowTreeVersion == view->m_LastShadowTreeVersion &&
                         !m_AnyDynamicRenderableGathered;
        if (unchanged && m_enableCascadedShadowMaps) {
            unchanged = (shadowCameraVP == view->m_LastShadowCameraVP);
        }
        if (unchanged) {
            return;
        }

        // Record the inputs now; m_HasRenderedShadows is only set once the rasterization
        // below actually completes (the shader-load guards may return early), so a
        // transient setup failure retries next frame instead of caching "no shadows".
        view->m_LastShadowEpoch = shadowEpoch;
        view->m_LastShadowTreeVersion = shadowTreeVersion;
        view->m_LastShadowCameraVP = shadowCameraVP;
    }

    for (size_t i = 0; i < view->m_activeLights.size() && i < MAX_LIGHTS_PER_FRAME; ++i) {
        view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(-1);

    }

    if (!m_shadowMapMaterial.isValid()) {
        auto shaders = loadShaders("ShadowMap");
        if (!shaders.success) {
            return;
        }

        VertexBufferLayout shadowVertexLayout;
        shadowVertexLayout.stride = sizeof(Vertex);
        shadowVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        shadowVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        shadowVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        shadowVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders.push_back(shaders.vertex);
        pipelineDesc.shaders.push_back(shaders.fragment);
        pipelineDesc.vertexLayout = shadowVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::Less;
        pipelineDesc.blendState = BlendState::opaque();
        // Shadow maps use DEPTH32F format, must match render target format
        pipelineDesc.depthFormat = TextureFormat::DEPTH32F;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            destroyLoadedShaders(shaders);
            return;
        }

        Material shadowMat;
        shadowMat.name = "ShadowMap";
        shadowMat.vertexShader = shaders.vertex;
        shadowMat.fragmentShader = shaders.fragment;
        shadowMat.pipeline = pipeline;
        shadowMat.renderLayer = RenderLayer::Opaque;
        shadowMat.isTransparent = false;

        m_shadowMapMaterial = createMaterial(shadowMat);

    }

    if (!m_shadowMapInstancedMaterial.isValid()) {
        auto shaders = loadShaders("ShadowMapInstanced");
        if (shaders.success) {
            VertexBufferLayout shadowVertexLayout;
            shadowVertexLayout.stride = sizeof(Vertex);
            shadowVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
            shadowVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
            shadowVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
            shadowVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});

            VertexBufferLayout instLayout;
            instLayout.stride = sizeof(InstanceVertexData);
            instLayout.inputRate = VertexInputRate::Instance;
            instLayout.attributes.push_back({"a_InstanceModel0", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol0)), 1, 4});
            instLayout.attributes.push_back({"a_InstanceModel1", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol1)), 1, 5});
            instLayout.attributes.push_back({"a_InstanceModel2", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol2)), 1, 6});
            instLayout.attributes.push_back({"a_InstanceModel3", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol3)), 1, 7});
            instLayout.attributes.push_back({"a_InstanceColor", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, color)), 1, 8});
            instLayout.attributes.push_back({"a_InstanceCustom", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, customData)), 1, 9});

            PipelineDesc pipelineDesc;
            pipelineDesc.shaders.push_back(shaders.vertex);
            pipelineDesc.shaders.push_back(shaders.fragment);
            pipelineDesc.vertexLayout = shadowVertexLayout;
            pipelineDesc.extraVertexBuffers = { instLayout };
            pipelineDesc.topology = PrimitiveTopology::TriangleList;
            pipelineDesc.rasterizerState.cullMode = CullMode::Back;
            pipelineDesc.depthStencilState.depthTestEnable = true;
            pipelineDesc.depthStencilState.depthWriteEnable = true;
            pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::Less;
            pipelineDesc.blendState = BlendState::opaque();
            pipelineDesc.depthFormat = TextureFormat::DEPTH32F;

            PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
            if (pipeline.isValid()) {
                Material shadowMat;
                shadowMat.name = "ShadowMapInstanced";
                shadowMat.vertexShader = shaders.vertex;
                shadowMat.fragmentShader = shaders.fragment;
                shadowMat.pipeline = pipeline;
                shadowMat.renderLayer = RenderLayer::Opaque;
                shadowMat.isTransparent = false;
                m_shadowMapInstancedMaterial = createMaterial(shadowMat);
            } else {
                destroyLoadedShaders(shaders);
            }
        }
    }

    if (!m_shadowCubeMaterial.isValid()) {
        auto shaders = loadShaders("ShadowCube");
        if (!shaders.success) {
            return;
        }

        VertexBufferLayout shadowVertexLayout;
        shadowVertexLayout.stride = sizeof(Vertex);
        shadowVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        shadowVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        shadowVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        shadowVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders.push_back(shaders.vertex);
        pipelineDesc.shaders.push_back(shaders.fragment);
        pipelineDesc.vertexLayout = shadowVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::Less;
        pipelineDesc.blendState = BlendState::opaque();
        // Shadow cube maps use R32_FLOAT for color and DEPTH32F for depth
        pipelineDesc.colorFormat = TextureFormat::R32_FLOAT;
        pipelineDesc.depthFormat = TextureFormat::DEPTH32F;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
        if (!pipeline.isValid()) {
            destroyLoadedShaders(shaders);
            return;
        }

        Material shadowCubeMat;
        shadowCubeMat.name = "ShadowCube";
        shadowCubeMat.vertexShader = shaders.vertex;
        shadowCubeMat.fragmentShader = shaders.fragment;
        shadowCubeMat.pipeline = pipeline;
        shadowCubeMat.renderLayer = RenderLayer::Opaque;
        shadowCubeMat.isTransparent = false;

        m_shadowCubeMaterial = createMaterial(shadowCubeMat);

    }

    if (!m_shadowCubeInstancedMaterial.isValid()) {
        auto shaders = loadShaders("ShadowCubeInstanced");
        if (shaders.success) {
            VertexBufferLayout shadowVertexLayout;
            shadowVertexLayout.stride = sizeof(Vertex);
            shadowVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
            shadowVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
            shadowVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
            shadowVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});

            VertexBufferLayout instLayout;
            instLayout.stride = sizeof(InstanceVertexData);
            instLayout.inputRate = VertexInputRate::Instance;
            instLayout.attributes.push_back({"a_InstanceModel0", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol0)), 1, 4});
            instLayout.attributes.push_back({"a_InstanceModel1", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol1)), 1, 5});
            instLayout.attributes.push_back({"a_InstanceModel2", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol2)), 1, 6});
            instLayout.attributes.push_back({"a_InstanceModel3", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, modelCol3)), 1, 7});
            instLayout.attributes.push_back({"a_InstanceColor", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, color)), 1, 8});
            instLayout.attributes.push_back({"a_InstanceCustom", VertexFormat::Float4, static_cast<uint32_t>(offsetof(InstanceVertexData, customData)), 1, 9});

            PipelineDesc pipelineDesc;
            pipelineDesc.shaders.push_back(shaders.vertex);
            pipelineDesc.shaders.push_back(shaders.fragment);
            pipelineDesc.vertexLayout = shadowVertexLayout;
            pipelineDesc.extraVertexBuffers = { instLayout };
            pipelineDesc.topology = PrimitiveTopology::TriangleList;
            pipelineDesc.rasterizerState.cullMode = CullMode::Back;
            pipelineDesc.depthStencilState.depthTestEnable = true;
            pipelineDesc.depthStencilState.depthWriteEnable = true;
            pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::Less;
            pipelineDesc.blendState = BlendState::opaque();
            pipelineDesc.colorFormat = TextureFormat::R32_FLOAT;
            pipelineDesc.depthFormat = TextureFormat::DEPTH32F;

            PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);
            if (pipeline.isValid()) {
                Material shadowCubeMat;
                shadowCubeMat.name = "ShadowCubeInstanced";
                shadowCubeMat.vertexShader = shaders.vertex;
                shadowCubeMat.fragmentShader = shaders.fragment;
                shadowCubeMat.pipeline = pipeline;
                shadowCubeMat.renderLayer = RenderLayer::Opaque;
                shadowCubeMat.isTransparent = false;
                m_shadowCubeInstancedMaterial = createMaterial(shadowCubeMat);
            } else {
                destroyLoadedShaders(shaders);
            }
        }
    }

    // Create skeletal shadow map material if needed
    if (!m_shadowMapSkeletalMaterial.isValid()) {

        auto shaders = loadShaders("ShadowMapSkeletal");

        if (!shaders.success) {
            return;
        }

        VertexBufferLayout skeletalVertexLayout;
        skeletalVertexLayout.stride = sizeof(Vertex);
        skeletalVertexLayout.attributes.push_back({"a_Position", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, position)), 0, 0});
        skeletalVertexLayout.attributes.push_back({"a_Normal", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, normal)), 0, 1});
        skeletalVertexLayout.attributes.push_back({"a_TexCoord", VertexFormat::Float2, static_cast<uint32_t>(offsetof(Vertex, texCoord)), 0, 2});
        skeletalVertexLayout.attributes.push_back({"a_Color", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, color)), 0, 3});
        skeletalVertexLayout.attributes.push_back({"a_Tangent", VertexFormat::Float3, static_cast<uint32_t>(offsetof(Vertex, tangent)), 0, 4});
        skeletalVertexLayout.attributes.push_back({"a_BoneIDs", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneIDs)), 0, 5});
        skeletalVertexLayout.attributes.push_back({"a_BoneWeights", VertexFormat::Float4, static_cast<uint32_t>(offsetof(Vertex, boneWeights)), 0, 6});

        PipelineDesc pipelineDesc;
        pipelineDesc.shaders.push_back(shaders.vertex);
        pipelineDesc.shaders.push_back(shaders.fragment);
        pipelineDesc.vertexLayout = skeletalVertexLayout;
        pipelineDesc.topology = PrimitiveTopology::TriangleList;
        pipelineDesc.rasterizerState.cullMode = CullMode::Back;
        pipelineDesc.depthStencilState.depthTestEnable = true;
        pipelineDesc.depthStencilState.depthWriteEnable = true;
        pipelineDesc.depthStencilState.depthCompareFunc = CompareFunc::Less;
        pipelineDesc.blendState = BlendState::opaque();
        // Shadow maps use DEPTH32F format, must match render target format
        pipelineDesc.depthFormat = TextureFormat::DEPTH32F;

        PipelineHandle pipeline = m_device->createPipeline(pipelineDesc);

        if (!pipeline.isValid()) {
            destroyLoadedShaders(shaders);
            return;
        }

        Material shadowSkeletalMat;
        shadowSkeletalMat.name = "ShadowMapSkeletal";
        shadowSkeletalMat.vertexShader = shaders.vertex;
        shadowSkeletalMat.fragmentShader = shaders.fragment;
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

        // Validate shadow resolution.  Valid values are powers-of-two in [256, 4096].
        // Corrupt / uninitialized values (e.g. 6804) get a safe default, not the max.
        int safeShadowResolution = light.shadowResolution;
        if (safeShadowResolution <= 0 || safeShadowResolution > MAX_SHADOW_RESOLUTION ||
            (safeShadowResolution & (safeShadowResolution - 1)) != 0) {  // not power of 2
            LOG_WARN(LogCategory::Render, "[Shadow] Light {} has invalid shadow resolution {} — using default 1024",
                     i, safeShadowResolution);
            safeShadowResolution = 1024;
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

            // Cascades consume slots from the same 8-entry shadow map sampler
            // array as regular shadow maps — clamp to what is still bindable.
            uint32_t remainingSlots = (static_cast<uint32_t>(shadowMapIndex) < MAX_SHADOW_MAPS)
                ? MAX_SHADOW_MAPS - static_cast<uint32_t>(shadowMapIndex) : 0u;
            if (remainingSlots == 0) {
                view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(-1);
                continue;
            }
            cascadeCount = std::min(cascadeCount, remainingSlots);

            float cascadeSplits[MAX_CASCADES + 1];
            calculateCascadeSplits(camera3D->nearPlane, camera3D->farPlane, cascadeCount, light.cascadeSplitLambda, cascadeSplits);

            CascadedShadowMapData& csmData = view->m_lightUniformData->cascadedShadowMaps[cascadedShadowMapIndex];
            csmData.cascadeParams.x = static_cast<float>(cascadeCount);
            csmData.cascadeParams.y = 0.001f;
            csmData.cascadeParams.z = 0.0f;
            csmData.cascadeParams.w = static_cast<float>(shadowMapIndex);

            // Backend-specific bias multiplier
            // DirectX 11 needs a larger bias due to depth precision differences
            float biasMultiplier = 1.0f;
            GraphicsBackend backend = getBackend();
            if (backend == GraphicsBackend::DirectX11) {
                biasMultiplier = 1.0f;  // Test
            }
            if (backend == GraphicsBackend::Vulkan) {
                biasMultiplier = 1.0f;
            }
            if (backend == GraphicsBackend::Metal) {
                biasMultiplier = 0.5f;
            }
            csmData.cascadeParams2.x = biasMultiplier;

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

                if (cascadeShadowMapIndex >= view->m_shadowMapFramebuffers.size()) {
                    RenderTargetDesc rtDesc;
                    rtDesc.width = safeShadowResolution;
                    rtDesc.height = safeShadowResolution;
                    rtDesc.hasColor = false;
                    rtDesc.depthFormat = TextureFormat::DEPTH32F;
                    rtDesc.hasDepth = true;
                    rtDesc.sampleCount = 1;

                    RenderTargetHandle shadowFB = m_device->createRenderTarget(rtDesc);
                    if (!shadowFB.isValid()) {

                        continue;
                    }

                    view->m_shadowMapFramebuffers.push_back(shadowFB);

                    TextureHandle depthTex = m_device->getRenderTargetDepthTexture(shadowFB);
                    if (!depthTex.isValid()) {

                        continue;
                    }

                    view->m_shadowMaps.push_back(depthTex);

                }

                renderShadowMapForLight(view, ctx, cascadeShadowMapIndex, cascadeMatrix, safeShadowResolution);
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

                if (cubeMapIndex >= view->m_shadowCubeMapFramebuffers.size()) {
                    needsRecreate = true;
                } else {

                    uint32_t currentResolution = (cubeMapIndex < view->m_shadowCubeMapResolutions.size())
                        ? view->m_shadowCubeMapResolutions[cubeMapIndex] : 0;

                    if (currentResolution != static_cast<uint32_t>(safeShadowResolution)) {

                        needsRecreate = true;

                        oldCubeRT = view->m_shadowCubeMapFramebuffers[cubeMapIndex];
                    }
                }

                if (needsRecreate) {

                    RenderTargetDesc cubeDesc;
                    cubeDesc.width = safeShadowResolution;
                    cubeDesc.height = safeShadowResolution;
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

                    if (cubeMapIndex >= view->m_shadowCubeMapFramebuffers.size()) {
                        view->m_shadowCubeMapFramebuffers.push_back(cubeRT);
                        view->m_shadowCubeMaps.push_back(colorCubeMap);
                        view->m_shadowCubeMapResolutions.push_back(safeShadowResolution);
                    } else {
                        view->m_shadowCubeMapFramebuffers[cubeMapIndex] = cubeRT;
                        view->m_shadowCubeMaps[cubeMapIndex] = colorCubeMap;
                        view->m_shadowCubeMapResolutions[cubeMapIndex] = safeShadowResolution;
                    }

                    if (oldCubeRT.isValid()) {
                        m_device->destroyRenderTarget(oldCubeRT);
                    }
                }
            }

            else {

                bool needsRecreate = false;
                RenderTargetHandle oldShadowFB;

                if (shadowMapIndex >= view->m_shadowMapFramebuffers.size()) {
                    needsRecreate = true;
                } else {

                    uint32_t currentResolution = (shadowMapIndex < view->m_shadowMapResolutions.size())
                        ? view->m_shadowMapResolutions[shadowMapIndex] : 0;

                    if (currentResolution != static_cast<uint32_t>(safeShadowResolution)) {

                        needsRecreate = true;

                        oldShadowFB = view->m_shadowMapFramebuffers[shadowMapIndex];
                    }
                }

                if (needsRecreate) {
                    RenderTargetDesc rtDesc;
                    rtDesc.width = safeShadowResolution;
                    rtDesc.height = safeShadowResolution;
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

                    if (shadowMapIndex >= view->m_shadowMapFramebuffers.size()) {
                        view->m_shadowMapFramebuffers.push_back(shadowFB);
                        view->m_shadowMaps.push_back(depthTex);
                        view->m_shadowMapResolutions.push_back(safeShadowResolution);
                    } else {
                        view->m_shadowMapFramebuffers[shadowMapIndex] = shadowFB;
                        view->m_shadowMaps[shadowMapIndex] = depthTex;
                        view->m_shadowMapResolutions[shadowMapIndex] = safeShadowResolution;
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
                view->m_lightUniformData->shadowMaps[shadowMapIndex].shadowParams2 = Vec4(static_cast<float>(safeShadowResolution), 1.0f, light.range, 0.0f);

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

                // Use appropriate projection for the graphics backend
                Mat4 lightProj = IsZeroToOneDepth(getBackend())
                    ? math::Camera::PerspectiveZO(1.5708f, 1.0f, 0.1f, light.range)
                    : math::Camera::Perspective(1.5708f, 1.0f, 0.1f, light.range);

                for (int face = 0; face < 6; ++face) {
                    Vec3 target = lightPos + faceDirections[face];
                    Mat4 lightView = math::Camera::LookAt(lightPos, target, faceUps[face]);
                    Mat4 faceMatrix = lightProj * lightView;

                    renderShadowCubeMapFace(view, ctx, cubeMapIndex, face, faceMatrix, safeShadowResolution, lightPos, light.range);
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
                // Use appropriate projection for the graphics backend
                Mat4 lightProj = IsZeroToOneDepth(getBackend())
                    ? math::Camera::PerspectiveZO(fov, 1.0f, nearPlane, farPlane)
                    : math::Camera::Perspective(fov, 1.0f, nearPlane, farPlane);

                lightSpaceMatrix = lightProj * lightView;
                validMatrix = true;

            }

            if (!validMatrix) {

                view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(-1);
                continue;
            }

            view->m_lightUniformData->shadowMaps[shadowMapIndex].lightSpaceMatrix = lightSpaceMatrix;

            float shadowBias = light.shadowBias;
            float normalBias = shadowBias * 10.0f; 
            view->m_lightUniformData->shadowMaps[shadowMapIndex].shadowParams = Vec4(shadowBias, normalBias, light.shadowBlur, light.shadowOpacity);

            float biasMultiplier = 1.0f;
            GraphicsBackend backend = getBackend();
            if (backend == GraphicsBackend::DirectX11) {
                biasMultiplier = 1.0f;
            } else if (backend == GraphicsBackend::Vulkan) {
                biasMultiplier = 1.0f;
            } else if (backend == GraphicsBackend::Metal) {
                biasMultiplier = 0.5f;
            }

            float isCubeMap = (light.type == LightType::Point) ? 1.0f : 0.0f;
            float lightRange = (light.type == LightType::Point) ? light.range : 0.0f;
            view->m_lightUniformData->shadowMaps[shadowMapIndex].shadowParams2 = Vec4(static_cast<float>(safeShadowResolution), isCubeMap, lightRange, biasMultiplier);

            renderShadowMapForLight(view, ctx, shadowMapIndex, lightSpaceMatrix, safeShadowResolution);

            view->m_lightUniformData->lights[i] = view->m_activeLights[i].toGPUData(shadowMapIndex);

            shadowMapIndex++;
        }
    }

    view->m_lightUniformData->lightCounts.y = static_cast<float>(shadowMapIndex);
    view->m_lightUniformData->lightCounts.z = static_cast<float>(cascadedShadowMapIndex);

    // Shadow maps rendered successfully this frame; the static-shadow cache may now
    // skip subsequent frames until an input (recorded above) changes.
    view->m_HasRenderedShadows = true;
}

void RenderWorld::renderShadowMapForLight(RenderView* view, RenderContext& ctx, int shadowMapIndex, const Mat4& lightSpaceMatrix, int shadowResolution) {
    if (shadowMapIndex < 0 || static_cast<size_t>(shadowMapIndex) >= view->m_shadowMapFramebuffers.size()) {
        return;
    }

    RenderTargetHandle shadowFB = view->m_shadowMapFramebuffers[shadowMapIndex];
    if (!shadowFB.isValid()) {
        return;
    }

    // Set swapchain hint for proper synchronization in multi-view scenarios
    // This ensures this view's shadow maps use this view's swapchain for sync
    if (view->hasSwapchain()) {
        m_device->setSwapchainHintForOffscreen(view->getSwapchain());
    }

    static int shadowPassCount = 0;
    shadowPassCount++;
    if (shadowPassCount <= 30) {
        LOG_INFO(LogCategory::Render, "[Shadow] renderShadowMapForLight: index={}, resolution={}", shadowMapIndex, shadowResolution);
    }

    auto cmd = m_device->beginFrame(shadowFB);
    if (!cmd) {
        LOG_ERROR(LogCategory::Render, "[Shadow] beginFrame returned null for shadow map {}", shadowMapIndex);
        return;
    }

    cmd->clearDepth(1.0f);

    const Material* shadowMaterial = getMaterial(m_shadowMapMaterial);
    const Material* shadowSkeletalMaterial = getMaterial(m_shadowMapSkeletalMaterial);
    if (!shadowMaterial) {
        LOG_ERROR(LogCategory::Render, "[Shadow] No shadow material available");
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

    // Build a frustum from the light-space matrix to cull objects that
    // cannot cast visible shadows.  This avoids drawing the entire scene
    // into every shadow map and prevents GPU TDR timeouts on large scenes.
    ShadowFrustum shadowFrustum = ShadowFrustum::fromMatrix(lightSpaceMatrix, IsZeroToOneDepth(getBackend()));

    // Bind shadow pipeline once and only switch when skeletal vs non-skeletal changes.
    // Avoids per-object pipeline bind overhead (SRV table recreation, state resets).
    PipelineHandle currentBoundPipeline;
    cmd->bindPipeline(shadowMaterial->pipeline);
    currentBoundPipeline = shadowMaterial->pipeline;

    const auto& drawItems = ctx.getDrawItems();
    int renderedCount = 0;

    for (const auto& item : drawItems) {

        if (item.spatialType != SpatialType::World3D || item.renderLayer != RenderLayer::Opaque) {
            continue;
        }

        if (!item.castShadow) {
            continue;
        }

        // Frustum cull: skip objects entirely outside the light's view volume
        if (!shadowFrustum.testAABB(item.worldBounds)) {
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

        // Instanced casters use the instanced shadow pipeline; skeletal casters
        // use the skeletal pipeline; everything else the plain shadow pipeline.
        const Material* instancedShadowMaterial = getMaterial(m_shadowMapInstancedMaterial);
        const Material* currentShadowMaterial =
            (item.isInstanced && instancedShadowMaterial) ? instancedShadowMaterial :
            (isSkeletal && shadowSkeletalMaterial) ? shadowSkeletalMaterial : shadowMaterial;
        if (currentShadowMaterial->pipeline.id != currentBoundPipeline.id) {
            cmd->bindPipeline(currentShadowMaterial->pipeline);
            currentBoundPipeline = currentShadowMaterial->pipeline;
        }

        // Push shadow uniforms using same layout as regular shaders
        // (lightSpaceMatrix at offset 0, model at offset 64)
        struct ShadowUniforms {
            Mat4 lightSpaceMatrix;
            Mat4 model;
            Mat4 normalMatrix;  // unused for shadows
            Vec4 tintColor;     // unused for shadows
        } shadowUniforms;

        shadowUniforms.lightSpaceMatrix = lightSpaceMatrix;
        shadowUniforms.model = item.worldTransform;
        shadowUniforms.normalMatrix = Mat4::Identity();
        shadowUniforms.tintColor = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

        cmd->pushConstants(ShaderStage::Vertex, &shadowUniforms, sizeof(ShadowUniforms), 0);

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

        const uint32_t shadowInstanceCount =
            (item.isInstanced && instancedShadowMaterial) ? std::max<uint32_t>(item.instanceCount, 1u) : 1u;
        if (item.isInstanced && instancedShadowMaterial && item.instanceBuffer.isValid()) {
            cmd->bindVertexBuffer(item.instanceBuffer, 1, 0);
        }

        if (item.submeshIndex < mesh->submeshes.size()) {
            const auto& submesh = mesh->submeshes[item.submeshIndex];
            cmd->drawIndexed(
                submesh.indexCount,
                shadowInstanceCount,
                submesh.indexOffset,
                submesh.vertexOffset,
                0
            );
        } else {
            cmd->drawIndexed(
                mesh->indexCount,
                shadowInstanceCount,
                0,
                0,
                0
            );
        }

        renderedCount++;
    }

    if (shadowPassCount <= 30) {
        LOG_INFO(LogCategory::Render, "[Shadow] map {}: rendered {}/{} shadow casters (frustum culled {})",
                 shadowMapIndex, renderedCount, static_cast<int>(drawItems.size()),
                 static_cast<int>(drawItems.size()) - renderedCount);
    }

    ScissorRect disableScissor{0, 0, 0, 0};
    cmd->setScissor(disableScissor);

    m_device->submit(std::move(cmd));

    m_device->unbindFramebuffer();
}

void RenderWorld::renderShadowCubeMapFace(RenderView* view, RenderContext& ctx, int cubeMapIndex, int faceIndex, const Mat4& lightSpaceMatrix, int shadowResolution, const Vec3& lightPos, float lightRange) {
    if (cubeMapIndex < 0 || static_cast<size_t>(cubeMapIndex) >= view->m_shadowCubeMapFramebuffers.size()) {
        return;
    }

    if (faceIndex < 0 || faceIndex >= 6) {
        return;
    }

    RenderTargetHandle cubeRT = view->m_shadowCubeMapFramebuffers[cubeMapIndex];

    m_device->attachCubeMapFace(cubeRT, faceIndex);

    // Set swapchain hint for proper synchronization in multi-view scenarios
    // This ensures this view's shadow cube maps use this view's swapchain for sync
    if (view->hasSwapchain()) {
        m_device->setSwapchainHintForOffscreen(view->getSwapchain());
    }

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

    const Material* instancedShadowMaterial = getMaterial(m_shadowCubeInstancedMaterial);

    PipelineHandle currentBoundPipeline = shadowMaterial->pipeline;
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

    ShadowFrustum shadowFrustum = ShadowFrustum::fromMatrix(lightSpaceMatrix, IsZeroToOneDepth(getBackend()));

    const auto& drawItems = ctx.getDrawItems();
    int renderedCount = 0;

    for (const auto& item : drawItems) {

        if (item.spatialType != SpatialType::World3D || item.renderLayer != RenderLayer::Opaque) {
            continue;
        }

        if (!item.castShadow) {
            continue;
        }

        // Frustum cull against the cube face's view volume
        if (!shadowFrustum.testAABB(item.worldBounds)) {
            continue;
        }

        const GPUMesh* mesh = m_device->getMesh(item.mesh);
        if (!mesh) {
            continue;
        }

        // Instanced casters use the instanced cube-shadow pipeline.
        const bool useInstanced = item.isInstanced && instancedShadowMaterial;
        PipelineHandle wantPipeline = useInstanced ? instancedShadowMaterial->pipeline : shadowMaterial->pipeline;
        if (wantPipeline.id != currentBoundPipeline.id) {
            cmd->bindPipeline(wantPipeline);
            currentBoundPipeline = wantPipeline;
            // Light uniforms must be re-set after a pipeline switch.
            cmd->setUniformVec3("u_LightPos", lightPos);
            cmd->setUniformFloat("u_LightRange", lightRange);
        }

        // Push shadow uniforms using same layout as regular shaders
        struct ShadowUniforms {
            Mat4 lightSpaceMatrix;
            Mat4 model;
            Mat4 normalMatrix;  // unused for shadows
            Vec4 tintColor;     // unused for shadows
        } shadowUniforms;

        shadowUniforms.lightSpaceMatrix = lightSpaceMatrix;
        shadowUniforms.model = item.worldTransform;
        shadowUniforms.normalMatrix = Mat4::Identity();
        shadowUniforms.tintColor = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

        cmd->pushConstants(ShaderStage::Vertex, &shadowUniforms, sizeof(ShadowUniforms), 0);

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

        const uint32_t cubeInstanceCount = useInstanced ? std::max<uint32_t>(item.instanceCount, 1u) : 1u;
        if (useInstanced && item.instanceBuffer.isValid()) {
            cmd->bindVertexBuffer(item.instanceBuffer, 1, 0);
        }

        if (item.submeshIndex < mesh->submeshes.size()) {
            const auto& submesh = mesh->submeshes[item.submeshIndex];
            cmd->drawIndexed(
                submesh.indexCount,
                cubeInstanceCount,
                submesh.indexOffset,
                submesh.vertexOffset,
                0
            );
        } else {
            cmd->drawIndexed(
                mesh->indexCount,
                cubeInstanceCount,
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
        // Use appropriate projection for the graphics backend
        Mat4 lightProj = IsZeroToOneDepth(getBackend())
            ? math::Camera::OrthographicZO(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 300.0f)
            : math::Camera::Orthographic(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 300.0f);
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

    // Skip texel snapping for degenerate frusta (all corners project to a
    // point in light space) — dividing by zero would propagate NaN bounds.
    if (worldUnitsPerTexel > 0.0f) {
        minX = std::floor(minX / worldUnitsPerTexel) * worldUnitsPerTexel;
        maxX = std::floor(maxX / worldUnitsPerTexel) * worldUnitsPerTexel;
        minY = std::floor(minY / worldUnitsPerTexel) * worldUnitsPerTexel;
        maxY = std::floor(maxY / worldUnitsPerTexel) * worldUnitsPerTexel;
    }

    // Use appropriate projection for the graphics backend
    math::Mat4 lightProj = IsZeroToOneDepth(getBackend())
        ? math::Camera::OrthographicZO(minX, maxX, minY, maxY, orthoNear, orthoFar)
        : math::Camera::Orthographic(minX, maxX, minY, maxY, orthoNear, orthoFar);

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
                if (a->canvasLayer != b->canvasLayer) {
                    return a->canvasLayer < b->canvasLayer;
                }
                if (a->renderLayer != b->renderLayer) {
                    return static_cast<uint32_t>(a->renderLayer) < static_cast<uint32_t>(b->renderLayer);
                }
                if (a->sortKey != b->sortKey) {
                    return a->sortKey < b->sortKey;
                }
                return a->drawOrder < b->drawOrder;
            });
            break;

        default:
            break;
    }
}

void RenderWorld::executeRenderPasses(
    RenderView* view,
    const std::vector<RenderPass>& passes,
    RenderContext&,
    bool skipClear)
{
    static int execPassCount = 0;
    execPassCount++;
    bool debugExec = execPassCount <= 5;

    if (debugExec) {

    }

    if (!m_device) {
        return;
    }

    if (debugExec) {

    }

    RenderTargetHandle target;
    if (view->hasSwapchain()) {
        target = m_device->getSwapchainBackbuffer(view->getSwapchain());
    } else if (view->hasRenderTarget()) {
        target = view->getRenderTarget();
    } else {
        return;
    }

    if (debugExec) {

    }

    m_device->unbindFramebuffer();

    if (debugExec) {

    }

    auto cmd = m_device->beginFrame(target);
    if (!cmd) {
        if (debugExec) {
            LOG_ERROR(LogCategory::Render, "executeRenderPasses: beginFrame returned null!");
            
        }
        return;
    }

    if (debugExec) {

    }

    const Viewport& vp = view->getViewport();

    cmd->setViewport(vp);

    cmd->setScissor(view->getScissor());

    if (debugExec) {

    }

    RenderCamera* camera = view->getCamera();
    // skipClear lets the ordered mid-scene capture accumulate grab objects on top of the
    // already-rendered scene in the capture target (one grab batch per call).
    if (camera && !skipClear) {
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

    if (debugExec) {

    }

    Mat4 viewProj = Mat4::Identity();
    if (camera) {
        float aspectRatio = view->getAspectRatio();
        Mat4 viewMatrix = camera->getViewMatrix();
        Mat4 proj = camera->getProjectionMatrix(aspectRatio);
        viewProj = proj * viewMatrix;
    }

    if (debugExec) {

    }

    uploadLightData(cmd.get(), view);

    if (debugExec) {

    }

    renderSkybox(cmd.get(), view, viewProj);

    if (debugExec) {

    }

    bool debugEnabled = m_debugRenderingEnabled && view->isDebugRenderingEnabled() && m_debugRenderer;
    bool gridEnabled = debugEnabled && view->isGridRenderingEnabled();
    if (gridEnabled) {
        // Set screen size for thick line rendering (geometry shader needs this)
        const Viewport& debugVp = view->getViewport();
        m_debugRenderer->setScreenSize(debugVp.width, debugVp.height);

        m_debugRenderer->beginFrame();
        renderDebugGrid(cmd.get(), view, viewProj);
        m_debugRenderer->endFrame();
        m_debugRenderer->render(cmd.get(), viewProj, 2.5f);

    }

    // Per-batch UI scissor. Canvas/World2D batches are merged only across runs of
    // items that share one clip rect (see mergeConsecutiveBatches), so every item in
    // a batch carries the same clip and items[0] is representative. Track the applied
    // scissor to avoid redundant state changes, skip batches whose clip is empty
    // (e.g. scrolled fully out of a ScrollContainer), and restore the view default
    // afterward so later draws are not clipped.
    ScissorRect appliedScissor = view->getScissor();

    for (size_t passIdx = 0; passIdx < passes.size(); ++passIdx) {
        const auto& pass = passes[passIdx];

        cmd->beginDebugMarker(getPassName(pass.type));

        for (size_t batchIdx = 0; batchIdx < pass.batches.size(); ++batchIdx) {
            const auto& batch = pass.batches[batchIdx];

            static int batchSizeLogCount = 0;
            if (batchSizeLogCount < 200) {
                const Material* bmat = getMaterial(batch.material);
                LOG_TRACE(LogCategory::Render, "[Batch] {}/{}: items={}, material='{}', mesh={}",
                          batchIdx, pass.batches.size(), batch.items.size(),
                          bmat ? bmat->name : "null", batch.mesh.id);
                batchSizeLogCount++;
            }

            const DrawItem* clipItem = batch.items.empty() ? nullptr : batch.items[0];
            if (clipItem && clipItem->clipEnabled &&
                (clipItem->clipWidth == 0 || clipItem->clipHeight == 0)) {
                // Clip intersection is empty — the item is entirely outside its
                // clipping ancestor, so draw nothing.
                continue;
            }

            ScissorRect wantScissor = view->getScissor();
            if (clipItem && clipItem->clipEnabled) {
                wantScissor = ScissorRect{clipItem->clipX, clipItem->clipY,
                                          clipItem->clipWidth, clipItem->clipHeight};
            }
            if (wantScissor != appliedScissor) {
                cmd->setScissor(wantScissor);
                appliedScissor = wantScissor;
            }

            if (batch.isGeometryBatched) {
                executeGeometryBatch(cmd.get(), batch, viewProj, view);
            } else {
                executeBatch(cmd.get(), batch, viewProj, camera, view);
            }
        }

        cmd->endDebugMarker();
    }

    // Restore the view's default scissor so custom-mesh/debug draws below and the
    // next frame are not clipped by the last UI element's clip rect.
    if (appliedScissor != view->getScissor()) {
        cmd->setScissor(view->getScissor());
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

            cmd->bindPipeline(m_device->getColorFormatVariant(
                material->pipeline, m_device->getRenderTargetColorFormat(target)));

            Mat4 mvp = viewProj * customMesh.transform;
            cmd->setUniformMat4("u_ModelViewProjection", mvp);
            cmd->setUniformMat4("u_Model", customMesh.transform);
            cmd->setUniformMat4("u_ViewProjection", viewProj);

            cmd->setUniformVec4("u_TintColor", Vec4(1.0f, 1.0f, 1.0f, 1.0f));
            cmd->setUniformVec4("u_AlbedoColor", Vec4(1.0f, 1.0f, 1.0f, 1.0f));
            cmd->setUniformVec4("u_UVRect", Vec4(0.0f, 0.0f, 1.0f, 1.0f));

            // Check if material has a texture
            bool useTexture = false;
            auto albedoIt = material->textures.find("u_AlbedoTexture");
            if (albedoIt != material->textures.end() && albedoIt->second.isValid()) {
                cmd->bindTexture(albedoIt->second, 0, 0);
                cmd->setUniformInt("u_AlbedoTexture", 0);
                useTexture = true;
            }
            cmd->setUniformBool("u_UseTexture", useTexture);

            cmd->bindVertexBuffer(gpuMesh->vertexBuffer, 0);
            cmd->bindIndexBuffer(gpuMesh->indexBuffer, IndexFormat::UInt32);

            cmd->drawIndexed(gpuMesh->indexCount, 1, 0, 0, 0);
        }

        cmd->endDebugMarker();

        view->clearCustomDrawMeshes();
    }

    if (debugEnabled) {
        // Set screen size for thick line rendering (geometry shader needs this)
        const Viewport& debugVp2 = view->getViewport();
        m_debugRenderer->setScreenSize(debugVp2.width, debugVp2.height);

        m_debugRenderer->beginFrame();
        renderDebugOverlayPostSprites(cmd.get(), view, viewProj);
        m_debugRenderer->endFrame();
        m_debugRenderer->render(cmd.get(), viewProj, 4.0f);

        cmd->setRenderTarget(target);
    }

    // User/script debug primitives (DebugDrawQueue). Independent of the editor
    // debug overlay above so debug_draw_* works in shipped runtime builds and never
    // emits editor chrome (grids/axes/gizmos).
    if (m_debugRenderer && !core::DebugDrawQueue::Get().Empty()) {
        const Viewport& userVp = view->getViewport();
        m_debugRenderer->setScreenSize(userVp.width, userVp.height);

        m_debugRenderer->beginFrame();
        core::DebugDrawQueue::Get().Submit(m_debugRenderer.get());
        m_debugRenderer->endFrame();
        m_debugRenderer->render(cmd.get(), viewProj, 2.5f);

        cmd->setRenderTarget(target);
    }


    m_device->submit(std::move(cmd));
}

namespace {
//  Per-object push-constant block shared by executeBatch (per item) and
// executeGeometryBatch (once per combined draw). Layout is the authoritative 352-byte
// contract the transpiled shaders read:
// - u_ViewProjection : offset 0   (mat4)
// - u_Model          : offset 64  (mat4)
// - u_NormalMatrix   : offset 128 (mat4)
// - u_TintColor      : offset 192 (vec4)
// - u_UseTexture     : offset 208 (int)
// - u_AlphaCutoff    : offset 212 (float) + _pad1/_pad2
// - u_UVRect         : offset 224 (vec4)
// - u_TextureFlags   : offset 240 (vec4)
// - u_MaterialParams1: offset 256 (vec4)
// - u_MaterialParams2: offset 272 (vec4)
// - u_CameraPosition : offset 288 (vec4)
// - u_AlbedoColor    : offset 304 (vec4)
// - u_EmissiveColor  : offset 320 (vec4)
// - u_ReceiveShadow  : offset 336 (int) + 12 bytes padding
struct PerObjectUniforms {
    Mat4 viewProjection;
    Mat4 model;
    Mat4 normalMatrix;
    Vec4 tintColor;
    int32_t useTexture;
    float alphaCutoff;
    float _pad1;
    float _pad2;
    Vec4 uvRect;
    Vec4 textureFlags;      // x=useAlbedo, y=useMetallicRoughness, z=useNormal, w=useEmissive
    Vec4 materialParams1;   // x=metallic, y=roughness, z=normalScale, w=emissiveStrength
    Vec4 materialParams2;   // x=alphaCutoff, y=aoStrength, z=heightScale, w=unused
    Vec4 cameraPosition;    // xyz=camera pos, w=unused
    Vec4 albedoColor;       // base material color (separate from tint)
    Vec4 emissiveColor;     // emissive color (RGB) + unused (A)
    int32_t receiveShadow;  // whether this object receives shadows
    float _pad3;
    float _pad4;
    float _pad5;
};
} // namespace

void RenderWorld::executeGeometryBatch(
    IGfxCommandList* cmd,
    const RenderBatch& batch,
    const Mat4& viewProj,
    RenderView* view)
{
    if (!cmd || !view || !batch.geomVertexBuffer.isValid() ||
        !batch.geomIndexBuffer.isValid() || batch.geomIndexCount == 0) {
        return;
    }

    // Resolve a pipeline whose render-target format matches the current target (same
    // logic as executeBatch; a no-op on backends that don't bake the format).
    RenderTargetHandle currentTarget = view->hasRenderTarget()
        ? view->getRenderTarget()
        : m_device->getSwapchainBackbuffer(view->getSwapchain());
    TextureFormat targetColorFormat = m_device->getRenderTargetColorFormat(currentTarget);
    cmd->bindPipeline(m_device->getColorFormatVariant(batch.pipeline, targetColorFormat));

    if (m_defaultSampler.isValid()) {
        cmd->bindSampler(m_defaultSampler, 0);
        cmd->bindSampler(m_defaultSampler, 1);
        cmd->bindSampler(m_defaultSampler, 2);
        cmd->bindSampler(m_defaultSampler, 3);
    }

    const bool sampleTexture = batch.batchedUseTexture && batch.batchedTexture.isValid();
    if (sampleTexture) {
        // The 2D Unlit shader reads its albedo from u_Texture at binding 0.
        cmd->bindTexture(batch.batchedTexture, 0, 0);
        cmd->setUniformInt("u_Texture", 0);
    }

    // Geometry already holds world-space positions, baked tint (vertex color) and
    // uv-rect-mapped texcoords, so the draw uses an identity model, white tint and
    // identity uv rect. Push the full block (offsets the shader expects) and also set
    // the handful of uniforms via the uniform API for backends (Vulkan) that route them
    // through a reflected UBO rather than the push constant.
    PerObjectUniforms uniforms{};
    uniforms.viewProjection = viewProj;
    uniforms.model = Mat4::Identity();
    uniforms.normalMatrix = Mat4::Identity();
    uniforms.tintColor = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    uniforms.useTexture = sampleTexture ? 1 : 0;
    uniforms.alphaCutoff = 0.5f;
    uniforms.uvRect = Vec4(0.0f, 0.0f, 1.0f, 1.0f);
    uniforms.textureFlags = Vec4(sampleTexture ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    uniforms.materialParams1 = Vec4(0.0f, 0.5f, 1.0f, 1.0f);
    uniforms.materialParams2 = Vec4(0.5f, 1.0f, 0.0f, 0.0f);
    uniforms.albedoColor = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    uniforms.emissiveColor = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    uniforms.receiveShadow = 1;

    cmd->pushConstants(ShaderStage::Vertex, &uniforms, sizeof(PerObjectUniforms), 0);
    cmd->setUniformInt("u_UseTexture", uniforms.useTexture);
    cmd->setUniformColor("u_TintColor", Color(1.0f, 1.0f, 1.0f, 1.0f));
    cmd->setUniformVec4("u_UVRect", Vec4(0.0f, 0.0f, 1.0f, 1.0f));
    if (sampleTexture) {
        cmd->setUniformVec4("u_TextureFlags", uniforms.textureFlags);
    }

    cmd->bindVertexBuffer(batch.geomVertexBuffer, 0, 0);
    cmd->bindIndexBuffer(batch.geomIndexBuffer, IndexFormat::UInt32, 0);
    cmd->drawIndexed(batch.geomIndexCount, 1, 0, 0, 0);

    m_stats.drawCalls++;
    m_stats.triangles += batch.geomIndexCount / 3;
}

void RenderWorld::executeBatch(
    IGfxCommandList* cmd,
    const RenderBatch& batch,
    const Mat4& viewProj,
    RenderCamera* camera,
    RenderView* view)
{
    static int batchCount = 0;
    batchCount++;
    bool debugBatch = batchCount <= 10;

    const Material* batchMaterial = getMaterial(batch.material);
    if (debugBatch && batchMaterial) {
        LOG_INFO(LogCategory::Render, "executeBatch: material='{}', id={}, pipeline.id={}",
                 batchMaterial->name, batch.material.id, batch.pipeline.id);
    }

    // Resolve a pipeline whose render-target format matches the target this view is
    // currently rendering into. On DX12 a pipeline built for the swapchain (e.g. R8) is
    // illegal to draw into the HDR post-process target (R16F); getColorFormatVariant
    // returns the base pipeline unchanged on backends that don't bake the format and when
    // the formats already match (so the swapchain path is a no-op).
    RenderTargetHandle currentTarget = view->hasRenderTarget()
        ? view->getRenderTarget()
        : m_device->getSwapchainBackbuffer(view->getSwapchain());
    TextureFormat targetColorFormat = m_device->getRenderTargetColorFormat(currentTarget);
    cmd->bindPipeline(m_device->getColorFormatVariant(batch.pipeline, targetColorFormat));

    if (m_defaultSampler.isValid()) {
        // Transpiled shaders give each texture its own sampler register:
        // s0 = albedo, s1 = metallicRoughness, s2 = normal, s3 = emissive
        // Bind the default sampler to all material texture slots.
        cmd->bindSampler(m_defaultSampler, 0);
        cmd->bindSampler(m_defaultSampler, 1);
        cmd->bindSampler(m_defaultSampler, 2);
        cmd->bindSampler(m_defaultSampler, 3);
    }

    if (m_shadowSampler.isValid()) {
        // Transpiled shaders place u_ShadowSampler at register(s8)
        cmd->bindSampler(m_shadowSampler, 8);
    }

    if (debugBatch) {

    }

    const Material* material = getMaterial(batch.material);
    if (!material) {
        return;
    }

    // Bind per-texture sampler overrides from .lsh @filter/@repeat hints (Gap G), after
    // the default sampler binds above so the configured slots win.
    for (const auto& cs : material->customSamplers) {
        if (cs.second.isValid()) {
            cmd->bindSampler(cs.second, static_cast<uint32_t>(cs.first));
        }
    }

    bool isLitMaterial = (material->usesLighting ||
                          material->name == "DefaultPBR" ||
                          material->name == "DefaultSkeletal" ||
                          material->name == "DefaultToon" ||
                          material->name == "DefaultSkeletalToon" ||
                          material->name == "DefaultStylized" ||
                          material->name == "DefaultSkeletalStylized" ||
                          material->name == "DefaultTransparent" ||
                          material->name == "DefaultGlow" ||
                          material->name.rfind("Custom_", 0) == 0);  

    UniformBufferHandle currentLightBuffer = view->m_lightUniformBuffers[view->m_currentLightBufferIndex];
    if (isLitMaterial && currentLightBuffer.isValid()) {
        cmd->bindUniformBuffer(currentLightBuffer, 3, 0);
    }

    if (isLitMaterial && camera) {
        Camera3D* camera3D = dynamic_cast<Camera3D*>(camera);
        if (camera3D) {
            cmd->setUniformVec3("u_CameraPosition", camera3D->position);
            cmd->setUniformMat4("u_View", camera3D->getViewMatrix());
        }
    }

    uint32_t textureUnit = 0;
    if (!isLitMaterial) {
        // Non-lit materials: bind textures starting at unit 0
        for (const auto& texturePair : material->textures) {
            cmd->bindTexture(texturePair.second, textureUnit, 0);
            cmd->setUniformInt(texturePair.first.c_str(), static_cast<int>(textureUnit));
            ++textureUnit;
        }
    } else {
        Vec4 textureFlags(0.0f, 0.0f, 0.0f, 0.0f);  

        for (const auto& texturePair : material->textures) {
            const std::string& texName = texturePair.first;
            uint32_t binding = getTextureBinding(texName);

            // Set texture flags based on texture type
            if (texName == "u_AlbedoTexture" || texName == "albedo" || texName == "diffuse") {
                textureFlags.x = 1.0f;  // useAlbedo
            } else if (texName == "u_MetallicRoughnessTexture" || texName == "metallicRoughness") {
                textureFlags.y = 1.0f;  // useMetallicRoughness
            } else if (texName == "u_NormalTexture" || texName == "normal") {
                textureFlags.z = 1.0f;  // useNormal
            } else if (texName == "u_EmissiveTexture" || texName == "emissive") {
                textureFlags.w = 1.0f;  // useEmissive
            }

            cmd->bindTexture(texturePair.second, binding, 0);
            cmd->setUniformInt(texName.c_str(), static_cast<int>(binding));

            if (texName == "u_AlbedoTexture" || texName == "albedo" || texName == "diffuse") {
                cmd->setUniformInt("u_Texture", static_cast<int>(binding));
                cmd->setUniformInt("u_AlbedoMap", static_cast<int>(binding));
            } else if (texName == "u_NormalTexture" || texName == "normal") {
                cmd->setUniformInt("u_NormalMap", static_cast<int>(binding));
            } else if (texName == "u_MetallicRoughnessTexture" || texName == "metallicRoughness") {
                cmd->setUniformInt("u_MetallicRoughnessMap", static_cast<int>(binding));
            } else if (texName == "u_EmissiveTexture" || texName == "emissive") {
                cmd->setUniformInt("u_EmissiveMap", static_cast<int>(binding));
            } else if (texName == "u_AOTexture" || texName == "ao") {
                cmd->setUniformInt("u_AOMap", static_cast<int>(binding));
            }
        }

        cmd->setUniformVec4("u_TextureFlags", textureFlags);
    }

    if (isLitMaterial) {
        const uint32_t shadowMapBaseUnit = getShadowMapBaseBinding();
        const bool isWebGL = (getBackend() == GraphicsBackend::WebGL);
        const size_t maxShadowMapsToBind = isWebGL ? 4 : 8;
        int boundShadowMaps = 0;

        for (size_t i = 0; i < maxShadowMapsToBind; ++i) {
            uint32_t unit = shadowMapBaseUnit + static_cast<uint32_t>(i);
            std::string uniformName = "u_ShadowMap" + std::to_string(i);
            cmd->setUniformInt(uniformName.c_str(), static_cast<int>(unit));

            if (i < view->m_shadowMaps.size() && view->m_shadowMaps[i].isValid()) {
                cmd->bindTexture(view->m_shadowMaps[i], unit, 0);
                ++boundShadowMaps;
            }
        }

        const uint32_t shadowCubeMapBaseUnit = getShadowCubeMapBaseBinding();
        const size_t maxShadowCubeMapsToBind = isWebGL ? 4 : 8;
        int boundShadowCubeMaps = 0;

        for (size_t i = 0; i < maxShadowCubeMapsToBind; ++i) {
            uint32_t unit = shadowCubeMapBaseUnit + static_cast<uint32_t>(i);
            std::string uniformName = "u_ShadowCubeMap" + std::to_string(i);
            cmd->setUniformInt(uniformName.c_str(), static_cast<int>(unit));

            if (i < view->m_shadowCubeMaps.size() && view->m_shadowCubeMaps[i].isValid()) {
                cmd->bindTexture(view->m_shadowCubeMaps[i], unit, 0);
                ++boundShadowCubeMaps;
            }
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

    bool materialHasTextures = !material->textures.empty();

    // Bind the captured scene color for post-process UI shaders (a ColorRect whose .lsh
    // declares u_SceneTexture). m_postProcessSceneTexture is only valid during the post-process
    // phase of renderViewPostProcess().
    if (material->usesSceneTexture && m_postProcessSceneTexture.isValid()) {
        uint32_t sceneSlot = getTextureBinding("u_SceneTexture");
        cmd->bindTexture(m_postProcessSceneTexture, sceneSlot, 0);
        cmd->setUniformInt("u_SceneTexture", static_cast<int>(sceneSlot));
    }

    // Bind vertex/index buffers once for the whole batch (same mesh for all items)
    cmd->bindVertexBuffer(mesh->vertexBuffer, 0, 0);
    cmd->bindIndexBuffer(mesh->indexBuffer, IndexFormat::UInt32, 0);

    // Track whether a previous item's property overrides changed texture bindings,
    // so we only re-bind base material textures when actually needed.
    bool needTextureRebind = false;

    int itemIdx = 0;
    for (const DrawItem* item : batch.items) {

        if (!item) {
            itemIdx++;
            continue;
        }

        // Only re-bind base material textures if a previous item's overrides
        // changed the texture slots.  The first item doesn't need this because
        // the batch-level binding above already set the material textures.
        if (needTextureRebind) {
            if (!isLitMaterial) {
                uint32_t rebindUnit = 0;
                for (const auto& texturePair : material->textures) {
                    cmd->bindTexture(texturePair.second, rebindUnit, 0);
                    ++rebindUnit;
                }
            } else {
                for (const auto& texturePair : material->textures) {
                    uint32_t binding = getTextureBinding(texturePair.first);
                    cmd->bindTexture(texturePair.second, binding, 0);
                }
            }
            needTextureRebind = false;
        }

        if (debugBatch) {

        }

        //  push constants layout
        // - u_ViewProjection: offset 0 (mat4, 64 bytes)
        // - u_Model: offset 64 (mat4, 64 bytes)
        // - u_NormalMatrix: offset 128 (mat4, 64 bytes)
        // - u_TintColor: offset 192 (vec4, 16 bytes)
        // - u_UseTexture: offset 208 (int, 4 bytes)
        // - u_AlphaCutoff: offset 212 (float, 4 bytes)
        // - _pad1, _pad2: offset 216-223 (padding)
        // - u_UVRect: offset 224 (vec4, 16 bytes)
        // PBR extensions:
        // - u_TextureFlags: offset 240 (vec4, 16 bytes)
        // - u_MaterialParams1: offset 256 (vec4, 16 bytes)
        // - u_MaterialParams2: offset 272 (vec4, 16 bytes)
        // - u_CameraPosition: offset 288 (vec4, 16 bytes)
        // - u_AlbedoColor: offset 304 (vec4, 16 bytes)
        // - u_EmissiveColor: offset 320 (vec4, 16 bytes)
        // - u_ReceiveShadow: offset 336 (int, 4 bytes) + padding (12 bytes)
        // Total: 352 bytes. Layout defined once at file scope (shared with
        // executeGeometryBatch).
        PerObjectUniforms uniforms{};

        uniforms.viewProjection = viewProj;
        uniforms.model = item->worldTransform;
        uniforms.normalMatrix = item->perObject.normalMatrix;
        uniforms.tintColor = Vec4(
            item->perObject.tintColor.r,
            item->perObject.tintColor.g,
            item->perObject.tintColor.b,
            item->perObject.tintColor.a
        );
        const MaterialPropertyValue* tintProp = item->propertyOverrides.getProperty("u_TintColor");
        if (tintProp && std::holds_alternative<Color>(*tintProp)) {
            Color c = std::get<Color>(*tintProp);
            uniforms.tintColor = Vec4(c.r, c.g, c.b, c.a);
            
        } else if (tintProp) {
            
        }

        bool itemHasTexture = materialHasTextures;
        const MaterialPropertyValue* useTexProp = item->propertyOverrides.getProperty("u_UseTexture");
        if (useTexProp && std::holds_alternative<bool>(*useTexProp)) {
            itemHasTexture = std::get<bool>(*useTexProp);
        } else {
            static const char* textureNames[] = {"u_Texture", "u_AlbedoTexture", "albedo", "diffuse"};
            for (const char* texName : textureNames) {
                const MaterialPropertyValue* texProp = item->propertyOverrides.getProperty(texName);
                if (texProp && std::holds_alternative<TextureHandle>(*texProp)) {
                    TextureHandle tex = std::get<TextureHandle>(*texProp);
                    if (tex.isValid()) {
                        itemHasTexture = true;
                        break;
                    }
                }
            }
        }
        uniforms.useTexture = itemHasTexture ? 1 : 0;

        uniforms.alphaCutoff = 0.5f;
        uniforms._pad1 = 0.0f;
        uniforms._pad2 = 0.0f;
        uniforms.uvRect = Vec4(0.0f, 0.0f, 1.0f, 1.0f);
        const MaterialPropertyValue* uvRectProp = item->propertyOverrides.getProperty("u_UVRect");
        if (uvRectProp && std::holds_alternative<Vec4>(*uvRectProp)) {
            uniforms.uvRect = std::get<Vec4>(*uvRectProp);
        }

        uniforms.textureFlags = Vec4(0.0f, 0.0f, 0.0f, 0.0f);

        // For non-lit materials (e.g., UI/rounded rects), set textureFlags.x based on itemHasTexture
        // This ensures Vulkan shaders that check u_TextureFlags.x work correctly
        if (itemHasTexture) {
            uniforms.textureFlags.x = 1.0f;
        }

        const MaterialPropertyValue* texFlagsProp = item->propertyOverrides.getProperty("u_TextureFlags");
        if (texFlagsProp && std::holds_alternative<Vec4>(*texFlagsProp)) {
            uniforms.textureFlags = std::get<Vec4>(*texFlagsProp);
        } else if (isLitMaterial) {
            for (const auto& texturePair : material->textures) {
                const std::string& texName = texturePair.first;
                if (texName == "u_AlbedoTexture" || texName == "albedo" || texName == "diffuse") {
                    uniforms.textureFlags.x = 1.0f;
                } else if (texName == "u_MetallicRoughnessTexture" || texName == "metallicRoughness") {
                    uniforms.textureFlags.y = 1.0f;
                } else if (texName == "u_NormalTexture" || texName == "normal") {
                    uniforms.textureFlags.z = 1.0f;
                } else if (texName == "u_EmissiveTexture" || texName == "emissive") {
                    uniforms.textureFlags.w = 1.0f;
                }
            }

            for (const auto& [name, value] : item->propertyOverrides.getProperties()) {
                if (std::holds_alternative<TextureHandle>(value)) {
                    TextureHandle tex = std::get<TextureHandle>(value);
                    if (tex.isValid()) {
                        if (name == "u_AlbedoTexture" || name == "albedo" || name == "diffuse") {
                            uniforms.textureFlags.x = 1.0f;
                        } else if (name == "u_MetallicRoughnessTexture" || name == "metallicRoughness") {
                            uniforms.textureFlags.y = 1.0f;
                        } else if (name == "u_NormalTexture" || name == "normal") {
                            uniforms.textureFlags.z = 1.0f;
                        } else if (name == "u_EmissiveTexture" || name == "emissive") {
                            uniforms.textureFlags.w = 1.0f;
                        }
                    }
                }
            }
        }

        // For RoundedRect shaders: u_CornerRadius uses the same offset as textureFlags (offset 240)
        const MaterialPropertyValue* cornerRadiusProp = item->propertyOverrides.getProperty("u_CornerRadius");
        if (cornerRadiusProp && std::holds_alternative<Vec4>(*cornerRadiusProp)) {
            uniforms.textureFlags = std::get<Vec4>(*cornerRadiusProp);
            
        }

        // For RadialGradient shaders: u_GradientParams uses the same offset as textureFlags (offset 240)
        const MaterialPropertyValue* gradientParamsProp = item->propertyOverrides.getProperty("u_GradientParams");
        if (gradientParamsProp && std::holds_alternative<Vec4>(*gradientParamsProp)) {
            uniforms.textureFlags = std::get<Vec4>(*gradientParamsProp);
            
        }

        // For Polygon shaders: u_PolygonParams (x=sides, y=rotation) uses the same offset as textureFlags (offset 240)
        const MaterialPropertyValue* polygonParamsProp = item->propertyOverrides.getProperty("u_PolygonParams");
        if (polygonParamsProp && std::holds_alternative<Vec4>(*polygonParamsProp)) {
            uniforms.textureFlags = std::get<Vec4>(*polygonParamsProp);
            
        }

        uniforms.materialParams1 = Vec4(0.0f, 0.5f, 1.0f, 1.0f);
        const MaterialPropertyValue* params1Prop = item->propertyOverrides.getProperty("u_MaterialParams1");
        if (params1Prop && std::holds_alternative<Vec4>(*params1Prop)) {
            uniforms.materialParams1 = std::get<Vec4>(*params1Prop);
        }

        // For RoundedRect shaders: u_Size uses the same offset as materialParams1 (offset 256)
        const MaterialPropertyValue* sizeProp = item->propertyOverrides.getProperty("u_Size");
        if (sizeProp && std::holds_alternative<Vec2>(*sizeProp)) {
            Vec2 size = std::get<Vec2>(*sizeProp);
            uniforms.materialParams1 = Vec4(size.x, size.y, 0.0f, 0.0f);
            
        }

        // x=alphaCutoff, y=aoStrength, z=heightScale, w=unused
        uniforms.materialParams2 = Vec4(0.5f, 1.0f, 0.0f, 0.0f);
        const MaterialPropertyValue* params2Prop = item->propertyOverrides.getProperty("u_MaterialParams2");
        if (params2Prop && std::holds_alternative<Vec4>(*params2Prop)) {
            uniforms.materialParams2 = std::get<Vec4>(*params2Prop);
        }

        uniforms.cameraPosition = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        if (camera) {
            Camera3D* camera3D = dynamic_cast<Camera3D*>(camera);
            if (camera3D) {
                uniforms.cameraPosition = Vec4(camera3D->position.x, camera3D->position.y, camera3D->position.z, 0.0f);
            }
        }

        uniforms.albedoColor = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
        const MaterialPropertyValue* albedoColorProp = item->propertyOverrides.getProperty("u_AlbedoColor");
        if (albedoColorProp && std::holds_alternative<Color>(*albedoColorProp)) {
            Color c = std::get<Color>(*albedoColorProp);
            uniforms.albedoColor = Vec4(c.r, c.g, c.b, c.a);
        }

        uniforms.emissiveColor = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        const MaterialPropertyValue* emissiveColorProp = item->propertyOverrides.getProperty("u_EmissiveColor");
        if (emissiveColorProp && std::holds_alternative<Color>(*emissiveColorProp)) {
            Color c = std::get<Color>(*emissiveColorProp);
            uniforms.emissiveColor = Vec4(c.r, c.g, c.b, c.a);
        } else if (emissiveColorProp && std::holds_alternative<Vec4>(*emissiveColorProp)) {
            uniforms.emissiveColor = std::get<Vec4>(*emissiveColorProp);
        }

        // Receive shadow Flag
        uniforms.receiveShadow = 1;
        const MaterialPropertyValue* receiveShadowProp = item->propertyOverrides.getProperty("u_ReceiveShadow");
        if (receiveShadowProp && std::holds_alternative<bool>(*receiveShadowProp)) {
            uniforms.receiveShadow = std::get<bool>(*receiveShadowProp) ? 1 : 0;
        } else if (receiveShadowProp && std::holds_alternative<int>(*receiveShadowProp)) {
            uniforms.receiveShadow = std::get<int>(*receiveShadowProp);
        }
        uniforms._pad3 = 0.0f;
        uniforms._pad4 = 0.0f;
        uniforms._pad5 = 0.0f;

        cmd->pushConstants(ShaderStage::Vertex, &uniforms, sizeof(PerObjectUniforms), 0);

        // For Vulkan, u_TextureFlags must also be set via uniform API to populate the UBO at binding 2
        // (Vulkan push constants don't include textureFlags - it's in a separate UBO)
        // This ensures non-lit materials like rounded rects with textures work correctly
        if (uniforms.textureFlags.x > 0.0f || uniforms.textureFlags.y > 0.0f ||
            uniforms.textureFlags.z > 0.0f || uniforms.textureFlags.w > 0.0f) {
            cmd->setUniformVec4("u_TextureFlags", uniforms.textureFlags);
        }

        if (!item->propertyOverrides.isEmpty()) {
            applyPropertyOverrides(cmd, item->propertyOverrides);

            // If overrides contain texture handles, the next item needs to
            // re-bind base material textures to undo the override.
            for (const auto& [name, value] : item->propertyOverrides.getProperties()) {
                if (std::holds_alternative<TextureHandle>(value)) {
                    needTextureRebind = true;
                    break;
                }
            }
        }

        // For post-process (u_SceneTexture) materials, drive u_SceneFlipY with the EFFECTIVE
        // flip = backend-required flip XOR the component's user flip. This makes the captured
        // scene sample upright on every backend by default (DX11/DX12/Metal flip the render
        // target relative to OpenGL/WebGL/Vulkan), while toggling "Scene Flip Y" on the component
        // always inverts it consistently across backends. Set after applyPropertyOverrides so it
        // overrides the user value carried in the property block.
        if (material->usesSceneTexture) {
            float userFlip = 0.0f;
            const MaterialPropertyValue* flipProp = item->propertyOverrides.getProperty("u_SceneFlipY");
            if (flipProp && std::holds_alternative<float>(*flipProp)) {
                userFlip = std::get<float>(*flipProp);
            }
            bool backendFlip = NeedsProjectionYFlip(getBackend());
            bool effectiveFlip = (userFlip > 0.5f) != backendFlip;  // XOR
            cmd->setUniformFloat("u_SceneFlipY", effectiveFlip ? 1.0f : 0.0f);
        }

        // Per-frame engine-fed built-ins for custom .lsh shaders (Gap E/F). Set after
        // applyPropertyOverrides so they win over any stale value in the property block.
        if (material->usesTime) {
            cmd->setUniformFloat("u_Time", elapsedSeconds());
        }
        if (material->usesScreenSize) {
            const Viewport& vp = view->getViewport();
            const float vw = std::max(1.0f, static_cast<float>(vp.width));
            const float vh = std::max(1.0f, static_cast<float>(vp.height));
            cmd->setUniformVec2("u_Resolution", Vec2(vw, vh));
            cmd->setUniformVec2("u_TexelSize", Vec2(1.0f / vw, 1.0f / vh));
        }

        // GPU instancing: bind the per-instance buffer at binding 1 and issue a
        // single instanced draw covering all instances in this item. The bound
        // instanced pipeline reads the per-instance transform/color/custom from
        // binding 1 (configured at instance step rate by the backend).
        const uint32_t instanceCount = item->isInstanced
            ? std::max<uint32_t>(item->instanceCount, 1u)
            : 1u;
        if (item->isInstanced && item->instanceBuffer.isValid()) {
            cmd->bindVertexBuffer(item->instanceBuffer, 1, 0);
        }

        if (submesh) {

            cmd->drawIndexed(
                submesh->indexCount,
                instanceCount,
                submesh->indexOffset,
                submesh->vertexOffset,
                0
            );
            m_stats.triangles += (submesh->indexCount / 3) * instanceCount;
        } else {

            cmd->drawIndexed(
                mesh->indexCount,
                instanceCount,
                0,
                0,
                0
            );
            m_stats.triangles += (mesh->indexCount / 3) * instanceCount;
        }

        m_stats.drawCalls++;
        itemIdx++;
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

void RenderWorld::renderDebugGrid(IGfxCommandList* cmd, RenderView* view, const Mat4&) {
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

        // Infinite-feeling grid: snap the grid to the camera and size it to cover the
        // entire visible region, so it keeps filling the viewport at any zoom or pan.
        float aspectRatio = view->getAspectRatio();
        if (aspectRatio <= 0.0f) {
            aspectRatio = 1.0f;
        }

        float zoom = camera2D->zoom;
        if (zoom < 0.0001f) {
            zoom = 0.0001f;
        }

        float visibleHeight = camera2D->orthoSize / zoom;
        float visibleWidth = visibleHeight * aspectRatio;

        // Pick a cell size that stays a power-of-two multiple of the base spacing so
        // the on-screen density holds steady as the camera zooms in and out.
        const float baseCellSize = 100.0f;
        const float targetMaxCellsPerHeight = 40.0f;
        const float targetMinCellsPerHeight = 10.0f;

        float cellSize = baseCellSize;
        while (visibleHeight / cellSize > targetMaxCellsPerHeight) {
            cellSize *= 2.0f;
        }
        while (cellSize > baseCellSize && visibleHeight / cellSize < targetMinCellsPerHeight) {
            cellSize *= 0.5f;
        }

        // The diagonal covers the visible region for any camera rotation, plus margin.
        float visibleHalfDiagonal = 0.5f * std::sqrt(visibleWidth * visibleWidth + visibleHeight * visibleHeight);
        int halfCells = static_cast<int>(std::ceil(visibleHalfDiagonal / cellSize)) + 2;
        int cellCount = halfCells * 2;

        // Snap the grid center to a cell boundary near the camera so the lines stay
        // anchored in world space while the camera pans.
        float centerX = std::floor(camera2D->position.x / cellSize) * cellSize;
        float centerY = std::floor(camera2D->position.y / cellSize) * cellSize;

        math::Color gridColor(0.35f, 0.35f, 0.35f, 1.0f);

        DebugGrid grid2D;
        grid2D.cellSize = cellSize;
        grid2D.cellCount = cellCount;
        grid2D.center = math::Vec3(centerX, centerY, 0.0f);
        grid2D.gridColor = gridColor;
        // Match the axis color to the grid color so the snapped center line is not
        // mistaken for the world axes (drawn explicitly below).
        grid2D.axisColor = gridColor;
        grid2D.is3D = false;

        m_debugRenderer->draw2DGrid(grid2D);

        // Draw the true world-origin axes across the visible extent so the origin stays
        // identifiable regardless of where the camera is panned.
        float halfExtent = halfCells * cellSize;
        math::Color axisColor(0.45f, 0.45f, 0.45f, 1.0f);
        m_debugRenderer->drawLine(
            math::Vec3(centerX - halfExtent, 0.0f, 0.0f),
            math::Vec3(centerX + halfExtent, 0.0f, 0.0f),
            axisColor, 0.0f, false);
        m_debugRenderer->drawLine(
            math::Vec3(0.0f, centerY - halfExtent, 0.0f),
            math::Vec3(0.0f, centerY + halfExtent, 0.0f),
            axisColor, 0.0f, false);

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

void RenderWorld::renderDebugOverlayPostSprites(IGfxCommandList* cmd, RenderView* view, const Mat4&) {
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

                    math::Vec2 unionMin(FLT_MAX, FLT_MAX);
                    math::Vec2 unionMax(-FLT_MAX, -FLT_MAX);
                    bool hasBounds = false;

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

                                        math::Vec3 bmin = worldBounds.min;
                                        math::Vec3 bmax = worldBounds.max;
                                        unionMin.x = std::min(unionMin.x, bmin.x);
                                        unionMin.y = std::min(unionMin.y, bmin.y);
                                        unionMax.x = std::max(unionMax.x, bmax.x);
                                        unionMax.y = std::max(unionMax.y, bmax.y);
                                        hasBounds = true;
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

                        // Derive the oriented half-extents of the selection rectangle for
                        // the scale gizmo. A single selection recovers the true rotated
                        // rectangle from its world AABB; multi-selection uses the union AABB.
                        if (hasBounds) {
                            math::Vec2 aabbHalf((unionMax.x - unionMin.x) * 0.5f,
                                                (unionMax.y - unionMin.y) * 0.5f);
                            if (nodesToConsider.size() == 1 && std::abs(gizmo.rotation2D) > 0.0001f) {
                                gizmo.halfExtents = GizmoUtils::SolveOrientedHalfExtents2D(aabbHalf, gizmo.rotation2D);
                            } else {
                                gizmo.halfExtents = aabbHalf;
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

    bool hasExplicitTextureFlags = false;
    auto it = overrides.getProperties().find("u_TextureFlags");
    if (it != overrides.getProperties().end() && std::holds_alternative<Vec4>(it->second)) {
        hasExplicitTextureFlags = true;
    }

    bool hasAlbedoOverride = false;
    bool hasMetallicRoughnessOverride = false;
    bool hasNormalOverride = false;
    bool hasEmissiveOverride = false;

    for (const auto& [name, value] : overrides.getProperties()) {

        if (std::holds_alternative<TextureHandle>(value)) {
            TextureHandle texture = std::get<TextureHandle>(value);
            uint32_t textureUnit = getTextureBinding(name);
            cmd->bindTexture(texture, textureUnit, 0);
            cmd->setUniformInt(name.c_str(), static_cast<int>(textureUnit));
            if (name == "u_AlbedoTexture" || name == "albedo" || name == "diffuse") {
                cmd->setUniformInt("u_Texture", static_cast<int>(textureUnit));
                cmd->setUniformInt("u_AlbedoMap", static_cast<int>(textureUnit));
            } else if (name == "u_NormalTexture" || name == "normal") {
                cmd->setUniformInt("u_NormalMap", static_cast<int>(textureUnit));
            } else if (name == "u_MetallicRoughnessTexture" || name == "metallicRoughness") {
                cmd->setUniformInt("u_MetallicRoughnessMap", static_cast<int>(textureUnit));
            } else if (name == "u_EmissiveTexture" || name == "emissive") {
                cmd->setUniformInt("u_EmissiveMap", static_cast<int>(textureUnit));
            } else if (name == "u_AOTexture" || name == "ao") {
                cmd->setUniformInt("u_AOMap", static_cast<int>(textureUnit));
            }
            if (texture.isValid()) {
                if (name == "u_AlbedoTexture" || name == "albedo" || name == "diffuse") {
                    hasAlbedoOverride = true;
                } else if (name == "u_MetallicRoughnessTexture" || name == "metallicRoughness") {
                    hasMetallicRoughnessOverride = true;
                } else if (name == "u_NormalTexture" || name == "normal") {
                    hasNormalOverride = true;
                } else if (name == "u_EmissiveTexture" || name == "emissive") {
                    hasEmissiveOverride = true;
                }
            }
        }

        else if (std::holds_alternative<Color>(value)) {
            Color color = std::get<Color>(value);
            // Debug: log color values before sending to backend
            static int colorDebugCount = 0;
            if (colorDebugCount < 20 && (name == "u_Color" || name == "u_TintColor")) {
                
                colorDebugCount++;
            }
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

    // Update u_TextureFlags if any texture overrides were applied.
    if (hasAlbedoOverride || hasMetallicRoughnessOverride || hasNormalOverride || hasEmissiveOverride) {
        Vec4 textureFlags(
            hasAlbedoOverride ? 1.0f : 0.0f,
            hasMetallicRoughnessOverride ? 1.0f : 0.0f,
            hasNormalOverride ? 1.0f : 0.0f,
            hasEmissiveOverride ? 1.0f : 0.0f
        );
        cmd->setUniformVec4("u_TextureFlags", textureFlags);
    }
}

}

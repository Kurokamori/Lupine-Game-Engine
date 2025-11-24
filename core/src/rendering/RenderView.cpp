#include "lupine/rendering/RenderView.hpp"
#include "lupine/rendering/RenderCamera.hpp"
#include "lupine/rendering/Light.hpp"

namespace lupine {

RenderView::RenderView(RenderViewID id, std::unique_ptr<RenderCamera> camera)
    : m_id(id)
    , m_name("RenderView_" + std::to_string(id))
    , m_camera(std::move(camera))
    , m_swapchain()
    , m_renderTarget()
    , m_viewport()
    , m_scissor()
    , m_scene(nullptr)
    , m_renderLayerMask(0xFFFFFFFF)
    , m_enable2D(true)
    , m_enable3D(true)
    , m_enableCanvas(true)
    , m_lightUniformData(nullptr)
{

    m_lightUniformData = new LightUniformBuffer();
}

RenderView::~RenderView() {

    delete m_lightUniformData;
    m_lightUniformData = nullptr;
}

void RenderView::setSwapchain(SwapchainHandle swapchain) {
    m_swapchain = swapchain;
    m_renderTarget = RenderTargetHandle();
}

void RenderView::setRenderTarget(RenderTargetHandle target) {
    m_renderTarget = target;
    m_swapchain = SwapchainHandle();
}

float RenderView::getAspectRatio() const {
    if (m_viewport.width > 0.0f && m_viewport.height > 0.0f) {
        return m_viewport.width / m_viewport.height;
    }
    return 16.0f / 9.0f;
}

void RenderView::addCustomDrawMesh(MeshHandle mesh, MaterialHandle material, const math::Mat4& transform) {
    CustomDrawMesh customMesh;
    customMesh.mesh = mesh;
    customMesh.material = material;
    customMesh.transform = transform;
    m_customDrawMeshes.push_back(customMesh);
}

void RenderView::clearCustomDrawMeshes() {
    m_customDrawMeshes.clear();
}

}


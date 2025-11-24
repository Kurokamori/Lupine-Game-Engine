# Lupine Engine Rendering Architecture

## Overview

The Lupine Engine rendering system is designed to be **API-agnostic**, **multi-surface**, and **multi-view**. It supports multiple graphics backends (OpenGL, Vulkan, Metal, DirectX 11/12, WebGL) and can render to multiple windows/surfaces simultaneously (PyQt widgets, SDL windows, standalone windows).

## Architecture Layers

The rendering system is organized into three distinct layers:

### 1. Graphics Device Layer (Low-Level)
**Location:** `lupine/rendering/gfx/` and `lupine/rendering/backends/`

This is the lowest layer that interfaces directly with graphics APIs.

- **IGfxDevice**: Abstract interface for all graphics operations
- **Backend Implementations**: Concrete implementations for each API
  - `GfxDeviceOpenGL` - OpenGL 3.3+ / OpenGL ES 3.0+
  - `GfxDeviceVulkan` - Vulkan 1.0+
  - `GfxDeviceMetal` - Metal 2.0+ (macOS/iOS)
  - `GfxDeviceWebGL` - WebGL 2.0
  - `GfxDeviceDX11` - DirectX 11
  - `GfxDeviceDX12` - DirectX 12

**Key Point:** Core rendering code *never* calls `gl*`, `vk*`, `ID3D*` functions directly. Everything goes through `IGfxDevice`.

### 2. Render Context Layer (Mid-Level)
**Location:** `lupine/rendering/`

This layer provides the high-level API for game/editor code to issue rendering commands.

- **RenderContext**: API for nodes/components to record draw commands
  - `drawMesh()`, `drawSprite()`, `drawQuad()`, `drawText()`, etc.
  - Does NOT execute GPU commands immediately
  - Records `DrawItem` structures into a list

- **RenderView**: Combines a camera + render target
  - Each editor tab or game window has its own `RenderView`
  - Contains viewport, scissor, and render settings

- **Cameras**: Three camera types
  - `Camera2D` - Orthographic, for 2D world (Node2D)
  - `Camera3D` - Perspective/Orthographic, for 3D world (Node3D)
  - `CameraCanvas` - Screen-space, for UI/Canvas rendering

### 3. Render World Layer (High-Level)
**Location:** `lupine/rendering/RenderWorld.hpp`

This is the orchestrator that manages the entire rendering pipeline.

- **RenderWorld**: Main rendering system
  - Owns the graphics device
  - Manages multiple render views
  - Manages global resources (materials, pipelines, etc.)
  - Orchestrates the per-frame rendering pipeline

## Per-Frame Rendering Pipeline

Each frame follows this pipeline:

```
1. RenderWorld::beginFrame()
   ↓
2. For each RenderView:
   a. Create RenderContext
   b. Gather visible renderables → buildDrawCommands()
   c. Collect DrawItems
   ↓
3. Batch & Sort DrawItems:
   a. Partition by render pass (Opaque3D, Transparent3D, 2D, Canvas)
   b. Sort within each pass:
      - Opaque: pipeline → material → mesh → distance (front-to-back)
      - Transparent: depth (back-to-front)
      - UI: layer → orderInLayer → material
   ↓
4. Execute Render Passes:
   a. For each RenderView:
      - Get command list from IGfxDevice
      - Set render target, viewport, scissor
      - Clear color/depth
      - For each batch:
        * Bind pipeline
        * Bind material resources
        * Upload per-object constants
        * Issue draw calls
   ↓
5. RenderWorld::endFrame()
   - Present swapchains
```

## Component Rendering Interface

To make a component renderable, implement `IRenderableComponent`:

```cpp
class MyComponent : public Component, public IRenderableComponent {
public:
    void buildDrawCommands(RenderContext& ctx) override {
        // Issue rendering commands
        ctx.drawMesh(m_mesh, m_material, getWorldTransform());
    }

    AABB getWorldBounds() const override {
        return m_bounds;
    }
};
```

## Multi-Window / Multi-Surface Support

The system supports rendering to different surface types:

### Creating a Swapchain

```cpp
// For a PyQt widget
NativeWindowHandle qtHandle;
qtHandle.platformHandle = (void*)widget->winId();

SwapchainDesc desc;
desc.window = qtHandle;
desc.width = 1920;
desc.height = 1080;
desc.colorFormat = TextureFormat::RGBA8_SRGB;
desc.vsync = true;

SwapchainHandle swapchain = device->createSwapchain(desc);
```

### Creating a RenderView

```cpp
// Create a view with a 3D camera
auto camera = std::make_unique<Camera3D>();
camera->position = Vec3(0, 5, 10);
camera->target = Vec3(0, 0, 0);

RenderViewID viewID = renderWorld->createRenderView(std::move(camera));
RenderView* view = renderWorld->getRenderView(viewID);
view->setSwapchain(swapchain);
view->setScene(myScene);
```

## Partial Rendering

Each `RenderView` can enable/disable specific render passes:

```cpp
// 2D-only editor tab
view->setRenderPass2DEnabled(true);
view->setRenderPass3DEnabled(false);
view->setRenderPassCanvasEnabled(false);

// 3D-only viewport
view->setRenderPass2DEnabled(false);
view->setRenderPass3DEnabled(true);
view->setRenderPassCanvasEnabled(true);
```

## Render Layers

Use `RenderLayer` to control draw order and filtering:

```cpp
enum class RenderLayer : uint32_t {
    Default = 0,
    Background = 100,
    Opaque = 1000,
    Transparent = 2000,
    UI = 3000,
    Overlay = 4000,
    Debug = 5000
};
```

Each camera has a `renderLayerMask` to filter which layers it renders:

```cpp
// Only render UI and overlay
camera->renderLayerMask = (1 << RenderLayer::UI) | (1 << RenderLayer::Overlay);
```

## Materials & Shaders

Materials combine shaders, textures, and rendering state:

```cpp
Material mat;
mat.name = "MyMaterial";
mat.vertexShader = vShader;
mat.fragmentShader = fShader;
mat.pipeline = pipeline;
mat.renderLayer = RenderLayer::Opaque;
mat.isTransparent = false;

MaterialHandle handle = renderWorld->createMaterial(mat);
```

## Cross-Backend Consistency

To ensure consistent visuals across backends:

1. **Coordinate System**: Establish a canonical coordinate system (right-handed/left-handed)
2. **Depth Range**: Standardize clip-space depth [0,1] or [-1,1], adapt per backend
3. **Winding Order**: Use consistent front-face winding (CW/CCW)
4. **sRGB Handling**: Enable/disable sRGB formats consistently
5. **Y-Axis Direction**: Ensure UI isn't upside down on any backend

Each backend adapts its projection matrices and raster state accordingly.

## File Structure

```
lupine/rendering/
├── Rendering.hpp              # Main include file
├── README.md                  # This file
├── ResourceHandles.hpp        # Type-safe resource handles
├── Material.hpp               # Material definitions
├── RenderCamera.hpp           # Camera types (2D/3D/Canvas)
├── RenderView.hpp             # View = Camera + Surface
├── DrawCommand.hpp            # Draw items, batches, passes
├── RenderContext.hpp          # High-level draw API
├── RenderWorld.hpp            # Main rendering orchestrator
├── gfx/
│   ├── GfxTypes.hpp          # Common enums and types
│   ├── GfxDescriptors.hpp    # Resource creation descriptors
│   ├── GfxCommandList.hpp    # Command recording interface
│   └── IGfxDevice.hpp        # Abstract device interface
└── backends/
    ├── GfxDeviceOpenGL.hpp
    ├── GfxDeviceVulkan.hpp
    ├── GfxDeviceMetal.hpp
    ├── GfxDeviceWebGL.hpp
    ├── GfxDeviceDX11.hpp
    └── GfxDeviceDX12.hpp
```

## Usage Example

```cpp
// Initialize rendering
RenderWorld renderWorld;
renderWorld.initialize(GraphicsBackend::Vulkan);

// Create a swapchain for a window
SwapchainDesc swapchainDesc;
swapchainDesc.window = windowHandle;
swapchainDesc.width = 1920;
swapchainDesc.height = 1080;
SwapchainHandle swapchain = renderWorld.getDevice()->createSwapchain(swapchainDesc);

// Create a view
auto camera = std::make_unique<Camera3D>();
RenderViewID viewID = renderWorld.createRenderView(std::move(camera));
RenderView* view = renderWorld.getRenderView(viewID);
view->setSwapchain(swapchain);
view->setScene(scene);

// Render loop
while (running) {
    renderWorld.beginFrame();
    renderWorld.renderAllViews();
    renderWorld.endFrame();
}

// Cleanup
renderWorld.shutdown();
```

## Future Extensions

- **Instanced Rendering**: Batch items with same mesh/material but different transforms
- **Compute Shaders**: Add compute pipeline support
- **Ray Tracing**: Add ray tracing pipeline for supported backends
- **Multi-threaded Rendering**: Parallel command recording
- **Shadow Mapping**: Dedicated shadow pass
- **Post-Processing**: Full-screen effects pipeline
- **Shader Hot-Reload**: Runtime shader recompilation

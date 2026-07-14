#include "lupine/rendering/backends/vulkan/GfxDeviceVulkan.hpp"

#ifdef LUPINE_HAS_VULKAN

#include "lupine/rendering/backends/vulkan/VulkanState.hpp"
#include "lupine/rendering/backends/vulkan/GfxCommandListVulkan.hpp"
#include "lupine/rendering/backends/vulkan/GLSLCompiler.hpp"
#include "lupine/rendering/Mesh.hpp"
#include "lupine/rendering/Font.hpp"
#include "lupine/rendering/FontBaker.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/FileSystem.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/asset/Asset.hpp"
#include <stb_truetype.h>
#include <vector>
#include <array>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <cstring>
#include <fstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#endif

namespace lupine {

// Validation layer callback
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT*,
    void*) {

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        
    }
    return VK_FALSE;
}

// Helper to load debug utils extension functions
static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

struct GfxDeviceVulkan::Impl {
    VulkanState state;
    bool initialized = false;
    bool enableValidation = true;

    // Current frame state
    GfxCommandListVulkan* currentCommandList = nullptr;
    std::vector<VkCommandBuffer> commandBuffers;
    uint32_t currentFrameIndex = 0;

    // Frame-in-progress tracking to prevent multiple fence waits per frame
    // Maps swapchain ID to whether a frame is currently in progress
    std::unordered_map<uint32_t, bool> frameInProgress;

    // Track swapchains that had acquire failures and need recovery
    // After acquire fails, semaphore state is undefined - need device idle before retry
    std::unordered_set<uint32_t> swapchainNeedsRecovery;

    // Track whether deferred destructions have been processed this frame
    // With multiple swapchains, we must only process once per frame
    bool deferredDestructionsProcessedThisFrame = false;

    // Track which frame indices have had descriptor sets reset this application frame
    // With multiple swapchains at different frame indices, we must only reset each once
    std::array<bool, VulkanState::MAX_FRAMES_IN_FLIGHT> descriptorSetsResetThisFrame = {false, false};

    // Default resources
    SamplerHandle defaultSampler;
    TextureHandle whiteTexture;
    TextureHandle blackTexture;
    TextureHandle normalTexture;

    // Swapchain being rendered to
    SwapchainHandle activeSwapchain;

    // Hint for which swapchain to use when rendering to off-screen targets
    // This is set by setSwapchainHintForOffscreen() and used by beginFrame()
    // to ensure each view's shadow maps use that view's swapchain for sync
    SwapchainHandle offscreenSwapchainHint;

    // Mesh storage
    std::unordered_map<uint32_t, GPUMesh> meshes;
    uint32_t nextMeshID = 1;

    // Font storage
    std::unordered_map<uint32_t, FontAtlas> fonts;
    std::unordered_map<uint32_t, FontDesc> fontDescs;
    uint32_t nextFontID = 1;
};

GfxDeviceVulkan::GfxDeviceVulkan() : m_impl(std::make_unique<Impl>()) {
}

GfxDeviceVulkan::~GfxDeviceVulkan() {
    shutdown();
}

bool GfxDeviceVulkan::initialize() {

    // Initialize GLSL compiler
    GLSLCompiler::initialize();

    // Check for validation layer support
    std::vector<const char*> validationLayers;
    if (m_impl->enableValidation) {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        bool validationFound = false;
        for (const auto& layer : availableLayers) {
            if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                validationFound = true;
                break;
            }
        }

        if (validationFound) {
            validationLayers.push_back("VK_LAYER_KHRONOS_validation");
        } else {
            
        }
    }

    // Get required extensions
    std::vector<const char*> extensions;
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef _WIN32
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
    if (m_impl->enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Create instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Lupine Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Lupine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;  // Required for dynamic rendering

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();

    // Debug messenger for instance creation/destruction
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_impl->enableValidation && !validationLayers.empty()) {
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        createInfo.pNext = &debugCreateInfo;
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_impl->state.instance);
    if (result != VK_SUCCESS) {
        
        return false;
    }

    // Setup debug messenger
    if (m_impl->enableValidation && !validationLayers.empty()) {
        result = CreateDebugUtilsMessengerEXT(m_impl->state.instance, &debugCreateInfo, nullptr, &m_impl->state.debugMessenger);
        if (result != VK_SUCCESS) {
            
        }
    }

    // Pick physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_impl->state.instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_impl->state.instance, &deviceCount, devices.data());

    // Select the best device (prefer discrete GPU)
    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            m_impl->state.physicalDevice = device;
            break;
        }
    }

    // Fallback to first device if no discrete GPU found
    if (m_impl->state.physicalDevice == VK_NULL_HANDLE) {
        m_impl->state.physicalDevice = devices[0];
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_impl->state.physicalDevice, &props);
    }

    // Get device properties
    vkGetPhysicalDeviceProperties(m_impl->state.physicalDevice, &m_impl->state.deviceProperties);
    vkGetPhysicalDeviceFeatures(m_impl->state.physicalDevice, &m_impl->state.deviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(m_impl->state.physicalDevice, &m_impl->state.memoryProperties);

    // OPTIMIZATION: Cache frequently-used device limits to avoid per-draw queries
    m_impl->state.minUniformBufferOffsetAlignment = m_impl->state.deviceProperties.limits.minUniformBufferOffsetAlignment;

    // Find queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_impl->state.physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_impl->state.physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_impl->state.graphicsQueueFamily = i;
            // For now, assume present queue is same as graphics
            m_impl->state.presentQueueFamily = i;
            break;
        }
    }

    if (m_impl->state.graphicsQueueFamily == UINT32_MAX) {
        
        return false;
    }

    // Create logical device
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        m_impl->state.graphicsQueueFamily,
        m_impl->state.presentQueueFamily
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Device features we want
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceFeatures.wideLines = VK_TRUE;
    deviceFeatures.geometryShader = VK_TRUE;

    // Device extensions
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
    };

    // Check which extensions are supported
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(m_impl->state.physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(m_impl->state.physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    bool dynamicRenderingSupported = false;
    bool descriptorIndexingSupported = false;
    for (const auto& ext : availableExtensions) {
        if (strcmp(ext.extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0) {
            dynamicRenderingSupported = true;
        }
        if (strcmp(ext.extensionName, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0) {
            descriptorIndexingSupported = true;
        }
    }

    if (!dynamicRenderingSupported) {
        
        return false;
    }

    // Add descriptor indexing extension if supported (required for bindless textures)
    if (descriptorIndexingSupported) {
        deviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        m_impl->state.descriptorIndexingSupported = true;
        
    } else {
        
        m_impl->state.descriptorIndexingSupported = false;
    }

    // Enable dynamic rendering feature via pNext chain
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

    // Enable descriptor indexing features for bindless textures and UPDATE_AFTER_BIND
    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures{};
    descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    if (descriptorIndexingSupported) {
        descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        descriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
        descriptorIndexingFeatures.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
        descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
        descriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;
        // Required for UPDATE_AFTER_BIND with different descriptor types
        descriptorIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        descriptorIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
        descriptorIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
        // Chain descriptor indexing to dynamic rendering
        dynamicRenderingFeatures.pNext = &descriptorIndexingFeatures;
    }

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.features = deviceFeatures;
    deviceFeatures2.pNext = &dynamicRenderingFeatures;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = nullptr;  // Use pNext for features instead
    deviceCreateInfo.pNext = &deviceFeatures2;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Deprecated but needed for older implementations
    if (!validationLayers.empty()) {
        deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
    }

    result = vkCreateDevice(m_impl->state.physicalDevice, &deviceCreateInfo, nullptr, &m_impl->state.device);
    if (result != VK_SUCCESS) {
        
        return false;
    }

    // Get queue handles
    vkGetDeviceQueue(m_impl->state.device, m_impl->state.graphicsQueueFamily, 0, &m_impl->state.graphicsQueue);
    vkGetDeviceQueue(m_impl->state.device, m_impl->state.presentQueueFamily, 0, &m_impl->state.presentQueue);

    // Create command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_impl->state.graphicsQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    result = vkCreateCommandPool(m_impl->state.device, &poolInfo, nullptr, &m_impl->state.commandPool);
    if (result != VK_SUCCESS) {

        return false;
    }

    // OPTIMIZATION: Create pipeline cache for faster pipeline compilation
    // The cache stores compiled pipeline state, speeding up subsequent pipeline creations
    VkPipelineCacheCreateInfo pipelineCacheInfo{};
    pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipelineCacheInfo.initialDataSize = 0;  // No pre-loaded cache data
    pipelineCacheInfo.pInitialData = nullptr;

    result = vkCreatePipelineCache(m_impl->state.device, &pipelineCacheInfo, nullptr, &m_impl->state.pipelineCache);
    if (result != VK_SUCCESS) {
        // Non-fatal - pipelines will still work without cache, just slower
        m_impl->state.pipelineCache = VK_NULL_HANDLE;
    }

    // Allocate command buffers (one per frame in flight)
    m_impl->commandBuffers.resize(VulkanState::MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_impl->state.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_impl->commandBuffers.size());

    result = vkAllocateCommandBuffers(m_impl->state.device, &allocInfo, m_impl->commandBuffers.data());
    if (result != VK_SUCCESS) {
        
        return false;
    }

    // Create dummy resources for descriptor set initialization
    m_impl->state.createDummyResources();

    // Create bindless texture resources (requires dummy resources to exist first)
    m_impl->state.createBindlessResources();

    // Create default resources
    createDefaultResources();

    // Populate capabilities
    m_caps.backend = GraphicsBackend::Vulkan;
    m_caps.deviceName = m_impl->state.deviceProperties.deviceName;
    m_caps.apiVersion = std::to_string(VK_API_VERSION_MAJOR(m_impl->state.deviceProperties.apiVersion)) + "." +
                        std::to_string(VK_API_VERSION_MINOR(m_impl->state.deviceProperties.apiVersion)) + "." +
                        std::to_string(VK_API_VERSION_PATCH(m_impl->state.deviceProperties.apiVersion));
    m_caps.maxTextureSize = m_impl->state.deviceProperties.limits.maxImageDimension2D;
    m_caps.maxTextureLayers = m_impl->state.deviceProperties.limits.maxImageArrayLayers;
    m_caps.maxVertexAttributes = m_impl->state.deviceProperties.limits.maxVertexInputAttributes;
    m_caps.maxUniformBufferSize = m_impl->state.deviceProperties.limits.maxUniformBufferRange;
    m_caps.supportsCompute = true;
    m_caps.supportsGeometryShader = m_impl->state.deviceFeatures.geometryShader;
    m_caps.supportsTessellation = m_impl->state.deviceFeatures.tessellationShader;
    m_caps.supportsBindless = m_impl->state.bindlessSupported;
    m_caps.supportsRayTracing = false;  // Would require VK_KHR_ray_tracing_pipeline

    m_impl->initialized = true;
    return true;
}

void GfxDeviceVulkan::shutdown() {
    if (!m_impl->initialized) return;

    // Wait for device to be idle
    if (m_impl->state.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_impl->state.device);
    }

    // Flush all deferred destructions now that device is idle
    m_impl->state.flushDeferredDestructions();

    // Destroy bindless resources before dummy resources (bindless uses dummy as fallback)
    m_impl->state.destroyBindlessResources();

    // Destroy dummy resources
    m_impl->state.destroyDummyResources();

    // Destroy default resources
    if (m_impl->defaultSampler.isValid()) {
        destroySampler(m_impl->defaultSampler);
    }
    if (m_impl->whiteTexture.isValid()) {
        destroyTexture(m_impl->whiteTexture);
    }
    if (m_impl->blackTexture.isValid()) {
        destroyTexture(m_impl->blackTexture);
    }
    if (m_impl->normalTexture.isValid()) {
        destroyTexture(m_impl->normalTexture);
    }

    // Destroy all fonts
    for (auto& [id, font] : m_impl->fonts) {
        destroyTexture(font.texture);
    }
    m_impl->fonts.clear();

    // Destroy all meshes
    for (auto& [id, mesh] : m_impl->meshes) {
        if (mesh.vertexBuffer.isValid()) {
            destroyBuffer(mesh.vertexBuffer);
        }
        if (mesh.indexBuffer.isValid()) {
            destroyBuffer(mesh.indexBuffer);
        }
    }
    m_impl->meshes.clear();

    // Destroy all swapchains
    for (auto& [id, swapchain] : m_impl->state.swapchains) {
        destroySwapchainResources(swapchain);
    }
    m_impl->state.swapchains.clear();

    // Destroy all pipelines
    for (auto& [id, pipeline] : m_impl->state.pipelines) {
        // Destroy all pipeline variants first
        for (auto& variantPair : pipeline.variants) {
            if (variantPair.second.pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_impl->state.device, variantPair.second.pipeline, nullptr);
                // If this variant is also the default/fallback pipeline, mark it as destroyed
                if (variantPair.second.pipeline == pipeline.pipeline) {
                    pipeline.pipeline = VK_NULL_HANDLE;
                }
            }
        }
        pipeline.variants.clear();

        if (pipeline.descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_impl->state.device, pipeline.descriptorPool, nullptr);
        }
        if (pipeline.descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_impl->state.device, pipeline.descriptorSetLayout, nullptr);
        }
        if (pipeline.layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_impl->state.device, pipeline.layout, nullptr);
        }
        // Destroy the default/fallback pipeline (only if not already destroyed above)
        if (pipeline.pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_impl->state.device, pipeline.pipeline, nullptr);
        }
    }
    m_impl->state.pipelines.clear();

    // Destroy all render targets
    for (auto& [id, rt] : m_impl->state.renderTargets) {
        if (!rt.isSwapchainBackbuffer) {
            if (rt.framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(m_impl->state.device, rt.framebuffer, nullptr);
            }
            if (rt.renderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(m_impl->state.device, rt.renderPass, nullptr);
            }
        }
    }
    m_impl->state.renderTargets.clear();

    // Destroy all textures
    for (auto& [id, texture] : m_impl->state.textures) {
        if (texture.view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_impl->state.device, texture.view, nullptr);
        }
        if (texture.cubeViews != nullptr) {
            for (uint32_t i = 0; i < 6; i++) {
                if (texture.cubeViews[i] != VK_NULL_HANDLE) {
                    vkDestroyImageView(m_impl->state.device, texture.cubeViews[i], nullptr);
                }
            }
            delete[] texture.cubeViews;
        }
        if (!texture.isSwapchainImage) {
            if (texture.image != VK_NULL_HANDLE) {
                vkDestroyImage(m_impl->state.device, texture.image, nullptr);
            }
            if (texture.memory != VK_NULL_HANDLE) {
                vkFreeMemory(m_impl->state.device, texture.memory, nullptr);
            }
        }
    }
    m_impl->state.textures.clear();

    // Destroy all samplers
    for (auto& [id, sampler] : m_impl->state.samplers) {
        if (sampler.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_impl->state.device, sampler.sampler, nullptr);
        }
    }
    m_impl->state.samplers.clear();

    // Destroy all buffers
    for (auto& [id, buffer] : m_impl->state.buffers) {
        if (buffer.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_impl->state.device, buffer.buffer, nullptr);
        }
        if (buffer.memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_impl->state.device, buffer.memory, nullptr);
        }
    }
    m_impl->state.buffers.clear();

    // Destroy all uniform buffers
    for (auto& [id, ubo] : m_impl->state.uniformBuffers) {
        if (ubo.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_impl->state.device, ubo.buffer, nullptr);
        }
        if (ubo.memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_impl->state.device, ubo.memory, nullptr);
        }
    }
    m_impl->state.uniformBuffers.clear();

    // Destroy all shaders
    for (auto& [id, shader] : m_impl->state.shaders) {
        if (shader.module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(m_impl->state.device, shader.module, nullptr);
        }
    }
    m_impl->state.shaders.clear();

    // Flush deferred destructions again - resources destroyed above may have been deferred
    m_impl->state.flushDeferredDestructions();

    // Destroy pipeline cache
    if (m_impl->state.pipelineCache != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(m_impl->state.device, m_impl->state.pipelineCache, nullptr);
        m_impl->state.pipelineCache = VK_NULL_HANDLE;
    }

    // Destroy command pool
    if (m_impl->state.commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_impl->state.device, m_impl->state.commandPool, nullptr);
        m_impl->state.commandPool = VK_NULL_HANDLE;
    }

    // Destroy device
    if (m_impl->state.device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_impl->state.device, nullptr);
        m_impl->state.device = VK_NULL_HANDLE;
    }

    // Destroy debug messenger
    if (m_impl->state.debugMessenger != VK_NULL_HANDLE) {
        DestroyDebugUtilsMessengerEXT(m_impl->state.instance, m_impl->state.debugMessenger, nullptr);
        m_impl->state.debugMessenger = VK_NULL_HANDLE;
    }

    // Destroy instance
    if (m_impl->state.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_impl->state.instance, nullptr);
        m_impl->state.instance = VK_NULL_HANDLE;
    }

    // Shutdown GLSL compiler
    GLSLCompiler::shutdown();

    m_impl->initialized = false;
}

void GfxDeviceVulkan::createDefaultResources() {
    // Create default sampler
    SamplerDesc samplerDesc{};
    samplerDesc.minFilter = FilterMode::Linear;
    samplerDesc.magFilter = FilterMode::Linear;
    samplerDesc.mipFilter = FilterMode::Linear;
    samplerDesc.wrapU = WrapMode::Repeat;
    samplerDesc.wrapV = WrapMode::Repeat;
    samplerDesc.wrapW = WrapMode::Repeat;
    samplerDesc.maxAnisotropy = 16.0f;
    m_impl->defaultSampler = createSampler(samplerDesc);

    // Create 1x1 white texture
    TextureDesc whiteDesc{};
    whiteDesc.width = 1;
    whiteDesc.height = 1;
    whiteDesc.format = TextureFormat::RGBA8_UNORM;
    whiteDesc.usage = TextureUsage::Sampled;
    m_impl->whiteTexture = createTexture(whiteDesc);
    uint8_t whiteData[] = {255, 255, 255, 255};
    updateTexture(m_impl->whiteTexture, whiteData, 0, 0);  // mipLevel=0, arrayLayer=0

    // Create 1x1 black texture
    TextureDesc blackDesc{};
    blackDesc.width = 1;
    blackDesc.height = 1;
    blackDesc.format = TextureFormat::RGBA8_UNORM;
    blackDesc.usage = TextureUsage::Sampled;
    m_impl->blackTexture = createTexture(blackDesc);
    uint8_t blackData[] = {0, 0, 0, 255};
    updateTexture(m_impl->blackTexture, blackData, 0, 0);  // mipLevel=0, arrayLayer=0

    // Create 1x1 normal texture (flat normal pointing up)
    TextureDesc normalDesc{};
    normalDesc.width = 1;
    normalDesc.height = 1;
    normalDesc.format = TextureFormat::RGBA8_UNORM;
    normalDesc.usage = TextureUsage::Sampled;
    m_impl->normalTexture = createTexture(normalDesc);
    uint8_t normalData[] = {128, 128, 255, 255};  // (0.5, 0.5, 1.0) in tangent space
    updateTexture(m_impl->normalTexture, normalData, 0, 0);  // mipLevel=0, arrayLayer=0
}

void GfxDeviceVulkan::destroySwapchainResources(VulkanSwapchain& swapchain) {
    // Destroy framebuffers
    for (auto fb : swapchain.framebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_impl->state.device, fb, nullptr);
        }
    }
    swapchain.framebuffers.clear();

    // Destroy image views
    for (auto view : swapchain.imageViews) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_impl->state.device, view, nullptr);
        }
    }
    swapchain.imageViews.clear();

    // Destroy depth buffer
    if (swapchain.depthView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_impl->state.device, swapchain.depthView, nullptr);
        swapchain.depthView = VK_NULL_HANDLE;
    }
    if (swapchain.depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_impl->state.device, swapchain.depthImage, nullptr);
        swapchain.depthImage = VK_NULL_HANDLE;
    }
    if (swapchain.depthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_impl->state.device, swapchain.depthMemory, nullptr);
        swapchain.depthMemory = VK_NULL_HANDLE;
    }

    // Destroy render pass
    if (swapchain.renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_impl->state.device, swapchain.renderPass, nullptr);
        swapchain.renderPass = VK_NULL_HANDLE;
    }

    // Destroy synchronization objects
    for (auto semaphore : swapchain.imageAvailableSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_impl->state.device, semaphore, nullptr);
        }
    }
    swapchain.imageAvailableSemaphores.clear();

    for (auto semaphore : swapchain.renderFinishedSemaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_impl->state.device, semaphore, nullptr);
        }
    }
    swapchain.renderFinishedSemaphores.clear();

    for (auto fence : swapchain.inFlightFences) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(m_impl->state.device, fence, nullptr);
        }
    }
    swapchain.inFlightFences.clear();

    // Free per-swapchain command buffers
    if (!swapchain.commandBuffers.empty() && m_impl->state.commandPool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(m_impl->state.device, m_impl->state.commandPool,
                             static_cast<uint32_t>(swapchain.commandBuffers.size()),
                             swapchain.commandBuffers.data());
        swapchain.commandBuffers.clear();
    }

    // Destroy swapchain
    if (swapchain.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_impl->state.device, swapchain.swapchain, nullptr);
        swapchain.swapchain = VK_NULL_HANDLE;
    }

    // Destroy surface
    if (swapchain.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_impl->state.instance, swapchain.surface, nullptr);
        swapchain.surface = VK_NULL_HANDLE;
    }
}

void GfxDeviceVulkan::initializeDescriptorSets(VulkanPipeline& pipeline) {
    // Initialize all descriptor sets with dummy resources to ensure all bindings are valid
    // This prevents validation errors when shaders use bindings that weren't explicitly bound

    // Descriptor set layout bindings:
    // 0-3: Uniform buffers
    // 4-7: Single textures (albedo, metallic/roughness, normal, emissive)
    // 8: Shadow map array[8]
    // 9: Shadow cube map array[8]
    // 10-15: Additional textures

    VkDescriptorBufferInfo dummyBufferInfo{};
    dummyBufferInfo.buffer = m_impl->state.dummyUniformBuffer;
    dummyBufferInfo.offset = 0;
    dummyBufferInfo.range = 256;

    VkDescriptorImageInfo dummyImageInfo{};
    dummyImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dummyImageInfo.imageView = m_impl->state.dummyTextureView;
    dummyImageInfo.sampler = m_impl->state.dummySampler;

    // Cube map dummy for shadow cube map array (binding 9)
    VkDescriptorImageInfo dummyCubeMapImageInfo{};
    dummyCubeMapImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dummyCubeMapImageInfo.imageView = m_impl->state.dummyCubeMapView;
    dummyCubeMapImageInfo.sampler = m_impl->state.dummySampler;

    // Create arrays for shadow map bindings (binding 8 and 9 are arrays of 8)
    std::array<VkDescriptorImageInfo, 8> shadowMapImageInfos;
    std::array<VkDescriptorImageInfo, 8> shadowCubeMapImageInfos;
    for (int i = 0; i < 8; i++) {
        shadowMapImageInfos[i] = dummyImageInfo;
        shadowCubeMapImageInfos[i] = dummyCubeMapImageInfo;  // Use cube map dummy
    }

    for (size_t frameIdx = 0; frameIdx < pipeline.descriptorSets.size(); frameIdx++) {
        VkDescriptorSet descriptorSet = pipeline.descriptorSets[frameIdx];

        std::vector<VkWriteDescriptorSet> descriptorWrites;

        // Uniform buffers (bindings 0-3)
        for (uint32_t binding = 0; binding < 4; binding++) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptorSet;
            write.dstBinding = binding;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &dummyBufferInfo;
            descriptorWrites.push_back(write);
        }

        // Single textures (bindings 4-7)
        for (uint32_t binding = 4; binding < 8; binding++) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptorSet;
            write.dstBinding = binding;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &dummyImageInfo;
            descriptorWrites.push_back(write);
        }

        // Shadow maps array (binding 8)
        {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptorSet;
            write.dstBinding = 8;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 8;
            write.pImageInfo = shadowMapImageInfos.data();
            descriptorWrites.push_back(write);
        }

        // Shadow cube maps array (binding 9)
        {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptorSet;
            write.dstBinding = 9;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 8;
            write.pImageInfo = shadowCubeMapImageInfos.data();
            descriptorWrites.push_back(write);
        }

        // Additional textures (bindings 10-15)
        for (uint32_t binding = 10; binding < 16; binding++) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptorSet;
            write.dstBinding = binding;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &dummyImageInfo;
            descriptorWrites.push_back(write);
        }

        vkUpdateDescriptorSets(m_impl->state.device,
                               static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }
}

const GfxDeviceCaps& GfxDeviceVulkan::getCapabilities() const {
    return m_caps;
}

GraphicsBackend GfxDeviceVulkan::getBackend() const {
    return GraphicsBackend::Vulkan;
}

void GfxDeviceVulkan::setDefaultTextureFiltering(FilterMode minFilter, FilterMode magFilter) {
    m_defaultMinFilter = minFilter;
    m_defaultMagFilter = magFilter;
}

// ============================================================================
// SWAPCHAIN MANAGEMENT
// ============================================================================

SwapchainHandle GfxDeviceVulkan::createSwapchain(const SwapchainDesc& desc) {
    VulkanSwapchain swapchain{};
    swapchain.window = desc.window;
    swapchain.width = desc.width;
    swapchain.height = desc.height;
    swapchain.vsync = desc.vsync;

#ifdef _WIN32
    // Create Win32 surface
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.hwnd = static_cast<HWND>(desc.window.platformHandle);
    surfaceCreateInfo.hinstance = GetModuleHandle(nullptr);

    VkResult result = vkCreateWin32SurfaceKHR(m_impl->state.instance, &surfaceCreateInfo, nullptr, &swapchain.surface);
    if (result != VK_SUCCESS) {
        
        return SwapchainHandle();
    }
#else
    
    return SwapchainHandle();
#endif

    // Check surface support
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_impl->state.physicalDevice, m_impl->state.presentQueueFamily, swapchain.surface, &presentSupport);
    if (!presentSupport) {
        
        vkDestroySurfaceKHR(m_impl->state.instance, swapchain.surface, nullptr);
        return SwapchainHandle();
    }

    // Query surface capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_impl->state.physicalDevice, swapchain.surface, &capabilities);

    // Query surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_impl->state.physicalDevice, swapchain.surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_impl->state.physicalDevice, swapchain.surface, &formatCount, formats.data());

    // Query present modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_impl->state.physicalDevice, swapchain.surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_impl->state.physicalDevice, swapchain.surface, &presentModeCount, presentModes.data());

    // Choose surface format (prefer BGRA8 UNORM to match OpenGL's linear color space)
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB) {
            surfaceFormat = format;
        }
    }
    swapchain.vkColorFormat = surfaceFormat.format;
    swapchain.colorFormat = (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB) ?
                            TextureFormat::BGRA8_SRGB : TextureFormat::BGRA8_UNORM;

    // Choose present mode
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;  // Always available, vsync
    if (!desc.vsync) {
        for (const auto& mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }

    // Choose extent
    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent.width = std::clamp(desc.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(desc.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    swapchain.width = extent.width;
    swapchain.height = extent.height;

    // Choose image count
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // Create swapchain
    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = swapchain.surface;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = {m_impl->state.graphicsQueueFamily, m_impl->state.presentQueueFamily};
    if (m_impl->state.graphicsQueueFamily != m_impl->state.presentQueueFamily) {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    swapchainCreateInfo.preTransform = capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    result = vkCreateSwapchainKHR(m_impl->state.device, &swapchainCreateInfo, nullptr, &swapchain.swapchain);
    if (result != VK_SUCCESS) {
        
        vkDestroySurfaceKHR(m_impl->state.instance, swapchain.surface, nullptr);
        return SwapchainHandle();
    }

    // Get swapchain images
    vkGetSwapchainImagesKHR(m_impl->state.device, swapchain.swapchain, &imageCount, nullptr);
    swapchain.images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_impl->state.device, swapchain.swapchain, &imageCount, swapchain.images.data());

    // Create image views
    swapchain.imageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchain.images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchain.vkColorFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        result = vkCreateImageView(m_impl->state.device, &viewInfo, nullptr, &swapchain.imageViews[i]);
        if (result != VK_SUCCESS) {
            
            // Cleanup and return
            for (uint32_t j = 0; j < i; j++) {
                vkDestroyImageView(m_impl->state.device, swapchain.imageViews[j], nullptr);
            }
            vkDestroySwapchainKHR(m_impl->state.device, swapchain.swapchain, nullptr);
            vkDestroySurfaceKHR(m_impl->state.instance, swapchain.surface, nullptr);
            return SwapchainHandle();
        }
    }

    // Create depth buffer
    VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
    m_impl->state.createImage(swapchain.width, swapchain.height, 1, 1, 1,
                              depthFormat, VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0,
                              swapchain.depthImage, swapchain.depthMemory);

    // Depth-aspect-only view: the swapchain depth buffer is used only as a depth
    // render attachment (never sampled, and the engine renders no stencil), and a
    // single-aspect view keeps it consistent with sampled depth views.
    swapchain.depthView = m_impl->state.createImageView(swapchain.depthImage, depthFormat,
                                                        VK_IMAGE_ASPECT_DEPTH_BIT);

    // Transition depth image to optimal layout
    m_impl->state.transitionImageLayout(swapchain.depthImage, depthFormat,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    // Create render pass for swapchain
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain.vkColorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    result = vkCreateRenderPass(m_impl->state.device, &renderPassInfo, nullptr, &swapchain.renderPass);
    if (result != VK_SUCCESS) {
        
        // Cleanup...
        return SwapchainHandle();
    }

    // Create framebuffers
    swapchain.framebuffers.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        std::array<VkImageView, 2> fbAttachments = {
            swapchain.imageViews[i],
            swapchain.depthView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = swapchain.renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(fbAttachments.size());
        framebufferInfo.pAttachments = fbAttachments.data();
        framebufferInfo.width = swapchain.width;
        framebufferInfo.height = swapchain.height;
        framebufferInfo.layers = 1;

        result = vkCreateFramebuffer(m_impl->state.device, &framebufferInfo, nullptr, &swapchain.framebuffers[i]);
        if (result != VK_SUCCESS) {
            
            return SwapchainHandle();
        }
    }

    // Create synchronization objects
    // imageAvailableSemaphores: per frame-in-flight (indexed by currentFrame during acquire)
    // renderFinishedSemaphores: per swapchain image (indexed by currentImageIndex for presentation)
    // inFlightFences: per frame-in-flight (to limit CPU work ahead of GPU)
    uint32_t swapchainImageCount = static_cast<uint32_t>(swapchain.images.size());
    swapchain.imageAvailableSemaphores.resize(VulkanState::MAX_FRAMES_IN_FLIGHT);
    swapchain.renderFinishedSemaphores.resize(swapchainImageCount);
    swapchain.inFlightFences.resize(VulkanState::MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    // Create per-frame imageAvailable semaphores and fences
    for (uint32_t i = 0; i < VulkanState::MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_impl->state.device, &semaphoreInfo, nullptr, &swapchain.imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_impl->state.device, &fenceInfo, nullptr, &swapchain.inFlightFences[i]) != VK_SUCCESS) {
            
            return SwapchainHandle();
        }
    }

    // Create per-image renderFinished semaphores
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        if (vkCreateSemaphore(m_impl->state.device, &semaphoreInfo, nullptr, &swapchain.renderFinishedSemaphores[i]) != VK_SUCCESS) {
            
            return SwapchainHandle();
        }
    }

    // Allocate per-swapchain command buffers (one per frame in flight)
    swapchain.commandBuffers.resize(VulkanState::MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = m_impl->state.commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = VulkanState::MAX_FRAMES_IN_FLIGHT;

    result = vkAllocateCommandBuffers(m_impl->state.device, &cmdAllocInfo, swapchain.commandBuffers.data());
    if (result != VK_SUCCESS) {
        
        return SwapchainHandle();
    }

    // Initialize render pass tracking (one per frame in flight)
    swapchain.renderPassUsed.resize(VulkanState::MAX_FRAMES_IN_FLIGHT, false);

    // Initialize image acquisition tracking (one per frame in flight)
    swapchain.imageAcquired.resize(VulkanState::MAX_FRAMES_IN_FLIGHT, false);

    // Determine swapchain ID first so we can set it in the render target
    uint32_t swapchainId = m_impl->state.nextSwapchainID++;

    // Create render target for backbuffer
    VulkanRenderTarget backbufferRT{};
    backbufferRT.width = swapchain.width;
    backbufferRT.height = swapchain.height;
    backbufferRT.colorFormat = swapchain.colorFormat;
    backbufferRT.depthFormat = TextureFormat::DEPTH24_STENCIL8;
    backbufferRT.hasColor = true;
    backbufferRT.hasDepth = true;
    backbufferRT.hasStencil = true;  // D24S8 has stencil
    backbufferRT.isSwapchainBackbuffer = true;
    backbufferRT.owningSwapchainId = swapchainId;
    backbufferRT.renderPass = swapchain.renderPass;
    // Framebuffer, colorImage will be set per-frame (dynamic swapchain images)
    backbufferRT.depthImage = swapchain.depthImage;
    backbufferRT.depthView = swapchain.depthView;
    backbufferRT.vkColorFormat = swapchain.vkColorFormat;
    backbufferRT.vkDepthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

    uint32_t rtId = m_impl->state.nextRenderTargetID++;
    m_impl->state.renderTargets[rtId] = backbufferRT;
    swapchain.backbuffer = RenderTargetHandle(rtId);

    // Store swapchain
    m_impl->state.swapchains[swapchainId] = swapchain;

    return SwapchainHandle(swapchainId);
}

void GfxDeviceVulkan::destroySwapchain(SwapchainHandle handle) {
    if (!handle.isValid()) return;

    auto it = m_impl->state.swapchains.find(handle.id);
    if (it != m_impl->state.swapchains.end()) {
        vkDeviceWaitIdle(m_impl->state.device);

        // Remove backbuffer render target
        if (it->second.backbuffer.isValid()) {
            m_impl->state.renderTargets.erase(it->second.backbuffer.id);
        }

        // Clear frame-in-progress tracking and recovery state
        m_impl->frameInProgress.erase(handle.id);
        m_impl->swapchainNeedsRecovery.erase(handle.id);

        destroySwapchainResources(it->second);
        m_impl->state.swapchains.erase(it);
    }
}

void GfxDeviceVulkan::resizeSwapchain(SwapchainHandle handle, uint32_t width, uint32_t height) {
    auto it = m_impl->state.swapchains.find(handle.id);
    if (it == m_impl->state.swapchains.end()) return;

    VulkanSwapchain& swapchain = it->second;

    // Skip resize if dimensions are already correct
    if (swapchain.width == width && swapchain.height == height && !swapchain.isDead) {
        return;
    }

    // ====================================================================================
    // MULTI-VIEWPORT RESIZE OPTIMIZATION
    // ====================================================================================
    // OLD: vkDeviceWaitIdle() - Stalls ALL viewports during resize
    // NEW: Wait only for THIS swapchain's fences - Other viewports continue rendering
    //
    // This is critical for smooth multi-viewport editing. When resizing an inactive tab,
    // we don't want to freeze the active tab's rendering.
    // ====================================================================================
    for (auto fence : swapchain.inFlightFences) {
        if (fence != VK_NULL_HANDLE) {
            vkWaitForFences(m_impl->state.device, 1, &fence, VK_TRUE, UINT64_MAX);
        }
    }

    // Clear dead flag - we're resurrecting this swapchain
    swapchain.isDead = false;

    // Store old values we need to recreate
    NativeWindowHandle window = swapchain.window;
    bool vsync = swapchain.vsync;
    VkSurfaceKHR surface = swapchain.surface;
    swapchain.surface = VK_NULL_HANDLE;  // Don't destroy surface

    // Destroy old swapchain resources (except surface)
    for (auto fb : swapchain.framebuffers) vkDestroyFramebuffer(m_impl->state.device, fb, nullptr);
    for (auto view : swapchain.imageViews) vkDestroyImageView(m_impl->state.device, view, nullptr);
    if (swapchain.depthView != VK_NULL_HANDLE) vkDestroyImageView(m_impl->state.device, swapchain.depthView, nullptr);
    if (swapchain.depthImage != VK_NULL_HANDLE) vkDestroyImage(m_impl->state.device, swapchain.depthImage, nullptr);
    if (swapchain.depthMemory != VK_NULL_HANDLE) vkFreeMemory(m_impl->state.device, swapchain.depthMemory, nullptr);
    if (swapchain.renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(m_impl->state.device, swapchain.renderPass, nullptr);

    VkSwapchainKHR oldSwapchain = swapchain.swapchain;

    // Query new capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_impl->state.physicalDevice, surface, &capabilities);

    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    swapchain.width = extent.width;
    swapchain.height = extent.height;
    swapchain.surface = surface;

    // Recreate swapchain
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = swapchain.vkColorFormat;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_MAILBOX_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    vkCreateSwapchainKHR(m_impl->state.device, &createInfo, nullptr, &swapchain.swapchain);
    vkDestroySwapchainKHR(m_impl->state.device, oldSwapchain, nullptr);

    // Get new images
    vkGetSwapchainImagesKHR(m_impl->state.device, swapchain.swapchain, &imageCount, nullptr);
    swapchain.images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_impl->state.device, swapchain.swapchain, &imageCount, swapchain.images.data());

    // Create new image views
    swapchain.imageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        swapchain.imageViews[i] = m_impl->state.createImageView(swapchain.images[i], swapchain.vkColorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    // Create new depth buffer
    VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
    m_impl->state.createImage(swapchain.width, swapchain.height, 1, 1, 1,
                              depthFormat, VK_IMAGE_TILING_OPTIMAL,
                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0,
                              swapchain.depthImage, swapchain.depthMemory);
    // Depth-aspect-only view: the swapchain depth buffer is used only as a depth
    // render attachment (never sampled, and the engine renders no stencil), and a
    // single-aspect view keeps it consistent with sampled depth views.
    swapchain.depthView = m_impl->state.createImageView(swapchain.depthImage, depthFormat,
                                                        VK_IMAGE_ASPECT_DEPTH_BIT);
    m_impl->state.transitionImageLayout(swapchain.depthImage, depthFormat,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    // Recreate render pass and framebuffers (similar to createSwapchain)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain.vkColorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;
    vkCreateRenderPass(m_impl->state.device, &rpInfo, nullptr, &swapchain.renderPass);

    swapchain.framebuffers.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        std::array<VkImageView, 2> fbAttachments = {swapchain.imageViews[i], swapchain.depthView};
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = swapchain.renderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = fbAttachments.data();
        fbInfo.width = swapchain.width;
        fbInfo.height = swapchain.height;
        fbInfo.layers = 1;
        vkCreateFramebuffer(m_impl->state.device, &fbInfo, nullptr, &swapchain.framebuffers[i]);
    }

    // Update backbuffer render target
    auto rtIt = m_impl->state.renderTargets.find(swapchain.backbuffer.id);
    if (rtIt != m_impl->state.renderTargets.end()) {
        rtIt->second.width = swapchain.width;
        rtIt->second.height = swapchain.height;
        rtIt->second.renderPass = swapchain.renderPass;
        rtIt->second.depthImage = swapchain.depthImage;
        rtIt->second.depthView = swapchain.depthView;
    }

}

RenderTargetHandle GfxDeviceVulkan::getSwapchainBackbuffer(SwapchainHandle handle) {
    auto it = m_impl->state.swapchains.find(handle.id);
    if (it != m_impl->state.swapchains.end()) {
        return it->second.backbuffer;
    }
    return RenderTargetHandle();
}

void GfxDeviceVulkan::makeContextCurrent(SwapchainHandle /*swapchain*/) {
    // No-op for Vulkan - Vulkan doesn't have the concept of "current context" like OpenGL.
    // Command buffers are recorded and submitted explicitly.
    // This method exists for API compatibility with OpenGL-style backends.
}

void GfxDeviceVulkan::setSwapchainHintForOffscreen(SwapchainHandle swapchain) {
    // Store the hint for use in beginFrame when rendering to off-screen targets
    // This ensures each view's shadow maps use that view's swapchain for sync
    m_impl->offscreenSwapchainHint = swapchain;
}

void GfxDeviceVulkan::getSwapchainSize(SwapchainHandle handle, uint32_t& width, uint32_t& height) {
    auto it = m_impl->state.swapchains.find(handle.id);
    if (it != m_impl->state.swapchains.end()) {
        width = it->second.width;
        height = it->second.height;
    } else {
        width = 0;
        height = 0;
    }
}

// ============================================================================
// BUFFER MANAGEMENT
// ============================================================================

BufferHandle GfxDeviceVulkan::createBuffer(const BufferDesc& desc) {
    VulkanBuffer buffer{};
    buffer.size = desc.size;
    buffer.usage = desc.usage;

    VkBufferUsageFlags vkUsage = 0;
    if ((desc.usage & BufferUsage::Vertex) != BufferUsage::None) {
        vkUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if ((desc.usage & BufferUsage::Index) != BufferUsage::None) {
        vkUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if ((desc.usage & BufferUsage::Uniform) != BufferUsage::None) {
        vkUsage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if ((desc.usage & BufferUsage::Storage) != BufferUsage::None) {
        vkUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    vkUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    // Dynamic buffers (frequently updated, e.g. per-instance data) are host-visible
    // and persistently mapped so updateBuffer is a plain memcpy. Static buffers are
    // device-local and uploaded once via a staging buffer. Without this, every
    // updateBuffer on a device-local buffer allocates a staging buffer and blocks the
    // queue (vkQueueWaitIdle in endSingleTimeCommands) - a full GPU stall per update.
    const VkMemoryPropertyFlags memProps = desc.dynamic
        ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    m_impl->state.createBuffer(desc.size, vkUsage, memProps, buffer.buffer, buffer.memory);

    if (desc.dynamic && desc.size > 0) {
        vkMapMemory(m_impl->state.device, buffer.memory, 0, desc.size, 0, &buffer.mappedData);
    }

    // Upload initial data if provided
    if (desc.initialData != nullptr && desc.size > 0) {
        if (desc.dynamic && buffer.mappedData != nullptr) {
            // Host-visible: copy straight into the persistent mapping.
            memcpy(buffer.mappedData, desc.initialData, desc.size);
        } else {
            // Device-local: stage and copy.
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingMemory;
            m_impl->state.createBuffer(desc.size,
                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       stagingBuffer, stagingMemory);

            void* data;
            vkMapMemory(m_impl->state.device, stagingMemory, 0, desc.size, 0, &data);
            memcpy(data, desc.initialData, desc.size);
            vkUnmapMemory(m_impl->state.device, stagingMemory);

            m_impl->state.copyBuffer(stagingBuffer, buffer.buffer, desc.size);

            vkDestroyBuffer(m_impl->state.device, stagingBuffer, nullptr);
            vkFreeMemory(m_impl->state.device, stagingMemory, nullptr);
        }
    }

    uint32_t id = m_impl->state.nextBufferID++;
    m_impl->state.buffers[id] = buffer;
    return BufferHandle(id);
}

void GfxDeviceVulkan::destroyBuffer(BufferHandle handle) {
    if (!handle.isValid()) return;

    auto it = m_impl->state.buffers.find(handle.id);
    if (it != m_impl->state.buffers.end()) {
        // Unmap memory before deferring (must be done before destruction)
        if (it->second.mappedData != nullptr) {
            vkUnmapMemory(m_impl->state.device, it->second.memory);
            it->second.mappedData = nullptr;
        }
        // Defer destruction to avoid destroying buffer while in-flight command buffers
        // are still using it. The buffer will be destroyed after MAX_FRAMES_IN_FLIGHT frames.
        // IMPORTANT: Do NOT remove from buffers map here! The buffer must remain in the map
        // so that draw calls can still find it. The map entry will be removed when the
        // deferred destruction actually happens in processDeferredDestructions().
        m_impl->state.deferBufferDestroy(it->second, handle.id);
    }
}

void GfxDeviceVulkan::updateBuffer(BufferHandle handle, const void* data, uint64_t size, uint64_t offset) {
    auto it = m_impl->state.buffers.find(handle.id);
    if (it == m_impl->state.buffers.end()) return;

    VulkanBuffer& buffer = it->second;

    if (buffer.mappedData != nullptr) {
        memcpy(static_cast<uint8_t*>(buffer.mappedData) + offset, data, size);
    } else {
        // Use staging buffer
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        m_impl->state.createBuffer(size,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   stagingBuffer, stagingMemory);

        void* mappedData;
        vkMapMemory(m_impl->state.device, stagingMemory, 0, size, 0, &mappedData);
        memcpy(mappedData, data, size);
        vkUnmapMemory(m_impl->state.device, stagingMemory);

        // Copy with offset
        VkCommandBuffer cmdBuffer = m_impl->state.beginSingleTimeCommands();
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = offset;
        copyRegion.size = size;
        vkCmdCopyBuffer(cmdBuffer, stagingBuffer, buffer.buffer, 1, &copyRegion);
        m_impl->state.endSingleTimeCommands(cmdBuffer);

        vkDestroyBuffer(m_impl->state.device, stagingBuffer, nullptr);
        vkFreeMemory(m_impl->state.device, stagingMemory, nullptr);
    }
}

void* GfxDeviceVulkan::mapBuffer(BufferHandle handle) {
    auto it = m_impl->state.buffers.find(handle.id);
    if (it != m_impl->state.buffers.end()) {
        return it->second.mappedData;
    }
    return nullptr;
}

void GfxDeviceVulkan::unmapBuffer(BufferHandle) {
    // Buffers stay mapped in Vulkan for simplicity (host coherent)
}

// ============================================================================
// TEXTURE MANAGEMENT
// ============================================================================

TextureHandle GfxDeviceVulkan::createTexture(const TextureDesc& desc) {

    VulkanTexture texture{};
    texture.width = desc.width;
    texture.height = desc.height;
    texture.depth = desc.depth;
    texture.mipLevels = desc.mipLevels;
    texture.arrayLayers = desc.arrayLayers;
    texture.format = desc.format;
    texture.type = desc.type;

    VkFormat vkFormat = VkUtils::toVkFormat(desc.format);
    VkImageViewType viewType = VkUtils::toVkImageViewType(desc.type);

    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if ((desc.usage & TextureUsage::RenderTarget) != TextureUsage::None) {
        if (VkUtils::isDepthFormat(desc.format)) {
            usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        } else {
            usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
    }
    if ((desc.usage & TextureUsage::Storage) != TextureUsage::None) {
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (desc.mipLevels > 1) {
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    VkImageCreateFlags flags = 0;
    uint32_t layers = desc.arrayLayers;
    if (desc.type == TextureType::TextureCube) {
        flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        layers = 6;
    } else if (desc.type == TextureType::TextureCubeArray) {
        flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        layers = desc.arrayLayers * 6;
    }

    m_impl->state.createImage(desc.width, desc.height, desc.depth,
                              desc.mipLevels, layers,
                              vkFormat, VK_IMAGE_TILING_OPTIMAL, usage,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, flags,
                              texture.image, texture.memory);

    // Create image view.
    // A combined-image-sampler view of a depth/stencil format must select a SINGLE
    // aspect (VUID-VkDescriptorImageInfo-imageView-01976). Depth textures here are
    // sampled for their depth component (shadow maps, depth sampling), so use the
    // depth aspect only. This view is also valid as a depth render attachment, so a
    // single depth-only view serves both uses (the stencil component is never read).
    VkImageAspectFlags aspect = VkUtils::getImageAspect(vkFormat);
    if (aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    texture.view = m_impl->state.createImageView(texture.image, vkFormat, aspect,
                                                  viewType, 0, desc.mipLevels, 0, layers);

    // For cubemaps, create individual face views
    if (desc.type == TextureType::TextureCube) {
        texture.cubeViews = new VkImageView[6];
        for (uint32_t i = 0; i < 6; i++) {
            texture.cubeViews[i] = m_impl->state.createImageView(
                texture.image, vkFormat, aspect,
                VK_IMAGE_VIEW_TYPE_2D, 0, desc.mipLevels, i, 1);
        }
    }

    // Upload initial data if provided
    if (desc.initialData != nullptr) {
        
        // Calculate size based on format
        uint32_t bytesPerPixel = 4;  // Default RGBA8
        switch (desc.format) {
            case TextureFormat::R8_UNORM: bytesPerPixel = 1; break;
            case TextureFormat::RG8_UNORM: bytesPerPixel = 2; break;
            case TextureFormat::RGBA8_UNORM:
            case TextureFormat::RGBA8_SRGB:
            case TextureFormat::BGRA8_UNORM:
            case TextureFormat::BGRA8_SRGB: bytesPerPixel = 4; break;
            case TextureFormat::R16_FLOAT: bytesPerPixel = 2; break;
            case TextureFormat::RG16_FLOAT: bytesPerPixel = 4; break;
            case TextureFormat::RGBA16_FLOAT: bytesPerPixel = 8; break;
            case TextureFormat::R32_FLOAT: bytesPerPixel = 4; break;
            case TextureFormat::RG32_FLOAT: bytesPerPixel = 8; break;
            case TextureFormat::RGB32_FLOAT: bytesPerPixel = 12; break;
            case TextureFormat::RGBA32_FLOAT: bytesPerPixel = 16; break;
            default: bytesPerPixel = 4; break;
        }
        uint64_t size = static_cast<uint64_t>(desc.width) * desc.height * desc.depth * bytesPerPixel;

        // Create staging buffer
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        m_impl->state.createBuffer(size,
                                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   stagingBuffer, stagingMemory);

        void* mappedData;
        vkMapMemory(m_impl->state.device, stagingMemory, 0, size, 0, &mappedData);
        memcpy(mappedData, desc.initialData, size);
        vkUnmapMemory(m_impl->state.device, stagingMemory);

        // Transition to transfer dst
        m_impl->state.transitionImageLayout(texture.image, vkFormat,
                                            VK_IMAGE_LAYOUT_UNDEFINED,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                            desc.mipLevels, layers);

        // Copy to image
        m_impl->state.copyBufferToImage(stagingBuffer, texture.image, desc.width, desc.height);

        // Transition to shader read optimal
        m_impl->state.transitionImageLayout(texture.image, vkFormat,
                                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                            desc.mipLevels, layers);

        // Cleanup staging buffer
        vkDestroyBuffer(m_impl->state.device, stagingBuffer, nullptr);
        vkFreeMemory(m_impl->state.device, stagingMemory, nullptr);
    } else {
        // No initial data - just transition to shader read optimal
        m_impl->state.transitionImageLayout(texture.image, vkFormat,
                                            VK_IMAGE_LAYOUT_UNDEFINED,
                                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                            desc.mipLevels, layers);
    }
    texture.currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    uint32_t id = m_impl->state.nextTextureID++;
    m_impl->state.textures[id] = texture;

    // Register with bindless system (non-cubemap 2D textures only for now)
    if (m_impl->state.bindlessSupported && texture.view != VK_NULL_HANDLE &&
        desc.type == TextureType::Texture2D) {
        m_impl->state.allocateBindlessTextureIndex(id, texture.view);
    }

    return TextureHandle(id);
}

void GfxDeviceVulkan::destroyTexture(TextureHandle handle) {
    if (!handle.isValid()) return;

    auto it = m_impl->state.textures.find(handle.id);
    if (it != m_impl->state.textures.end()) {
        // Free bindless index first (before deferred destruction)
        if (m_impl->state.bindlessSupported) {
            m_impl->state.freeBindlessTextureIndex(handle.id);
        }

        // Defer destruction to avoid destroying texture while in-flight command buffers
        // are still using it. The texture will be destroyed after MAX_FRAMES_IN_FLIGHT frames.
        m_impl->state.deferTextureDestroy(it->second);
        m_impl->state.textures.erase(it);
    }
}

void GfxDeviceVulkan::updateTexture(TextureHandle handle, const void* data, uint32_t mipLevel, uint32_t arrayLayer) {
    auto it = m_impl->state.textures.find(handle.id);
    if (it == m_impl->state.textures.end()) return;

    VulkanTexture& texture = it->second;
    VkFormat vkFormat = VkUtils::toVkFormat(texture.format);

    // Validate mipLevel
    if (mipLevel >= texture.mipLevels) {
        
        return;
    }

    uint32_t mipWidth = std::max(1u, texture.width >> mipLevel);
    uint32_t mipHeight = std::max(1u, texture.height >> mipLevel);
    uint32_t mipDepth = std::max(1u, texture.depth >> mipLevel);

    // Calculate size based on format
    uint32_t bytesPerPixel = 4;  // Default RGBA8
    switch (texture.format) {
        case TextureFormat::R8_UNORM: bytesPerPixel = 1; break;
        case TextureFormat::RG8_UNORM: bytesPerPixel = 2; break;
        case TextureFormat::RGBA8_UNORM:
        case TextureFormat::RGBA8_SRGB:
        case TextureFormat::BGRA8_UNORM:
        case TextureFormat::BGRA8_SRGB: bytesPerPixel = 4; break;
        case TextureFormat::R16_FLOAT: bytesPerPixel = 2; break;
        case TextureFormat::RG16_FLOAT: bytesPerPixel = 4; break;
        case TextureFormat::RGBA16_FLOAT: bytesPerPixel = 8; break;
        case TextureFormat::R32_FLOAT: bytesPerPixel = 4; break;
        case TextureFormat::RG32_FLOAT: bytesPerPixel = 8; break;
        case TextureFormat::RGB32_FLOAT: bytesPerPixel = 12; break;
        case TextureFormat::RGBA32_FLOAT: bytesPerPixel = 16; break;
        default: bytesPerPixel = 4; break;
    }
    uint64_t size = mipWidth * mipHeight * mipDepth * bytesPerPixel;

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    m_impl->state.createBuffer(size,
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               stagingBuffer, stagingMemory);

    void* mappedData;
    vkMapMemory(m_impl->state.device, stagingMemory, 0, size, 0, &mappedData);
    memcpy(mappedData, data, size);
    vkUnmapMemory(m_impl->state.device, stagingMemory);

    // Transition only the mip level / array layer being written. The whole level
    // is overwritten by the copy, so UNDEFINED is a valid (and correct) old layout
    // and avoids needing per-level layout tracking — important now that textures
    // upload a full mip chain one level at a time.
    m_impl->state.transitionImageLayout(texture.image, vkFormat,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        1, 1, mipLevel, arrayLayer);

    // Copy buffer to image
    VkCommandBuffer cmdBuffer = m_impl->state.beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VkUtils::getImageAspect(vkFormat);
    region.imageSubresource.mipLevel = mipLevel;
    region.imageSubresource.baseArrayLayer = arrayLayer;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {mipWidth, mipHeight, mipDepth};

    vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer, texture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    m_impl->state.endSingleTimeCommands(cmdBuffer);

    // Transition the written level to shader read.
    m_impl->state.transitionImageLayout(texture.image, vkFormat,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        1, 1, mipLevel, arrayLayer);
    texture.currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkDestroyBuffer(m_impl->state.device, stagingBuffer, nullptr);
    vkFreeMemory(m_impl->state.device, stagingMemory, nullptr);
}

void GfxDeviceVulkan::updateTextureFace(TextureHandle handle, uint32_t face, const void* data, uint64_t size, uint32_t mipLevel) {
    auto it = m_impl->state.textures.find(handle.id);
    if (it == m_impl->state.textures.end()) return;

    VulkanTexture& texture = it->second;
    if (texture.type != TextureType::TextureCube && texture.type != TextureType::TextureCubeArray) return;

    // Validate mipLevel
    if (mipLevel >= texture.mipLevels) {
        
        return;
    }

    VkFormat vkFormat = VkUtils::toVkFormat(texture.format);
    uint32_t mipWidth = std::max(1u, texture.width >> mipLevel);
    uint32_t mipHeight = std::max(1u, texture.height >> mipLevel);

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    m_impl->state.createBuffer(size,
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               stagingBuffer, stagingMemory);

    void* mappedData;
    vkMapMemory(m_impl->state.device, stagingMemory, 0, size, 0, &mappedData);
    memcpy(mappedData, data, size);
    vkUnmapMemory(m_impl->state.device, stagingMemory);

    // Transition specific layer
    VkCommandBuffer cmdBuffer = m_impl->state.beginSingleTimeCommands();

    VkImageAspectFlags aspectMask = VkUtils::getImageAspect(vkFormat);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = texture.currentLayout;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texture.image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = mipLevel;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = face;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmdBuffer,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.imageSubresource.aspectMask = aspectMask;
    region.imageSubresource.mipLevel = mipLevel;
    region.imageSubresource.baseArrayLayer = face;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {mipWidth, mipHeight, 1};

    vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer, texture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition back
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmdBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_impl->state.endSingleTimeCommands(cmdBuffer);

    vkDestroyBuffer(m_impl->state.device, stagingBuffer, nullptr);
    vkFreeMemory(m_impl->state.device, stagingMemory, nullptr);
}

void GfxDeviceVulkan::generateMipmaps(TextureHandle handle) {
    auto it = m_impl->state.textures.find(handle.id);
    if (it == m_impl->state.textures.end()) return;

    VulkanTexture& texture = it->second;
    if (texture.mipLevels <= 1) return;

    VkFormat vkFormat = VkUtils::toVkFormat(texture.format);
    m_impl->state.generateMipmaps(texture.image, vkFormat, texture.width, texture.height, texture.mipLevels);
    texture.currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void GfxDeviceVulkan::getTextureSize(TextureHandle handle, uint32_t& width, uint32_t& height) {
    auto it = m_impl->state.textures.find(handle.id);
    if (it != m_impl->state.textures.end()) {
        width = it->second.width;
        height = it->second.height;
    } else {
        width = 0;
        height = 0;
    }
}

// ============================================================================
// SAMPLER MANAGEMENT
// ============================================================================

SamplerHandle GfxDeviceVulkan::createSampler(const SamplerDesc& desc) {
    VulkanSampler sampler{};
    sampler.desc = desc;

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VkUtils::toVkFilter(desc.magFilter);
    samplerInfo.minFilter = VkUtils::toVkFilter(desc.minFilter);
    samplerInfo.mipmapMode = (desc.mipFilter == FilterMode::Linear) ?
                             VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VkUtils::toVkWrapMode(desc.wrapU);
    samplerInfo.addressModeV = VkUtils::toVkWrapMode(desc.wrapV);
    samplerInfo.addressModeW = VkUtils::toVkWrapMode(desc.wrapW);
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.anisotropyEnable = (desc.maxAnisotropy > 1.0f) ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = std::min(desc.maxAnisotropy, m_impl->state.deviceProperties.limits.maxSamplerAnisotropy);
    samplerInfo.compareEnable = desc.compareEnable ? VK_TRUE : VK_FALSE;
    samplerInfo.compareOp = VkUtils::toVkCompareOp(desc.compareFunc);
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    VkResult result = vkCreateSampler(m_impl->state.device, &samplerInfo, nullptr, &sampler.sampler);
    if (result != VK_SUCCESS) {
        
        return SamplerHandle();
    }

    uint32_t id = m_impl->state.nextSamplerID++;
    m_impl->state.samplers[id] = sampler;

    // Register with bindless system
    if (m_impl->state.bindlessSupported && sampler.sampler != VK_NULL_HANDLE) {
        m_impl->state.allocateBindlessSamplerIndex(id, sampler.sampler);
    }

    return SamplerHandle(id);
}

void GfxDeviceVulkan::destroySampler(SamplerHandle handle) {
    if (!handle.isValid()) return;

    auto it = m_impl->state.samplers.find(handle.id);
    if (it != m_impl->state.samplers.end()) {
        // Free bindless index first
        if (m_impl->state.bindlessSupported) {
            m_impl->state.freeBindlessSamplerIndex(handle.id);
        }

        if (it->second.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_impl->state.device, it->second.sampler, nullptr);
        }
        m_impl->state.samplers.erase(it);
    }
}

// ============================================================================
// SHADER MANAGEMENT
// ============================================================================

ShaderHandle GfxDeviceVulkan::createShader(const ShaderDesc& desc) {
    VulkanShader shader{};
    shader.stage = desc.stage;
    shader.entryPoint = desc.entryPoint.empty() ? "main" : desc.entryPoint;

    if (desc.bytecode == nullptr || desc.bytecodeSize == 0) {
        
        return ShaderHandle();
    }

    // Check if this is GLSL source or SPIR-V bytecode
    // SPIR-V magic number is 0x07230203
    const uint32_t* data = reinterpret_cast<const uint32_t*>(desc.bytecode);
    bool isSpirv = (desc.bytecodeSize >= 4 && data[0] == 0x07230203);

    std::vector<uint32_t> spirvCode;
    const uint32_t* spirvData = nullptr;
    size_t spirvSize = 0;

    if (isSpirv) {
        // Already SPIR-V, use directly
        spirvData = data;
        spirvSize = desc.bytecodeSize;
    } else {
        // GLSL source code, compile to SPIR-V
        std::string errorLog;
        const char* source = static_cast<const char*>(desc.bytecode);

        // Log first few lines of shader for debugging
        std::string shaderPreview(source, std::min<size_t>(200, desc.bytecodeSize));

        if (!GLSLCompiler::compileGLSLToSPIRV(source, desc.stage, spirvCode, errorLog)) {

            return ShaderHandle();
        }

        spirvData = spirvCode.data();
        spirvSize = spirvCode.size() * sizeof(uint32_t);

        // Reflect the shader's uniform layout from its GLSL source so the command
        // list can route engine uniforms to the exact offsets this shader expects.
        // The source is NUL-terminated GLSL (the engine stores shaders as C strings).
        GlslReflection reflection;
        GLSLCompiler::reflectUniformLayout(source, reflection);
        for (const auto& m : reflection.pushConstants) {
            shader.pushConstantMembers[m.name] = VulkanUniformMember{m.offset, m.size};
        }
        for (const auto& m : reflection.materialUBO) {
            shader.materialUBOMembers[m.name] = VulkanUniformMember{m.offset, m.size};
        }
        shader.materialUBOSize = reflection.materialUBOSize;
    }

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvSize;
    createInfo.pCode = spirvData;

    VkResult result = vkCreateShaderModule(m_impl->state.device, &createInfo, nullptr, &shader.module);
    if (result != VK_SUCCESS) {
        
        return ShaderHandle();
    }

    // Store the SPIR-V code if we compiled it (to keep it alive)
    if (!isSpirv) {
        shader.spirvCode = std::move(spirvCode);
    }

    uint32_t id = m_impl->state.nextShaderID++;
    m_impl->state.shaders[id] = shader;
    return ShaderHandle(id);
}

void GfxDeviceVulkan::destroyShader(ShaderHandle handle) {
    if (!handle.isValid()) return;

    auto it = m_impl->state.shaders.find(handle.id);
    if (it != m_impl->state.shaders.end()) {
        // Defer destruction to avoid destroying shader while in-flight command buffers
        // that use pipelines containing this shader are still executing.
        m_impl->state.deferShaderDestroy(it->second);
        m_impl->state.shaders.erase(it);
    }
}

// ============================================================================
// PIPELINE MANAGEMENT
// ============================================================================

PipelineHandle GfxDeviceVulkan::createPipeline(const PipelineDesc& desc) {
    VulkanPipeline pipeline{};
    pipeline.shaders = desc.shaders;
    pipeline.vertexLayout = desc.vertexLayout;
    pipeline.extraVertexBuffers = desc.extraVertexBuffers;
    pipeline.topology = desc.topology;
    pipeline.blendState = desc.blendState;
    pipeline.depthStencilState = desc.depthStencilState;
    pipeline.rasterizerState = desc.rasterizerState;

    // Create descriptor set layout
    // Layout matches Vulkan shader conventions:
    // Binding 0: UBO for transform matrices (vertex + fragment)
    // Binding 1: UBO for bone data (vertex - for skeletal shaders)
    // Binding 2: UBO for material parameters (vertex + fragment)
    // Binding 3: UBO for light data (vertex + fragment)
    // Bindings 4-15: Combined image samplers for textures
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    // Binding 0: UBO for transforms (u_ViewProjection, u_Model, etc.)
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(binding);
    }

    // Binding 1: UBO for bone data (skeletal animation)
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 1;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        bindings.push_back(binding);
    }

    // Binding 2: UBO for material parameters
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 2;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(binding);
    }

    // Binding 3: UBO for light data
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 3;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(binding);
    }

    // Bindings 4-7: Regular textures (albedo, metallic/roughness, normal, emissive, etc.)
    for (uint32_t i = 4; i < 8; i++) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = i;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(binding);
    }

    // Binding 8: Shadow maps array (sampler2D u_ShadowMaps[8])
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 8;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 8;  // Array of 8 shadow maps
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(binding);
    }

    // Binding 9: Shadow cube maps array (samplerCube u_ShadowCubeMaps[8])
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 9;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 8;  // Array of 8 shadow cube maps
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(binding);
    }

    // Bindings 10-15: Additional textures (AO, shadow ramp, etc.)
    for (uint32_t i = 10; i < 16; i++) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = i;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(binding);
    }

    // Bindings 24-25: Skybox textures (cubemap and panoramic)
    for (uint32_t i = 24; i < 26; i++) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = i;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(binding);
    }

    // Create binding flags for UPDATE_AFTER_BIND (if supported) - allows updating descriptor sets while bound
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    std::vector<VkDescriptorBindingFlags> bindingFlags;
    bool useUpdateAfterBind = m_impl->state.descriptorIndexingSupported;

    if (useUpdateAfterBind) {
        bindingFlags.resize(bindings.size(),
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
        bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
        bindingFlagsInfo.pBindingFlags = bindingFlags.data();
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    if (useUpdateAfterBind) {
        layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layoutInfo.pNext = &bindingFlagsInfo;
    }

    VkResult result = vkCreateDescriptorSetLayout(m_impl->state.device, &layoutInfo, nullptr, &pipeline.descriptorSetLayout);
    if (result != VK_SUCCESS) {
        
        return PipelineHandle();
    }

    // Create descriptor pool
    // bindings 4-7 (4 textures) + binding 8 (8 shadow maps) + binding 9 (8 shadow cube maps) + bindings 10-15 (6 textures) + bindings 24-25 (2 skybox) = 28 samplers
    // Allocate DESCRIPTOR_SETS_PER_FRAME sets per frame to allow multiple draws with different textures
    const uint32_t totalDescriptorSets = VulkanPipeline::DESCRIPTOR_SETS_PER_FRAME * VulkanPipeline::MAX_FRAMES;
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 28 * totalDescriptorSets},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 * totalDescriptorSets}  // bindings 0-3
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = totalDescriptorSets;
    if (useUpdateAfterBind) {
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    }

    result = vkCreateDescriptorPool(m_impl->state.device, &poolInfo, nullptr, &pipeline.descriptorPool);
    if (result != VK_SUCCESS) {
        
        vkDestroyDescriptorSetLayout(m_impl->state.device, pipeline.descriptorSetLayout, nullptr);
        return PipelineHandle();
    }

    // Allocate descriptor sets - one pool per frame with DESCRIPTOR_SETS_PER_FRAME sets each
    std::vector<VkDescriptorSetLayout> layouts(totalDescriptorSets, pipeline.descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pipeline.descriptorPool;
    allocInfo.descriptorSetCount = totalDescriptorSets;
    allocInfo.pSetLayouts = layouts.data();

    pipeline.descriptorSets.resize(totalDescriptorSets);
    result = vkAllocateDescriptorSets(m_impl->state.device, &allocInfo, pipeline.descriptorSets.data());
    if (result != VK_SUCCESS) {
        
        vkDestroyDescriptorPool(m_impl->state.device, pipeline.descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_impl->state.device, pipeline.descriptorSetLayout, nullptr);
        return PipelineHandle();
    }

    // Initialize all descriptor sets with dummy resources
    // This ensures all bindings have valid values even if not explicitly bound
    initializeDescriptorSets(pipeline);

    // Create push constant range (for uniforms)
    // 256 bytes allows for 4 mat4s (model, view, projection, extra) plus some vec4s
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 256;  // Max typical push constant size (some GPUs limit to 128, most support 256)

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &pipeline.descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    result = vkCreatePipelineLayout(m_impl->state.device, &pipelineLayoutInfo, nullptr, &pipeline.layout);
    if (result != VK_SUCCESS) {
        
        vkDestroyDescriptorPool(m_impl->state.device, pipeline.descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_impl->state.device, pipeline.descriptorSetLayout, nullptr);
        return PipelineHandle();
    }

    // Build the pipeline's uniform routing tables from per-shader reflection.
    // Each shader declares its own push_constant block and its own material UBO
    // (set=0, binding=2) layout; we merge them across the pipeline's stages so a
    // uniform set by name (or carried in the canonical push-constant blob) can be
    // written to the exact byte offset the bound shaders expect.
    for (const auto& shaderHandle : desc.shaders) {
        auto shaderIt = m_impl->state.shaders.find(shaderHandle.id);
        if (shaderIt == m_impl->state.shaders.end()) continue;
        const VulkanShader& sh = shaderIt->second;
        for (const auto& [name, member] : sh.pushConstantMembers) {
            pipeline.pushConstantOffsets[name] = member.offset;
        }
        for (const auto& [name, member] : sh.materialUBOMembers) {
            pipeline.materialUBOOffsets[name] = member;
        }
        pipeline.materialUBOSize = std::max(pipeline.materialUBOSize, sh.materialUBOSize);
    }

    // Expand engine uniform aliases: several engine-side names refer to the same
    // shader member (the .lsh transpiler emits one canonical name). If a shader
    // declares one name in a group, make every alias resolve to the same slot so
    // applyPropertyOverrides() can use any of them.
    {
        static const std::vector<std::vector<std::string>> kAliasGroups = {
            {"u_TintColor", "u_Modulate"},
            {"u_AlbedoColor", "u_Color"},
            {"u_UVRect", "u_UVMinMax"},
            {"u_CornerRadius", "u_GradientParams", "u_PolygonParams"},
        };
        for (const auto& group : kAliasGroups) {
            // Push-constant aliases.
            for (const auto& name : group) {
                auto it = pipeline.pushConstantOffsets.find(name);
                if (it != pipeline.pushConstantOffsets.end()) {
                    uint32_t offset = it->second;
                    for (const auto& alias : group) {
                        pipeline.pushConstantOffsets.emplace(alias, offset);
                    }
                    break;
                }
            }
            // Material-UBO aliases.
            for (const auto& name : group) {
                auto it = pipeline.materialUBOOffsets.find(name);
                if (it != pipeline.materialUBOOffsets.end()) {
                    VulkanUniformMember member = it->second;
                    for (const auto& alias : group) {
                        pipeline.materialUBOOffsets.emplace(alias, member);
                    }
                    break;
                }
            }
        }
    }

    // Gather shader stages
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    for (const auto& shaderHandle : desc.shaders) {
        auto shaderIt = m_impl->state.shaders.find(shaderHandle.id);
        if (shaderIt == m_impl->state.shaders.end()) {
            
            continue;
        }

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VkUtils::toVkShaderStage(shaderIt->second.stage);
        stageInfo.module = shaderIt->second.module;
        stageInfo.pName = shaderIt->second.entryPoint.c_str();
        shaderStages.push_back(stageInfo);
    }

    // Vertex input
    std::vector<VkVertexInputBindingDescription> bindingDescs;
    std::vector<VkVertexInputAttributeDescription> attributeDescs;

    // Binding 0 holds the per-vertex geometry data
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = desc.vertexLayout.stride;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescs.push_back(bindingDesc);

    // Create attribute descriptions
    for (const auto& attr : desc.vertexLayout.attributes) {
        VkVertexInputAttributeDescription attrDesc{};
        attrDesc.location = attr.location;  // Use explicit location from attribute
        attrDesc.binding = attr.binding;
        attrDesc.format = VkUtils::toVkVertexFormat(attr.format);
        attrDesc.offset = attr.offset;
        attributeDescs.push_back(attrDesc);
    }

    // Additional bindings (e.g. per-instance data on binding 1+) advance at the
    // rate specified by each layout; instance layouts use VK_VERTEX_INPUT_RATE_INSTANCE.
    for (const auto& extra : desc.extraVertexBuffers) {
        VkVertexInputBindingDescription extraBinding{};
        extraBinding.binding = extra.attributes.empty() ? 0u : extra.attributes.front().binding;
        extraBinding.stride = extra.stride;
        extraBinding.inputRate = (extra.inputRate == VertexInputRate::Instance)
            ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescs.push_back(extraBinding);

        for (const auto& attr : extra.attributes) {
            VkVertexInputAttributeDescription attrDesc{};
            attrDesc.location = attr.location;
            attrDesc.binding = attr.binding;
            attrDesc.format = VkUtils::toVkVertexFormat(attr.format);
            attrDesc.offset = attr.offset;
            attributeDescs.push_back(attrDesc);
        }
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescs.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescs.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescs.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescs.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VkUtils::toVkTopology(desc.topology);
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Dynamic viewport and scissor
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = desc.rasterizerState.depthClampEnable ? VK_TRUE : VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VkUtils::toVkPolygonMode(desc.rasterizerState.fillMode);
    rasterizer.cullMode = VkUtils::toVkCullMode(desc.rasterizerState.cullMode);
    rasterizer.frontFace = VkUtils::toVkFrontFace(desc.rasterizerState.frontFace);
    rasterizer.depthBiasEnable = desc.rasterizerState.depthBiasEnable ? VK_TRUE : VK_FALSE;
    rasterizer.depthBiasConstantFactor = desc.rasterizerState.depthBiasConstantFactor;
    rasterizer.depthBiasSlopeFactor = desc.rasterizerState.depthBiasSlopeFactor;
    rasterizer.depthBiasClamp = desc.rasterizerState.depthBiasClamp;
    rasterizer.lineWidth = 1.0f;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = desc.depthStencilState.depthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = desc.depthStencilState.depthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VkUtils::toVkCompareOp(desc.depthStencilState.depthCompareFunc);
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = desc.depthStencilState.stencilEnable ? VK_TRUE : VK_FALSE;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = desc.blendState.blendEnable ? VK_TRUE : VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VkUtils::toVkBlendFactor(desc.blendState.srcColorBlend);
    colorBlendAttachment.dstColorBlendFactor = VkUtils::toVkBlendFactor(desc.blendState.dstColorBlend);
    colorBlendAttachment.colorBlendOp = VkUtils::toVkBlendOp(desc.blendState.colorBlendOp);
    colorBlendAttachment.srcAlphaBlendFactor = VkUtils::toVkBlendFactor(desc.blendState.srcAlphaBlend);
    colorBlendAttachment.dstAlphaBlendFactor = VkUtils::toVkBlendFactor(desc.blendState.dstAlphaBlend);
    colorBlendAttachment.alphaBlendOp = VkUtils::toVkBlendOp(desc.blendState.alphaBlendOp);
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic states
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Store pipeline state - variants will be created on demand
    uint32_t id = m_impl->state.nextPipelineID++;
    m_impl->state.pipelines[id] = pipeline;

    // Create the default variant with common swapchain format
    // This will be used as fallback and for swapchain rendering
    // Using UNORM (linear) to match OpenGL's color space
    VkFormat defaultColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkFormat defaultDepthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

    VkPipeline defaultVariant = createPipelineVariantInternal(
        m_impl->state.pipelines[id], defaultColorFormat, defaultDepthFormat);

    if (defaultVariant == VK_NULL_HANDLE) {
        
        vkDestroyPipelineLayout(m_impl->state.device, pipeline.layout, nullptr);
        vkDestroyDescriptorPool(m_impl->state.device, pipeline.descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_impl->state.device, pipeline.descriptorSetLayout, nullptr);
        m_impl->state.pipelines.erase(id);
        return PipelineHandle();
    }

    // Store the default variant info
    m_impl->state.pipelines[id].pipeline = defaultVariant;
    m_impl->state.pipelines[id].colorFormat = defaultColorFormat;
    m_impl->state.pipelines[id].depthFormat = defaultDepthFormat;

    return PipelineHandle(id);
}

VkPipeline GfxDeviceVulkan::createPipelineVariantInternal(VulkanPipeline& pipeline, VkFormat colorFormat, VkFormat depthFormat) {
    // Check if variant already exists
    PipelineFormatKey key{colorFormat, depthFormat};
    auto existingIt = pipeline.variants.find(key);
    if (existingIt != pipeline.variants.end()) {
        return existingIt->second.pipeline;
    }

    // Collect shader stages
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    for (const auto& shaderHandle : pipeline.shaders) {
        auto shaderIt = m_impl->state.shaders.find(shaderHandle.id);
        if (shaderIt == m_impl->state.shaders.end()) {
            
            return VK_NULL_HANDLE;
        }

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VkUtils::toVkShaderStage(shaderIt->second.stage);
        stageInfo.module = shaderIt->second.module;
        stageInfo.pName = shaderIt->second.entryPoint.c_str();
        shaderStages.push_back(stageInfo);
    }

    // Vertex input
    std::vector<VkVertexInputBindingDescription> bindingDescs;
    std::vector<VkVertexInputAttributeDescription> attributeDescs;

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = pipeline.vertexLayout.stride;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescs.push_back(bindingDesc);

    for (const auto& attr : pipeline.vertexLayout.attributes) {
        VkVertexInputAttributeDescription attrDesc{};
        attrDesc.location = attr.location;
        attrDesc.binding = attr.binding;
        attrDesc.format = VkUtils::toVkVertexFormat(attr.format);
        attrDesc.offset = attr.offset;
        attributeDescs.push_back(attrDesc);
    }

    // Additional bindings (e.g. per-instance data on binding 1+) advance at the
    // rate specified by each layout; instance layouts use VK_VERTEX_INPUT_RATE_INSTANCE.
    for (const auto& extra : pipeline.extraVertexBuffers) {
        VkVertexInputBindingDescription extraBinding{};
        extraBinding.binding = extra.attributes.empty() ? 0u : extra.attributes.front().binding;
        extraBinding.stride = extra.stride;
        extraBinding.inputRate = (extra.inputRate == VertexInputRate::Instance)
            ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescs.push_back(extraBinding);

        for (const auto& attr : extra.attributes) {
            VkVertexInputAttributeDescription attrDesc{};
            attrDesc.location = attr.location;
            attrDesc.binding = attr.binding;
            attrDesc.format = VkUtils::toVkVertexFormat(attr.format);
            attrDesc.offset = attr.offset;
            attributeDescs.push_back(attrDesc);
        }
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescs.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescs.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescs.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescs.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VkUtils::toVkTopology(pipeline.topology);
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport state (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = pipeline.rasterizerState.depthClampEnable ? VK_TRUE : VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VkUtils::toVkPolygonMode(pipeline.rasterizerState.fillMode);
    rasterizer.cullMode = VkUtils::toVkCullMode(pipeline.rasterizerState.cullMode);
    rasterizer.frontFace = VkUtils::toVkFrontFace(pipeline.rasterizerState.frontFace);
    rasterizer.depthBiasEnable = pipeline.rasterizerState.depthBiasEnable ? VK_TRUE : VK_FALSE;
    rasterizer.depthBiasConstantFactor = pipeline.rasterizerState.depthBiasConstantFactor;
    rasterizer.depthBiasSlopeFactor = pipeline.rasterizerState.depthBiasSlopeFactor;
    rasterizer.depthBiasClamp = pipeline.rasterizerState.depthBiasClamp;
    rasterizer.lineWidth = 1.0f;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencilState{};
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = pipeline.depthStencilState.depthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencilState.depthWriteEnable = pipeline.depthStencilState.depthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencilState.depthCompareOp = VkUtils::toVkCompareOp(pipeline.depthStencilState.depthCompareFunc);
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.stencilTestEnable = pipeline.depthStencilState.stencilEnable ? VK_TRUE : VK_FALSE;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = pipeline.blendState.blendEnable ? VK_TRUE : VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VkUtils::toVkBlendFactor(pipeline.blendState.srcColorBlend);
    colorBlendAttachment.dstColorBlendFactor = VkUtils::toVkBlendFactor(pipeline.blendState.dstColorBlend);
    colorBlendAttachment.colorBlendOp = VkUtils::toVkBlendOp(pipeline.blendState.colorBlendOp);
    colorBlendAttachment.srcAlphaBlendFactor = VkUtils::toVkBlendFactor(pipeline.blendState.srcAlphaBlend);
    colorBlendAttachment.dstAlphaBlendFactor = VkUtils::toVkBlendFactor(pipeline.blendState.dstAlphaBlend);
    colorBlendAttachment.alphaBlendOp = VkUtils::toVkBlendOp(pipeline.blendState.alphaBlendOp);
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    // For depth-only pipelines, don't specify color blend attachments
    if (colorFormat != VK_FORMAT_UNDEFINED) {
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
    } else {
        colorBlending.attachmentCount = 0;
        colorBlending.pAttachments = nullptr;
    }

    // Dynamic states
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Dynamic rendering info with the requested formats
    VkFormat colorFormats[] = { colorFormat };

    // The engine renders no stencil, and depth attachment views are depth-aspect
    // only (so the same view can be sampled, e.g. shadow maps). This pipeline therefore
    // declares NO stencil attachment. This MUST match beginRenderPass(), which binds
    // only a depth attachment and no stencil attachment — otherwise the dynamic-rendering
    // pipeline/attachment formats mismatch and draws are silently dropped (or stall the GPU).
    VkFormat stencilFormat = VK_FORMAT_UNDEFINED;

    VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
    pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    // For depth-only render targets (shadow maps), colorFormat is VK_FORMAT_UNDEFINED
    // In that case, set colorAttachmentCount to 0 to match the render pass
    if (colorFormat != VK_FORMAT_UNDEFINED) {
        pipelineRenderingInfo.colorAttachmentCount = 1;
        pipelineRenderingInfo.pColorAttachmentFormats = colorFormats;
    } else {
        pipelineRenderingInfo.colorAttachmentCount = 0;
        pipelineRenderingInfo.pColorAttachmentFormats = nullptr;
    }
    pipelineRenderingInfo.depthAttachmentFormat = depthFormat;
    pipelineRenderingInfo.stencilAttachmentFormat = stencilFormat;
    pipelineRenderingInfo.viewMask = 0;

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &pipelineRenderingInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipeline.layout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;

    VkPipeline vkPipeline = VK_NULL_HANDLE;
    // OPTIMIZATION: Use pipeline cache for faster compilation of similar pipelines
    VkResult result = vkCreateGraphicsPipelines(m_impl->state.device, m_impl->state.pipelineCache, 1, &pipelineInfo, nullptr, &vkPipeline);
    if (result != VK_SUCCESS) {

        return VK_NULL_HANDLE;
    }

    // Store the variant
    VulkanPipelineVariant variant;
    variant.pipeline = vkPipeline;
    variant.colorFormat = colorFormat;
    variant.depthFormat = depthFormat;
    pipeline.variants[key] = variant;

    return vkPipeline;
}

VkPipeline GfxDeviceVulkan::createPipelineVariant(PipelineHandle handle, VkFormat colorFormat, VkFormat depthFormat) {
    auto it = m_impl->state.pipelines.find(handle.id);
    if (it == m_impl->state.pipelines.end()) {
        
        return VK_NULL_HANDLE;
    }
    return createPipelineVariantInternal(it->second, colorFormat, depthFormat);
}

void GfxDeviceVulkan::destroyPipeline(PipelineHandle handle) {
    if (!handle.isValid()) return;

    auto it = m_impl->state.pipelines.find(handle.id);
    if (it != m_impl->state.pipelines.end()) {
        // Defer destruction to avoid destroying pipeline while in-flight command buffers
        // are still using it. The pipeline will be destroyed after MAX_FRAMES_IN_FLIGHT frames.
        m_impl->state.deferPipelineDestroy(it->second);

        // Clear the pipeline pointer if it's the current one
        if (m_impl->state.currentPipeline == &it->second) {
            m_impl->state.currentPipeline = nullptr;
        }

        m_impl->state.pipelines.erase(it);
    }
}

// ============================================================================
// RENDER TARGET MANAGEMENT
// ============================================================================

RenderTargetHandle GfxDeviceVulkan::createRenderTarget(const RenderTargetDesc& desc) {
    VulkanRenderTarget renderTarget{};
    renderTarget.width = desc.width;
    renderTarget.height = desc.height;
    renderTarget.colorFormat = desc.colorFormat;
    renderTarget.depthFormat = desc.depthFormat;
    // Respect the hasColor/hasDepth flags from the descriptor
    // This is important for depth-only render targets like shadow maps
    renderTarget.hasColor = desc.hasColor && (desc.colorFormat != TextureFormat::Unknown);
    renderTarget.hasDepth = desc.hasDepth && (desc.depthFormat != TextureFormat::Unknown);

    // Create color texture if needed
    if (renderTarget.hasColor) {
        TextureDesc colorDesc{};
        colorDesc.width = desc.width;
        colorDesc.height = desc.height;
        colorDesc.format = desc.colorFormat;
        colorDesc.usage = TextureUsage::RenderTarget | TextureUsage::Sampled;

        if (desc.isCubeMap) {
            colorDesc.type = TextureType::TextureCube;
            colorDesc.arrayLayers = 6;
            renderTarget.isCubeMap = true;
        }

        renderTarget.colorTextureHandle = createTexture(colorDesc);
        auto texIt = m_impl->state.textures.find(renderTarget.colorTextureHandle.id);
        if (texIt != m_impl->state.textures.end()) {
            renderTarget.colorView = texIt->second.view;
            renderTarget.colorImage = texIt->second.image;  // For dynamic rendering
        }
        renderTarget.vkColorFormat = VkUtils::toVkFormat(desc.colorFormat);
    }

    // Create depth texture if needed
    // For shadow maps (depth-only render targets), we need Sampled usage to read the depth
    if (renderTarget.hasDepth) {
        TextureDesc depthDesc{};
        depthDesc.width = desc.width;
        depthDesc.height = desc.height;
        depthDesc.format = desc.depthFormat;
        // If this is a depth-only render target (shadow map), allow sampling
        depthDesc.usage = TextureUsage::RenderTarget;
        if (!renderTarget.hasColor) {
            depthDesc.usage = TextureUsage::RenderTarget | TextureUsage::Sampled;
        }

        if (desc.isCubeMap) {
            depthDesc.type = TextureType::TextureCube;
            depthDesc.arrayLayers = 6;
        }

        renderTarget.depthTextureHandle = createTexture(depthDesc);
        auto texIt = m_impl->state.textures.find(renderTarget.depthTextureHandle.id);
        if (texIt != m_impl->state.textures.end()) {
            renderTarget.depthView = texIt->second.view;
            renderTarget.depthImage = texIt->second.image;  // For dynamic rendering
        }
        renderTarget.vkDepthFormat = VkUtils::toVkFormat(desc.depthFormat);

        // Check if the depth format includes stencil
        renderTarget.hasStencil = (desc.depthFormat == TextureFormat::DEPTH24_STENCIL8 ||
                                   desc.depthFormat == TextureFormat::DEPTH32F_STENCIL8);
    }

    // Create render pass
    std::vector<VkAttachmentDescription> attachments;
    VkAttachmentReference colorRef{};
    VkAttachmentReference depthRef{};

    if (renderTarget.hasColor) {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = VkUtils::toVkFormat(desc.colorFormat);
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments.push_back(colorAttachment);

        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    if (renderTarget.hasDepth) {
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = VkUtils::toVkFormat(desc.depthFormat);
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // For shadow maps (depth-only), transition to read-only for sampling
        // For regular render targets, keep as depth attachment
        depthAttachment.finalLayout = renderTarget.hasColor ?
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        attachments.push_back(depthAttachment);

        depthRef.attachment = renderTarget.hasColor ? 1 : 0;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    if (renderTarget.hasColor) {
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
    }
    if (renderTarget.hasDepth) {
        subpass.pDepthStencilAttachment = &depthRef;
    }

    // Add subpass dependency for proper layout transitions
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;

    if (renderTarget.hasColor) {
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    if (renderTarget.hasDepth) {
        dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    // Add a second dependency for transitioning to shader read (for shadow maps)
    VkSubpassDependency dependency2{};
    dependency2.srcSubpass = 0;
    dependency2.dstSubpass = VK_SUBPASS_EXTERNAL;
    if (!renderTarget.hasColor && renderTarget.hasDepth) {
        // Shadow map: depth needs to be readable in fragment shader
        dependency2.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency2.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency2.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    } else {
        dependency2.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency2.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency2.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }

    std::array<VkSubpassDependency, 2> dependencies = {dependency, dependency2};

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    rpInfo.pDependencies = dependencies.data();

    vkCreateRenderPass(m_impl->state.device, &rpInfo, nullptr, &renderTarget.renderPass);

    // Create framebuffer
    std::vector<VkImageView> fbAttachments;
    if (renderTarget.hasColor) fbAttachments.push_back(renderTarget.colorView);
    if (renderTarget.hasDepth) fbAttachments.push_back(renderTarget.depthView);

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = renderTarget.renderPass;
    fbInfo.attachmentCount = static_cast<uint32_t>(fbAttachments.size());
    fbInfo.pAttachments = fbAttachments.data();
    fbInfo.width = desc.width;
    fbInfo.height = desc.height;
    fbInfo.layers = 1;

    vkCreateFramebuffer(m_impl->state.device, &fbInfo, nullptr, &renderTarget.framebuffer);

    uint32_t id = m_impl->state.nextRenderTargetID++;
    m_impl->state.renderTargets[id] = renderTarget;
    return RenderTargetHandle(id);
}

void GfxDeviceVulkan::destroyRenderTarget(RenderTargetHandle handle) {
    if (!handle.isValid()) return;

    auto it = m_impl->state.renderTargets.find(handle.id);
    if (it != m_impl->state.renderTargets.end()) {
        VulkanRenderTarget& rt = it->second;

        if (!rt.isSwapchainBackbuffer) {
            if (rt.framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(m_impl->state.device, rt.framebuffer, nullptr);
            }
            if (rt.renderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(m_impl->state.device, rt.renderPass, nullptr);
            }
            if (rt.colorTextureHandle.isValid()) {
                destroyTexture(rt.colorTextureHandle);
            }
            if (rt.depthTextureHandle.isValid()) {
                destroyTexture(rt.depthTextureHandle);
            }
        }

        m_impl->state.renderTargets.erase(it);
    }
}

TextureHandle GfxDeviceVulkan::getRenderTargetColorTexture(RenderTargetHandle handle) {
    auto it = m_impl->state.renderTargets.find(handle.id);
    if (it != m_impl->state.renderTargets.end()) {
        return it->second.colorTextureHandle;
    }
    return TextureHandle();
}

TextureHandle GfxDeviceVulkan::getRenderTargetDepthTexture(RenderTargetHandle handle) {
    auto it = m_impl->state.renderTargets.find(handle.id);
    if (it != m_impl->state.renderTargets.end()) {
        return it->second.depthTextureHandle;
    }
    return TextureHandle();
}

void GfxDeviceVulkan::getRenderTargetSize(RenderTargetHandle handle, uint32_t& width, uint32_t& height) {
    auto it = m_impl->state.renderTargets.find(handle.id);
    if (it != m_impl->state.renderTargets.end()) {
        width = it->second.width;
        height = it->second.height;
    } else {
        width = 0;
        height = 0;
    }
}

void GfxDeviceVulkan::attachCubeMapFace(RenderTargetHandle handle, uint32_t faceIndex) {
    auto it = m_impl->state.renderTargets.find(handle.id);
    if (it != m_impl->state.renderTargets.end() && it->second.isCubeMap) {
        it->second.currentCubeFace = static_cast<int>(faceIndex);

        // Recreate framebuffer with the specific face view
        if (it->second.framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_impl->state.device, it->second.framebuffer, nullptr);
        }

        std::vector<VkImageView> attachments;

        // Handle color attachment if present
        if (it->second.hasColor) {
            auto texIt = m_impl->state.textures.find(it->second.colorTextureHandle.id);
            if (texIt != m_impl->state.textures.end() && texIt->second.cubeViews != nullptr) {
                attachments.push_back(texIt->second.cubeViews[faceIndex]);
            }
        }

        // Handle depth attachment if present (for shadow cube maps)
        if (it->second.hasDepth) {
            auto depthTexIt = m_impl->state.textures.find(it->second.depthTextureHandle.id);
            if (depthTexIt != m_impl->state.textures.end() && depthTexIt->second.cubeViews != nullptr) {
                // Use per-face depth view for cube shadow maps
                attachments.push_back(depthTexIt->second.cubeViews[faceIndex]);
            } else if (it->second.depthView != VK_NULL_HANDLE) {
                // Regular depth buffer (non-cube)
                attachments.push_back(it->second.depthView);
            }
        }

        if (!attachments.empty()) {
            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass = it->second.renderPass;
            fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            fbInfo.pAttachments = attachments.data();
            fbInfo.width = it->second.width;
            fbInfo.height = it->second.height;
            fbInfo.layers = 1;

            vkCreateFramebuffer(m_impl->state.device, &fbInfo, nullptr, &it->second.framebuffer);
        }
    }
}

void GfxDeviceVulkan::unbindFramebuffer() {
    // In Vulkan, we don't need to explicitly unbind framebuffers
    // The render pass will handle the transition to shader-readable state
}

// ============================================================================
// UNIFORM BUFFER MANAGEMENT
// ============================================================================

UniformBufferHandle GfxDeviceVulkan::createUniformBuffer(uint32_t size) {
    VulkanUniformBuffer ubo{};
    ubo.size = size;

    m_impl->state.createBuffer(size,
                               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               ubo.buffer, ubo.memory);

    vkMapMemory(m_impl->state.device, ubo.memory, 0, size, 0, &ubo.mappedData);

    uint32_t id = m_impl->state.nextUniformBufferID++;
    m_impl->state.uniformBuffers[id] = ubo;
    return UniformBufferHandle(id);
}

void GfxDeviceVulkan::destroyUniformBuffer(UniformBufferHandle handle) {
    if (!handle.isValid()) return;

    auto it = m_impl->state.uniformBuffers.find(handle.id);
    if (it != m_impl->state.uniformBuffers.end()) {
        if (it->second.mappedData != nullptr) {
            vkUnmapMemory(m_impl->state.device, it->second.memory);
        }
        if (it->second.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_impl->state.device, it->second.buffer, nullptr);
        }
        if (it->second.memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_impl->state.device, it->second.memory, nullptr);
        }
        m_impl->state.uniformBuffers.erase(it);
    }
}

void GfxDeviceVulkan::updateUniformBuffer(UniformBufferHandle handle, const void* data, uint32_t size, uint32_t offset) {
    auto it = m_impl->state.uniformBuffers.find(handle.id);
    if (it != m_impl->state.uniformBuffers.end() && it->second.mappedData != nullptr) {
        memcpy(static_cast<uint8_t*>(it->second.mappedData) + offset, data, size);
    }
}

// ============================================================================
// FRAME MANAGEMENT
// ============================================================================

std::unique_ptr<IGfxCommandList> GfxDeviceVulkan::beginFrame(RenderTargetHandle target) {
    // Find the swapchain that owns this render target (if it's a backbuffer)
    VulkanSwapchain* swapchain = nullptr;
    uint32_t swapchainId = 0;
    for (auto& [id, sc] : m_impl->state.swapchains) {
        if (sc.backbuffer.id == target.id) {
            // Skip dead swapchains (surface destroyed)
            if (sc.isDead) {
                return nullptr;
            }
            swapchain = &sc;
            swapchainId = id;
            m_impl->activeSwapchain = SwapchainHandle(id);
            break;
        }
    }

    if (swapchain == nullptr) {
        // Off-screen render target - use the hint if available
        // This is critical for multi-view rendering: each view's off-screen work
        // (like shadow maps) should use that view's swapchain for synchronization
        if (m_impl->offscreenSwapchainHint.isValid()) {
            auto hintIt = m_impl->state.swapchains.find(m_impl->offscreenSwapchainHint.id);
            if (hintIt != m_impl->state.swapchains.end() && !hintIt->second.isDead) {
                swapchain = &hintIt->second;
                swapchainId = m_impl->offscreenSwapchainHint.id;
                m_impl->activeSwapchain = m_impl->offscreenSwapchainHint;
            }
        }

        // Fallback: use first available if hint not set or invalid
        if (swapchain == nullptr) {
            for (auto& [id, sc] : m_impl->state.swapchains) {
                if (!sc.isDead) {
                    swapchain = &sc;
                    swapchainId = id;
                    m_impl->activeSwapchain = SwapchainHandle(id);
                    break;
                }
            }
        }
        if (swapchain == nullptr) {

            return nullptr;
        }
    }

    m_impl->state.currentSwapchain = swapchain;

    // Acquire the swapchain image for a backbuffer target and bind it to the
    // backbuffer render target. This is decoupled from "frame already in progress"
    // because a frame can be STARTED by an offscreen target (a shadow map or the
    // scene-capture target for post-process), which does not acquire a swapchain
    // image. When the backbuffer is later targeted in the same frame, it must still
    // be acquired here, otherwise its color view stays unset -> the render pass is
    // begun with 0 color attachments and every scene draw is dropped
    // (VUID-vkCmdDraw*-colorAttachmentCount-06179).
    // No-op for offscreen targets and idempotent once acquired this frame. Returns
    // false only when acquisition fails and the frame should be skipped.
    auto acquireBackbuffer = [&]() -> bool {
        auto rtIt = m_impl->state.renderTargets.find(target.id);
        if (rtIt == m_impl->state.renderTargets.end() || !rtIt->second.isSwapchainBackbuffer) {
            return true;  // Offscreen target: nothing to acquire.
        }
        if (swapchain->currentFrame < swapchain->imageAcquired.size() &&
            swapchain->imageAcquired[swapchain->currentFrame]) {
            return true;  // Already acquired this frame.
        }

        // Auto-resize the swapchain if the surface size changed (e.g. inactive editor
        // tab catching up after a window resize) before acquiring.
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_impl->state.physicalDevice,
                                                  swapchain->surface, &capabilities);
        uint32_t surfaceWidth = capabilities.currentExtent.width;
        uint32_t surfaceHeight = capabilities.currentExtent.height;
        if (surfaceWidth != UINT32_MAX && surfaceHeight != UINT32_MAX &&
            (surfaceWidth != swapchain->width || surfaceHeight != swapchain->height)) {
            resizeSwapchain(SwapchainHandle(swapchainId), surfaceWidth, surfaceHeight);
            auto rtUpdateIt = m_impl->state.renderTargets.find(target.id);
            if (rtUpdateIt != m_impl->state.renderTargets.end()) {
                rtUpdateIt->second.width = surfaceWidth;
                rtUpdateIt->second.height = surfaceHeight;
            }
        }

        VkResult result = vkAcquireNextImageKHR(m_impl->state.device, swapchain->swapchain, UINT64_MAX,
                                                swapchain->imageAvailableSemaphores[swapchain->currentFrame],
                                                VK_NULL_HANDLE, &swapchain->currentImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_impl->state.physicalDevice,
                                                      swapchain->surface, &capabilities);
            surfaceWidth = capabilities.currentExtent.width;
            surfaceHeight = capabilities.currentExtent.height;
            if (surfaceWidth != UINT32_MAX && surfaceHeight != UINT32_MAX) {
                resizeSwapchain(SwapchainHandle(swapchainId), surfaceWidth, surfaceHeight);
                result = vkAcquireNextImageKHR(m_impl->state.device, swapchain->swapchain, UINT64_MAX,
                                               swapchain->imageAvailableSemaphores[swapchain->currentFrame],
                                               VK_NULL_HANDLE, &swapchain->currentImageIndex);
                if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
                    m_impl->swapchainNeedsRecovery.insert(swapchainId);
                    return false;
                }
            } else {
                m_impl->swapchainNeedsRecovery.insert(swapchainId);
                return false;
            }
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            m_impl->swapchainNeedsRecovery.insert(swapchainId);
            return false;
        }

        if (swapchain->imageAcquired.size() <= swapchain->currentFrame) {
            swapchain->imageAcquired.resize(swapchain->currentFrame + 1, false);
        }
        swapchain->imageAcquired[swapchain->currentFrame] = true;
        rtIt->second.framebuffer = swapchain->framebuffers[swapchain->currentImageIndex];
        rtIt->second.colorView = swapchain->imageViews[swapchain->currentImageIndex];
        rtIt->second.colorImage = swapchain->images[swapchain->currentImageIndex];
        return true;
    };

    // Check if we're already in a frame for this swapchain
    // This can happen when multiple beginFrame calls are made before present()
    // (e.g., RuntimeApp calls beginFrame, then RenderWorld::renderView calls it again)
    bool alreadyInFrame = m_impl->frameInProgress[swapchainId];

    if (!alreadyInFrame) {
        // ====================================================================================
        // MULTI-VIEWPORT SYNCHRONIZATION CRITICAL PATH
        // ====================================================================================
        // This section handles proper synchronization for multiple simultaneous swapchains.
        // Key points:
        // 1. Wait for fence FIRST - this ensures the previous frame's work is done and the
        //    semaphore has been consumed (no longer has pending operations)
        // 2. THEN acquire the next image - safe because semaphore is now available
        // 3. Use vkQueueWaitIdle (not vkDeviceWaitIdle) for recovery to avoid stalling all viewports
        // 4. Recreate semaphores instead of trying to reuse ones in undefined states
        // ====================================================================================

        // Check if this swapchain needs recovery from a previous acquire failure
        // Recreate the semaphores to get them back into a valid state
        if (m_impl->swapchainNeedsRecovery.count(swapchainId) > 0) {

            // Wait for this swapchain's queue to be idle before recreating semaphores
            // Using vkQueueWaitIdle instead of vkDeviceWaitIdle to avoid blocking other viewports
            vkQueueWaitIdle(m_impl->state.graphicsQueue);

            // Recreate the imageAvailable semaphore for this frame
            uint32_t frameIdx = swapchain->currentFrame;
            if (swapchain->imageAvailableSemaphores[frameIdx] != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_impl->state.device, swapchain->imageAvailableSemaphores[frameIdx], nullptr);
            }

            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            vkCreateSemaphore(m_impl->state.device, &semaphoreInfo, nullptr, &swapchain->imageAvailableSemaphores[frameIdx]);

            m_impl->swapchainNeedsRecovery.erase(swapchainId);
        }

        // ====================================================================================
        // FENCE WAIT - MUST happen BEFORE acquire to ensure semaphore is not pending
        // ====================================================================================
        // The fence is signaled when the previous frame's command buffer submission completes.
        // That submission waits on imageAvailableSemaphore, so once the fence is signaled,
        // we know the semaphore has been consumed and is no longer pending.
        // This fixes VUID-vkAcquireNextImageKHR-semaphore-01779.
        // ====================================================================================
        {
            // Use a timeout to detect GPU hangs - 5 seconds should be more than enough for any frame
            VkResult fenceResult = vkWaitForFences(m_impl->state.device, 1, &swapchain->inFlightFences[swapchain->currentFrame], VK_TRUE, 5000000000ULL);
            if (fenceResult == VK_TIMEOUT) {
                // GPU appears to be hung - force a device wait to try to recover
                // This is a last-ditch effort before the hang becomes permanent

                vkDeviceWaitIdle(m_impl->state.device);
                // Reset the fence manually since the GPU work may have completed during waitIdle
                vkResetFences(m_impl->state.device, 1, &swapchain->inFlightFences[swapchain->currentFrame]);

            } else if (fenceResult != VK_SUCCESS) {
                // Some other error - try to recover

                vkDeviceWaitIdle(m_impl->state.device);
            }
        }

        // NOTE: The swapchain image acquire happens via acquireBackbuffer() below,
        // AFTER this frame-setup block. It must be decoupled from frame setup so that
        // a frame STARTED by an offscreen target (shadow map / scene capture) still
        // acquires the backbuffer when it is later targeted in the same frame.

        // Process deferred resource destructions AFTER the fence wait
        // This ensures all command buffers that referenced these resources have completed execution
        // IMPORTANT: Only process once per frame even with multiple swapchains
        if (!m_impl->deferredDestructionsProcessedThisFrame) {
            m_impl->state.processDeferredDestructions();
            m_impl->deferredDestructionsProcessedThisFrame = true;
        }

        // Reset fence - we always wait for it now, so always reset
        vkResetFences(m_impl->state.device, 1, &swapchain->inFlightFences[swapchain->currentFrame]);

        // Reset descriptor set allocation for all pipelines for this frame
        // ====================================================================================
        // MULTI-VIEWPORT DESCRIPTOR SET SYNCHRONIZATION
        // ====================================================================================
        // CRITICAL: Only reset once per frame index across ALL swapchains!
        //
        // Why: Descriptor sets are SHARED across all pipelines globally, but each frame
        // index (0 or 1) has its own pool. If multiple viewports are at the same frame
        // index and we reset multiple times:
        //   1. Viewport 1 resets frame 0 → pointer = 0
        //   2. Viewport 1 allocates descriptor sets 0-100
        //   3. Viewport 2 resets frame 0 → pointer = 0 (BUG: overwrites viewport 1!)
        //   4. Viewport 2 allocates descriptor sets 0-50 (CORRUPTION: same as viewport 1!)
        //   5. GPU renders both viewports using overlapping descriptor sets → TEARING
        //
        // Fix: Only the FIRST viewport to use a frame index resets its descriptor sets.
        // Subsequent viewports at the same frame index continue allocating from the pool.
        // The flag is cleared in present() when advancing to a new frame, which is safe
        // because we've waited for the fence, ensuring no GPU work is using that frame.
        // ====================================================================================
        uint32_t frameIndex = swapchain->currentFrame;
        if (!m_impl->descriptorSetsResetThisFrame[frameIndex]) {
            for (auto& [id, pipeline] : m_impl->state.pipelines) {
                pipeline.resetDescriptorSetAllocation(frameIndex);
            }
            m_impl->descriptorSetsResetThisFrame[frameIndex] = true;
        }

        // Get per-swapchain command buffer for this frame and reset it
        VkCommandBuffer cmdBuffer = swapchain->commandBuffers[swapchain->currentFrame];
        vkResetCommandBuffer(cmdBuffer, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmdBuffer, &beginInfo);

        m_impl->state.currentCommandBuffer = cmdBuffer;
        m_impl->currentFrameIndex = swapchain->currentFrame;

        // Reset render pass tracking for this frame
        swapchain->renderPassUsed[swapchain->currentFrame] = false;

        // Mark frame as in progress
        m_impl->frameInProgress[swapchainId] = true;
    }

    // Acquire the swapchain image for the backbuffer now (after the per-frame setup,
    // and regardless of whether the frame was already started by an offscreen target).
    // No-op for offscreen targets / if already acquired this frame.
    if (!acquireBackbuffer()) {
        return nullptr;
    }

    // Return a command list wrapper for the per-swapchain command buffer
    VkCommandBuffer cmdBuffer = swapchain->commandBuffers[swapchain->currentFrame];
    auto cmd = std::make_unique<GfxCommandListVulkan>(this, &m_impl->state, cmdBuffer);

    // Set the render target on the command list (matching OpenGL behavior)
    cmd->setRenderTarget(target);

    return cmd;
}

void GfxDeviceVulkan::submit(std::unique_ptr<IGfxCommandList> commandList) {
    if (commandList == nullptr) return;

    auto* vulkanCmdList = static_cast<GfxCommandListVulkan*>(commandList.get());

    // End any active render pass
    vulkanCmdList->endRenderPass();

    // Command list is automatically destroyed when unique_ptr goes out of scope
}

void GfxDeviceVulkan::present(SwapchainHandle swapchainHandle) {
    auto it = m_impl->state.swapchains.find(swapchainHandle.id);
    if (it == m_impl->state.swapchains.end()) return;

    VulkanSwapchain& swapchain = it->second;

    // Skip dead swapchains
    if (swapchain.isDead) {
        m_impl->frameInProgress[swapchainHandle.id] = false;
        return;
    }

    // Check if a frame was actually started for this swapchain
    if (!m_impl->frameInProgress[swapchainHandle.id]) {

        return;
    }

    // Validate that an image was successfully acquired for this frame
    // This prevents presenting an image that was never acquired, which would cause validation errors
    if (swapchain.currentFrame >= swapchain.imageAcquired.size() ||
        !swapchain.imageAcquired[swapchain.currentFrame]) {

        m_impl->frameInProgress[swapchainHandle.id] = false;
        return;
    }

    VkCommandBuffer cmdBuffer = swapchain.commandBuffers[swapchain.currentFrame];

    // Only add a layout transition barrier if NO render pass was used this frame.
    // If a render pass WAS used, it already transitioned to PRESENT_SRC_KHR via finalLayout.
    // Using UNDEFINED as oldLayout would discard the rendered content!
    bool renderPassWasUsed = swapchain.renderPassUsed.size() > swapchain.currentFrame
                              && swapchain.renderPassUsed[swapchain.currentFrame];

    if (!renderPassWasUsed) {
        VkImage swapchainImage = swapchain.images[swapchain.currentImageIndex];

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = swapchainImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;

        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    }

    // End command buffer
    vkEndCommandBuffer(cmdBuffer);

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // Wait on imageAvailable semaphore (indexed by currentFrame, set during acquire)
    VkSemaphore waitSemaphores[] = {swapchain.imageAvailableSemaphores[swapchain.currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    // Signal renderFinished semaphore indexed by the acquired image index
    // This ensures each image has its own semaphore for presentation synchronization
    VkSemaphore signalSemaphores[] = {swapchain.renderFinishedSemaphores[swapchain.currentImageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult result = vkQueueSubmit(m_impl->state.graphicsQueue, 1, &submitInfo, swapchain.inFlightFences[swapchain.currentFrame]);
    if (result != VK_SUCCESS) {
        
    }

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {swapchain.swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &swapchain.currentImageIndex;

    result = vkQueuePresentKHR(m_impl->state.presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        
    } else if (result != VK_SUCCESS) {
        
    }

    // Mark frame as complete - next beginFrame will do full initialization
    m_impl->frameInProgress[swapchainHandle.id] = false;

    // Reset image acquired flag for current frame - it will be set again on next beginRenderTarget
    if (swapchain.currentFrame < swapchain.imageAcquired.size()) {
        swapchain.imageAcquired[swapchain.currentFrame] = false;
    }

    // Reset deferred destructions flag so next frame can process them
    // This ensures deferred destructions are processed once per frame cycle
    m_impl->deferredDestructionsProcessedThisFrame = false;

    // Advance to next frame
    swapchain.currentFrame = (swapchain.currentFrame + 1) % VulkanState::MAX_FRAMES_IN_FLIGHT;

    // Clear the descriptor sets reset flag for the frame we're advancing TO
    // This is safe because we waited for inFlightFences[currentFrame] in beginRenderTarget,
    // which guarantees that any previous GPU work using this frame index has completed.
    // This allows the descriptor sets to be reset on the next use of this frame index.
    m_impl->descriptorSetsResetThisFrame[swapchain.currentFrame] = false;
}

void GfxDeviceVulkan::waitIdle() {
    if (m_impl->state.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_impl->state.device);
    }
}

// ============================================================================
// MESH MANAGEMENT
// ============================================================================

MeshHandle GfxDeviceVulkan::createMesh(const MeshData& meshData) {
    GPUMesh gpuMesh;

    BufferDesc vbDesc;
    vbDesc.size = meshData.vertices.size() * sizeof(Vertex);
    vbDesc.usage = BufferUsage::Vertex;
    vbDesc.initialData = meshData.vertices.data();

    gpuMesh.vertexBuffer = createBuffer(vbDesc);
    gpuMesh.vertexCount = static_cast<uint32_t>(meshData.vertices.size());

    BufferDesc ibDesc;
    ibDesc.size = meshData.indices.size() * sizeof(uint32_t);
    ibDesc.usage = BufferUsage::Index;
    ibDesc.initialData = meshData.indices.data();

    gpuMesh.indexBuffer = createBuffer(ibDesc);
    gpuMesh.indexCount = static_cast<uint32_t>(meshData.indices.size());

    gpuMesh.submeshes = meshData.submeshes;
    gpuMesh.bounds = meshData.bounds;

    uint32_t id = m_impl->nextMeshID++;
    m_impl->meshes[id] = gpuMesh;

    return MeshHandle(id);
}

const GPUMesh* GfxDeviceVulkan::getMesh(MeshHandle handle) const {
    auto it = m_impl->meshes.find(handle.id);
    if (it != m_impl->meshes.end()) {
        return &it->second;
    }
    return nullptr;
}

void GfxDeviceVulkan::destroyMesh(MeshHandle handle) {
    if (!handle.isValid()) return;

    auto it = m_impl->meshes.find(handle.id);
    if (it != m_impl->meshes.end()) {
        if (it->second.vertexBuffer.isValid()) {
            destroyBuffer(it->second.vertexBuffer);
        }
        if (it->second.indexBuffer.isValid()) {
            destroyBuffer(it->second.indexBuffer);
        }
        m_impl->meshes.erase(it);
    }
}

// ============================================================================
// FONT MANAGEMENT
// ============================================================================

FontAtlas GfxDeviceVulkan::buildBakedAtlas(const BakedFontAtlas& baked) {
    FontAtlas fontAtlas;
    if (!baked.success) {
        return fontAtlas;
    }

    // Create texture for font atlas (R8_UNORM for single-channel grayscale)
    TextureDesc texDesc;
    texDesc.width = baked.atlasWidth;
    texDesc.height = baked.atlasHeight;
    texDesc.format = TextureFormat::R8_UNORM;
    texDesc.mipLevels = 1;
    texDesc.usage = TextureUsage::Sampled;

    TextureHandle atlasTexture = createTexture(texDesc);
    if (!atlasTexture.isValid()) {
        return fontAtlas;
    }

    // Upload atlas bitmap data to texture
    updateTexture(atlasTexture, baked.bitmap.data(), 0, 0);

    fontAtlas.texture = atlasTexture;
    fontAtlas.atlasWidth = baked.atlasWidth;
    fontAtlas.atlasHeight = baked.atlasHeight;
    fontAtlas.fontSize = baked.fontSize;
    fontAtlas.lineHeight = baked.lineHeight;
    fontAtlas.ascent = baked.ascent;
    fontAtlas.descent = baked.descent;
    fontAtlas.glyphs = baked.glyphs;
    fontAtlas.kerning = baked.kerning;
    return fontAtlas;
}

FontHandle GfxDeviceVulkan::createFontAtlas(const FontDesc& desc) {
    BakedFontAtlas baked = BakeFontAtlas(desc, GetFontOversample());
    FontAtlas fontAtlas = buildBakedAtlas(baked);
    if (!fontAtlas.texture.isValid()) {
        return FontHandle();
    }

    uint32_t fontID = m_impl->nextFontID++;
    m_impl->fonts[fontID] = std::move(fontAtlas);
    m_impl->fontDescs[fontID] = desc;

    return FontHandle(fontID);
}

void GfxDeviceVulkan::refreshFontAtlases() {
    const float oversample = GetFontOversample();
    for (const auto& [fontID, desc] : m_impl->fontDescs) {
        BakedFontAtlas baked = BakeFontAtlas(desc, oversample);
        FontAtlas fontAtlas = buildBakedAtlas(baked);
        if (!fontAtlas.texture.isValid()) {
            continue;
        }

        auto it = m_impl->fonts.find(fontID);
        if (it != m_impl->fonts.end() && it->second.texture.isValid()) {
            destroyTexture(it->second.texture);
        }
        m_impl->fonts[fontID] = std::move(fontAtlas);
    }
}

const FontAtlas* GfxDeviceVulkan::getFontAtlas(FontHandle handle) const {
    if (!handle.isValid()) {
        return nullptr;
    }

    auto it = m_impl->fonts.find(handle.id);
    if (it != m_impl->fonts.end()) {
        return &it->second;
    }
    return nullptr;
}

void GfxDeviceVulkan::destroyFontAtlas(FontHandle handle) {
    if (!handle.isValid()) {
        return;
    }

    auto it = m_impl->fonts.find(handle.id);
    if (it != m_impl->fonts.end()) {
        // Destroy the font atlas texture
        destroyTexture(it->second.texture);
        m_impl->fonts.erase(it);

    }
    m_impl->fontDescs.erase(handle.id);
}

// ============================================================================
// DEFAULT RESOURCES
// ============================================================================

SamplerHandle GfxDeviceVulkan::getDefaultSampler() {
    return m_impl->defaultSampler;
}

TextureHandle GfxDeviceVulkan::getWhiteTexture() {
    return m_impl->whiteTexture;
}

TextureHandle GfxDeviceVulkan::getBlackTexture() {
    return m_impl->blackTexture;
}

TextureHandle GfxDeviceVulkan::getNormalTexture() {
    return m_impl->normalTexture;
}

// ============================================================================
// INTERNAL VULKAN STATE ACCESS
// ============================================================================

VulkanState* GfxDeviceVulkan::getVulkanState() {
    return &m_impl->state;
}

VkDevice GfxDeviceVulkan::getDevice() {
    return m_impl->state.device;
}

VkPhysicalDevice GfxDeviceVulkan::getPhysicalDevice() {
    return m_impl->state.physicalDevice;
}

VkInstance GfxDeviceVulkan::getInstance() {
    return m_impl->state.instance;
}

VkQueue GfxDeviceVulkan::getGraphicsQueue() {
    return m_impl->state.graphicsQueue;
}

uint32_t GfxDeviceVulkan::getGraphicsQueueFamily() {
    return m_impl->state.graphicsQueueFamily;
}

} // namespace lupine

#endif // LUPINE_HAS_VULKAN

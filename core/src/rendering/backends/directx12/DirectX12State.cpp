#include "lupine/rendering/backends/directx12/DirectX12State.hpp"

#ifdef LUPINE_HAS_DIRECTX12

#include "lupine/logger/Logger.hpp"
#include <algorithm>
#include <cctype>

namespace lupine {

// ============================================================================
// DescriptorHeapAllocator Implementation
// ============================================================================

void DescriptorHeapAllocator::initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
                                         uint32_t maxDescriptors, bool shaderVisible) {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = maxDescriptors;
    heapDesc.Type = type;
    heapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heapDesc.NodeMask = 0;

    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap));
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create descriptor heap: HRESULT = 0x{:08X}", static_cast<unsigned int>(hr));
        return;
    }

    m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);
    m_maxDescriptors = maxDescriptors;
    m_nextFreeIndex = 0;
}

void DescriptorHeapAllocator::shutdown() {
    m_heap.Reset();
    m_freeList.clear();
    m_descriptorSize = 0;
    m_maxDescriptors = 0;
    m_nextFreeIndex = 0;
}

uint32_t DescriptorHeapAllocator::allocate() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Try to reuse a freed slot
    if (!m_freeList.empty()) {
        uint32_t index = m_freeList.back();
        m_freeList.pop_back();
        return index;
    }

    // Allocate new slot
    if (m_nextFreeIndex < m_maxDescriptors) {
        return m_nextFreeIndex++;
    }

    LOG_ERROR(LogCategory::Render, "[DX12] Descriptor heap exhausted!");
    return UINT32_MAX;
}

uint32_t DescriptorHeapAllocator::allocateRange(uint32_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // For contiguous allocation, we must use new slots (can't reuse from free list)
    if (m_nextFreeIndex + count <= m_maxDescriptors) {
        uint32_t startIndex = m_nextFreeIndex;
        m_nextFreeIndex += count;
        return startIndex;
    }

    LOG_ERROR(LogCategory::Render, "[DX12] Descriptor heap exhausted! Cannot allocate {} contiguous descriptors", count);
    return UINT32_MAX;
}

void DescriptorHeapAllocator::free(uint32_t index) {
    if (index == UINT32_MAX) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_freeList.push_back(index);
}

void DescriptorHeapAllocator::freeRange(uint32_t startIndex, uint32_t count) {
    if (startIndex == UINT32_MAX) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    for (uint32_t i = 0; i < count; ++i) {
        m_freeList.push_back(startIndex + i);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapAllocator::getCPUHandle(uint32_t index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * m_descriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapAllocator::getGPUHandle(uint32_t index) const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(index) * m_descriptorSize;
    return handle;
}

// ============================================================================
// UploadHeap Implementation
// ============================================================================

void UploadHeap::initialize(ID3D12Device* device, uint64_t size) {
    m_size = size;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = size;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_resource)
    );

    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to create upload heap: HRESULT = 0x{:08X}", static_cast<unsigned int>(hr));
        return;
    }

    // Map the entire buffer
    D3D12_RANGE readRange = {0, 0};  // We don't read from this resource
    hr = m_resource->Map(0, &readRange, &m_cpuAddress);
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to map upload heap");
        m_resource.Reset();
        return;
    }

    m_gpuAddress = m_resource->GetGPUVirtualAddress();
    m_offset = 0;
}

void UploadHeap::shutdown() {
    if (m_resource && m_cpuAddress) {
        m_resource->Unmap(0, nullptr);
    }
    m_resource.Reset();
    m_cpuAddress = nullptr;
    m_gpuAddress = 0;
    m_size = 0;
    m_offset = 0;
}

UploadHeap::Allocation UploadHeap::allocate(uint64_t size, uint64_t alignment) {
    Allocation alloc = {};

    // Align offset
    uint64_t alignedOffset = DX12Utils::align(m_offset, alignment);

    if (alignedOffset + size > m_size) {
        // Log once per frame (reset in reset())
        if (!m_overflowLogged) {
            LOG_ERROR(LogCategory::Render, "[DX12] Upload heap out of space ({}MB used of {}MB). "
                      "Reduce draw calls per frame or increase heap size.",
                      m_offset / (1024 * 1024), m_size / (1024 * 1024));
            m_overflowLogged = true;
        }
        return alloc;
    }

    // Warn at 90% capacity (once per frame)
    if (!m_highUsageWarned && alignedOffset + size > m_size * 9 / 10) {
        LOG_WARN(LogCategory::Render, "[DX12] Upload heap at 90%% capacity ({}MB/{}MB).",
                 alignedOffset / (1024 * 1024), m_size / (1024 * 1024));
        m_highUsageWarned = true;
    }

    alloc.cpuAddress = static_cast<uint8_t*>(m_cpuAddress) + alignedOffset;
    alloc.gpuAddress = m_gpuAddress + alignedOffset;
    alloc.offset = alignedOffset;

    m_offset = alignedOffset + size;

    return alloc;
}

void UploadHeap::reset() {
    m_offset = 0;
    m_overflowLogged = false;
    m_highUsageWarned = false;
}

// ============================================================================
// DirectX12State Implementation
// ============================================================================

DirectX12State::DirectX12State() = default;

DirectX12State::~DirectX12State() {
    flushDeferredDestructions();

    // Close fence event
    if (fenceEvent) {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }

    // Shutdown descriptor heaps
    rtvHeap.shutdown();
    dsvHeap.shutdown();
    cbvSrvUavHeap.shutdown();
    samplerHeap.shutdown();
    cbvSrvUavStagingHeap.shutdown();
    samplerStagingHeap.shutdown();

    // Shutdown per-frame resources
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        frameResources[i].uploadHeap.shutdown();
        frameResources[i].commandAllocatorPool.clear();
        frameResources[i].commandAllocatorCursor = 0;
    }

    commandList.Reset();
    fence.Reset();
    commandQueue.Reset();

#ifdef _DEBUG
    infoQueue.Reset();
    debugInterface.Reset();
#endif

    device.Reset();
    dxgiAdapter.Reset();
    dxgiFactory.Reset();
}

void DirectX12State::waitForGPU() {
    if (!commandQueue || !fence) return;

    uint64_t value = signalFence();
    waitForFenceValue(value);
}

void DirectX12State::moveToNextFrame() {
    // Track peak SRV table usage for diagnostics before resetting
    if (srvTablePoolIndex > srvTablePoolHighWaterMark) {
        srvTablePoolHighWaterMark = srvTablePoolIndex;
    }

    // Warn once if SRV usage exceeded 75% of budget
    static bool highUsageWarned = false;
    if (!highUsageWarned && srvTablePoolIndex > SRV_TABLES_PER_FRAME * 3 / 4) {
        LOG_WARN(LogCategory::Render, "[DX12] High SRV table usage: {}/{} tables this frame. "
                 "Consider reducing unique texture combinations per frame.",
                 srvTablePoolIndex, SRV_TABLES_PER_FRAME);
        highUsageWarned = true;
    }

    currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

    waitForFenceValue(frameResources[currentFrameIndex].fenceValue);

    frameResources[currentFrameIndex].uploadHeap.reset();

    // The GPU has finished every pass from this frame's previous cycle (waited above), so
    // its whole command-allocator pool is free to reuse from the start.
    frameResources[currentFrameIndex].commandAllocatorCursor = 0;

    srvTablePoolIndex = 0;
}

void DirectX12State::waitForFenceValue(uint64_t value) {
    if (deviceLost) return;

    if (fence->GetCompletedValue() < value) {
        // Check device health before waiting
        HRESULT deviceStatus = device->GetDeviceRemovedReason();
        if (deviceStatus != S_OK) {
            LOG_ERROR(LogCategory::Render, "[DX12] Device removed (0x{:08X}), skipping fence wait",
                      static_cast<unsigned int>(deviceStatus));
            deviceLost = true;
            return;
        }

        HRESULT hr = fence->SetEventOnCompletion(value, fenceEvent);
        if (SUCCEEDED(hr)) {
            // 10-second timeout: long enough for heavy first-frame loads,
            // short enough to detect TDR and keep the editor responsive.
            DWORD result = WaitForSingleObjectEx(fenceEvent, 10000, FALSE);
            if (result == WAIT_TIMEOUT) {
                HRESULT reason = device->GetDeviceRemovedReason();
                LOG_ERROR(LogCategory::Render, "[DX12] Fence wait timed out (10s). Device status: 0x{:08X}. "
                          "GPU hung or scene too complex. Stopping rendering.",
                          static_cast<unsigned int>(reason));
                deviceLost = true;
            } else {
                // Wait completed — but device may have been removed (TDR) during
                // execution.  Check health so we don't proceed with a dead device.
                HRESULT postWaitStatus = device->GetDeviceRemovedReason();
                if (postWaitStatus != S_OK) {
                    LOG_ERROR(LogCategory::Render, "[DX12] Device removed after fence wait: 0x{:08X}",
                              static_cast<unsigned int>(postWaitStatus));
                    deviceLost = true;
                }
            }
        }
    }
}

uint64_t DirectX12State::signalFence() {
    uint64_t value = ++fenceValue;
    commandQueue->Signal(fence.Get(), value);
    return value;
}

void DirectX12State::beginCommandList() {
    if (deviceLost) return;

    if (commandListRecording) {
        return;
    }

    DX12FrameResources& frame = frameResources[currentFrameIndex];

    // Take the next command allocator from this frame's pool (growing it on demand), so
    // this pass records into an allocator no in-flight pass is using. Unlike a single
    // shared allocator, this needs NO per-pass fence wait: every allocator in the pool
    // was last used at least MAX_FRAMES_IN_FLIGHT frames ago, and moveToNextFrame already
    // waited on that frame's final fence before resetting the cursor to 0. Consecutive
    // passes therefore pipeline on the GPU instead of the CPU stalling between them.
    if (frame.commandAllocatorCursor >= frame.commandAllocatorPool.size()) {
        ComPtr<ID3D12CommandAllocator> newAllocator;
        HRESULT createHr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&newAllocator));
        if (FAILED(createHr) || !newAllocator) {
            LOG_ERROR(LogCategory::Render, "[DX12] Failed to grow command allocator pool: HRESULT=0x{:08X}",
                      static_cast<unsigned int>(createHr));
            return;
        }
        frame.commandAllocatorPool.push_back(newAllocator);
    }

    ID3D12CommandAllocator* allocator = frame.commandAllocatorPool[frame.commandAllocatorCursor].Get();
    frame.commandAllocatorCursor++;

    // Reset the allocator (the GPU finished its previous use a full frame cycle ago) then
    // record the command list into it.
    HRESULT hr = allocator->Reset();
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to reset command allocator: HRESULT=0x{:08X}", static_cast<unsigned int>(hr));
        return;
    }

    hr = commandList->Reset(allocator, nullptr);
    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to reset command list: HRESULT=0x{:08X}", static_cast<unsigned int>(hr));
        return;
    }

    commandListRecording = true;
}

void DirectX12State::executeCommandList() {
    if (deviceLost) return;

    // Check if we're actually recording
    if (!commandListRecording) {
        LOG_ERROR(LogCategory::Render, "[DX12] executeCommandList called but command list not recording");
        return;
    }

    // Check device health before executing
    HRESULT deviceStatus = device->GetDeviceRemovedReason();
    if (deviceStatus != S_OK) {
        LOG_ERROR(LogCategory::Render, "[DX12] Device already removed before command list execution! Reason: 0x{:08X}",
                  static_cast<unsigned int>(deviceStatus));
        deviceLost = true;

        // Query DRED — this is likely the first time we detect a GPU hang from a prior command list
        ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dred)))) {
            D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs = {};
            if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs))) {
                const D3D12_AUTO_BREADCRUMB_NODE1* node = breadcrumbs.pHeadAutoBreadcrumbNode;
                while (node) {
                    if (node->pLastBreadcrumbValue && node->BreadcrumbCount > 0) {
                        LOG_ERROR(LogCategory::Render, "[DX12 DRED] CmdList op: {}/{}",
                                  *node->pLastBreadcrumbValue, node->BreadcrumbCount);
                    }
                    node = node->pNext;
                }
            }
            D3D12_DRED_PAGE_FAULT_OUTPUT1 pageFault = {};
            if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&pageFault))) {
                if (pageFault.PageFaultVA != 0) {
                    LOG_ERROR(LogCategory::Render, "[DX12 DRED] Page fault VA: 0x{:016X}", pageFault.PageFaultVA);
                }
            }
        }

        commandListRecording = false;
        return;
    }

    // Close command list
    HRESULT hr = commandList->Close();
    commandListRecording = false;  // Mark as not recording regardless of success

    if (FAILED(hr)) {
        LOG_ERROR(LogCategory::Render, "[DX12] Failed to close command list: HRESULT=0x{:08X}", static_cast<unsigned int>(hr));

        // Try to get more detailed error info from the info queue
        ComPtr<ID3D12InfoQueue> errorInfoQueue;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&errorInfoQueue)))) {
            UINT64 messageCount = errorInfoQueue->GetNumStoredMessages();
            for (UINT64 i = 0; i < messageCount && i < 10; ++i) {
                SIZE_T messageLength = 0;
                errorInfoQueue->GetMessage(i, nullptr, &messageLength);
                if (messageLength > 0) {
                    std::vector<char> messageData(messageLength);
                    D3D12_MESSAGE* message = reinterpret_cast<D3D12_MESSAGE*>(messageData.data());
                    if (SUCCEEDED(errorInfoQueue->GetMessage(i, message, &messageLength))) {
                        LOG_ERROR(LogCategory::Render, "[DX12 Debug] {}", message->pDescription);
                    }
                }
            }
            errorInfoQueue->ClearStoredMessages();
        }

        // Check for device removal
        HRESULT removeReason = device->GetDeviceRemovedReason();
        if (removeReason != S_OK) {
            LOG_ERROR(LogCategory::Render, "[DX12] Device removed! Reason: 0x{:08X}", static_cast<unsigned int>(removeReason));
        }
        return;
    }

    // Execute
    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(1, ppCommandLists);

    // Check for device removal immediately after execute
    HRESULT postExecStatus = device->GetDeviceRemovedReason();
    if (postExecStatus != S_OK) {
        LOG_ERROR(LogCategory::Render, "[DX12] Device removed AFTER ExecuteCommandLists! Reason: 0x{:08X}",
                  static_cast<unsigned int>(postExecStatus));
        deviceLost = true;

        // Query DRED immediately while data is still available
        ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&dred)))) {
            D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs = {};
            if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs))) {
                const D3D12_AUTO_BREADCRUMB_NODE1* node = breadcrumbs.pHeadAutoBreadcrumbNode;
                while (node) {
                    if (node->pLastBreadcrumbValue && node->BreadcrumbCount > 0) {
                        LOG_ERROR(LogCategory::Render, "[DX12 DRED] CmdList op: {}/{}",
                                  *node->pLastBreadcrumbValue, node->BreadcrumbCount);
                    }
                    node = node->pNext;
                }
            }
            D3D12_DRED_PAGE_FAULT_OUTPUT1 pageFault = {};
            if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&pageFault))) {
                if (pageFault.PageFaultVA != 0) {
                    LOG_ERROR(LogCategory::Render, "[DX12 DRED] Page fault VA: 0x{:016X}", pageFault.PageFaultVA);
                    const D3D12_DRED_ALLOCATION_NODE1* freedNode = pageFault.pHeadRecentFreedAllocationNode;
                    int cnt = 0;
                    while (freedNode && cnt < 5) {
                        LOG_ERROR(LogCategory::Render, "[DX12 DRED] Freed: type={}, name='{}'",
                                  static_cast<int>(freedNode->AllocationType),
                                  freedNode->ObjectNameA ? freedNode->ObjectNameA : "unnamed");
                        freedNode = freedNode->pNext;
                        cnt++;
                    }
                    if (!pageFault.pHeadRecentFreedAllocationNode) {
                        LOG_ERROR(LogCategory::Render, "[DX12 DRED] No freed allocs (VA never allocated or still live)");
                    }
                }
            }
        }
    }

    // Signal fence after execution so we can wait for it before resetting allocator
    DX12FrameResources& frame = frameResources[currentFrameIndex];
    frame.fenceValue = signalFence();
}

void DirectX12State::drainDebugMessages() {
    if (!infoQueue) return;

    UINT64 messageCount = infoQueue->GetNumStoredMessages();
    for (UINT64 i = 0; i < messageCount; ++i) {
        SIZE_T messageLength = 0;
        if (FAILED(infoQueue->GetMessage(i, nullptr, &messageLength)) || messageLength == 0) {
            continue;
        }

        std::vector<char> messageData(messageLength);
        D3D12_MESSAGE* message = reinterpret_cast<D3D12_MESSAGE*>(messageData.data());
        if (FAILED(infoQueue->GetMessage(i, message, &messageLength))) {
            continue;
        }

        const char* desc = message->pDescription ? message->pDescription : "(no description)";
        switch (message->Severity) {
            case D3D12_MESSAGE_SEVERITY_CORRUPTION:
            case D3D12_MESSAGE_SEVERITY_ERROR:
                LOG_ERROR(LogCategory::Render, "[DX12 Validation] {}", desc);
                break;
            case D3D12_MESSAGE_SEVERITY_WARNING:
                LOG_WARN(LogCategory::Render, "[DX12 Validation] {}", desc);
                break;
            default:
                LOG_INFO(LogCategory::Render, "[DX12 Validation] {}", desc);
                break;
        }
    }

    infoQueue->ClearStoredMessages();
}

void DirectX12State::transitionResource(ID3D12Resource* resource,
                                        D3D12_RESOURCE_STATES before,
                                        D3D12_RESOURCE_STATES after) {
    if (before == after) return;

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrier);
}

void DirectX12State::deferDestroy(DeferredDestroy::Type type, uint32_t id) {
    deferredDestroys.push_back({type, id, DEFERRED_DESTROY_FRAMES});
}

void DirectX12State::processDeferredDestructions() {
    for (auto& deferred : deferredDestroys) {
        if (deferred.framesRemaining > 0) {
            deferred.framesRemaining--;
        }
    }

    // Remove expired items
    deferredDestroys.erase(
        std::remove_if(deferredDestroys.begin(), deferredDestroys.end(),
            [this](const DeferredDestroy& d) {
                if (d.framesRemaining == 0) {
                    // Actually destroy the resource
                    switch (d.type) {
                        case DeferredDestroy::Type::Buffer:
                            buffers.erase(d.id);
                            break;
                        case DeferredDestroy::Type::Texture: {
                            auto it = textures.find(d.id);
                            if (it != textures.end()) {
                                // SRV/UAV descriptors live in the STAGING heap (see
                                // GfxDeviceDX12::createTexture). Freeing them into the
                                // shader-visible heap leaks staging slots and poisons the
                                // shader-visible free list, so later CBV allocations land
                                // inside the SRV table pool and stomp live descriptors.
                                if (it->second.srvIndex != UINT32_MAX) cbvSrvUavStagingHeap.free(it->second.srvIndex);
                                if (it->second.uavIndex != UINT32_MAX) cbvSrvUavStagingHeap.free(it->second.uavIndex);
                                if (it->second.rtvIndex != UINT32_MAX) rtvHeap.free(it->second.rtvIndex);
                                if (it->second.dsvIndex != UINT32_MAX) dsvHeap.free(it->second.dsvIndex);
                                textures.erase(it);
                            }
                            break;
                        }
                        case DeferredDestroy::Type::Pipeline:
                            pipelines.erase(d.id);
                            break;
                        case DeferredDestroy::Type::Shader:
                            shaders.erase(d.id);
                            break;
                        case DeferredDestroy::Type::Sampler: {
                            auto it = samplers.find(d.id);
                            if (it != samplers.end()) {
                                if (it->second.samplerIndex != UINT32_MAX) samplerStagingHeap.free(it->second.samplerIndex);
                                samplers.erase(it);
                            }
                            break;
                        }
                        case DeferredDestroy::Type::UniformBuffer: {
                            auto it = uniformBuffers.find(d.id);
                            if (it != uniformBuffers.end()) {
                                if (it->second.cbvIndex != UINT32_MAX) cbvSrvUavHeap.free(it->second.cbvIndex);
                                uniformBuffers.erase(it);
                            }
                            break;
                        }
                    }
                    return true;
                }
                return false;
            }),
        deferredDestroys.end()
    );
}

void DirectX12State::flushDeferredDestructions() {
    // Immediately destroy all deferred resources
    for (const auto& d : deferredDestroys) {
        switch (d.type) {
            case DeferredDestroy::Type::Buffer:
                buffers.erase(d.id);
                break;
            case DeferredDestroy::Type::Texture: {
                auto it = textures.find(d.id);
                if (it != textures.end()) {
                    // SRV/UAV descriptors live in the STAGING heap — see the
                    // matching comment in processDeferredDestructions.
                    if (it->second.srvIndex != UINT32_MAX) cbvSrvUavStagingHeap.free(it->second.srvIndex);
                    if (it->second.uavIndex != UINT32_MAX) cbvSrvUavStagingHeap.free(it->second.uavIndex);
                    if (it->second.rtvIndex != UINT32_MAX) rtvHeap.free(it->second.rtvIndex);
                    if (it->second.dsvIndex != UINT32_MAX) dsvHeap.free(it->second.dsvIndex);
                    textures.erase(it);
                }
                break;
            }
            case DeferredDestroy::Type::Pipeline:
                pipelines.erase(d.id);
                break;
            case DeferredDestroy::Type::Shader:
                shaders.erase(d.id);
                break;
            case DeferredDestroy::Type::Sampler: {
                auto it = samplers.find(d.id);
                if (it != samplers.end()) {
                    if (it->second.samplerIndex != UINT32_MAX) samplerStagingHeap.free(it->second.samplerIndex);
                    samplers.erase(it);
                }
                break;
            }
            case DeferredDestroy::Type::UniformBuffer: {
                auto it = uniformBuffers.find(d.id);
                if (it != uniformBuffers.end()) {
                    if (it->second.cbvIndex != UINT32_MAX) cbvSrvUavHeap.free(it->second.cbvIndex);
                    uniformBuffers.erase(it);
                }
                break;
            }
        }
    }
    deferredDestroys.clear();
}

// ============================================================================
// DirectX 12 Utility Functions
// ============================================================================

namespace DX12Utils {

DXGI_FORMAT toDXGIFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_UNORM: return DXGI_FORMAT_R8_UNORM;
        case TextureFormat::RG8_UNORM: return DXGI_FORMAT_R8G8_UNORM;
        case TextureFormat::RGBA8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGBA8_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case TextureFormat::BGRA8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::BGRA8_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

        case TextureFormat::R16_FLOAT: return DXGI_FORMAT_R16_FLOAT;
        case TextureFormat::RG16_FLOAT: return DXGI_FORMAT_R16G16_FLOAT;
        case TextureFormat::RGBA16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;

        case TextureFormat::R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
        case TextureFormat::RG32_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
        case TextureFormat::RGB32_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
        case TextureFormat::RGBA32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;

        case TextureFormat::DEPTH16: return DXGI_FORMAT_D16_UNORM;
        case TextureFormat::DEPTH24: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::DEPTH32F: return DXGI_FORMAT_D32_FLOAT;
        case TextureFormat::DEPTH24_STENCIL8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::DEPTH32F_STENCIL8: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

        case TextureFormat::BC1_RGB: return DXGI_FORMAT_BC1_UNORM;
        case TextureFormat::BC1_RGBA: return DXGI_FORMAT_BC1_UNORM;
        case TextureFormat::BC3_RGBA: return DXGI_FORMAT_BC3_UNORM;
        case TextureFormat::BC5_RG: return DXGI_FORMAT_BC5_UNORM;
        case TextureFormat::BC7_RGBA: return DXGI_FORMAT_BC7_UNORM;

        default: return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT toDXGIFormatTypeless(TextureFormat format) {
    switch (format) {
        case TextureFormat::DEPTH16: return DXGI_FORMAT_R16_TYPELESS;
        case TextureFormat::DEPTH24:
        case TextureFormat::DEPTH24_STENCIL8: return DXGI_FORMAT_R24G8_TYPELESS;
        case TextureFormat::DEPTH32F: return DXGI_FORMAT_R32_TYPELESS;
        case TextureFormat::DEPTH32F_STENCIL8: return DXGI_FORMAT_R32G8X24_TYPELESS;
        default: return toDXGIFormat(format);
    }
}

DXGI_FORMAT toDXGIFormatDSV(TextureFormat format) {
    switch (format) {
        case TextureFormat::DEPTH16: return DXGI_FORMAT_D16_UNORM;
        case TextureFormat::DEPTH24:
        case TextureFormat::DEPTH24_STENCIL8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::DEPTH32F: return DXGI_FORMAT_D32_FLOAT;
        case TextureFormat::DEPTH32F_STENCIL8: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        default: return toDXGIFormat(format);
    }
}

DXGI_FORMAT toDXGIFormatSRV(TextureFormat format) {
    switch (format) {
        case TextureFormat::DEPTH16: return DXGI_FORMAT_R16_UNORM;
        case TextureFormat::DEPTH24:
        case TextureFormat::DEPTH24_STENCIL8: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case TextureFormat::DEPTH32F: return DXGI_FORMAT_R32_FLOAT;
        case TextureFormat::DEPTH32F_STENCIL8: return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        default: return toDXGIFormat(format);
    }
}

D3D12_PRIMITIVE_TOPOLOGY toD3D12Topology(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::PointList: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case PrimitiveTopology::LineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveTopology::LineStrip: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case PrimitiveTopology::TriangleList: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        default: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE toD3D12TopologyType(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::PointList: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case PrimitiveTopology::LineList:
        case PrimitiveTopology::LineStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case PrimitiveTopology::TriangleList:
        case PrimitiveTopology::TriangleStrip: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        default: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
}

D3D12_COMPARISON_FUNC toD3D12CompareFunc(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never: return D3D12_COMPARISON_FUNC_NEVER;
        case CompareFunc::Less: return D3D12_COMPARISON_FUNC_LESS;
        case CompareFunc::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
        case CompareFunc::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case CompareFunc::Greater: return D3D12_COMPARISON_FUNC_GREATER;
        case CompareFunc::NotEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case CompareFunc::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case CompareFunc::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
        default: return D3D12_COMPARISON_FUNC_LESS;
    }
}

D3D12_BLEND toD3D12Blend(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero: return D3D12_BLEND_ZERO;
        case BlendFactor::One: return D3D12_BLEND_ONE;
        case BlendFactor::SrcColor: return D3D12_BLEND_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
        case BlendFactor::DstColor: return D3D12_BLEND_DEST_COLOR;
        case BlendFactor::OneMinusDstColor: return D3D12_BLEND_INV_DEST_COLOR;
        case BlendFactor::SrcAlpha: return D3D12_BLEND_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DstAlpha: return D3D12_BLEND_DEST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
        default: return D3D12_BLEND_ONE;
    }
}

D3D12_BLEND_OP toD3D12BlendOp(BlendOp op) {
    switch (op) {
        case BlendOp::Add: return D3D12_BLEND_OP_ADD;
        case BlendOp::Subtract: return D3D12_BLEND_OP_SUBTRACT;
        case BlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
        case BlendOp::Min: return D3D12_BLEND_OP_MIN;
        case BlendOp::Max: return D3D12_BLEND_OP_MAX;
        default: return D3D12_BLEND_OP_ADD;
    }
}

D3D12_FILTER toD3D12Filter(FilterMode minFilter, FilterMode magFilter, FilterMode mipFilter, bool comparison) {
    bool minLinear = (minFilter == FilterMode::Linear);
    bool magLinear = (magFilter == FilterMode::Linear);
    bool mipLinear = (mipFilter == FilterMode::Linear);

    // Build filter value manually for all combinations
    if (comparison) {
        if (minLinear && magLinear && mipLinear) {
            return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        } else if (minLinear && magLinear && !mipLinear) {
            return D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        } else if (minLinear && !magLinear && mipLinear) {
            return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        } else if (minLinear && !magLinear && !mipLinear) {
            return D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
        } else if (!minLinear && magLinear && mipLinear) {
            return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR;
        } else if (!minLinear && magLinear && !mipLinear) {
            return D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT;
        } else if (!minLinear && !magLinear && mipLinear) {
            return D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR;
        } else {
            return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
        }
    } else {
        if (minLinear && magLinear && mipLinear) {
            return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        } else if (minLinear && magLinear && !mipLinear) {
            return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        } else if (minLinear && !magLinear && mipLinear) {
            return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        } else if (minLinear && !magLinear && !mipLinear) {
            return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        } else if (!minLinear && magLinear && mipLinear) {
            return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
        } else if (!minLinear && magLinear && !mipLinear) {
            return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        } else if (!minLinear && !magLinear && mipLinear) {
            return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        } else {
            return D3D12_FILTER_MIN_MAG_MIP_POINT;
        }
    }
}

D3D12_TEXTURE_ADDRESS_MODE toD3D12AddressMode(WrapMode mode) {
    switch (mode) {
        case WrapMode::Repeat: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case WrapMode::MirroredRepeat: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case WrapMode::ClampToEdge: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case WrapMode::ClampToBorder: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

D3D12_CULL_MODE toD3D12CullMode(CullMode mode) {
    switch (mode) {
        case CullMode::None: return D3D12_CULL_MODE_NONE;
        case CullMode::Front: return D3D12_CULL_MODE_FRONT;
        case CullMode::Back: return D3D12_CULL_MODE_BACK;
        default: return D3D12_CULL_MODE_BACK;
    }
}

D3D12_FILL_MODE toD3D12FillMode(FillMode mode) {
    switch (mode) {
        case FillMode::Solid: return D3D12_FILL_MODE_SOLID;
        case FillMode::Wireframe: return D3D12_FILL_MODE_WIREFRAME;
        default: return D3D12_FILL_MODE_SOLID;
    }
}

DXGI_FORMAT toD3D12VertexFormat(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float: return DXGI_FORMAT_R32_FLOAT;
        case VertexFormat::Float2: return DXGI_FORMAT_R32G32_FLOAT;
        case VertexFormat::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case VertexFormat::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case VertexFormat::Int: return DXGI_FORMAT_R32_SINT;
        case VertexFormat::Int2: return DXGI_FORMAT_R32G32_SINT;
        case VertexFormat::Int3: return DXGI_FORMAT_R32G32B32_SINT;
        case VertexFormat::Int4: return DXGI_FORMAT_R32G32B32A32_SINT;
        case VertexFormat::UInt: return DXGI_FORMAT_R32_UINT;
        case VertexFormat::UInt2: return DXGI_FORMAT_R32G32_UINT;
        case VertexFormat::UInt3: return DXGI_FORMAT_R32G32B32_UINT;
        case VertexFormat::UInt4: return DXGI_FORMAT_R32G32B32A32_UINT;
        default: return DXGI_FORMAT_UNKNOWN;
    }
}

const char* getSemanticNameFromAttributeName(const std::string& name) {
    // Convert attribute name to HLSL semantic name
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    if (lowerName.find("position") != std::string::npos || lowerName == "pos") return "POSITION";
    if (lowerName.find("normal") != std::string::npos) return "NORMAL";
    if (lowerName.find("tangent") != std::string::npos && lowerName.find("bi") == std::string::npos) return "TANGENT";
    if (lowerName.find("bitangent") != std::string::npos) return "BINORMAL";
    if (lowerName.find("color") != std::string::npos || lowerName.find("colour") != std::string::npos) return "COLOR";

    // Bone/skeletal animation attributes
    if (lowerName.find("boneid") != std::string::npos ||
        lowerName.find("boneindex") != std::string::npos ||
        lowerName.find("boneindices") != std::string::npos ||
        lowerName.find("blendindices") != std::string::npos) return "BLENDINDICES";
    if (lowerName.find("boneweight") != std::string::npos ||
        lowerName.find("blendweight") != std::string::npos) return "BLENDWEIGHT";

    if (lowerName.find("texcoord") != std::string::npos || lowerName.find("uv") != std::string::npos) return "TEXCOORD";

    // Default to TEXCOORD for unknown attributes
    return "TEXCOORD";
}

uint32_t getVertexFormatSize(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float: return 4;
        case VertexFormat::Float2: return 8;
        case VertexFormat::Float3: return 12;
        case VertexFormat::Float4: return 16;
        case VertexFormat::Int: return 4;
        case VertexFormat::Int2: return 8;
        case VertexFormat::Int3: return 12;
        case VertexFormat::Int4: return 16;
        case VertexFormat::UInt: return 4;
        case VertexFormat::UInt2: return 8;
        case VertexFormat::UInt3: return 12;
        case VertexFormat::UInt4: return 16;
        default: return 0;
    }
}

bool hasStencilComponent(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_D24_UNORM_S8_UINT ||
           format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
}

bool isDepthFormat(TextureFormat format) {
    return format == TextureFormat::DEPTH16 ||
           format == TextureFormat::DEPTH24 ||
           format == TextureFormat::DEPTH32F ||
           format == TextureFormat::DEPTH24_STENCIL8 ||
           format == TextureFormat::DEPTH32F_STENCIL8;
}

uint32_t getBytesPerPixel(DXGI_FORMAT format) {
    switch (format) {
        case DXGI_FORMAT_R8_UNORM: return 1;
        case DXGI_FORMAT_R8G8_UNORM: return 2;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return 4;
        case DXGI_FORMAT_R16_FLOAT: return 2;
        case DXGI_FORMAT_R16G16_FLOAT: return 4;
        case DXGI_FORMAT_R16G16B16A16_FLOAT: return 8;
        case DXGI_FORMAT_R32_FLOAT: return 4;
        case DXGI_FORMAT_R32G32_FLOAT: return 8;
        case DXGI_FORMAT_R32G32B32_FLOAT: return 12;
        case DXGI_FORMAT_R32G32B32A32_FLOAT: return 16;
        case DXGI_FORMAT_D16_UNORM: return 2;
        case DXGI_FORMAT_D24_UNORM_S8_UINT: return 4;
        case DXGI_FORMAT_D32_FLOAT: return 4;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return 8;
        default: return 4;
    }
}

uint32_t getRowPitch(uint32_t width, DXGI_FORMAT format) {
    uint32_t bytesPerPixel = getBytesPerPixel(format);
    // DX12 requires row pitch to be aligned to D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256 bytes)
    uint32_t rowPitch = width * bytesPerPixel;
    return static_cast<uint32_t>(align(rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
}

} // namespace DX12Utils

} // namespace lupine

#endif // LUPINE_HAS_DIRECTX12

#pragma once

#include <cstdint>

namespace lupine {

/**
 * Type-safe handle template for GPU resources.
 * Prevents accidental mixing of different resource types.
 */
template<typename Tag>
struct Handle {
    uint32_t id = 0;

    constexpr Handle() = default;
    constexpr explicit Handle(uint32_t id) : id(id) {}

    constexpr bool isValid() const { return id != 0; }
    constexpr operator bool() const { return isValid(); }

    constexpr bool operator==(const Handle& other) const { return id == other.id; }
    constexpr bool operator!=(const Handle& other) const { return id != other.id; }
    constexpr bool operator<(const Handle& other) const { return id < other.id; }
};

// Resource handle types
struct MeshTag {};
struct TextureTag {};
struct MaterialTag {};
struct ShaderTag {};
struct PipelineTag {};
struct BufferTag {};
struct SwapchainTag {};
struct RenderTargetTag {};
struct SamplerTag {};
struct UniformBufferTag {};
struct FontTag {};

using MeshHandle = Handle<MeshTag>;
using TextureHandle = Handle<TextureTag>;
using MaterialHandle = Handle<MaterialTag>;
using ShaderHandle = Handle<ShaderTag>;
using PipelineHandle = Handle<PipelineTag>;
using BufferHandle = Handle<BufferTag>;
using SwapchainHandle = Handle<SwapchainTag>;
using RenderTargetHandle = Handle<RenderTargetTag>;
using SamplerHandle = Handle<SamplerTag>;
using UniformBufferHandle = Handle<UniformBufferTag>;
using FontHandle = Handle<FontTag>;

} // namespace lupine

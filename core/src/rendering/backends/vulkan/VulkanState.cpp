#include "lupine/rendering/backends/vulkan/VulkanState.hpp"
#include "lupine/logger/Logger.hpp"

#ifdef LUPINE_HAS_VULKAN

namespace lupine {

VulkanState::VulkanState() {
}

VulkanState::~VulkanState() {
}

uint32_t VulkanState::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return UINT32_MAX;
}

VkCommandBuffer VulkanState::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanState::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void VulkanState::transitionImageLayout(VkImage image, VkFormat format,
                                         VkImageLayout oldLayout, VkImageLayout newLayout,
                                         uint32_t mipLevels, uint32_t layerCount,
                                         uint32_t baseMipLevel, uint32_t baseArrayLayer) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.baseMipLevel = baseMipLevel;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
    barrier.subresourceRange.layerCount = layerCount;

    // Use format-based aspect mask detection
    barrier.subresourceRange.aspectMask = VkUtils::getImageAspect(format);

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else {
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

void VulkanState::copyBufferToImage(VkBuffer buffer, VkImage image,
                                     uint32_t width, uint32_t height, uint32_t depth) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, depth};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

void VulkanState::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

void VulkanState::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                VkMemoryPropertyFlags properties,
                                VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        
        return;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        
        return;
    }

    vkBindBufferMemory(device, buffer, memory, 0);
}

void VulkanState::createImage(uint32_t width, uint32_t height, uint32_t depth,
                               uint32_t mipLevels, uint32_t arrayLayers,
                               VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                               VkMemoryPropertyFlags properties, VkImageCreateFlags flags,
                               VkImage& image, VkDeviceMemory& memory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = depth;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = arrayLayers;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = flags;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        
        return;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        
        return;
    }

    vkBindImageMemory(device, image, memory, 0);
}

VkImageView VulkanState::createImageView(VkImage image, VkFormat format,
                                          VkImageAspectFlags aspectFlags,
                                          VkImageViewType viewType,
                                          uint32_t baseMipLevel, uint32_t mipLevels,
                                          uint32_t baseArrayLayer, uint32_t layerCount) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = viewType;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
    viewInfo.subresourceRange.layerCount = layerCount;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        
        return VK_NULL_HANDLE;
    }

    return imageView;
}

void VulkanState::generateMipmaps(VkImage image, VkFormat format,
                                   uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        
        return;
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
                       image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_LINEAR);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             0, nullptr, 0, nullptr, 1, &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    endSingleTimeCommands(commandBuffer);
}

// VkUtils implementation
namespace VkUtils {

VkFormat toVkFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::R8_UNORM: return VK_FORMAT_R8_UNORM;
        case TextureFormat::RG8_UNORM: return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGBA8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case TextureFormat::BGRA8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::BGRA8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
        case TextureFormat::R16_FLOAT: return VK_FORMAT_R16_SFLOAT;
        case TextureFormat::RG16_FLOAT: return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::RGBA16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::R32_FLOAT: return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::RG32_FLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::RGB32_FLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case TextureFormat::RGBA32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::DEPTH16: return VK_FORMAT_D16_UNORM;
        case TextureFormat::DEPTH24: return VK_FORMAT_D24_UNORM_S8_UINT;  // Vulkan doesn't have pure D24
        case TextureFormat::DEPTH32F: return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::DEPTH24_STENCIL8: return VK_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::DEPTH32F_STENCIL8: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case TextureFormat::BC1_RGB: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case TextureFormat::BC1_RGBA: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case TextureFormat::BC3_RGBA: return VK_FORMAT_BC3_UNORM_BLOCK;
        case TextureFormat::BC5_RG: return VK_FORMAT_BC5_UNORM_BLOCK;
        case TextureFormat::BC7_RGBA: return VK_FORMAT_BC7_UNORM_BLOCK;
        default: return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

VkPrimitiveTopology toVkTopology(PrimitiveTopology topology) {
    switch (topology) {
        case PrimitiveTopology::PointList: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case PrimitiveTopology::LineList: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case PrimitiveTopology::LineStrip: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case PrimitiveTopology::TriangleList: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

VkCompareOp toVkCompareOp(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never: return VK_COMPARE_OP_NEVER;
        case CompareFunc::Less: return VK_COMPARE_OP_LESS;
        case CompareFunc::Equal: return VK_COMPARE_OP_EQUAL;
        case CompareFunc::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareFunc::Greater: return VK_COMPARE_OP_GREATER;
        case CompareFunc::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
        case CompareFunc::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case CompareFunc::Always: return VK_COMPARE_OP_ALWAYS;
        default: return VK_COMPARE_OP_LESS;
    }
}

VkBlendFactor toVkBlendFactor(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero: return VK_BLEND_FACTOR_ZERO;
        case BlendFactor::One: return VK_BLEND_FACTOR_ONE;
        case BlendFactor::SrcColor: return VK_BLEND_FACTOR_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor: return VK_BLEND_FACTOR_DST_COLOR;
        case BlendFactor::OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha: return VK_BLEND_FACTOR_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case BlendFactor::ConstantColor: return VK_BLEND_FACTOR_CONSTANT_COLOR;
        case BlendFactor::OneMinusConstantColor: return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
        default: return VK_BLEND_FACTOR_ONE;
    }
}

VkBlendOp toVkBlendOp(BlendOp op) {
    switch (op) {
        case BlendOp::Add: return VK_BLEND_OP_ADD;
        case BlendOp::Subtract: return VK_BLEND_OP_SUBTRACT;
        case BlendOp::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendOp::Min: return VK_BLEND_OP_MIN;
        case BlendOp::Max: return VK_BLEND_OP_MAX;
        default: return VK_BLEND_OP_ADD;
    }
}

VkFilter toVkFilter(FilterMode mode) {
    switch (mode) {
        case FilterMode::Nearest: return VK_FILTER_NEAREST;
        case FilterMode::Linear: return VK_FILTER_LINEAR;
        default: return VK_FILTER_LINEAR;
    }
}

VkSamplerAddressMode toVkWrapMode(WrapMode mode) {
    switch (mode) {
        case WrapMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case WrapMode::MirroredRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case WrapMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case WrapMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VkShaderStageFlagBits toVkShaderStage(ShaderStage stage) {
    switch (stage) {
        case ShaderStage::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
        default: return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

VkCullModeFlags toVkCullMode(CullMode mode) {
    switch (mode) {
        case CullMode::None: return VK_CULL_MODE_NONE;
        case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case CullMode::Back: return VK_CULL_MODE_BACK_BIT;
        default: return VK_CULL_MODE_BACK_BIT;
    }
}

VkPolygonMode toVkPolygonMode(FillMode mode) {
    switch (mode) {
        case FillMode::Solid: return VK_POLYGON_MODE_FILL;
        case FillMode::Wireframe: return VK_POLYGON_MODE_LINE;
        default: return VK_POLYGON_MODE_FILL;
    }
}

VkFrontFace toVkFrontFace(WindingOrder order) {
    // Vulkan uses top-left origin, so counter-clockwise in engine space
    // becomes clockwise in Vulkan space
    switch (order) {
        case WindingOrder::Clockwise: return VK_FRONT_FACE_CLOCKWISE;
        case WindingOrder::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        default: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
}

VkIndexType toVkIndexType(IndexFormat format) {
    switch (format) {
        case IndexFormat::UInt16: return VK_INDEX_TYPE_UINT16;
        case IndexFormat::UInt32: return VK_INDEX_TYPE_UINT32;
        default: return VK_INDEX_TYPE_UINT32;
    }
}

VkFormat toVkVertexFormat(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float: return VK_FORMAT_R32_SFLOAT;
        case VertexFormat::Float2: return VK_FORMAT_R32G32_SFLOAT;
        case VertexFormat::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
        case VertexFormat::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case VertexFormat::Int: return VK_FORMAT_R32_SINT;
        case VertexFormat::Int2: return VK_FORMAT_R32G32_SINT;
        case VertexFormat::Int3: return VK_FORMAT_R32G32B32_SINT;
        case VertexFormat::Int4: return VK_FORMAT_R32G32B32A32_SINT;
        case VertexFormat::UInt: return VK_FORMAT_R32_UINT;
        case VertexFormat::UInt2: return VK_FORMAT_R32G32_UINT;
        case VertexFormat::UInt3: return VK_FORMAT_R32G32B32_UINT;
        case VertexFormat::UInt4: return VK_FORMAT_R32G32B32A32_UINT;
        default: return VK_FORMAT_R32_SFLOAT;
    }
}

VkImageType toVkImageType(TextureType type) {
    switch (type) {
        case TextureType::Texture2D:
        case TextureType::TextureCube:
        case TextureType::Texture2DArray:
        case TextureType::TextureCubeArray:
            return VK_IMAGE_TYPE_2D;
        case TextureType::Texture3D:
            return VK_IMAGE_TYPE_3D;
        default:
            return VK_IMAGE_TYPE_2D;
    }
}

VkImageViewType toVkImageViewType(TextureType type) {
    switch (type) {
        case TextureType::Texture2D: return VK_IMAGE_VIEW_TYPE_2D;
        case TextureType::Texture3D: return VK_IMAGE_VIEW_TYPE_3D;
        case TextureType::TextureCube: return VK_IMAGE_VIEW_TYPE_CUBE;
        case TextureType::Texture2DArray: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case TextureType::TextureCubeArray: return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        default: return VK_IMAGE_VIEW_TYPE_2D;
    }
}

uint32_t getVertexFormatSize(VertexFormat format) {
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

bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           format == VK_FORMAT_D24_UNORM_S8_UINT;
}

bool isDepthFormat(TextureFormat format) {
    return format == TextureFormat::DEPTH16 ||
           format == TextureFormat::DEPTH24 ||
           format == TextureFormat::DEPTH32F ||
           format == TextureFormat::DEPTH24_STENCIL8 ||
           format == TextureFormat::DEPTH32F_STENCIL8;
}

VkImageAspectFlags getImageAspect(VkFormat format) {
    switch (format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

} // namespace VkUtils

// Deferred destruction implementation
void VulkanState::deferPipelineDestroy(const VulkanPipeline& pipeline) {
    // Keep the pipeline alive for MAX_FRAMES_IN_FLIGHT frames
    // to ensure all in-flight command buffers have completed
    deferredPipelineDestroys.push_back({pipeline, MAX_FRAMES_IN_FLIGHT});
}

void VulkanState::deferShaderDestroy(const VulkanShader& shader) {
    deferredShaderDestroys.push_back({shader, MAX_FRAMES_IN_FLIGHT});
}

void VulkanState::deferBufferDestroy(const VulkanBuffer& buffer, uint32_t bufferId) {
    deferredBufferDestroys.push_back({buffer, bufferId, MAX_FRAMES_IN_FLIGHT});
}

void VulkanState::deferBufferDestroyNoMap(const VulkanBuffer& buffer) {
    // Use 0 as a sentinel value meaning "don't remove from map"
    deferredBufferDestroys.push_back({buffer, 0, MAX_FRAMES_IN_FLIGHT});
}

void VulkanState::deferTextureDestroy(const VulkanTexture& texture) {
    deferredTextureDestroys.push_back({texture, MAX_FRAMES_IN_FLIGHT});
}

void VulkanState::processDeferredDestructions() {
    // Process deferred pipeline destructions
    for (auto it = deferredPipelineDestroys.begin(); it != deferredPipelineDestroys.end();) {
        if (--it->framesRemaining == 0) {
            VulkanPipeline& pipeline = it->pipeline;

            // Destroy all pipeline variants
            for (auto& variantPair : pipeline.variants) {
                if (variantPair.second.pipeline != VK_NULL_HANDLE) {
                    vkDestroyPipeline(device, variantPair.second.pipeline, nullptr);
                    // If this variant is also the default/fallback pipeline, mark it as destroyed
                    if (variantPair.second.pipeline == pipeline.pipeline) {
                        pipeline.pipeline = VK_NULL_HANDLE;
                    }
                }
            }
            pipeline.variants.clear();

            // Destroy the default/fallback pipeline (only if not already destroyed above)
            if (pipeline.pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(device, pipeline.pipeline, nullptr);
            }
            if (pipeline.layout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
            }
            if (pipeline.descriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, pipeline.descriptorPool, nullptr);
            }
            if (pipeline.descriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, pipeline.descriptorSetLayout, nullptr);
            }
            it = deferredPipelineDestroys.erase(it);
        } else {
            ++it;
        }
    }

    // Process deferred shader destructions
    for (auto it = deferredShaderDestroys.begin(); it != deferredShaderDestroys.end();) {
        if (--it->framesRemaining == 0) {
            if (it->shader.module != VK_NULL_HANDLE) {
                vkDestroyShaderModule(device, it->shader.module, nullptr);
            }
            it = deferredShaderDestroys.erase(it);
        } else {
            ++it;
        }
    }

    // Process deferred buffer destructions
    for (auto it = deferredBufferDestroys.begin(); it != deferredBufferDestroys.end();) {
        if (--it->framesRemaining == 0) {
            VulkanBuffer& buffer = it->buffer;
            if (buffer.buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, buffer.buffer, nullptr);
            }
            if (buffer.memory != VK_NULL_HANDLE) {
                vkFreeMemory(device, buffer.memory, nullptr);
            }
            // Now safe to remove from the buffers map since GPU is done with it
            // Only remove if bufferId is non-zero (0 means buffer was not in the map)
            if (it->bufferId != 0) {
                buffers.erase(it->bufferId);
            }
            it = deferredBufferDestroys.erase(it);
        } else {
            ++it;
        }
    }

    // Process deferred texture destructions
    for (auto it = deferredTextureDestroys.begin(); it != deferredTextureDestroys.end();) {
        if (--it->framesRemaining == 0) {
            VulkanTexture& texture = it->texture;
            if (texture.cubeViews != nullptr) {
                for (uint32_t i = 0; i < 6; i++) {
                    if (texture.cubeViews[i] != VK_NULL_HANDLE) {
                        vkDestroyImageView(device, texture.cubeViews[i], nullptr);
                    }
                }
                delete[] texture.cubeViews;
            }
            if (texture.view != VK_NULL_HANDLE) {
                vkDestroyImageView(device, texture.view, nullptr);
            }
            if (!texture.isSwapchainImage) {
                if (texture.image != VK_NULL_HANDLE) {
                    vkDestroyImage(device, texture.image, nullptr);
                }
                if (texture.memory != VK_NULL_HANDLE) {
                    vkFreeMemory(device, texture.memory, nullptr);
                }
            }
            it = deferredTextureDestroys.erase(it);
        } else {
            ++it;
        }
    }
}

void VulkanState::flushDeferredDestructions() {
    // Force destroy all deferred resources immediately - used during shutdown
    for (auto& deferred : deferredPipelineDestroys) {
        VulkanPipeline& pipeline = deferred.pipeline;

        // Destroy all pipeline variants first
        for (auto& variantPair : pipeline.variants) {
            if (variantPair.second.pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(device, variantPair.second.pipeline, nullptr);
                // If this variant is also the default/fallback pipeline, mark it as destroyed
                if (variantPair.second.pipeline == pipeline.pipeline) {
                    pipeline.pipeline = VK_NULL_HANDLE;
                }
            }
        }
        pipeline.variants.clear();

        // Destroy the default/fallback pipeline (only if not already destroyed above)
        if (pipeline.pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline.pipeline, nullptr);
        }
        if (pipeline.layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipeline.layout, nullptr);
        }
        if (pipeline.descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, pipeline.descriptorPool, nullptr);
        }
        if (pipeline.descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, pipeline.descriptorSetLayout, nullptr);
        }
    }
    deferredPipelineDestroys.clear();

    for (auto& deferred : deferredShaderDestroys) {
        if (deferred.shader.module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, deferred.shader.module, nullptr);
        }
    }
    deferredShaderDestroys.clear();

    for (auto& deferred : deferredBufferDestroys) {
        VulkanBuffer& buffer = deferred.buffer;
        if (buffer.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, buffer.buffer, nullptr);
        }
        if (buffer.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, buffer.memory, nullptr);
        }
    }
    deferredBufferDestroys.clear();

    for (auto& deferred : deferredTextureDestroys) {
        VulkanTexture& texture = deferred.texture;
        if (texture.cubeViews != nullptr) {
            for (uint32_t i = 0; i < 6; i++) {
                if (texture.cubeViews[i] != VK_NULL_HANDLE) {
                    vkDestroyImageView(device, texture.cubeViews[i], nullptr);
                }
            }
            delete[] texture.cubeViews;
        }
        if (texture.view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, texture.view, nullptr);
        }
        if (!texture.isSwapchainImage) {
            if (texture.image != VK_NULL_HANDLE) {
                vkDestroyImage(device, texture.image, nullptr);
            }
            if (texture.memory != VK_NULL_HANDLE) {
                vkFreeMemory(device, texture.memory, nullptr);
            }
        }
    }
    deferredTextureDestroys.clear();
}

void VulkanState::createDummyResources() {
    // Create dummy uniform buffer. Sized to match GfxCommandListVulkan::MATERIAL_UBO_SIZE
    // (512) so it can serve as a safe fallback for the binding-2 material UBO range when no
    // real material buffer is bound, in addition to the smaller camera/bone fallbacks.
    constexpr size_t DUMMY_UBO_SIZE = 512;
    createBuffer(
        DUMMY_UBO_SIZE,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        dummyUniformBuffer,
        dummyUniformBufferMemory
    );

    // Initialize the dummy buffer with sensible defaults
    // This buffer may be used for various UBO bindings (camera, material, light)
    // when no specific buffer is bound. We set defaults that won't cause black rendering.
    void* data;
    vkMapMemory(device, dummyUniformBufferMemory, 0, DUMMY_UBO_SIZE, 0, &data);
    memset(data, 0, DUMMY_UBO_SIZE);

    // Material UBO defaults (binding 2) - NEW STANDARDIZED LAYOUT
    // Layout matches MaterialUBOData struct in GfxCommandListVulkan.hpp
    // Skybox params (offset 0-79):
    //   - int u_SkyboxType (offset 0)
    //   - float _pad0[3] (offset 4)
    //   - vec4 u_SkyboxColor (offset 16)
    //   - vec4 u_SkyTopColor (offset 32)
    //   - vec4 u_SkyHorizonColor (offset 48)
    //   - vec4 u_SkyBottomColor (offset 64)
    // PBR material params (offset 80+):
    //   - vec4 u_AlbedoColor (offset 80, floats 20-23)
    //   - vec4 u_EmissiveColor (offset 96, floats 24-27)
    //   - vec4 u_MaterialParams1 (offset 112, floats 28-31) [metallic, roughness, normalScale, emissiveStrength]
    //   - vec4 u_MaterialParams2 (offset 128, floats 32-35) [alphaCutoff, aoStrength, heightScale, unused]
    //   - vec4 u_TextureFlags (offset 144, floats 36-39) [useAlbedo, useMetallicRoughness, useNormal, useEmissive]
    //   - vec4 u_TintColor (offset 160, floats 40-43)
    //   - int u_ReceiveShadow (offset 176, int 44)
    float* floatData = static_cast<float*>(data);
    int32_t* intData = static_cast<int32_t*>(data);

    // Skybox params (offset 0-79) - defaults for non-skybox shaders
    intData[0] = 0;  // u_SkyboxType = 0 (None)
    // Skybox colors at offsets 16, 32, 48, 64 - leave as zeros (not used by PBR shaders)

    // PBR material params (offset 80+)
    // u_AlbedoColor = vec4(1,1,1,0) at offset 80 (floats 20-23)
    // Alpha = 0 so rounded rect shaders use v_Color * u_TintColor instead
    floatData[20] = 1.0f;
    floatData[21] = 1.0f;
    floatData[22] = 1.0f;
    floatData[23] = 0.0f;  // Alpha 0 = use tint color in rounded rect shaders

    // u_EmissiveColor = vec4(0,0,0,1) at offset 96 (floats 24-27)
    floatData[24] = 0.0f;
    floatData[25] = 0.0f;
    floatData[26] = 0.0f;
    floatData[27] = 1.0f;

    // u_MaterialParams1 = vec4(0.0, 0.5, 1.0, 1.0) at offset 112 (floats 28-31)
    floatData[28] = 0.0f;   // metallic
    floatData[29] = 0.5f;   // roughness
    floatData[30] = 1.0f;   // normalScale
    floatData[31] = 1.0f;   // emissiveStrength

    // u_MaterialParams2 = vec4(0.5, 1.0, 0.0, 0.0) at offset 128 (floats 32-35)
    floatData[32] = 0.5f;   // alphaCutoff
    floatData[33] = 1.0f;   // aoStrength
    floatData[34] = 0.0f;   // heightScale
    floatData[35] = 0.0f;   // unused

    // u_TextureFlags = vec4(1,1,1,1) at offset 144 (floats 36-39)
    // ENABLE ALL TEXTURE SAMPLING BY DEFAULT - Critical for PBR shaders!
    floatData[36] = 1.0f;   // useAlbedo
    floatData[37] = 1.0f;   // useMetallicRoughness
    floatData[38] = 1.0f;   // useNormal
    floatData[39] = 1.0f;   // useEmissive

    // u_TintColor = vec4(1,1,1,1) at offset 160 (floats 40-43)
    floatData[40] = 1.0f;
    floatData[41] = 1.0f;
    floatData[42] = 1.0f;
    floatData[43] = 1.0f;

    // u_ReceiveShadow = 1 at offset 176 (int 44)
    intData[44] = 1;

    // Rounded rect params (offset 192+)
    // u_CornerRadius = vec4(0,0,0,0) at offset 192 (floats 48-51)
    floatData[48] = 0.0f;
    floatData[49] = 0.0f;
    floatData[50] = 0.0f;
    floatData[51] = 0.0f;

    // u_SizeAndUV = vec4(100,100,0,0) at offset 208 (floats 52-55)
    floatData[52] = 100.0f;  // width
    floatData[53] = 100.0f;  // height
    floatData[54] = 0.0f;    // unused
    floatData[55] = 0.0f;    // unused

    // u_UVMinMax = vec4(0,0,1,1) at offset 224 (floats 56-59)
    floatData[56] = 0.0f;    // uvMin.x
    floatData[57] = 0.0f;    // uvMin.y
    floatData[58] = 1.0f;    // uvMax.x
    floatData[59] = 1.0f;    // uvMax.y

    // u_BorderWidth = vec4(0,0,0,0) at offset 240 (floats 60-63)
    floatData[60] = 0.0f;
    floatData[61] = 0.0f;
    floatData[62] = 0.0f;
    floatData[63] = 0.0f;

    // u_EnableAntialiasing = 1 at offset 256 (int 64)
    intData[64] = 1;

    // For camera UBO at binding 0: set identity view matrix
    // First 16 floats would be view matrix (identity)
    // Already zeroed, identity matrix diagonal is set below if needed

    vkUnmapMemory(device, dummyUniformBufferMemory);

    // Create dedicated dummy light UBO (binding 3)
    // Must be large enough for LightUniformBuffer (~7KB) and initialized with lightCounts = 0
    // This prevents infinite shader loops when the dummy is used
    createBuffer(
        DUMMY_LIGHT_UBO_SIZE,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        dummyLightUBO,
        dummyLightUBOMemory
    );

    // Initialize dummy light UBO with zeros - critically, lightCounts will be 0
    // LightUniformBuffer layout (approximate offsets):
    //   - GPULightData lights[16]: 0 - 1280
    //   - ShadowMapData shadowMaps[8]: 1280 - 2048
    //   - CascadedShadowMapData cascadedShadowMaps[8]: 2048 - 6528
    //   - Vec4 ambientLight: ~6528
    //   - Vec4 lightCounts: ~6544 (x=numLights MUST be 0)
    void* lightData;
    vkMapMemory(device, dummyLightUBOMemory, 0, DUMMY_LIGHT_UBO_SIZE, 0, &lightData);
    // A full zero-fill is what this buffer needs: the only field that must hold a
    // specific value is lightCounts.x = 0 (no lights), and ambientLight stays black.
    memset(lightData, 0, DUMMY_LIGHT_UBO_SIZE);
    vkUnmapMemory(device, dummyLightUBOMemory);

    // Create dummy 1x1 white texture
    createImage(
        1, 1, 1,  // width, height, depth
        1,        // mip levels
        1,        // array layers
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        0,  // flags
        dummyTexture,
        dummyTextureMemory
    );

    // Create image view
    dummyTextureView = createImageView(
        dummyTexture,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    // Transition to shader read and upload a white pixel
    transitionImageLayout(dummyTexture, VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Create staging buffer for 1 pixel (4 bytes RGBA)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    createBuffer(
        4,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingMemory
    );

    // White pixel
    vkMapMemory(device, stagingMemory, 0, 4, 0, &data);
    uint8_t* pixelData = static_cast<uint8_t*>(data);
    pixelData[0] = 255;  // R
    pixelData[1] = 255;  // G
    pixelData[2] = 255;  // B
    pixelData[3] = 255;  // A
    vkUnmapMemory(device, stagingMemory);

    // Copy to image
    copyBufferToImage(stagingBuffer, dummyTexture, 1, 1);

    // Transition to shader read optimal
    transitionImageLayout(dummyTexture, VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    // Create dummy 1x1 cube map texture (6 faces for cube map sampling)
    createImage(
        1, 1, 1,  // width, height, depth
        1,        // mip levels
        6,        // array layers (6 faces for cube map)
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,  // Cube map flag
        dummyCubeMap,
        dummyCubeMapMemory
    );

    // Create cube map image view
    dummyCubeMapView = createImageView(
        dummyCubeMap,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_VIEW_TYPE_CUBE,  // Cube map view type
        0, 1,  // mip levels
        0, 6   // all 6 faces
    );

    // Transition cube map to shader read optimal
    transitionImageLayout(dummyCubeMap, VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          1, 6);

    // Create dummy sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &dummySampler) != VK_SUCCESS) {
        
    }

}

void VulkanState::destroyDummyResources() {
    if (dummySampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, dummySampler, nullptr);
        dummySampler = VK_NULL_HANDLE;
    }
    if (dummyTextureView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, dummyTextureView, nullptr);
        dummyTextureView = VK_NULL_HANDLE;
    }
    if (dummyTexture != VK_NULL_HANDLE) {
        vkDestroyImage(device, dummyTexture, nullptr);
        dummyTexture = VK_NULL_HANDLE;
    }
    if (dummyTextureMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, dummyTextureMemory, nullptr);
        dummyTextureMemory = VK_NULL_HANDLE;
    }
    if (dummyUniformBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, dummyUniformBuffer, nullptr);
        dummyUniformBuffer = VK_NULL_HANDLE;
    }
    if (dummyUniformBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, dummyUniformBufferMemory, nullptr);
        dummyUniformBufferMemory = VK_NULL_HANDLE;
    }
    // Cleanup dedicated dummy light UBO
    if (dummyLightUBO != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, dummyLightUBO, nullptr);
        dummyLightUBO = VK_NULL_HANDLE;
    }
    if (dummyLightUBOMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, dummyLightUBOMemory, nullptr);
        dummyLightUBOMemory = VK_NULL_HANDLE;
    }
    if (dummyCubeMapView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, dummyCubeMapView, nullptr);
        dummyCubeMapView = VK_NULL_HANDLE;
    }
    if (dummyCubeMap != VK_NULL_HANDLE) {
        vkDestroyImage(device, dummyCubeMap, nullptr);
        dummyCubeMap = VK_NULL_HANDLE;
    }
    if (dummyCubeMapMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, dummyCubeMapMemory, nullptr);
        dummyCubeMapMemory = VK_NULL_HANDLE;
    }
}

// ============================================================================
// BINDLESS TEXTURE SYSTEM IMPLEMENTATION
// ============================================================================

void VulkanState::createBindlessResources() {
    if (!descriptorIndexingSupported) {
        
        bindlessSupported = false;
        return;
    }

    // Create bindless descriptor set layout with:
    // - Binding 0: Unbounded array of sampled images (textures)
    // - Binding 1: Array of samplers
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

    // Binding 0: Texture array (sampled images)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[0].descriptorCount = MAX_BINDLESS_TEXTURES;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // Binding 1: Sampler array
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[1].descriptorCount = MAX_BINDLESS_SAMPLERS;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    // Binding flags for update-after-bind and partially bound descriptors
    // NOTE: VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT must be on the LAST binding
    // per Vulkan spec VUID-VkDescriptorSetLayoutBindingFlagsCreateInfo-pBindingFlags-03004
    std::array<VkDescriptorBindingFlags, 2> bindingFlags{};
    bindingFlags[0] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                      VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    bindingFlags[1] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                      VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                      VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
    bindingFlagsInfo.pBindingFlags = bindingFlags.data();

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.pNext = &bindingFlagsInfo;

    VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &bindlessDescriptorSetLayout);
    if (result != VK_SUCCESS) {
        
        bindlessSupported = false;
        return;
    }

    // Create descriptor pool for bindless resources
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[0].descriptorCount = MAX_BINDLESS_TEXTURES;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[1].descriptorCount = MAX_BINDLESS_SAMPLERS;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;  // Single global bindless set
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

    result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &bindlessDescriptorPool);
    if (result != VK_SUCCESS) {
        
        vkDestroyDescriptorSetLayout(device, bindlessDescriptorSetLayout, nullptr);
        bindlessDescriptorSetLayout = VK_NULL_HANDLE;
        bindlessSupported = false;
        return;
    }

    // Allocate the single global bindless descriptor set with variable count
    // The variable count applies to the last binding (binding 1 = samplers)
    uint32_t variableDescriptorCount = MAX_BINDLESS_SAMPLERS;
    VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo{};
    variableCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
    variableCountInfo.descriptorSetCount = 1;
    variableCountInfo.pDescriptorCounts = &variableDescriptorCount;

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = bindlessDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &bindlessDescriptorSetLayout;
    allocInfo.pNext = &variableCountInfo;

    result = vkAllocateDescriptorSets(device, &allocInfo, &bindlessDescriptorSet);
    if (result != VK_SUCCESS) {
        
        vkDestroyDescriptorPool(device, bindlessDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, bindlessDescriptorSetLayout, nullptr);
        bindlessDescriptorPool = VK_NULL_HANDLE;
        bindlessDescriptorSetLayout = VK_NULL_HANDLE;
        bindlessSupported = false;
        return;
    }

    // Initialize index 0 with the dummy texture so we have a valid fallback
    if (dummyTextureView != VK_NULL_HANDLE) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = dummyTextureView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = bindlessDescriptorSet;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    // Initialize sampler slot 0 with the dummy sampler
    if (dummySampler != VK_NULL_HANDLE) {
        VkDescriptorImageInfo samplerInfo{};
        samplerInfo.sampler = dummySampler;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = bindlessDescriptorSet;
        write.dstBinding = 1;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &samplerInfo;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    // Reserve index 0 for the dummy/default texture
    nextBindlessIndex = 1;
    nextBindlessSamplerIndex = 1;

    bindlessSupported = true;
    
}

void VulkanState::destroyBindlessResources() {
    // Descriptor sets are freed when the pool is destroyed
    if (bindlessDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, bindlessDescriptorPool, nullptr);
        bindlessDescriptorPool = VK_NULL_HANDLE;
    }
    if (bindlessDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, bindlessDescriptorSetLayout, nullptr);
        bindlessDescriptorSetLayout = VK_NULL_HANDLE;
    }
    bindlessDescriptorSet = VK_NULL_HANDLE;
    textureToBindlessIndex.clear();
    freeBindlessIndices.clear();
    samplerToBindlessIndex.clear();
    freeBindlessSamplerIndices.clear();
    nextBindlessIndex = 0;
    nextBindlessSamplerIndex = 0;
    bindlessSupported = false;
}

uint32_t VulkanState::allocateBindlessTextureIndex(uint32_t textureHandleId, VkImageView imageView) {
    if (!bindlessSupported || imageView == VK_NULL_HANDLE) {
        return 0;  // Return dummy texture index
    }

    // Check if already allocated
    auto it = textureToBindlessIndex.find(textureHandleId);
    if (it != textureToBindlessIndex.end()) {
        // Update existing binding
        updateBindlessTexture(it->second, imageView);
        return it->second;
    }

    // Allocate new index
    uint32_t index;
    if (!freeBindlessIndices.empty()) {
        index = freeBindlessIndices.back();
        freeBindlessIndices.pop_back();
    } else {
        if (nextBindlessIndex >= MAX_BINDLESS_TEXTURES) {
            
            return 0;  // Return dummy texture index
        }
        index = nextBindlessIndex++;
    }

    // Update the descriptor
    updateBindlessTexture(index, imageView);

    // Track the mapping
    textureToBindlessIndex[textureHandleId] = index;

    return index;
}

void VulkanState::freeBindlessTextureIndex(uint32_t textureHandleId) {
    auto it = textureToBindlessIndex.find(textureHandleId);
    if (it != textureToBindlessIndex.end()) {
        uint32_t index = it->second;
        // Don't free index 0 (dummy texture)
        if (index > 0) {
            // Update descriptor to point to dummy texture
            updateBindlessTexture(index, dummyTextureView);
            freeBindlessIndices.push_back(index);
        }
        textureToBindlessIndex.erase(it);
    }
}

uint32_t VulkanState::getBindlessTextureIndex(uint32_t textureHandleId) const {
    auto it = textureToBindlessIndex.find(textureHandleId);
    if (it != textureToBindlessIndex.end()) {
        return it->second;
    }
    return 0;  // Return dummy texture index if not found
}

uint32_t VulkanState::allocateBindlessSamplerIndex(uint32_t samplerHandleId, VkSampler sampler) {
    if (!bindlessSupported || sampler == VK_NULL_HANDLE) {
        return 0;  // Return dummy sampler index
    }

    // Check if already allocated
    auto it = samplerToBindlessIndex.find(samplerHandleId);
    if (it != samplerToBindlessIndex.end()) {
        // Update existing binding
        updateBindlessSampler(it->second, sampler);
        return it->second;
    }

    // Allocate new index
    uint32_t index;
    if (!freeBindlessSamplerIndices.empty()) {
        index = freeBindlessSamplerIndices.back();
        freeBindlessSamplerIndices.pop_back();
    } else {
        if (nextBindlessSamplerIndex >= MAX_BINDLESS_SAMPLERS) {
            
            return 0;  // Return dummy sampler index
        }
        index = nextBindlessSamplerIndex++;
    }

    // Update the descriptor
    updateBindlessSampler(index, sampler);

    // Track the mapping
    samplerToBindlessIndex[samplerHandleId] = index;

    return index;
}

void VulkanState::freeBindlessSamplerIndex(uint32_t samplerHandleId) {
    auto it = samplerToBindlessIndex.find(samplerHandleId);
    if (it != samplerToBindlessIndex.end()) {
        uint32_t index = it->second;
        // Don't free index 0 (dummy sampler)
        if (index > 0) {
            updateBindlessSampler(index, dummySampler);
            freeBindlessSamplerIndices.push_back(index);
        }
        samplerToBindlessIndex.erase(it);
    }
}

uint32_t VulkanState::getBindlessSamplerIndex(uint32_t samplerHandleId) const {
    auto it = samplerToBindlessIndex.find(samplerHandleId);
    if (it != samplerToBindlessIndex.end()) {
        return it->second;
    }
    return 0;  // Return dummy sampler index if not found
}

void VulkanState::updateBindlessTexture(uint32_t bindlessIndex, VkImageView imageView) {
    if (!bindlessSupported || bindlessDescriptorSet == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = imageView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = bindlessDescriptorSet;
    write.dstBinding = 0;
    write.dstArrayElement = bindlessIndex;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void VulkanState::updateBindlessSampler(uint32_t bindlessIndex, VkSampler sampler) {
    if (!bindlessSupported || bindlessDescriptorSet == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorImageInfo samplerInfo{};
    samplerInfo.sampler = sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = bindlessDescriptorSet;
    write.dstBinding = 1;
    write.dstArrayElement = bindlessIndex;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &samplerInfo;

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

} // namespace lupine

#endif // LUPINE_HAS_VULKAN

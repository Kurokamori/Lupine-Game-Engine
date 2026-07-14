/**
 * @file lc_asset.h
 * @brief Lupine Engine C API - Asset Loading System
 *
 * This header provides asset loading and management functionality including:
 * - Image loading (PNG, JPG, BMP, TGA, HDR)
 * - Model loading (GLTF, FBX, OBJ) with meshes, materials, skeletons, and animations
 * - Audio loading (WAV, MP3, OGG, FLAC) with streaming and preload support
 * - Font loading (TTF, OTF) with glyph atlas generation
 *
 * Asset handles are reference-counted and must be released with lc_asset_release().
 */

#ifndef LUPINE_CAPI_ASSET_H
#define LUPINE_CAPI_ASSET_H

#include "../core/lc_core.h"
#include "../math/lc_math.h"

/* ============================================================================
 * Asset Handle Types
 * ============================================================================ */

/** Opaque handle to an asset */
typedef struct LCAsset_s* LCAssetHandle;

/** Opaque handle to an image asset */
typedef struct LCImageAsset_s* LCImageAssetHandle;

/** Opaque handle to a model asset */
typedef struct LCModelAsset_s* LCModelAssetHandle;

/** Opaque handle to an audio asset */
typedef struct LCAudioAsset_s* LCAudioAssetHandle;

/** Opaque handle to a font asset */
typedef struct LCFontAsset_s* LCFontAssetHandle;

/* ============================================================================
 * Asset Type Enumeration
 * ============================================================================ */

/**
 * @brief Asset type enumeration
 */
typedef enum LCAssetType {
    LC_ASSET_TYPE_UNKNOWN = 0,
    LC_ASSET_TYPE_IMAGE = 1,
    LC_ASSET_TYPE_MODEL = 2,
    LC_ASSET_TYPE_MESH = 3,
    LC_ASSET_TYPE_MATERIAL = 4,
    LC_ASSET_TYPE_TEXTURE = 5,
    LC_ASSET_TYPE_SKELETON = 6,
    LC_ASSET_TYPE_ANIMATION = 7,
    LC_ASSET_TYPE_SPRITE_ANIMATION = 8,
    LC_ASSET_TYPE_FONT = 9,
    LC_ASSET_TYPE_AUDIO = 10,
    LC_ASSET_TYPE_SHADER = 11
} LCAssetType;

/* ============================================================================
 * Image Asset Types
 * ============================================================================ */

/**
 * @brief Image format enumeration
 */
typedef enum LCImageFormat {
    LC_IMAGE_FORMAT_UNKNOWN = 0,
    LC_IMAGE_FORMAT_R8,
    LC_IMAGE_FORMAT_RG8,
    LC_IMAGE_FORMAT_RGB8,
    LC_IMAGE_FORMAT_RGBA8,
    LC_IMAGE_FORMAT_R16F,
    LC_IMAGE_FORMAT_RG16F,
    LC_IMAGE_FORMAT_RGB16F,
    LC_IMAGE_FORMAT_RGBA16F,
    LC_IMAGE_FORMAT_R32F,
    LC_IMAGE_FORMAT_RG32F,
    LC_IMAGE_FORMAT_RGB32F,
    LC_IMAGE_FORMAT_RGBA32F
} LCImageFormat;

/**
 * @brief Image color space enumeration
 */
typedef enum LCImageColorSpace {
    LC_IMAGE_COLOR_SPACE_LINEAR = 0,
    LC_IMAGE_COLOR_SPACE_SRGB = 1
} LCImageColorSpace;

/**
 * @brief Image information structure
 */
typedef struct LCImageInfo {
    uint32_t width;
    uint32_t height;
    uint32_t channels;
    uint32_t mipLevels;
    LCImageFormat format;
    LCImageColorSpace colorSpace;
    bool hasAlpha;
} LCImageInfo;

/* ============================================================================
 * Model Asset Types
 * ============================================================================ */

/**
 * @brief Vertex structure for mesh data
 */
typedef struct LCVertex {
    LCVec3 position;
    LCVec3 normal;
    LCVec2 texCoord;
    LCVec3 tangent;
    LCVec3 bitangent;
    int boneIDs[4];
    float boneWeights[4];
} LCVertex;

/**
 * @brief Material properties structure
 */
typedef struct LCMaterialInfo {
    char name[256];
    LCVec3 albedo;
    float metallic;
    float roughness;
    float ao;
    LCVec3 emissive;
    float opacity;
    char albedoMap[256];
    char normalMap[256];
    char metallicMap[256];
    char roughnessMap[256];
    char aoMap[256];
    char emissiveMap[256];
} LCMaterialInfo;

/**
 * @brief Mesh information structure
 */
typedef struct LCMeshInfo {
    char name[256];
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t materialIndex;
    LCVec3 boundsMin;
    LCVec3 boundsMax;
} LCMeshInfo;

/**
 * @brief Bone structure for skeletal animation
 */
typedef struct LCBone {
    char name[128];
    int parentIndex;
    LCMat4 offsetMatrix;
    LCMat4 localTransform;
} LCBone;

/**
 * @brief Animation clip information
 */
typedef struct LCAnimationInfo {
    char name[128];
    float duration;
    float ticksPerSecond;
    uint32_t channelCount;
} LCAnimationInfo;

/**
 * @brief Model information structure
 */
typedef struct LCModelInfo {
    uint32_t meshCount;
    uint32_t materialCount;
    uint32_t animationCount;
    uint32_t boneCount;
    bool hasSkeleton;
    bool hasAnimations;
    bool hasEmbeddedTextures;
} LCModelInfo;

/* ============================================================================
 * Audio Asset Types
 * ============================================================================ */

/**
 * @brief Audio format enumeration
 */
typedef enum LCAudioFormat {
    LC_AUDIO_FORMAT_UNKNOWN = 0,
    LC_AUDIO_FORMAT_MONO8,      /**< 8-bit mono */
    LC_AUDIO_FORMAT_MONO16,     /**< 16-bit mono */
    LC_AUDIO_FORMAT_STEREO8,    /**< 8-bit stereo */
    LC_AUDIO_FORMAT_STEREO16    /**< 16-bit stereo */
} LCAudioFormat;

/**
 * @brief Audio loading mode
 */
typedef enum LCAudioLoadMode {
    LC_AUDIO_LOAD_PRELOAD = 0,  /**< Load entire audio file into memory (for sound effects) */
    LC_AUDIO_LOAD_STREAMING = 1 /**< Stream audio from disk (for music/long audio) */
} LCAudioLoadMode;

/**
 * @brief Audio information structure
 */
typedef struct LCAudioInfo {
    uint32_t sampleRate;
    uint32_t channels;
    uint32_t bitsPerSample;
    LCAudioFormat format;
    LCAudioLoadMode loadMode;
    float duration;
    size_t dataSize;
    bool isStreaming;
} LCAudioInfo;

/* ============================================================================
 * Font Asset Types
 * ============================================================================ */

/**
 * @brief Glyph information structure
 */
typedef struct LCGlyph {
    uint32_t codepoint;
    float advance;
    float bearingX;
    float bearingY;
    float width;
    float height;
    float atlasX;       /**< Atlas coordinate (normalized 0-1) */
    float atlasY;
    float atlasWidth;
    float atlasHeight;
    uint32_t pixelX;    /**< Pixel coordinates in atlas */
    uint32_t pixelY;
    uint32_t pixelWidth;
    uint32_t pixelHeight;
} LCGlyph;

/**
 * @brief Font information structure
 */
typedef struct LCFontInfo {
    float fontSize;
    float lineHeight;
    float ascent;
    float descent;
    uint32_t atlasWidth;
    uint32_t atlasHeight;
    uint32_t glyphCount;
} LCFontInfo;

/* ============================================================================
 * Common Asset Functions
 * ============================================================================ */

/**
 * @brief Add a reference to an asset
 * @param asset Asset handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_asset_add_ref(LCAssetHandle asset);

/**
 * @brief Release a reference to an asset
 *
 * When the reference count reaches zero, the asset is destroyed.
 *
 * @param asset Asset handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_asset_release(LCAssetHandle asset);

/**
 * @brief Get the reference count of an asset
 * @param asset Asset handle
 * @param count Output parameter for reference count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_asset_get_ref_count(LCAssetHandle asset, int* count);

/**
 * @brief Get the type of an asset
 * @param asset Asset handle
 * @param type Output parameter for asset type
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_asset_get_type(LCAssetHandle asset, LCAssetType* type);

/**
 * @brief Get the path of an asset
 * @param asset Asset handle
 * @param path Output buffer for path string
 * @param pathSize Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_asset_get_path(LCAssetHandle asset, char* path, size_t pathSize);

/**
 * @brief Get the name of an asset
 * @param asset Asset handle
 * @param name Output buffer for name string
 * @param nameSize Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_asset_get_name(LCAssetHandle asset, char* name, size_t nameSize);

/**
 * @brief Check if an asset is loaded
 * @param asset Asset handle
 * @param loaded Output parameter for loaded state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_asset_is_loaded(LCAssetHandle asset, bool* loaded);

/**
 * @brief Convert asset type to string
 * @param type Asset type
 * @return String representation of asset type (owned by engine, do not free)
 */
LC_API const char* lc_asset_type_to_string(LCAssetType type);

/* ============================================================================
 * Image Asset Functions
 * ============================================================================ */

/**
 * @brief Load an image from file
 * @param filepath Path to the image file
 * @param generateMips Whether to generate mipmaps
 * @param colorSpace Color space for the image
 * @param asset Output parameter for asset handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_image_load(const char* filepath, bool generateMips, LCImageColorSpace colorSpace, LCImageAssetHandle* asset);

/**
 * @brief Load an image from memory (compressed data like PNG, JPG)
 * @param data Pointer to compressed image data
 * @param dataSize Size of data in bytes
 * @param generateMips Whether to generate mipmaps
 * @param colorSpace Color space for the image
 * @param asset Output parameter for asset handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_image_load_from_memory(const uint8_t* data, size_t dataSize, bool generateMips, LCImageColorSpace colorSpace, LCImageAssetHandle* asset);

/**
 * @brief Load an image from raw RGBA data
 * @param data Pointer to raw pixel data
 * @param width Image width in pixels
 * @param height Image height in pixels
 * @param channels Number of channels (1, 2, 3, or 4)
 * @param generateMips Whether to generate mipmaps
 * @param colorSpace Color space for the image
 * @param asset Output parameter for asset handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_image_load_from_raw(const uint8_t* data, uint32_t width, uint32_t height, uint32_t channels, bool generateMips, LCImageColorSpace colorSpace, LCImageAssetHandle* asset);

/**
 * @brief Get image information
 * @param asset Image asset handle
 * @param info Output parameter for image info
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_image_get_info(LCImageAssetHandle asset, LCImageInfo* info);

/**
 * @brief Get image raw data pointer (base mip level)
 * @param asset Image asset handle
 * @param data Output parameter for data pointer (owned by asset, do not free)
 * @param dataSize Output parameter for data size in bytes
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_image_get_data(LCImageAssetHandle asset, const uint8_t** data, size_t* dataSize);

/**
 * @brief Get mip level data
 * @param asset Image asset handle
 * @param level Mip level (0 = base)
 * @param data Output parameter for data pointer
 * @param width Output parameter for mip width
 * @param height Output parameter for mip height
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_image_get_mip_data(LCImageAssetHandle asset, uint32_t level, const uint8_t** data, uint32_t* width, uint32_t* height);

/**
 * @brief Release an image asset
 * @param asset Image asset handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_image_release(LCImageAssetHandle asset);

/* ============================================================================
 * Model Asset Functions
 * ============================================================================ */

/**
 * @brief Load a model from file
 * @param filepath Path to the model file (GLTF, FBX, OBJ)
 * @param optimizeMesh Whether to optimize the mesh
 * @param asset Output parameter for asset handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_model_load(const char* filepath, bool optimizeMesh, LCModelAssetHandle* asset);

/**
 * @brief Get model information
 * @param asset Model asset handle
 * @param info Output parameter for model info
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_model_get_info(LCModelAssetHandle asset, LCModelInfo* info);

/**
 * @brief Get mesh information by index
 * @param asset Model asset handle
 * @param meshIndex Index of the mesh
 * @param info Output parameter for mesh info
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_model_get_mesh_info(LCModelAssetHandle asset, uint32_t meshIndex, LCMeshInfo* info);

/**
 * @brief Get mesh vertex data
 * @param asset Model asset handle
 * @param meshIndex Index of the mesh
 * @param vertices Output buffer for vertices (must be pre-allocated)
 * @param maxVertices Maximum number of vertices to copy
 * @param verticesCopied Output parameter for actual vertices copied
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_model_get_mesh_vertices(LCModelAssetHandle asset, uint32_t meshIndex, LCVertex* vertices, uint32_t maxVertices, uint32_t* verticesCopied);

/**
 * @brief Get mesh index data
 * @param asset Model asset handle
 * @param meshIndex Index of the mesh
 * @param indices Output buffer for indices (must be pre-allocated)
 * @param maxIndices Maximum number of indices to copy
 * @param indicesCopied Output parameter for actual indices copied
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_model_get_mesh_indices(LCModelAssetHandle asset, uint32_t meshIndex, uint32_t* indices, uint32_t maxIndices, uint32_t* indicesCopied);

/**
 * @brief Get material information by index
 * @param asset Model asset handle
 * @param materialIndex Index of the material
 * @param info Output parameter for material info
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_model_get_material_info(LCModelAssetHandle asset, uint32_t materialIndex, LCMaterialInfo* info);

/**
 * @brief Get bone information by index
 * @param asset Model asset handle
 * @param boneIndex Index of the bone
 * @param bone Output parameter for bone info
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_model_get_bone(LCModelAssetHandle asset, uint32_t boneIndex, LCBone* bone);

/**
 * @brief Get animation information by index
 * @param asset Model asset handle
 * @param animIndex Index of the animation
 * @param info Output parameter for animation info
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_model_get_animation_info(LCModelAssetHandle asset, uint32_t animIndex, LCAnimationInfo* info);

/**
 * @brief Release a model asset
 * @param asset Model asset handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_model_release(LCModelAssetHandle asset);

/* ============================================================================
 * Audio Asset Functions
 * ============================================================================ */

/**
 * @brief Load an audio file
 * @param filepath Path to the audio file (WAV, MP3, OGG, FLAC)
 * @param loadMode Loading mode (preload or streaming)
 * @param asset Output parameter for asset handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_audio_asset_load(const char* filepath, LCAudioLoadMode loadMode, LCAudioAssetHandle* asset);

/**
 * @brief Get audio information
 * @param asset Audio asset handle
 * @param info Output parameter for audio info
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_audio_asset_get_info(LCAudioAssetHandle asset, LCAudioInfo* info);

/**
 * @brief Get audio raw data pointer (only for preloaded audio)
 * @param asset Audio asset handle
 * @param data Output parameter for data pointer (owned by asset, do not free)
 * @param dataSize Output parameter for data size in bytes
 * @return LC_SUCCESS on success, LC_ERROR_INVALID_PARAMETER for streaming audio
 */
LC_API LCResult lc_audio_asset_get_data(LCAudioAssetHandle asset, const uint8_t** data, size_t* dataSize);

/**
 * @brief Release an audio asset
 * @param asset Audio asset handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_audio_asset_release(LCAudioAssetHandle asset);

/* ============================================================================
 * Font Asset Functions
 * ============================================================================ */

/**
 * @brief Load a font from file
 * @param filepath Path to the font file (TTF, OTF)
 * @param fontSize Base font size for atlas generation
 * @param atlasWidth Width of glyph atlas in pixels
 * @param atlasHeight Height of glyph atlas in pixels
 * @param asset Output parameter for asset handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_font_load(const char* filepath, float fontSize, uint32_t atlasWidth, uint32_t atlasHeight, LCFontAssetHandle* asset);

/**
 * @brief Get font information
 * @param asset Font asset handle
 * @param info Output parameter for font info
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_font_get_info(LCFontAssetHandle asset, LCFontInfo* info);

/**
 * @brief Get glyph information for a codepoint
 * @param asset Font asset handle
 * @param codepoint Unicode codepoint
 * @param glyph Output parameter for glyph info
 * @return LC_SUCCESS on success, LC_ERROR_NOT_FOUND if glyph not available
 */
LC_API LCResult lc_font_get_glyph(LCFontAssetHandle asset, uint32_t codepoint, LCGlyph* glyph);

/**
 * @brief Check if a glyph is available
 * @param asset Font asset handle
 * @param codepoint Unicode codepoint
 * @param hasGlyph Output parameter for result
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_font_has_glyph(LCFontAssetHandle asset, uint32_t codepoint, bool* hasGlyph);

/**
 * @brief Get font atlas data
 * @param asset Font asset handle
 * @param data Output parameter for atlas data pointer (owned by asset, do not free)
 * @param dataSize Output parameter for atlas data size
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_font_get_atlas_data(LCFontAssetHandle asset, const uint8_t** data, size_t* dataSize);

/**
 * @brief Get kerning between two characters
 * @param asset Font asset handle
 * @param first First codepoint
 * @param second Second codepoint
 * @param kerning Output parameter for kerning value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_font_get_kerning(LCFontAssetHandle asset, uint32_t first, uint32_t second, float* kerning);

/**
 * @brief Measure text dimensions
 * @param asset Font asset handle
 * @param text Text to measure (UTF-8)
 * @param fontSize Font size to measure at
 * @param size Output parameter for text dimensions
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_font_measure_text(LCFontAssetHandle asset, const char* text, float fontSize, LCVec2* size);

/**
 * @brief Release a font asset
 * @param asset Font asset handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_font_release(LCFontAssetHandle asset);

#endif /* LUPINE_CAPI_ASSET_H */

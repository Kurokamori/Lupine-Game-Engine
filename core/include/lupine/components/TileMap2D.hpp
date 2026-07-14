#pragma once

#include "lupine/core/Component.hpp"
#include "lupine/core/ComponentProperty.hpp"
#include "lupine/math/Math.hpp"
#include "lupine/rendering/ResourceHandles.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

namespace lupine {
namespace components {

/**
 * Collision shape types for tileset tiles
 */
enum class TileCollisionType {
    None,
    Rectangle,
    Circle,
    Polygon
};

/**
 * Collision shape data for a tile
 */
struct TileCollisionShape {
    TileCollisionType type = TileCollisionType::None;
    float radius = 0.5f;  // For circle collision (normalized 0-1)
    math::Vec4 rect = math::Vec4(0.0f, 0.0f, 1.0f, 1.0f);  // For rectangle collision (x, y, w, h normalized)
    std::vector<math::Vec2> vertices;  // For polygon collision (normalized 0-1)
};

/**
 * Tile metadata from tileset
 */
struct TileMetadata {
    int index = 0;
    std::vector<std::string> tags;
    std::unordered_map<std::string, std::string> metadata;
    TileCollisionShape collision;
};

/**
 * Tileset data loaded from .tileset2D file
 */
struct TilesetData {
    std::string uuid;
    std::string imagePath;
    int tileWidth = 32;
    int tileHeight = 32;
    std::vector<TileMetadata> tiles;

    // Runtime data
    asset::AssetRef<asset::ImageAsset> imageAsset;
    TextureHandle textureHandle;
    int columns = 0;
    int rows = 0;

    bool isLoaded() const { return imageAsset.IsValid() && imageAsset->IsLoaded(); }
    int getTileCount() const { return columns * rows; }
};

/**
 * Single tile instance in a layer
 */
struct TileInstance {
    int tilesetIndex = -1;
    int tileIndex = -1;
};

/**
 * Tilemap layer data
 */
struct TileMapLayer {
    std::string name;
    bool visible = true;
    float opacity = 1.0f;
    int offsetX = 0;  // Grid offset in tiles
    int offsetY = 0;
    int zIndexOffset = 0;  // Z-index offset for this layer
    std::unordered_map<uint64_t, TileInstance> tiles;  // Key = (y << 32) | x

    static uint64_t MakeKey(int x, int y) {
        return (static_cast<uint64_t>(y) << 32) | static_cast<uint64_t>(static_cast<uint32_t>(x));
    }

    static void SplitKey(uint64_t key, int& x, int& y) {
        x = static_cast<int>(static_cast<uint32_t>(key & 0xFFFFFFFF));
        y = static_cast<int>(key >> 32);
    }
};

/**
 * Cell data returned by GetCellData accessor
 */
struct TileMapCellData {
    bool hasData = false;
    int tilesetIndex = -1;
    int tileIndex = -1;
    std::string layerName;
    TileCollisionType collisionType = TileCollisionType::None;
    std::vector<std::string> tags;
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * TileMap2D Component
 *
 * Renders a 2D tilemap with multiple layers, collision data, and per-layer z-index support.
 *
 * Features:
 * - Load tilemap files (.tilemap format)
 * - Multiple layer support with individual z-index offsets
 * - Per-layer visibility and opacity
 * - Collision shape integration from tilesets
 * - Cell data accessors for game logic
 * - Open in editor functionality
 * - Auto-refresh on file changes
 */
class TileMap2D : public core::Component, public IRenderableComponent {
public:
    TileMap2D();
    explicit TileMap2D(const std::string& name);
    virtual ~TileMap2D();

    // ISerializable interface
    std::string GetTypeName() const override { return "TileMap2D"; }
    void DefineProperties() override;

    // Lifecycle hooks
    void OnAwake() override;
    void OnReady() override;
    void OnUpdate(float deltaTime) override;
    void OnRender() override;
    void OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) override;

    // ===== Tilemap Loading =====

    /**
     * Load tilemap from file path
     */
    bool LoadTileMap(const std::string& filepath);

    /**
     * Reload the current tilemap (for refresh functionality)
     */
    bool ReloadTileMap();

    /**
     * Clear all tilemap data
     */
    void ClearTileMap();

    // ===== Property Accessors =====

    // Tilemap file path
    const std::string& GetTileMapPath() const;
    void SetTileMapPath(const std::string& path);

    // Base Z-index for the tilemap
    int GetBaseZIndex() const;
    void SetBaseZIndex(int zIndex);

    // Whether to show collision shapes in editor
    bool GetShowCollision() const;
    void SetShowCollision(bool show);

    // Global modulate color
    math::Color GetModulate() const;
    void SetModulate(const math::Color& color);

    // ===== Map Information =====

    int GetMapWidth() const { return m_MapWidth; }
    int GetMapHeight() const { return m_MapHeight; }
    int GetTileWidth() const { return m_TileWidth; }
    int GetTileHeight() const { return m_TileHeight; }

    math::Vec2 GetMapSizePixels() const {
        return math::Vec2(
            static_cast<float>(m_MapWidth * m_TileWidth),
            static_cast<float>(m_MapHeight * m_TileHeight)
        );
    }

    // ===== Layer Management =====

    int GetLayerCount() const { return static_cast<int>(m_Layers.size()); }

    const TileMapLayer* GetLayer(int index) const;
    const TileMapLayer* GetLayerByName(const std::string& name) const;

    bool SetLayerVisible(int index, bool visible);
    bool SetLayerVisible(const std::string& name, bool visible);

    bool SetLayerOpacity(int index, float opacity);
    bool SetLayerOpacity(const std::string& name, float opacity);

    bool SetLayerZIndexOffset(int index, int zOffset);
    bool SetLayerZIndexOffset(const std::string& name, int zOffset);

    // ===== Cell Data Accessors =====

    /**
     * Get cell data at position for all layers
     */
    std::vector<TileMapCellData> GetCellDataAllLayers(int cellX, int cellY) const;

    /**
     * Get cell data at position for a specific layer
     */
    TileMapCellData GetCellData(int cellX, int cellY, int layerIndex = 0) const;
    TileMapCellData GetCellData(int cellX, int cellY, const std::string& layerName) const;

    /**
     * Get cell data at world position (converts world coords to cell coords)
     */
    TileMapCellData GetCellDataAtWorldPos(const math::Vec2& worldPos, int layerIndex = 0) const;

    /**
     * Convert world position to cell coordinates
     */
    math::Vec2 WorldToCell(const math::Vec2& worldPos) const;

    /**
     * Convert cell coordinates to world position (center of cell)
     */
    math::Vec2 CellToWorld(int cellX, int cellY) const;

    /**
     * Check if a cell has collision at any layer
     */
    bool HasCollisionAt(int cellX, int cellY) const;

    /**
     * Get collision shape at cell position
     */
    TileCollisionShape GetCollisionShapeAt(int cellX, int cellY, int layerIndex = 0) const;

    // ===== Tileset Access =====

    int GetTilesetCount() const { return static_cast<int>(m_Tilesets.size()); }
    const TilesetData* GetTileset(int index) const;

    // ===== IRenderableComponent Interface =====

    void buildDrawCommands(RenderContext& ctx) override;
    AABB getWorldBounds() const override;
    RenderLayer getRenderLayer() const override;
    SpatialType getSpatialType() const override { return SpatialType::World2D; }

private:
    // Tilemap data
    int m_MapWidth = 32;
    int m_MapHeight = 32;
    int m_TileWidth = 32;
    int m_TileHeight = 32;

    std::vector<TilesetData> m_Tilesets;
    std::vector<TileMapLayer> m_Layers;

    // Cached paths. m_CurrentTileMapPath / m_LastLoadedPath hold the path as authored
    // (normally a res:// resource path); m_ResolvedTileMapPath holds the physical path
    // on disk that filesystem calls must use.
    std::string m_CurrentTileMapPath;
    std::string m_LastLoadedPath;
    std::string m_ResolvedTileMapPath;

    // File watching
    int64_t m_LastFileModTime = 0;
    float m_FileCheckTimer = 0.0f;
    static constexpr float FILE_CHECK_INTERVAL = 1.0f;  // Check every second

    /**
     * Resolve an authored path (res:// resource path, asset UUID or plain filesystem
     * path) to a physical path that std::filesystem and std::ifstream can open.
     */
    static std::string ResolveResourcePath(const std::string& path);

    /**
     * Read a tilemap/tileset file addressed by an authored path, transparently handling
     * both loose files (editor, dev runs) and pack-file mode (exported games).
     * @param path Authored path (res:// resource path or plain filesystem path).
     * @param outContents Receives the file's text on success.
     * @param outResolvedPath Receives the physical path, or the authored path in pack mode.
     * @return True if the file was read.
     */
    static bool ReadResourceFile(const std::string& path, std::string& outContents, std::string& outResolvedPath);

    /**
     * Reload the tilemap if the tileMapPath property no longer matches what is loaded.
     * Cheap to call every frame: it only does work when the path actually changed.
     */
    void SyncTileMapFromPath();

    /**
     * Load a tileset from file
     */
    bool LoadTileset(const std::string& tilesetPath);

    /**
     * Upload tileset texture to GPU
     */
    void UploadTilesetToGPU(TilesetData& tileset, IGfxDevice* device);

    /**
     * Parse tilemap JSON data
     */
    bool ParseTileMapData(const std::string& jsonData);

    /**
     * Parse tileset JSON data
     */
    bool ParseTilesetData(const std::string& jsonData, TilesetData& tileset);

    /**
     * Get tile metadata from tileset
     */
    const TileMetadata* GetTileMetadata(int tilesetIndex, int tileIndex) const;

    /**
     * Check if file has been modified
     */
    bool CheckFileModified() const;

    /**
     * Render collision shapes for debugging
     */
    void RenderCollisionDebug(RenderContext& ctx);
};

} // namespace components
} // namespace lupine

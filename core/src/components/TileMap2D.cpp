#include "lupine/components/TileMap2D.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/gfx/IGfxDevice.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/rendering/gfx/GfxDescriptors.hpp"
#include "lupine/logger/Logger.hpp"
#include "lupine/platform/Platform.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/asset/Asset.hpp"
#include "lupine/asset/AssetDatabase.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <climits>

namespace lupine {
namespace components {

using namespace core;
using namespace math;

namespace {

/**
 * True for paths addressed through a virtual mount (res://, user://, temp://) rather
 * than the host filesystem. Such a path must never be joined onto a directory prefix.
 */
bool IsVirtualResourcePath(const std::string& path) {
    return path.find("://") != std::string::npos;
}

} // namespace

TileMap2D::TileMap2D()
    : Component("TileMap2D")
{
}

TileMap2D::TileMap2D(const std::string& name)
    : Component(name)
{
}

TileMap2D::~TileMap2D() {
}

void TileMap2D::DefineProperties() {
    // Tilemap file path
    DefineProperty(PROPERTY_FILE_GROUP(tileMapPath, std::string(""), "*.tilemap", "TileMap"));

    // Display options
    DefineProperty(PROPERTY_DEFAULT_GROUP(modulate, Color, Color::White(), "Display"));
    DefineProperty(PROPERTY_DEFAULT_GROUP(showCollision, Bool, false, "Display"));

    // Z-Index
    DefineProperty(PROPERTY_INT_RANGE_GROUP(baseZIndex, 0, -1000, 1000, 1, "Rendering"));
}

void TileMap2D::OnAwake() {
    SyncTileMapFromPath();
}

void TileMap2D::OnReady() {
    // Ensure all tilesets are uploaded to GPU
}

void TileMap2D::OnUpdate(float deltaTime) {
    // Check for file modifications (auto-refresh)
    m_FileCheckTimer += deltaTime;
    if (m_FileCheckTimer >= FILE_CHECK_INTERVAL) {
        m_FileCheckTimer = 0.0f;
        if (CheckFileModified()) {
            ReloadTileMap();
        }
    }
}

void TileMap2D::OnRender() {
}

void TileMap2D::OnPropertyChanged(const std::string& propertyName, const nlohmann::json& newValue) {
    if (propertyName == "tileMapPath") {
        (void)newValue;
        SyncTileMapFromPath();
    }
}

std::string TileMap2D::ResolveResourcePath(const std::string& path) {
    // Tilemap and tileset files are loaded by path directly rather than through the asset
    // database, so a raw filesystem read would not understand the res:// mount and would
    // silently fail, leaving the tilemap blank. Asset::ResolveAssetPath is the engine's
    // canonical resolver: it is pack-aware and, crucially, resolves res:// via the
    // AssetDatabase (which knows the project root) rather than the VFS (whose res:// mount
    // points at the engine's own resources directory).
    return asset::Asset::ResolveAssetPath(path);
}

bool TileMap2D::ReadResourceFile(const std::string& path, std::string& outContents, std::string& outResolvedPath) {
    outContents.clear();
    outResolvedPath.clear();

    if (path.empty()) {
        return false;
    }

    // Exported games serve assets out of a pack file, where no physical path exists.
    platform::PackFileSystem& packFS = platform::PackFileSystem::Instance();
    if (packFS.isPackMode() && packFS.exists(path)) {
        outContents = packFS.readFileAsString(path);
        if (outContents.empty()) {
            LOG_ERROR(LogCategory::ECS, "TileMap2D: empty file in pack: {}", path);
            return false;
        }
        outResolvedPath = path;
        return true;
    }

    std::string resolvedPath = ResolveResourcePath(path);
    if (resolvedPath.empty() || !std::filesystem::exists(resolvedPath)) {
        LOG_ERROR(LogCategory::ECS, "TileMap2D: file not found: {} (resolved: {})", path, resolvedPath);
        return false;
    }

    std::ifstream file(resolvedPath);
    if (!file.is_open()) {
        LOG_ERROR(LogCategory::ECS, "TileMap2D: failed to open file: {}", resolvedPath);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    outContents = buffer.str();
    file.close();

    outResolvedPath = resolvedPath;
    return true;
}

void TileMap2D::SyncTileMapFromPath() {
    std::string desiredPath = GetTileMapPath();
    if (desiredPath == m_CurrentTileMapPath) {
        return;
    }

    ClearTileMap();

    // Record the attempted path up front so a path that fails to load is not retried
    // on every single frame from buildDrawCommands().
    m_CurrentTileMapPath = desiredPath;
    m_LastLoadedPath.clear();
    m_ResolvedTileMapPath.clear();

    if (!desiredPath.empty()) {
        LoadTileMap(desiredPath);
    }
}

bool TileMap2D::LoadTileMap(const std::string& filepath) {
    if (filepath.empty()) {
        return false;
    }

    std::string content;
    std::string resolvedPath;
    if (!ReadResourceFile(filepath, content, resolvedPath)) {
        return false;
    }

    // Clear existing data
    ClearTileMap();

    // Parse tilemap data
    if (!ParseTileMapData(content)) {
        LOG_ERROR(LogCategory::ECS, "TileMap2D: failed to parse tilemap file: {}", resolvedPath);
        return false;
    }

    // Get file modification time
    try {
        if (std::filesystem::exists(resolvedPath)) {
            std::filesystem::file_time_type lastWrite = std::filesystem::last_write_time(resolvedPath);
            m_LastFileModTime = lastWrite.time_since_epoch().count();
        } else {
            m_LastFileModTime = 0;
        }
    } catch (...) {
        m_LastFileModTime = 0;
    }

    m_ResolvedTileMapPath = resolvedPath;

    // Canonicalize the stored property (SetTileMapPath converts to res:// when possible),
    // then cache the canonical form so the change check in buildDrawCommands() is stable.
    SetTileMapPath(filepath);
    m_CurrentTileMapPath = GetTileMapPath();
    m_LastLoadedPath = m_CurrentTileMapPath;

    return true;
}

bool TileMap2D::ReloadTileMap() {
    if (m_LastLoadedPath.empty()) {
        return false;
    }

    return LoadTileMap(m_LastLoadedPath);
}

void TileMap2D::ClearTileMap() {
    m_Tilesets.clear();
    m_Layers.clear();
    m_MapWidth = 32;
    m_MapHeight = 32;
    m_TileWidth = 32;
    m_TileHeight = 32;
}

bool TileMap2D::ParseTileMapData(const std::string& jsonData) {
    try {
        nlohmann::json data = nlohmann::json::parse(jsonData);

        m_MapWidth = data.value("width", 32);
        m_MapHeight = data.value("height", 32);
        m_TileWidth = data.value("tile_width", 32);
        m_TileHeight = data.value("tile_height", 32);

        // Load tilesets
        if (data.contains("tilesets") && data["tilesets"].is_array()) {
            for (const auto& tilesetPath : data["tilesets"]) {
                if (tilesetPath.is_string()) {
                    std::string path = tilesetPath.get<std::string>();
                    if (!path.empty()) {
                        LoadTileset(path);
                    }
                }
            }
        }

        // Load layers
        if (data.contains("layers") && data["layers"].is_array()) {
            int layerIndex = 0;
            for (const auto& layerData : data["layers"]) {
                TileMapLayer layer;
                layer.name = layerData.value("name", "Layer " + std::to_string(layerIndex));
                layer.visible = layerData.value("visible", true);
                layer.opacity = layerData.value("opacity", 1.0f);
                layer.offsetX = layerData.value("offset_x", 0);
                layer.offsetY = layerData.value("offset_y", 0);
                layer.zIndexOffset = layerIndex;  // Default z-index based on layer order

                // Load tiles
                if (layerData.contains("tiles") && layerData["tiles"].is_object()) {
                    for (auto& [key, tileData] : layerData["tiles"].items()) {
                        // Parse key "x,y"
                        size_t commaPos = key.find(',');
                        if (commaPos != std::string::npos) {
                            int x = std::stoi(key.substr(0, commaPos));
                            int y = std::stoi(key.substr(commaPos + 1));

                            TileInstance tile;
                            tile.tilesetIndex = tileData.value("tileset_index", -1);
                            tile.tileIndex = tileData.value("tile_index", -1);

                            if (tile.tilesetIndex >= 0 && tile.tileIndex >= 0) {
                                uint64_t tileKey = TileMapLayer::MakeKey(x, y);
                                layer.tiles[tileKey] = tile;
                            }
                        }
                    }
                }

                m_Layers.push_back(std::move(layer));
                layerIndex++;
            }
        }

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::ECS, "TileMap2D: failed to parse tilemap data: {}", e.what());
        return false;
    }
}

bool TileMap2D::LoadTileset(const std::string& tilesetPath) {
    if (tilesetPath.empty()) {
        return false;
    }

    std::string content;
    std::string resolvedTilesetPath;
    if (!ReadResourceFile(tilesetPath, content, resolvedTilesetPath)) {
        LOG_ERROR(LogCategory::ECS, "TileMap2D: failed to read tileset: {}", tilesetPath);
        return false;
    }

    TilesetData tileset;
    if (!ParseTilesetData(content, tileset)) {
        return false;
    }

    if (tileset.imagePath.empty()) {
        LOG_ERROR(LogCategory::ECS, "TileMap2D: tileset has no image path: {}", tilesetPath);
        m_Tilesets.push_back(std::move(tileset));
        return true;
    }

    // ImageAsset::LoadFromFile understands res:// itself, so hand it the authored path
    // and only fall back to a tileset-relative lookup for genuinely relative paths.
    std::string imagePathToLoad = tileset.imagePath;
    std::string resolvedImagePath = ResolveResourcePath(tileset.imagePath);
    bool inPackMode = platform::PackFileSystem::Instance().isPackMode();

    if (!inPackMode && !std::filesystem::exists(resolvedImagePath) &&
        !IsVirtualResourcePath(tileset.imagePath)) {
        std::filesystem::path tilesetDir = std::filesystem::path(resolvedTilesetPath).parent_path();
        std::filesystem::path relativePath = tilesetDir / tileset.imagePath;
        if (std::filesystem::exists(relativePath)) {
            imagePathToLoad = relativePath.string();
        }
    }

    tileset.imageAsset = asset::AssetRef<asset::ImageAsset>(new asset::ImageAsset());
    bool loaded = tileset.imageAsset->LoadFromFile(imagePathToLoad, true, asset::ImageColorSpace::sRGB);
    if (loaded && tileset.imageAsset->IsLoaded()) {
        int imgWidth = tileset.imageAsset->GetWidth();
        int imgHeight = tileset.imageAsset->GetHeight();
        if (tileset.tileWidth > 0 && tileset.tileHeight > 0) {
            tileset.columns = imgWidth / tileset.tileWidth;
            tileset.rows = imgHeight / tileset.tileHeight;
        }
    } else {
        LOG_ERROR(LogCategory::ECS, "TileMap2D: failed to load tileset image: {} (from tileset {})",
                  imagePathToLoad, tilesetPath);
        tileset.imageAsset.Reset();
    }

    m_Tilesets.push_back(std::move(tileset));
    return true;
}

bool TileMap2D::ParseTilesetData(const std::string& jsonData, TilesetData& tileset) {
    try {
        nlohmann::json data = nlohmann::json::parse(jsonData);

        // Handle potentially null string fields
        if (data.contains("uuid") && data["uuid"].is_string()) {
            tileset.uuid = data["uuid"].get<std::string>();
        }
        if (data.contains("tileset_image_path") && data["tileset_image_path"].is_string()) {
            tileset.imagePath = data["tileset_image_path"].get<std::string>();
        }
        tileset.tileWidth = data.value("tile_width", 32);
        tileset.tileHeight = data.value("tile_height", 32);

        // Load tile metadata
        if (data.contains("tiles") && data["tiles"].is_array()) {
            for (const auto& tileData : data["tiles"]) {
                TileMetadata meta;
                meta.index = tileData.value("index", 0);

                // Tags
                if (tileData.contains("tags") && tileData["tags"].is_array()) {
                    for (const auto& tag : tileData["tags"]) {
                        if (tag.is_string()) {
                            meta.tags.push_back(tag.get<std::string>());
                        }
                    }
                }

                // Metadata
                if (tileData.contains("metadata") && tileData["metadata"].is_object()) {
                    for (auto& [key, value] : tileData["metadata"].items()) {
                        if (value.is_string()) {
                            meta.metadata[key] = value.get<std::string>();
                        }
                    }
                }

                // Collision
                std::string collisionType = tileData.value("collision_shape_type", "None");
                if (collisionType == "Rectangle") {
                    meta.collision.type = TileCollisionType::Rectangle;
                    if (tileData.contains("collision_rect") && tileData["collision_rect"].is_array()) {
                        auto& rect = tileData["collision_rect"];
                        if (rect.size() >= 4) {
                            meta.collision.rect = Vec4(
                                rect[0].get<float>(),
                                rect[1].get<float>(),
                                rect[2].get<float>(),
                                rect[3].get<float>()
                            );
                        }
                    }
                } else if (collisionType == "Circle") {
                    meta.collision.type = TileCollisionType::Circle;
                    meta.collision.radius = tileData.value("collision_radius", 0.5f);
                } else if (collisionType == "Polygon") {
                    meta.collision.type = TileCollisionType::Polygon;
                    if (tileData.contains("collision_vertices") && tileData["collision_vertices"].is_array()) {
                        for (const auto& vertex : tileData["collision_vertices"]) {
                            if (vertex.is_array() && vertex.size() >= 2) {
                                meta.collision.vertices.push_back(Vec2(
                                    vertex[0].get<float>(),
                                    vertex[1].get<float>()
                                ));
                            }
                        }
                    }
                }

                // Ensure tiles vector is large enough
                while (tileset.tiles.size() <= static_cast<size_t>(meta.index)) {
                    TileMetadata emptyMeta;
                    emptyMeta.index = static_cast<int>(tileset.tiles.size());
                    tileset.tiles.push_back(emptyMeta);
                }
                tileset.tiles[meta.index] = std::move(meta);
            }
        }

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(LogCategory::ECS, "TileMap2D: failed to parse tileset data: {}", e.what());
        return false;
    }
}

void TileMap2D::UploadTilesetToGPU(TilesetData& tileset, IGfxDevice* device) {
    if (!tileset.imageAsset.IsValid() || !tileset.imageAsset->IsLoaded()) {
        return;
    }

    if (tileset.textureHandle.isValid()) {
        return;  // Already uploaded
    }

    if (tileset.imageAsset->GetWidth() == 0 || tileset.imageAsset->GetHeight() == 0 ||
        tileset.imageAsset->GetData() == nullptr) {
        return;
    }

    tileset.textureHandle = lupine::CreateTexture2DFromImage(device, *tileset.imageAsset, TextureFormat::RGBA8_UNORM);
}

const std::string& TileMap2D::GetTileMapPath() const {
    static std::string cachedPath;
    cachedPath = GetPropertyValue<std::string>("tileMapPath");
    return cachedPath;
}

void TileMap2D::SetTileMapPath(const std::string& path) {
    // Store as a res:// resource path where possible so scenes stay portable across machines.
    std::string resPath = path;
    if (!path.empty() && !IsVirtualResourcePath(path)) {
        asset::AssetDatabase& assetDb = asset::AssetDatabase::GetInstance();
        if (assetDb.IsInitialized()) {
            std::string converted = assetDb.ToResourcePath(path);
            if (!converted.empty()) {
                resPath = converted;
            }
        }
    }
    SetPropertyValue<std::string>("tileMapPath", resPath);
}

int TileMap2D::GetBaseZIndex() const {
    return GetPropertyValue<int>("baseZIndex");
}

void TileMap2D::SetBaseZIndex(int zIndex) {
    SetPropertyValue<int>("baseZIndex", zIndex);
}

bool TileMap2D::GetShowCollision() const {
    return GetPropertyValue<bool>("showCollision");
}

void TileMap2D::SetShowCollision(bool show) {
    SetPropertyValue<bool>("showCollision", show);
}

Color TileMap2D::GetModulate() const {
    const ComponentProperty* prop = m_CustomProperties.GetProperty("modulate");
    if (prop) {
        return prop->GetValue<Color>();
    }
    return Color::White();
}

void TileMap2D::SetModulate(const Color& color) {
    SetPropertyValue<Color>("modulate", color);
}

const TileMapLayer* TileMap2D::GetLayer(int index) const {
    if (index >= 0 && index < static_cast<int>(m_Layers.size())) {
        return &m_Layers[index];
    }
    return nullptr;
}

const TileMapLayer* TileMap2D::GetLayerByName(const std::string& name) const {
    for (const auto& layer : m_Layers) {
        if (layer.name == name) {
            return &layer;
        }
    }
    return nullptr;
}

bool TileMap2D::SetLayerVisible(int index, bool visible) {
    if (index >= 0 && index < static_cast<int>(m_Layers.size())) {
        m_Layers[index].visible = visible;
        return true;
    }
    return false;
}

bool TileMap2D::SetLayerVisible(const std::string& name, bool visible) {
    for (auto& layer : m_Layers) {
        if (layer.name == name) {
            layer.visible = visible;
            return true;
        }
    }
    return false;
}

bool TileMap2D::SetLayerOpacity(int index, float opacity) {
    if (index >= 0 && index < static_cast<int>(m_Layers.size())) {
        m_Layers[index].opacity = std::max(0.0f, std::min(1.0f, opacity));
        return true;
    }
    return false;
}

bool TileMap2D::SetLayerOpacity(const std::string& name, float opacity) {
    for (auto& layer : m_Layers) {
        if (layer.name == name) {
            layer.opacity = std::max(0.0f, std::min(1.0f, opacity));
            return true;
        }
    }
    return false;
}

bool TileMap2D::SetLayerZIndexOffset(int index, int zOffset) {
    if (index >= 0 && index < static_cast<int>(m_Layers.size())) {
        m_Layers[index].zIndexOffset = zOffset;
        return true;
    }
    return false;
}

bool TileMap2D::SetLayerZIndexOffset(const std::string& name, int zOffset) {
    for (auto& layer : m_Layers) {
        if (layer.name == name) {
            layer.zIndexOffset = zOffset;
            return true;
        }
    }
    return false;
}

std::vector<TileMapCellData> TileMap2D::GetCellDataAllLayers(int cellX, int cellY) const {
    std::vector<TileMapCellData> results;
    for (int i = 0; i < static_cast<int>(m_Layers.size()); i++) {
        TileMapCellData data = GetCellData(cellX, cellY, i);
        if (data.hasData) {
            results.push_back(data);
        }
    }
    return results;
}

TileMapCellData TileMap2D::GetCellData(int cellX, int cellY, int layerIndex) const {
    TileMapCellData data;

    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_Layers.size())) {
        return data;
    }

    const TileMapLayer& layer = m_Layers[layerIndex];
    data.layerName = layer.name;

    uint64_t key = TileMapLayer::MakeKey(cellX, cellY);
    auto it = layer.tiles.find(key);
    if (it == layer.tiles.end()) {
        return data;
    }

    const TileInstance& tile = it->second;
    data.hasData = true;
    data.tilesetIndex = tile.tilesetIndex;
    data.tileIndex = tile.tileIndex;

    // Get metadata from tileset
    const TileMetadata* meta = GetTileMetadata(tile.tilesetIndex, tile.tileIndex);
    if (meta) {
        data.collisionType = meta->collision.type;
        data.tags = meta->tags;
        data.metadata = meta->metadata;
    }

    return data;
}

TileMapCellData TileMap2D::GetCellData(int cellX, int cellY, const std::string& layerName) const {
    for (int i = 0; i < static_cast<int>(m_Layers.size()); i++) {
        if (m_Layers[i].name == layerName) {
            return GetCellData(cellX, cellY, i);
        }
    }
    return TileMapCellData();
}

TileMapCellData TileMap2D::GetCellDataAtWorldPos(const Vec2& worldPos, int layerIndex) const {
    Vec2 cellCoords = WorldToCell(worldPos);
    return GetCellData(static_cast<int>(cellCoords.x), static_cast<int>(cellCoords.y), layerIndex);
}

Vec2 TileMap2D::WorldToCell(const Vec2& worldPos) const {
    if (!m_Owner) {
        return Vec2(0, 0);
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return Vec2(0, 0);
    }

    Vec2 nodePos = node2D->GetGlobalPosition();
    Vec2 localPos = worldPos - nodePos;

    // Tilemap uses Y-down (Y=0 at top), engine uses Y-up, so negate Y
    int cellX = static_cast<int>(std::floor(localPos.x / m_TileWidth));
    int cellY = static_cast<int>(std::floor(-localPos.y / m_TileHeight));

    return Vec2(static_cast<float>(cellX), static_cast<float>(cellY));
}

Vec2 TileMap2D::CellToWorld(int cellX, int cellY) const {
    if (!m_Owner) {
        return Vec2(0, 0);
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return Vec2(0, 0);
    }

    Vec2 nodePos = node2D->GetGlobalPosition();

    // Return center of cell
    // Tilemap uses Y-down (Y=0 at top), engine uses Y-up, so negate Y
    float worldX = nodePos.x + (cellX + 0.5f) * m_TileWidth;
    float worldY = nodePos.y - (cellY + 0.5f) * m_TileHeight;

    return Vec2(worldX, worldY);
}

bool TileMap2D::HasCollisionAt(int cellX, int cellY) const {
    for (const auto& layer : m_Layers) {
        if (!layer.visible) continue;

        uint64_t key = TileMapLayer::MakeKey(cellX, cellY);
        auto it = layer.tiles.find(key);
        if (it != layer.tiles.end()) {
            const TileMetadata* meta = GetTileMetadata(it->second.tilesetIndex, it->second.tileIndex);
            if (meta && meta->collision.type != TileCollisionType::None) {
                return true;
            }
        }
    }
    return false;
}

TileCollisionShape TileMap2D::GetCollisionShapeAt(int cellX, int cellY, int layerIndex) const {
    TileCollisionShape shape;

    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_Layers.size())) {
        return shape;
    }

    const TileMapLayer& layer = m_Layers[layerIndex];
    uint64_t key = TileMapLayer::MakeKey(cellX, cellY);
    auto it = layer.tiles.find(key);
    if (it == layer.tiles.end()) {
        return shape;
    }

    const TileMetadata* meta = GetTileMetadata(it->second.tilesetIndex, it->second.tileIndex);
    if (meta) {
        return meta->collision;
    }

    return shape;
}

const TilesetData* TileMap2D::GetTileset(int index) const {
    if (index >= 0 && index < static_cast<int>(m_Tilesets.size())) {
        return &m_Tilesets[index];
    }
    return nullptr;
}

const TileMetadata* TileMap2D::GetTileMetadata(int tilesetIndex, int tileIndex) const {
    if (tilesetIndex < 0 || tilesetIndex >= static_cast<int>(m_Tilesets.size())) {
        return nullptr;
    }

    const TilesetData& tileset = m_Tilesets[tilesetIndex];
    if (tileIndex < 0 || tileIndex >= static_cast<int>(tileset.tiles.size())) {
        return nullptr;
    }

    return &tileset.tiles[tileIndex];
}

bool TileMap2D::CheckFileModified() const {
    // Pack-file assets are immutable, so there is nothing to watch in an exported game.
    if (m_ResolvedTileMapPath.empty() || platform::PackFileSystem::Instance().isPackMode()) {
        return false;
    }

    try {
        if (!std::filesystem::exists(m_ResolvedTileMapPath)) {
            return false;
        }

        std::filesystem::file_time_type lastWrite = std::filesystem::last_write_time(m_ResolvedTileMapPath);
        int64_t currentModTime = lastWrite.time_since_epoch().count();
        return currentModTime != m_LastFileModTime;
    } catch (...) {
        return false;
    }
}

void TileMap2D::buildDrawCommands(RenderContext& ctx) {
    if (!IsEnabled() || !m_Owner) {
        return;
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return;
    }

    // The editor never runs OnAwake(), and a tilemap assigned at runtime has no other
    // load trigger, so pick up a changed tileMapPath here. This is a string compare in
    // the common case and only touches the disk when the path actually changed.
    SyncTileMapFromPath();

    if (m_Layers.empty() || m_Tilesets.empty()) {
        return;
    }

    // Get global transform
    Vec2 nodePos = node2D->GetGlobalPosition();
    Vec2 nodeScale = node2D->GetGlobalScale();
    float nodeRotation = node2D->GetGlobalRotation();

    Color globalModulate = GetModulate();
    int baseZIndex = GetBaseZIndex();

    IGfxDevice* device = ctx.getDevice();

    // Ensure all tilesets are uploaded to GPU
    for (auto& tileset : m_Tilesets) {
        if (!tileset.textureHandle.isValid() && tileset.isLoaded()) {
            UploadTilesetToGPU(tileset, device);
        }
    }

    // Render each layer
    for (size_t layerIdx = 0; layerIdx < m_Layers.size(); layerIdx++) {
        const TileMapLayer& layer = m_Layers[layerIdx];

        if (!layer.visible || layer.opacity <= 0.0f) {
            continue;
        }

        // Calculate effective Z-index for this layer
        int layerZIndex = baseZIndex + layer.zIndexOffset;
        ctx.setZIndex(layerZIndex);

        // Calculate layer opacity combined with modulate alpha
        Color layerColor = globalModulate;
        layerColor.a *= layer.opacity;

        // Render tiles in this layer
        for (const auto& [key, tile] : layer.tiles) {
            if (tile.tilesetIndex < 0 || tile.tilesetIndex >= static_cast<int>(m_Tilesets.size())) {
                continue;
            }

            const TilesetData& tileset = m_Tilesets[tile.tilesetIndex];
            if (!tileset.textureHandle.isValid() || tileset.columns <= 0 || tileset.rows <= 0) {
                continue;
            }

            // Get cell position from key (lower 32 bits = x, upper 32 bits = y)
            int cellX = static_cast<int>(static_cast<uint32_t>(key & 0xFFFFFFFF));
            int cellY = static_cast<int>(key >> 32);

            // Apply layer offset
            int actualX = cellX + layer.offsetX;
            int actualY = cellY + layer.offsetY;

            // Calculate tile size (use map tile dimensions for consistent grid)
            float tileDisplayWidth = static_cast<float>(m_TileWidth) * nodeScale.x;
            float tileDisplayHeight = static_cast<float>(m_TileHeight) * nodeScale.y;

            // Calculate world position (bottom-left of tile, since pivot (0,0) = bottom-left)
            // Tilemap uses Y-down (Y=0 at top), engine uses Y-up
            // Tile (0,0) top-left should be at nodePos, so bottom-left is at nodePos.y - tileHeight
            float worldX = nodePos.x + actualX * m_TileWidth * nodeScale.x;
            float worldY = nodePos.y - (actualY + 1) * m_TileHeight * nodeScale.y;

            // Calculate UV coordinates for this tile (safety check for columns > 0 done above)
            int tileCol = tile.tileIndex % tileset.columns;
            int tileRow = tile.tileIndex / tileset.columns;

            float textureWidth = static_cast<float>(tileset.imageAsset->GetWidth());
            float textureHeight = static_cast<float>(tileset.imageAsset->GetHeight());

            float uvMinX = (tileCol * tileset.tileWidth) / textureWidth;
            float uvMaxX = ((tileCol + 1) * tileset.tileWidth) / textureWidth;
            // Flip Y coordinates: OpenGL has Y=0 at bottom, images have Y=0 at top
            float uvMinY = 1.0f - ((tileRow + 1) * tileset.tileHeight) / textureHeight;
            float uvMaxY = 1.0f - (tileRow * tileset.tileHeight) / textureHeight;

            // Create sprite draw data
            SpriteDrawData sprite;
            sprite.texture = tileset.textureHandle;
            sprite.position = Vec2(worldX, worldY);
            sprite.size = Vec2(tileDisplayWidth, tileDisplayHeight);
            sprite.rotation = nodeRotation;
            sprite.tint = layerColor;
            sprite.uvMin = Vec2(uvMinX, uvMinY);
            sprite.uvMax = Vec2(uvMaxX, uvMaxY);
            sprite.pivot = Vec2(0.0f, 0.0f);  // Top-left pivot for tiles

            ctx.drawSprite(sprite);
        }
    }

    // Render collision debug if enabled
    if (GetShowCollision()) {
        RenderCollisionDebug(ctx);
    }
}

void TileMap2D::RenderCollisionDebug(RenderContext& ctx) {
    if (!m_Owner) return;

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) return;

    Vec2 nodePos = node2D->GetGlobalPosition();
    Vec2 nodeScale = node2D->GetGlobalScale();

    Color collisionColor(1.0f, 0.0f, 0.0f, 0.3f);  // Semi-transparent red

    for (const auto& layer : m_Layers) {
        if (!layer.visible) continue;

        for (const auto& [key, tile] : layer.tiles) {
            const TileMetadata* meta = GetTileMetadata(tile.tilesetIndex, tile.tileIndex);
            if (!meta || meta->collision.type == TileCollisionType::None) {
                continue;
            }

            int cellX, cellY;
            cellX = static_cast<int>(static_cast<uint32_t>(key & 0xFFFFFFFF));
            cellY = static_cast<int>(key >> 32);

            int actualX = cellX + layer.offsetX;
            int actualY = cellY + layer.offsetY;

            float tileWidth = static_cast<float>(m_TileWidth) * nodeScale.x;
            float tileHeight = static_cast<float>(m_TileHeight) * nodeScale.y;

            // Calculate world position (bottom-left corner of tile for collision rendering)
            // Tilemap uses Y-down (Y=0 at top), engine uses Y-up
            float worldX = nodePos.x + actualX * m_TileWidth * nodeScale.x;
            float worldY = nodePos.y - (actualY + 1) * m_TileHeight * nodeScale.y;

            switch (meta->collision.type) {
                case TileCollisionType::Rectangle: {
                    Vec2 rectPos(
                        worldX + meta->collision.rect.x * tileWidth,
                        worldY + meta->collision.rect.y * tileHeight
                    );
                    Vec2 rectSize(
                        meta->collision.rect.z * tileWidth,
                        meta->collision.rect.w * tileHeight
                    );
                    ctx.drawRoundedRect(rectPos, rectSize, 0.0f, collisionColor);
                    break;
                }
                case TileCollisionType::Circle: {
                    float radius = meta->collision.radius * std::min(tileWidth, tileHeight);
                    Vec3 center(worldX + tileWidth * 0.5f, worldY + tileHeight * 0.5f, 0.0f);
                    ctx.drawCircle(center, radius, collisionColor, true);
                    break;
                }
                case TileCollisionType::Polygon: {
                    // Draw polygon as lines (simplified for debug view)
                    if (meta->collision.vertices.size() >= 3) {
                        for (size_t i = 0; i < meta->collision.vertices.size(); i++) {
                            size_t nextI = (i + 1) % meta->collision.vertices.size();
                            Vec3 start(
                                worldX + meta->collision.vertices[i].x * tileWidth,
                                worldY + meta->collision.vertices[i].y * tileHeight,
                                0.0f
                            );
                            Vec3 end(
                                worldX + meta->collision.vertices[nextI].x * tileWidth,
                                worldY + meta->collision.vertices[nextI].y * tileHeight,
                                0.0f
                            );
                            ctx.drawLine(start, end, collisionColor, 2.0f);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
}

AABB TileMap2D::getWorldBounds() const {
    if (!m_Owner) {
        return AABB();
    }

    Node2D* node2D = dynamic_cast<Node2D*>(m_Owner);
    if (!node2D) {
        return AABB();
    }

    // The gather loop culls against these bounds *before* it calls buildDrawCommands(),
    // and an unloaded tilemap has only a placeholder bounds around the node origin. If
    // that placeholder is off-screen the tilemap would be culled and so never get the
    // chance to load, leaving it permanently invisible. Loading is logically a lazy
    // cache fill, so it is safe to do from this const query.
    const_cast<TileMap2D*>(this)->SyncTileMapFromPath();

    Vec2 nodePos = node2D->GetGlobalPosition();
    Vec2 nodeScale = node2D->GetGlobalScale();

    // Calculate actual bounds from placed tiles rather than declared map dimensions
    if (m_Layers.empty()) {
        // No layers, return a small default bounds
        return AABB(
            Vec3(nodePos.x - 16.0f, nodePos.y - 16.0f, -0.1f),
            Vec3(nodePos.x + 16.0f, nodePos.y + 16.0f, 0.1f)
        );
    }

    // Find the actual extent of all placed tiles
    int minCellX = INT_MAX;
    int maxCellX = INT_MIN;
    int minCellY = INT_MAX;
    int maxCellY = INT_MIN;
    bool hasTiles = false;

    for (const auto& layer : m_Layers) {
        for (const auto& [key, tile] : layer.tiles) {
            int cellX, cellY;
            TileMapLayer::SplitKey(key, cellX, cellY);

            // Apply layer offset
            cellX += layer.offsetX;
            cellY += layer.offsetY;

            minCellX = std::min(minCellX, cellX);
            maxCellX = std::max(maxCellX, cellX);
            minCellY = std::min(minCellY, cellY);
            maxCellY = std::max(maxCellY, cellY);
            hasTiles = true;
        }
    }

    if (!hasTiles) {
        // No tiles placed, return a small default bounds
        return AABB(
            Vec3(nodePos.x - 16.0f, nodePos.y - 16.0f, -0.1f),
            Vec3(nodePos.x + 16.0f, nodePos.y + 16.0f, 0.1f)
        );
    }

    // Calculate world bounds from tile extent
    // Tilemap uses Y-down (Y=0 at top), engine uses Y-up
    // minCellX, minCellY is the top-left tile
    // maxCellX, maxCellY is the bottom-right tile
    float worldMinX = nodePos.x + minCellX * m_TileWidth * nodeScale.x;
    float worldMaxX = nodePos.x + (maxCellX + 1) * m_TileWidth * nodeScale.x;

    // Y is inverted: higher cellY means lower world Y
    float worldMaxY = nodePos.y - minCellY * m_TileHeight * nodeScale.y;
    float worldMinY = nodePos.y - (maxCellY + 1) * m_TileHeight * nodeScale.y;

    return AABB(
        Vec3(worldMinX, worldMinY, -0.1f),
        Vec3(worldMaxX, worldMaxY, 0.1f)
    );
}

RenderLayer TileMap2D::getRenderLayer() const {
    // Tilemaps can have transparency, so use transparent layer
    return RenderLayer::Transparent;
}

} // namespace components
} // namespace lupine

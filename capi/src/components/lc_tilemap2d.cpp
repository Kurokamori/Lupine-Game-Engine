/**
 * @file lc_tilemap2d.cpp
 * @brief Implementation of TileMap2D C API
 */

#include "components/lc_tilemap2d.h"
#include "../core/lc_internal.h"

#include <lupine/core/Node.hpp>
#include <lupine/components/TileMap2D.hpp>

#include <cstring>

using namespace lupine;
using namespace lupine::core;
using namespace lupine::components;

namespace {

lupine::math::Color ToEngineColor(LCColor color) {
    return lupine::math::Color(color.r, color.g, color.b, color.a);
}

LCColor FromEngineColor(const lupine::math::Color& color) {
    return LCColor{color.r, color.g, color.b, color.a};
}

lupine::math::Vec2 ToEngineVec2(LCVec2 v) {
    return lupine::math::Vec2(v.x, v.y);
}

LCVec2 FromEngineVec2(const lupine::math::Vec2& v) {
    return LCVec2{v.x, v.y};
}

LCVec4 FromEngineVec4(const lupine::math::Vec4& v) {
    return LCVec4{v.x, v.y, v.z, v.w};
}

LCTileCollisionType FromEngineCollisionType(TileCollisionType type) {
    switch (type) {
        case TileCollisionType::None: return LC_TILE_COLLISION_NONE;
        case TileCollisionType::Rectangle: return LC_TILE_COLLISION_RECTANGLE;
        case TileCollisionType::Circle: return LC_TILE_COLLISION_CIRCLE;
        case TileCollisionType::Polygon: return LC_TILE_COLLISION_POLYGON;
        default: return LC_TILE_COLLISION_NONE;
    }
}

TileMap2D* GetTileMap2D(LCComponentHandle handle) {
    auto comp = GetComponent(handle);
    if (!comp) return nullptr;
    return dynamic_cast<TileMap2D*>(comp.get());
}

void CopyCellData(const TileMapCellData& src, LCTileMapCellData* dest) {
    dest->has_data = src.hasData;
    dest->tileset_index = src.tilesetIndex;
    dest->tile_index = src.tileIndex;
    CopyStringToBuffer(dest->layer_name, sizeof(dest->layer_name), src.layerName.c_str());
    dest->collision_type = FromEngineCollisionType(src.collisionType);

    // Copy tags
    dest->tag_count = static_cast<uint32_t>(src.tags.size());
    dest->tags = nullptr;
    if (dest->tag_count > 0) {
        dest->tags = static_cast<char**>(malloc(sizeof(char*) * dest->tag_count));
        if (dest->tags) {
            for (uint32_t i = 0; i < dest->tag_count; ++i) {
                dest->tags[i] = DuplicateString(src.tags[i]);
            }
        } else {
            dest->tag_count = 0;
        }
    }

    // Copy metadata
    dest->metadata_count = static_cast<uint32_t>(src.metadata.size());
    dest->metadata_keys = nullptr;
    dest->metadata_values = nullptr;
    if (dest->metadata_count > 0) {
        dest->metadata_keys = static_cast<char**>(malloc(sizeof(char*) * dest->metadata_count));
        dest->metadata_values = static_cast<char**>(malloc(sizeof(char*) * dest->metadata_count));
        if (dest->metadata_keys && dest->metadata_values) {
            uint32_t idx = 0;
            for (const auto& [key, value] : src.metadata) {
                dest->metadata_keys[idx] = DuplicateString(key);
                dest->metadata_values[idx] = DuplicateString(value);
                ++idx;
            }
        } else {
            free(dest->metadata_keys);
            free(dest->metadata_values);
            dest->metadata_keys = nullptr;
            dest->metadata_values = nullptr;
            dest->metadata_count = 0;
        }
    }
}

void CopyLayerData(const TileMapLayer& src, LCTileMapLayer* dest) {
    CopyStringToBuffer(dest->name, sizeof(dest->name), src.name.c_str());
    dest->visible = src.visible;
    dest->opacity = src.opacity;
    dest->offset_x = src.offsetX;
    dest->offset_y = src.offsetY;
    dest->z_index_offset = src.zIndexOffset;
}

void CopyCollisionShape(const TileCollisionShape& src, LCTileCollisionShape* dest) {
    dest->type = FromEngineCollisionType(src.type);
    dest->radius = src.radius;
    dest->rect = FromEngineVec4(src.rect);

    dest->vertex_count = static_cast<uint32_t>(src.vertices.size());
    if (dest->vertex_count > 0) {
        dest->vertices = static_cast<LCVec2*>(malloc(sizeof(LCVec2) * dest->vertex_count));
        for (uint32_t i = 0; i < dest->vertex_count; ++i) {
            dest->vertices[i] = FromEngineVec2(src.vertices[i]);
        }
    } else {
        dest->vertices = nullptr;
    }
}

} // anonymous namespace

/* ============================================================================
 * TileMap2D Creation
 * ============================================================================ */

LC_API LCResult lc_tilemap2d_create(const char* name, LCComponentHandle* out_component) {
    if (!out_component) {
        SetError(LC_ERROR_NULL_POINTER, "out_component is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    try {
        auto comp = name ? std::make_shared<TileMap2D>(name) : std::make_shared<TileMap2D>();
        comp->RegisterProperties();
        *out_component = CreateComponentHandle(comp);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Failed to create TileMap2D component");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Tilemap Loading
 * ============================================================================ */

LC_API LCResult lc_tilemap2d_load(LCComponentHandle component, const char* filepath) {
    if (!filepath) {
        SetError(LC_ERROR_NULL_POINTER, "filepath is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid TileMap2D handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        if (!tilemap->LoadTileMap(filepath)) {
            SetError(LC_ERROR_FILE_READ_FAILED, "Failed to load tilemap");
            return LC_ERROR_FILE_READ_FAILED;
        }
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception while loading tilemap");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_tilemap2d_reload(LCComponentHandle component) {
    auto tilemap = GetTileMap2D(component);
    if (!tilemap) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid TileMap2D handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        if (!tilemap->ReloadTileMap()) {
            SetError(LC_ERROR_OPERATION_FAILED, "Failed to reload tilemap");
            return LC_ERROR_OPERATION_FAILED;
        }
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception while reloading tilemap");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_tilemap2d_clear(LCComponentHandle component) {
    auto tilemap = GetTileMap2D(component);
    if (!tilemap) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid TileMap2D handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        tilemap->ClearTileMap();
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception while clearing tilemap");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

/* ============================================================================
 * Property Accessors
 * ============================================================================ */

LC_API LCResult lc_tilemap2d_get_path(LCComponentHandle component, char* out_path, size_t path_size) {
    if (!out_path) {
        SetError(LC_ERROR_NULL_POINTER, "out_path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid TileMap2D handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    const std::string& path = tilemap->GetTileMapPath();
    CopyStringToBuffer(out_path, path_size, path.c_str());
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_set_path(LCComponentHandle component, const char* path) {
    if (!path) {
        SetError(LC_ERROR_NULL_POINTER, "path is NULL");
        return LC_ERROR_NULL_POINTER;
    }

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) {
        SetError(LC_ERROR_INVALID_HANDLE, "Invalid TileMap2D handle");
        return LC_ERROR_INVALID_HANDLE;
    }

    try {
        tilemap->SetTileMapPath(path);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception while setting tilemap path");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_tilemap2d_get_base_z_index(LCComponentHandle component, int* out_z_index) {
    if (!out_z_index) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    *out_z_index = tilemap->GetBaseZIndex();
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_set_base_z_index(LCComponentHandle component, int z_index) {
    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    tilemap->SetBaseZIndex(z_index);
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_show_collision(LCComponentHandle component, bool* out_show) {
    if (!out_show) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    *out_show = tilemap->GetShowCollision();
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_set_show_collision(LCComponentHandle component, bool show) {
    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    tilemap->SetShowCollision(show);
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_modulate(LCComponentHandle component, LCColor* out_color) {
    if (!out_color) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    *out_color = FromEngineColor(tilemap->GetModulate());
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_set_modulate(LCComponentHandle component, LCColor color) {
    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    tilemap->SetModulate(ToEngineColor(color));
    return LC_SUCCESS;
}

/* ============================================================================
 * Map Information
 * ============================================================================ */

LC_API LCResult lc_tilemap2d_get_width(LCComponentHandle component, int* out_width) {
    if (!out_width) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    *out_width = tilemap->GetMapWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_height(LCComponentHandle component, int* out_height) {
    if (!out_height) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    *out_height = tilemap->GetMapHeight();
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_tile_width(LCComponentHandle component, int* out_width) {
    if (!out_width) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    *out_width = tilemap->GetTileWidth();
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_tile_height(LCComponentHandle component, int* out_height) {
    if (!out_height) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    *out_height = tilemap->GetTileHeight();
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_map_size_pixels(LCComponentHandle component, LCVec2* out_size) {
    if (!out_size) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    *out_size = FromEngineVec2(tilemap->GetMapSizePixels());
    return LC_SUCCESS;
}

/* ============================================================================
 * Layer Management
 * ============================================================================ */

LC_API LCResult lc_tilemap2d_get_layer_count(LCComponentHandle component, int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    *out_count = tilemap->GetLayerCount();
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_layer(LCComponentHandle component, int index, LCTileMapLayer* out_layer) {
    if (!out_layer) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    const TileMapLayer* layer = tilemap->GetLayer(index);
    if (!layer) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Invalid layer index");
        return LC_ERROR_INVALID_PARAMETER;
    }

    CopyLayerData(*layer, out_layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_layer_by_name(LCComponentHandle component, const char* name, LCTileMapLayer* out_layer) {
    if (!name || !out_layer) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    const TileMapLayer* layer = tilemap->GetLayerByName(name);
    if (!layer) {
        SetError(LC_ERROR_NOT_FOUND, "Layer not found");
        return LC_ERROR_NOT_FOUND;
    }

    CopyLayerData(*layer, out_layer);
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_set_layer_visible(LCComponentHandle component, int index, bool visible) {
    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    if (!tilemap->SetLayerVisible(index, visible)) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Invalid layer index");
        return LC_ERROR_INVALID_PARAMETER;
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_set_layer_visible_by_name(LCComponentHandle component, const char* name, bool visible) {
    if (!name) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    if (!tilemap->SetLayerVisible(name, visible)) {
        SetError(LC_ERROR_NOT_FOUND, "Layer not found");
        return LC_ERROR_NOT_FOUND;
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_layer_visible(LCComponentHandle component, int index, bool* out_visible) {
    if (!out_visible) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    const TileMapLayer* layer = tilemap->GetLayer(index);
    if (!layer) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Invalid layer index");
        return LC_ERROR_INVALID_PARAMETER;
    }

    *out_visible = layer->visible;
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_layer_visible_by_name(LCComponentHandle component, const char* name, bool* out_visible) {
    if (!name || !out_visible) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    const TileMapLayer* layer = tilemap->GetLayerByName(name);
    if (!layer) {
        SetError(LC_ERROR_NOT_FOUND, "Layer not found");
        return LC_ERROR_NOT_FOUND;
    }

    *out_visible = layer->visible;
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_set_layer_opacity(LCComponentHandle component, int index, float opacity) {
    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    if (!tilemap->SetLayerOpacity(index, opacity)) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Invalid layer index");
        return LC_ERROR_INVALID_PARAMETER;
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_set_layer_opacity_by_name(LCComponentHandle component, const char* name, float opacity) {
    if (!name) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    if (!tilemap->SetLayerOpacity(name, opacity)) {
        SetError(LC_ERROR_NOT_FOUND, "Layer not found");
        return LC_ERROR_NOT_FOUND;
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_layer_opacity(LCComponentHandle component, int index, float* out_opacity) {
    if (!out_opacity) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    const TileMapLayer* layer = tilemap->GetLayer(index);
    if (!layer) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Invalid layer index");
        return LC_ERROR_INVALID_PARAMETER;
    }

    *out_opacity = layer->opacity;
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_layer_opacity_by_name(LCComponentHandle component, const char* name, float* out_opacity) {
    if (!name || !out_opacity) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    const TileMapLayer* layer = tilemap->GetLayerByName(name);
    if (!layer) {
        SetError(LC_ERROR_NOT_FOUND, "Layer not found");
        return LC_ERROR_NOT_FOUND;
    }

    *out_opacity = layer->opacity;
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_set_layer_z_offset(LCComponentHandle component, int index, int z_offset) {
    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    if (!tilemap->SetLayerZIndexOffset(index, z_offset)) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Invalid layer index");
        return LC_ERROR_INVALID_PARAMETER;
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_set_layer_z_offset_by_name(LCComponentHandle component, const char* name, int z_offset) {
    if (!name) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    if (!tilemap->SetLayerZIndexOffset(name, z_offset)) {
        SetError(LC_ERROR_NOT_FOUND, "Layer not found");
        return LC_ERROR_NOT_FOUND;
    }
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_layer_z_offset(LCComponentHandle component, int index, int* out_z_offset) {
    if (!out_z_offset) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    const TileMapLayer* layer = tilemap->GetLayer(index);
    if (!layer) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Invalid layer index");
        return LC_ERROR_INVALID_PARAMETER;
    }

    *out_z_offset = layer->zIndexOffset;
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_layer_z_offset_by_name(LCComponentHandle component, const char* name, int* out_z_offset) {
    if (!name || !out_z_offset) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    const TileMapLayer* layer = tilemap->GetLayerByName(name);
    if (!layer) {
        SetError(LC_ERROR_NOT_FOUND, "Layer not found");
        return LC_ERROR_NOT_FOUND;
    }

    *out_z_offset = layer->zIndexOffset;
    return LC_SUCCESS;
}

/* ============================================================================
 * Cell Data Accessors
 * ============================================================================ */

LC_API LCResult lc_tilemap2d_get_cell_data(
    LCComponentHandle component,
    int cell_x, int cell_y,
    int layer_index,
    LCTileMapCellData* out_cell_data
) {
    if (!out_cell_data) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    try {
        TileMapCellData cellData = tilemap->GetCellData(cell_x, cell_y, layer_index);
        CopyCellData(cellData, out_cell_data);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception while getting cell data");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_tilemap2d_get_cell_data_by_layer_name(
    LCComponentHandle component,
    int cell_x, int cell_y,
    const char* layer_name,
    LCTileMapCellData* out_cell_data
) {
    if (!layer_name || !out_cell_data) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    try {
        TileMapCellData cellData = tilemap->GetCellData(cell_x, cell_y, layer_name);
        CopyCellData(cellData, out_cell_data);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception while getting cell data");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API LCResult lc_tilemap2d_get_cell_data_at_world_pos(
    LCComponentHandle component,
    LCVec2 world_pos,
    int layer_index,
    LCTileMapCellData* out_cell_data
) {
    if (!out_cell_data) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    try {
        TileMapCellData cellData = tilemap->GetCellDataAtWorldPos(ToEngineVec2(world_pos), layer_index);
        CopyCellData(cellData, out_cell_data);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception while getting cell data");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API void lc_tilemap2d_free_cell_data(LCTileMapCellData* cell_data) {
    if (!cell_data) return;

    // Free tags
    if (cell_data->tags) {
        for (uint32_t i = 0; i < cell_data->tag_count; ++i) {
            free(cell_data->tags[i]);
        }
        free(cell_data->tags);
        cell_data->tags = nullptr;
    }

    // Free metadata
    if (cell_data->metadata_keys) {
        for (uint32_t i = 0; i < cell_data->metadata_count; ++i) {
            free(cell_data->metadata_keys[i]);
        }
        free(cell_data->metadata_keys);
        cell_data->metadata_keys = nullptr;
    }

    if (cell_data->metadata_values) {
        for (uint32_t i = 0; i < cell_data->metadata_count; ++i) {
            free(cell_data->metadata_values[i]);
        }
        free(cell_data->metadata_values);
        cell_data->metadata_values = nullptr;
    }

    cell_data->tag_count = 0;
    cell_data->metadata_count = 0;
}

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

LC_API LCResult lc_tilemap2d_world_to_cell(
    LCComponentHandle component,
    LCVec2 world_pos,
    LCVec2* out_cell_coords
) {
    if (!out_cell_coords) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    *out_cell_coords = FromEngineVec2(tilemap->WorldToCell(ToEngineVec2(world_pos)));
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_cell_to_world(
    LCComponentHandle component,
    int cell_x, int cell_y,
    LCVec2* out_world_pos
) {
    if (!out_world_pos) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    *out_world_pos = FromEngineVec2(tilemap->CellToWorld(cell_x, cell_y));
    return LC_SUCCESS;
}

/* ============================================================================
 * Collision Queries
 * ============================================================================ */

LC_API LCResult lc_tilemap2d_has_collision_at(
    LCComponentHandle component,
    int cell_x, int cell_y,
    bool* out_has_collision
) {
    if (!out_has_collision) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    *out_has_collision = tilemap->HasCollisionAt(cell_x, cell_y);
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_collision_shape_at(
    LCComponentHandle component,
    int cell_x, int cell_y,
    int layer_index,
    LCTileCollisionShape* out_shape
) {
    if (!out_shape) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    try {
        TileCollisionShape shape = tilemap->GetCollisionShapeAt(cell_x, cell_y, layer_index);
        CopyCollisionShape(shape, out_shape);
        return LC_SUCCESS;
    } catch (...) {
        SetError(LC_ERROR_INTERNAL_ERROR, "Exception while getting collision shape");
        return LC_ERROR_INTERNAL_ERROR;
    }
}

LC_API void lc_tilemap2d_free_collision_shape(LCTileCollisionShape* shape) {
    if (!shape) return;

    if (shape->vertices) {
        free(shape->vertices);
        shape->vertices = nullptr;
    }
    shape->vertex_count = 0;
}

/* ============================================================================
 * Tileset Access
 * ============================================================================ */

LC_API LCResult lc_tilemap2d_get_tileset_count(LCComponentHandle component, int* out_count) {
    if (!out_count) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    *out_count = tilemap->GetTilesetCount();
    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_tileset(
    LCComponentHandle component,
    int index,
    LCTilesetData* out_tileset
) {
    if (!out_tileset) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    const TilesetData* tileset = tilemap->GetTileset(index);
    if (!tileset) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Invalid tileset index");
        return LC_ERROR_INVALID_PARAMETER;
    }

    CopyStringToBuffer(out_tileset->uuid, sizeof(out_tileset->uuid), tileset->uuid.c_str());
    CopyStringToBuffer(out_tileset->image_path, sizeof(out_tileset->image_path), tileset->imagePath.c_str());
    out_tileset->tile_width = tileset->tileWidth;
    out_tileset->tile_height = tileset->tileHeight;
    out_tileset->columns = tileset->columns;
    out_tileset->rows = tileset->rows;
    out_tileset->tile_count = tileset->getTileCount();
    out_tileset->is_loaded = tileset->isLoaded();

    return LC_SUCCESS;
}

LC_API LCResult lc_tilemap2d_get_tile_metadata(
    LCComponentHandle component,
    int tileset_index,
    int tile_index,
    LCTileMetadata* out_metadata
) {
    if (!out_metadata) return LC_ERROR_NULL_POINTER;

    auto tilemap = GetTileMap2D(component);
    if (!tilemap) return LC_ERROR_INVALID_HANDLE;

    const TilesetData* tileset = tilemap->GetTileset(tileset_index);
    if (!tileset) {
        SetError(LC_ERROR_INVALID_PARAMETER, "Invalid tileset index");
        return LC_ERROR_INVALID_PARAMETER;
    }

    if (tile_index < 0 || tile_index >= static_cast<int>(tileset->tiles.size())) {
        // Return empty metadata for tiles without explicit metadata
        out_metadata->index = tile_index;
        out_metadata->tags = nullptr;
        out_metadata->tag_count = 0;
        out_metadata->metadata_keys = nullptr;
        out_metadata->metadata_values = nullptr;
        out_metadata->metadata_count = 0;
        out_metadata->collision.type = LC_TILE_COLLISION_NONE;
        out_metadata->collision.radius = 0.5f;
        out_metadata->collision.rect = LCVec4{0.0f, 0.0f, 1.0f, 1.0f};
        out_metadata->collision.vertices = nullptr;
        out_metadata->collision.vertex_count = 0;
        return LC_SUCCESS;
    }

    const TileMetadata& meta = tileset->tiles[tile_index];
    out_metadata->index = meta.index;

    // Copy tags
    out_metadata->tag_count = static_cast<uint32_t>(meta.tags.size());
    out_metadata->tags = nullptr;
    if (out_metadata->tag_count > 0) {
        out_metadata->tags = static_cast<char**>(malloc(sizeof(char*) * out_metadata->tag_count));
        if (out_metadata->tags) {
            for (uint32_t i = 0; i < out_metadata->tag_count; ++i) {
                out_metadata->tags[i] = DuplicateString(meta.tags[i]);
            }
        } else {
            out_metadata->tag_count = 0;
        }
    }

    // Copy metadata
    out_metadata->metadata_count = static_cast<uint32_t>(meta.metadata.size());
    out_metadata->metadata_keys = nullptr;
    out_metadata->metadata_values = nullptr;
    if (out_metadata->metadata_count > 0) {
        out_metadata->metadata_keys = static_cast<char**>(malloc(sizeof(char*) * out_metadata->metadata_count));
        out_metadata->metadata_values = static_cast<char**>(malloc(sizeof(char*) * out_metadata->metadata_count));
        if (out_metadata->metadata_keys && out_metadata->metadata_values) {
            uint32_t idx = 0;
            for (const auto& [key, value] : meta.metadata) {
                out_metadata->metadata_keys[idx] = DuplicateString(key);
                out_metadata->metadata_values[idx] = DuplicateString(value);
                ++idx;
            }
        } else {
            free(out_metadata->metadata_keys);
            free(out_metadata->metadata_values);
            out_metadata->metadata_keys = nullptr;
            out_metadata->metadata_values = nullptr;
            out_metadata->metadata_count = 0;
        }
    }

    // Copy collision shape
    CopyCollisionShape(meta.collision, &out_metadata->collision);

    return LC_SUCCESS;
}

LC_API void lc_tilemap2d_free_metadata(LCTileMetadata* metadata) {
    if (!metadata) return;

    // Free tags
    if (metadata->tags) {
        for (uint32_t i = 0; i < metadata->tag_count; ++i) {
            free(metadata->tags[i]);
        }
        free(metadata->tags);
        metadata->tags = nullptr;
    }

    // Free metadata
    if (metadata->metadata_keys) {
        for (uint32_t i = 0; i < metadata->metadata_count; ++i) {
            free(metadata->metadata_keys[i]);
        }
        free(metadata->metadata_keys);
        metadata->metadata_keys = nullptr;
    }

    if (metadata->metadata_values) {
        for (uint32_t i = 0; i < metadata->metadata_count; ++i) {
            free(metadata->metadata_values[i]);
        }
        free(metadata->metadata_values);
        metadata->metadata_values = nullptr;
    }

    // Free collision vertices
    lc_tilemap2d_free_collision_shape(&metadata->collision);

    metadata->tag_count = 0;
    metadata->metadata_count = 0;
}

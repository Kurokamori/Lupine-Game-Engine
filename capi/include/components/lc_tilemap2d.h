/**
 * @file lc_tilemap2d.h
 * @brief Lupine Engine C API - TileMap2D component
 *
 * This header provides 2D tilemap rendering with multiple layers,
 * collision data, and per-layer z-index support.
 */

#ifndef LUPINE_CAPI_TILEMAP2D_H
#define LUPINE_CAPI_TILEMAP2D_H

#include "core/lc_core.h"
#include "core/lc_node.h"
#include "math/lc_math.h"

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Tile collision shape types
 */
typedef enum LCTileCollisionType {
    LC_TILE_COLLISION_NONE = 0,      /**< No collision */
    LC_TILE_COLLISION_RECTANGLE = 1, /**< Rectangle collision */
    LC_TILE_COLLISION_CIRCLE = 2,    /**< Circle collision */
    LC_TILE_COLLISION_POLYGON = 3    /**< Polygon collision */
} LCTileCollisionType;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Collision shape data for a tile
 * All coordinates are normalized (0-1) relative to tile size
 */
typedef struct LCTileCollisionShape {
    LCTileCollisionType type;    /**< Type of collision shape */
    float radius;                 /**< For circle collision (normalized 0-1) */
    LCVec4 rect;                  /**< For rectangle collision (x, y, w, h normalized) */
    LCVec2* vertices;             /**< For polygon collision (normalized 0-1), caller must free with lc_free() */
    uint32_t vertex_count;        /**< Number of vertices for polygon */
} LCTileCollisionShape;

/**
 * @brief Tile metadata from tileset
 */
typedef struct LCTileMetadata {
    int index;                    /**< Tile index in tileset */
    char** tags;                  /**< Array of tag strings, caller must free with lc_tilemap2d_free_metadata() */
    uint32_t tag_count;           /**< Number of tags */
    char** metadata_keys;         /**< Metadata keys, caller must free with lc_tilemap2d_free_metadata() */
    char** metadata_values;       /**< Metadata values */
    uint32_t metadata_count;      /**< Number of metadata entries */
    LCTileCollisionShape collision; /**< Collision shape */
} LCTileMetadata;

/**
 * @brief Tileset data loaded from .tileset2D file
 */
typedef struct LCTilesetData {
    char uuid[256];               /**< Unique identifier */
    char image_path[512];         /**< Path to tileset image */
    int tile_width;               /**< Width of each tile in pixels */
    int tile_height;              /**< Height of each tile in pixels */
    int columns;                  /**< Grid columns (calculated from image width) */
    int rows;                     /**< Grid rows (calculated from image height) */
    int tile_count;               /**< Total tile count (columns * rows) */
    bool is_loaded;               /**< Whether tileset texture is loaded */
} LCTilesetData;

/**
 * @brief Tilemap layer data
 */
typedef struct LCTileMapLayer {
    char name[256];               /**< Layer name */
    bool visible;                 /**< Visibility flag */
    float opacity;                /**< Opacity 0.0-1.0 */
    int offset_x;                 /**< Grid offset in tiles (X) */
    int offset_y;                 /**< Grid offset in tiles (Y) */
    int z_index_offset;           /**< Z-index offset for rendering */
} LCTileMapLayer;

/**
 * @brief Cell data returned by accessor functions
 */
typedef struct LCTileMapCellData {
    bool has_data;                /**< Whether cell has tile data */
    int tileset_index;            /**< Which tileset */
    int tile_index;               /**< Tile index in tileset */
    char layer_name[256];         /**< Layer name */
    LCTileCollisionType collision_type; /**< Collision type at this cell */
    char** tags;                  /**< Tile tags, caller must free with lc_tilemap2d_free_cell_data() */
    uint32_t tag_count;           /**< Number of tags */
    char** metadata_keys;         /**< Metadata keys */
    char** metadata_values;       /**< Metadata values */
    uint32_t metadata_count;      /**< Number of metadata entries */
} LCTileMapCellData;

/* ============================================================================
 * TileMap2D Creation
 * ============================================================================ */

/**
 * @brief Create a TileMap2D component
 * @param name Optional name for the component (can be NULL)
 * @param out_component Output parameter for the created component handle
 * @return LC_SUCCESS on success, error code otherwise
 * @threadsafety Thread-safe
 */
LC_API LCResult lc_tilemap2d_create(const char* name, LCComponentHandle* out_component);

/* ============================================================================
 * Tilemap Loading
 * ============================================================================ */

/**
 * @brief Load tilemap from file path (.tilemap format)
 * @param component Component handle
 * @param filepath Path to the tilemap file
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_load(LCComponentHandle component, const char* filepath);

/**
 * @brief Reload the current tilemap
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_reload(LCComponentHandle component);

/**
 * @brief Clear all tilemap data
 * @param component Component handle
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_clear(LCComponentHandle component);

/* ============================================================================
 * Property Accessors
 * ============================================================================ */

/**
 * @brief Get the tilemap file path
 * @param component Component handle
 * @param out_path Output buffer for path
 * @param path_size Size of output buffer
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_path(LCComponentHandle component, char* out_path, size_t path_size);

/**
 * @brief Set the tilemap file path (triggers load)
 * @param component Component handle
 * @param path Path to tilemap file
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_set_path(LCComponentHandle component, const char* path);

/**
 * @brief Get the base Z-index
 * @param component Component handle
 * @param out_z_index Output parameter for z-index
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_base_z_index(LCComponentHandle component, int* out_z_index);

/**
 * @brief Set the base Z-index
 * @param component Component handle
 * @param z_index Base z-index value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_set_base_z_index(LCComponentHandle component, int z_index);

/**
 * @brief Get whether collision shapes are shown
 * @param component Component handle
 * @param out_show Output parameter for show state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_show_collision(LCComponentHandle component, bool* out_show);

/**
 * @brief Set whether to show collision shapes (debug visualization)
 * @param component Component handle
 * @param show Show collision shapes
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_set_show_collision(LCComponentHandle component, bool show);

/**
 * @brief Get the global modulate color
 * @param component Component handle
 * @param out_color Output parameter for color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_modulate(LCComponentHandle component, LCColor* out_color);

/**
 * @brief Set the global modulate color
 * @param component Component handle
 * @param color Modulate color
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_set_modulate(LCComponentHandle component, LCColor color);

/* ============================================================================
 * Map Information (Read-Only)
 * ============================================================================ */

/**
 * @brief Get map width in tiles
 * @param component Component handle
 * @param out_width Output parameter for width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_width(LCComponentHandle component, int* out_width);

/**
 * @brief Get map height in tiles
 * @param component Component handle
 * @param out_height Output parameter for height
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_height(LCComponentHandle component, int* out_height);

/**
 * @brief Get tile width in pixels
 * @param component Component handle
 * @param out_width Output parameter for width
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_tile_width(LCComponentHandle component, int* out_width);

/**
 * @brief Get tile height in pixels
 * @param component Component handle
 * @param out_height Output parameter for height
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_tile_height(LCComponentHandle component, int* out_height);

/**
 * @brief Get map size in pixels
 * @param component Component handle
 * @param out_size Output parameter for size (width, height)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_map_size_pixels(LCComponentHandle component, LCVec2* out_size);

/* ============================================================================
 * Layer Management
 * ============================================================================ */

/**
 * @brief Get number of layers
 * @param component Component handle
 * @param out_count Output parameter for layer count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_layer_count(LCComponentHandle component, int* out_count);

/**
 * @brief Get layer by index
 * @param component Component handle
 * @param index Layer index
 * @param out_layer Output parameter for layer data
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_layer(LCComponentHandle component, int index, LCTileMapLayer* out_layer);

/**
 * @brief Get layer by name
 * @param component Component handle
 * @param name Layer name
 * @param out_layer Output parameter for layer data
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_layer_by_name(LCComponentHandle component, const char* name, LCTileMapLayer* out_layer);

/**
 * @brief Set layer visibility by index
 * @param component Component handle
 * @param index Layer index
 * @param visible Visibility state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_set_layer_visible(LCComponentHandle component, int index, bool visible);

/**
 * @brief Set layer visibility by name
 * @param component Component handle
 * @param name Layer name
 * @param visible Visibility state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_set_layer_visible_by_name(LCComponentHandle component, const char* name, bool visible);

/**
 * @brief Get layer visibility by index
 * @param component Component handle
 * @param index Layer index
 * @param out_visible Output parameter for visibility
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_layer_visible(LCComponentHandle component, int index, bool* out_visible);

/**
 * @brief Get layer visibility by name
 * @param component Component handle
 * @param name Layer name
 * @param out_visible Output parameter for visibility
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_layer_visible_by_name(LCComponentHandle component, const char* name, bool* out_visible);

/**
 * @brief Set layer opacity by index
 * @param component Component handle
 * @param index Layer index
 * @param opacity Opacity value (0.0-1.0)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_set_layer_opacity(LCComponentHandle component, int index, float opacity);

/**
 * @brief Set layer opacity by name
 * @param component Component handle
 * @param name Layer name
 * @param opacity Opacity value (0.0-1.0)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_set_layer_opacity_by_name(LCComponentHandle component, const char* name, float opacity);

/**
 * @brief Get layer opacity by index
 * @param component Component handle
 * @param index Layer index
 * @param out_opacity Output parameter for opacity
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_layer_opacity(LCComponentHandle component, int index, float* out_opacity);

/**
 * @brief Get layer opacity by name
 * @param component Component handle
 * @param name Layer name
 * @param out_opacity Output parameter for opacity
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_layer_opacity_by_name(LCComponentHandle component, const char* name, float* out_opacity);

/**
 * @brief Set layer z-index offset by index
 * @param component Component handle
 * @param index Layer index
 * @param z_offset Z-index offset value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_set_layer_z_offset(LCComponentHandle component, int index, int z_offset);

/**
 * @brief Set layer z-index offset by name
 * @param component Component handle
 * @param name Layer name
 * @param z_offset Z-index offset value
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_set_layer_z_offset_by_name(LCComponentHandle component, const char* name, int z_offset);

/**
 * @brief Get layer z-index offset by index
 * @param component Component handle
 * @param index Layer index
 * @param out_z_offset Output parameter for z-offset
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_layer_z_offset(LCComponentHandle component, int index, int* out_z_offset);

/**
 * @brief Get layer z-index offset by name
 * @param component Component handle
 * @param name Layer name
 * @param out_z_offset Output parameter for z-offset
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_layer_z_offset_by_name(LCComponentHandle component, const char* name, int* out_z_offset);

/* ============================================================================
 * Cell Data Accessors
 * ============================================================================ */

/**
 * @brief Get cell data at position for a specific layer
 * @param component Component handle
 * @param cell_x Cell X coordinate
 * @param cell_y Cell Y coordinate
 * @param layer_index Layer index (0 for first layer)
 * @param out_cell_data Output parameter for cell data
 * @return LC_SUCCESS on success, error code otherwise
 * @note Call lc_tilemap2d_free_cell_data() to free allocated strings in cell data
 */
LC_API LCResult lc_tilemap2d_get_cell_data(
    LCComponentHandle component,
    int cell_x, int cell_y,
    int layer_index,
    LCTileMapCellData* out_cell_data
);

/**
 * @brief Get cell data at position for a specific layer by name
 * @param component Component handle
 * @param cell_x Cell X coordinate
 * @param cell_y Cell Y coordinate
 * @param layer_name Layer name
 * @param out_cell_data Output parameter for cell data
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_cell_data_by_layer_name(
    LCComponentHandle component,
    int cell_x, int cell_y,
    const char* layer_name,
    LCTileMapCellData* out_cell_data
);

/**
 * @brief Get cell data at world position
 * @param component Component handle
 * @param world_pos World position
 * @param layer_index Layer index
 * @param out_cell_data Output parameter for cell data
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_cell_data_at_world_pos(
    LCComponentHandle component,
    LCVec2 world_pos,
    int layer_index,
    LCTileMapCellData* out_cell_data
);

/**
 * @brief Free memory allocated in cell data structure
 * @param cell_data Cell data to free
 */
LC_API void lc_tilemap2d_free_cell_data(LCTileMapCellData* cell_data);

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

/**
 * @brief Convert world position to cell coordinates
 * @param component Component handle
 * @param world_pos World position
 * @param out_cell_coords Output parameter for cell coordinates (x, y)
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_world_to_cell(
    LCComponentHandle component,
    LCVec2 world_pos,
    LCVec2* out_cell_coords
);

/**
 * @brief Convert cell coordinates to world position (center of cell)
 * @param component Component handle
 * @param cell_x Cell X coordinate
 * @param cell_y Cell Y coordinate
 * @param out_world_pos Output parameter for world position
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_cell_to_world(
    LCComponentHandle component,
    int cell_x, int cell_y,
    LCVec2* out_world_pos
);

/* ============================================================================
 * Collision Queries
 * ============================================================================ */

/**
 * @brief Check if a cell has collision at any layer
 * @param component Component handle
 * @param cell_x Cell X coordinate
 * @param cell_y Cell Y coordinate
 * @param out_has_collision Output parameter for collision state
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_has_collision_at(
    LCComponentHandle component,
    int cell_x, int cell_y,
    bool* out_has_collision
);

/**
 * @brief Get collision shape at specific cell
 * @param component Component handle
 * @param cell_x Cell X coordinate
 * @param cell_y Cell Y coordinate
 * @param layer_index Layer index
 * @param out_shape Output parameter for collision shape
 * @return LC_SUCCESS on success, error code otherwise
 * @note Caller must free vertices array with lc_free() if vertex_count > 0
 */
LC_API LCResult lc_tilemap2d_get_collision_shape_at(
    LCComponentHandle component,
    int cell_x, int cell_y,
    int layer_index,
    LCTileCollisionShape* out_shape
);

/**
 * @brief Free collision shape vertices
 * @param shape Collision shape to free
 */
LC_API void lc_tilemap2d_free_collision_shape(LCTileCollisionShape* shape);

/* ============================================================================
 * Tileset Access
 * ============================================================================ */

/**
 * @brief Get number of tilesets
 * @param component Component handle
 * @param out_count Output parameter for tileset count
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_tileset_count(LCComponentHandle component, int* out_count);

/**
 * @brief Get tileset by index
 * @param component Component handle
 * @param index Tileset index
 * @param out_tileset Output parameter for tileset data
 * @return LC_SUCCESS on success, error code otherwise
 */
LC_API LCResult lc_tilemap2d_get_tileset(
    LCComponentHandle component,
    int index,
    LCTilesetData* out_tileset
);

/**
 * @brief Get tile metadata from tileset
 * @param component Component handle
 * @param tileset_index Tileset index
 * @param tile_index Tile index within tileset
 * @param out_metadata Output parameter for tile metadata
 * @return LC_SUCCESS on success, error code otherwise
 * @note Call lc_tilemap2d_free_metadata() to free allocated memory
 */
LC_API LCResult lc_tilemap2d_get_tile_metadata(
    LCComponentHandle component,
    int tileset_index,
    int tile_index,
    LCTileMetadata* out_metadata
);

/**
 * @brief Free tile metadata memory
 * @param metadata Metadata to free
 */
LC_API void lc_tilemap2d_free_metadata(LCTileMetadata* metadata);

#endif /* LUPINE_CAPI_TILEMAP2D_H */

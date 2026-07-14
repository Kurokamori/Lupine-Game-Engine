#pragma once

#include "lupine/math/Math.hpp"
#include "lupine/rendering/Rendering.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include <map>
#include <vector>
#include <string>
#include <tuple>

namespace lupine {
namespace engine {

using namespace math;

/**
 * Represents a single textured quad face in 3D space
 */
struct TileFace25D {
    std::string id;

    // Four vertices defining the quad (counter-clockwise)
    Vec3 vertices[4];

    // UV coordinates for each vertex
    Vec2 uvs[4];

    // Tileset and tile reference
    int32_t tilesetIndex;
    int32_t tileIndex;

    // Visual properties
    bool visible;
    bool twoSided;

    // Color tint (RGBA)
    Color color;

    TileFace25D()
        : tilesetIndex(0)
        , tileIndex(0)
        , visible(true)
        , twoSided(true)
        , color(Color::White())
    {
        vertices[0] = Vec3(0, 0, 0);
        vertices[1] = Vec3(1, 0, 0);
        vertices[2] = Vec3(1, 0, 1);
        vertices[3] = Vec3(0, 0, 1);

        uvs[0] = Vec2(0, 0);
        uvs[1] = Vec2(1, 0);
        uvs[2] = Vec2(1, 1);
        uvs[3] = Vec2(0, 1);
    }

    Vec3 GetCenter() const {
        return Vec3(
            (vertices[0].x + vertices[1].x + vertices[2].x + vertices[3].x) / 4.0f,
            (vertices[0].y + vertices[1].y + vertices[2].y + vertices[3].y) / 4.0f,
            (vertices[0].z + vertices[1].z + vertices[2].z + vertices[3].z) / 4.0f
        );
    }

    Vec3 GetNormal() const {
        Vec3 edge1 = vertices[1] - vertices[0];
        Vec3 edge2 = vertices[3] - vertices[0];
        return edge1.Cross(edge2).Normalized();
    }
};

/**
 * TileMap25DBuilder - Creates and manages 3D tile maps using 2D tilesets
 * Similar to VoxelBuilder but for textured quad faces
 */
class TileMap25DBuilder {
public:
    TileMap25DBuilder();
    ~TileMap25DBuilder();

    // Face management
    std::string AddFace(const TileFace25D& face);
    void RemoveFace(const std::string& faceId);
    TileFace25D* GetFace(const std::string& faceId);
    const TileFace25D* GetFace(const std::string& faceId) const;
    bool HasFace(const std::string& faceId) const;
    void ClearFaces();
    size_t GetFaceCount() const { return m_Faces.size(); }

    // Vertex manipulation
    void SetFaceVertex(const std::string& faceId, int vertexIndex, const Vec3& position);
    void SetFaceUV(const std::string& faceId, int vertexIndex, const Vec2& uv);
    void SetFaceTile(const std::string& faceId, int tilesetIndex, int tileIndex);

    // Tileset management
    void AddTileset(const std::string& imagePath, int tileWidth, int tileHeight);
    void ClearTilesets();
    size_t GetTilesetCount() const { return m_TilesetPaths.size(); }

    // Rendering
    bool InitializeRendering(RenderWorld* renderWorld);
    void ShutdownRendering();
    MeshHandle GetMesh();
    MaterialHandle GetMaterial() const { return m_Material; }
    void RegenerateMesh();

    // Texture management
    bool LoadTilesetTexture(int tilesetIndex);
    void SetTexture(TextureHandle texture);
    TextureHandle GetTexture() const { return m_Texture; }

    // Serialization
    std::string ToJSON() const;
    bool FromJSON(const std::string& jsonStr);

    // Export
    std::string ExportToOBJ(bool mergeVertices = true, bool useTextureAtlas = true) const;
    std::string ExportToMTL(bool useTextureAtlas = true) const;

private:
    void GenerateFaceMesh(const TileFace25D& face, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) const;
    std::string GenerateUUID() const;
    void UpdateMaterialTexture();

    std::map<std::string, TileFace25D> m_Faces;

    // Tileset info
    std::vector<std::string> m_TilesetPaths;
    std::vector<int> m_TilesetWidths;
    std::vector<int> m_TilesetHeights;

    // Rendering
    RenderWorld* m_RenderWorld;
    MeshHandle m_CombinedMesh;
    MaterialHandle m_Material;
    TextureHandle m_Texture;
    std::vector<TextureHandle> m_TilesetTextures;
    bool m_MeshDirty;
    bool m_RenderingInitialized;
};

} // namespace engine
} // namespace lupine

#include "lupine/engine/TileMap25DBuilder.hpp"
#include "lupine/rendering/MeshBuilder.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/rendering/TextureUpload.hpp"
#include "lupine/asset/ImageAsset.hpp"
#include "lupine/logger/Logger.hpp"
#include <sstream>
#include <random>
#include <nlohmann/json.hpp>

namespace lupine {
namespace engine {

using namespace math;
using json = nlohmann::json;

TileMap25DBuilder::TileMap25DBuilder()
    : m_RenderWorld(nullptr)
    , m_CombinedMesh()
    , m_Material()
    , m_MeshDirty(true)
    , m_RenderingInitialized(false)
{
}

TileMap25DBuilder::~TileMap25DBuilder() {
    ShutdownRendering();
}

std::string TileMap25DBuilder::GenerateUUID() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string uuid;
    uuid.reserve(36);

    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid += '-';
        } else {
            uuid += hex[dis(gen)];
        }
    }

    return uuid;
}

std::string TileMap25DBuilder::AddFace(const TileFace25D& face) {
    TileFace25D newFace = face;
    if (newFace.id.empty()) {
        newFace.id = GenerateUUID();
    }

    m_Faces[newFace.id] = newFace;
    m_MeshDirty = true;

    return newFace.id;
}

void TileMap25DBuilder::RemoveFace(const std::string& faceId) {
    auto it = m_Faces.find(faceId);
    if (it != m_Faces.end()) {
        m_Faces.erase(it);
        m_MeshDirty = true;
    }
}

TileFace25D* TileMap25DBuilder::GetFace(const std::string& faceId) {
    auto it = m_Faces.find(faceId);
    if (it != m_Faces.end()) {
        return &it->second;
    }
    return nullptr;
}

const TileFace25D* TileMap25DBuilder::GetFace(const std::string& faceId) const {
    auto it = m_Faces.find(faceId);
    if (it != m_Faces.end()) {
        return &it->second;
    }
    return nullptr;
}

bool TileMap25DBuilder::HasFace(const std::string& faceId) const {
    return m_Faces.find(faceId) != m_Faces.end();
}

void TileMap25DBuilder::ClearFaces() {
    m_Faces.clear();
    m_MeshDirty = true;
}

void TileMap25DBuilder::SetFaceVertex(const std::string& faceId, int vertexIndex, const Vec3& position) {
    auto* face = GetFace(faceId);
    if (face && vertexIndex >= 0 && vertexIndex < 4) {
        face->vertices[vertexIndex] = position;
        m_MeshDirty = true;
    }
}

void TileMap25DBuilder::SetFaceUV(const std::string& faceId, int vertexIndex, const Vec2& uv) {
    auto* face = GetFace(faceId);
    if (face && vertexIndex >= 0 && vertexIndex < 4) {
        face->uvs[vertexIndex] = uv;
        m_MeshDirty = true;
    }
}

void TileMap25DBuilder::SetFaceTile(const std::string& faceId, int tilesetIndex, int tileIndex) {
    auto* face = GetFace(faceId);
    if (face) {
        face->tilesetIndex = tilesetIndex;
        face->tileIndex = tileIndex;
        m_MeshDirty = true;
    }
}

void TileMap25DBuilder::AddTileset(const std::string& imagePath, int tileWidth, int tileHeight) {
    m_TilesetPaths.push_back(imagePath);
    m_TilesetWidths.push_back(tileWidth);
    m_TilesetHeights.push_back(tileHeight);
}

void TileMap25DBuilder::ClearTilesets() {
    m_TilesetPaths.clear();
    m_TilesetWidths.clear();
    m_TilesetHeights.clear();
}

bool TileMap25DBuilder::InitializeRendering(RenderWorld* renderWorld) {
    if (!renderWorld) {
        return false;
    }

    m_RenderWorld = renderWorld;

    // Create a material that can hold textures
    // We copy from the default colored material since it supports both vertex colors and textures
    MaterialHandle defaultColored = renderWorld->getDefaultColoredDoubleSidedMaterial();
    const Material* defaultMat = renderWorld->getMaterial(defaultColored);
    if (defaultMat) {
        Material tileMaterial = *defaultMat;  // Copy the default material
        tileMaterial.name = "TileMap25DMaterial";
        m_Material = renderWorld->createMaterial(tileMaterial);
    } else {
        // Fallback - use default colored material directly
        m_Material = defaultColored;
    }

    m_RenderingInitialized = true;
    m_MeshDirty = true;

    return true;
}

void TileMap25DBuilder::ShutdownRendering() {
    if (m_RenderingInitialized && m_RenderWorld) {
        IGfxDevice* device = m_RenderWorld->getDevice();
        if (device) {
            if (m_CombinedMesh.isValid()) {
                device->destroyMesh(m_CombinedMesh);
            }
            // Destroy tileset textures
            for (auto& tex : m_TilesetTextures) {
                if (tex.isValid()) {
                    device->destroyTexture(tex);
                }
            }
            m_TilesetTextures.clear();
            // Note: Don't destroy m_Texture if it was set externally
        }
        m_CombinedMesh = MeshHandle();
    }
    m_RenderingInitialized = false;
    m_RenderWorld = nullptr;
}

bool TileMap25DBuilder::LoadTilesetTexture(int tilesetIndex) {
    if (!m_RenderWorld || tilesetIndex < 0 || tilesetIndex >= static_cast<int>(m_TilesetPaths.size())) {
        return false;
    }

    IGfxDevice* device = m_RenderWorld->getDevice();
    if (!device) {
        return false;
    }

    // Ensure texture vector is large enough
    while (m_TilesetTextures.size() <= static_cast<size_t>(tilesetIndex)) {
        m_TilesetTextures.push_back(TextureHandle());
    }

    // Load image from file using ImageAsset
    const std::string& path = m_TilesetPaths[tilesetIndex];
    asset::ImageAsset imageAsset;
    if (!imageAsset.LoadFromFile(path, true, asset::ImageColorSpace::sRGB)) {
        return false;
    }

    // Create the tileset texture with a full mip chain (when smooth filtering is
    // enabled) so tiles minify cleanly below the design resolution. The shared
    // helper uploads the chain backend-appropriately - setting desc.mipLevels with
    // only the base level uploaded leaves the smaller levels empty on
    // DirectX/Vulkan/Metal.
    TextureHandle texture = CreateTexture2DFromImage(device, imageAsset, TextureFormat::RGBA8_SRGB);
    if (!texture.isValid()) {
        return false;
    }

    // Destroy old texture if exists
    if (m_TilesetTextures[tilesetIndex].isValid()) {
        device->destroyTexture(m_TilesetTextures[tilesetIndex]);
    }

    m_TilesetTextures[tilesetIndex] = texture;

    // If this is the first/primary tileset, set it as the main texture
    if (tilesetIndex == 0) {
        m_Texture = texture;
        UpdateMaterialTexture();
    }

    return true;
}

void TileMap25DBuilder::SetTexture(TextureHandle texture) {
    m_Texture = texture;
    UpdateMaterialTexture();
}

void TileMap25DBuilder::UpdateMaterialTexture() {
    if (!m_RenderWorld || !m_Material.isValid()) {
        return;
    }

    const Material* mat = m_RenderWorld->getMaterial(m_Material);
    if (!mat) {
        return;
    }

    // Update the material with the texture
    Material updatedMat = *mat;

    if (m_Texture.isValid()) {
        // Set texture flag to indicate albedo texture is present
        updatedMat.properties["u_TextureFlags"] = Vec4(1.0f, 0.0f, 0.0f, 0.0f);
        // Set texture in the textures map
        updatedMat.textures["u_AlbedoTexture"] = m_Texture;
    } else {
        updatedMat.properties["u_TextureFlags"] = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
        updatedMat.textures.erase("u_AlbedoTexture");
    }

    m_RenderWorld->updateMaterial(m_Material, updatedMat);
}

MeshHandle TileMap25DBuilder::GetMesh() {
    if (!m_RenderingInitialized || !m_RenderWorld) {
        return MeshHandle();
    }

    if (m_MeshDirty) {
        RegenerateMesh();
        m_MeshDirty = false;
    }

    return m_CombinedMesh;
}

void TileMap25DBuilder::RegenerateMesh() {
    if (!m_RenderWorld) {
        return;
    }

    IGfxDevice* device = m_RenderWorld->getDevice();
    if (!device) {
        return;
    }

    // Destroy old mesh
    if (m_CombinedMesh.isValid()) {
        device->destroyMesh(m_CombinedMesh);
        m_CombinedMesh = MeshHandle();
    }

    if (m_Faces.empty()) {
        return;
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Reserve space for all faces
    size_t estimatedVertices = m_Faces.size() * 4;
    size_t estimatedIndices = m_Faces.size() * 6;

    // Account for two-sided faces
    for (const auto& [id, face] : m_Faces) {
        if (face.twoSided) {
            estimatedVertices += 4;
            estimatedIndices += 6;
        }
    }

    vertices.reserve(estimatedVertices);
    indices.reserve(estimatedIndices);

    // Generate mesh for each face
    for (const auto& [id, face] : m_Faces) {
        if (face.visible) {
            GenerateFaceMesh(face, vertices, indices);
        }
    }

    // Create mesh
    MeshData meshData;
    meshData.vertices = std::move(vertices);
    meshData.indices = std::move(indices);

    m_CombinedMesh = device->createMesh(meshData);
}

void TileMap25DBuilder::GenerateFaceMesh(const TileFace25D& face, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) const {
    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

    // Calculate normal
    Vec3 normal = face.GetNormal();

    // Add four vertices for the quad
    for (int i = 0; i < 4; ++i) {
        Vertex vertex;
        vertex.position = face.vertices[i];
        vertex.normal = normal;
        vertex.texCoord = face.uvs[i];
        vertex.color = Vec4(face.color.r, face.color.g, face.color.b, face.color.a);
        vertices.push_back(vertex);
    }

    // Two triangles for the quad
    // Triangle 1: 0, 1, 2
    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 1);
    indices.push_back(baseIndex + 2);

    // Triangle 2: 0, 2, 3
    indices.push_back(baseIndex + 0);
    indices.push_back(baseIndex + 2);
    indices.push_back(baseIndex + 3);

    // If two-sided, add back face
    if (face.twoSided) {
        baseIndex = static_cast<uint32_t>(vertices.size());
        Vec3 backNormal = normal * -1.0f;

        // Add vertices with flipped normal
        for (int i = 0; i < 4; ++i) {
            Vertex vertex;
            vertex.position = face.vertices[i];
            vertex.normal = backNormal;
            vertex.texCoord = face.uvs[i];
            vertex.color = Vec4(face.color.r, face.color.g, face.color.b, face.color.a);
            vertices.push_back(vertex);
        }

        // Reversed winding order for back face
        indices.push_back(baseIndex + 2);
        indices.push_back(baseIndex + 1);
        indices.push_back(baseIndex + 0);

        indices.push_back(baseIndex + 3);
        indices.push_back(baseIndex + 2);
        indices.push_back(baseIndex + 0);
    }
}

std::string TileMap25DBuilder::ToJSON() const {
    json j;
    j["faces"] = json::array();

    for (const auto& [id, face] : m_Faces) {
        json faceJson;
        faceJson["id"] = face.id;
        faceJson["vertices"] = json::array();
        faceJson["uvs"] = json::array();

        for (int i = 0; i < 4; ++i) {
            faceJson["vertices"].push_back({face.vertices[i].x, face.vertices[i].y, face.vertices[i].z});
            faceJson["uvs"].push_back({face.uvs[i].x, face.uvs[i].y});
        }

        faceJson["tileset_index"] = face.tilesetIndex;
        faceJson["tile_index"] = face.tileIndex;
        faceJson["visible"] = face.visible;
        faceJson["two_sided"] = face.twoSided;
        faceJson["color"] = {face.color.r, face.color.g, face.color.b, face.color.a};

        j["faces"].push_back(faceJson);
    }

    j["tilesets"] = json::array();
    for (size_t i = 0; i < m_TilesetPaths.size(); ++i) {
        j["tilesets"].push_back({
            {"path", m_TilesetPaths[i]},
            {"tile_width", m_TilesetWidths[i]},
            {"tile_height", m_TilesetHeights[i]}
        });
    }

    return j.dump(2);
}

bool TileMap25DBuilder::FromJSON(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);

        ClearFaces();
        ClearTilesets();

        // Load tilesets
        if (j.contains("tilesets") && j["tilesets"].is_array()) {
            for (const auto& ts : j["tilesets"]) {
                std::string path = ts.value("path", "");
                int tileWidth = ts.value("tile_width", 32);
                int tileHeight = ts.value("tile_height", 32);
                AddTileset(path, tileWidth, tileHeight);
            }
        }

        // Load faces
        if (j.contains("faces") && j["faces"].is_array()) {
            for (const auto& faceJson : j["faces"]) {
                TileFace25D face;
                face.id = faceJson.value("id", GenerateUUID());

                // Load vertices
                if (faceJson.contains("vertices") && faceJson["vertices"].is_array()) {
                    for (int i = 0; i < 4 && i < faceJson["vertices"].size(); ++i) {
                        const auto& v = faceJson["vertices"][i];
                        if (v.is_array() && v.size() >= 3) {
                            face.vertices[i] = Vec3(v[0], v[1], v[2]);
                        }
                    }
                }

                // Load UVs
                if (faceJson.contains("uvs") && faceJson["uvs"].is_array()) {
                    for (int i = 0; i < 4 && i < faceJson["uvs"].size(); ++i) {
                        const auto& uv = faceJson["uvs"][i];
                        if (uv.is_array() && uv.size() >= 2) {
                            face.uvs[i] = Vec2(uv[0], uv[1]);
                        }
                    }
                }

                face.tilesetIndex = faceJson.value("tileset_index", 0);
                face.tileIndex = faceJson.value("tile_index", 0);
                face.visible = faceJson.value("visible", true);
                face.twoSided = faceJson.value("two_sided", true);

                // Load color
                if (faceJson.contains("color") && faceJson["color"].is_array()) {
                    const auto& c = faceJson["color"];
                    if (c.size() >= 4) {
                        face.color = Color(c[0], c[1], c[2], c[3]);
                    }
                }

                m_Faces[face.id] = face;
            }
        }

        m_MeshDirty = true;
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR(LogCategory::Tools, "TileMap25DBuilder: failed to parse JSON: {}", e.what());
        return false;
    }
}

std::string TileMap25DBuilder::ExportToOBJ(bool mergeVertices, bool useTextureAtlas) const {
    std::ostringstream obj;
    obj << "# TileMap 2.5D Export\n";
    obj << "# Faces: " << m_Faces.size() << "\n";
    obj << "# Merge Vertices: " << (mergeVertices ? "Yes" : "No") << "\n";
    obj << "# Texture Mode: " << (useTextureAtlas ? "Atlas" : "Per-Face UVs") << "\n\n";

    if (m_Faces.empty()) {
        return obj.str();
    }

    // Reference MTL file
    obj << "mtllib tilemap25d.mtl\n";
    obj << "usemtl tilemap_material\n\n";

    // Build atlas UV mapping if needed
    std::map<std::pair<int, int>, Vec4> atlasUVMap; // (tilesetIndex, tileIndex) -> (u_min, v_min, u_max, v_max)
    int atlasSize = 0;

    if (useTextureAtlas && !m_TilesetPaths.empty()) {
        // Calculate total tiles across all tilesets
        int totalTiles = 0;
        for (size_t i = 0; i < m_TilesetPaths.size(); ++i) {
            // Calculate tiles per tileset
            // Assuming tileset images are loaded, we need to know their dimensions
            // For now, we'll use a simple grid layout
            int tilesInSet = 100; // Placeholder - in real implementation, calculate from image dimensions
            totalTiles += tilesInSet;
        }

        // Calculate atlas grid size
        atlasSize = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(totalTiles))));

        // Map each tile to atlas position
        int currentTile = 0;
        for (size_t tsIdx = 0; tsIdx < m_TilesetPaths.size(); ++tsIdx) {
            int tilesInSet = 100; // Placeholder
            for (int tileIdx = 0; tileIdx < tilesInSet; ++tileIdx) {
                int atlasRow = currentTile / atlasSize;
                int atlasCol = currentTile % atlasSize;

                float u_min = static_cast<float>(atlasCol) / static_cast<float>(atlasSize);
                float v_min = static_cast<float>(atlasRow) / static_cast<float>(atlasSize);
                float u_max = static_cast<float>(atlasCol + 1) / static_cast<float>(atlasSize);
                float v_max = static_cast<float>(atlasRow + 1) / static_cast<float>(atlasSize);

                atlasUVMap[{static_cast<int>(tsIdx), tileIdx}] = Vec4(u_min, v_min, u_max, v_max);
                currentTile++;
            }
        }
    }

    uint32_t vertexOffset = 1;
    uint32_t normalOffset = 1;

    for (const auto& [id, face] : m_Faces) {
        if (!face.visible) continue;

        // Write vertices
        for (int i = 0; i < 4; ++i) {
            obj << "v " << face.vertices[i].x << " "
                << face.vertices[i].y << " "
                << face.vertices[i].z << "\n";
        }

        // Write texture coordinates
        if (useTextureAtlas && atlasUVMap.count({face.tilesetIndex, face.tileIndex})) {
            // Use atlas UVs
            Vec4 atlasUV = atlasUVMap[{face.tilesetIndex, face.tileIndex}];

            // Map face UVs to atlas region
            obj << "vt " << atlasUV.x << " " << (1.0f - atlasUV.y) << "\n";  // Bottom-left
            obj << "vt " << atlasUV.z << " " << (1.0f - atlasUV.y) << "\n";  // Bottom-right
            obj << "vt " << atlasUV.z << " " << (1.0f - atlasUV.w) << "\n";  // Top-right
            obj << "vt " << atlasUV.x << " " << (1.0f - atlasUV.w) << "\n";  // Top-left
        } else {
            // Use per-face UVs (original tile UVs)
            for (int i = 0; i < 4; ++i) {
                obj << "vt " << face.uvs[i].x << " " << (1.0f - face.uvs[i].y) << "\n";
            }
        }

        // Write face (two triangles forming a quad)
        // Vertices are ordered counter-clockwise: 0-1-2-3
        // For flat shading, calculate normals per triangle
        uint32_t v0 = vertexOffset;
        uint32_t v1 = vertexOffset + 1;
        uint32_t v2 = vertexOffset + 2;
        uint32_t v3 = vertexOffset + 3;

        uint32_t vt0 = vertexOffset;
        uint32_t vt1 = vertexOffset + 1;
        uint32_t vt2 = vertexOffset + 2;
        uint32_t vt3 = vertexOffset + 3;

        Vec3 v0_pos = face.vertices[0];
        Vec3 v1_pos = face.vertices[1];
        Vec3 v2_pos = face.vertices[2];
        Vec3 v3_pos = face.vertices[3];

        // Calculate normals for both diagonal options
        Vec3 n1_diag02 = (v1_pos - v0_pos).Cross(v2_pos - v0_pos).Normalized();
        Vec3 n2_diag02 = (v2_pos - v0_pos).Cross(v3_pos - v0_pos).Normalized();
        Vec3 n1_diag13 = (v1_pos - v0_pos).Cross(v3_pos - v0_pos).Normalized();
        Vec3 n2_diag13 = (v2_pos - v1_pos).Cross(v3_pos - v1_pos).Normalized();

        // Calculate dot products (closer to 1.0 means more coplanar)
        float dot_diag02 = n1_diag02.Dot(n2_diag02);
        float dot_diag13 = n1_diag13.Dot(n2_diag13);

        bool use_diag_02 = dot_diag02 >= dot_diag13;

        if (use_diag_02) {
            // Write normals for this diagonal option
            obj << "vn " << n1_diag02.x << " " << n1_diag02.y << " " << n1_diag02.z << "\n";
            obj << "vn " << n2_diag02.x << " " << n2_diag02.y << " " << n2_diag02.z << "\n";

            uint32_t vn1 = normalOffset;
            uint32_t vn2 = normalOffset + 1;

            // Front face triangles using 0-2 diagonal
            obj << "f " << v0 << "/" << vt0 << "/" << vn1 << " "
                << v1 << "/" << vt1 << "/" << vn1 << " "
                << v2 << "/" << vt2 << "/" << vn1 << "\n";

            obj << "f " << v0 << "/" << vt0 << "/" << vn2 << " "
                << v2 << "/" << vt2 << "/" << vn2 << " "
                << v3 << "/" << vt3 << "/" << vn2 << "\n";

            // Back face if two-sided (reverse winding order)
            if (face.twoSided) {
                Vec3 n1_back = n1_diag02 * -1.0f;
                Vec3 n2_back = n2_diag02 * -1.0f;
                obj << "vn " << n1_back.x << " " << n1_back.y << " " << n1_back.z << "\n";
                obj << "vn " << n2_back.x << " " << n2_back.y << " " << n2_back.z << "\n";

                uint32_t vn1_back = normalOffset + 2;
                uint32_t vn2_back = normalOffset + 3;

                obj << "f " << v2 << "/" << vt2 << "/" << vn1_back << " "
                    << v1 << "/" << vt1 << "/" << vn1_back << " "
                    << v0 << "/" << vt0 << "/" << vn1_back << "\n";

                obj << "f " << v3 << "/" << vt3 << "/" << vn2_back << " "
                    << v2 << "/" << vt2 << "/" << vn2_back << " "
                    << v0 << "/" << vt0 << "/" << vn2_back << "\n";

                normalOffset += 4;
            } else {
                normalOffset += 2;
            }
        } else {
            // Write normals for this diagonal option
            obj << "vn " << n1_diag13.x << " " << n1_diag13.y << " " << n1_diag13.z << "\n";
            obj << "vn " << n2_diag13.x << " " << n2_diag13.y << " " << n2_diag13.z << "\n";

            uint32_t vn1 = normalOffset;
            uint32_t vn2 = normalOffset + 1;

            // Front face triangles using 1-3 diagonal
            obj << "f " << v0 << "/" << vt0 << "/" << vn1 << " "
                << v1 << "/" << vt1 << "/" << vn1 << " "
                << v3 << "/" << vt3 << "/" << vn1 << "\n";

            obj << "f " << v1 << "/" << vt1 << "/" << vn2 << " "
                << v2 << "/" << vt2 << "/" << vn2 << " "
                << v3 << "/" << vt3 << "/" << vn2 << "\n";

            // Back face if two-sided (reverse winding order)
            if (face.twoSided) {
                Vec3 n1_back = n1_diag13 * -1.0f;
                Vec3 n2_back = n2_diag13 * -1.0f;
                obj << "vn " << n1_back.x << " " << n1_back.y << " " << n1_back.z << "\n";
                obj << "vn " << n2_back.x << " " << n2_back.y << " " << n2_back.z << "\n";

                uint32_t vn1_back = normalOffset + 2;
                uint32_t vn2_back = normalOffset + 3;

                obj << "f " << v3 << "/" << vt3 << "/" << vn1_back << " "
                    << v1 << "/" << vt1 << "/" << vn1_back << " "
                    << v0 << "/" << vt0 << "/" << vn1_back << "\n";

                obj << "f " << v3 << "/" << vt3 << "/" << vn2_back << " "
                    << v2 << "/" << vt2 << "/" << vn2_back << " "
                    << v1 << "/" << vt1 << "/" << vn2_back << "\n";

                normalOffset += 4;
            } else {
                normalOffset += 2;
            }
        }

        vertexOffset += 4;
    }

    return obj.str();
}

std::string TileMap25DBuilder::ExportToMTL(bool useTextureAtlas) const {
    std::ostringstream mtl;
    mtl << "# TileMap 2.5D Material Export\n";
    mtl << "# Texture Mode: " << (useTextureAtlas ? "Atlas" : "Per-Face UVs") << "\n\n";

    mtl << "newmtl tilemap_material\n";
    mtl << "Ka 1.0 1.0 1.0\n";  // Ambient color
    mtl << "Kd 1.0 1.0 1.0\n";  // Diffuse color
    mtl << "Ks 0.0 0.0 0.0\n";  // Specular color
    mtl << "Ns 10.0\n";          // Specular exponent
    mtl << "d 1.0\n";            // Dissolve (opacity)
    mtl << "illum 1\n";          // Illumination model (1 = diffuse)

    if (useTextureAtlas) {
        mtl << "map_Kd tilemap25d_atlas.png\n";  // Diffuse texture map
    } else {
        // For per-face UVs, reference the first tileset texture
        if (!m_TilesetPaths.empty()) {
            // Extract filename from path
            std::string texturePath = m_TilesetPaths[0];
            size_t lastSlash = texturePath.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ? texturePath.substr(lastSlash + 1) : texturePath;
            mtl << "map_Kd " << filename << "\n";
        }
    }

    mtl << "\n";
    return mtl.str();
}

} // namespace engine
} // namespace lupine

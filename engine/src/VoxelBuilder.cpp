#include "lupine/engine/VoxelBuilder.hpp"
#include "lupine/rendering/MeshBuilder.hpp"
#include "lupine/rendering/RenderWorld.hpp"
#include "lupine/logger/Logger.hpp"
#include <sstream>
#include <nlohmann/json.hpp>

namespace lupine {
namespace engine {

using namespace math;
using json = nlohmann::json;

VoxelBuilder::VoxelBuilder()
    : m_RenderWorld(nullptr)
    , m_CombinedMesh()
    , m_Material()
    , m_MeshDirty(true)
    , m_RenderingInitialized(false)
{
}

VoxelBuilder::~VoxelBuilder() {
    ShutdownRendering();
}

void VoxelBuilder::PlaceVoxel(int32_t x, int32_t y, int32_t z, const Color& color) {
    auto key = std::make_tuple(x, y, z);
    m_Voxels[key] = Voxel(x, y, z, color);
    m_MeshDirty = true;
}

void VoxelBuilder::EraseVoxel(int32_t x, int32_t y, int32_t z) {
    auto key = std::make_tuple(x, y, z);
    auto it = m_Voxels.find(key);
    if (it != m_Voxels.end()) {
        m_Voxels.erase(it);
        m_MeshDirty = true;
    }
}

bool VoxelBuilder::HasVoxel(int32_t x, int32_t y, int32_t z) const {
    auto key = std::make_tuple(x, y, z);
    return m_Voxels.find(key) != m_Voxels.end();
}

const Voxel* VoxelBuilder::GetVoxel(int32_t x, int32_t y, int32_t z) const {
    auto key = std::make_tuple(x, y, z);
    auto it = m_Voxels.find(key);
    if (it != m_Voxels.end()) {
        return &it->second;
    }
    return nullptr;
}

void VoxelBuilder::Clear() {
    m_Voxels.clear();
    m_MeshDirty = true;
}

bool VoxelBuilder::InitializeRendering(RenderWorld* renderWorld) {
    if (!renderWorld) {

        return false;
    }

    m_RenderWorld = renderWorld;

    MaterialHandle defaultPBR = renderWorld->getDefaultPBRMaterial();
    if (!defaultPBR.isValid()) {

        return false;
    }

    const Material* defaultMat = renderWorld->getMaterial(defaultPBR);
    if (!defaultMat) {

        return false;
    }

    Material voxelMat;
    voxelMat.name = "VoxelPBR";
    voxelMat.vertexShader = defaultMat->vertexShader;
    voxelMat.fragmentShader = defaultMat->fragmentShader;
    voxelMat.pipeline = defaultMat->pipeline;
    voxelMat.renderLayer = RenderLayer::Opaque;
    voxelMat.castsShadows = true;
    voxelMat.receivesShadows = true;
    voxelMat.isTransparent = false;
    voxelMat.alphaClipThreshold = 0.5f;

    voxelMat.properties["u_AlbedoColor"] = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    voxelMat.properties["u_EmissiveColor"] = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    voxelMat.properties["u_MaterialParams1"] = Vec4(0.0f, 0.8f, 1.0f, 1.0f);
    voxelMat.properties["u_MaterialParams2"] = Vec4(0.5f, 1.0f, 0.05f, 0.0f);
    voxelMat.properties["u_TextureFlags"] = Vec4(0.0f, 0.0f, 0.0f, 0.0f);
    voxelMat.properties["u_TintColor"] = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

    m_Material = renderWorld->createMaterial(voxelMat);
    if (!m_Material.isValid()) {

        return false;
    }

    m_RenderingInitialized = true;
    m_MeshDirty = true;

    return true;
}

void VoxelBuilder::ShutdownRendering() {
    if (m_RenderingInitialized && m_RenderWorld && m_CombinedMesh.isValid()) {
        IGfxDevice* device = m_RenderWorld->getDevice();
        if (device) {
            device->destroyMesh(m_CombinedMesh);
        }
        m_CombinedMesh = MeshHandle();
    }
    m_RenderingInitialized = false;
    m_RenderWorld = nullptr;
}

MeshHandle VoxelBuilder::GetMesh() {
    if (!m_RenderingInitialized || !m_RenderWorld) {
        return MeshHandle();
    }

    if (m_MeshDirty) {
        RegenerateMesh();
        m_MeshDirty = false;
    }

    return m_CombinedMesh;
}

void VoxelBuilder::RegenerateMesh() {
    if (!m_RenderWorld) {
        return;
    }

    IGfxDevice* device = m_RenderWorld->getDevice();
    if (!device) {
        return;
    }

    if (m_CombinedMesh.isValid()) {
        device->destroyMesh(m_CombinedMesh);
        m_CombinedMesh = MeshHandle();
    }

    if (m_Voxels.empty()) {
        return;
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    vertices.reserve(m_Voxels.size() * 24);
    indices.reserve(m_Voxels.size() * 36);

    if (!m_Voxels.empty()) {
        const auto& firstVoxel = m_Voxels.begin()->second;

    }

    for (const auto& [pos, voxel] : m_Voxels) {
        GenerateCubeMesh(voxel, vertices, indices);
    }

    MeshData meshData;
    meshData.vertices = std::move(vertices);
    meshData.indices = std::move(indices);

    m_CombinedMesh = device->createMesh(meshData);

    if (m_CombinedMesh.isValid()) {

    } else {

    }
}

void VoxelBuilder::GenerateCubeMesh(const Voxel& voxel, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) const {
    const float size = 1.0f;
    const float halfSize = size * 0.5f;
    const float x = static_cast<float>(voxel.x);
    const float y = static_cast<float>(voxel.y);
    const float z = static_cast<float>(voxel.z);

    uint32_t baseIndex = static_cast<uint32_t>(vertices.size());

    Vec3 corners[8] = {
        Vec3(x - halfSize, y - halfSize, z - halfSize),
        Vec3(x + halfSize, y - halfSize, z - halfSize),
        Vec3(x + halfSize, y + halfSize, z - halfSize),
        Vec3(x - halfSize, y + halfSize, z - halfSize),
        Vec3(x - halfSize, y - halfSize, z + halfSize),
        Vec3(x + halfSize, y - halfSize, z + halfSize),
        Vec3(x + halfSize, y + halfSize, z + halfSize),
        Vec3(x - halfSize, y + halfSize, z + halfSize)
    };

    struct Face {
        int corners[4];
        Vec3 normal;
        float lightingFactor;
    };

    Face faces[6] = {
        { {0, 1, 2, 3}, Vec3(0, 0, -1), 0.65f },
        { {5, 4, 7, 6}, Vec3(0, 0, 1),  0.50f },
        { {4, 0, 3, 7}, Vec3(-1, 0, 0), 0.55f },
        { {1, 5, 6, 2}, Vec3(1, 0, 0),  0.70f },
        { {3, 2, 6, 7}, Vec3(0, 1, 0),  0.95f },
        { {4, 5, 1, 0}, Vec3(0, -1, 0), 0.35f }
    };

    Vec2 uvs[4] = {
        Vec2(0, 0), Vec2(1, 0), Vec2(1, 1), Vec2(0, 1)
    };

    for (int f = 0; f < 6; ++f) {
        const Face& face = faces[f];

        Vec4 faceColor(
            voxel.color.r * face.lightingFactor,
            voxel.color.g * face.lightingFactor,
            voxel.color.b * face.lightingFactor,
            voxel.color.a
        );

        for (int v = 0; v < 4; ++v) {
            Vertex vertex;
            vertex.position = corners[face.corners[v]];
            vertex.normal = face.normal;
            vertex.texCoord = uvs[v];
            vertex.color = faceColor;
            vertices.push_back(vertex);
        }

        uint32_t faceBase = baseIndex + f * 4;
        indices.push_back(faceBase + 0);
        indices.push_back(faceBase + 2);
        indices.push_back(faceBase + 1);
        indices.push_back(faceBase + 0);
        indices.push_back(faceBase + 3);
        indices.push_back(faceBase + 2);
    }
}

std::string VoxelBuilder::ToJSON() const {
    json j;
    j["voxels"] = json::array();

    for (const auto& [pos, voxel] : m_Voxels) {
        json voxelJson;
        voxelJson["x"] = voxel.x;
        voxelJson["y"] = voxel.y;
        voxelJson["z"] = voxel.z;
        voxelJson["color"] = {
            voxel.color.r,
            voxel.color.g,
            voxel.color.b,
            voxel.color.a
        };
        j["voxels"].push_back(voxelJson);
    }

    return j.dump(2);
}

bool VoxelBuilder::FromJSON(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);

        Clear();

        if (j.contains("voxels") && j["voxels"].is_array()) {
            for (const auto& voxelJson : j["voxels"]) {
                int32_t x = voxelJson["x"];
                int32_t y = voxelJson["y"];
                int32_t z = voxelJson["z"];

                Color color = Color::White();
                if (voxelJson.contains("color") && voxelJson["color"].is_array()) {
                    auto colorArray = voxelJson["color"];
                    if (colorArray.size() >= 3) {
                        color.r = colorArray[0];
                        color.g = colorArray[1];
                        color.b = colorArray[2];
                        color.a = (colorArray.size() >= 4) ? static_cast<float>(colorArray[3]) : 1.0f;
                    }
                }

                PlaceVoxel(x, y, z, color);
            }
        }

        return true;
    }
    catch (const std::exception& e) {

        return false;
    }
}

std::string VoxelBuilder::ExportToOBJ(bool mergeFaces, bool textureAtlas) const {
    std::ostringstream obj;
    obj << "# Voxel Builder Export\n";
    obj << "# Voxels: " << m_Voxels.size() << "\n";
    obj << "# Merge Faces: " << (mergeFaces ? "Yes" : "No") << "\n";
    obj << "# Texture Mode: " << (textureAtlas ? "Atlas" : "Vertex Colors") << "\n\n";

    if (m_Voxels.empty()) {
        return obj.str();
    }

    uint32_t vertexOffset = 1;

    struct Face {
        int corners[4];
        float normal[3];
    };

    for (const auto& [pos, voxel] : m_Voxels) {
        const float size = 1.0f;
        const float s = size * 0.5f;
        const float x = static_cast<float>(voxel.x);
        const float y = static_cast<float>(voxel.y);
        const float z = static_cast<float>(voxel.z);

        float corners[8][3] = {
            {x - s, y - s, z - s},
            {x + s, y - s, z - s},
            {x + s, y + s, z - s},
            {x - s, y + s, z - s},
            {x - s, y - s, z + s},
            {x + s, y - s, z + s},
            {x + s, y + s, z + s},
            {x - s, y + s, z + s}
        };

        Face faces[6] = {
            { {0, 1, 2, 3}, {0, 0, -1} },
            { {5, 4, 7, 6}, {0, 0, 1} },
            { {4, 0, 3, 7}, {-1, 0, 0} },
            { {1, 5, 6, 2}, {1, 0, 0} },
            { {3, 2, 6, 7}, {0, 1, 0} },
            { {4, 5, 1, 0}, {0, -1, 0} }
        };

        float uvs[4][2] = {
            {0, 0}, {1, 0}, {1, 1}, {0, 1}
        };

        uint32_t faceVertexStart = vertexOffset;

        for (int f = 0; f < 6; ++f) {
            const Face& face = faces[f];

            if (mergeFaces) {
                int nx = voxel.x + static_cast<int>(face.normal[0]);
                int ny = voxel.y + static_cast<int>(face.normal[1]);
                int nz = voxel.z + static_cast<int>(face.normal[2]);

                if (HasVoxel(nx, ny, nz)) {
                    continue;
                }
            }

            for (int v = 0; v < 4; ++v) {
                int cornerIdx = face.corners[v];
                obj << "v " << corners[cornerIdx][0] << " "
                    << corners[cornerIdx][1] << " "
                    << corners[cornerIdx][2];

                if (!textureAtlas) {
                    obj << " " << voxel.color.r << " "
                        << voxel.color.g << " "
                        << voxel.color.b;
                }
                obj << "\n";
            }

            for (int v = 0; v < 4; ++v) {
                obj << "vn " << face.normal[0] << " "
                    << face.normal[1] << " "
                    << face.normal[2] << "\n";
            }

            if (textureAtlas) {
                for (int v = 0; v < 4; ++v) {
                    obj << "vt " << uvs[v][0] << " " << uvs[v][1] << "\n";
                }
            }

            if (textureAtlas) {

                obj << "f " << vertexOffset << "/" << vertexOffset << "/" << vertexOffset << " "
                    << (vertexOffset + 1) << "/" << (vertexOffset + 1) << "/" << (vertexOffset + 1) << " "
                    << (vertexOffset + 2) << "/" << (vertexOffset + 2) << "/" << (vertexOffset + 2) << "\n";
                obj << "f " << vertexOffset << "/" << vertexOffset << "/" << vertexOffset << " "
                    << (vertexOffset + 2) << "/" << (vertexOffset + 2) << "/" << (vertexOffset + 2) << " "
                    << (vertexOffset + 3) << "/" << (vertexOffset + 3) << "/" << (vertexOffset + 3) << "\n";
            } else {

                obj << "f " << vertexOffset << "//" << vertexOffset << " "
                    << (vertexOffset + 1) << "//" << (vertexOffset + 1) << " "
                    << (vertexOffset + 2) << "//" << (vertexOffset + 2) << "\n";
                obj << "f " << vertexOffset << "//" << vertexOffset << " "
                    << (vertexOffset + 2) << "//" << (vertexOffset + 2) << " "
                    << (vertexOffset + 3) << "//" << (vertexOffset + 3) << "\n";
            }

            vertexOffset += 4;
        }
    }

    return obj.str();
}

std::string VoxelBuilder::ExportToGLTF(bool mergeFaces, bool textureAtlas) const {
    using json = nlohmann::json;

    if (m_Voxels.empty()) {

        return "{}";
    }

    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> colors;
    std::vector<uint32_t> indices;

    positions.reserve(m_Voxels.size() * 24 * 3);
    normals.reserve(m_Voxels.size() * 24 * 3);
    colors.reserve(m_Voxels.size() * 24 * 4);
    indices.reserve(m_Voxels.size() * 36);

    for (const auto& [pos, voxel] : m_Voxels) {
        const float size = 1.0f;
        const float s = size * 0.5f;
        const float x = static_cast<float>(voxel.x);
        const float y = static_cast<float>(voxel.y);
        const float z = static_cast<float>(voxel.z);

        float corners[8][3] = {
            {x - s, y - s, z - s},
            {x + s, y - s, z - s},
            {x + s, y + s, z - s},
            {x - s, y + s, z - s},
            {x - s, y - s, z + s},
            {x + s, y - s, z + s},
            {x + s, y + s, z + s},
            {x - s, y + s, z + s}
        };

        struct Face {
            int corners[4];
            float normal[3];
        };

        Face faces[6] = {
            { {0, 1, 2, 3}, {0, 0, -1} },
            { {5, 4, 7, 6}, {0, 0, 1} },
            { {4, 0, 3, 7}, {-1, 0, 0} },
            { {1, 5, 6, 2}, {1, 0, 0} },
            { {3, 2, 6, 7}, {0, 1, 0} },
            { {4, 5, 1, 0}, {0, -1, 0} }
        };

        uint32_t baseIndex = static_cast<uint32_t>(positions.size() / 3);

        for (int f = 0; f < 6; ++f) {
            const Face& face = faces[f];

            if (mergeFaces) {

                int nx = voxel.x + static_cast<int>(face.normal[0]);
                int ny = voxel.y + static_cast<int>(face.normal[1]);
                int nz = voxel.z + static_cast<int>(face.normal[2]);

                if (HasVoxel(nx, ny, nz)) {
                    continue;
                }
            }

            for (int v = 0; v < 4; ++v) {
                int cornerIdx = face.corners[v];

                positions.push_back(corners[cornerIdx][0]);
                positions.push_back(corners[cornerIdx][1]);
                positions.push_back(corners[cornerIdx][2]);

                normals.push_back(face.normal[0]);
                normals.push_back(face.normal[1]);
                normals.push_back(face.normal[2]);

                colors.push_back(voxel.color.r);
                colors.push_back(voxel.color.g);
                colors.push_back(voxel.color.b);
                colors.push_back(voxel.color.a);
            }

            uint32_t faceBase = baseIndex;
            baseIndex += 4;

            indices.push_back(faceBase + 0);
            indices.push_back(faceBase + 1);
            indices.push_back(faceBase + 2);
            indices.push_back(faceBase + 0);
            indices.push_back(faceBase + 2);
            indices.push_back(faceBase + 3);
        }
    }

    if (positions.empty()) {

        return "{}";
    }

    float minPos[3] = {positions[0], positions[1], positions[2]};
    float maxPos[3] = {positions[0], positions[1], positions[2]};

    for (size_t i = 0; i < positions.size(); i += 3) {
        for (int j = 0; j < 3; ++j) {
            minPos[j] = std::min(minPos[j], positions[i + j]);
            maxPos[j] = std::max(maxPos[j], positions[i + j]);
        }
    }

    std::vector<uint8_t> buffer;
    size_t positionsOffset = buffer.size();
    size_t positionsSize = positions.size() * sizeof(float);
    buffer.resize(buffer.size() + positionsSize);
    memcpy(buffer.data() + positionsOffset, positions.data(), positionsSize);

    size_t normalsOffset = buffer.size();
    size_t normalsSize = normals.size() * sizeof(float);
    buffer.resize(buffer.size() + normalsSize);
    memcpy(buffer.data() + normalsOffset, normals.data(), normalsSize);

    size_t colorsOffset = buffer.size();
    size_t colorsSize = colors.size() * sizeof(float);
    buffer.resize(buffer.size() + colorsSize);
    memcpy(buffer.data() + colorsOffset, colors.data(), colorsSize);

    size_t indicesOffset = buffer.size();
    size_t indicesSize = indices.size() * sizeof(uint32_t);
    buffer.resize(buffer.size() + indicesSize);
    memcpy(buffer.data() + indicesOffset, indices.data(), indicesSize);

    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string base64;
    int i = 0;
    int j = 0;
    uint8_t char_array_3[3];
    uint8_t char_array_4[4];

    for (size_t idx = 0; idx < buffer.size(); ++idx) {
        char_array_3[i++] = buffer[idx];
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                base64 += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; j < i + 1; j++)
            base64 += base64_chars[char_array_4[j]];

        while ((i++ < 3))
            base64 += '=';
    }

    json gltf;
    gltf["asset"] = {
        {"version", "2.0"},
        {"generator", "Lupine Voxel Builder"}
    };

    gltf["buffers"] = json::array({
        {
            {"byteLength", buffer.size()},
            {"uri", "data:application/octet-stream;base64," + base64}
        }
    });

    gltf["bufferViews"] = json::array({
        {{"buffer", 0}, {"byteOffset", positionsOffset}, {"byteLength", positionsSize}, {"target", 34962}},
        {{"buffer", 0}, {"byteOffset", normalsOffset}, {"byteLength", normalsSize}, {"target", 34962}},
        {{"buffer", 0}, {"byteOffset", colorsOffset}, {"byteLength", colorsSize}, {"target", 34962}},
        {{"buffer", 0}, {"byteOffset", indicesOffset}, {"byteLength", indicesSize}, {"target", 34963}}
    });

    gltf["accessors"] = json::array({
        {
            {"bufferView", 0},
            {"byteOffset", 0},
            {"componentType", 5126},
            {"count", positions.size() / 3},
            {"type", "VEC3"},
            {"min", {minPos[0], minPos[1], minPos[2]}},
            {"max", {maxPos[0], maxPos[1], maxPos[2]}}
        },
        {
            {"bufferView", 1},
            {"byteOffset", 0},
            {"componentType", 5126},
            {"count", normals.size() / 3},
            {"type", "VEC3"}
        },
        {
            {"bufferView", 2},
            {"byteOffset", 0},
            {"componentType", 5126},
            {"count", colors.size() / 4},
            {"type", "VEC4"}
        },
        {
            {"bufferView", 3},
            {"byteOffset", 0},
            {"componentType", 5125},
            {"count", indices.size()},
            {"type", "SCALAR"}
        }
    });

    gltf["materials"] = json::array({
        {
            {"name", "VoxelMaterial"},
            {"pbrMetallicRoughness", {
                {"baseColorFactor", {1.0, 1.0, 1.0, 1.0}},
                {"metallicFactor", 0.0},
                {"roughnessFactor", 1.0}
            }}
        }
    });

    gltf["meshes"] = json::array({
        {
            {"name", "VoxelMesh"},
            {"primitives", json::array({
                {
                    {"attributes", {
                        {"POSITION", 0},
                        {"NORMAL", 1},
                        {"COLOR_0", 2}
                    }},
                    {"indices", 3},
                    {"material", 0}
                }
            })}
        }
    });

    gltf["nodes"] = json::array({
        {
            {"name", "VoxelModel"},
            {"mesh", 0}
        }
    });

    gltf["scenes"] = json::array({
        {
            {"name", "Scene"},
            {"nodes", {0}}
        }
    });

    gltf["scene"] = 0;

    return gltf.dump(2);
}

}
}

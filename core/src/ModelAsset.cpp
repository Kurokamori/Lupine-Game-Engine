#include "lupine/asset/ModelAsset.hpp"
#include "lupine/platform/PackFile.hpp"
#include "lupine/logger/Logger.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// These must be Assimp's own declarations on every platform. A locally redeclared
// IOSystem/IOStream is an ODR violation: the vtable it produces has to match the one the
// Assimp library was compiled against, slot for slot, and any drift silently sends a call
// through the wrong slot. Native builds tolerated that; wasm type-checks every indirect
// call and traps with "function signature mismatch" inside the importer.
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>

#include <glm/glm.hpp>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace lupine {
namespace asset {

// Upper bound on one uncompressed embedded texture. Anything above this is treated as a
// corrupt or hostile dimension pair rather than a legitimate texture.
static constexpr uint64_t kMaxEmbeddedTextureBytes = 1ull << 31;

// Custom IOStream for reading from pack files
class PackIOStream : public Assimp::IOStream {
public:
    PackIOStream(std::vector<uint8_t> data, const std::string& path)
        : m_data(std::move(data)), m_path(path), m_pos(0) {}

    ~PackIOStream() override = default;

    size_t Read(void* pvBuffer, size_t pSize, size_t pCount) override {
        // Assimp mirrors fread() semantics, which allow a zero element size; the return
        // value is an element count, so pSize must be checked before it is divided by.
        if (!pvBuffer || pSize == 0 || pCount == 0 || m_pos >= m_data.size()) {
            return 0;
        }

        // Clamping the element count (rather than pSize * pCount) keeps the byte count
        // from overflowing and reports only whole elements, as fread() does.
        const size_t bytesAvailable = m_data.size() - m_pos;
        const size_t elements = std::min(pCount, bytesAvailable / pSize);
        if (elements == 0) {
            return 0;
        }

        const size_t bytesRead = elements * pSize;
        std::memcpy(pvBuffer, m_data.data() + m_pos, bytesRead);
        m_pos += bytesRead;

        return elements;
    }

    size_t Write(const void*, size_t, size_t) override { return 0; }

    aiReturn Seek(size_t pOffset, aiOrigin pOrigin) override {
        size_t newPos = 0;
        switch (pOrigin) {
            case aiOrigin_SET: newPos = pOffset; break;
            case aiOrigin_CUR: newPos = m_pos + pOffset; break;
            case aiOrigin_END: newPos = m_data.size() + pOffset; break;
            default: return aiReturn_FAILURE;
        }
        if (newPos > m_data.size()) return aiReturn_FAILURE;
        m_pos = newPos;
        return aiReturn_SUCCESS;
    }

    size_t Tell() const override { return m_pos; }
    size_t FileSize() const override { return m_data.size(); }
    void Flush() override {}

private:
    std::vector<uint8_t> m_data;
    std::string m_path;
    size_t m_pos;
};

// Custom IOSystem that reads from pack files
// Works on all platforms including Emscripten
class PackIOSystem : public Assimp::IOSystem {
public:
    PackIOSystem(const std::string& basePath) : m_basePath(basePath) {
        // Extract directory from base path
        size_t lastSlash = m_basePath.rfind('/');
        if (lastSlash == std::string::npos) lastSlash = m_basePath.rfind('\\');
        if (lastSlash != std::string::npos) {
            m_baseDir = m_basePath.substr(0, lastSlash + 1);
        }
        
    }

    ~PackIOSystem() override = default;

    bool Exists(const char* pFile) const override {
        std::string resolved = resolvePath(pFile);
        auto& packFS = platform::PackFileSystem::Instance();
        return packFS.exists(resolved);
    }

    char getOsSeparator() const override { return '/'; }

    Assimp::IOStream* Open(const char* pFile, const char* /*pMode*/) override {
        std::string resolved = resolvePath(pFile);
        auto& packFS = platform::PackFileSystem::Instance();

        std::vector<uint8_t> data = packFS.readFile(resolved);
        if (data.empty()) {
            LOG_ERROR(LogCategory::Core, "PackIOSystem::Open failed - no data for {}", resolved);
            return nullptr;
        }
        return new PackIOStream(std::move(data), resolved);
    }

    void Close(Assimp::IOStream* pFile) override {
        delete pFile;
    }

    // Override ChangeDirectory - no-op since we handle paths ourselves
    bool ChangeDirectory(const std::string& path) override {
        if (!path.empty()) {
            if (path[0] == '/' || (path.size() > 6 && path.substr(0, 6) == "res://")) {
                m_currentDir = path;
            } else {
                m_currentDir = m_baseDir + path;
            }
            if (!m_currentDir.empty() && m_currentDir.back() != '/') {
                m_currentDir += '/';
            }
        }
        return true;
    }

private:
    std::string resolvePath(const char* pFile) const {
        std::string path = pFile;

        // If it's already a full res:// path, use it directly
        if (path.size() >= 6 && path.substr(0, 6) == "res://") {
            return path;
        }

        // If it's an absolute path, use it directly
        if (!path.empty() && path[0] == '/') {
            return path;
        }

        // Resolve relative to current directory or base directory
        if (!m_currentDir.empty()) {
            return m_currentDir + path;
        }

        return m_baseDir + path;
    }

    std::string m_basePath;
    std::string m_baseDir;
    mutable std::string m_currentDir;
};

static math::Vec3 AssimpToVec3(const aiVector3D& v) {
    return math::Vec3(v.x, v.y, v.z);
}

static math::Vec2 AssimpToVec2(const aiVector3D& v) {
    return math::Vec2(v.x, v.y);
}

static math::Quat AssimpToQuat(const aiQuaternion& q) {
    return math::Quat(q.w, q.x, q.y, q.z);
}

static math::Mat4 AssimpToMat4(const aiMatrix4x4& m) {
    // Assimp matrices are row-major, but GLM matrices are column-major
    // The glm::mat4 constructor takes arguments in column-major order
    // So we need to transpose the Assimp matrix when converting
    //
    // Assimp matrix layout (row-major):
    // a1 a2 a3 a4
    // b1 b2 b3 b4
    // c1 c2 c3 c4
    // d1 d2 d3 d4
    //
    // GLM expects column-major, so we pass columns:
    glm::mat4 glmMat(
        m.a1, m.b1, m.c1, m.d1,  // First column
        m.a2, m.b2, m.c2, m.d2,  // Second column
        m.a3, m.b3, m.c3, m.d3,  // Third column
        m.a4, m.b4, m.c4, m.d4   // Fourth column
    );
    return math::Mat4(glmMat);
}

ModelAsset::ModelAsset()
    : Asset() {
}

ModelAsset::ModelAsset(const core::UUID& uuid)
    : Asset(uuid) {
}

ModelAsset::~ModelAsset() {
    m_Meshes.clear();
    m_Materials.clear();
    m_Animations.clear();
}

bool ModelAsset::LoadFromFile(const std::string& filepath, bool optimizeMesh) {
    // Store the path (will be converted to res:// format if possible)
    SetPath(filepath);

    // Resolve the path using the asset system (handles UUID fallback)
    std::string resolvedPath = ResolveAssetPath(filepath);

    Assimp::Importer importer;

    unsigned int flags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace;

    if (optimizeMesh) {
        flags |= aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_OptimizeMeshes;
    }

    const aiScene* scene = nullptr;

    // Check if running from pack file first
    auto& packFS = platform::PackFileSystem::Instance();

    if (packFS.isPackMode() && packFS.exists(filepath)) {
        // Use custom IOSystem that reads from pack files
        // This allows Assimp to resolve relative paths (textures, .bin files) from the pack
        // Works on all platforms including Emscripten
        
        importer.SetIOHandler(new PackIOSystem(filepath));
        scene = importer.ReadFile(filepath, flags);
    } else {
        // Not in pack mode or file not in pack - use resolved physical path
        scene = importer.ReadFile(resolvedPath, flags);
    }

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR(LogCategory::Core, "ModelAsset: Failed to load model: {} - {}",
                  filepath, importer.GetErrorString());
        return false;
    }

    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        aiMaterial* aiMat = scene->mMaterials[i];
        Material mat;

        aiString name;
        if (aiMat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
            mat.name = name.C_Str();
        }

        aiColor3D color;
        if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            mat.albedo = math::Vec3(color.r, color.g, color.b);
        }

        float value;
        if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, value) == AI_SUCCESS) {
            mat.metallic = value;
        }
        if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, value) == AI_SUCCESS) {
            mat.roughness = value;
        }
        if (aiMat->Get(AI_MATKEY_OPACITY, value) == AI_SUCCESS) {
            mat.opacity = value;
        }

        aiString texPath;

        // Helper to resolve texture path relative to model directory
        auto resolveTexturePath = [&filepath](const std::string& texturePath) -> std::string {
            if (texturePath.empty()) return "";

            std::string path = texturePath;

            // Normalize path separators first
            std::replace(path.begin(), path.end(), '\\', '/');

            // Check for res:/ or res:// anywhere in the path (handles malformed paths)
            // Look for "res:/" pattern and extract the path after it
            size_t resPos = path.find("res:/");
            if (resPos != std::string::npos) {
                // Skip past "res:/"
                size_t startPos = resPos + 5;
                // Skip additional slashes if present (handles res:// or res:///)
                while (startPos < path.size() && path[startPos] == '/') {
                    startPos++;
                }
                // Reconstruct as proper res:// path
                if (startPos < path.size()) {
                    path = "res://" + path.substr(startPos);
                    return path;
                }
            }

            // If starts with /, it's an absolute path
            if (!path.empty() && path[0] == '/') {
                return path;
            }

            // Get directory of the model file
            std::string modelDir;
            size_t lastSlash = filepath.rfind('/');
            if (lastSlash == std::string::npos) lastSlash = filepath.rfind('\\');
            if (lastSlash != std::string::npos) {
                modelDir = filepath.substr(0, lastSlash + 1);
            }

            // Resolve relative path
            std::string resolved = modelDir + path;

            return resolved;
        };

        if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
            mat.albedoMap = resolveTexturePath(texPath.C_Str());

        } else if (aiMat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS) {
            mat.albedoMap = resolveTexturePath(texPath.C_Str());

        }

        if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS) {
            mat.normalMap = resolveTexturePath(texPath.C_Str());

        } else if (aiMat->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == AI_SUCCESS) {
            mat.normalMap = resolveTexturePath(texPath.C_Str());

        }

        if (aiMat->GetTexture(aiTextureType_METALNESS, 0, &texPath) == AI_SUCCESS) {
            mat.metallicMap = resolveTexturePath(texPath.C_Str());

        }

        if (aiMat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &texPath) == AI_SUCCESS) {
            mat.roughnessMap = resolveTexturePath(texPath.C_Str());

        }

        if (aiMat->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &texPath) == AI_SUCCESS) {
            mat.aoMap = resolveTexturePath(texPath.C_Str());

        }

        if (aiMat->GetTexture(aiTextureType_EMISSIVE, 0, &texPath) == AI_SUCCESS) {
            mat.emissiveMap = resolveTexturePath(texPath.C_Str());

        }

        m_Materials.push_back(mat);
    }

    if (m_Materials.empty()) {
        Material defaultMat;
        defaultMat.name = "Default";
        m_Materials.push_back(defaultMat);
    }

    ProcessEmbeddedTextures(scene);

    ProcessNode(scene->mRootNode, scene);

    ProcessSkeleton(scene);
    ProcessAnimations(scene);

    SetLoaded(true);
    RefreshTrackedBytes();

    return true;
}

void ModelAsset::RefreshTrackedBytes() {
    size_t total = 0;
    for (const Mesh& mesh : m_Meshes) {
        total += mesh.vertices.size() * sizeof(Vertex);
        total += mesh.indices.size() * sizeof(uint32_t);
    }
    SetTrackedBytes(total);
}

void ModelAsset::ProcessNode(void* nodePtr, const void* scenePtr) {
    struct aiNode* node = static_cast<struct aiNode*>(nodePtr);
    const struct aiScene* scene = static_cast<const struct aiScene*>(scenePtr);

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        struct aiMesh* aiMesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(aiMesh, scene);
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(node->mChildren[i], scene);
    }
}

void ModelAsset::ProcessMesh(void* aiMeshPtr, const void* scenePtr) {
    struct aiMesh* aiMesh = static_cast<struct aiMesh*>(aiMeshPtr);

    (void)scenePtr;

    Mesh mesh;
    mesh.name = aiMesh->mName.C_Str();
    mesh.materialIndex = aiMesh->mMaterialIndex;

    mesh.boundsMin = math::Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    mesh.boundsMax = math::Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (unsigned int i = 0; i < aiMesh->mNumVertices; ++i) {
        Vertex vertex;

        vertex.position = AssimpToVec3(aiMesh->mVertices[i]);

        mesh.boundsMin.x = std::min(mesh.boundsMin.x, vertex.position.x);
        mesh.boundsMin.y = std::min(mesh.boundsMin.y, vertex.position.y);
        mesh.boundsMin.z = std::min(mesh.boundsMin.z, vertex.position.z);
        mesh.boundsMax.x = std::max(mesh.boundsMax.x, vertex.position.x);
        mesh.boundsMax.y = std::max(mesh.boundsMax.y, vertex.position.y);
        mesh.boundsMax.z = std::max(mesh.boundsMax.z, vertex.position.z);

        if (aiMesh->HasNormals()) {
            vertex.normal = AssimpToVec3(aiMesh->mNormals[i]);
        }

        if (aiMesh->HasTextureCoords(0)) {
            vertex.texCoord = AssimpToVec2(aiMesh->mTextureCoords[0][i]);
        }

        if (aiMesh->HasTangentsAndBitangents()) {
            vertex.tangent = AssimpToVec3(aiMesh->mTangents[i]);
            vertex.bitangent = AssimpToVec3(aiMesh->mBitangents[i]);
        }

        mesh.vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < aiMesh->mNumFaces; ++i) {
        aiFace face = aiMesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            mesh.indices.push_back(face.mIndices[j]);
        }
    }

    if (aiMesh->HasBones()) {
        ProcessBones(aiMesh, mesh);
    }

    m_Meshes.push_back(std::move(mesh));
}

void ModelAsset::ProcessBones(void* aiMeshPtr, Mesh& mesh) {
    struct aiMesh* aiMesh = static_cast<struct aiMesh*>(aiMeshPtr);

    for (unsigned int i = 0; i < aiMesh->mNumBones; ++i) {
        aiBone* aiBone = aiMesh->mBones[i];
        std::string boneName = aiBone->mName.C_Str();

        uint32_t boneIndex = 0;
        if (m_Skeleton.boneMap.find(boneName) == m_Skeleton.boneMap.end()) {

            boneIndex = static_cast<uint32_t>(m_Skeleton.bones.size());
            m_Skeleton.boneMap[boneName] = boneIndex;

            Bone bone;
            bone.name = boneName;
            bone.offsetMatrix = AssimpToMat4(aiBone->mOffsetMatrix);
            m_Skeleton.bones.push_back(bone);
        } else {
            boneIndex = m_Skeleton.boneMap[boneName];
        }

        for (unsigned int j = 0; j < aiBone->mNumWeights; ++j) {
            unsigned int vertexID = aiBone->mWeights[j].mVertexId;
            float weight = aiBone->mWeights[j].mWeight;

            if (vertexID >= mesh.vertices.size()) continue;

            Vertex& vertex = mesh.vertices[vertexID];

            for (int k = 0; k < 4; ++k) {
                if (vertex.boneWeights[k] == 0.0f) {
                    vertex.boneIDs[k] = static_cast<int>(boneIndex);
                    vertex.boneWeights[k] = weight;
                    break;
                }
            }
        }
    }

    // Normalize bone weights to ensure they sum to 1.0
    for (auto& vertex : mesh.vertices) {
        float totalWeight = vertex.boneWeights[0] + vertex.boneWeights[1] +
                           vertex.boneWeights[2] + vertex.boneWeights[3];

        if (totalWeight > 0.0f && std::abs(totalWeight - 1.0f) > 0.001f) {
            vertex.boneWeights[0] /= totalWeight;
            vertex.boneWeights[1] /= totalWeight;
            vertex.boneWeights[2] /= totalWeight;
            vertex.boneWeights[3] /= totalWeight;
        }
    }

    m_HasSkeleton = true;
}

void ModelAsset::ProcessSkeleton(const void* scenePtr) {
    const struct aiScene* scene = static_cast<const struct aiScene*>(scenePtr);

    if (!scene->mRootNode || m_Skeleton.bones.empty()) {
        return;
    }

    m_Skeleton.globalInverseTransform = AssimpToMat4(scene->mRootNode->mTransformation);
    m_Skeleton.globalInverseTransform = m_Skeleton.globalInverseTransform.Inverse();

    BuildBoneHierarchy(scene->mRootNode, -1);
}

void ModelAsset::BuildBoneHierarchy(void* nodePtr, int parentIndex) {
    struct aiNode* node = static_cast<struct aiNode*>(nodePtr);

    std::string nodeName = node->mName.C_Str();

    int currentIndex = -1;
    if (m_Skeleton.boneMap.find(nodeName) != m_Skeleton.boneMap.end()) {
        currentIndex = static_cast<int>(m_Skeleton.boneMap[nodeName]);
        m_Skeleton.bones[currentIndex].parentIndex = parentIndex;
        m_Skeleton.bones[currentIndex].localTransform = AssimpToMat4(node->mTransformation);
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        BuildBoneHierarchy(node->mChildren[i], currentIndex != -1 ? currentIndex : parentIndex);
    }
}

void ModelAsset::ProcessAnimations(const void* scenePtr) {
    const struct aiScene* scene = static_cast<const struct aiScene*>(scenePtr);

    if (!scene->HasAnimations()) {
        return;
    }

    for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
        aiAnimation* aiAnim = scene->mAnimations[i];

        AnimationClip clip;
        clip.name = aiAnim->mName.C_Str();
        if (clip.name.empty()) {
            clip.name = "Animation_" + std::to_string(i);
        }

        // Assimp stores animation times in "ticks", not seconds
        // We need to convert to seconds using ticksPerSecond
        float ticksPerSecond = static_cast<float>(aiAnim->mTicksPerSecond != 0 ? aiAnim->mTicksPerSecond : 25.0);
        clip.ticksPerSecond = ticksPerSecond;

        // Convert duration from ticks to seconds
        float durationInTicks = static_cast<float>(aiAnim->mDuration);
        clip.duration = durationInTicks / ticksPerSecond;

        for (unsigned int j = 0; j < aiAnim->mNumChannels; ++j) {
            aiNodeAnim* aiChannel = aiAnim->mChannels[j];

            AnimationChannel channel;
            channel.boneName = aiChannel->mNodeName.C_Str();

            // Convert all keyframe times from ticks to seconds
            for (unsigned int k = 0; k < aiChannel->mNumPositionKeys; ++k) {
                PositionKey key;
                key.time = static_cast<float>(aiChannel->mPositionKeys[k].mTime) / ticksPerSecond;
                key.value = AssimpToVec3(aiChannel->mPositionKeys[k].mValue);
                channel.positionKeys.push_back(key);
            }

            for (unsigned int k = 0; k < aiChannel->mNumRotationKeys; ++k) {
                RotationKey key;
                key.time = static_cast<float>(aiChannel->mRotationKeys[k].mTime) / ticksPerSecond;
                key.value = AssimpToQuat(aiChannel->mRotationKeys[k].mValue);
                channel.rotationKeys.push_back(key);
            }

            for (unsigned int k = 0; k < aiChannel->mNumScalingKeys; ++k) {
                ScaleKey key;
                key.time = static_cast<float>(aiChannel->mScalingKeys[k].mTime) / ticksPerSecond;
                key.value = AssimpToVec3(aiChannel->mScalingKeys[k].mValue);
                channel.scaleKeys.push_back(key);
            }

            clip.channels.push_back(std::move(channel));
        }

        m_Animations.push_back(std::move(clip));

    }
}

void ModelAsset::ProcessEmbeddedTextures(const void* scenePtr) {
    const aiScene* scene = static_cast<const aiScene*>(scenePtr);

    if (!scene->HasTextures()) {

        return;
    }

    for (unsigned int i = 0; i < scene->mNumTextures; ++i) {
        const aiTexture* aiTex = scene->mTextures[i];

        EmbeddedTexture embeddedTex;

        embeddedTex.name = "*" + std::to_string(i);

        if (aiTex->mHeight == 0) {

            embeddedTex.compressed = true;
            embeddedTex.width = aiTex->mWidth;
            embeddedTex.height = 0;

            const uint8_t* srcData = reinterpret_cast<const uint8_t*>(aiTex->pcData);
            embeddedTex.data.assign(srcData, srcData + aiTex->mWidth);

        } else {

            // aiTexel is 4 bytes; the product must be computed in 64-bit because
            // mWidth * mHeight * 4 wraps in 32-bit for very large embedded textures,
            // which would silently truncate the copy below.
            const uint64_t texelCount = static_cast<uint64_t>(aiTex->mWidth) *
                                        static_cast<uint64_t>(aiTex->mHeight);
            const uint64_t dataSize64 = texelCount * 4ull;

            if (texelCount == 0 || dataSize64 > kMaxEmbeddedTextureBytes ||
                dataSize64 > static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
                LOG_ERROR(LogCategory::Core,
                          "ModelAsset: skipping embedded texture {} with unusable dimensions {}x{}",
                          i, aiTex->mWidth, aiTex->mHeight);
                continue;
            }

            embeddedTex.compressed = false;
            embeddedTex.width = aiTex->mWidth;
            embeddedTex.height = aiTex->mHeight;

            const uint8_t* srcData = reinterpret_cast<const uint8_t*>(aiTex->pcData);
            const size_t dataSize = static_cast<size_t>(dataSize64);
            embeddedTex.data.assign(srcData, srcData + dataSize);

        }

        m_EmbeddedTextures.push_back(std::move(embeddedTex));
    }
}

const EmbeddedTexture* ModelAsset::GetEmbeddedTexture(const std::string& name) const {
    for (const auto& tex : m_EmbeddedTextures) {
        if (tex.name == name) {
            return &tex;
        }
    }
    return nullptr;
}

}
}


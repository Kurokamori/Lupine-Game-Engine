#include "lupine/core/Core.hpp"
#include "lupine/asset/Assets.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <filesystem>

using namespace lupine;
using namespace lupine::asset;

// Helper function to get the project root directory
static std::string GetProjectRoot() {
    // Get the current executable path
    std::filesystem::path exePath = std::filesystem::current_path();

    // Navigate up to find the project root (where debug_tools is located)
    // From build/bin/Debug -> go up 3 levels
    std::filesystem::path projectRoot = exePath.parent_path().parent_path().parent_path();

    return projectRoot.string();
}

#define PRINT_INFO(label, value) \
    std::cout << "  " << label << ": " << value << std::endl

void TestImageLoading() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "   Image Asset Loading Tests" << std::endl;
    std::cout << "========================================\n" << std::endl;

    LOG_INFO(LogCategory::Asset, "=== Testing Image Loading ===");

    // Test PNG loading
    {
        std::cout << "Test 1: Load PNG Image" << std::endl;

        std::string imagePath = GetProjectRoot() + "/debug_tools/Test.png";
        AssetRef<ImageAsset> image(new ImageAsset());
        bool loaded = image->LoadFromFile(imagePath, true, ImageColorSpace::sRGB);
        
        TEST_RESULT(loaded, "Image loaded successfully");
        TEST_RESULT(image->IsLoaded(), "Image marked as loaded");
        TEST_RESULT(image->GetWidth() > 0, "Image has valid width");
        TEST_RESULT(image->GetHeight() > 0, "Image has valid height");
        TEST_RESULT(image->GetChannels() > 0, "Image has valid channels");
        TEST_RESULT(image->GetMipLevels() > 0, "Image has mip levels");
        TEST_RESULT(image->GetData() != nullptr, "Image has valid data");
        
        if (loaded) {
            PRINT_INFO("Width", image->GetWidth());
            PRINT_INFO("Height", image->GetHeight());
            PRINT_INFO("Channels", image->GetChannels());
            PRINT_INFO("Mip Levels", image->GetMipLevels());
            PRINT_INFO("Has Alpha", (image->HasAlpha() ? "Yes" : "No"));
            PRINT_INFO("Color Space", (image->GetColorSpace() == ImageColorSpace::sRGB ? "sRGB" : "Linear"));
            PRINT_INFO("Data Size", image->GetDataSize());
            PRINT_INFO("UUID", image->GetUUID().ToString().c_str());
            PRINT_INFO("Ref Count", image->GetRefCount());
        }
    }

    // Test linear color space
    {
        std::cout << "\nTest 2: Load Image with Linear Color Space" << std::endl;

        std::string imagePath = GetProjectRoot() + "/debug_tools/Test.png";
        AssetRef<ImageAsset> image(new ImageAsset());
        bool loaded = image->LoadFromFile(imagePath, false, ImageColorSpace::Linear);
        
        TEST_RESULT(loaded, "Image loaded with linear color space");
        TEST_RESULT(image->GetColorSpace() == ImageColorSpace::Linear, "Color space is linear");
        TEST_RESULT(image->GetMipLevels() == 1, "No mipmaps generated");
    }

    std::cout << "\n========================================\n" << std::endl;
}

void TestModelLoading() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "   Model Asset Loading Tests" << std::endl;
    std::cout << "========================================\n" << std::endl;

    LOG_INFO(LogCategory::Asset, "=== Testing Model Loading ===");

    // Test FBX loading
    {
        std::cout << "Test 1: Load FBX Model" << std::endl;

        std::string modelPath = GetProjectRoot() + "/debug_tools/Test.fbx";
        AssetRef<ModelAsset> model(new ModelAsset());
        bool loaded = model->LoadFromFile(modelPath, true);
        
        TEST_RESULT(loaded, "Model loaded successfully");
        TEST_RESULT(model->IsLoaded(), "Model marked as loaded");
        TEST_RESULT(model->GetMeshCount() > 0, "Model has meshes");
        
        if (loaded) {
            PRINT_INFO("Mesh Count", model->GetMeshCount());
            PRINT_INFO("Material Count", model->GetMaterialCount());
            PRINT_INFO("Has Skeleton", (model->HasSkeleton() ? "Yes" : "No"));
            PRINT_INFO("Animation Count", model->GetAnimationCount());
            PRINT_INFO("UUID", model->GetUUID().ToString().c_str());
            PRINT_INFO("Ref Count", model->GetRefCount());
            
            // Print mesh details
            const auto& meshes = model->GetMeshes();
            for (size_t i = 0; i < meshes.size(); ++i) {
                std::cout << "\n  Mesh " << i << ":" << std::endl;
                PRINT_INFO("    Name", (meshes[i].name.empty() ? "(unnamed)" : meshes[i].name.c_str()));
                PRINT_INFO("    Vertices", meshes[i].vertices.size());
                PRINT_INFO("    Indices", meshes[i].indices.size());
                PRINT_INFO("    Triangles", meshes[i].indices.size() / 3);
                PRINT_INFO("    Material Index", meshes[i].materialIndex);
            }
            
            // Print material details
            const auto& materials = model->GetMaterials();
            for (size_t i = 0; i < materials.size(); ++i) {
                std::cout << "\n  Material " << i << ":" << std::endl;
                PRINT_INFO("    Name", (materials[i].name.empty() ? "(unnamed)" : materials[i].name.c_str()));
                PRINT_INFO("    Albedo Map", (materials[i].albedoMap.empty() ? "(none)" : materials[i].albedoMap.c_str()));
                PRINT_INFO("    Normal Map", (materials[i].normalMap.empty() ? "(none)" : materials[i].normalMap.c_str()));
            }
            
            // Print skeleton details
            if (model->HasSkeleton()) {
                const auto& skeleton = model->GetSkeleton();
                std::cout << "\n  Skeleton:" << std::endl;
                PRINT_INFO("    Bone Count", skeleton.bones.size());
                
                for (size_t i = 0; i < std::min(size_t(5), skeleton.bones.size()); ++i) {
                    std::cout << "    Bone " << i << ": " << skeleton.bones[i].name << std::endl;
                }
                if (skeleton.bones.size() > 5) {
                    std::cout << "    ... and " << (skeleton.bones.size() - 5) << " more bones" << std::endl;
                }
            }
            
            // Print animation details
            if (model->HasAnimations()) {
                const auto& animations = model->GetAnimations();
                for (size_t i = 0; i < animations.size(); ++i) {
                    std::cout << "\n  Animation " << i << ":" << std::endl;
                    PRINT_INFO("    Name", animations[i].name);
                    PRINT_INFO("    Duration", animations[i].duration);
                    PRINT_INFO("    Ticks Per Second", animations[i].ticksPerSecond);
                    PRINT_INFO("    Channels", animations[i].channels.size());
                }
            }
        }
    }

    std::cout << "\n========================================\n" << std::endl;
}

void TestFontLoading() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "   Font Asset Loading Tests" << std::endl;
    std::cout << "========================================\n" << std::endl;

    LOG_INFO(LogCategory::Asset, "=== Testing Font Loading ===");

    // Test TTF loading
    {
        std::cout << "Test 1: Load TTF Font" << std::endl;

        std::string fontPath = GetProjectRoot() + "/debug_tools/Test.ttf";
        AssetRef<FontAsset> font(new FontAsset());
        bool loaded = font->LoadFromFile(fontPath, 48.0f, 512, 512);
        
        TEST_RESULT(loaded, "Font loaded successfully");
        TEST_RESULT(font->IsLoaded(), "Font marked as loaded");
        
        if (loaded) {
            PRINT_INFO("Font Size", font->GetFontSize());
            PRINT_INFO("Line Height", font->GetLineHeight());
            PRINT_INFO("Ascent", font->GetAscent());
            PRINT_INFO("Descent", font->GetDescent());
            PRINT_INFO("Atlas Width", font->GetAtlasWidth());
            PRINT_INFO("Atlas Height", font->GetAtlasHeight());
            PRINT_INFO("Atlas Data Size", font->GetAtlasDataSize());
            PRINT_INFO("Glyph Count", font->GetAllGlyphs().size());
            PRINT_INFO("UUID", font->GetUUID().ToString());
            PRINT_INFO("Ref Count", font->GetRefCount());
            
            // Test specific glyphs
            TEST_RESULT(font->HasGlyph('A'), "Has glyph 'A'");
            TEST_RESULT(font->HasGlyph('a'), "Has glyph 'a'");
            TEST_RESULT(font->HasGlyph('0'), "Has glyph '0'");
            
            // Qualified: lupine::Glyph (renderer) and lupine::asset::Glyph are distinct
            // types with different metric conventions, and both are in scope here.
            const asset::Glyph* glyphA = font->GetGlyph('A');
            if (glyphA) {
                std::cout << "\n  Glyph 'A' Details:" << std::endl;
                PRINT_INFO("    Advance", glyphA->advance);
                PRINT_INFO("    Width", glyphA->width);
                PRINT_INFO("    Height", glyphA->height);
                PRINT_INFO("    Bearing X", glyphA->bearingX);
                PRINT_INFO("    Bearing Y", glyphA->bearingY);
            }
        }
    }

    std::cout << "\n========================================\n" << std::endl;
}

// Main test runner
void RunAllAssetTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "   Lupine Asset Loading Test Suite" << std::endl;
    std::cout << "========================================\n" << std::endl;

    lupine_test::SetCurrentSuite("Asset Loading");

    // Initialize logger
    lupine::Logger::Init("lupine_asset_tests.log", true);
    LOG_INFO(LogCategory::Asset, "=== Starting Asset Loading Tests ===");

    // Initialize asset module
    asset::InitializeAssets();

    TestImageLoading();
    TestModelLoading();
    TestFontLoading();

    // Shutdown asset module
    asset::ShutdownAssets();

    std::cout << "\n========================================" << std::endl;
    std::cout << "   All Asset Tests Complete!" << std::endl;
    std::cout << "========================================\n" << std::endl;

    LOG_INFO(LogCategory::Asset, "=== Asset Loading Tests Complete ===");
}


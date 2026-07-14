#include "lupine/engine/Engine.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/core/Node.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace lupine;
using namespace lupine::core;

namespace {

std::string ScenePath() {
    std::filesystem::path p = std::filesystem::temp_directory_path() / "lupine_test_scene.scene";
    return p.string();
}

std::shared_ptr<Scene> BuildScene() {
    std::shared_ptr<Scene> scene = std::make_shared<Scene>("TestScene");
    std::shared_ptr<Node2D> root = std::make_shared<Node2D>("Root");
    std::shared_ptr<Node2D> player = std::make_shared<Node2D>("Player");
    std::shared_ptr<Node> weapon = std::make_shared<Node>("Weapon");
    std::shared_ptr<Node2D> enemy = std::make_shared<Node2D>("Enemy");

    player->SetPosition(math::Vec2(64.0f, 32.0f));
    player->AddToGroup("actors");
    enemy->AddToGroup("actors");
    enemy->AddToGroup("enemies");

    scene->SetRoot(root);
    root->AddChild(player);
    player->AddChild(weapon);
    root->AddChild(enemy);

    return scene;
}

bool TestSceneStructure() {
    TEST_SECTION("Scene Structure Tests");

    std::shared_ptr<Scene> scene = BuildScene();
    TEST_ASSERT(scene->GetName() == "TestScene", "Scene keeps its name");
    TEST_ASSERT(scene->GetRoot() != nullptr, "Scene has a root");
    TEST_ASSERT(scene->GetRoot()->GetName() == "Root", "Root node name is correct");

    std::shared_ptr<Node> player = scene->FindNode("Player");
    TEST_ASSERT(player != nullptr, "FindNode resolves a direct child of root");
    TEST_ASSERT(scene->FindNode("Player/Weapon") != nullptr,
        "FindNode resolves a nested path");
    TEST_ASSERT(scene->FindNode("Nope") == nullptr, "FindNode returns null for a missing node");

    return true;
}

bool TestFindByUUID() {
    TEST_SECTION("Find By UUID Tests");

    std::shared_ptr<Scene> scene = BuildScene();
    std::shared_ptr<Node> player = scene->FindNode("Player");
    TEST_ASSERT(player != nullptr, "Player exists");

    std::shared_ptr<Node> found = scene->FindNodeByUUID(player->GetUUID());
    TEST_ASSERT(found == player, "FindNodeByUUID returns the matching node");

    TEST_ASSERT(scene->FindNodeByUUID(UUID(0xDEADBEEF)) == nullptr,
        "FindNodeByUUID returns null for an unknown UUID");

    return true;
}

bool TestGroups() {
    TEST_SECTION("Scene Group Tests");

    std::shared_ptr<Scene> scene = BuildScene();

    std::vector<Node*> actors = scene->GetNodesInGroup("actors");
    TEST_ASSERT(actors.size() == 2, "Two nodes are in the 'actors' group");

    std::vector<Node*> enemies = scene->GetNodesInGroup("enemies");
    TEST_ASSERT(enemies.size() == 1, "One node is in the 'enemies' group");
    TEST_ASSERT(enemies[0]->GetName() == "Enemy", "The enemy node is the group member");

    std::vector<Node*> none = scene->GetNodesInGroup("missing");
    TEST_ASSERT(none.empty(), "An unknown group has no members");

    return true;
}

bool TestSaveLoadRoundTrip() {
    TEST_SECTION("Scene Save / Load Tests");

    const std::string path = ScenePath();
    std::error_code ec;
    std::filesystem::remove(path, ec);

    std::shared_ptr<Scene> scene = BuildScene();
    std::shared_ptr<Node> player = scene->FindNode("Player");
    UUID playerUuid = player->GetUUID();

    bool saved = scene->Save(path);
    TEST_ASSERT(saved, "Scene saves to disk");
    TEST_ASSERT(std::filesystem::exists(path), "Scene file exists after save");

    std::shared_ptr<Scene> loaded = std::make_shared<Scene>();
    bool ok = loaded->Load(path);
    TEST_ASSERT(ok, "Scene loads from disk");
    TEST_ASSERT(loaded->GetRoot() != nullptr, "Loaded scene has a root");
    TEST_ASSERT(loaded->GetRoot()->GetName() == "Root", "Loaded root name matches");

    std::shared_ptr<Node> loadedPlayer = loaded->FindNode("Player");
    TEST_ASSERT(loadedPlayer != nullptr, "Player restored after load");
    TEST_ASSERT(loadedPlayer->GetUUID() == playerUuid, "Player UUID preserved across save/load");
    TEST_ASSERT(loaded->FindNode("Player/Weapon") != nullptr, "Nested weapon restored after load");

    std::shared_ptr<Node2D> loadedPlayer2D = std::dynamic_pointer_cast<Node2D>(loadedPlayer);
    TEST_ASSERT(loadedPlayer2D != nullptr, "Loaded player is a Node2D");
    TEST_ASSERT(math::Equals(loadedPlayer2D->GetPosition().x, 64.0f) &&
                math::Equals(loadedPlayer2D->GetPosition().y, 32.0f),
        "Player transform preserved across save/load");

    std::vector<Node*> loadedActors = loaded->GetNodesInGroup("actors");
    TEST_ASSERT(loadedActors.size() == 2, "Group membership preserved across save/load");

    std::filesystem::remove(path, ec);
    return true;
}

bool TestLoadMissingFile() {
    TEST_SECTION("Load Missing File Tests");

    std::shared_ptr<Scene> scene = std::make_shared<Scene>();
    bool ok = scene->Load((std::filesystem::temp_directory_path() / "no_such_scene.scene").string());
    TEST_ASSERT(!ok, "Loading a non-existent scene file fails gracefully");

    return true;
}

} // namespace

void RunSceneTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "SCENE TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Scene");

    engine::InitializeEngine();

    bool allPassed = true;

    allPassed &= TestSceneStructure();
    allPassed &= TestFindByUUID();
    allPassed &= TestGroups();
    allPassed &= TestSaveLoadRoundTrip();
    allPassed &= TestLoadMissingFile();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL SCENE TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}

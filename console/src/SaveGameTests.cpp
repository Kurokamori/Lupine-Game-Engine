#include "lupine/engine/Engine.hpp"
#include "lupine/core/Node.hpp"
#include "lupine/core/Scene.hpp"
#include "lupine/save/SaveData.hpp"
#include "lupine/save/SaveFormat.hpp"
#include "lupine/save/SaveMigration.hpp"
#include "lupine/save/SaveGameManager.hpp"
#include "lupine/save/SceneSaveState.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <memory>
#include <string>

using namespace lupine;
using namespace lupine::save;

namespace {

const char* kTestDir = "user://console_savetests";

void ResetManager() {
    SaveGameManager& mgr = SaveGameManager::GetInstance();
    mgr.SetSaveDirectory(kTestDir);
    mgr.SetSchemaVersion(1);
    mgr.SetFormatType(SaveFormatType::JsonText);
    mgr.SetTransform(nullptr);
    mgr.SetBackupOnWrite(true);
    mgr.SetAtomicWrite(true);
    mgr.SetPreSaveHook(nullptr);
    mgr.SetPostLoadHook(nullptr);
    mgr.Migrations().Clear();
}

bool TestSaveDataAccessors() {
    TEST_SECTION("SaveData Typed Accessors");

    SaveData data;
    data.SetInt("player/level", 12);
    data.SetDouble("player/health", 73.5);
    data.SetBool("flags/intro", true);
    data.SetString("player/name", "Hero");
    data.SetVec2("player/pos2d", math::Vec2(10.0f, 20.0f));
    data.SetVec3("player/pos3d", math::Vec3(1.0f, 2.0f, 3.0f));
    data.SetColor("ui/tint", math::Color(0.1f, 0.2f, 0.3f, 0.4f));

    TEST_ASSERT(data.GetInt("player/level") == 12, "nested int round-trips");
    TEST_ASSERT(math::Equals(static_cast<float>(data.GetDouble("player/health")), 73.5f),
        "nested double round-trips");
    TEST_ASSERT(data.GetBool("flags/intro"), "nested bool round-trips");
    TEST_ASSERT(data.GetString("player/name") == "Hero", "nested string round-trips");

    math::Vec2 p2 = data.GetVec2("player/pos2d");
    TEST_ASSERT(math::Equals(p2.x, 10.0f) && math::Equals(p2.y, 20.0f), "Vec2 round-trips");
    math::Vec3 p3 = data.GetVec3("player/pos3d");
    TEST_ASSERT(math::Equals(p3.x, 1.0f) && math::Equals(p3.z, 3.0f), "Vec3 round-trips");
    math::Color c = data.GetColor("ui/tint");
    TEST_ASSERT(math::Equals(c.a, 0.4f), "Color round-trips");

    TEST_ASSERT(data.GetInt("player/missing", 99) == 99, "missing key returns default");
    TEST_ASSERT(data.Has("player/level"), "Has() true for present nested key");
    TEST_ASSERT(!data.Has("player/nope"), "Has() false for absent key");

    data.Append("log", "a");
    data.Append("log", "b");
    TEST_ASSERT(data.ArraySize("log") == 2, "Append builds an array");

    SaveData section = data.GetSection("player");
    TEST_ASSERT(section.GetInt("level") == 12, "GetSection returns nested document");

    data.Erase("player/level");
    TEST_ASSERT(!data.Has("player/level"), "Erase removes nested key");

    bool ok = false;
    SaveData parsed = SaveData::Parse(data.ToString(), &ok);
    TEST_ASSERT(ok, "ToString/Parse round-trips");
    TEST_ASSERT(parsed.GetString("player/name") == "Hero", "parsed data retains values");

    return true;
}

bool TestFormatsRoundTrip() {
    TEST_SECTION("SaveData Format Round-Trips");

    const SaveFormatType formats[] = {
        SaveFormatType::JsonText,
        SaveFormatType::JsonCompact,
        SaveFormatType::BinaryCbor,
        SaveFormatType::BinaryMsgpack
    };

    bool allOk = true;
    for (SaveFormatType type : formats) {
        std::shared_ptr<ISaveFormat> format = CreateSaveFormat(type);
        nlohmann::json envelope = {
            {"magic", "LUPINE_SAVE"},
            {"schema_version", 1},
            {"data", {{"score", 4242}, {"name", "round"}}}
        };
        std::vector<uint8_t> bytes;
        bool encoded = format->Encode(envelope, bytes);
        TEST_ASSERT(encoded && !bytes.empty(),
            std::string("Encode produces bytes for ") + SaveFormatTypeToString(type));

        nlohmann::json decoded;
        bool ok = format->Decode(bytes, decoded);
        bool match = ok && decoded.is_object() && decoded["data"]["score"].get<int>() == 4242;
        TEST_ASSERT(match, std::string("Decode round-trips for ") + SaveFormatTypeToString(type));
        allOk &= match;
    }

    XorObfuscationTransform xform("a-key");
    std::vector<uint8_t> plain = {1, 2, 3, 250, 255, 0};
    std::vector<uint8_t> hidden;
    std::vector<uint8_t> revealed;
    xform.Apply(plain, hidden);
    xform.Revert(hidden, revealed);
    TEST_ASSERT(hidden != plain, "XOR transform changes the bytes");
    TEST_ASSERT(revealed == plain, "XOR transform reverses cleanly");

    return allOk;
}

bool TestSlotSaveLoad() {
    TEST_SECTION("SaveGameManager Save/Load");

    ResetManager();
    SaveGameManager& mgr = SaveGameManager::GetInstance();

    SaveData data;
    data.SetInt("score", 1500);
    data.SetString("zone", "cavern");

    SaveSlotMeta meta;
    meta.title = "Chapter 1";
    meta.playtimeSeconds = 360.0;

    TEST_ASSERT(mgr.Save("slot1", data, meta) == SaveResult::Success, "Save() succeeds");
    TEST_ASSERT(mgr.SlotExists("slot1"), "SlotExists() true after save");

    SaveData loaded;
    SaveSlotInfo info;
    SaveResult result = mgr.Load("slot1", loaded, info);
    TEST_ASSERT(result == SaveResult::Success, "Load() succeeds");
    TEST_ASSERT(loaded.GetInt("score") == 1500, "loaded score round-trips");
    TEST_ASSERT(loaded.GetString("zone") == "cavern", "loaded string round-trips");
    TEST_ASSERT(info.title == "Chapter 1", "slot header carries title");
    TEST_ASSERT(info.payloadBytes > 0, "slot reports a non-zero payload size");

    SaveData missing;
    TEST_ASSERT(mgr.Load("does_not_exist", missing) == SaveResult::SlotNotFound,
        "Load() reports SlotNotFound for absent slot");

    std::vector<std::string> slots = mgr.ListSlots();
    bool found = false;
    for (const std::string& s : slots) {
        if (s == "slot1") found = true;
    }
    TEST_ASSERT(found, "ListSlots() includes the saved slot");

    TEST_ASSERT(mgr.CopySlot("slot1", "slot2") == SaveResult::Success, "CopySlot() succeeds");
    TEST_ASSERT(mgr.SlotExists("slot2"), "copy creates destination slot");
    TEST_ASSERT(mgr.RenameSlot("slot2", "slot3") == SaveResult::Success, "RenameSlot() succeeds");
    TEST_ASSERT(!mgr.SlotExists("slot2"), "rename removes source slot");
    TEST_ASSERT(mgr.SlotExists("slot3"), "rename creates destination slot");

    TEST_ASSERT(mgr.DeleteSlot("slot1") == SaveResult::Success, "DeleteSlot() succeeds");
    TEST_ASSERT(!mgr.SlotExists("slot1"), "deleted slot no longer exists");
    mgr.DeleteSlot("slot3");

    return true;
}

bool TestQuickAndObfuscated() {
    TEST_SECTION("SaveGameManager Quick/Auto + Obfuscation");

    ResetManager();
    SaveGameManager& mgr = SaveGameManager::GetInstance();

    SaveData data;
    data.SetInt("checkpoint", 9);

    TEST_ASSERT(mgr.QuickSave(data) == SaveResult::Success, "QuickSave() succeeds");
    TEST_ASSERT(mgr.HasQuickSave(), "HasQuickSave() true after quick save");
    SaveData quick;
    TEST_ASSERT(mgr.QuickLoad(quick) == SaveResult::Success, "QuickLoad() succeeds");
    TEST_ASSERT(quick.GetInt("checkpoint") == 9, "quick load round-trips");

    mgr.SetTransform(std::make_shared<XorObfuscationTransform>("p@ss"));
    TEST_ASSERT(mgr.Save("obf", data) == SaveResult::Success, "obfuscated Save() succeeds");
    SaveData obfLoaded;
    TEST_ASSERT(mgr.Load("obf", obfLoaded) == SaveResult::Success, "obfuscated Load() succeeds");
    TEST_ASSERT(obfLoaded.GetInt("checkpoint") == 9, "obfuscated save round-trips");
    mgr.SetTransform(nullptr);

    mgr.DeleteSlot(mgr.GetQuickSaveSlot());
    mgr.DeleteSlot("obf");

    return true;
}

bool TestMigration() {
    TEST_SECTION("SaveGameManager Schema Migration");

    ResetManager();
    SaveGameManager& mgr = SaveGameManager::GetInstance();

    mgr.SetSchemaVersion(1);
    SaveData oldData;
    oldData.SetInt("hp", 100);
    TEST_ASSERT(mgr.Save("migrate", oldData) == SaveResult::Success, "save at schema 1");

    mgr.SetSchemaVersion(2);
    mgr.Migrations().Register(1, 2, [](nlohmann::json& payload) {
        if (payload.contains("hp")) {
            payload["health"] = payload["hp"];
            payload.erase("hp");
        }
        payload["mana"] = 50;
        return true;
    }, "rename hp->health, add mana");

    SaveData migrated;
    SaveResult result = mgr.Load("migrate", migrated);
    TEST_ASSERT(result == SaveResult::Success, "load triggers migration");
    TEST_ASSERT(migrated.GetInt("health") == 100, "migration renamed hp to health");
    TEST_ASSERT(!migrated.Has("hp"), "migration removed old key");
    TEST_ASSERT(migrated.GetInt("mana") == 50, "migration added new key");

    mgr.SetSchemaVersion(3);
    SaveData blocked;
    TEST_ASSERT(mgr.Load("migrate", blocked) == SaveResult::MigrationFailed,
        "missing migration step fails the load");

    mgr.DeleteSlot("migrate");
    mgr.Migrations().Clear();
    mgr.SetSchemaVersion(1);

    return true;
}

bool TestSceneStateCapture() {
    TEST_SECTION("SceneSaveState Capture/Restore");

    std::shared_ptr<core::Scene> scene = std::make_shared<core::Scene>("CaptureScene");
    std::shared_ptr<core::Node> root = std::make_shared<core::Node>("Root");
    std::shared_ptr<core::Node2D> mover = std::make_shared<core::Node2D>("Mover");
    mover->SetPosition(math::Vec2(5.0f, 6.0f));
    mover->AddToGroup("persistent");
    root->AddChild(mover);
    scene->SetRoot(root);

    nlohmann::json captured = SceneSaveState::CaptureGroup(scene.get(), "persistent");
    TEST_ASSERT(captured.is_array() && captured.size() == 1, "captures one persistent node");

    mover->SetPosition(math::Vec2(99.0f, 99.0f));
    int restored = SceneSaveState::RestoreGroup(scene.get(), captured);
    TEST_ASSERT(restored == 1, "restores one node");
    TEST_ASSERT(math::Equals(mover->GetPosition().x, 5.0f) && math::Equals(mover->GetPosition().y, 6.0f),
        "node position restored from captured state");

    SaveData data;
    SceneSaveState::CaptureGroupInto(data, "scene_state", scene.get(), "persistent");
    TEST_ASSERT(data.HasSection("scene_state") || data.Has("scene_state"),
        "CaptureGroupInto writes a key into SaveData");

    return true;
}

} // namespace

void RunSaveGameTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "SAVE GAME TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("SaveGame");

    engine::InitializeEngine();

    bool allPassed = true;

    allPassed &= TestSaveDataAccessors();
    allPassed &= TestFormatsRoundTrip();
    allPassed &= TestSlotSaveLoad();
    allPassed &= TestQuickAndObfuscated();
    allPassed &= TestMigration();
    allPassed &= TestSceneStateCapture();

    ResetManager();
    SaveGameManager::GetInstance().SetSaveDirectory("user://saves");

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL SAVE GAME TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}

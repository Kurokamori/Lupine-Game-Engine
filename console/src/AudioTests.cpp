/**
 * @file AudioTests.cpp
 * @brief Headless console tests for the audio system (lupine::audio::AudioManager).
 *
 * Actual sample playback needs the miniaudio device, but every backend call in
 * AudioManager is guarded by `if (!m_Engine) ...`, so the entire data/config layer
 * runs safely without ever opening audio hardware. These tests therefore drive the
 * manager WITHOUT calling Initialize() and verify the bus hierarchy, listener,
 * master controls, source/bus structs and JSON serialization — the parts a project
 * configures and that must be correct independently of the audio device.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/audio/AudioManager.hpp"
#include "TestFramework.hpp"
#include <iostream>
#include <cmath>

using namespace lupine;
using namespace lupine::audio;

namespace {

bool FloatEq(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) < tol;
}

bool TestBuses() {
    TEST_SECTION("Audio: Bus Management");

    AudioManager& audio = AudioManager::GetInstance();

    audio.CreateBus("TestSFX");
    TEST_ASSERT(audio.HasBus("TestSFX"), "Bus created and reported by HasBus");
    AudioBus* bus = audio.GetBus("TestSFX");
    TEST_ASSERT(bus != nullptr && bus->name == "TestSFX", "GetBus returns the created bus");

    audio.SetBusVolume("TestSFX", 0.5f);
    TEST_ASSERT(FloatEq(audio.GetBusVolume("TestSFX"), 0.5f), "Bus volume round-trips");

    audio.SetBusMuted("TestSFX", true);
    TEST_ASSERT(audio.IsBusMuted("TestSFX"), "Bus mute flag round-trips");
    TEST_ASSERT(FloatEq(audio.GetBus("TestSFX")->GetEffectiveVolume(), 0.0f),
                "Muted bus reports zero effective volume");
    audio.SetBusMuted("TestSFX", false);
    TEST_ASSERT(FloatEq(audio.GetBus("TestSFX")->GetEffectiveVolume(), 0.5f),
                "Unmuted bus reports its volume as effective volume");

    audio.SetBusSolo("TestSFX", true);
    TEST_ASSERT(audio.IsBusSolo("TestSFX"), "Bus solo flag round-trips");
    audio.SetBusSolo("TestSFX", false);

    audio.DestroyBus("TestSFX");
    TEST_ASSERT(!audio.HasBus("TestSFX"), "Bus removed by DestroyBus");

    return true;
}

bool TestBusHierarchy() {
    TEST_SECTION("Audio: Bus Hierarchy");

    AudioManager& audio = AudioManager::GetInstance();

    audio.CreateBus("TestParent");
    audio.CreateBus("TestChild", "TestParent");

    AudioBus* child = audio.GetBus("TestChild");
    TEST_ASSERT(child != nullptr && child->parentBus == "TestParent",
                "Child bus records its parent bus");

    audio.DestroyBus("TestChild");
    audio.DestroyBus("TestParent");
    TEST_ASSERT(!audio.HasBus("TestChild") && !audio.HasBus("TestParent"),
                "Hierarchy buses removed cleanly");

    return true;
}

bool TestListener() {
    TEST_SECTION("Audio: 3D Listener");

    AudioManager& audio = AudioManager::GetInstance();

    audio.SetListenerPosition(math::Vec3(5.0f, 1.0f, -2.0f));
    audio.SetListenerOrientation(math::Vec3(0.0f, 0.0f, 1.0f), math::Vec3(0.0f, 1.0f, 0.0f));
    audio.SetListenerVelocity(math::Vec3(1.0f, 0.0f, 0.0f));

    const AudioListener& listener = audio.GetListener();
    TEST_ASSERT(FloatEq(listener.position.x, 5.0f) && FloatEq(listener.position.z, -2.0f),
                "Listener position round-trips");
    TEST_ASSERT(FloatEq(listener.forward.z, 1.0f), "Listener forward orientation round-trips");
    TEST_ASSERT(FloatEq(listener.up.y, 1.0f), "Listener up orientation round-trips");
    TEST_ASSERT(FloatEq(listener.velocity.x, 1.0f), "Listener velocity round-trips");

    return true;
}

bool TestMasterControls() {
    TEST_SECTION("Audio: Master Controls");

    AudioManager& audio = AudioManager::GetInstance();

    audio.SetMasterVolume(0.3f);
    TEST_ASSERT(FloatEq(audio.GetMasterVolume(), 0.3f), "Master volume round-trips");

    audio.SetMasterMuted(true);
    TEST_ASSERT(audio.IsMasterMuted(), "Master mute round-trips");
    audio.SetMasterMuted(false);
    TEST_ASSERT(!audio.IsMasterMuted(), "Master unmute round-trips");

    audio.SetMasterVolume(1.0f);
    return true;
}

bool TestStructDefaults() {
    TEST_SECTION("Audio: Source / Bus Struct Defaults");

    AudioSource source;
    TEST_ASSERT(source.state == PlaybackState::Stopped, "New source defaults to Stopped");
    TEST_ASSERT(source.mode == PlaybackMode::OneShot, "New source defaults to OneShot");
    TEST_ASSERT(FloatEq(source.volume, 1.0f) && FloatEq(source.pitch, 1.0f), "Source volume/pitch default to 1.0");
    TEST_ASSERT(!source.is3D, "Source defaults to non-3D");
    TEST_ASSERT(source.busName == "Master", "Source defaults to the Master bus");

    AudioBus bus;
    bus.volume = 0.8f;
    bus.muted = false;
    TEST_ASSERT(FloatEq(bus.GetEffectiveVolume(), 0.8f), "Unmuted bus effective volume is its volume");
    bus.muted = true;
    TEST_ASSERT(FloatEq(bus.GetEffectiveVolume(), 0.0f), "Muted bus effective volume is zero");

    return true;
}

bool TestSerialization() {
    TEST_SECTION("Audio: Bus Serialization Round-Trip");

    AudioManager& audio = AudioManager::GetInstance();

    audio.CreateBus("TestPersistA");
    audio.SetBusVolume("TestPersistA", 0.42f);
    audio.SetBusMuted("TestPersistA", true);
    audio.CreateBus("TestPersistB", "TestPersistA");

    nlohmann::json json = audio.Serialize();
    TEST_ASSERT(json.contains("buses") && json["buses"].is_array(),
                "Serialization contains a buses array");

    audio.DestroyBus("TestPersistA");
    audio.DestroyBus("TestPersistB");
    TEST_ASSERT(!audio.HasBus("TestPersistA"), "Buses cleared before deserialize");

    audio.Deserialize(json);
    TEST_ASSERT(audio.HasBus("TestPersistA"), "Deserialize restores bus A");
    TEST_ASSERT(audio.HasBus("TestPersistB"), "Deserialize restores bus B");
    TEST_ASSERT(FloatEq(audio.GetBusVolume("TestPersistA"), 0.42f), "Deserialized bus volume preserved");
    TEST_ASSERT(audio.IsBusMuted("TestPersistA"), "Deserialized bus mute preserved");
    AudioBus* restoredChild = audio.GetBus("TestPersistB");
    TEST_ASSERT(restoredChild != nullptr && restoredChild->parentBus == "TestPersistA",
                "Deserialized bus parent preserved");

    audio.DestroyBus("TestPersistA");
    audio.DestroyBus("TestPersistB");
    return true;
}

bool TestUpdateNoCrash() {
    TEST_SECTION("Audio: Update Without Device");

    AudioManager& audio = AudioManager::GetInstance();
    TEST_ASSERT(!audio.IsInitialized(), "Audio manager is uninitialized in headless tests");

    audio.Update(0.016f);
    audio.StopAll();
    TEST_ASSERT(true, "Update/StopAll are safe no-ops without an audio device");

    return true;
}

} // namespace

void RunAudioTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "AUDIO TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("Audio");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestBuses();
    allPassed &= TestBusHierarchy();
    allPassed &= TestListener();
    allPassed &= TestMasterControls();
    allPassed &= TestStructDefaults();
    allPassed &= TestSerialization();
    allPassed &= TestUpdateNoCrash();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL AUDIO TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}

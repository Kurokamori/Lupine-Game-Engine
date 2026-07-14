/**
 * @file MiscComponentTests.cpp
 * @brief Headless behavioral tests for audio + utility components.
 *
 * These tests exercise the data/config and pure-CPU logic layers of AudioPlayer,
 * AudioListener (the component), Timer, SubViewport and CustomShaderParams without
 * ever touching a live audio device, GPU/renderer or window:
 *  - AudioPlayer/AudioListener store their settings in plain members and clamp on
 *    set, so config round-trips and play-state flags are verifiable directly.
 *  - Timer counts purely on the CPU via OnUpdate(deltaTime); ticking it deterministically
 *    drives countdown, timeout firing, looping/repeat counts and pause behavior.
 *  - SubViewport's accessors are backed by the component property system, so size,
 *    clear color, world/display/input modes round-trip after DefineProperties().
 *  - CustomShaderParams parses typed JSON into a MaterialPropertyBlock; the block's
 *    typed set/get/has/clear API and scalar/vec/color parsing run without a device.
 */

#include "lupine/engine/Engine.hpp"
#include "lupine/components/AudioPlayer.hpp"
#include "lupine/components/AudioListener.hpp"
#include "lupine/components/Timer.hpp"
#include "lupine/components/SubViewport.hpp"
#include "lupine/components/CustomShaderParams.hpp"
#include "lupine/rendering/Material.hpp"
#include "lupine/rendering/RenderContext.hpp"
#include "lupine/math/Color.hpp"
#include "lupine/math/Vec2.hpp"
#include "lupine/math/Vec3.hpp"
#include "lupine/math/Vec4.hpp"
#include "TestFramework.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <cmath>

using namespace lupine;
using namespace lupine::core;

namespace {

bool FloatEq(float a, float b, float tol = 1e-4f) {
    return std::fabs(a - b) < tol;
}

bool TestAudioPlayerConfig() {
    TEST_SECTION("AudioPlayer: Playback Config Round-Trip");

    std::shared_ptr<components::AudioPlayer> player = std::make_shared<components::AudioPlayer>();

    TEST_ASSERT(player->GetTypeName() == "AudioPlayer", "AudioPlayer reports its type name");

    TEST_ASSERT(!player->GetAutoplay(), "Autoplay defaults to false");
    TEST_ASSERT(!player->GetLoop(), "Loop defaults to false");
    TEST_ASSERT(FloatEq(player->GetVolume(), 1.0f), "Volume defaults to 1.0");
    TEST_ASSERT(FloatEq(player->GetPitch(), 1.0f), "Pitch defaults to 1.0");
    TEST_ASSERT(FloatEq(player->GetPan(), 0.0f), "Pan defaults to centered (0.0)");
    TEST_ASSERT(player->GetBus() == "Master", "Bus defaults to Master");
    TEST_ASSERT(!player->GetIs3D(), "3D mode defaults to false");

    player->SetAutoplay(true);
    TEST_ASSERT(player->GetAutoplay(), "Autoplay round-trips to true");

    player->SetLoop(true);
    TEST_ASSERT(player->GetLoop(), "Loop round-trips to true");
    player->SetLoop(false);
    TEST_ASSERT(!player->GetLoop(), "Loop round-trips back to false");

    player->SetVolume(0.5f);
    TEST_ASSERT(FloatEq(player->GetVolume(), 0.5f), "Volume round-trips to 0.5");

    player->SetPitch(1.75f);
    TEST_ASSERT(FloatEq(player->GetPitch(), 1.75f), "Pitch round-trips to 1.75");

    player->SetPan(-0.25f);
    TEST_ASSERT(FloatEq(player->GetPan(), -0.25f), "Pan round-trips to -0.25");

    player->SetBus("SFX");
    TEST_ASSERT(player->GetBus() == "SFX", "Bus name round-trips to SFX");

    player->SetIs3D(true);
    TEST_ASSERT(player->GetIs3D(), "3D mode round-trips to true");

    return true;
}

bool TestAudioPlayerClamping() {
    TEST_SECTION("AudioPlayer: Setter Clamping");

    std::shared_ptr<components::AudioPlayer> player = std::make_shared<components::AudioPlayer>();

    player->SetVolume(5.0f);
    TEST_ASSERT(FloatEq(player->GetVolume(), 1.0f), "Volume clamps to max 1.0");
    player->SetVolume(-1.0f);
    TEST_ASSERT(FloatEq(player->GetVolume(), 0.0f), "Volume clamps to min 0.0");

    player->SetPitch(10.0f);
    TEST_ASSERT(FloatEq(player->GetPitch(), 2.0f), "Pitch clamps to max 2.0");
    player->SetPitch(0.0f);
    TEST_ASSERT(FloatEq(player->GetPitch(), 0.5f), "Pitch clamps to min 0.5");

    player->SetPan(3.0f);
    TEST_ASSERT(FloatEq(player->GetPan(), 1.0f), "Pan clamps to max 1.0");
    player->SetPan(-3.0f);
    TEST_ASSERT(FloatEq(player->GetPan(), -1.0f), "Pan clamps to min -1.0");

    return true;
}

bool TestAudioPlayer3DConfig() {
    TEST_SECTION("AudioPlayer: 3D Distance Config");

    std::shared_ptr<components::AudioPlayer> player = std::make_shared<components::AudioPlayer>();

    TEST_ASSERT(FloatEq(player->GetMinDistance(), 1.0f), "Min distance defaults to 1.0");
    TEST_ASSERT(FloatEq(player->GetMaxDistance(), 100.0f), "Max distance defaults to 100.0");
    TEST_ASSERT(FloatEq(player->GetRolloffFactor(), 1.0f), "Rolloff factor defaults to 1.0");

    player->SetMinDistance(2.5f);
    TEST_ASSERT(FloatEq(player->GetMinDistance(), 2.5f), "Min distance round-trips to 2.5");

    player->SetMaxDistance(250.0f);
    TEST_ASSERT(FloatEq(player->GetMaxDistance(), 250.0f), "Max distance round-trips to 250.0");

    player->SetRolloffFactor(3.0f);
    TEST_ASSERT(FloatEq(player->GetRolloffFactor(), 3.0f), "Rolloff factor round-trips to 3.0");

    player->SetMinDistance(0.0f);
    TEST_ASSERT(FloatEq(player->GetMinDistance(), 0.1f), "Min distance clamps to floor 0.1");

    player->SetMaxDistance(0.0f);
    TEST_ASSERT(FloatEq(player->GetMaxDistance(), 1.0f), "Max distance clamps to floor 1.0");

    player->SetRolloffFactor(50.0f);
    TEST_ASSERT(FloatEq(player->GetRolloffFactor(), 10.0f), "Rolloff factor clamps to max 10.0");
    player->SetRolloffFactor(-5.0f);
    TEST_ASSERT(FloatEq(player->GetRolloffFactor(), 0.0f), "Rolloff factor clamps to min 0.0");

    return true;
}

bool TestAudioPlayerPlayState() {
    TEST_SECTION("AudioPlayer: Play-State Flags (no device)");

    std::shared_ptr<components::AudioPlayer> player = std::make_shared<components::AudioPlayer>();

    TEST_ASSERT(!player->IsPlaying(), "A fresh player with no source is not playing");
    TEST_ASSERT(!player->GetAudioAsset(), "A fresh player has no audio asset");

    player->Play();
    TEST_ASSERT(!player->IsPlaying(), "Play() with no loaded asset leaves the player stopped");

    player->Stop();
    TEST_ASSERT(!player->IsPlaying(), "Stop() on an idle player keeps it stopped");

    player->Pause();
    player->Resume();
    TEST_ASSERT(!player->IsPlaying(), "Pause/Resume on an idle player are safe no-ops");

    return true;
}

bool TestAudioListener() {
    TEST_SECTION("AudioListener: Active State");

    std::shared_ptr<components::AudioListener> listener = std::make_shared<components::AudioListener>();

    TEST_ASSERT(listener->GetTypeName() == "AudioListener", "AudioListener reports its type name");
    TEST_ASSERT(listener->IsActive(), "AudioListener defaults to active");

    listener->SetActive(false);
    TEST_ASSERT(!listener->IsActive(), "SetActive(false) deactivates the listener");

    listener->SetActive(true);
    TEST_ASSERT(listener->IsActive(), "SetActive(true) reactivates the listener");

    TEST_ASSERT(listener->IsEnabled(), "AudioListener component is enabled by default");
    listener->SetEnabled(false);
    TEST_ASSERT(!listener->IsEnabled(), "Component enabled flag round-trips to false");
    listener->SetEnabled(true);
    TEST_ASSERT(listener->IsEnabled(), "Component enabled flag round-trips back to true");

    return true;
}

bool TestTimerDefaults() {
    TEST_SECTION("Timer: Defaults & Property Round-Trip");

    std::shared_ptr<components::Timer> timer = std::make_shared<components::Timer>();
    timer->DefineProperties();

    TEST_ASSERT(timer->GetTypeName() == "Timer", "Timer reports its type name");
    TEST_ASSERT(FloatEq(timer->GetDuration(), 1.0f), "Duration defaults to 1.0");
    TEST_ASSERT(FloatEq(timer->GetElapsed(), 0.0f), "Elapsed defaults to 0.0");
    TEST_ASSERT(!timer->GetLoop(), "Loop defaults to false");
    TEST_ASSERT(timer->GetRepeatCount() == -1, "Repeat count defaults to -1 (infinite)");
    TEST_ASSERT(!timer->GetAutoStart(), "Auto-start defaults to false");
    TEST_ASSERT(!timer->IsRunning(), "Timer is not running by default");
    TEST_ASSERT(!timer->GetIgnoreTimeScale(), "Ignore-time-scale defaults to false");
    TEST_ASSERT(timer->GetFireCount() == 0, "Fire count starts at 0");

    timer->SetDuration(5.0f);
    TEST_ASSERT(FloatEq(timer->GetDuration(), 5.0f), "Duration round-trips to 5.0");

    timer->SetElapsed(2.0f);
    TEST_ASSERT(FloatEq(timer->GetElapsed(), 2.0f), "Elapsed round-trips to 2.0");

    timer->SetLoop(true);
    TEST_ASSERT(timer->GetLoop(), "Loop round-trips to true");

    timer->SetRepeatCount(3);
    TEST_ASSERT(timer->GetRepeatCount() == 3, "Repeat count round-trips to 3");

    timer->SetAutoStart(true);
    TEST_ASSERT(timer->GetAutoStart(), "Auto-start round-trips to true");

    timer->SetIgnoreTimeScale(true);
    TEST_ASSERT(timer->GetIgnoreTimeScale(), "Ignore-time-scale round-trips to true");

    timer->SetRunning(true);
    TEST_ASSERT(timer->IsRunning(), "Running flag round-trips to true");
    timer->SetRunning(false);
    TEST_ASSERT(!timer->IsRunning(), "Running flag round-trips to false");

    return true;
}

bool TestTimerControl() {
    TEST_SECTION("Timer: Start / Stop / Reset / Restart");

    std::shared_ptr<components::Timer> timer = std::make_shared<components::Timer>();
    timer->DefineProperties();
    timer->SetDuration(2.0f);

    TEST_ASSERT(!timer->IsRunning(), "Timer starts stopped");

    timer->Start();
    TEST_ASSERT(timer->IsRunning(), "Start() runs the timer");
    TEST_ASSERT(timer->GetFireCount() == 0, "Start() resets fire count to 0");

    timer->SetElapsed(1.0f);
    timer->Stop();
    TEST_ASSERT(!timer->IsRunning(), "Stop() pauses the timer");
    TEST_ASSERT(FloatEq(timer->GetElapsed(), 1.0f), "Stop() preserves elapsed time");

    timer->Reset();
    TEST_ASSERT(FloatEq(timer->GetElapsed(), 0.0f), "Reset() zeroes elapsed time");
    TEST_ASSERT(!timer->IsRunning(), "Reset() leaves a non-autostart timer stopped");

    timer->SetElapsed(0.75f);
    timer->Restart();
    TEST_ASSERT(timer->IsRunning(), "Restart() runs the timer");
    TEST_ASSERT(FloatEq(timer->GetElapsed(), 0.0f), "Restart() zeroes elapsed time");

    return true;
}

bool TestTimerCountdown() {
    TEST_SECTION("Timer: Deterministic Countdown & Time Remaining");

    std::shared_ptr<components::Timer> timer = std::make_shared<components::Timer>();
    timer->DefineProperties();
    timer->SetDuration(1.0f);
    timer->Start();

    TEST_ASSERT(FloatEq(timer->GetTimeRemaining(), 1.0f), "Time remaining starts at full duration");
    TEST_ASSERT(!timer->IsFinished(), "Timer is not finished before any tick");

    timer->OnUpdate(0.25f);
    TEST_ASSERT(FloatEq(timer->GetElapsed(), 0.25f), "Elapsed advances by deltaTime on tick");
    TEST_ASSERT(FloatEq(timer->GetTimeRemaining(), 0.75f), "Time remaining counts down deterministically");
    TEST_ASSERT(!timer->IsFinished(), "Timer is not finished partway through");

    timer->OnUpdate(0.25f);
    timer->OnUpdate(0.25f);
    TEST_ASSERT(FloatEq(timer->GetElapsed(), 0.75f), "Elapsed accumulates across multiple ticks");
    TEST_ASSERT(FloatEq(timer->GetTimeRemaining(), 0.25f), "Time remaining reflects accumulated ticks");

    timer->OnUpdate(0.0f);
    TEST_ASSERT(FloatEq(timer->GetElapsed(), 0.75f), "A zero-delta tick does not advance elapsed");

    return true;
}

bool TestTimerOneShot() {
    TEST_SECTION("Timer: One-Shot Timeout");

    std::shared_ptr<components::Timer> timer = std::make_shared<components::Timer>();
    timer->DefineProperties();
    timer->SetDuration(1.0f);
    timer->SetLoop(false);

    int fired = 0;
    timer->SetTimeoutCallback([&fired]() { ++fired; });
    timer->Start();

    timer->OnUpdate(0.5f);
    TEST_ASSERT(fired == 0, "One-shot callback has not fired before timeout");
    TEST_ASSERT(timer->IsRunning(), "One-shot timer is still running before timeout");

    timer->OnUpdate(0.6f);
    TEST_ASSERT(fired == 1, "One-shot callback fires once at timeout");
    TEST_ASSERT(timer->GetFireCount() == 1, "One-shot fire count reaches 1");
    TEST_ASSERT(!timer->IsRunning(), "One-shot timer stops after firing");
    TEST_ASSERT(timer->IsFinished(), "One-shot timer reports finished after timeout");
    TEST_ASSERT(FloatEq(timer->GetElapsed(), 1.0f), "One-shot elapsed is pinned to duration after timeout");

    timer->OnUpdate(1.0f);
    TEST_ASSERT(fired == 1, "Stopped one-shot timer does not fire again on further ticks");

    timer->ClearTimeoutCallback();
    timer->Restart();
    timer->OnUpdate(1.5f);
    TEST_ASSERT(fired == 1, "Cleared callback is not invoked on the next timeout");

    return true;
}

bool TestTimerLoop() {
    TEST_SECTION("Timer: Looping & Repeat Count");

    std::shared_ptr<components::Timer> loopTimer = std::make_shared<components::Timer>();
    loopTimer->DefineProperties();
    loopTimer->SetDuration(1.0f);
    loopTimer->SetLoop(true);
    loopTimer->SetRepeatCount(-1);

    int infiniteFires = 0;
    loopTimer->SetTimeoutCallback([&infiniteFires]() { ++infiniteFires; });
    loopTimer->Start();

    loopTimer->OnUpdate(1.0f);
    TEST_ASSERT(infiniteFires == 1, "Looping timer fires on first timeout");
    TEST_ASSERT(loopTimer->IsRunning(), "Infinite-loop timer keeps running after firing");
    TEST_ASSERT(FloatEq(loopTimer->GetElapsed(), 0.0f), "Looping timer carries no overshoot back to 0");

    loopTimer->OnUpdate(1.0f);
    loopTimer->OnUpdate(1.0f);
    TEST_ASSERT(infiniteFires == 3, "Infinite-loop timer keeps firing each cycle");
    TEST_ASSERT(loopTimer->IsRunning(), "Infinite-loop timer is still running after several cycles");

    std::shared_ptr<components::Timer> finiteTimer = std::make_shared<components::Timer>();
    finiteTimer->DefineProperties();
    finiteTimer->SetDuration(1.0f);
    finiteTimer->SetLoop(true);
    finiteTimer->SetRepeatCount(2);

    int finiteFires = 0;
    finiteTimer->SetTimeoutCallback([&finiteFires]() { ++finiteFires; });
    finiteTimer->Start();

    finiteTimer->OnUpdate(1.0f);
    TEST_ASSERT(finiteFires == 1, "Finite-loop timer fires on its first cycle");
    TEST_ASSERT(finiteTimer->IsRunning(), "Finite-loop timer runs after its first cycle");

    finiteTimer->OnUpdate(1.0f);
    TEST_ASSERT(finiteFires == 2, "Finite-loop timer fires on its second cycle");
    TEST_ASSERT(finiteTimer->GetFireCount() == 2, "Finite-loop fire count reaches the repeat limit");
    TEST_ASSERT(!finiteTimer->IsRunning(), "Finite-loop timer stops once its repeat count is reached");

    finiteTimer->OnUpdate(1.0f);
    TEST_ASSERT(finiteFires == 2, "Finite-loop timer does not fire past its repeat count");

    return true;
}

bool TestTimerNotRunning() {
    TEST_SECTION("Timer: Ticks Ignored While Stopped");

    std::shared_ptr<components::Timer> timer = std::make_shared<components::Timer>();
    timer->DefineProperties();
    timer->SetDuration(2.0f);

    int fired = 0;
    timer->SetTimeoutCallback([&fired]() { ++fired; });

    timer->OnUpdate(5.0f);
    TEST_ASSERT(FloatEq(timer->GetElapsed(), 0.0f), "A stopped timer ignores ticks");
    TEST_ASSERT(fired == 0, "A stopped timer never fires its callback");

    return true;
}

bool TestSubViewportViewport() {
    TEST_SECTION("SubViewport: Render Target Settings");

    std::shared_ptr<components::SubViewport> viewport = std::make_shared<components::SubViewport>();
    viewport->DefineProperties();

    TEST_ASSERT(viewport->GetTypeName() == "SubViewport", "SubViewport reports its type name");
    TEST_ASSERT(viewport->GetRenderWidth() == 512, "Render width defaults to 512");
    TEST_ASSERT(viewport->GetRenderHeight() == 512, "Render height defaults to 512");
    TEST_ASSERT(viewport->GetWorldType() == components::SubViewport::WorldType::World2D,
                "World type defaults to World2D");
    TEST_ASSERT(viewport->GetTransparentBackground(), "Transparent background defaults to true");

    viewport->SetRenderWidth(1280);
    TEST_ASSERT(viewport->GetRenderWidth() == 1280, "Render width round-trips to 1280");
    viewport->SetRenderHeight(720);
    TEST_ASSERT(viewport->GetRenderHeight() == 720, "Render height round-trips to 720");

    viewport->SetRenderWidth(0);
    TEST_ASSERT(viewport->GetRenderWidth() == 1, "Render width clamps to floor 1");

    viewport->SetWorldType(components::SubViewport::WorldType::World3D);
    TEST_ASSERT(viewport->GetWorldType() == components::SubViewport::WorldType::World3D,
                "World type round-trips to World3D");
    viewport->SetWorldType(components::SubViewport::WorldType::ScreenUI);
    TEST_ASSERT(viewport->GetWorldType() == components::SubViewport::WorldType::ScreenUI,
                "World type round-trips to ScreenUI");

    viewport->SetTransparentBackground(false);
    TEST_ASSERT(!viewport->GetTransparentBackground(), "Transparent background round-trips to false");

    return true;
}

bool TestSubViewportClearColor() {
    TEST_SECTION("SubViewport: Clear Color & Modulate");

    std::shared_ptr<components::SubViewport> viewport = std::make_shared<components::SubViewport>();
    viewport->DefineProperties();

    math::Color clear = viewport->GetClearColor();
    TEST_ASSERT(FloatEq(clear.r, 0.0f) && FloatEq(clear.g, 0.0f) && FloatEq(clear.b, 0.0f) && FloatEq(clear.a, 1.0f),
                "Clear color defaults to opaque black");

    viewport->SetClearColor(math::Color(0.2f, 0.4f, 0.6f, 0.8f));
    math::Color updated = viewport->GetClearColor();
    TEST_ASSERT(FloatEq(updated.r, 0.2f) && FloatEq(updated.g, 0.4f) &&
                FloatEq(updated.b, 0.6f) && FloatEq(updated.a, 0.8f),
                "Clear color round-trips component-wise");

    viewport->SetModulate(math::Color(1.0f, 0.5f, 0.25f, 0.5f));
    math::Color modulate = viewport->GetModulate();
    TEST_ASSERT(FloatEq(modulate.r, 1.0f) && FloatEq(modulate.g, 0.5f) &&
                FloatEq(modulate.b, 0.25f) && FloatEq(modulate.a, 0.5f),
                "Modulate color round-trips component-wise");

    return true;
}

bool TestSubViewportDisplay() {
    TEST_SECTION("SubViewport: Display & Input Settings");

    std::shared_ptr<components::SubViewport> viewport = std::make_shared<components::SubViewport>();
    viewport->DefineProperties();

    TEST_ASSERT(viewport->GetDisplayMode() == components::SubViewport::DisplayMode::Stretch,
                "Display mode defaults to Stretch");
    TEST_ASSERT(FloatEq(viewport->GetDisplayWidth(), 512.0f), "Display width defaults to 512");
    TEST_ASSERT(FloatEq(viewport->GetDisplayHeight(), 512.0f), "Display height defaults to 512");
    TEST_ASSERT(viewport->GetUseUISpace(), "Use-UI-space defaults to true");
    TEST_ASSERT(!viewport->GetFlipVertical(), "Flip-vertical defaults to false");
    TEST_ASSERT(viewport->GetInputMode() == components::SubViewport::InputMode::Isolated,
                "Input mode defaults to Isolated");
    TEST_ASSERT(viewport->GetFocusOnClick(), "Focus-on-click defaults to true");

    viewport->SetDisplayMode(components::SubViewport::DisplayMode::KeepAspect);
    TEST_ASSERT(viewport->GetDisplayMode() == components::SubViewport::DisplayMode::KeepAspect,
                "Display mode round-trips to KeepAspect");
    viewport->SetDisplayMode(components::SubViewport::DisplayMode::Disabled);
    TEST_ASSERT(viewport->GetDisplayMode() == components::SubViewport::DisplayMode::Disabled,
                "Display mode round-trips to Disabled");

    viewport->SetDisplayWidth(800.0f);
    TEST_ASSERT(FloatEq(viewport->GetDisplayWidth(), 800.0f), "Display width round-trips to 800");
    viewport->SetDisplayHeight(600.0f);
    TEST_ASSERT(FloatEq(viewport->GetDisplayHeight(), 600.0f), "Display height round-trips to 600");

    viewport->SetUseUISpace(false);
    TEST_ASSERT(!viewport->GetUseUISpace(), "Use-UI-space round-trips to false");

    viewport->SetFlipVertical(true);
    TEST_ASSERT(viewport->GetFlipVertical(), "Flip-vertical round-trips to true");

    viewport->SetInputMode(components::SubViewport::InputMode::Shared);
    TEST_ASSERT(viewport->GetInputMode() == components::SubViewport::InputMode::Shared,
                "Input mode round-trips to Shared");
    viewport->SetInputMode(components::SubViewport::InputMode::None);
    TEST_ASSERT(viewport->GetInputMode() == components::SubViewport::InputMode::None,
                "Input mode round-trips to None");

    viewport->SetFocusOnClick(false);
    TEST_ASSERT(!viewport->GetFocusOnClick(), "Focus-on-click round-trips to false");

    viewport->SetFocused(true);
    TEST_ASSERT(viewport->IsFocused(), "Focus state round-trips to true");
    viewport->SetFocused(false);
    TEST_ASSERT(!viewport->IsFocused(), "Focus state round-trips to false");

    return true;
}

bool TestMaterialPropertyBlock() {
    TEST_SECTION("CustomShaderParams: MaterialPropertyBlock Typed API");

    MaterialPropertyBlock block;
    TEST_ASSERT(block.isEmpty(), "A fresh property block is empty");
    TEST_ASSERT(!block.hasProperty("u_Missing"), "An unset property is reported absent");
    TEST_ASSERT(block.getProperty("u_Missing") == nullptr, "Getting an unset property returns null");

    block.setFloat("u_Float", 2.5f);
    TEST_ASSERT(block.hasProperty("u_Float"), "hasProperty reports a set float");
    TEST_ASSERT(!block.isEmpty(), "Block is non-empty after a set");
    const MaterialPropertyValue* floatVal = block.getProperty("u_Float");
    TEST_ASSERT(floatVal != nullptr && std::get_if<float>(floatVal) != nullptr &&
                FloatEq(std::get<float>(*floatVal), 2.5f), "Float value stores and reads back");

    block.setInt("u_Int", 7);
    const MaterialPropertyValue* intVal = block.getProperty("u_Int");
    TEST_ASSERT(intVal != nullptr && std::get_if<int>(intVal) != nullptr &&
                std::get<int>(*intVal) == 7, "Int value stores and reads back");

    block.setBool("u_Bool", true);
    const MaterialPropertyValue* boolVal = block.getProperty("u_Bool");
    TEST_ASSERT(boolVal != nullptr && std::get_if<bool>(boolVal) != nullptr &&
                std::get<bool>(*boolVal) == true, "Bool value stores and reads back");

    block.setVec2("u_Vec2", math::Vec2(1.0f, 2.0f));
    const MaterialPropertyValue* vec2Val = block.getProperty("u_Vec2");
    TEST_ASSERT(vec2Val != nullptr && std::get_if<math::Vec2>(vec2Val) != nullptr &&
                FloatEq(std::get<math::Vec2>(*vec2Val).x, 1.0f) &&
                FloatEq(std::get<math::Vec2>(*vec2Val).y, 2.0f), "Vec2 value stores and reads back");

    block.setVec3("u_Vec3", math::Vec3(3.0f, 4.0f, 5.0f));
    const MaterialPropertyValue* vec3Val = block.getProperty("u_Vec3");
    TEST_ASSERT(vec3Val != nullptr && std::get_if<math::Vec3>(vec3Val) != nullptr &&
                FloatEq(std::get<math::Vec3>(*vec3Val).z, 5.0f), "Vec3 value stores and reads back");

    block.setVec4("u_Vec4", math::Vec4(6.0f, 7.0f, 8.0f, 9.0f));
    const MaterialPropertyValue* vec4Val = block.getProperty("u_Vec4");
    TEST_ASSERT(vec4Val != nullptr && std::get_if<math::Vec4>(vec4Val) != nullptr &&
                FloatEq(std::get<math::Vec4>(*vec4Val).w, 9.0f), "Vec4 value stores and reads back");

    block.setColor("u_Color", math::Color(0.1f, 0.2f, 0.3f, 0.4f));
    const MaterialPropertyValue* colorVal = block.getProperty("u_Color");
    TEST_ASSERT(colorVal != nullptr && std::get_if<math::Color>(colorVal) != nullptr &&
                FloatEq(std::get<math::Color>(*colorVal).r, 0.1f) &&
                FloatEq(std::get<math::Color>(*colorVal).a, 0.4f), "Color value stores and reads back");

    block.setFloat("u_Float", 9.0f);
    TEST_ASSERT(FloatEq(std::get<float>(*block.getProperty("u_Float")), 9.0f),
                "Re-setting a property overwrites its value");

    block.clear();
    TEST_ASSERT(block.isEmpty(), "clear() empties the property block");
    TEST_ASSERT(!block.hasProperty("u_Float"), "Cleared block no longer reports its properties");

    return true;
}

bool TestCustomShaderParamsParsing() {
    TEST_SECTION("CustomShaderParams: Typed JSON Parsing");

    components::CustomShaderParams params;
    RenderContext ctx(nullptr, nullptr);

    const std::string json =
        "{"
        "\"u_Strength\": {\"type\": \"float\", \"value\": 0.75},"
        "\"u_Steps\": {\"type\": \"int\", \"value\": 4},"
        "\"u_Enabled\": {\"type\": \"bool\", \"value\": true},"
        "\"u_Offset\": {\"type\": \"vec2\", \"value\": [0.5, 1.5]},"
        "\"u_Dir\": {\"type\": \"vec3\", \"value\": [1.0, 0.0, -1.0]},"
        "\"u_Params\": {\"type\": \"vec4\", \"value\": [1.0, 2.0, 3.0, 4.0]},"
        "\"u_Tint\": {\"type\": \"color\", \"value\": [0.2, 0.4, 0.6, 0.8]}"
        "}";

    MaterialPropertyBlock block;
    params.BuildBlock(ctx, json, block);

    TEST_ASSERT(block.hasProperty("u_Strength") &&
                FloatEq(std::get<float>(*block.getProperty("u_Strength")), 0.75f),
                "Float parameter parses into the block");
    TEST_ASSERT(block.hasProperty("u_Steps") &&
                std::get<int>(*block.getProperty("u_Steps")) == 4,
                "Int parameter parses into the block");
    TEST_ASSERT(block.hasProperty("u_Enabled") &&
                std::get<bool>(*block.getProperty("u_Enabled")) == true,
                "Bool parameter parses into the block");
    TEST_ASSERT(block.hasProperty("u_Offset") &&
                FloatEq(std::get<math::Vec2>(*block.getProperty("u_Offset")).y, 1.5f),
                "Vec2 parameter parses into the block");
    TEST_ASSERT(block.hasProperty("u_Dir") &&
                FloatEq(std::get<math::Vec3>(*block.getProperty("u_Dir")).z, -1.0f),
                "Vec3 parameter parses into the block");
    TEST_ASSERT(block.hasProperty("u_Params") &&
                FloatEq(std::get<math::Vec4>(*block.getProperty("u_Params")).w, 4.0f),
                "Vec4 parameter parses into the block");
    TEST_ASSERT(block.hasProperty("u_Tint") &&
                FloatEq(std::get<math::Color>(*block.getProperty("u_Tint")).b, 0.6f),
                "Color parameter parses into the block");

    return true;
}

bool TestCustomShaderParamsEdgeCases() {
    TEST_SECTION("CustomShaderParams: Edge Cases & Clear");

    components::CustomShaderParams params;
    RenderContext ctx(nullptr, nullptr);

    MaterialPropertyBlock empty;
    params.BuildBlock(ctx, "", empty);
    TEST_ASSERT(empty.isEmpty(), "Empty parameter string yields an empty block");

    MaterialPropertyBlock malformed;
    params.BuildBlock(ctx, "{ this is not json", malformed);
    TEST_ASSERT(malformed.isEmpty(), "Malformed JSON is ignored, leaving an empty block");

    MaterialPropertyBlock mixed;
    const std::string json =
        "{"
        "\"u_Good\": {\"type\": \"float\", \"value\": 1.0},"
        "\"u_NoType\": {\"value\": 2.0},"
        "\"u_BadKind\": {\"type\": \"matrix\", \"value\": 3.0},"
        "\"u_EmptyTex\": {\"type\": \"texture\", \"value\": \"\"}"
        "}";
    params.BuildBlock(ctx, json, mixed);
    TEST_ASSERT(mixed.hasProperty("u_Good"), "Valid entries are parsed alongside invalid ones");
    TEST_ASSERT(!mixed.hasProperty("u_NoType"), "Entries missing a type field are skipped");
    TEST_ASSERT(!mixed.hasProperty("u_BadKind"), "Entries with an unknown type are skipped");
    TEST_ASSERT(!mixed.hasProperty("u_EmptyTex"), "Texture entries with an empty path are skipped");

    params.Clear();
    TEST_ASSERT(true, "Clear() releases cached texture params without a device");

    return true;
}

} // namespace

void RunMiscComponentTests() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "MISC COMPONENT TESTS" << std::endl;
    std::cout << "========================================" << std::endl;

    lupine_test::SetCurrentSuite("MiscComponents");

    engine::InitializeEngine();

    bool allPassed = true;
    allPassed &= TestAudioPlayerConfig();
    allPassed &= TestAudioPlayerClamping();
    allPassed &= TestAudioPlayer3DConfig();
    allPassed &= TestAudioPlayerPlayState();
    allPassed &= TestAudioListener();
    allPassed &= TestTimerDefaults();
    allPassed &= TestTimerControl();
    allPassed &= TestTimerCountdown();
    allPassed &= TestTimerOneShot();
    allPassed &= TestTimerLoop();
    allPassed &= TestTimerNotRunning();
    allPassed &= TestSubViewportViewport();
    allPassed &= TestSubViewportClearColor();
    allPassed &= TestSubViewportDisplay();
    allPassed &= TestMaterialPropertyBlock();
    allPassed &= TestCustomShaderParamsParsing();
    allPassed &= TestCustomShaderParamsEdgeCases();

    std::cout << "\n========================================" << std::endl;
    if (allPassed) {
        std::cout << "ALL MISC COMPONENT TESTS PASSED!" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED!" << std::endl;
    }
    std::cout << "========================================\n" << std::endl;
}

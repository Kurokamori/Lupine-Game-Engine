#include "lupine/audio/Audio.hpp"
#include "lupine/logger/Logger.hpp"

namespace lupine {
namespace audio {

void InitializeAudio() {

    AudioManager::GetInstance().Initialize();
}

void ShutdownAudio() {

    AudioManager::GetInstance().Shutdown();
}

}
}


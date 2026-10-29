// Menu/park UI click sounds (see ui_sound.h).

#include "ui_sound.h"

#include "../formats/path.h"

#include <algorithm>
#include <cstdio>
#include <exception>

void UiSoundPlayer::play(const std::string& relative) {
    if (!enabled || relative.empty()) {
        return;
    }
    std::string path;
    try {
        path = dino::asset_path(asset_root, relative);
    } catch (const std::exception&) {
        return;
    }
    auto it = cache.find(path);
    if (it == cache.end()) {
        try {
            it = cache.emplace(path, dino::load_wav(path)).first;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "sound load %s: %s\n", path.c_str(), e.what());
            return;
        }
    }
    const dino::Wav& wav = it->second;
    if (stream != nullptr) {
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
    }
    SDL_AudioSpec spec{};
    spec.format = wav.bits_per_sample == 16 ? SDL_AUDIO_S16 : SDL_AUDIO_U8;
    spec.channels = wav.channels;
    spec.freq = static_cast<int>(wav.sample_rate);
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                       &spec, nullptr, nullptr);
    if (stream == nullptr) {
        std::fprintf(stderr, "sound device: %s\n", SDL_GetError());
        enabled = false;
        return;
    }
    SDL_SetAudioStreamGain(stream, muted ? 0.0f : gain);
    SDL_ResumeAudioStreamDevice(stream);
    SDL_PutAudioStreamData(stream, wav.data.data(),
                           static_cast<int>(wav.data.size()));
    remaining = wav.duration;
    std::printf("sound: %s\n", path.c_str());
}

void UiSoundPlayer::set_muted(bool value) {
    muted = value;
    if (stream != nullptr) {
        SDL_SetAudioStreamGain(stream, muted ? 0.0f : gain);
    }
}

void UiSoundPlayer::set_gain(float value) {
    gain = std::clamp(value, 0.0f, 1.0f);
    if (stream != nullptr) {
        SDL_SetAudioStreamGain(stream, muted ? 0.0f : gain);
    }
}

void UiSoundPlayer::update(double dt) {
    if (stream == nullptr) {
        return;
    }
    remaining -= dt;
    if (remaining <= 0.0) {
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
    }
}

void UiSoundPlayer::destroy() {
    if (stream != nullptr) {
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
    }
}

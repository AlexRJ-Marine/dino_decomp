// Island music streaming (see music.h).

#include "music.h"

#include "../world/vegetation.h"

#include <algorithm>
#include <cstdio>
#include <exception>

void MusicPlayer::stop() {
    if (stream != nullptr) {
        SDL_DestroyAudioStream(stream);
        stream = nullptr;
    }
    wav = dino::Wav{};
    offset = 0;
    current.clear();
}

void MusicPlayer::play(const std::string& relative) {
    if (!enabled || relative.empty()) {
        return;
    }
    stop();
    std::string path;
    try {
        path = dino::asset_path(asset_root, relative);
    } catch (const std::exception&) {
        return;
    }
    try {
        wav = dino::load_wav(path);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "music load %s: %s\n", path.c_str(), e.what());
        return;
    }
    if (wav.data.empty() || wav.sample_rate == 0 || wav.channels == 0) {
        return;
    }
    SDL_AudioSpec spec{};
    spec.format = wav.bits_per_sample == 16 ? SDL_AUDIO_S16 : SDL_AUDIO_U8;
    spec.channels = wav.channels;
    spec.freq = static_cast<int>(wav.sample_rate);
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                       &spec, nullptr, nullptr);
    if (stream == nullptr) {
        std::fprintf(stderr, "music device: %s\n", SDL_GetError());
        enabled = false;
        return;
    }
    SDL_SetAudioStreamGain(stream, gain);
    SDL_ResumeAudioStreamDevice(stream);
    current = path;
    offset = 0;
    feed();
    std::printf("music: %s, %u Hz, %u ch, %.1fs (loop)\n", path.c_str(),
                wav.sample_rate, wav.channels, wav.duration);
}

void MusicPlayer::feed() {
    if (stream == nullptr || wav.data.empty()) {
        return;
    }
    const int bytes_per_second =
        static_cast<int>(wav.sample_rate) * wav.channels *
        (wav.bits_per_sample / 8);
    if (bytes_per_second <= 0) {
        return;
    }
    while (SDL_GetAudioStreamQueued(stream) < bytes_per_second) {
        if (offset >= wav.data.size()) {
            if (!looping) {
                break;
            }
            offset = 0;
        }
        const std::size_t chunk =
            std::min<std::size_t>(65536, wav.data.size() - offset);
        SDL_PutAudioStreamData(stream, wav.data.data() + offset,
                               static_cast<int>(chunk));
        offset += chunk;
    }
}

void MusicPlayer::update() {
    if (stream != nullptr) {
        feed();
    }
}

void MusicPlayer::set_gain(float value) {
    gain = value;
    if (stream != nullptr) {
        SDL_SetAudioStreamGain(stream, gain);
    }
}

void MusicPlayer::destroy() {
    stop();
    enabled = false;
}

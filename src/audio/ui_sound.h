// Menu and park UI click sounds.
//
// Retail plays a sound from MenuSoundDB.ini on every button press; a new click
// cuts the previous one. SDL3 makes one short device stream per click, which is
// cheap enough at UI rates and avoids mixing code of our own.
//
// Split out of the app shell: the menu and the park HUD both trigger sounds,
// and neither should own an SDL audio stream.

#ifndef DINO_AUDIO_UI_SOUND_H
#define DINO_AUDIO_UI_SOUND_H

#include <SDL3/SDL_audio.h>

#include <string>
#include <unordered_map>

#include "../formats/wav.h"

struct UiSoundPlayer {
    std::string asset_root;
    std::unordered_map<std::string, dino::Wav> cache;
    SDL_AudioStream* stream = nullptr;
    double remaining = 0.0;
    bool enabled = true;
    bool muted = false;
    float gain = 1.0f;

    // Resolves `relative` under `asset_root`, loads and caches the wav, then
    // starts a fresh stream (cutting the previous sound). Missing files and
    // unknown paths are silent, not fatal.
    void play(const std::string& relative);

    void set_muted(bool value);
    void set_gain(float value);

    // Frees the stream once the clip has played out. Call every frame.
    void update(double dt);

    void destroy();
};

#endif  // DINO_AUDIO_UI_SOUND_H

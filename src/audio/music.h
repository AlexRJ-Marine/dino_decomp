// Island music.
//
// The retail themes are 26-28 MB wavs (one per island plus the menu loop).
// Loading one and handing the whole buffer to SDL would block for seconds and
// waste the queue budget, so this player streams the file in 64 KB chunks and
// wraps to the start, matching the retail loop. See `docs/formats/wav.md`.

#ifndef DINO_AUDIO_MUSIC_H
#define DINO_AUDIO_MUSIC_H

#include <SDL3/SDL_audio.h>

#include <cstddef>
#include <string>

#include "../formats/wav.h"

struct MusicPlayer {
    MusicPlayer() = default;
    ~MusicPlayer() { destroy(); }
    MusicPlayer(const MusicPlayer&) = delete;
    MusicPlayer& operator=(const MusicPlayer&) = delete;
    MusicPlayer(MusicPlayer&&) = delete;
    MusicPlayer& operator=(MusicPlayer&&) = delete;

    std::string asset_root;
    SDL_AudioStream* stream = nullptr;
    dino::Wav wav;
    std::size_t offset = 0;
    bool looping = true;
    bool enabled = false;
    float gain = 0.97f;
    std::string current;

    void stop();

    // Loads `relative`, starts a stream and primes one second. A missing file
    // disables the player rather than throwing: the game must still run.
    void play(const std::string& relative);

    // Keeps the queue about one second ahead, wrapping at the end when
    // `looping`. Call every frame.
    void feed();
    void update();

    void set_gain(float value);
    void destroy();
};

#endif  // DINO_AUDIO_MUSIC_H

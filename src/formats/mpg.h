#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace dino {

// MPEG-1 probe result for a retail `.mpg` ending video.
struct MpgInfo {
    int width = 0;
    int height = 0;
    double duration = 0.0;    // seconds
    double frame_rate = 0.0;  // frames per second
    // MPEG-1 Audio Layer II track (the cinematics' soundtrack).
    int audio_streams = 0;
    int audio_samplerate = 0;
};

struct MpgFrame {
    int width = 0;
    int height = 0;
    int index = 0;      // decoded frame index (0-based)
    double time = 0.0;  // presentation time in seconds
    std::vector<std::uint8_t> rgb;  // width*height*3, top-down
};

// Reads the MPEG-PS header (PL_MPEG); throws std::runtime_error on failure.
MpgInfo probe_mpg(const std::string& path);

// Decodes forward to `frame_index` and returns it as RGB. Audio is disabled.
MpgFrame decode_mpg_frame(const std::string& path, int frame_index = 0);

// Streaming decoder for real-time playback. One `next()` per video frame.
class MpgStream {
public:
    MpgStream();
    ~MpgStream();
    MpgStream(const MpgStream&) = delete;
    MpgStream& operator=(const MpgStream&) = delete;

    bool open(const std::string& path, std::string& error);
    void close();
    int width() const;
    int height() const;
    double frame_rate() const;
    double duration() const;
    // Embedded MP2 audio: interleaved stereo float in [-1, 1].
    bool has_audio() const;
    int audio_samplerate() const;
    // Moves the samples decoded since the previous call out of the stream.
    std::vector<float> take_audio();
    bool next(MpgFrame& frame);

    struct Impl;

private:
    Impl* impl_ = nullptr;
};

}  // namespace dino

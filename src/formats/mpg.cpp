#include "mpg.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#define PL_MPEG_IMPLEMENTATION
#include "../../third_party/pl_mpeg.h"

namespace dino {
namespace {

plm_t* open_video(const std::string& path) {
    plm_t* video = plm_create_with_filename(path.c_str());
    if (video == nullptr) {
        throw std::runtime_error("mpg: cannot open " + path);
    }
    plm_set_audio_enabled(video, 0);
    return video;
}

struct DecodeTarget {
    int target = 0;
    int index = 0;
    bool found = false;
    MpgFrame frame;
};

void on_video(plm_t* self, plm_frame_t* frame, void* user) {
    (void)self;
    DecodeTarget* target = static_cast<DecodeTarget*>(user);
    if (target == nullptr || target->found) {
        return;
    }
    if (target->index == target->target) {
        const int width = static_cast<int>(frame->width);
        const int height = static_cast<int>(frame->height);
        target->frame.width = width;
        target->frame.height = height;
        target->frame.index = target->index;
        target->frame.time = frame->time;
        target->frame.rgb.resize(static_cast<std::size_t>(width) * height * 3);
        plm_frame_to_rgb(frame, target->frame.rgb.data(), width * 3);
        target->found = true;
    }
    ++target->index;
}

}  // namespace

MpgInfo probe_mpg(const std::string& path) {
    plm_t* video = open_video(path);
    MpgInfo info;
    info.width = static_cast<int>(plm_get_width(video));
    info.height = static_cast<int>(plm_get_height(video));
    info.duration = plm_get_duration(video);
    info.frame_rate = plm_get_framerate(video);
    plm_destroy(video);
    if (info.width <= 0 || info.height <= 0) {
        throw std::runtime_error("mpg: no video stream in " + path);
    }
    return info;
}

MpgFrame decode_mpg_frame(const std::string& path, int frame_index) {
    if (frame_index < 0) {
        throw std::runtime_error("mpg: negative frame index");
    }
    plm_t* video = open_video(path);
    DecodeTarget target;
    target.target = frame_index;
    plm_set_video_decode_callback(video, on_video, &target);
    const double rate = plm_get_framerate(video);
    const double step = rate > 0.0 ? 1.0 / rate : 1.0 / 30.0;
    while (!plm_has_ended(video) && !target.found) {
        plm_decode(video, step);
    }
    plm_destroy(video);
    if (!target.found) {
        throw std::runtime_error("mpg: frame " + std::to_string(frame_index) +
                                 " not found in " + path);
    }
    return target.frame;
}

}  // namespace dino

#include "mpg.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

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

struct MpgStream::Impl {
    plm_t* video = nullptr;
    int width = 0;
    int height = 0;
    double frame_rate = 0.0;
    double duration = 0.0;
    bool ready = false;
    MpgFrame pending;
};

namespace {

void on_stream_video(plm_t* self, plm_frame_t* frame, void* user) {
    (void)self;
    MpgStream::Impl* impl = static_cast<MpgStream::Impl*>(user);
    if (impl == nullptr) {
        return;
    }
    const int width = static_cast<int>(frame->width);
    const int height = static_cast<int>(frame->height);
    impl->pending.width = width;
    impl->pending.height = height;
    impl->pending.time = frame->time;
    impl->pending.rgb.resize(static_cast<std::size_t>(width) * height * 3);
    plm_frame_to_rgb(frame, impl->pending.rgb.data(), width * 3);
    impl->ready = true;
}

}  // namespace

MpgStream::MpgStream() : impl_(new Impl()) {}

MpgStream::~MpgStream() { close(); }

bool MpgStream::open(const std::string& path, std::string& error) {
    close();
    impl_->video = plm_create_with_filename(path.c_str());
    if (impl_->video == nullptr) {
        error = "cannot open " + path;
        return false;
    }
    plm_set_audio_enabled(impl_->video, 0);
    impl_->width = static_cast<int>(plm_get_width(impl_->video));
    impl_->height = static_cast<int>(plm_get_height(impl_->video));
    impl_->frame_rate = plm_get_framerate(impl_->video);
    impl_->duration = plm_get_duration(impl_->video);
    if (impl_->width <= 0 || impl_->height <= 0) {
        error = "no video stream in " + path;
        return false;
    }
    plm_set_video_decode_callback(impl_->video, on_stream_video, impl_);
    return true;
}

void MpgStream::close() {
    if (impl_ == nullptr) {
        return;
    }
    if (impl_->video != nullptr) {
        plm_destroy(impl_->video);
        impl_->video = nullptr;
    }
    impl_->ready = false;
    impl_->pending = MpgFrame{};
}

int MpgStream::width() const { return impl_ != nullptr ? impl_->width : 0; }
int MpgStream::height() const { return impl_ != nullptr ? impl_->height : 0; }
double MpgStream::frame_rate() const {
    return impl_ != nullptr ? impl_->frame_rate : 0.0;
}
double MpgStream::duration() const {
    return impl_ != nullptr ? impl_->duration : 0.0;
}

bool MpgStream::next(MpgFrame& frame) {
    if (impl_ == nullptr || impl_->video == nullptr) {
        return false;
    }
    const double rate = impl_->frame_rate > 0.0 ? impl_->frame_rate : 24.0;
    const double step = 1.0 / rate;
    while (!plm_has_ended(impl_->video)) {
        if (impl_->ready) {
            frame = std::move(impl_->pending);
            impl_->pending = MpgFrame{};
            impl_->ready = false;
            return true;
        }
        plm_decode(impl_->video, step);
    }
    if (impl_->ready) {
        frame = std::move(impl_->pending);
        impl_->pending = MpgFrame{};
        impl_->ready = false;
        return true;
    }
    return false;
}

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

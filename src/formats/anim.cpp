#include "anim.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "bytes.h"

namespace dino {
namespace {

struct Quat {
    double w, x, y, z;
};

Quat quat_from_matrix(const Mat4& m) {
    // Column-vector rotation is the transpose of the row-vector matrix.
    const double r00 = m.m[0], r01 = m.m[4], r02 = m.m[8];
    const double r10 = m.m[1], r11 = m.m[5], r12 = m.m[9];
    const double r20 = m.m[2], r21 = m.m[6], r22 = m.m[10];
    const double t = r00 + r11 + r22;
    Quat q{};
    if (t > 0.0) {
        const double s = std::sqrt(t + 1.0) * 2.0;
        q.w = 0.25 * s;
        q.x = (r21 - r12) / s;
        q.y = (r02 - r20) / s;
        q.z = (r10 - r01) / s;
    } else if (r00 > r11 && r00 > r22) {
        const double s = std::sqrt(1.0 + r00 - r11 - r22) * 2.0;
        q.w = (r21 - r12) / s;
        q.x = 0.25 * s;
        q.y = (r01 + r10) / s;
        q.z = (r02 + r20) / s;
    } else if (r11 > r22) {
        const double s = std::sqrt(1.0 + r11 - r00 - r22) * 2.0;
        q.w = (r02 - r20) / s;
        q.x = (r01 + r10) / s;
        q.y = 0.25 * s;
        q.z = (r12 + r21) / s;
    } else {
        const double s = std::sqrt(1.0 + r22 - r00 - r11) * 2.0;
        q.w = (r10 - r01) / s;
        q.x = (r02 + r20) / s;
        q.y = (r12 + r21) / s;
        q.z = 0.25 * s;
    }
    const double n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return {q.w / n, q.x / n, q.y / n, q.z / n};
}

Quat slerp(Quat a, Quat b, double t) {
    double dot = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    if (dot < 0.0) {
        b = {-b.w, -b.x, -b.y, -b.z};
        dot = -dot;
    }
    if (dot > 0.9995) {
        Quat q{a.w + t * (b.w - a.w), a.x + t * (b.x - a.x),
               a.y + t * (b.y - a.y), a.z + t * (b.z - a.z)};
        const double n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
        return {q.w / n, q.x / n, q.y / n, q.z / n};
    }
    const double theta = std::acos(std::clamp(dot, -1.0, 1.0));
    const double sin_theta = std::sin(theta);
    const double w0 = std::sin((1.0 - t) * theta) / sin_theta;
    const double w1 = std::sin(t * theta) / sin_theta;
    return {w0 * a.w + w1 * b.w, w0 * a.x + w1 * b.x,
            w0 * a.y + w1 * b.y, w0 * a.z + w1 * b.z};
}

Mat4 matrix_from_quat(const Quat& q, const Vec3& translation) {
    const double w = q.w, x = q.x, y = q.y, z = q.z;
    const double xx = x * x, yy = y * y, zz = z * z;
    const double xy = x * y, xz = x * z, yz = y * z;
    const double wx = w * x, wy = w * y, wz = w * z;
    // Column-vector matrix.
    const double col[3][3] = {
        {1 - 2 * (yy + zz), 2 * (xy - wz), 2 * (xz + wy)},
        {2 * (xy + wz), 1 - 2 * (xx + zz), 2 * (yz - wx)},
        {2 * (xz - wy), 2 * (yz + wx), 1 - 2 * (xx + yy)},
    };
    Mat4 m;
    // Transpose to row-vector convention.
    m.m[0] = static_cast<float>(col[0][0]);
    m.m[1] = static_cast<float>(col[1][0]);
    m.m[2] = static_cast<float>(col[2][0]);
    m.m[4] = static_cast<float>(col[0][1]);
    m.m[5] = static_cast<float>(col[1][1]);
    m.m[6] = static_cast<float>(col[2][1]);
    m.m[8] = static_cast<float>(col[0][2]);
    m.m[9] = static_cast<float>(col[1][2]);
    m.m[10] = static_cast<float>(col[2][2]);
    m.m[12] = translation.x;
    m.m[13] = translation.y;
    m.m[14] = translation.z;
    return m;
}

}  // namespace

Mat4 AnimBone::sample(double time) const {
    if (times.empty()) {
        return Mat4{};
    }
    if (time <= static_cast<double>(times.front())) {
        return matrices.front();
    }
    if (time >= static_cast<double>(times.back())) {
        return matrices.back();
    }
    const auto it = std::upper_bound(times.begin(), times.end(),
                                     static_cast<std::uint64_t>(time));
    const std::size_t i = static_cast<std::size_t>(it - times.begin()) - 1;
    const double t0 = static_cast<double>(times[i]);
    const double t1 = static_cast<double>(times[i + 1]);
    if (time == t0) {
        return matrices[i];
    }
    if (time == t1) {
        return matrices[i + 1];
    }
    const double a = (time - t0) / (t1 - t0);
    const Mat4& m0 = matrices[i];
    const Mat4& m1 = matrices[i + 1];
    const Quat q = slerp(quat_from_matrix(m0), quat_from_matrix(m1), a);
    const Vec3 tr{
        static_cast<float>(m0.m[12] + a * (m1.m[12] - m0.m[12])),
        static_cast<float>(m0.m[13] + a * (m1.m[13] - m0.m[13])),
        static_cast<float>(m0.m[14] + a * (m1.m[14] - m0.m[14])),
    };
    return matrix_from_quat(q, tr);
}

const AnimBone* Animation::find(const std::string& name) const {
    for (const auto& bone : bones) {
        if (bone.name == name) {
            return &bone;
        }
    }
    return nullptr;
}

Animation load_anim(const std::string& path) {
    const auto d = read_binary_file(path);
    static constexpr std::uint8_t kMagic[5] = {1, 3, 0, 0, 0};
    if (d.size() < 0x19 || std::memcmp(d.data(), kMagic, 5) != 0) {
        throw std::runtime_error("anim: bad magic in " + path);
    }
    Animation anim;
    anim.hash = read_u32(d, 5, "hash");
    if (read_u32(d, 9, "sentinel") != 0xDEF01234u) {
        throw std::runtime_error("anim: bad sentinel in " + path);
    }
    anim.duration = read_u32(d, 0x0D, "duration");
    anim.start = read_u32(d, 0x11, "start");
    const std::uint32_t bone_count = read_u32(d, 0x15, "bone count");
    if (bone_count > 10'000) {
        throw std::runtime_error("anim: bad bone count in " + path);
    }

    std::size_t off = 0x19;
    anim.bones.resize(bone_count);
    for (std::uint32_t b = 0; b < bone_count; ++b) {
        AnimBone& bone = anim.bones[b];
        bone.name = read_string(d, off, "anim bone name");
        const std::uint32_t key_count = read_u32(d, off, "key count");
        off += 4;
        if (key_count > 100'000) {
            throw std::runtime_error("anim: bad key count in " + path);
        }
        need_bytes(d, off, static_cast<std::size_t>(key_count) * 8, "key times");
        bone.times.resize(key_count);
        for (std::uint32_t k = 0; k < key_count; ++k) {
            bone.times[k] = read_u64(d, off, "key time");
            off += 8;
            if (k > 0 && bone.times[k] <= bone.times[k - 1]) {
                throw std::runtime_error("anim: times are not increasing in " + path);
            }
        }
        const std::uint32_t matrix_count = read_u32(d, off, "matrix count");
        off += 4;
        if (matrix_count != key_count) {
            throw std::runtime_error("anim: matrix count mismatch in " + path);
        }
        need_bytes(d, off, static_cast<std::size_t>(matrix_count) * 64, "key matrices");
        bone.matrices.resize(matrix_count);
        for (std::uint32_t k = 0; k < matrix_count; ++k) {
            for (int j = 0; j < 16; ++j) {
                bone.matrices[k].m[j] = read_f32(d, off + static_cast<std::size_t>(j) * 4,
                                                 "key matrix");
            }
            off += 64;
        }
    }
    if (off != d.size()) {
        throw std::runtime_error("anim: trailing data in " + path);
    }
    return anim;
}

}  // namespace dino

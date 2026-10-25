#pragma once

#include <cmath>
#include <cstddef>

#include "mesh.h"

namespace dino {

// Row-major 4x4, matching the retail files:
//   [ R00 R01 R02 0 ]
//   [ R10 R11 R12 0 ]
//   [ R20 R21 R22 0 ]
//   [ Tx  Ty  Tz  1 ]
struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

inline Mat4 identity() {
    return Mat4{};
}

inline Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 out;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[row * 4 + k] * b.m[k * 4 + col];
            }
            out.m[row * 4 + col] = sum;
        }
    }
    return out;
}

inline Vec3 transform_point(const Mat4& m, const Vec3& p) {
    return {
        p.x * m.m[0] + p.y * m.m[4] + p.z * m.m[8] + m.m[12],
        p.x * m.m[1] + p.y * m.m[5] + p.z * m.m[9] + m.m[13],
        p.x * m.m[2] + p.y * m.m[6] + p.z * m.m[10] + m.m[14],
    };
}

inline Vec3 transform_direction(const Mat4& m, const Vec3& v) {
    return {
        v.x * m.m[0] + v.y * m.m[4] + v.z * m.m[8],
        v.x * m.m[1] + v.y * m.m[5] + v.z * m.m[9],
        v.x * m.m[2] + v.y * m.m[6] + v.z * m.m[10],
    };
}

inline Vec3 normalize(const Vec3& v) {
    const float n = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return n > 0.0f ? Vec3{v.x / n, v.y / n, v.z / n} : Vec3{0.0f, 0.0f, 0.0f};
}

// The skeleton world matrix is in metres (root scale 0.01); skinning works in
// raw centimetres. W_raw = W_scaled @ diag(100, 100, 100, 1).
inline Mat4 raw_centimetres(const Mat4& scaled) {
    Mat4 out = scaled;
    out.m[0] *= 100.0f;
    out.m[1] *= 100.0f;
    out.m[2] *= 100.0f;
    out.m[4] *= 100.0f;
    out.m[5] *= 100.0f;
    out.m[6] *= 100.0f;
    out.m[8] *= 100.0f;
    out.m[9] *= 100.0f;
    out.m[10] *= 100.0f;
    out.m[12] *= 100.0f;
    out.m[13] *= 100.0f;
    out.m[14] *= 100.0f;
    return out;
}

}  // namespace dino

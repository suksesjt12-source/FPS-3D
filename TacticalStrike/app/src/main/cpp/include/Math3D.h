#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace ts {

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;

inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    float length() const { return std::sqrt(x * x + y * y); }
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }
    Vec3 normalized() const {
        float l = length();
        if (l < 1e-6f) return {0.0f, 0.0f, 0.0f};
        float inv = 1.0f / l;
        return {x * inv, y * inv, z * inv};
    }
};

inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
    return a + (b - a) * t;
}

inline Vec3 minVec3(const Vec3& a, const Vec3& b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}

inline Vec3 maxVec3(const Vec3& a, const Vec3& b) {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

struct Quat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    Quat() = default;
    Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    Quat normalized() const {
        float l = std::sqrt(x * x + y * y + z * z + w * w);
        if (l < 1e-6f) return {0.0f, 0.0f, 0.0f, 1.0f};
        float inv = 1.0f / l;
        return {x * inv, y * inv, z * inv, w * inv};
    }

    Quat operator*(const Quat& r) const {
        return {
            w * r.x + x * r.w + y * r.z - z * r.y,
            w * r.y - x * r.z + y * r.w + z * r.x,
            w * r.z + x * r.y - y * r.x + z * r.w,
            w * r.w - x * r.x - y * r.y - z * r.z
        };
    }

    static Quat fromAxisAngle(const Vec3& axis, float radians) {
        Vec3 n = axis.normalized();
        float half = radians * 0.5f;
        float s = std::sin(half);
        return Quat(n.x * s, n.y * s, n.z * s, std::cos(half)).normalized();
    }

    static Quat fromEulerYXZ(float yaw, float pitch, float roll) {
        Quat qy = fromAxisAngle({0, 1, 0}, yaw);
        Quat qx = fromAxisAngle({1, 0, 0}, pitch);
        Quat qz = fromAxisAngle({0, 0, 1}, roll);
        return (qy * qx * qz).normalized();
    }

    static Quat slerp(const Quat& a, const Quat& b, float t) {
        Quat q1 = a.normalized();
        Quat q2 = b.normalized();
        float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;
        if (dot < 0.0f) {
            dot = -dot;
            q2.x = -q2.x; q2.y = -q2.y; q2.z = -q2.z; q2.w = -q2.w;
        }
        if (dot > 0.9995f) {
            return Quat(
                lerpf(q1.x, q2.x, t),
                lerpf(q1.y, q2.y, t),
                lerpf(q1.z, q2.z, t),
                lerpf(q1.w, q2.w, t)
            ).normalized();
        }
        float theta0 = std::acos(clampf(dot, -1.0f, 1.0f));
        float theta = theta0 * t;
        float sinTheta = std::sin(theta);
        float sinTheta0 = std::sin(theta0);
        float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
        float s1 = sinTheta / sinTheta0;
        return Quat(
            s0 * q1.x + s1 * q2.x,
            s0 * q1.y + s1 * q2.y,
            s0 * q1.z + s1 * q2.z,
            s0 * q1.w + s1 * q2.w
        ).normalized();
    }
};

// Column-major 4x4 matrix matching OpenGL ES 3.0 layout
struct Mat4 {
    float m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    static Mat4 identity() { return Mat4(); }

    static Mat4 translate(const Vec3& t) {
        Mat4 r;
        r.m[12] = t.x;
        r.m[13] = t.y;
        r.m[14] = t.z;
        return r;
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 r;
        r.m[0] = s.x;
        r.m[5] = s.y;
        r.m[10] = s.z;
        return r;
    }

    static Mat4 rotateY(float rad) {
        Mat4 r;
        float c = std::cos(rad);
        float s = std::sin(rad);
        r.m[0] = c;  r.m[8] = s;
        r.m[2] = -s; r.m[10] = c;
        return r;
    }

    static Mat4 rotateX(float rad) {
        Mat4 r;
        float c = std::cos(rad);
        float s = std::sin(rad);
        r.m[5] = c; r.m[9] = -s;
        r.m[6] = s; r.m[10] = c;
        return r;
    }

    static Mat4 rotateZ(float rad) {
        Mat4 r;
        float c = std::cos(rad);
        float s = std::sin(rad);
        r.m[0] = c; r.m[4] = -s;
        r.m[1] = s; r.m[5] = c;
        return r;
    }

    static Mat4 fromQuat(const Quat& qIn) {
        Quat q = qIn.normalized();
        float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
        float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
        float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

        Mat4 r;
        r.m[0] = 1.0f - 2.0f * (yy + zz);
        r.m[1] = 2.0f * (xy + wz);
        r.m[2] = 2.0f * (xz - wy);
        r.m[3] = 0.0f;

        r.m[4] = 2.0f * (xy - wz);
        r.m[5] = 1.0f - 2.0f * (xx + zz);
        r.m[6] = 2.0f * (yz + wx);
        r.m[7] = 0.0f;

        r.m[8] = 2.0f * (xz + wy);
        r.m[9] = 2.0f * (yz - wx);
        r.m[10] = 1.0f - 2.0f * (xx + yy);
        r.m[11] = 0.0f;

        r.m[12] = 0.0f; r.m[13] = 0.0f; r.m[14] = 0.0f; r.m[15] = 1.0f;
        return r;
    }

    static Mat4 composeTRS(const Vec3& t, const Quat& q, const Vec3& s) {
        return translate(t) * fromQuat(q) * scale(s);
    }

    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                r.m[col * 4 + row] =
                    m[0 * 4 + row] * b.m[col * 4 + 0] +
                    m[1 * 4 + row] * b.m[col * 4 + 1] +
                    m[2 * 4 + row] * b.m[col * 4 + 2] +
                    m[3 * 4 + row] * b.m[col * 4 + 3];
            }
        }
        return r;
    }

    Vec4 transform(const Vec4& v) const {
        return {
            m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
            m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
            m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w
        };
    }

    Vec3 transformPoint(const Vec3& p) const {
        Vec4 r = transform({p.x, p.y, p.z, 1.0f});
        if (std::fabs(r.w) > 1e-6f) {
            return {r.x / r.w, r.y / r.w, r.z / r.w};
        }
        return {r.x, r.y, r.z};
    }

    static Mat4 perspective(float fovYRad, float aspect, float nearZ, float farZ) {
        Mat4 r;
        std::fill(std::begin(r.m), std::end(r.m), 0.0f);
        float f = 1.0f / std::tan(fovYRad * 0.5f);
        r.m[0] = f / std::max(aspect, 0.01f);
        r.m[5] = f;
        r.m[10] = (farZ + nearZ) / (nearZ - farZ);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
        return r;
    }

    static Mat4 ortho(float left, float right, float bottom, float top, float nearZ, float farZ) {
        Mat4 r;
        r.m[0] = 2.0f / (right - left);
        r.m[5] = 2.0f / (top - bottom);
        r.m[10] = -2.0f / (farZ - nearZ);
        r.m[12] = -(right + left) / (right - left);
        r.m[13] = -(top + bottom) / (top - bottom);
        r.m[14] = -(farZ + nearZ) / (farZ - nearZ);
        return r;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        Vec3 f = (center - eye).normalized();
        Vec3 s = f.cross(up).normalized();
        Vec3 u = s.cross(f);
        Mat4 r;
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;  r.m[12] = -s.dot(eye);
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;  r.m[13] = -u.dot(eye);
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] = f.dot(eye);
        r.m[3] = 0.0f; r.m[7] = 0.0f; r.m[11] = 0.0f; r.m[15] = 1.0f;
        return r;
    }
};

struct AABB {
    Vec3 min{0, 0, 0};
    Vec3 max{0, 0, 0};

    Vec3 center() const { return (min + max) * 0.5f; }
    Vec3 extents() const { return (max - min) * 0.5f; }

    bool intersects(const AABB& o) const {
        return (min.x <= o.max.x && max.x >= o.min.x) &&
               (min.y <= o.max.y && max.y >= o.min.y) &&
               (min.z <= o.max.z && max.z >= o.min.z);
    }

    bool contains(const Vec3& p) const {
        return p.x >= min.x && p.x <= max.x &&
               p.y >= min.y && p.y <= max.y &&
               p.z >= min.z && p.z <= max.z;
    }
};

struct Ray {
    Vec3 origin;
    Vec3 dir; // normalized
};

struct Plane {
    Vec3 normal{0, 1, 0};
    float d = 0.0f;

    void normalize() {
        float l = normal.length();
        if (l > 1e-6f) {
            float inv = 1.0f / l;
            normal *= inv;
            d *= inv;
        }
    }

    float distance(const Vec3& p) const {
        return normal.dot(p) + d;
    }
};

struct Frustum {
    std::array<Plane, 6> planes;

    void extractFromVP(const Mat4& vp) {
        // Left
        planes[0].normal = {vp.m[3] + vp.m[0], vp.m[7] + vp.m[4], vp.m[11] + vp.m[8]};
        planes[0].d = vp.m[15] + vp.m[12];
        // Right
        planes[1].normal = {vp.m[3] - vp.m[0], vp.m[7] - vp.m[4], vp.m[11] - vp.m[8]};
        planes[1].d = vp.m[15] - vp.m[12];
        // Bottom
        planes[2].normal = {vp.m[3] + vp.m[1], vp.m[7] + vp.m[5], vp.m[11] + vp.m[9]};
        planes[2].d = vp.m[15] + vp.m[13];
        // Top
        planes[3].normal = {vp.m[3] - vp.m[1], vp.m[7] - vp.m[5], vp.m[11] - vp.m[9]};
        planes[3].d = vp.m[15] - vp.m[13];
        // Near
        planes[4].normal = {vp.m[3] + vp.m[2], vp.m[7] + vp.m[6], vp.m[11] + vp.m[10]};
        planes[4].d = vp.m[15] + vp.m[14];
        // Far
        planes[5].normal = {vp.m[3] - vp.m[2], vp.m[7] - vp.m[6], vp.m[11] - vp.m[10]};
        planes[5].d = vp.m[15] - vp.m[14];

        for (auto& p : planes) p.normalize();
    }

    bool intersectsAABB(const AABB& box) const {
        for (const auto& p : planes) {
            Vec3 positive{
                p.normal.x >= 0.0f ? box.max.x : box.min.x,
                p.normal.y >= 0.0f ? box.max.y : box.min.y,
                p.normal.z >= 0.0f ? box.max.z : box.min.z
            };
            if (p.distance(positive) < 0.0f) {
                return false;
            }
        }
        return true;
    }
};

} // namespace ts

#pragma once
#include <algorithm>
#include <cmath>

namespace openshao::photo_mode {
struct Vec3 { float x = 0, y = 0, z = 0; };

struct Sticks {
    float move_x = 0, move_y = 0, look_x = 0, look_y = 0, down = 0, up = 0, zoom_in = 0, zoom_out = 0;
};

// Eye and a point ahead of it from a row-major view matrix (rows are x, y, z axis columns, then translation).
inline bool FromView(const float m[16], Vec3& eye, Vec3& ahead) {
    const Vec3 x{m[0], m[4], m[8]}, y{m[1], m[5], m[9]}, z{m[2], m[6], m[10]};
    const float length = std::sqrt(z.x * z.x + z.y * z.y + z.z * z.z);
    if (!std::isfinite(length) || length < .5f || length > 2) return false;
    const float tx = m[12], ty = m[13], tz = m[14];
    eye = {-(tx * x.x + ty * y.x + tz * z.x), -(tx * x.y + ty * y.y + tz * z.y), -(tx * x.z + ty * y.z + tz * z.z)};
    ahead = {eye.x + z.x / length * 100, eye.y + z.y / length * 100, eye.z + z.z / length * 100};
    return std::isfinite(eye.x + eye.y + eye.z);
}

// Free camera around the spot where photo mode opened; the game's Y axis points down.
class FreeCamera {
public:
    // Normal and Large areas around the centre; Range and Height are the normal ones.
    static constexpr float Range = 3000, Height = 3000, LargeRange = 8000, LargeHeight = 6000;
    static constexpr float Speed = 900, TurnRate = 1.8f, PitchLimit = 1.4f;
    // Zoom scales the game's field of view: below 1 narrows it (closer), above 1 widens it.
    static constexpr float MinZoom = 0.2f, MaxZoom = 1.5f, ZoomRate = 1.4f;
    static constexpr float TargetDistance = 100, DeadZone = 0.2f;

    void Begin(Vec3 eye, Vec3 target) {
        origin_ = center_ = eye_ = eye;
        range_ = Range;
        height_ = Height;
        const float dx = target.x - eye.x, dy = target.y - eye.y, dz = target.z - eye.z;
        const float flat = std::sqrt(dx * dx + dz * dz);
        yaw_ = std::atan2(dx, dz);
        pitch_ = std::clamp(std::atan2(dy, flat), -PitchLimit, PitchLimit);
        zoom_ = 1;
    }
    // The area the camera may move in: a cylinder around centre; a range of 0 removes the limit.
    void SetArea(Vec3 center, float range, float height) {
        center_ = center;
        range_ = range;
        height_ = height;
    }
    void Reset() { Begin(origin_, Add(origin_, Forward(), TargetDistance)); }
    void Update(const Sticks& input, float seconds) {
        seconds = std::clamp(seconds, 0.f, 0.1f);
        const float zoom = std::clamp(input.zoom_out, 0.f, 1.f) - std::clamp(input.zoom_in, 0.f, 1.f);
        zoom_ = std::clamp(zoom_ * std::exp(zoom * ZoomRate * seconds), MinZoom, MaxZoom);
        // Turning slows down with the zoom so a narrow view stays easy to aim.
        const float turn = TurnRate * std::min(1.f, zoom_) * seconds;
        yaw_ += Filter(input.look_x) * turn;
        pitch_ = std::clamp(pitch_ - Filter(input.look_y) * turn, -PitchLimit, PitchLimit);
        const float forward_x = std::sin(yaw_), forward_z = std::cos(yaw_);
        const float step = Speed * seconds, move_x = Filter(input.move_x), move_y = Filter(input.move_y);
        eye_.x += (forward_x * move_y + forward_z * move_x) * step;
        eye_.z += (forward_z * move_y - forward_x * move_x) * step;
        eye_.y += (std::clamp(input.down, 0.f, 1.f) - std::clamp(input.up, 0.f, 1.f)) * step;
        if (range_ <= 0) return;
        const float dx = eye_.x - center_.x, dz = eye_.z - center_.z, flat = std::sqrt(dx * dx + dz * dz);
        if (flat > range_) { eye_.x = center_.x + dx * range_ / flat; eye_.z = center_.z + dz * range_ / flat; }
        eye_.y = std::clamp(eye_.y, center_.y - height_, center_.y + height_);
    }
    Vec3 Eye() const { return eye_; }
    Vec3 Target() const { return Add(eye_, Forward(), TargetDistance); }
    Vec3 Origin() const { return origin_; }
    Vec3 Center() const { return center_; }
    float Yaw() const { return yaw_; }
    float Pitch() const { return pitch_; }
    float Zoom() const { return zoom_; }

private:
    static float Filter(float value) {
        const float magnitude = std::abs(value);
        if (magnitude <= DeadZone) return 0;
        return std::copysign(std::min(1.f, (magnitude - DeadZone) / (1 - DeadZone)), value);
    }
    Vec3 Forward() const {
        return {std::sin(yaw_) * std::cos(pitch_), std::sin(pitch_), std::cos(yaw_) * std::cos(pitch_)};
    }
    static Vec3 Add(Vec3 a, Vec3 b, float scale) { return {a.x + b.x * scale, a.y + b.y * scale, a.z + b.z * scale}; }

    Vec3 origin_, center_, eye_;
    float range_ = Range, height_ = Height;
    float yaw_ = 0, pitch_ = 0, zoom_ = 1;
};
}

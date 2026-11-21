#include "launcher.h"
#include "controller.h"
#include "keyboard.h"
#include "camera_framing.h"
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr uint32_t CameraGateReturn = 0x5D99B, CameraInputReturn = 0x5D9F9;
constexpr uint32_t CameraGateRoutine = 0x13A94, CameraModeSwitchReturn = 0x5B5B7;
constexpr uint32_t CameraDeadZone = 0x4E6460, CurrentRoom = 0x505970;
constexpr uint32_t Preferences = 0x5ACEBC, GameMode = 0x88;
constexpr uint32_t World = 0x639224, Loading = 0x6C92F8;
constexpr uint32_t HorizontalSample = 8, VerticalSample = 4;
constexpr uint32_t ScreenStack = 0x6C9308, Cinematic = 0x6C4198, WorldPaused = 0x5AE5B0;
constexpr float TicksToLimit = 24;

struct HeldAngle {
    uint32_t world = 0, room = 0;
    float horizontal = 0, vertical = 0;
    bool wait_for_neutral = false;
};
thread_local HeldAngle held;
thread_local unsigned recenter_buttons = 0;
thread_local openshao::camera::Framing framing;

bool RecenterPressed() {
    const unsigned down = unsigned(openshao::controller::CameraRecenterDown()) |
                          (unsigned(openshao::keyboard::CameraRecenterDown()) << 1);
    const bool pressed = (down & ~recenter_buttons) != 0;
    recenter_buttons = down;
    return pressed;
}

bool SoloScene(uint32_t world) {
    const uint32_t preferences = kernel::R32(Preferences);
    return world && preferences && kernel::R32(preferences + GameMode) == 0 && !kernel::R32(Loading);
}

bool RightStickShouldRecenter(const X86Context& ctx) {
    using namespace openshao;
    const auto& settings = launcher::Current();
    if (!settings.hold_camera_angle || settings.bindings[bindings::RecenterCamera] != "rightstick" ||
        settings.bindings[bindings::RightStickClick] != "rightstick" || !controller::CameraRecenterDown() ||
        !SoloScene(kernel::R32(World))) return false;

    // R3 normally cycles camera modes. Give recenter priority only while the current mode accepts panning.
    auto camera = ctx;
    camera.ecx = ctx.esi;
    return (kernel::CallGuest(camera, CameraGateRoutine, {}) & 255) != 0;
}

void Advance(float& sample, float& offset, float dead_zone, float speed) {
    const float input = std::max(0.f, std::abs(sample) - dead_zone);
    offset = std::clamp(offset + std::copysign(input * speed / TicksToLimit, sample),
                        -128.f + dead_zone, 127.f - dead_zone);
    // Reapply the dead zone for the retail camera's own input conversion.
    sample = offset == 0 ? 0 : offset + std::copysign(dead_zone, offset);
}

void UpdateFraming(const X86Context& ctx,bool allowed) {
    const auto& settings = openshao::launcher::Current();
    const auto world = kernel::R32(World), room = kernel::R32(CurrentRoom);
    if (!allowed || !settings.hold_camera_angle || !settings.smooth_camera_framing || !SoloScene(world) ||
        kernel::R32(ScreenStack) || kernel::G<uint8_t>(Cinematic) || kernel::G<uint8_t>(WorldPaused)) {
        framing.Reset();
        return;
    }
    // The caller's base yaw/pitch pair precedes manual offsets and the camera's native path constraints.
    const uint32_t pointer = kernel::R32(ctx.esp + 0x28);
    if (!pointer) { framing.Reset(); return; }
    auto& angles = kernel::G<openshao::camera::Angles>(pointer);
    angles = framing.Update(angles,world,room,ctx.esi);
}

void UpdateCameraInput(const X86Context& ctx) {
    const bool recenter = RecenterPressed();
    const uint32_t world = kernel::R32(World);
    const auto& settings = openshao::launcher::Current();
    if ((ctx.eax & 255) || !settings.hold_camera_angle || !SoloScene(world)) {
        held = {};
        return;
    }

    const uint32_t room = kernel::R32(CurrentRoom);
    if (held.world != world || held.room != room) held = {world, room};

    // At 0x5D9F9 the right-stick axes are decoded; room limits, inversion and easing stay native.
    const float dead_zone = kernel::G<float>(CameraDeadZone);
    auto& horizontal = kernel::G<float>(ctx.esp + HorizontalSample);
    auto& vertical = kernel::G<float>(ctx.esp + VerticalSample);
    if (recenter) {
        held.horizontal = held.vertical = 0;
        held.wait_for_neutral = true;
    }
    if (held.wait_for_neutral) {
        // Let the retail easing reach center without fighting a still-deflected stick.
        held.wait_for_neutral = std::abs(horizontal) > dead_zone || std::abs(vertical) > dead_zone;
        horizontal = vertical = 0;
        return;
    }
    Advance(horizontal, held.horizontal, dead_zone, settings.camera_speed / 100.f);
    Advance(vertical, held.vertical, dead_zone, settings.camera_speed / 100.f);
}
}

X86_EXTERN_FUNC(__imp__sub_00013A94);
X86_FUNC(sub_00013A94) {
    const bool camera_gate = kernel::R32(ctx.esp) == CameraGateReturn;
    __imp__sub_00013A94(ctx, base);
    if (camera_gate) {
        const bool allowed = (ctx.eax & 255) != 0;
        if (!allowed) { RecenterPressed(); held = {}; }
        UpdateFraming(ctx,allowed);
    }
}

X86_EXTERN_FUNC(__imp__sub_001973D0);
X86_FUNC(sub_001973D0) {
    const bool camera_input = kernel::R32(ctx.esp) == CameraInputReturn;
    __imp__sub_001973D0(ctx, base);
    if (camera_input) UpdateCameraInput(ctx);
}

X86_EXTERN_FUNC(__imp__sub_0015AE70);
X86_FUNC(sub_0015AE70) {
    const bool camera_mode_switch = kernel::R32(ctx.esp) == CameraModeSwitchReturn;
    __imp__sub_0015AE70(ctx, base);
    if (camera_mode_switch && (ctx.eax & 255) && RightStickShouldRecenter(ctx)) ctx.eax &= ~255u;
}


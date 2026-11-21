#include "launcher.h"
#include "camera_framing.h"
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"
#include <array>
#include <cstdio>
#include <vector>

namespace kernel { uint8_t* g_base = nullptr; }
namespace {
std::vector<uint8_t> memory(0x700000);
openshao::launcher::Settings settings;
constexpr uint32_t Stack = 0x1000, Preferences = 0x4000, Camera = 0x5000;
constexpr uint32_t Room = 0x505970, World = 0x639224, Loading = 0x6C92F8;
constexpr uint32_t CameraGateReturn = 0x5D99B, CameraInputReturn = 0x5D9F9;
constexpr uint32_t Horizontal = Stack + 16, Vertical = Stack + 12;
bool camera_allowed = true, player_busy = false;
bool recenter_controller = false, recenter_keyboard = false;
unsigned failures = 0;
void Check(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}
}

namespace openshao::launcher { const Settings& Current() { return settings; } }
namespace openshao::controller { bool CameraRecenterDown() { return recenter_controller; } }
namespace openshao::keyboard { bool CameraRecenterDown() { return recenter_keyboard; } }

X86_FUNC(__imp__sub_00013A94) {
    ctx.eax = 0xAA00 | camera_allowed;
    ctx.esp += 4;
}
X86_FUNC(__imp__sub_001973D0) {
    ctx.eax = 0xBB00 | player_busy;
    ctx.esp += 8;
}
X86_WEAK_FUNC(sub_00013A94, __imp__sub_00013A94)
X86_WEAK_FUNC(sub_001973D0, __imp__sub_001973D0)
X86_FUNC(__imp__sub_0015AE70) {
    ctx.eax = 0xCC01;
    ctx.esp += 12;
}
X86_WEAK_FUNC(sub_0015AE70, __imp__sub_0015AE70)
namespace kernel {
uint32_t CallGuest(X86Context& ctx, uint32_t fn, const uint32_t*, size_t count) {
    Check(fn == 0x13A94 && count == 0 && ctx.ecx == Camera,
          "The camera mode conflict must use the active camera's native input gate");
    ctx.eax = 0xAA00 | camera_allowed;
    return ctx.eax;
}
}

namespace {
void CameraGate(uint32_t caller = CameraGateReturn) {
    X86Context ctx{};
    ctx.esp = Stack;
    ctx.esi = Camera;
    kernel::W32(Stack, caller);
    kernel::W32(Stack+0x2C,0x8000);
    sub_00013A94(ctx, memory.data());
    Check(ctx.esp == Stack + 4 && ctx.eax == (0xAA00 | camera_allowed), "Preserve the original camera gate result");
}

bool NativeCameraModeSwitch(uint32_t caller = 0x5B5B7) {
    X86Context ctx{};
    ctx.esp = Stack;
    ctx.esi = Camera;
    kernel::W32(Stack, caller);
    sub_0015AE70(ctx, memory.data());
    Check(ctx.esp == Stack + 12 && (ctx.eax & ~255u) == 0xCC00,
          "The recenter shortcut preserves the original input predicate ABI");
    return (ctx.eax & 255) != 0;
}

using Axes = std::array<float, 2>;
Axes CameraInput(float x = 0, float y = 0, uint32_t caller = CameraInputReturn) {
    X86Context ctx{};
    ctx.esp = Stack;
    ctx.esi = Camera;
    kernel::W32(Stack, caller);
    kernel::G<float>(Horizontal) = x;
    kernel::G<float>(Vertical) = y;
    sub_001973D0(ctx, memory.data());
    Check(ctx.esp == Stack + 8 && ctx.eax == (0xBB00 | player_busy), "Preserve the original player gate result");
    return {kernel::G<float>(Horizontal), kernel::G<float>(Vertical)};
}
}

int main() {
    kernel::g_base = memory.data();
    kernel::W32(0x5ACEBC, Preferences);
    kernel::W32(World, 0x6000);
    kernel::W32(Room, 96);
    kernel::G<float>(0x4E6460) = 20;
    Check(CameraInput(127, -128) == Axes{127, -128}, "Disabled enhancement preserves the native camera input");
    settings.hold_camera_angle = true;
    Check(CameraInput(15, -20) == Axes{}, "Stick drift within the native dead zone never moves the camera");
    const auto moved = CameraInput(127, -128);
    Check(moved[0] > 20 && moved[0] < 127 && moved[1] < -20 && moved[1] > -128,
          "Deflecting the stick moves the angle progressively in both directions");
    camera_allowed = false;
    CameraGate(0x2222);
    camera_allowed = true;
    Check(CameraInput(60, 70, 0x2222) == Axes{60, 70}, "Other calls to the player gate keep their input intact");
    Check(CameraInput() == moved && CameraInput(20, -19) == moved, "Release and drift preserve the adjusted angle");
    for (unsigned tick = 0; tick < 120; ++tick) CameraInput(127, -128);
    Check(CameraInput() == Axes{127, -128}, "Hold stops at the native stick limits");
    const auto reversed = CameraInput(-128, 0);
    Check(reversed[0] > 20 && reversed[0] < 127 && reversed[1] == -128,
          "Opposite input moves back from the held angle without snapping across center");

    camera_allowed = false;
    CameraGate();
    camera_allowed = true;
    Check(CameraInput() == Axes{}, "A camera section that disables input clears the held angle");
    CameraInput(127);
    player_busy = true;
    Check(CameraInput(-128, 127) == Axes{-128, 127}, "Input blocked by the game remains under the native gate");
    player_busy = false;
    Check(CameraInput() == Axes{}, "When the native gate reopens, blocked input has not accumulated");

    CameraInput(127);
    kernel::W32(Room, 97);
    Check(CameraInput() == Axes{}, "A new room starts centered");
    CameraInput(127);
    kernel::W32(Loading, 1);
    Check(CameraInput(127) == Axes{127, 0}, "A loading scene keeps original camera input");
    kernel::W32(Loading, 0);
    Check(CameraInput() == Axes{}, "Reloading a room clears the angle even when the room ID stays the same");
    CameraInput(127);
    kernel::W32(World, 0x7000);
    Check(CameraInput() == Axes{}, "A new world starts centered");

    for (uint32_t mode : {1u, 2u}) {
        kernel::W32(Preferences + 0x88, mode);
        Check(CameraInput(127, -128) == Axes{127, -128}, "Multiplayer keeps the original camera input");
    }
    kernel::W32(Preferences + 0x88, 0);
    Check(CameraInput() == Axes{}, "Returning to solo starts centered");
    CameraInput(127);
    settings.hold_camera_angle = false;
    Check(CameraInput() == Axes{}, "Disabling the setting restores the native neutral input");
    settings.hold_camera_angle = true;
    Check(CameraInput() == Axes{}, "Re-enabling the setting starts centered");

    CameraInput(127, -128);
    recenter_controller = true;
    Check(CameraInput(127, -128) == Axes{}, "A controller tap resets both camera axes, even with the stick deflected");
    Check(CameraInput(127, -128) == Axes{}, "Recentering waits for neutral camera input before panning again");
    Check(CameraInput(20, -19) == Axes{}, "Returning inside the dead zone leaves the camera centered");
    const auto panned_while_held = CameraInput(127, -128);
    Check(panned_while_held[0] > 20 && panned_while_held[1] < -20,
          "Holding the shortcut must not repeatedly recenter after the camera stick is neutral");
    recenter_keyboard = true;
    Check(CameraInput() == Axes{}, "The keyboard can recenter independently while the controller shortcut is held");
    recenter_controller = recenter_keyboard = false;
    CameraInput(127);
    recenter_keyboard = true;
    Check(CameraInput() == Axes{}, "Releasing the shortcut permits another tap");

    recenter_controller = true;
    Check(NativeCameraModeSwitch(), "The native camera mode switch remains available with the default L3 shortcut");
    settings.bindings[openshao::bindings::RecenterCamera] = "rightstick";
    Check(!NativeCameraModeSwitch() && NativeCameraModeSwitch(0x2222),
          "An R3 recenter tap suppresses only the competing native camera mode switch");
    camera_allowed = false;
    Check(NativeCameraModeSwitch(), "The native mode switch can leave a camera mode where the enhancement cannot pan");
    camera_allowed = true;
    recenter_controller = false;
    Check(NativeCameraModeSwitch(), "A native right stick click from another input remains available");
    recenter_keyboard = false;
    camera_allowed = false;CameraGate();camera_allowed = true;
    settings.camera_speed = 200;
    const auto fast = CameraInput(127);
    Check(std::abs((fast[0]-20)-2*(moved[0]-20))<.001f,"Camera speed doubles angle progress without changing the native dead zone");

    openshao::camera::Framing framing;
    using openshao::camera::Angles;
    const Angles initial{0,0}, change{.15f,.1f};
    Check(framing.Update(initial,1,96,10)==initial,"The first update uses the native framing");
    const auto blended=framing.Update(change,1,96,10);
    Check(blended[0]>0 && blended[0]<change[0] && blended[1]>0 && blended[1]<change[1],
          "A tracking turn blends toward its new framing");
    Check(framing.Update(change,1,97,10)==change,"A room change uses the new framing immediately");
    const Angles cut{2,0};
    Check(framing.Update(cut,1,97,10)==cut,"An authored camera cut remains immediate");
    settings.smooth_camera_framing=true;
    kernel::G<Angles>(0x8000)={0,0};CameraGate();
    kernel::G<Angles>(0x8000)=change;CameraGate();
    Check(kernel::G<Angles>(0x8000)[0]>0 && kernel::G<Angles>(0x8000)[0]<change[0],
          "The native camera gate smooths its base angles before manual offsets");
    kernel::G<uint8_t>(0x6C4198)=1;kernel::G<Angles>(0x8000)=change;CameraGate();
    Check(kernel::G<Angles>(0x8000)==change,"Scripted scenes use their native framing immediately");
    return failures != 0;
}

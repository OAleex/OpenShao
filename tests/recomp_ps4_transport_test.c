#define SDL_MAIN_HANDLED
#include "SDL_internal.h"
#include "joystick/hidapi/SDL_hidapijoystick_c.h"
#include "joystick/hidapi/SDL_hidapi_rumble.h"
#include "joystick/usb_ids.h"
#include <stdio.h>
#include <stdlib.h>

static Uint64 clock_ms;
static bool fresh_report;
static int read_result;
static unsigned removals, insertions, requests;
static SDL_JoystickID device_id = 501;

static Uint64 SDLCALL TestTicks(void) { return clock_ms; }
static SDL_Joystick * SDLCALL TestJoystick(SDL_JoystickID id) { (void)id; return NULL; }
static int SDLCALL TestRead(SDL_hid_device *device, unsigned char *data, size_t size, int timeout)
{
    (void)device; (void)timeout;
    if (fresh_report && size >= 78) {
        const Uint8 hidp = 0xA1;
        SDL_memset(data, 0, 78);
        data[0] = 17;
        data[1] = 0x80;
        data[8] = 1; /* L1 is held in every report. */
        const Uint32 crc = SDL_crc32(SDL_crc32(0, &hidp, 1), data, 74);
        for (unsigned i = 0; i < 4; ++i) data[74 + i] = (Uint8)(crc >> (8 * i));
        fresh_report = false;
        return 78;
    }
    return read_result;
}
static void TestDisconnect(SDL_HIDAPI_Device *device, SDL_JoystickID id)
{
    if (id != device_id) abort();
    ++removals;
    device->num_joysticks = 0;
}
static bool TestConnect(SDL_HIDAPI_Device *device, SDL_JoystickID *id)
{
    ++insertions;
    device->num_joysticks = 1;
    if (id) *id = device_id;
    return true;
}
static bool TestUsb(const char *serial) { (void)serial; return false; }
static bool TestRumbleLock(void) { return false; }
static int TestSendEffect(SDL_HIDAPI_Device *device, const Uint8 *data, int size)
{
    (void)device;
    if (size == 78 && data[0] == 17 && (data[1] & 0xC0) == 0xC0 && data[3] == 0) ++requests;
    return size;
}

#undef SDL_GetTicks
#undef SDL_GetJoystickFromID
#undef SDL_hid_read_timeout
#define SDL_GetTicks TestTicks
#define SDL_GetJoystickFromID TestJoystick
#define SDL_hid_read_timeout TestRead
#define HIDAPI_JoystickDisconnected TestDisconnect
#define HIDAPI_JoystickConnected TestConnect
#define HIDAPI_HasConnectedUSBDevice TestUsb
#define SDL_HIDAPI_LockRumble TestRumbleLock
#define SDL_HIDAPI_SendRumble TestSendEffect
#define SDL_HIDAPI_DriverPS4 OpenShaoTestPS4Driver
#include OPENSHAO_SDL_PS4_SOURCE

static void Check(bool ok, const char *message)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}
static void Setup(SDL_HIDAPI_Device *device, SDL_DriverPS4_Context *ctx)
{
    SDL_zero(*device);
    SDL_zero(*ctx);
    device->context = ctx;
    ctx->device = device;
    device->is_bluetooth = true;
    device->vendor_id = USB_VENDOR_SONY;
    device->product_id = USB_PRODUCT_SONY_DS4_SLIM;
    device->joysticks = &device_id;
    device->num_joysticks = 1;
    ctx->official_controller = true;
    ctx->effects_supported = true;
    ctx->report_interval = 4;
    ctx->enhanced_reports = true;
    ctx->last_packet = 1000;
    ctx->last_state.rgucButtonsHatAndCounter[1] = 1;
    clock_ms = 1000;
    read_result = 0;
    removals = insertions = requests = 0;
    fresh_report = false;
}
int main(void)
{
    SDL_HIDAPI_Device device;
    SDL_DriverPS4_Context ctx;

    Setup(&device, &ctx);
    ctx.enhanced_reports = false;
    ctx.enhanced_mode_available = true;
    SDL_PS4EnhancedReportsChanged(&ctx, NULL, NULL, "auto");
    Check(requests == 1, "Auto mode must request continuous reports from a wireless Sony PS4 before rumble is used");
    Check(!ctx.enhanced_reports, "Requesting continuous reports must not pretend negotiation has already succeeded");
    clock_ms = 9000;
    HIDAPI_DriverPS4_UpdateDevice(&device);
    Check(removals == 0, "The heartbeat timeout starts only after enhanced reports have been observed");

    Setup(&device, &ctx);
    ctx.enhanced_mode_available = true;
    ctx.official_controller = false;
    device.vendor_id = 0x1234;
    SDL_PS4EnhancedReportsChanged(&ctx, NULL, NULL, "auto");
    Check(!ctx.enhanced_mode && requests == 0, "Auto mode must preserve third-party reporting policy");
    ctx.official_controller = true;
    device.vendor_id = USB_VENDOR_SONY;
    device.product_id = USB_PRODUCT_SONY_DS4_STRIKEPAD;
    SDL_PS4EnhancedReportsChanged(&ctx, NULL, NULL, "auto");
    Check(!ctx.enhanced_mode && requests == 0, "Auto mode must preserve the policy for other Sony-identified devices");
    device.product_id = USB_PRODUCT_SONY_DS4;
    SDL_PS4EnhancedReportsChanged(&ctx, NULL, NULL, "0");
    Check(!ctx.enhanced_mode && requests == 0, "An explicit enhanced-report opt-out must be respected");

    Setup(&device, &ctx);
    clock_ms = 1501;
    Check(HIDAPI_DriverPS4_UpdateDevice(&device), "A stalled read is not a fatal HID handle error");
    Check(removals == 1 && device.num_joysticks == 0,
          "A wireless Sony PS4 with stopped reports must signal removal instead of retaining held input");
    clock_ms += 501;
    HIDAPI_DriverPS4_UpdateDevice(&device);
    Check(removals == 1, "Communication loss must not repeatedly remove the same device");
    fresh_report = true;
    HIDAPI_DriverPS4_UpdateDevice(&device);
    Check(insertions == 1 && device.num_joysticks == 1,
          "Fresh reports must allow the same Bluetooth device to reconnect");

    Setup(&device, &ctx);
    for (unsigned i = 0; i < 20; ++i) {
        clock_ms += 400;
        fresh_report = true;
        HIDAPI_DriverPS4_UpdateDevice(&device);
        Check(removals == 0 && ctx.last_packet == clock_ms,
              "Fresh unchanged held-button reports must keep the controller connected");
    }

    Setup(&device, &ctx);
    clock_ms = 1200;
    HIDAPI_DriverPS4_UpdateDevice(&device);
    Check(removals == 0, "A brief gap must not disconnect a healthy controller");

    Setup(&device, &ctx);
    ctx.enhanced_reports = false;
    clock_ms = 9000;
    HIDAPI_DriverPS4_UpdateDevice(&device);
    Check(removals == 0, "Basic report-on-change controllers must not be treated as a missing heartbeat");

    Setup(&device, &ctx);
    ctx.official_controller = false;
    device.vendor_id = 0x1234;
    clock_ms = 9000;
    HIDAPI_DriverPS4_UpdateDevice(&device);
    Check(removals == 0, "Third-party controller reporting policies must remain intact");

    Setup(&device, &ctx);
    device.is_bluetooth = false;
    clock_ms = 9000;
    HIDAPI_DriverPS4_UpdateDevice(&device);
    Check(removals == 0, "Wired devices must not use the Bluetooth heartbeat rule");

    Setup(&device, &ctx);
    read_result = -1;
    Check(!HIDAPI_DriverPS4_UpdateDevice(&device) && removals == 1,
          "A confirmed HID read failure must still remove the device immediately");
    puts("PASS: PS4 transport loss, held input, compatible reporting and reconnect");
    return 0;
}

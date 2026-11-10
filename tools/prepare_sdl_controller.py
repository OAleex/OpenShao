"""Adapt PS4 Bluetooth liveness without changing the pinned SDL checkout."""
import argparse
from pathlib import Path


def adapt(text):
    changes = (
        ('''static void HIDAPI_DriverPS4_TickleBluetooth(SDL_HIDAPI_Device *device)
{
    SDL_DriverPS4_Context *ctx = (SDL_DriverPS4_Context *)device->context;

    if (ctx->enhanced_reports) {
        // This is just a dummy packet''', '''static bool HIDAPI_DriverPS4_IsSonyDualShock4Bluetooth(const SDL_HIDAPI_Device *device)
{
    return device->is_bluetooth && device->vendor_id == USB_VENDOR_SONY &&
           (device->product_id == USB_PRODUCT_SONY_DS4 || device->product_id == USB_PRODUCT_SONY_DS4_SLIM);
}

static void HIDAPI_DriverPS4_TickleBluetooth(SDL_HIDAPI_Device *device)
{
    SDL_DriverPS4_Context *ctx = (SDL_DriverPS4_Context *)device->context;

    if (ctx->enhanced_reports) {
        // OpenShao: continuous Sony reports have stopped. Release input even
        // when the Bluetooth stack has not reported removal yet.
        if (HIDAPI_DriverPS4_IsSonyDualShock4Bluetooth(device) && device->num_joysticks > 0) {
            HIDAPI_JoystickDisconnected(device, device->joysticks[0]);
        }
        // This is just a dummy packet'''),
        ('''            HIDAPI_DriverPS4_SetEnhancedReportHint(ctx, PS4_ENHANCED_REPORT_HINT_AUTO);''',
         '''            // OpenShao: request a continuous stream before the first rumble.
            // The timeout still requires an observed enhanced report.
            HIDAPI_DriverPS4_SetEnhancedReportHint(ctx,
                HIDAPI_DriverPS4_IsSonyDualShock4Bluetooth(ctx->device)
                    ? PS4_ENHANCED_REPORT_HINT_ON : PS4_ENHANCED_REPORT_HINT_AUTO);'''),
    )
    for old, new in changes:
        if text.count(old) != 1:
            raise ValueError(f"The pinned SDL PS4 source changed at {old[:80]!r}")
        text = text.replace(old, new, 1)
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    text = adapt(args.source.read_text(encoding='utf-8'))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text(encoding='utf-8') != text:
        args.output.write_text(text, encoding='utf-8', newline='\n')


if __name__ == '__main__':
    main()

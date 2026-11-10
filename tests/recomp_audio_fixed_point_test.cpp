#include "audio_fixed_point.h"
#include <cstdio>
#include <cstdlib>
using namespace openshao::audio::signal;
static void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main() {
    Check(DecodeSample(0xFFFFFF) == -1 && DecodeSample(0x800000) == -8388608 && DecodeSample(0x7FFFFF) == 8388607,
          "signed 24-bit sample decoding must preserve the full positive and negative range");
    Check(ClampSample(8388608) == 8388607 && ClampSample(-8388609) == -8388608,
          "signal overload must saturate instead of wrapping its sign");
    Check(TruncateProduct(2ll*4194304*4194304) == 2097152 && TruncateProduct(-2ll*4194304*4194304-1) == -2097153,
          "fractional products must retain precision until sample storage");
    Check(TruncateProduct(2ll*8388608*8388608) == 8388607, "full-scale positive products must saturate");
    Check(RoundProduct((1ll<<24)+0x800000) == 2 && RoundProduct((2ll<<24)+0x800000) == 2 &&
          RoundProduct(-(2ll<<24)+0x800000) == -2 && RoundProduct(-(3ll<<24)+0x800000) == -2,
          "fractional ties must use convergent rounding in both signs");
    Check(QuantizeSample(.5f) == 4194304 && QuantizeSample(-1.f) == -8388608 && QuantizeSample(1.f) == 8388607,
          "float conversion must preserve scale and clamp full scale");
    Check(NormalizeSample(4194304) == .5f, "native audio conversion must preserve unity gain");
    Check(WrapAccumulator(1ll<<55) == -(1ll<<55) && ShiftAccumulator(1ll<<54, 1) == -(1ll<<55) &&
          ShiftAccumulator(-(1ll<<54), 2) == 0,
          "wide filter arithmetic must retain the original signed 56-bit wrap behavior");
    std::puts("PASS: fixed-point scale, sign, saturation, fractional precision and convergent rounding");
}

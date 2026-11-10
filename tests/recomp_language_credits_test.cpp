#include "cpu/x86_ops.h"
#include "difficulty.h"
namespace openshao::difficulty { void PopupFooter(X86Context&) {} }
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
std::array<uint8_t, 0x8000> memory{};
bool enabled = true;
unsigned comparisons = 0, draws = 0, input_reads = 0;
uint32_t drawn_text = 0, drawn_style = 0, input_bits = 0;
constexpr uint32_t stack = 0x1000, text = 0x2000;

void Check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
uint32_t Read(uint32_t address) {
    uint32_t value; std::memcpy(&value, memory.data() + address, 4); return value;
}
void Write(uint32_t address, uint32_t value) {
    std::memcpy(memory.data() + address, &value, 4);
}
X86Context Frame(uint32_t caller) {
    X86Context ctx{};
    ctx.esp = stack; ctx.edi = text; ctx.ecx = 0x67C658;
    ctx.ebx = 0xABCD1234; ctx.ebp = 0x1234ABCD; ctx.esi = 0xDEADBEEF;
    Write(stack, caller); return ctx;
}
void Line(const char* value) {
    std::memset(memory.data() + text, 0, 128);
    std::memcpy(memory.data() + text, value, std::strlen(value) + 1);
}
}

namespace openshao::language { bool CreditsEnabled() { return enabled; } }

// These stubs model the three original call boundaries, not an Xbox game run.
X86_FUNC(__imp__sub_000EBD00) {
    ++comparisons;
    const auto source = Read(ctx.esp + 4);
    ctx.eax = uint32_t(std::strncmp(reinterpret_cast<char*>(base + source),
                                 "CREDITS HEADING ", Read(ctx.esp + 12)));
    ctx.ecx = 0; ctx.esp += 4;
}
X86_FUNC(__imp__sub_00152960) {
    (void)base;
    ++draws; drawn_style = Read(ctx.esp + 16); drawn_text = Read(ctx.esp + 20);
    ctx.eax = 0xCAFEBABE; ctx.ecx = 0x12345678; ctx.esp += 4;
}
X86_FUNC(__imp__sub_0015ABF0) {
    (void)base;
    ++input_reads; ctx.eax = input_bits; ctx.ecx = 0x87654321; ctx.esp += 8;
}
X86_EXTERN_FUNC(sub_000EBD00);
X86_EXTERN_FUNC(sub_00152960);
X86_EXTERN_FUNC(sub_0015ABF0);

int main() {
    const auto compare = [](const char* line, uint32_t caller, uint32_t prefix = 0x49D4BC,
                            uint32_t length = 16) {
        Line(line); auto ctx = Frame(caller);
        Write(stack + 4, text); Write(stack + 8, prefix); Write(stack + 12, length);
        sub_000EBD00(ctx, memory.data());
        Check(ctx.esp == stack + 4 && ctx.ebx == 0xABCD1234 && ctx.edi == text &&
              ctx.esi == 0xDEADBEEF && ctx.ebp == 0x1234ABCD,
              "comparison preserves cdecl stack and nonvolatile registers");
        return ctx.eax;
    };
    Check(compare("#Voice direction", 0x4B3E6) == 0 && comparisons == 0,
          "credits recognize literal hash headings without the original comparison");
    Check(compare("CREDITS HEADING Original", 0x4B3E6) == 0 && comparisons == 1,
          "original credits heading syntax still uses the original comparison");
    Check(compare("Normal name", 0x4B3E6) != 0,
          "ordinary credits lines retain the normal style");
    Check(compare(" #Indented", 0x4B3E6) != 0,
          "a hash must be the first byte of a literal heading");
    Check(compare("#Controller token", 0x123456) != 0,
          "hash outside the credits caller retains original behavior");
    Check(compare("#Other comparison", 0x4B3E6, 0x456789) != 0 &&
          compare("#Other length", 0x4B3E6, 0x49D4BC, 8) != 0,
          "comparison hook requires the original credits prefix and bound");
    enabled = false;
    Check(compare("#Disabled pack", 0x4B3E6) != 0,
          "inactive packs preserve original heading recognition");
    enabled = true;

    const auto draw = [](const char* line, uint32_t caller, uint32_t style, uint32_t pointer) {
        Line(line); auto ctx = Frame(caller);
        Write(stack + 16, style); Write(stack + 20, pointer);
        sub_00152960(ctx, memory.data());
        Check(Read(stack + 20) == pointer && ctx.esp == stack + 4 &&
              ctx.eax == 0xCAFEBABE && ctx.ecx == 0x12345678 && ctx.ebx == 0xABCD1234,
              "drawing restores its argument slot and preserves the original return state");
        Check(drawn_style == style, "drawing preserves the selected original style");
        return drawn_text;
    };
    Check(draw("#Direction", 0x4B419, 2, text + 16) == text + 1,
          "literal heading draw skips exactly the hash byte");
    Check(draw("CREDITS HEADING Original", 0x4B419, 2, text + 16) == text + 16,
          "original heading draw retains the 16-byte prefix skip");
    Check(draw("Normal name", 0x4B419, 1, text) == text &&
          draw("#Outside", 0x765432, 2, text + 16) == text + 16 &&
          draw("#Normal style", 0x4B419, 1, text) == text &&
          draw("#Different argument", 0x4B419, 2, text + 8) == text + 8,
          "unrelated callers, styles, and argument layouts remain unchanged");
    enabled = false;
    Check(draw("#Disabled", 0x4B419, 2, text + 16) == text + 16,
          "inactive pack does not change the draw pointer");
    enabled = true;

    const auto input = [](uint32_t caller, uint32_t object = 0x67C658, uint32_t kind = 4) {
        auto ctx = Frame(caller); ctx.ecx = object; Write(stack + 4, kind);
        sub_0015ABF0(ctx, memory.data());
        Check(ctx.esp == stack + 8 && ctx.ecx == 0x87654321 && ctx.ebx == 0xABCD1234,
              "input wrapper preserves the original RET4 and register changes");
        return ctx.eax;
    };
    input_bits = 0xA5F0;
    const auto campaign = input(0xC5D3D);
    Check(campaign == (input_bits & ~0x40u),
          "only the campaign credits skip bit is suppressed");
    const auto transition = [](uint32_t buttons, bool scrolling) {
        return (buttons & 0x40u) != 0 || !scrolling;
    };
    Check(!transition(campaign, true) && transition(campaign, false),
          "campaign credits continue while scrolling and exit on natural completion");
    Check(transition(input(0x765432), true),
          "extras and other input callers retain their manual exit");
    Check(input(0xC5D3D, 0x123456) == input_bits &&
          input(0xC5D3D, 0x67C658, 2) == input_bits,
          "input hook requires the audited controller object and accessor kind");
    enabled = false;
    Check(input(0xC5D3D) == input_bits, "inactive pack preserves campaign skip input");
    Check(draws == 7 && input_reads == 5, "every draw and input call forwards once");
    std::puts("PASS: Xbox credits headings, campaign skip scope, original forwarding and ABI");
}

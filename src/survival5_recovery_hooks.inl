namespace {
bool survival5_start_pending = false;
bool OriginalSurvival5Room() {
    constexpr uint32_t descriptor = 0x4FFF20;
    return kernel::R32(0x505970) == 0xA4 &&
           kernel::R32(0x5020E8 + 0xA4 * 4) == descriptor &&
           kernel::R32(descriptor + 0x44) == 0xA4 &&
           kernel::R32(descriptor + 0x48) == 0x10001 &&
           kernel::R32(descriptor + 0x4C) == 4213 &&
           kernel::R32(descriptor + 0x50) == 4214;
}
struct Survival5ResourceArgument {
    uint32_t address, original;
    Survival5ResourceArgument(uint32_t at, uint32_t value, uint32_t replacement)
        : address(at), original(value) { kernel::W32(address, replacement); }
    ~Survival5ResourceArgument() { kernel::W32(address, original); }
};
void Survival5ArenaCamera(uint32_t camera) {
    const auto scalar = [camera](uint32_t offset, float value) {
        kernel::W32(camera + offset, std::bit_cast<uint32_t>(value));
    };
    const auto vector = [&scalar](uint32_t offset, float x, float y, float z) {
        scalar(offset, x); scalar(offset + 4, y); scalar(offset + 8, z);
        scalar(offset + 12, 0);
    };
    // 2NDCAMERA has no A4 profile: apply the Pit eye/focus offsets and bounds after construction.
    scalar(0x8AC, 69);
    vector(0x8B0, 999, -807, 634);
    vector(0x8C0, 115, -1375, -100);
    vector(0x8DC, 700, -50000, -6000);
    vector(0x8EC, 11200, 7000, 6000);
    scalar(0x92C, 7000);
    scalar(0xAB0, -150);
}
}

X86_EXTERN_FUNC(__imp__sub_0011DC60);
X86_FUNC(sub_0011DC60) {
    const uint32_t argument = ctx.esp + 4;
    const uint32_t resource = kernel::R32(argument);
    if (!OriginalSurvival5Room() || (resource != 4213 && resource != 4214)) {
        __imp__sub_0011DC60(ctx, base);
        return;
    }
    // The Xbox A4 scene export is incomplete: load the Pit arena and textures, keep A4 gameplay.
    const uint32_t replacement = resource == 4213 ? 4373 : 4374;
    Survival5ResourceArgument restored(argument, resource, replacement);
    LOG_FIRST(4, "openshao graphics: Survival 5 arena resource %u -> %u", resource, replacement);
    if (resource == 4213) survival5_start_pending = true;
    try {
        __imp__sub_0011DC60(ctx, base);
    } catch (...) {
        if (resource == 4213) survival5_start_pending = false;
        throw;
    }
    if (resource == 4213 && !ctx.eax) survival5_start_pending = false;
}

X86_EXTERN_FUNC(__imp__sub_001B9EF0);
X86_FUNC(sub_001B9EF0) {
    const bool arenaCamera = OriginalSurvival5Room() &&
        kernel::R32(ctx.esp) == 0x56BD7 && ctx.edi == 0xA4 &&
        kernel::R32(ctx.esp + 4) == 0x49DED8;
    __imp__sub_001B9EF0(ctx, base);
    // Only the caller's local dispatch register selects the native constructor.
    if (arenaCamera && ctx.eax) ctx.edi = 0xAC;
}

X86_EXTERN_FUNC(__imp__sub_00012F72);
X86_FUNC(sub_00012F72) {
    const bool arenaCamera = OriginalSurvival5Room();
    __imp__sub_00012F72(ctx, base);
    if (arenaCamera && ctx.eax && kernel::R32(ctx.eax) == 0x49E4F0)
        Survival5ArenaCamera(ctx.eax);
}

X86_EXTERN_FUNC(__imp__sub_001B7470);
X86_FUNC(sub_001B7470) {
    // Wait for the player before the first camera query; teleport updates collision state.
    if (survival5_start_pending && OriginalSurvival5Room() &&
        kernel::R32(ctx.esp + 4) == 1) {
        // ObjectManager::Find(1): table 0x64D218, entry stride 12, pointer at +12.
        const uint32_t player = kernel::R32(0x64D230);
        if (player && kernel::R32(player + 0x68) == 1) {
            const float x = std::bit_cast<float>(kernel::R32(player + 0x80));
            const float y = std::bit_cast<float>(kernel::R32(player + 0x84));
            const float z = std::bit_cast<float>(kernel::R32(player + 0x88));
            if (x > 100 && x < 15000 && y > 3000 && y < 10000 && z > -10000 && z < 10000) {
                survival5_start_pending = false;
                const auto saved = ctx;
                ctx.esp -= 16;
                const auto position = ctx.esp;
                kernel::W32(position, std::bit_cast<uint32_t>(5800.f));
                kernel::W32(position + 4, kernel::R32(player + 0x84));
                kernel::W32(position + 8, std::bit_cast<uint32_t>(-4600.f));
                kernel::W32(position + 12, std::bit_cast<uint32_t>(1.f));
                ctx.ecx = player;
                try {
                    kernel::CallGuest(ctx, 0x15DC30, {position});
                } catch (...) {
                    ctx = saved;
                    throw;
                }
                ctx = saved;
                LOG_FIRST(4, "openshao graphics: Survival 5 start (%.1f, %.1f, %.1f) -> (5800, %.1f, -4600)", x, y, z, y);
            }
        }
    }
    __imp__sub_001B7470(ctx, base);
}

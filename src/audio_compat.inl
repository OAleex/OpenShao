// SetEG uses the supported executable's RET8 ABI; the scanner profile omits this wrapper.
#undef SYM_IDirectSoundBuffer_SetEG
#define SYM_IDirectSoundBuffer_SetEG 003FCDAE
BUFFER(SetEG, 2) {
    constexpr uint32_t invalid = 0x80070057u;
    const uint32_t descriptor = a[1];
    if (!descriptor || descriptor > 0xFFFFFFFFu - 39 ||
        !mem::IsCommitted(descriptor) || !mem::IsCommitted(descriptor + 39)) return invalid;
    uint32_t envelope[10];
    std::memcpy(envelope, HostPtr(descriptor), sizeof(envelope));
    constexpr uint32_t off[10] = {1, 0, 0, 0, 0, 0, 0, 255, 0, 0};
    if (std::memcmp(envelope, off, sizeof(off)))
        x86_trap(a.ctx, 0x003FCDAE, "OpenShao buffer envelope: only audited amplitude OFF is supported");
    // OFF preserves dry-voice volume, headroom and playback position.
    LOG_FIRST(3, "openshao audio: amplitude envelope OFF; dry voice gain preserved");
    return 0;
}

// Pitch is absolute: zero means 48 kHz. Applying the file rate here would convert twice.
#define SYM_IDirectSoundBuffer_SetPitch 003FCD76
BUFFER(SetPitch, 2) {
    const int32_t pitch = int32_t(a[1]);
    const double frequency = 48000.0 * std::exp2(double(pitch) / 4096.0);
    if (!std::isfinite(frequency) || frequency < 100 || frequency > 192000) return 0x80070057u;
    v.freq = uint32_t(frequency);
    LOG_FIRST(3, "openshao audio: pitch=%d rate=%u frequency=%u", pitch, v.rate, v.freq);
    return 0;
}

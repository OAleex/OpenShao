// Appended to dsound.cpp after audio_compat.inl. OPENSHAO_AUDIO_AUDIT=1 logs scene voices (level, volume, headroom, bins, 3D distance, gain) twice per voice.
#include <string>
extern void (*openshao_audio_audit)(bool begin);  // defined in dialogue_restore.cpp
namespace {
std::map<uint32_t, int> audit_seen;
// Follow stereo volume changes and stalled ring buffers throughout the scene.
std::map<uint32_t, std::chrono::steady_clock::time_point> audit_followed;
unsigned audit_lines = 0;
void AuditVoices(bool begin) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (begin) {
        audit_seen.clear();
        audit_followed.clear();
        audit_lines = 0;
        LOGI("audio audit: listener %.1f,%.1f,%.1f front %.2f,%.2f,%.2f", g_listener[0], g_listener[1], g_listener[2],
             g_front[0], g_front[1], g_front[2]);
    }
    const auto now = std::chrono::steady_clock::now();
    for (auto& [obj, v] : g_voices) {
        const bool heard = v.stream ? v.active && !v.packets.empty() : v.playing;
        if (v.channels == 2 && audit_seen.count(obj) && audit_lines < 400) {
            auto& last = audit_followed[obj];
            if (now - last >= std::chrono::milliseconds(500)) {
                last = now;
                ++audit_lines;
                float g[2][2];
                Levels(v, g);
                LOGI("audio audit: follow %08X playing=%d paused=%d looping=%d pos=%.0f freq=%u volume=%.3f gain L=%.3f R=%.3f",
                     obj, int(v.playing), int(v.paused), int(v.looping), v.pos, v.freq, v.volume,
                     std::max(g[0][0], g[1][0]), std::max(g[0][1], g[1][1]));
            }
        }
        if (!heard || v.paused || audit_lines >= 400) continue;
        int& seen = audit_seen[obj];
        if (seen >= 2 || (seen == 1 && v.pos < v.rate)) continue;
        ++seen;
        ++audit_lines;
        float g[2][2];
        Levels(v, g);
        std::string bins;
        for (uint32_t i = 0; i < v.nbins && i < 8; ++i)
            bins += (i ? "," : "") + std::to_string(v.bin[i]) + ":" + std::to_string(v.bin_mb[v.bin[i] & 31]);
        const float d[3] = {v.p[0] - g_listener[0], v.p[1] - g_listener[1], v.p[2] - g_listener[2]};
        const float distance = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        // Source level of a whole buffer, measured once; streams arrive in packets and are not measured.
        float peak = -1, rms = -1;
        const uint32_t frames = v.stream || !v.data ? 0 : FramesOf(v, v.bytes);
        if (seen == 1 && frames && frames <= 4'000'000) {
            double sum = 0;
            peak = 0;
            const uint32_t channels = std::max<uint32_t>(v.channels, 1);
            for (uint32_t f = 0; f < frames; ++f)
                for (uint32_t c = 0; c < channels; ++c) {
                    const float s = std::abs(Sample(v, v.data, f, int(c)));
                    peak = std::max(peak, s);
                    sum += double(s) * s;
                }
            for (auto& block : v.block) block = -1;  // the mixer decodes its own blocks again
            rms = float(std::sqrt(sum / (double(frames) * channels)));
        }
        LOGI("audio audit: voice %08X %s seen=%d rate=%u ch=%u tag=%X frames=%u pos=%.0f volume=%.3f headroom=%.3f "
             "bins=[%s] 3d=%d mode=%u distance=%.1f min=%.1f max=%.1f curve=%zu gain L=%.3f R=%.3f peak=%.3f rms=%.3f",
             obj, v.stream ? "stream" : "buffer", seen, v.rate, v.channels, v.tag, frames, v.pos, v.volume, v.headroom,
             bins.c_str(), int(v.is3d), v.mode, distance, v.min_dist, v.max_dist, v.curve.size(),
             std::max(g[0][0], g[1][0]), std::max(g[0][1], g[1][1]), peak, rms);
    }
}
struct AuditHook {
    AuditHook() {
        const char* enabled = std::getenv("OPENSHAO_AUDIO_AUDIT");
        if (enabled && *enabled && std::strcmp(enabled, "0") != 0) openshao_audio_audit = AuditVoices;
    }
} audit_hook;
}

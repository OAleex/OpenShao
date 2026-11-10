// Included inside the adapted DirectSound runtime's anonymous namespace.
using openshao::audio::EffectChain;
std::unique_ptr<EffectChain> g_effect_chain;
std::recursive_mutex g_effect_chain_lock;
bool g_audio_effect_fault_logged = false;
uint32_t g_room_effect_index = 0xFFFFFFFFu;
struct AudioSegment { uint32_t guest = 0, size = 0; };
std::vector<std::array<AudioSegment, 4>> g_effect_image_segments;
uint32_t g_effect_image_descriptor = 0;
void ResetMixOutput();

bool IsGuestAudioRange(uint32_t address, uint32_t bytes) {
    if (!bytes) return true;
    if (!address || bytes > 16*1024*1024 || address > 0xFFFFFFFFu-(bytes-1)) return false;
    const uint32_t last = address+bytes-1;
    for (uint64_t page = address & ~0xFFFu; page <= last; page += 0x1000)
        if (!mem::IsCommitted(uint32_t(page))) return false;
    return true;
}

bool AudioDiagnosticsEnabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("OPENSHAO_AUDIO_DIAGNOSTICS");
        return value && *value && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

uint32_t LoadEffectImage(const std::vector<uint8_t>& image, const char* name, uint32_t location) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    std::lock_guard<std::recursive_mutex> effects_lock(g_effect_chain_lock);
    auto replacement = std::make_unique<EffectChain>();
    if (!replacement->LoadImage(image)) {
        LOGE("audio effects: cannot load %s: %s", name, replacement->LastError().c_str()); return 0;
    }
    const auto& descriptions = replacement->Descriptions();
    const uint32_t descriptor = mem::AllocSystem(8 + uint32_t(descriptions.size())*32);
    if (!descriptor) return 0;
    W32(descriptor, uint32_t(descriptions.size()));
    uint32_t code_words, state_words, scratch_bytes;
    std::memcpy(&code_words, image.data()+0x804, 4);
    std::memcpy(&state_words, image.data()+0x80C, 4);
    std::memcpy(&scratch_bytes, image.data()+0x81C+size_t(code_words+state_words)*4, 4);
    W32(descriptor+4, scratch_bytes);
    std::vector<std::array<AudioSegment, 4>> segments(descriptions.size());
    auto discard = [&] {
        for (const auto& effect : segments)
            for (const auto& segment : effect) if (segment.guest) mem::FreeSystem(segment.guest);
        mem::FreeSystem(descriptor);
        return 0u;
    };
    for (uint32_t i = 0; i < descriptions.size(); ++i) {
        const auto& effect = descriptions[i];
        const openshao::audio::EffectSegment parts[] = {effect.code, effect.state, effect.y, effect.scratch};
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t size = parts[k].size;
            const uint32_t guest = mem::AllocSystem(std::max(size, 0x40u));
            if (!guest) return discard();
            segments[i][k] = {guest, size};
            std::memset(HostPtr(guest), 0, std::max(size, 0x40u));
            if (size && !replacement->ReadImageSegment(i, k, 0, {static_cast<uint8_t*>(HostPtr(guest)), size})) return discard();
            W32(descriptor+8+i*32+k*8, guest);
            W32(descriptor+12+i*32+k*8, size);
        }
    }
    // Release old guest descriptors only after the new image has loaded successfully.
    for (const auto& effect : g_effect_image_segments)
        for (const auto& segment : effect) if (segment.guest) mem::FreeSystem(segment.guest);
    if (g_effect_image_descriptor) mem::FreeSystem(g_effect_image_descriptor);
    g_effect_image_descriptor = descriptor;
    g_effect_image_segments = std::move(segments);
    g_effect_chain = std::move(replacement);
    g_audio_effect_fault_logged = false;
    g_room_effect_index = location ? R32(location) : 0xFFFFFFFFu;
    ResetMixOutput();
    LOGI("audio effects: loaded %s, %zu effects, reverb=%d, crosstalk=%d; C++ filter chain active",
         name, descriptions.size(), int32_t(g_room_effect_index), location ? int32_t(R32(location+4)) : -1);
    return descriptor;
}

void RenderEffectBlock(const openshao::audio::mix::Frame& buses, openshao::audio::SpeakerBlock& speakers) {
    std::lock_guard<std::recursive_mutex> effects_lock(g_effect_chain_lock);
    bool processed = false;
    const auto before = std::chrono::steady_clock::now();
    if (g_effect_chain && g_effect_chain->IsReady()) {
        processed = g_effect_chain->ProcessSpeakers(reinterpret_cast<const float(*)[32]>(buses.data()), speakers);
        if (!processed && !g_audio_effect_fault_logged) {
            g_audio_effect_fault_logged = true;
            LOGE("audio effects: processing stopped: %s", g_effect_chain->LastError().c_str());
        }
    }
    if (!processed) {
        // Dry startup/failure path. Effects are never reported as active in this path.
        for (uint32_t c=0;c<6;++c) std::copy_n(buses[c].begin(),32,speakers[c].begin());
        for (uint32_t i = 0; i < 32; ++i) {
            speakers[0][i]+=buses[6][i]; speakers[1][i]+=buses[7][i];
            speakers[4][i]+=buses[8][i]; speakers[5][i]+=buses[9][i];
        }
    }
    if (AudioDiagnosticsEnabled()) {
        static uint64_t frames = 0, elapsed_us = 0, max_us = 0;
        static uint32_t active_bins = 0;
        static float input_peak = 0, output_peak = 0;
        const uint64_t us = uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now()-before).count());
        elapsed_us += us; max_us = std::max(max_us, us);
        for (uint32_t b = 0; b < 32; ++b)
            for (float sample : buses[b]) {
                input_peak = std::max(input_peak, std::abs(sample));
                if (sample != 0) active_bins |= 1u << b;
            }
        for (const auto& channel:speakers) for (float sample:channel) output_peak=std::max(output_peak,std::abs(sample));
        if (++frames % 1500 == 0) {
            const auto metrics = g_effect_chain ? g_effect_chain->Statistics() : openshao::audio::EffectStatistics{};
            LOGI("audio effects: blocks=%llu failures=%llu active=%d bins=%08X input_peak=%.4f output_peak=%.4f avg_us=%llu max_us=%llu",
                 static_cast<unsigned long long>(metrics.blocks), static_cast<unsigned long long>(metrics.failures), int(processed), active_bins, input_peak, output_peak,
                 static_cast<unsigned long long>(elapsed_us/1500), static_cast<unsigned long long>(max_us));
            elapsed_us = max_us = 0; active_bins = 0; input_peak = output_peak = 0;
        }
    }
}

void AudioOutputMetrics(const float* pcm,uint32_t samples,uint32_t channels,openshao::audio::output::Mode mode) {
    if (!AudioDiagnosticsEnabled()) return;
    static uint32_t blocks=0;
    static std::array<float,6> peaks{};
    for (uint32_t i=0;i<samples;++i) for (uint32_t c=0;c<channels;++c) peaks[c]=std::max(peaks[c],std::abs(pcm[i*channels+c]));
    if (++blocks%1500==0) {
        LOGI("audio output: mode=%s channels=%u peaks FL=%.4f FR=%.4f C=%.4f LFE=%.4f BL=%.4f BR=%.4f",
             openshao::audio::output::Name(mode),channels,peaks[0],peaks[1],peaks[2],peaks[3],peaks[4],peaks[5]);
        peaks={};
    }
}

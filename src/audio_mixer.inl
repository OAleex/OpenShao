// Included inside the adapted DirectSound runtime's anonymous namespace.
namespace mix = openshao::audio::mix;
mix::Headroom g_mix_headroom;
std::array<float, 192> g_mix_output{};
openshao::audio::SpeakerPeakLimiter g_mix_peak_limiter;
uint32_t g_mix_channels=2;
auto g_mix_mode=openshao::audio::output::Mode::Stereo;
uint32_t g_mix_output_at = mix::FrameSamples;
std::atomic<bool> g_mix_reset{false};
void ResetMixOutput() { g_mix_reset.store(true, std::memory_order_release); }

mix::Routing VoiceRouting(const Voice& v) {
    const float voice_gain = v.volume * v.headroom;
    auto routing = mix::BuildRouting(v.channels, v.nbins, v.bin, v.bin_mb, 1.f);
    if (!v.is3d || v.mode == 2) {
        for (uint32_t i = 0; i < routing.count; ++i) routing.slots[i].gain *= voice_gain;
        return routing;
    }
    const bool relative = v.mode == 1;
    float d[3] = {v.p[0], v.p[1], v.p[2]};
    for (int i = 0; i < 3 && !relative; ++i) d[i] -= g_listener[i];
    const float distance = std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
    float attenuation = 1.f;
    if (distance > v.min_dist && v.curve.empty()) {
        attenuation = v.min_dist / std::min(distance, v.max_dist);
    } else if (distance > v.min_dist && v.max_dist > v.min_dist) {
        const float at = (std::min(distance, v.max_dist) - v.min_dist) /
                         (v.max_dist - v.min_dist) * v.curve.size();
        const size_t i = std::min(size_t(at), v.curve.size()-1);
        const float from = i ? v.curve[i-1] : 1.f;
        attenuation = from + (v.curve[i] - from) * (at-i);
    }
    const float right[3] = {g_top[1]*g_front[2] - g_top[2]*g_front[1],
                            g_top[2]*g_front[0] - g_top[0]*g_front[2],
                            g_top[0]*g_front[1] - g_top[1]*g_front[0]};
    const float side = relative ? d[0] : d[0]*right[0] + d[1]*right[1] + d[2]*right[2];
    const float pan = distance > 1e-3f ? side/distance : 0.f;
    const float forward=relative ? d[2] : d[0]*g_front[0]+d[1]*g_front[1]+d[2]*g_front[2];
    const bool surround=openshao::audio::output::Current().EffectiveMode()==openshao::audio::output::Mode::Surround;
    std::array<float,5> speakers{};
    if (surround) {
        bool centre=false;
        for (uint32_t i=0;i<routing.count;++i) centre|=routing.slots[i].bin==2;
        speakers=openshao::audio::SurroundPan(side,forward,centre);
    }
    if (surround && !v.nbins) {
        constexpr uint8_t spatial_bins[]={6,7,8,9};
        routing=mix::BuildRouting(v.channels,4,spatial_bins,v.bin_mb,1.f);
    }
    float dry = 1, wet = 1;
    AudioI3DL2Gains(v, distance, dry, wet);
    for (uint32_t i = 0; i < routing.count; ++i) {
        auto& slot = routing.slots[i];
        float spatial = attenuation;
        if (slot.bin == 10) spatial *= wet;
        else if (slot.bin == 2 || (slot.bin >= 6 && slot.bin <= 9)) spatial *= dry;
        if (surround && slot.bin==2) spatial*=speakers[4];
        if (surround && ((slot.bin>=6 && slot.bin<=9) || slot.bin==0 || slot.bin==1 || slot.bin==4 || slot.bin==5)) {
            const unsigned speaker=slot.bin>=6 ? slot.bin-6 : slot.bin<2 ? slot.bin : slot.bin-2;
            spatial*=speakers[speaker];
            constexpr uint8_t direct_bins[]={0,1,4,5};
            slot.bin=direct_bins[speaker];
        } else switch (slot.bin) {
        case 0: case 4: case 6: case 8: spatial *= std::min(1.f, 1.f-pan); break;
        case 1: case 5: case 7: case 9: spatial *= std::min(1.f, 1.f+pan); break;
        }
        slot.gain *= std::min(spatial, 1.f)*voice_gain;
    }
    return routing;
}

// Diagnostic estimate before effects. DSP sends cannot be expressed as two static gains.
void Levels(Voice& v, float g[2][2]) {
    g[0][0] = g[0][1] = g[1][0] = g[1][1] = 0;
    const auto routing = VoiceRouting(v);
    for (uint32_t i = 0; i < routing.count; ++i) {
        const auto& slot = routing.slots[i];
        if (slot.channel > 1 || slot.bin > 9 || slot.bin == 3) continue;
        const float gain = slot.gain * (v.is3d && i < 4 ? 1.f : g_mix_headroom.PcmGain(slot.bin));
        if (slot.bin == 2) { g[slot.channel][0] += gain*.70710678f; g[slot.channel][1] += gain*.70710678f; }
        else g[slot.channel][slot.bin & 1] += gain * ((slot.bin==4 || slot.bin==5 || slot.bin>=8) ? .70710678f : 1.f);
    }
}

void Deactivate(Voice& v) {
    v.sync = false;
    if (v.active) v.active = v.paused = v.ending = false;
}

void FillMixFrame(mix::Frame& buses) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    for (auto& [obj, v] : g_voices) {
        if (v.paused || v.sync || (!v.stream && !v.playing)) continue;
        const auto routing = VoiceRouting(v);
        auto* environment = AudioI3DL2State(v);
        const uint32_t channels = std::clamp<uint32_t>(v.channels, 1, 8);
        const uint32_t base = v.data + v.play_start;
        const double step = double(v.freq ? v.freq : v.rate) / audio::kRate;
        for (uint32_t i = 0; i < mix::FrameSamples; ++i) {
            const uint32_t start = v.looping ? FramesOf(v, v.loop_start) : 0;
            const uint32_t end = FramesOf(v, v.looping ? v.loop_start + v.loop_len : v.play_len);
            uint32_t f = uint32_t(v.pos), data = base, next_data = base, next = 0;
            if (v.stream) {
                while (!v.packets.empty() && f >= FramesOf(v, v.packets.front().size)) {
                    Packet packet = v.packets.front(); v.packets.pop_front();
                    packet.used = packet.size; Complete(v, packet, 0);
                    v.pos -= FramesOf(v, packet.size); f = uint32_t(v.pos);
                }
                if (v.packets.empty()) {
                    v.pos = 0;
                    if (v.ending) Deactivate(v);
                    break;
                }
                data = next_data = v.packets.front().data;
                if (f+1 < FramesOf(v, v.packets.front().size)) next = f+1;
                else if (v.packets.size() > 1) { next_data = v.packets[1].data; next = 0; }
                else next = f;
            } else {
                if (f >= end) {
                    if (!v.looping || end <= start) { v.playing = false; v.pos = 0; break; }
                    v.pos = start + std::fmod(v.pos-start, double(end-start));
                    f = uint32_t(v.pos);
                }
                if (!v.data) break;
                next = f+1 < end ? f+1 : v.looping ? start : f;
            }
            float samples[8]{};
            const float t = float(v.pos-f);
            for (uint32_t c = 0; c < channels; ++c) {
                const float a = Sample(v, data, f, int(c));
                const float b = Sample(v, next_data, next, int(c));
                samples[c] = a + (b-a)*t;
            }
            float dry[8]{}, wet[8]{};
            AudioI3DL2Samples(environment, samples, dry, wet, channels);
            mix::Accumulate(buses, i, routing, dry, g_mix_headroom, v.is3d, wet);
            v.pos += step;
        }
    }
}

void RenderMixFrame() {
    mix::Frame buses{};
    FillMixFrame(buses);
    // Effects use their own lock and keep running in silence so tails decay.
    openshao::audio::SpeakerBlock speakers{};
    RenderEffectBlock(buses,speakers);
    openshao::audio::FormatSpeakers(speakers,g_mix_output.data(),g_mix_channels,g_mix_mode);
    g_mix_peak_limiter.Process(g_mix_output.data(),mix::FrameSamples,g_mix_channels);
    AudioOutputMetrics(g_mix_output.data(),mix::FrameSamples,g_mix_channels,g_mix_mode);
    g_mix_output_at = 0;
}

void Render(float* out, uint32_t frames) {
    const auto state=openshao::audio::output::Current();
    if (g_mix_reset.exchange(false, std::memory_order_acquire) || g_mix_channels!=state.channels || g_mix_mode!=state.EffectiveMode()) {
        g_mix_output_at = mix::FrameSamples;
        g_mix_peak_limiter.Reset();
        g_mix_channels=state.channels;
        g_mix_mode=state.EffectiveMode();
    }
    for (uint32_t i = 0; i < frames; ++i) {
        if (g_mix_output_at == mix::FrameSamples) RenderMixFrame();
        for (uint32_t channel = 0; channel < g_mix_channels; ++channel) {
            const float sample = g_mix_output[g_mix_channels*g_mix_output_at + channel];
            out[g_mix_channels*i + channel] = std::isfinite(sample) ? std::clamp(sample, -1.f, 1.f) : 0.f;
        }
        ++g_mix_output_at;
    }
}

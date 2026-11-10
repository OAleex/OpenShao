// Included inside the adapted DirectSound runtime's anonymous namespace.
namespace i3dl2 = openshao::audio::i3dl2;
struct AudioSourceEnvironment {
    i3dl2::Deferred<9> parameters;
    bool dirty = true;
    float distance = -1, dry = 1, wet = 1, dry_pole = 0, wet_pole = 0;
    uint64_t listener_version = 0;
    std::array<float, 8> dry_history{}, wet_history{};
};
std::map<uint32_t, AudioSourceEnvironment> g_audio_sources;
i3dl2::Deferred<12> g_audio_listener;
uint64_t g_audio_listener_version = 1;

const i3dl2::Words<12>& AudioListenerEnvironment() {
    if (!g_audio_listener.Configured()) {
        i3dl2::Words<12> preset{};
        // DirectSoundI3DL2ListenerPreset_Default in the supported original executable.
        std::memcpy(preset.data(), HostPtr(0x4253E8), sizeof(preset));
        g_audio_listener.Set(preset, false);
    }
    return g_audio_listener.Active();
}

bool AudioApplyListener(X86Context& ctx) {
    std::lock_guard<std::recursive_mutex> effects_lock(g_effect_chain_lock);
    if (!g_effect_chain || g_room_effect_index >= g_effect_chain->Descriptions().size()) return false;
    static uint32_t workspace = 0;
    if (!workspace) workspace = mem::AllocSystem(0x300);
    if (!workspace) return false;
    const uint32_t output = workspace+0x80;
    std::memset(HostPtr(workspace), 0, 0x300);
    auto* state = static_cast<uint8_t*>(HostPtr(output));
    if (!g_effect_chain->ReadParameters(g_room_effect_index, 0, {state, 0x118})) return false;
    W32(workspace, output);
    W32(workspace+4, 0xFFF);
    std::memcpy(HostPtr(workspace+8), AudioListenerEnvironment().data(), 48);
    // Reuse the original pure coefficient calculator. It never touches hardware or SDK objects.
    const X86Context saved = ctx;
    ctx.ecx = workspace;
    CallGuest(ctx, 0x408EC9, {workspace+4, output});
    ctx = saved;
    W32(output+0x10, R32(output+0x10) | 4);
    if (!g_effect_chain->SetParameters(g_room_effect_index, 0x10, {state+0x10, 4}, true) ||
        !g_effect_chain->SetParameters(g_room_effect_index, 0x118, {state+0x118, 0x108}, true)) return false;
    g_effect_chain->CommitParameters();
    if (AudioDiagnosticsEnabled()) LOGI("audio effects: I3DL2 listener coefficients applied to effect %u", g_room_effect_index);
    return true;
}

uint32_t AudioSetI3DL2Listener(const Args& a) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (a[2] > 1 || !IsGuestAudioRange(a[1], 48)) return 0x80070057u;
    {
        std::lock_guard<std::recursive_mutex> effects_lock(g_effect_chain_lock);
        if (!g_effect_chain || g_room_effect_index >= g_effect_chain->Descriptions().size()) return 0x88780032u;
    }
    i3dl2::Words<12> value;
    std::memcpy(value.data(), HostPtr(a[1]), sizeof(value));
    if (!i3dl2::ValidListener(value)) return 0x80070057u;
    AudioListenerEnvironment();
    g_audio_listener.Set(value, a[2] != 0);
    if (!a[2]) {
        ++g_audio_listener_version;
        if (!AudioApplyListener(a.ctx)) return 0x88780032u;
    }
    return 0;
}

uint32_t AudioSetI3DL2Source(Voice& v, const Args& a) {
    if (a[2] > 1 || !IsGuestAudioRange(a[1], 36)) return 0x80070057u;
    if (!v.is3d) return 0x88780032u;
    i3dl2::Words<9> value;
    std::memcpy(value.data(), HostPtr(a[1]), sizeof(value));
    if (!i3dl2::ValidSource(value)) return 0x80070057u;
    auto& environment = g_audio_sources[v.obj];
    environment.parameters.Set(value, a[2] != 0);
    environment.dirty = true;
    if (AudioDiagnosticsEnabled()) LOG_FIRST(32, "audio effects: I3DL2 source voice=%08X direct=%d room=%d deferred=%u",
                                      v.obj, int32_t(value[0]), int32_t(value[2]), a[2]);
    return 0;
}

uint32_t AudioCommitI3DL2(const Args& a) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    for (auto& [obj, environment] : g_audio_sources)
        if (environment.parameters.Commit()) environment.dirty = true;
    if (g_audio_listener.Commit()) {
        ++g_audio_listener_version;
        if (!AudioApplyListener(a.ctx)) return 0x88780032u;
    }
    return 0;
}
void AudioForgetI3DL2(uint32_t object) { g_audio_sources.erase(object); }

void AudioI3DL2Gains(const Voice& v, float distance, float& dry, float& wet) {
    dry = wet = 1;
    auto found = g_audio_sources.find(v.obj);
    if (found == g_audio_sources.end() || !found->second.parameters.Configured() || v.mode == 2) return;
    auto& environment = found->second;
    if (environment.dirty || environment.distance != distance || environment.listener_version != g_audio_listener_version) {
        const auto& listener = AudioListenerEnvironment();
        const auto levels = i3dl2::Calculate(i3dl2::Decode(environment.parameters.Active()), distance,
                                            i3dl2::Float(listener[2]), i3dl2::Float(listener[11]));
        environment.dry = i3dl2::Gain(levels.direct_mb);
        environment.wet = i3dl2::Gain(levels.room_mb);
        environment.dry_pole = i3dl2::Pole(levels.direct_hf_mb, i3dl2::Float(listener[11]));
        environment.wet_pole = i3dl2::Pole(levels.room_hf_mb, i3dl2::Float(listener[11]));
        environment.distance = distance;
        environment.listener_version = g_audio_listener_version;
        environment.dirty = false;
    }
    dry = environment.dry; wet = environment.wet;
}

AudioSourceEnvironment* AudioI3DL2State(const Voice& v) {
    auto found = g_audio_sources.find(v.obj);
    return found == g_audio_sources.end() || !found->second.parameters.Configured() || v.mode == 2 ? nullptr : &found->second;
}
void AudioI3DL2Samples(AudioSourceEnvironment* environment, const float* input, float* dry, float* wet, uint32_t channels) {
    if (!environment) {
        std::copy_n(input, channels, dry); std::copy_n(input, channels, wet); return;
    }
    for (uint32_t c = 0; c < channels; ++c) {
        dry[c] = i3dl2::Filter(input[c], environment->dry_pole, environment->dry_history[c]);
        wet[c] = i3dl2::Filter(input[c], environment->wet_pole, environment->wet_history[c]);
    }
}

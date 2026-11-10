"""Connect DirectSound voices, effect parameters and the C++ audio filter chain."""


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"SDK audio source changed at {old[:80]!r}")
    return text.replace(old, new, 1)


def replace_function(text, signature, replacement):
    begin = text.index(signature + ' {')
    opening = begin + len(signature) + 1
    end = text.index('\n}', opening) + 2
    if text[opening:opening + len('{ return 0; }')] == '{ return 0; }':
        end = opening + len('{ return 0; }')
    return text[:begin] + replacement + text[end:]


def adapt(text):
    text = ('#include "audio_mix_bins.h"\n#include "audio_effect_chain.h"\n'
            '#include "audio_i3dl2.h"\n#include "audio_spatial.h"\n'
            '#include <cstdlib>\n#include <cstring>\n' + text)
    begin = text.index('// the console\'s stereo downmix of the speaker bins:')
    end = text.index('int64_t NowMs()', begin)
    text = text[:begin] + ('#include "audio_effects_runtime.inl"\n'
                          '#include "audio_i3dl2_runtime.inl"\n'
                          '#include "audio_mixer.inl"\n\n') + text[end:]
    begin = text.index('// The descriptor keeps the image\'s effect list')
    end = text.index('HLE_STDCALL(XAudioDownloadEffectsImage, 4)', begin)
    text = text[:begin] + text[end:]
    text = replace_once(text, 'EffectsDesc(img, name.c_str(), a[1])', 'LoadEffectImage(img, name.c_str(), a[1])')
    text = replace_once(text, 'EffectsDesc(img, "from memory", a[3])', 'LoadEffectImage(img, "from memory", a[3])')
    for marker in ('if (a[3]) W32(a[3], desc);', 'if (a[4]) W32(a[4], desc);'):
        text = replace_once(text, marker, 'if (!desc) return 0x80004005u;\n    ' + marker)
    text = replace_function(text, 'HLE_STDCALL(IDirectSound_SetMixBinHeadroom, 3)', '''HLE_STDCALL(IDirectSound_SetMixBinHeadroom, 3) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    if (!g_mix_headroom.Set(a[1], a[2])) return 0x80070057u;
    if (AudioDiagnosticsEnabled()) LOGI("audio effects: bin %u headroom shift=%u", a[1], a[2]);
    return 0;
}''')
    text = replace_function(text, 'HLE_STDCALL(IDirectSound_SetEffectData, 6)', '''HLE_STDCALL(IDirectSound_SetEffectData, 6) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    std::lock_guard<std::recursive_mutex> effects_lock(g_effect_chain_lock);
    if (a[5] > 1 || !IsGuestAudioRange(a[3], a[4]) || !g_effect_chain ||
        !g_effect_chain->SetParameters(a[1], a[2], {static_cast<const uint8_t*>(HostPtr(a[3])), a[4]}, a[5] != 0))
        return 0x88780032u;
    if (a[1] < g_effect_image_segments.size() && a[4])
        std::memcpy(HostPtr(g_effect_image_segments[a[1]][1].guest+a[2]), HostPtr(a[3]), a[4]);
    if (AudioDiagnosticsEnabled()) LOG_FIRST(48, "audio effects: effect %u data offset=%u bytes=%u deferred=%u", a[1], a[2], a[4], a[5]);
    return 0;
}''')
    begin = text.index('// (this, effect, offset, data, size): reads the effect\'s state')
    end = text.index('// (this, DSOUTPUTLEVELS out', begin)
    text = text[:begin] + '''HLE_STDCALL(IDirectSound_GetEffectData, 5) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    std::lock_guard<std::recursive_mutex> effects_lock(g_effect_chain_lock);
    if (!IsGuestAudioRange(a[3], a[4]) || !g_effect_chain ||
        !g_effect_chain->ReadParameters(a[1], a[2], {static_cast<uint8_t*>(HostPtr(a[3])), a[4]})) return 0x88780032u;
    return 0;
}
HLE_STDCALL(IDirectSound_CommitEffectData, 1) {
    std::lock_guard<std::recursive_mutex> lock(g_lock);
    std::lock_guard<std::recursive_mutex> effects_lock(g_effect_chain_lock);
    if (g_effect_chain) g_effect_chain->CommitParameters();
    return g_effect_chain && !g_effect_chain->IsReady() ? 0x80004005u : 0;
}
''' + text[end:]
    text = replace_function(text, 'HLE_STDCALL(IDirectSound_SetI3DL2Listener, 3)',
                        'HLE_STDCALL(IDirectSound_SetI3DL2Listener, 3) { return AudioSetI3DL2Listener(a); }')
    text = replace_once(text, 'HLE_STDCALL(IDirectSound_CommitDeferredSettings, 1) { return 0; }',
                        'HLE_STDCALL(IDirectSound_CommitDeferredSettings, 1) { return AudioCommitI3DL2(a); }')
    if text.count('    g_voices.erase(obj);') != 2:
        raise ValueError('SDK audio release paths changed')
    text = text.replace('    g_voices.erase(obj);', '    AudioForgetI3DL2(obj);\n    g_voices.erase(obj);')
    for prefix in ('BUFFER', 'STREAM'):
        text = replace_once(text, f'{prefix}(SetI3DL2Source, 3) {{ return 0; }}',
                            f'{prefix}(SetI3DL2Source, 3) {{ return AudioSetI3DL2Source(v, a); }}')
    # The supported title uses the audited envelope wrapper in audio_compat.inl.
    text = replace_once(text, 'BUFFER(SetEG, 2) { return 0; }', '')
    return text

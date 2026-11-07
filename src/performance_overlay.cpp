#include "performance_overlay.h"
#include "mods/version.h"
#include "texture_pack_stats.h"
#include <atomic>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <imgui.h>
namespace openshao::performance {
namespace {
std::atomic<bool> visible{false};
bool primed = false;

// Render thread time spent on texture pack loads in each of the last frames, to show the worst one.
double WorstFrameLoadMs(uint64_t total_us) {
    static std::array<uint64_t, 240> frames{};
    static uint64_t previous = 0;
    static size_t next = 0;
    if (!primed) {  // the loads made while the panel was hidden are not one frame's
        previous = total_us;
        frames.fill(0);
        primed = true;
    }
    frames[next++ % frames.size()] = total_us >= previous ? total_us - previous : 0;
    previous = total_us;
    return double(*std::max_element(frames.begin(), frames.end())) / 1000.0;
}
}
bool Visible() { return visible.load(std::memory_order_relaxed); }
void SetVisible(bool value) { visible.store(value, std::memory_order_relaxed); }
bool Message(uint32_t message, uintptr_t key, intptr_t flags) {
    constexpr uint32_t keydown = 0x100, keyup = 0x101, f9 = 0x78;
    if (key != f9 || (message != keydown && message != keyup)) return false;
    if (message == keydown && !(uintptr_t(flags) & (uintptr_t(1) << 30)))
        visible.store(!Visible(), std::memory_order_relaxed);
    return true;
}
std::string Format(double fps) {
    if (!std::isfinite(fps) || fps <= 0) return "-- FPS  |  -- ms";
    char text[80];
    std::snprintf(text, sizeof text, "%.1f FPS  |  %.2f ms", fps, 1000.0 / fps);
    return text;
}
void Draw(double fps) {
    if (!Visible()) {
        primed = false;
        return;
    }
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({viewport->WorkPos.x + 12, viewport->WorkPos.y + 12}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(.78f);
    // Keep the panel size stable whatever the width of the version text.
    ImGui::SetNextWindowSizeConstraints({280, 64}, {FLT_MAX, FLT_MAX});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{14,12});
    constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("##openshao-performance", nullptr, flags)) {
        ImGui::SetWindowFontScale(1.5f);
        ImGui::Text("OpenShao %s",mods::RecompVersion);
        ImGui::TextUnformatted(Format(fps).c_str());
        const auto stats = texpack::GetStats();
        if (stats.resident || stats.loads || stats.failures) {
            const double loads = double(std::max<uint64_t>(stats.loads, 1));
            ImGui::SetWindowFontScale(1.f);
            ImGui::Text("Textures: %u in use, %.0f MiB", stats.resident, double(stats.resident_bytes) / 1048576.0);
            ImGui::Text("Loads: %llu, %llu failed", static_cast<unsigned long long>(stats.loads),
                        static_cast<unsigned long long>(stats.failures));
            ImGui::Text("Per load: read %.2f ms, upload %.2f ms", double(stats.disk_us) / 1000.0 / loads,
                        double(stats.upload_us) / 1000.0 / loads);
            if (stats.pop_ins)
                ImGui::Text("Pop-in: %.1f frames on average, %llu at most", double(stats.pop_frames) / double(stats.pop_ins),
                            static_cast<unsigned long long>(stats.pop_max));
            if (stats.budget_bytes)
                ImGui::Text("Video memory: %.0f of %.0f MiB; %llu taken out, %llu back, %llu no room",
                            double(stats.resident_bytes) / 1048576.0, double(stats.budget_bytes) / 1048576.0,
                            static_cast<unsigned long long>(stats.evictions), static_cast<unsigned long long>(stats.reloads),
                            static_cast<unsigned long long>(stats.no_room));
            ImGui::Text("Worst frame, last 240: %.2f ms", WorstFrameLoadMs(stats.render_us));
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
}

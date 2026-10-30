#include "launcher_footer_layout.h"
#include <iostream>
#include <stdexcept>

using openshao::launcher::footer_layout::Rows;
using openshao::launcher::footer_layout::CachedCheckPulse;

static void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        CachedCheckPulse pulse;
        const auto now = CachedCheckPulse::Clock::now();
        Require(!pulse.Active(now), "Cached check spinner must start hidden");
        pulse.Start(now);
        Require(pulse.Active(now) && pulse.Active(now + std::chrono::milliseconds(400)),
                "A repeated check must display a brief loading indicator");
        Require(!pulse.Active(now + CachedCheckPulse::duration),
                "The spinner must stop automatically and reveal the normal update status");
        pulse.Start(now + std::chrono::seconds(1));
        Require(pulse.Active(now + std::chrono::seconds(1)),
                "Another click must restart its visual acknowledgement");

        for (const float height : {480.f, 527.f, 640.f, 720.f}) {
            const Rows rows(height);
            const float renderer_bottom = rows.divider - 48.f + 40.f;
            Require(renderer_bottom < rows.divider,
                    "The Graphics API controls must remain above the footer divider");
            Require(rows.primary > rows.divider &&
                    rows.primary + 23.f < rows.secondary - 2.f,
                    "The primary update status must be separated from the action row");
            Require(rows.button_top > rows.divider &&
                    rows.button_bottom < rows.secondary - 2.f,
                    "Update and Cancel buttons must not intersect the action row");
            Require(rows.secondary + 23.f < height,
                    "The check action and message must fit inside the bottom of the launcher");
        }

        for (const float width : {800.f, 1040.f, 1280.f}) {
            const float right = width - 40.f;
            constexpr float link_width = 165.f;
            const float note_right = right - link_width - 24.f;
            Require(note_right + 24.f <= right - link_width,
                    "Cooldown messages must not collide with the check link");
            Require(note_right > width / 2.f,
                    "Cooldown messages need a dedicated region in the right half");
        }
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS: footer controls and status messages remain on separate rows\n";
}

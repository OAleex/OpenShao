// Native save-buffer swaps and list hooks for the supported retail executable.
#include "save_pages.h"
#include "file_ops.h"
#include "launcher.h"
#include "menu_text.h"
#include "mods/languages.h"
#include "restart.h"
#include "runtime_paths.h"
#include "survival_unlock.h"
#include "cpu/x86_context.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"
#include "core/input.h"
#include "core/log.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <unordered_map>

namespace openshao::save_pages {
namespace {
constexpr uint32_t SaveManager = 0x59E600, SavePath = 0x49F6C8;
constexpr uint32_t Checksum = 0x1059A0, ChecksumBytes = 0x2F228;
constexpr uint32_t MenuHidden = 0x5967BC, KoopLayout = 0x5CE864;
constexpr uint32_t DrawCentered = 0x152CE0, DrawLeft = 0x152AE0, TextLookup = 0x16992;
constexpr uint32_t WriteFile = 0x1272A0, SaveBytes = 0x2F22C;
constexpr uint32_t MenuList = 0x5967C0, MenuSelection = MenuList + 0x38, ScreenStack = 0x6C9308;
constexpr uint32_t MoveSound = 0x6CA3C0, AcceptSound = 0x6CA3B8, BackSound = 0x6CA3BC;  // frontend sounds
constexpr uint32_t YesText = 0x5AA, NoText = 0x5AB;
constexpr uint32_t HintX = 0x1C2;  // the footer's Accept glyph column
// Menu sprite cache 0x4EF3C8 caches icons by id; the game clears it after saving (0x152A9 with 0).
constexpr uint32_t MenuManager = 0x4EF3C8, ClearMenuSprites = 0x152A9;

std::filesystem::path PageFile(uint32_t page) {
    char name[40];
    std::snprintf(name, sizeof(name), "openshao-save-page-%02u.bin", page);
    // Beside the active save drive, so portable folders keep their pages.
    const auto& drives = restart::ActiveSaveDirectory();
    return (drives.empty() ? paths::SaveRoot() : drives) / name;
}
std::optional<std::vector<uint8_t>> LoadPage(uint32_t page) {
    const auto path = PageFile(page);
    std::error_code error;
    if (!std::filesystem::exists(path, error)) return std::nullopt;
    std::ifstream file(path, std::ios::binary);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::vector<uint8_t> region;
    if (DecodePage(bytes, page, region)) return region;
    // Keep an unreadable page aside instead of overwriting it with the next save.
    const auto aside = files::UniqueSibling(path, ".damaged-");
    std::filesystem::rename(path, aside, error);
    LOGW("openshao save pages: page %u is damaged, moved to %s", page, files::Utf8(aside.filename()).c_str());
    return std::nullopt;
}
bool StorePage(uint32_t page, const std::vector<uint8_t>& region) {
    try {
        files::AtomicWrite(PageFile(page), EncodePage(page, region));
        return true;
    } catch (const std::exception& error) {
        LOGW("openshao save pages: cannot write page %u: %s", page, error.what());
        return false;
    }
}

Book book(LoadPage);
Triggers triggers;
Edge delete_button, yes_button, no_button;
uint32_t deleting = 0;  // slot waiting for the delete answer, 0 when no question is open
bool holding = false;   // A and B stay hidden from the game until released after the answer
bool question_in_menu = false;  // the question belongs to the main-menu list (else in-game)
uint32_t selected = 0;  // list cursor, 1..9
int modal = 0;          // native confirmation dialogs open above a save list
bool reopened = true;  // no save list has been open since the last controller read without one

uint8_t R8(uint32_t address) { return kernel::G<uint8_t>(address); }
bool Valid(uint32_t address, uint32_t size) {
    return address && address <= 0xFFFFFFFFu - size && mem::IsCommitted(address) && mem::IsCommitted(address + size - 1);
}
uint32_t known_buffer = 0;  // the save buffer the shown page was put into
uint32_t Buffer() {
    const uint32_t buffer = kernel::R32(SaveManager);
    if (!Valid(buffer, SlotBase + RegionBytes)) return 0;
    // A new session buffer never holds the page shown before.
    if (buffer != known_buffer) {
        if (known_buffer && book.Shown() != 1) LOGI("openshao save pages: new save buffer, page 1 shown");
        book.AfterRead();
        known_buffer = buffer;
    }
    return buffer;
}
std::span<uint8_t> Region(uint32_t buffer) {
    if (!buffer) return {};
    return {static_cast<uint8_t*>(kernel::HostPtr(buffer + SlotBase)), RegionBytes};
}
void Ensure() { book.Ensure(Region(Buffer())); }

uint32_t GuestText(const std::string& text) {
    static std::unordered_map<std::string, uint32_t> strings;
    if (const auto found = strings.find(text); found != strings.end()) return found->second;
    if (strings.size() >= 256 || text.size() > 127) return 0;
    const uint32_t address = mem::AllocSystem(128);
    if (!address) return 0;
    std::memset(kernel::HostPtr(address), 0, 128);
    std::memcpy(kernel::HostPtr(address), text.data(), text.size());
    strings.emplace(text, address);
    return address;
}
std::string Native(std::string_view id) {
    const auto& language = launcher::Current().language;
    return menu::EncodeNative(menu::Catalog::Builtin().Get(language, id), mods::SameLanguage(language, "pt-BR"));
}
std::string GameText(X86Context& ctx, uint32_t id) {
    const uint32_t address = kernel::CallGuest(ctx, TextLookup, {id});
    if (!Valid(address, 1)) return {};
    std::string text;
    for (uint32_t i = 0; i < 32 && Valid(address + i, 1) && R8(address + i); ++i) text += char(R8(address + i));
    return text;
}
// Native font glyphs: # and $ are the left and right triggers, = and | are A and B, * is X.
std::string IndicatorText() {
    return "#   " + Native("save.page") + " " + std::to_string(book.Shown()) + "/" + std::to_string(PageCount) + "   $";
}
std::string QuestionText(X86Context& ctx) {
    return Native("save.delete_prompt") + "   = " + GameText(ctx, YesText) + "   | " + GameText(ctx, NoText);
}
bool SlotUsed(uint32_t slot) {
    const uint32_t buffer = Buffer();
    return buffer && slot >= 1 && slot <= SlotsPerPage && R8(buffer + SlotBase + (slot - 1) * RecordBytes + 0x84) == 1;
}
void Draw(X86Context& ctx, uint32_t function, uint32_t x, uint32_t y, uint32_t flags, float scale, const std::string& text) {
    const uint32_t address = GuestText(text);
    if (!address) return;
    const uint32_t eax = ctx.eax;
    const uint32_t s = std::bit_cast<uint32_t>(scale);
    kernel::CallGuest(ctx, function, {x, y, 0x60, flags, address, s, s});
    ctx.eax = eax;
}
// Page line under a list: page or delete question, plus the X hint on an occupied slot.
void DrawIndicator(X86Context& ctx, uint32_t y, uint32_t flags, float scale) {
    if (deleting) { Draw(ctx, DrawCentered, 320, y, flags, scale, QuestionText(ctx)); return; }
    Draw(ctx, DrawCentered, 320, y, flags, scale, IndicatorText());
    if (SlotUsed(selected)) Draw(ctx, DrawLeft, HintX, y, flags, scale, "* " + Native("save.delete"));
}
bool MenuListOnTop() {
    const uint32_t depth = kernel::R32(ScreenStack);
    return depth <= 16 && kernel::R32(ScreenStack + depth * 4) == MenuList;
}
void Sound(X86Context& ctx, uint32_t pointer) {
    const uint32_t function = kernel::R32(pointer);
    if (!function) return;
    const uint32_t eax = ctx.eax;
    kernel::CallGuest(ctx, function, {});
    ctx.eax = eax;
}

// Page 1 is emptied through the native writer (checksum and signature); other pages in their own files.
void DeleteSlot(X86Context& ctx, uint32_t slot) {
    const uint32_t buffer = Buffer();
    const auto region = Region(buffer);
    if (!ClearSlot(region, slot)) return;
    if (book.Shown() == 1) {
        kernel::W32(buffer, kernel::CallGuest(ctx, Checksum, {buffer + 4, ChecksumBytes}));
        const uint32_t written = kernel::CallGuest(ctx, WriteFile, {SavePath, buffer, SaveBytes});
        if (written != 1) LOGW("openshao save pages: deleting page 1 slot %u was not written", slot);
    } else {
        book.Commit(region);
        StorePage(book.Shown(), std::vector<uint8_t>(region.begin(), region.end()));
    }
    LOGI("openshao save pages: page %u slot %u deleted", book.Shown(), slot);
}
// Rebuilds the in-game slot icons for the current slot states, as the game does after a save.
void ClearIcons(X86Context& ctx) {
    const X86Context saved = ctx;
    ctx.ecx = MenuManager;
    kernel::CallGuest(ctx, ClearMenuSprites, {0});
    ctx = saved;
}
void Answer(X86Context& ctx, bool yes, bool no) {
    if (!deleting || (!yes && !no)) return;
    const uint32_t slot = deleting;
    deleting = 0;
    holding = true;
    if (yes) {
        DeleteSlot(ctx, slot);
        if (!question_in_menu) ClearIcons(ctx);
        Sound(ctx, AcceptSound);
    }
    else { Sound(ctx, BackSound); LOGI("openshao save pages: page %u slot %u kept", book.Shown(), slot); }
}

// Triggers turn pages while a list is shown; the last page is remembered. X asks before deleting.
void Turn(X86Context& ctx, uint32_t cursor, bool menu) {
    uint8_t left = 0, right = 0, x = 0, a = 0, b = 0;
    for (int port = 0; port < 2; ++port) {
        const auto pad = ::input::Poll(port);
        if (!pad.connected) continue;
        a = std::max(a, pad.analog[0]);
        b = std::max(b, pad.analog[1]);
        x = std::max(x, pad.analog[2]);
        left = std::max(left, pad.analog[6]);
        right = std::max(right, pad.analog[7]);
    }
    selected = cursor;
    // The in-game list runs with a message current: only native dialogs block paging.
    if (modal && deleting) { LOGI("openshao save pages: delete question closed by a dialog"); deleting = 0; }
    // A newly opened list ignores buttons already held; paused frames never count as a new list.
    if (reopened || modal) {
        reopened = false;
        triggers.Resume(left, right);
        delete_button.Resume(x);
    } else if (deleting) {
        triggers.Resume(left, right);
        delete_button.Resume(x);
        const bool yes = yes_button.Update(a), no = no_button.Update(b);
        Answer(ctx, yes, no && !yes);
    } else if (const int delta = triggers.Update(left, right)) {
        book.Want(Step(book.Wanted(), delta));
        if (menu) Sound(ctx, MoveSound);  // in-game, the slot icons already sound when they change
    } else if (delete_button.Update(x) && SlotUsed(selected)) {
        deleting = selected;
        question_in_menu = menu;
        LOGI("openshao save pages: delete question for page %u slot %u", book.Shown(), selected);
        yes_button.Resume(a);
        no_button.Resume(b);
        Sound(ctx, MoveSound);
    }
    if (!book.Ensure(Region(Buffer()))) return;
    LOGI("openshao save pages: page %u shown", book.Shown());
    if (!menu) ClearIcons(ctx);
}
}

uint32_t ShownPage() { return book.Shown(); }
void BeforeCopyIn(X86Context&, uint8_t*, uint32_t) { Ensure(); }
void AfterCopyIn(uint32_t) {}
void BeforeCopyOut(X86Context&, uint8_t*) { Ensure(); }
void AfterCopyOut() {}

// While the delete question is open, and until A and B are released, the game sees no A/B/X.
void HoldAnswer(::input::Pad& pad) {
    if (!MenuListOnTop() && !kernel::R32(SaveManager)) reopened = true;
    // The question ends with its list; frames that skip drawing keep it open.
    if (deleting && (question_in_menu ? !MenuListOnTop() : !kernel::R32(SaveManager))) {
        LOGI("openshao save pages: delete question closed with its list");
        deleting = 0;
        holding = true;
    }
    // Released on every controller that reads the lists, connected or not.
    if (holding && !deleting) {
        uint8_t a = 0, b = 0;
        for (int port = 0; port < 2; ++port) {
            const auto raw = ::input::Poll(port);
            if (!raw.connected) continue;
            a = std::max(a, raw.analog[0]);
            b = std::max(b, raw.analog[1]);
        }
        if (a <= 0x30 && b <= 0x30) holding = false;
    }
    if (!deleting && !holding) return;
    pad.analog[0] = pad.analog[1] = pad.analog[2] = 0;
}
}

using namespace openshao::save_pages;

// Native save file read: whatever page was shown is cached, then the buffer holds page 1 again.
X86_EXTERN_FUNC(__imp__sub_00127420);
X86_FUNC(sub_00127420) {
    const uint32_t path = kernel::R32(ctx.esp + 4), buffer = kernel::R32(ctx.esp + 8);
    const bool save = path == SavePath && buffer && buffer == Buffer();
    __imp__sub_00127420(ctx, base);
    if (!save) return;
    if (ctx.eax == 1) book.AfterRead();
    else book.ReadFailed(Region(buffer));
}

// Native save file write: page 1 goes to the native file, the shown page to its own file.
X86_EXTERN_FUNC(__imp__sub_001272A0);
X86_FUNC(sub_001272A0) {
    const uint32_t path = kernel::R32(ctx.esp + 4), buffer = kernel::R32(ctx.esp + 8);
    // Only a real Save Game (0xA14CA returning to 0xA14CF) confirms the Survival unlock.
    const bool story_write = kernel::R32(ctx.esp) == 0xA14CF;
    const bool save = path == SavePath && buffer && buffer == Buffer();
    auto pending = save ? book.BeginWrite(Region(buffer)) : std::nullopt;
    if (pending) kernel::W32(buffer, kernel::CallGuest(ctx, Checksum, {buffer + 4, ChecksumBytes}));
    __imp__sub_001272A0(ctx, base);
    if (!pending) {
        openshao::survival::unlock::NativeSaveCompleted(ctx.eax == 1 && save, story_write);
        return;
    }
    const uint32_t result = ctx.eax;
    book.EndWrite(Region(buffer));
    kernel::W32(buffer, kernel::CallGuest(ctx, Checksum, {buffer + 4, ChecksumBytes}));
    ctx.eax = result;
    const bool stored = StorePage(pending->first, pending->second);
    openshao::survival::unlock::NativeSaveCompleted(result == 1 && stored && save, story_write);
    if (stored) LOGI("openshao save pages: page %u written", pending->first);
}

// Overwrite and load confirmations run their own loop above the list; paging waits for them.
X86_EXTERN_FUNC(__imp__sub_0001677B);
X86_FUNC(sub_0001677B) { ++modal; __imp__sub_0001677B(ctx, base); --modal; }
X86_EXTERN_FUNC(__imp__sub_00013CF6);
X86_FUNC(sub_00013CF6) { ++modal; __imp__sub_00013CF6(ctx, base); --modal; }

// Main-menu Load Game list.
X86_EXTERN_FUNC(__imp__sub_000152FE);
X86_FUNC(sub_000152FE) {
    const uint32_t self = ctx.ecx;
    const bool shown = Valid(self + 0x3C, 4) && !kernel::R32(self + 0x3C) && !R8(MenuHidden);
    if (shown) Turn(ctx, kernel::R32(MenuSelection) + 1, true);
    __imp__sub_000152FE(ctx, base);
    if (!shown) return;
    const bool koop = kernel::R32(KoopLayout) == 1;
    const uint32_t y = 0x81 + (koop ? 0x0A + 0x1C * 10 : 0x1A * 10) - 4;
    DrawIndicator(ctx, y, 2, .9f);
}

// Turn the page before the slot icons so icons and text come from the same page.
X86_EXTERN_FUNC(__imp__sub_000144A8);
X86_FUNC(sub_000144A8) {
    Turn(ctx, kernel::R32(ctx.esp + 12) + 1, false);
    __imp__sub_000144A8(ctx, base);
}

// In-game Save Game list: indicator between the panel border and the footer (y 400).
X86_EXTERN_FUNC(__imp__sub_0001691F);
X86_FUNC(sub_0001691F) {
    __imp__sub_0001691F(ctx, base);
    DrawIndicator(ctx, 370, 1, 1.f);
}

// While the delete question is open the lists ignore their own input (the cursor stays put).
X86_EXTERN_FUNC(__imp__sub_00011AA5);
X86_FUNC(sub_00011AA5) {
    if (deleting) { kernel::Return(ctx, 0, 2); return; }
    __imp__sub_00011AA5(ctx, base);
}
X86_EXTERN_FUNC(__imp__sub_000129AA);
X86_FUNC(sub_000129AA) {
    if (deleting) { kernel::Return(ctx, 0, 0); return; }
    __imp__sub_000129AA(ctx, base);
}

#include "difficulty.h"
#include <array>
#include <cstdio>
#include <cstdlib>

using namespace openshao::difficulty;
namespace {
unsigned failures = 0;
void Check(bool value, const char* text) { if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", text); } }
}

int main() {
    Check(MenuChoice(1) == Mode::Easy && MenuChoice(2) == Mode::Normal && MenuChoice(3) == Mode::Hard &&
          MenuChoice(4) == Mode::Monk && !MenuChoice(0) && !MenuChoice(5),
          "the expanded selector maps its four rows to logical modes");
    Check(NativeMode(0) == Mode::Easy && NativeMode(2) == Mode::Hard && NativeMode(3) == Mode::Monk && !NativeMode(4),
          "native values keep their meaning and the serialized marker decodes as Monk");
    Check(ProfileFor(Mode::Monk).native == 2 && ProfileFor(Mode::Easy).native == 0,
          "Monk runs on native Hard and Easy uses the internal easier profile");
    Check(ProfileFor(Mode::Normal).incoming == 1.f && ProfileFor(Mode::Hard).incoming == 1.f &&
          ProfileFor(Mode::Normal).attack_delay < 0 && ProfileFor(Mode::Hard).history < 0,
          "Normal and Hard keep the original combat values");
    Check(ProfileFor(Mode::Monk).incoming > 1.f && ProfileFor(Mode::Easy).incoming < 1.f,
          "Monk increases and Easy reduces incoming damage");
    Check(ScaleDamage(10, 2.6f) == 26 && ScaleDamage(1, .6f) == 1 && ScaleDamage(0, 1.7f) == 0 &&
          ScaleDamage(-5, 1.7f) == -5 && ScaleDamage(5, 1.f) == 5,
          "damage scaling only changes positive hits and never removes a hit");
    Check(ScaleHealing(100, 200, 500, .2f) == 120 && ScaleHealing(100, 102, 500, .2f) == 101 &&
          ScaleHealing(100, 500, 500, .2f) == 100 && ScaleHealing(100, 900, 500, .2f) == 100 &&
          ScaleHealing(0, 300, 500, .2f) == 300 && ScaleHealing(1, 500, 500, .2f) == 500 &&
          ScaleHealing(300, 200, 500, .2f) == 200 && ScaleHealing(100, 200, 500, 1.f) == 200,
          "Monk reduces partial recovery, cancels full restores and keeps damage and revivals");
    Check(ProfileFor(Mode::Monk).healing < 1.f && ProfileFor(Mode::Hard).healing == 1.f, "only Monk reduces recovery");
    const float monk_healing = ProfileFor(Mode::Monk).healing;
    Check(ScaleHealing(450, 500, 500, monk_healing, true) == 460 &&
          ScaleHealing(490, 540, 500, monk_healing, true) == 500 &&
          ScaleHealing(499, 549, 500, monk_healing, true) == 500 &&
          ScaleHealing(100, 600, 500, monk_healing, true) == 200,
          "Monk pickups scale their original gain before clamping, even at full health");
    for (const int amount : {1, 2, 5, 10, 50, 100, 500}) {
        bool complete = true;
        for (int initial = 2; initial < 500; ++initial) {
            int health = initial;
            for (int count = 0; count < 500 && health < 500; ++count) {
                const int next = ScaleHealing(health, health + amount, 500, monk_healing, true);
                if (next <= health || next > 500) { complete = false; break; }
                health = next;
            }
            if (health != 500) { complete = false; break; }
        }
        Check(complete, "repeated Monk pickups reach exactly full health from every living health value");
    }
    Check(ScaleHealing(450, 500, 500, monk_healing) == 450 &&
          ScaleHealing(499, 500, 500, monk_healing) == 499 &&
          ScaleHealing(0, 500, 500, monk_healing, true) == 500 &&
          ScaleHealing(1, 500, 500, monk_healing, true) == 500 &&
          ScaleHealing(300, 200, 500, monk_healing, true) == 200,
          "allowing pickups to finish healing preserves Monk totem, revival and damage rules");
    for (const auto other : {Mode::Easy, Mode::Normal, Mode::Hard}) {
        bool unchanged = ProfileFor(other).healing == 1.f;
        for (const int current : {0, 1, 100, 450, 499, 500})
            for (const int requested : {0, 1, 80, 200, 500, 549, 900})
                for (const bool pickup : {false, true})
                    unchanged &= ScaleHealing(current, requested, 500, ProfileFor(other).healing, pickup) == requested;
        Check(unchanged, "Easy, Normal and Hard retain every native health request for pickups and restores");
    }
    Check(ScaleExperience(0, 50, .35f) == 18 && ScaleExperience(0, 100, .35f) == 35 &&
          ScaleExperience(7018, 7068, .35f) == 7036, "experience popup shows the Monk gain");
    Check(ScaleExperience(100, 200, .35f) == 135 && ScaleExperience(100, 101, .35f) == 101 &&
          ScaleExperience(300, 200, .35f) == 200 && ScaleExperience(100, 200, 1.f) == 200 &&
          ProfileFor(Mode::Monk).experience == .35f && ProfileFor(Mode::Hard).experience == 1.f,
          "Monk grants 35 percent experience and keeps purchases");
    Check(Reinforceable(15) && Reinforceable(14) && !Reinforceable(0) && !Reinforceable(1) && !Reinforceable(8) &&
          !Reinforceable(31) && !Reinforceable(36),
          "reinforcements exclude heroes, corpse props and large families");
    Check(HeapExtras(2, 4u << 20, 1u << 20) == 2 && HeapExtras(2, HeapReserve + HeapPerExtra, 1u << 20) == 1 &&
          HeapExtras(2, HeapReserve, 1u << 20) == 0 && HeapExtras(2, 8u << 20, HeapBlock - 1) == 0,
          "reinforcements stop before the object heap runs out");
    Check(Reinforcements(97, 1, 2) == 1 && Reinforcements(97, 2, 2) == 1 && Reinforcements(97, 2, 8) == 0,
          "small ordinary encounters receive one reinforcement within the actor limit");
    Check(Reinforcements(96, 1, 2) == 0 && Reinforcements(96, 3, 2) == 0,
          "the Wu Shi tutorial keeps its original encounter sizes");
    for (const unsigned members : {1u, 2u, 3u, 4u, 5u})
        Check(Reinforcements(7, members, 0, 32) == 0,
              "Tombs Map 8 never receives extra enemies even when capacity is available");
    Check(Reinforcements(6, 1, 0) == 1 && Reinforcements(6, 3, 0) == 2 &&
          Reinforcements(8, 1, 0) == 1 && Reinforcements(8, 3, 0) == 2,
          "the Tombs Map 8 exception leaves both neighboring maps eligible for reinforcements");
    Check(Reinforcements(97, 3, 2) == 2 && Reinforcements(97, 5, 2) == 2 && Reinforcements(97, 6, 2) == 0 &&
          Reinforcements(97, 3, 5) == 1 && Reinforcements(97, 3, 6) == 0 && Reinforcements(97, 0, 2) == 0,
          "reinforcements stay within group, actor and reserve limits");

    Progress progress;
    progress.Enter(10); progress.Track(3);
    Check(progress.Tracked(3) && !progress.Exempt(10), "a visited room is locked until it is committed");
    progress.Leave(10);
    Check(progress.Working().test(10) && !progress.Committed().test(10) && progress.Exempt(10),
          "a cleared room stays open before the next totem save");
    progress.Rollback();
    Check(!progress.Working().test(10) && !progress.Exempt(10) && !progress.Tracked(3), "death discards clearance after the last totem");
    progress.Enter(11); progress.Track(1); progress.Leave(11); progress.Commit();
    Check(progress.Exempt(11), "a totem save commits cleared rooms");
    Check(!progress.Enter(11) && progress.Enter(12) && !progress.Tracked(1), "changing rooms starts a new visit");
    progress.Enter(13); progress.Leave(13);
    Check(!progress.Working().test(13), "rooms without tracked encounters are not marked cleared");

    std::bitset<512> rooms; rooms.set(0); rooms.set(42); rooms.set(511);
    Check(SerializeRooms(rooms) == "0,42,511" && ParseRooms("0,42,511") == rooms, "clearance round-trips through text");
    Check(SerializeRooms({}) == "-" && ParseRooms("-").none() && ParseRooms("1,x").none() && ParseRooms("512").none(),
          "malformed clearance is ignored");

    std::array<uint8_t, 0x100> record{};
    for (size_t i = 0; i < record.size(); ++i) record[i] = uint8_t(i * 7);
    const auto print = Fingerprint(record.data(), record.size(), 0xD4);
    record[0xD4] = 3; record[0xD5] = 0;
    Check(Fingerprint(record.data(), record.size(), 0xD4) == print, "the fingerprint ignores the difficulty marker");
    record[0x10] ^= 1;
    Check(Fingerprint(record.data(), record.size(), 0xD4) != print, "the fingerprint follows the saved record");
    return failures ? 1 : 0;
}

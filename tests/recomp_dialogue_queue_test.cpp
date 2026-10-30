#include "dialogue_queue.h"
#include <iostream>
int main() {
    using namespace openshao::dialogue;
    unsigned failures = 0;
    auto check = [&](bool ok, const char* text) { if (!ok) { ++failures; std::cerr << text << '\n'; } };
    // Wu Shi opening: Raiden coin line, then the authored Liu Kang follow-up.
    constexpr uint32_t liu = 0x3185C09E, kung = 0x5873A43A, room = 96;
    Queue queue;
    check(queue.Push({liu, 0x871111EC, 0, room}), "A rejected follow-up must be kept");
    check(!queue.Push({liu, 0x871111EC, 0, room}), "A repeated request must not play twice");
    check(queue.Push({kung, 0x871111EC, 1, room}), "Another line must queue after the first");
    queue.Age(3.0, true);queue.Age(Queue::MaxWait - .5, false);
    check(queue.Size() == 2, "Pause time must not expire waiting lines");
    auto first = queue.Pop();
    check(first && first->sound == liu && queue.Size() == 1, "Lines must play in request order");
    queue.Age(1.0, false);
    check(queue.Size() == 0, "A line that waits too long must be dropped");
    queue.Push({liu, 0, 0, room});queue.DropOtherRooms(room + 1);
    check(queue.Size() == 0, "Changing rooms must discard pending lines");
    for (uint32_t i = 0; i < Queue::Capacity + 2; ++i) queue.Push({i + 1, 0, 0, room});
    check(queue.Size() == Queue::Capacity, "The queue must stay bounded");
    queue.Clear();
    check(!queue.Pop(), "Clearing must remove every pending line");
    check(queue.Push({liu, 11, 0, room}) && queue.Push({liu, 12, 0, room}),
          "The same sound ID in different explicit banks must remain two distinct requests");
    check(ActorAccepts(0xFFFFFFFF, 0) && ActorAccepts(0, 0) && ActorAccepts(1, 1) && !ActorAccepts(1, 0),
          "The deferred request must apply the retail actor filter");
    return failures ? 1 : 0;
}

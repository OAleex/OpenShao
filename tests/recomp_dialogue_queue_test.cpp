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
    queue.Push({liu, 0, 0, room});
    queue.Age(Queue::MaxBlocked - 1, false, true);
    check(queue.Size() == 1, "A line may wait behind another voice for a while");
    check(queue.Age(2.0, false, true) == 1 && queue.Size() == 0,
          "A line kept behind other speech for too long must be dropped, not played out of context");
    Line after{liu, 0, 0, room};after.after_cinematic = true;queue.Push(after);
    queue.Age(Queue::MaxBlocked * 2, false, true, true);
    check(queue.Size() == 1, "A line kept for after a cinematic must wait for the whole cinematic");
    queue.Age(Queue::MaxBlocked * 2, true, true);
    check(queue.Size() == 1, "Pause time must not count as blocked time");
    queue.Clear();
    Line timed{liu, 0, 0, room};timed.timed = true;timed.behind = 0xA1;queue.Push(timed);
    queue.Age(Queue::MaxLate * 4, false, true, false, true, 0xA1);
    check(queue.Size() == 1, "A deferred reply must wait out the long voice that blocked it");
    queue.Age(Queue::MaxLate - .5, false, true, false, true, 0xB2);
    check(queue.Size() == 1, "A deferred native line may still play shortly after its moment");
    queue.Age(1.0, false, true, false, true, 0xB2);
    check(queue.Size() == 0, "A deferred native line held behind other speech must drop instead of playing out of sync");
    queue.Push(timed);queue.Age(Queue::MaxLate * 4, true, false, false, true);
    queue.Age(Queue::MaxLate * 4, false, true, true, true);
    check(queue.Size() == 1, "Pause and cinematic time must not make a deferred native line late");
    queue.Clear();
    queue.Push({kung, 0, 0, room});queue.Age(Queue::MaxLate * 2, false, true, false, true);
    check(queue.Size() == 1, "Untimed lines keep their own waiting rules");
    queue.Clear();
    queue.Push({liu, 0, 0, room});queue.DropOtherRooms(room + 1);
    check(queue.Size() == 0, "Changing rooms must discard pending lines");
    for (uint32_t i = 0; i < Queue::Capacity + 2; ++i) queue.Push({i + 1, 0, 0, room});
    check(queue.Size() == Queue::Capacity, "The queue must stay bounded");
    queue.Clear();
    check(!queue.Pop(), "Clearing must remove every pending line");
    check(queue.Push({liu, 11, 0, room}) && queue.Push({liu, 12, 0, room}),
          "The same sound ID in different explicit banks must remain two distinct requests");
    queue.Clear();
    Line extra{0xE1, 0, 0, room};extra.optional = true;queue.Push(extra);
    check(!queue.HasRequired(), "Optional additions alone must not hold back native speech");
    Line native{0xA1, 0, 0, room};native.timed = true;queue.Push(native);
    check(queue.HasRequired() && queue.Pop()->sound == 0xA1 && queue.Pop()->sound == 0xE1,
          "A deferred native line goes ahead of an optional addition");
    for (uint32_t i = 0; i < Queue::Capacity; ++i) { Line o{0xE10u + i, 0, 0, room}; o.optional = true; queue.Push(o); }
    check(queue.Push(native) && queue.Size() == Queue::Capacity && queue.Pop()->sound == 0xA1,
          "A full queue of additions must still accept native speech");
    queue.Clear();
    check(ActorAccepts(0xFFFFFFFF, 0) && ActorAccepts(0, 0) && ActorAccepts(1, 1) && !ActorAccepts(1, 0),
          "The deferred request must apply the retail actor filter");
    return failures ? 1 : 0;
}

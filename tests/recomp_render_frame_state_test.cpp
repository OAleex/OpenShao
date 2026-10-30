#include "render_frame_state.h"
#include <iostream>

int main() {
    using namespace openshao::frames;
    unsigned failures=0;
    auto check=[&](bool ok,const char* text){if(!ok){++failures;std::cerr<<text<<'\n';}};
    constexpr uint32_t address=0x6644E0;
    FontQueue queue{};
    const auto write=[&](size_t offset,uint32_t value){std::memcpy(queue.data()+offset,&value,4);};
    check(ValidFontQueue(queue,address),"An empty native font queue must be reusable");
    write(0,2);write(4,7);
    write(8+0x34,address+0x5408);write(8+0x54+0x34,address+0x540A);
    std::memcpy(queue.data()+0x5408,"A\0Exit\0",7);
    check(ValidFontQueue(queue,address),"A queued button and label must retain both native strings");
    const auto original=queue;
    write(0,257);check(!ValidFontQueue(queue,address),"An oversized record count must disable extra rendering");
    queue=original;write(4,4097);check(!ValidFontQueue(queue,address),"The string arena must stay inside its allocated region");
    queue=original;write(8+0x34,address+0x6408);check(!ValidFontQueue(queue,address),"A string must not read the font configuration after the arena");
    queue=original;write(8+0x34,address+0x540F);check(!ValidFontQueue(queue,address),"A stale string beyond the current arena cursor must disable extra rendering");
    queue=original;queue[0x540E]='!';check(!ValidFontQueue(queue,address),"A missing terminator must not read stale arena contents");
    queue=original;write(0,0);write(4,0);check(ValidFontQueue(queue,address),"A flushed queue may retain old records and strings");
    check(!ValidFontQueue(std::span(queue).first(8),address),"A partial queue snapshot must be rejected");
    return failures?1:0;
}

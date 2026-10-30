#include "frame_status.h"
#include <iostream>
#include <string_view>

int main() {
    unsigned failures=0;
    const auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<'\n';}};
    using namespace openshao::frames;
    Window window;
    check(window.Dominant()==Block::None,"An empty window must not report a limit");
    window.Add(Block::Fonts);window.Add(Block::Sprites);window.Add(Block::Sprites);window.Add(Block::None);
    check(window.Dominant()==Block::Sprites,"The most frequent limit must be reported");
    window.Add(Block::Count);
    check(window.Dominant()==Block::Sprites,"Out-of-range reasons must be ignored");
    for(int i=0;i<int(Block::Count);++i)check(BlockName(Block(i))[0]!=0,"Every limit must have a display name");
    check(BlockName(Block::Late)==std::string_view("frame time"),"Missed output frames must be reported as frame time");
    return failures?1:0;
}

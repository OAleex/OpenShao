#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace openshao::frames {
// Why a gameplay interval was shown without an interpolated output frame.
enum class Block : uint8_t {None, Buffer, Sprites, Polygons, Fonts, Queue, Scene, State, Late, Fault, Count};

inline const char* BlockName(Block block) {
    switch(block) {
    case Block::Buffer: return "render data";
    case Block::Sprites: return "sprite limit";
    case Block::Polygons: return "polygon limit";
    case Block::Fonts: return "text queue";
    case Block::Queue: return "render queue moved";
    case Block::Scene: return "scene change";
    case Block::State: return "non-gameplay frame";
    case Block::Late: return "frame time";
    case Block::Fault: return "state check";
    default: return "none";
    }
}

struct Window {
    uint32_t ticks=0,extras=0;
    std::array<uint32_t,size_t(Block::Count)> blocked{};
    void Add(Block block){if(block!=Block::None && block<Block::Count)++blocked[size_t(block)];}
    Block Dominant()const {
        size_t best=0;
        for(size_t i=1;i<blocked.size();++i)if(blocked[i]>blocked[best])best=i;
        return blocked[best]?Block(best):Block::None;
    }
};

}

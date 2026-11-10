#pragma once
#include "pipeline_precompile.h"
#include <deque>
#include <string>
#include <unordered_map>

namespace openshao::pipelines {
// Keep exact recipe bytes (state can vary without shader changes).
class SourceHashes {
public:
    template<class Generate> uint64_t Vertex(const VertexRecipe& key, Generate generate) {
        return Get('v', &key, sizeof(key), generate);
    }
    template<class Generate> uint64_t Pixel(const gpu::PixelKey& key, Generate generate) {
        return Get('p', &key, sizeof(key), generate);
    }
private:
    template<class Generate> uint64_t Get(char stage, const void* bytes, size_t size, Generate generate) {
        std::string key(1, stage);
        key.append(static_cast<const char*>(bytes), size);
        if (auto found=hashes_.find(key); found!=hashes_.end()) return found->second;
        const uint64_t hash=generate();
        if (hashes_.size()>=4096) {
            hashes_.erase(order_.front());
            order_.pop_front();
        }
        order_.push_back(key);
        hashes_.emplace(std::move(key),hash);
        return hash;
    }
    std::unordered_map<std::string,uint64_t> hashes_;
    std::deque<std::string> order_;
};
}

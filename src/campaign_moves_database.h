#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace openshao::campaign_moves {

struct ExtendedDatabase {
    std::vector<uint8_t> bytes;
    std::array<uint32_t, 4> extra_root_ids{};
};

bool ExtendDatabase(std::span<const uint8_t> raw,
                    std::span<const uint32_t> text_crcs,
                    ExtendedDatabase& result, std::string& error);

}

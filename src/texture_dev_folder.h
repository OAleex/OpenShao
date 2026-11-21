#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "texture_pack_source.h"

// The developer mode's texture folder: PNG files named <hash>_<format>_<width>x<height>.png, as the extraction tool writes them,
// with a manifest.json that holds the signature of each picture's pixels. Only the pictures whose pixels differ from their
// signature count as replacements; the others are the game's own textures as they were extracted. The design is in
// docs/MODO_DESENVOLVEDOR.md.
namespace openshao::texpack {
// SHA-256, in hexadecimal, of "<width>x<height>:" followed by the RGBA bytes row by row, top row first: what the manifest holds
std::string PixelSignature(uint32_t width, uint32_t height, const uint8_t* rgba);

struct DevFolderReport {
    size_t files = 0;         // PNG files looked at
    size_t edited = 0;        // pixels differ from the signature: these are the replacements
    size_t unchanged = 0;     // pixels as extracted: left out
    size_t unsigned_files = 0;  // no signature in the manifest: counted as edited
    size_t refused = 0;       // would not decode, or too small or large, or not in the proportion of the original
    size_t ignored = 0;       // a name that is not <hash>_<format>_<width>x<height>.png
    size_t duplicates = 0;    // a hash that came twice: the file with the smaller name is used
    double seconds = 0;
    bool manifest = false;    // manifest.json was there and could be read
    std::vector<std::string> problems;  // what was refused or odd, with the file name; the first ones only
};

// Null when nothing in the folder is a replacement (the report says why). `threads` 0 uses what the machine has.
std::shared_ptr<const PackSource> OpenDevFolder(const std::filesystem::path& folder, DevFolderReport& report, unsigned threads = 0);
}

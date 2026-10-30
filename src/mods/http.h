#pragma once
#include "progress.h"
#include <filesystem>
#include <string>
#include <stdexcept>
#include <cstdint>
namespace openshao::mods {
struct HttpError : std::runtime_error {
    int status;
    int64_t retry_at;
    HttpError(int code, int64_t retry);
};
void Download(std::string_view url,const std::filesystem::path& file,uint64_t maximum,const Progress& progress={});
}

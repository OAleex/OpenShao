#pragma once
#include <string>
#include <string_view>

namespace openshao::privacy {
// Used at the log boundary and again when exporting logs from older versions.
std::string Redact(std::string_view text);
}

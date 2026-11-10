#pragma once
#include <cstdint>
struct X86Context;
namespace openshao::audio::options {
void Apply(X86Context& ctx,int mode);
}

#pragma once

namespace openshao { struct Options; }

namespace openshao::discord {
enum class State { Launcher, Playing };
void SetState(State state, const Options& options, bool enabled = true);
void Shutdown();
}

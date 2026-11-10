#pragma once
#include "survival_unlock_state.h"

namespace openshao::survival::unlock {
bool Available();
bool Persisted();
bool Pending();
Encounter Discover();
void NativeSaveSlotPrepared();
void NativeSaveCompleted(bool successful, bool from_story_save);
void CampaignLoaded();
}

#pragma once

namespace openshao::scorpion::audio_preview {
// AFS indices 0..3; reads the player's own game files, without bundling voices.
bool Play(unsigned index);
void Stop();
// Play an authored campaign cue from a localized AFS bank.
bool PlayCampaignVoice(unsigned bank, unsigned index);
void StopCampaignVoice();
bool CampaignVoicePlaying();
// Called after the original guest-audio render, on the existing output device.
void Mix(float* interleaved, unsigned frames,unsigned channels=2);
}

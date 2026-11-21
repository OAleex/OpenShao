#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace input { struct Pad; }
namespace openshao::photo_mode {
struct PreviewParams;
// Game pad callback: keyboard mapping first, then photo mode takes the pad while it is open.
void Poll(int port, input::Pad& pad);
bool Active();
// True while photo mode queues its own controls through the 2D passes it otherwise blocks.
bool DrawingControls();
// ImGui callback: the performance overlay, then the photo mode controls and the saved notice.
void Ui(double fps);
// Select root: whether this game mode offers photo mode (not Versus), and the row that opens it.
bool Available();
void OpenFromMenu();
// Window key message: F12 takes a screenshot anywhere in the game; true when handled.
bool Message(uint32_t message, uintptr_t key, intptr_t flags);
// Renderer, once per present: true once after the player asked for a photo.
bool ShotRequested();
// Renderer: the game frame as BGRA rows at render size, shown on screen at this aspect; empty when the copy failed.
void SaveShot(std::vector<uint8_t> bgra, uint32_t width, uint32_t height, float aspect);
// Settings: the factor on the render scale while a High or Max photo is taken, given the normal render height.
float RenderBoost(float render_height);
// Vulkan renderer, presenting: the options panel and effects pass for this frame; false keeps the plain blit.
bool Preview(float aspect, PreviewParams& out);
// HLSL source of the Vulkan preview pass (vs and psmain).
const char* PreviewShader();
// Text queue render (game thread): the first one of a photo mode frame lets the renderer put the effects under the controls.
void BeforeText();
// Renderer: true once after BeforeText; the next 2D draw (or the present) gets the scene effects first.
bool TakeSceneArm();
// Renderer: scene pass constants (filter, intensity, vignette, aspect, colour range, panel); false when none is needed.
bool SceneParams(float params[8]);
// HLSL of the scene pass (vs and psmain), drawn with the renderer's Pass.
const char* SceneShader();
// Renderer: the frame being presented carries the effects, drawn under its controls or over the finished frame.
void NoteSceneInFrame(bool before_controls);
// d3d8 layer: returns once the renderer has drawn everything queued so far.
void FlushRenderer();
}

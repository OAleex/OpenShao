# Graphics diagnostics

This guide covers local rendering investigations. Frame captures require **Direct3D 12**.
The last section describes Vulkan device diagnostics.
For an initial error report, follow [How do I report a problem?](../README.md#how-do-i-report-a-problem).

Captures contain game textures, shaders and other resources. Keep them local.
Public reports should use screenshots and a text summary with private data removed.
See the [contribution rules](../CONTRIBUTING.md#game-files-and-private-data).

| Task | Build requirement |
| --- | --- |
| Capture one frame with F3 | Standard Direct3D 12 release. Set `XBE_CAPTURE_DIR` before startup. |
| Compare experimental shadow paths | Direct3D 12 build with `OPENSHAO_SHADOW_DIAGNOSTICS=ON`. |
| Collect the five-frame shadow suite | Diagnostic build and the capture script described below. |

## Material captures

F3 requests one diagnostic frame capture when `XBE_CAPTURE_DIR` contains an output path.
The standard release does not enable frame captures by default.
The capture includes rendering resources and state, not only a screenshot.

1. Open PowerShell in the folder that contains `OpenShao.exe`.
2. Run these commands:

```powershell
$env:XBE_CAPTURE_DIR = Join-Path (Get-Location) 'captures'
.\OpenShao.exe
```

3. Enter the affected scene.
4. Press **F3** once.
5. Wait until the log reports the completed capture.
6. Close the game.
7. Remove the variable from that PowerShell session:

```powershell
Remove-Item Env:XBE_CAPTURE_DIR
```

The output directory contains a timestamped folder for each captured frame.
The log identifies the output path.
Each capture includes textures, generated shaders, target images and `frame.txt`.
The frame log includes alpha testing, fog, blending, texture filtering, addressing and color-key state.
Logged guest antialiasing states do not establish that the host renderer implements those modes.

### Capture selected draw steps

Set `OPENSHAO_CAPTURE_DRAW_STEPS` before startup to select decimal draw IDs from `frame.txt`.
Use commas between IDs. For example, `125,218,347` selects three draws from the captured frame.
The extra images use the target's host resolution and the filename `draw_<id>_<target>.bmp`.
Selected draws also save assembled vertex/index data and shader constants as binary files.
The frame log describes their layouts.

This option applies only during F3 captures. GPU readbacks can pause rendering.
Draw IDs can change between frames. Check each frame log before comparing images.
Remove the variable from the terminal session after the investigation.

## Extra-map surfaces

Some extra-map meshes have incomplete strip-orientation metadata. Verified repairs
match the complete packed vertices and strip words before the native index-building
routine runs. Known meshes without a winding reference use the original engine's
two-sided object flag.

Both renderers use these source repairs. The runtime preserves unknown or modified meshes and original files on disk.
Culling follows the requested state. The renderer does not select repairs by GPU or infer them during drawing.
Vulkan runtime validation remains separate
from the shared policy checks.

## Shadow depth storage

The Direct3D 12 renderer prefers D32 for offscreen depth-only guest formats `0x2C`, `0x2D`, `0x30` and `0x31`.
Selection uses guest format, surface dimensions and device capabilities.
The matching resource, depth view, shader view, pipeline and depth-copy formats use the same selection.
Screen-sized buffers and formats with stencil retain D24S8 storage.
An adapter without D32 depth rendering, R32 sampling or R32 comparison sampling keeps D24S8 and logs an error.

The D32 policy addresses incorrect sampling from cleared D24S8 regions observed on an affected AMD adapter.
Raw copies and sampled results differed in that investigation.
This result does not establish compatibility for every GPU, scene or texture.

The constant depth bias keeps 24-bit UNORM units. On a D32 map, each unit scales with the primitive's depth exponent.
The units match from 0.5 to 1 and are smaller below 0.5. The session log reports the first constant biases on D32 maps.

## Build the capture tools

1. Open an x64 Visual Studio developer terminal.
2. Add the selected LLVM, CMake and Ninja tools to its `PATH`.
3. Configure the Direct3D 12 build using the [build instructions](BUILDING.md).
   Use a separate output directory, such as `build/shadow-diagnostics`, with `--configure-only`.
4. Enable the diagnostic option:

   ```powershell
   cmake -S . -B build/shadow-diagnostics -DOPENSHAO_SHADOW_DIAGNOSTICS=ON
   ```

5. Build the executable:

   ```powershell
   cmake --build build/shadow-diagnostics --parallel 2
   ```

6. Run its tests:

   ```powershell
   ctest --test-dir build/shadow-diagnostics --output-on-failure
   ```

Use the CMake, Ninja and LLVM tools selected during configuration.
The normal build script explicitly sets this option to `OFF`, including when reusing a build directory.
It retains the D32 format regression test.
Enabling diagnostics adds shader-comparison, raster-clear and capture-sequence tests.

## Collect a diagnostic archive

1. Copy [Capture-Shadow-Error.cmd](../tests/Capture-Shadow-Error.cmd) beside the diagnostic executable.
2. Open that script.
3. Enter the affected scene.
4. Keep the character still.
5. Press F3 once.
6. Wait for the five captures to finish.
7. Close the game.
8. Inspect the local `Captures-Diagnostics-<timestamp>.zip` reported by the script.

Keep this archive local. It contains game resources and is unsuitable for a public attachment.

The script places `gpu-info.txt`, the executable hash and `runtime.log` inside `captures/` beside the executable.
It creates the archive after the game exits.
Captures can pause rendering. The session log reports `shadow diagnostic suite complete` when the sequence finishes.
The archive includes existing contents of `captures/`.

Use a separate executable folder for a new investigation.
Keep the capture directory and its parent writable.

The five captures use native rendering, native rendering with an earlier raw depth copy,
rasterized clearing, comparison in the shader, and both alternatives combined.
The suite restores the process defaults after the last capture.
The separate native baseline helps detect whether an earlier readback changes the error.

Each capture records depth formats, clear operations, stage bindings and available GPU validation messages.
Depth BMP files show sampled depth. Raw depth files record resource copies before sampling and after the frame.
`frame.txt` records guest depth state and viewport depth range.

The first mode also runs 24 offscreen Direct3D 12 cases covering depth formats, target sizes and clear methods.
Those cases compare sampled images and raw values against known patterns.
They are separate from the D3D11 shader checks run by CTest.
Warning 821 can occur in synthetic cases that vary optimized clear values.
Inspect other warnings and errors separately.

## Individual diagnostic options

These options require `OPENSHAO_SHADOW_DIAGNOSTICS=ON` at build time:

| Variable | Effect when set to `1` |
| --- | --- |
| `OPENSHAO_SHADOW_SHADER_COMPARE` | Compare shadow depth in the shader |
| `OPENSHAO_RASTER_DEPTH_CLEAR` | Rasterize offscreen depth-only clears |
| `OPENSHAO_CAPTURE_SUITE` | Run the five-frame sequence from one F3 request |

Set `XBE_CAPTURE_DIR` to the output directory to enable F3 captures.
Set `XBE_D3D12_DEBUG=1` to request the installed Direct3D 12 debug layer.
Remove diagnostic variables after an investigation.
Capture tests help identify the cause of an error.
They do not establish correctness for a complete scene or campaign.

## Survival 5 arena recovery

The supported Xbox Survival 5 scene (A4, survival level 4) has an incomplete export
and no authored camera. Recovery uses the existing Xbox Pit scene and textures from the player's game.
It uses Pit camera parameters and preserves the original A4 collision, markers and gameplay resources.
The initial player position uses the native teleport routine.
Modified room descriptors keep their own behavior.

This is an approximation using existing Xbox content: the complete Pit scene has
more geometry than the console Survival 5 reference. It does not establish the exact
intended scene. Source files contain no game resources. Entry and movement checks
do not certify every survival wave or multiplayer behavior.

## Graphics cache details

The Direct3D 12 renderer stores compiled shaders and pipelines in `cache/d3d12/` inside the data folder.
The Vulkan renderer stores its pipeline cache in `cache/vulkan_pipelines.bin`.
Compatible cache entries can reduce compilation in later sessions.
New scenes can still require compilation.

The Direct3D 12 log uses the `graphics cache:` prefix for cache statistics.
These entries include loaded records, cache hits, compilation counts, pipeline creation time and storage errors.
Renderer changes can invalidate caches. GPU or driver changes can invalidate pipeline entries.
The renderer compiles missing, damaged or rejected entries again.

To compare sessions:

1. Play through the affected area.
2. Close the game.
3. Start the game again.
4. Follow the same route.
5. Keep both session logs.

## Vulkan device diagnostics

These device checks apply to the Vulkan target.
The `vulkaninfo --summary` command lists available devices and their capabilities.

| Variable | Effect |
| --- | --- |
| `OPENSHAO_VK_DEVICE` | Prefer a supported GPU whose name contains this value. The match is case-sensitive. Normal device selection applies without a match. |
| `OPENSHAO_VK_VALIDATION=1` | Request an installed Khronos validation layer. |

Set the variable in the terminal session before starting the executable.
The session log identifies initialization errors and rejected devices.
Remove diagnostic variables from that session after the investigation.

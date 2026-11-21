# Mods

This guide is for mod authors. Players install, update and enable mods in the launcher's **Mods** page.
The launcher checks downloads, files and patch results with SHA-256.
A failed installation preserves the previous version.

Keep original game resources and private authoring files out of public commits and
attachments. Read the [contribution rules](../CONTRIBUTING.md#game-files-and-private-data)
and [AI usage policy](../AI_POLICY.md) before submitting work.

Mods can offer creative additions or speculative reconstructions, including
AI-generated content absent from the original game. Label them as new creations or
interpretations. Disclose AI use. Keep them separate from restorations in the base game.
Current support covers resource replacements and routine scripts, with the limits below.

## Compatibility

- Different language packs can remain installed and enabled. Only the selected language applies.
- Two enabled packs cannot provide the same language.
- Content mods apply with the selected language pack.
- Mods cannot replace the same file, archive record, voice or embedded font.
- Replacing a whole archive conflicts with mods that replace its individual resources.
- Dependencies must be installed, enabled and sufficiently recent.
- An enabled dependent mod prevents disabling or removing its dependency.
- Routine replacements cannot target the same function. Before and after hooks can share a target.

The launcher reports conflicts when installing or enabling a mod.
It checks routine scripts again at game startup.
Content must match the supported original game and the declared minimum Recomp version.
Unknown future mod features remain unavailable until the runtime supports them.

## Prepare a package

Use the [OpenShao-Mods builder](https://github.com/OAleex/OpenShao-Mods).
Add the mod to its `mods.json` before building.
The builder creates `mod.json` inside the package. Older packages with `manifest.json` remain supported.
Keep the mod ID and installation directory unchanged across updates.

Required metadata:

| Field | Purpose |
| --- | --- |
| `id` | Stable mod identifier |
| `name`, `summary`, `authors` | Information shown in Mods |
| `type` | `language`, `texture`, `data`, `gameplay` or `mixed` |
| `install_dir` | `packages/<id>` or `languages/<id>` for language packs |
| `versions` | Release records maintained by the builder |

Language mods declare their code in `language`.
Set `language_name` to show a readable name in the selector.
Custom languages use the original English slot. Prepare their text, fonts and audio for that slot.
Fonts retain the original resource headers and character encoding constraints.
New scripts do not add Unicode font rendering to the original game.

Put finished files in `mods/<id>/pack/`:

| Path | Runtime use |
| --- | --- |
| `gamedata/recordNNNNN.bin` | Replace an uncompressed PWF resource |
| `gamedata/recordNNNNN.ewdf` | Replace an EWDF resource after validation and decompression |
| `audio/voices/*.adx` | Replace matching recordings in the English and shared voice banks |
| `audio/movies/<name>.adx` and `.aix` | Replace both audio representations of a movie |
| `fonts/small.xpr`, `fonts/large.xpr` | Replace the embedded fonts |
| `hd/index.txt`, `hd/*` | Replace game textures with high resolution ones, as a texture pack |
| `files/<game path>` | Replace a directly read game file |
| `scripts/*.lua` | Supply routine callbacks listed in `routines` |
| `README.txt`, `CREDITS.md` | Instructions and attribution |

The `pack/` directory is local authoring input. Keep it untracked.
Review the generated package before publication. The project code license does not cover game
assets or third-party mod content.

The archive planner combines compatible resources from active mods.
It preserves original disc files. Direct replacements apply only to game-disc reads.
They do not replace saves, settings or host files.
Both movie audio payloads must match their original track lengths.

Use `sources` to map replacement files to original game data for xdelta generation.
The builder infers sources for numbered `gamedata/` records.
Other source entries specify either `file`, optional `offset` and `size`, or `wad` and `record`.
All patches use the original game as their source, including updates.

`dependencies` contains objects with `id` and minimum `version`.
`conflicts` contains incompatible mod IDs.
`routines` contains the script paths to execute.
The builder derives required `features`, package hashes and release records.
New packages require Recomp 0.2.0 or later. Existing PT-BR packages remain supported.

## Texture packs

A texture pack replaces game textures with larger ones. The pictures are read outside the game archive and kept only in video memory.
Several packs can be enabled at once. Direct3D 12 and Vulkan both replace textures.

A game texture is identified by the SDK `Hash` of the bytes of its level 0, as 16 hexadecimal digits.
A replacement must have the same proportion as the game's texture.
The pictures are read on threads of their own. A texture is drawn as the game made it until its replacement is ready, which takes a few frames.

### Single-file packs (`.ostp`)

The mod's package holds one `pack.ostp`. Its manifest has `"schema": 3` and an `ostp` block with the file's `path`, `size` and `sha256`. It has no `files` or `patches`.
[TEXTURE_PACK_FORMAT.md](TEXTURE_PACK_FORMAT.md) specifies the file.
**Install from file** on the Mods page takes a `.ostp` directly. The launcher checks the file's structure and SHA-256 before it installs the pack.

- **Quality.** High uses every level of each texture. Low uses each texture at twice the game's size, which needs a quarter of the video memory.
  The choice is stored by mod ID in `settings.toml` and applies the next time the game starts.
  Pictures can be up to 8192 pixels on a side. An 8K texture takes about 85 MB of video memory, so use Low on cards with little of it. Sizes above 4K are untested in the game.
- **Order.** When several packs replace the same texture, the first in the order on the Mods page wins.
  The order is stored by mod ID in `settings.toml`. Without a stored order, packs follow their installation order.
- **Video memory.** Replacements stay within a budget. The ones drawn least recently go first, and a texture that was taken out is read again when it is drawn.
  The budget is 60% of the memory the driver reports for the game (`VK_EXT_memory_budget` on Vulkan, the adapter's budget on Direct3D 12), or 40% of the largest local heap when the driver reports none.
  `OPENSHAO_HD_VRAM_MB` sets it in MiB.
- **Preload.** A pack can list textures to prepare before the game draws them: those of the menus and the first picture the game shows.
- **Diagnostics.** `OPENSHAO_HD_LOG_MISSES=1` writes to the log, once each, the textures the game drew that no pack replaces (`no replacement for <hash>`)
  and the kinds that cannot be replaced (`not replaceable`: render targets, depth textures and linear textures).

The [OpenShao ModKit](https://github.com/HexAbyss/OpenShao-ModKit) extracts the textures from the player's own copy of the game and compiles edited pictures into a `.ostp`.

### Folder packs (`hd/`)

A mod with an `hd/` folder lists `hdtextures` in `features`.
`hd/index.txt` has one line for each replaced texture: the hash, the width, the height, the number of levels and the file, separated by spaces.
Lines starting with `#` are comments.

```
# hash width height levels file
0bac2904604d3ecc 2048 2048 10 tex/0bac2904604d3ecc.dds
```

- Each file is a DDS in the `DXT5` format with every level, from the largest down. Each level has sides that are a multiple of 4.
- A file that is missing or does not match its line is not used, and the game's texture stays.

`hd/preload.txt` is optional. It lists, one hash per line, the replacements to prepare before the game draws them.
Lines starting with `#` are comments, and a hash without a line in `index.txt` is ignored.
Keep it short: each listed texture takes video memory from the start.

### Developer mode

Turn on **Developer** on the Files page to try textures without building a pack, then choose a folder on the Developer page.
The folder holds PNG files named `<hash>_<format>_<width>x<height>.png` and the `manifest.json` that the ModKit's extraction writes.
The game reads the folder when it starts, before every texture mod.
It uses only the pictures whose pixels differ from the signature in the manifest. Nothing is compressed in this mode.
Restart the game to see a change.

To try a folder of `.dds` files with an `index.txt` the same way, set `OPENSHAO_HD_PACK_DIR` to the folder.
The game reads it before the developer folder and the packs of the enabled mods.

## Routine scripts

Routine mods use Lua 5.4.9 source scripts.
Each script returns an array of hooks with a guest function `address` and callbacks.
Use `before` to change arguments or state before the original routine.
Use `after` to change its result or state afterward.
Use `replace` to supply the routine behavior.

```lua
return {
    {
        address = 0x00011BCC,
        before = function(ctx)
            local caller = game.read_u32(ctx.esp)
        end,
        after = function(ctx)
            -- Inspect or change guest registers and game state here.
        end,
    },
}
```

Determine targets and calling conventions from the supported game before writing a mod.
The generated `mod_routine_targets.h` lists available addresses for the current build.
Host compatibility overrides and functions without translated entries are unavailable.
Hooks cover direct and indirect calls through translated function entry points.
They do not intercept arbitrary instructions or internal jumps within a function.

Callbacks receive the registers `eax`, `ecx`, `edx`, `ebx`, `esp`, `ebp`, `esi` and `edi`.
They also receive the flags `cf`, `zf`, `sf`, `of`, `pf` and `df`.
Register values are unsigned 32-bit integers. Flags contain zero or one.

A replacement can declare `stack_pop`, in bytes, for callee-cleaned arguments.
The runtime also removes the four-byte return address after a successful replacement.
Do not remove it again in the script.

| API | Operation |
| --- | --- |
| `game.read_u8`, `game.read_u16`, `game.read_u32` | Read an unsigned value at a guest address |
| `game.write_u8`, `game.write_u16`, `game.write_u32` | Stage an unsigned value at a guest address |
| `game.read_f32`, `game.write_f32` | Read or stage a single-precision value |

Script reads see earlier staged writes from the same callback.
Writes commit only after the callback and register checks succeed.
A failed callback preserves its registers and staged memory changes.
The runtime logs the error and disables that script for the session.
A failed replacement uses the original routine.

Each script has an 8 MiB allocation limit and a budget of 100,000 Lua instructions per callback.
Active scripts share a 64 MiB allocation limit.
Callbacks can stage at most 256 memory writes.
Script entry files cannot exceed 1 MiB.
The runtime serializes callbacks within each script across guest threads.
Shared hooks follow mod-ID order and script-list order.

Host files, processes, networking, native modules and dynamic script loading are unavailable.
Protected calls, coroutines, metatable changes and native string-pattern searches are unavailable.
Guest memory access requires valid, committed addresses.
Changing guest code bytes does not change compiled instructions. Use routine hooks instead.
The runtime does not support DLL, shared-library or raw Xbox executable mods.

## Publish and update

1. Build the package with the OpenShao-Mods builder.
2. Test the generated `.tar.xz` through **Install from file**.
3. Attach it to a public OpenShao-Mods release using the printed tag.
4. Publish the updated `mods.json` on `master`, which is the branch read by the launcher.

Publish the asset before the catalog that points to it.
Repeat this process with a higher version to offer an update.
Add a new mod entry before building a different mod.
The launcher reads the catalog rather than scanning release names.
New mods appear after reopening the launcher or selecting **Check for updates**.

Test appearance, audio and gameplay for each release.
Automated installer and routine tests cannot establish compatibility for every future mod.

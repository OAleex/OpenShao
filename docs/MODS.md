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

A texture pack replaces game textures with larger ones, without using the game archive or the game's memory: the pictures only exist in
video memory. A pack comes in one of two forms, and several packs can be enabled at once.

The replacement is found by the **hash** of the game's own texture: the SDK `Hash` of the bytes of its level 0, as 16 hexadecimal digits.
A replacement must have the same proportion as the game's texture. The runtime reads the pictures on threads of its own, so the game never waits
for the disk: a texture is drawn as the game made it until its replacement has been read, which takes a few frames.
Both renderers, Direct3D 12 and Vulkan, replace textures.

### A pack in one file (`.ostp`)

The mod's package holds a single `pack.ostp`, and its manifest has `"schema": 3` and an `ostp` block with the file's `path`, `size` and `sha256`
(it holds no `files` or `patches`). The file's format is specified in [TEXTURE_PACK_FORMAT.md](TEXTURE_PACK_FORMAT.md): a header, the pack's information
(id, name, version, authors, summary, description, credits, license, website), an optional logo of up to 256 x 256 pixels, an index sorted by hash,
and the BC1 or BC3 blocks of every texture with all their levels.

On the Mods page, **Install from file** takes a `.ostp` directly. The launcher checks the file's structure and its SHA-256, copies it once, and
only then replaces an installed version, which it asks about first: it shows the installed and the new version, and says so when they are the same.
The page shows the mod's logo, and **Details** opens its description, credits, website and license.

- **Quality.** For a pack file, the Mods page offers High (every level of each texture) or Low (each texture at twice the game's size,
  a quarter of the video memory and of the reading). It applies the next time the game starts. The choice is kept by the mod's id in `settings.toml`
  beside the mod registry, so it survives updating or reinstalling the mod. A pack may have pictures up to 8192 pixels on a side (8K); those
  need a graphics card with a lot of video memory (an 8K texture takes about 85 MB), and on a card of 4 GB, Low is the setting to use. Sizes beyond 4K have not been measured in the game.
- **Order.** When several texture mods are on, the first in the order wins a texture that more than one has. The user sets the order with Up and
  Down on the Mods page (kept by id in `settings.toml`; without a choice, the order they were installed in), and the page lists how many textures each
  pair has in common.
- **Video memory.** The pictures a pack puts in video memory are kept within a budget, and the ones not drawn for the longest time go first, never
  one drawn in the last two seconds. The budget is 60% of what the driver says the game may use (`VK_EXT_memory_budget` with Vulkan, the adapter's
  video memory budget with Direct3D 12, both of which leave out other programs), or 40% of the largest local heap (of the adapter's own memory with
  Direct3D 12) without it. `OPENSHAO_HD_VRAM_MB` sets it by hand. A picture that is taken out is read again when the game draws it.
- The pack file can list the textures to have ready before the game draws them (`preload`): those of the menus, which keeps them from changing a
  few frames after they appear, and the first picture the game shows, which it draws for only a few frames before it stops to load.

The OpenShao ModKit (the OpenShao-ModKit repository) extracts the game's textures from the player's own disc image and
compiles an edited folder into a `.ostp`, with a window, a command line and a Python package.

### A pack as a folder (`hd/`)

A mod with an `hd/` folder, listed in `features` as `hdtextures`. `hd/index.txt` has one line per replaced texture: the hash, the width, the height,
the number of levels and the file, separated by spaces. Lines starting with `#` are comments.

```
# hash width height levels file
0bac2904604d3ecc 2048 2048 10 tex/0bac2904604d3ecc.dds
```

- The **file** is a DDS with the `DXT5` format and every level, from the largest down, each a multiple of 4 in size.
- The pack's textures keep their own size and levels. A file that is missing or does not match its line is not used, and the game's texture stays.

`hd/preload.txt` is optional. It lists, one hash per line, the replacements to have ready before the game draws them: those of the menus and of the
first picture the game shows. Lines starting with `#` are comments, and a hash that has no line in `index.txt` is ignored. Keep it to those textures:
each one takes video memory from the start.

### Developer mode

To try textures without building a pack, turn on **Developer** on the launcher's Files page. A Developer page appears, where you choose a folder of
pictures: PNG files named `<hash>_<format>_<width>x<height>.png`, as the texture tool's extraction writes them, with its `manifest.json`. The
game reads that folder when it starts, before every texture mod. Only a picture whose pixels differ from the signature the manifest holds for it
counts, so a single changed pixel is an edit and a picture saved again with the same pixels is not; nothing is compressed in this mode. The game's log
says how many pictures it found and how many it uses. Restart the game to see a change.

To try a folder of `.dds` files and an `index.txt` the same way, set `OPENSHAO_HD_PACK_DIR` to the folder: it is read before the developer folder
and the packs of the enabled mods.

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

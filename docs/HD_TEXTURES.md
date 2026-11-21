# HD texture replacement (Direct3D 12)

Copyright (c) 2026 refresh50. Original design and implementation of the Direct3D 12 texture dump and replacement,
and of the Xbox-based 4x HD texture pack. Contributed to OpenShao under the project's license (GPL-2.0).

The game renderer can draw high-resolution textures from a folder on the host instead of the original ones.
The game's memory and files are not changed, so the 64 MB memory limit and the volume space do not apply.
The feature is off by default and only runs when its environment variable points to a folder.

## Texture key

Each texture is identified by a file name shared by the dump and the replacement:

```
<CRC>_<FMT>_<W>x<H>.dds
```

- `CRC`: CRC-32 (IEEE, the same as zlib's `crc32`) of the level 0 bytes exactly as the game submits them,
  before any unswizzle or format conversion, without the other mip levels. Eight uppercase hex digits.
- `FMT`: the original Xbox texture format, two uppercase hex digits.
- `W`, `H`: the original size of level 0, in pixels.

The format and size are part of the key, so textures with identical bytes but a different format or size do not
collide.

## Dumping the game's textures

Set `OPENSHAO_TEXDUMP=<file>` and play. Each distinct texture the game draws adds one tab-separated line to the file:
frame, guest address, format, width, height, levels, level 0 size in bytes and CRC-32. The CRC is taken over the
swizzled guest bytes, so tools can find the same texture in the game's XPR files and export it under its key.
A pack author edits or upscales those images (any method) and saves them as DDS with the same name.

## Using a pack

Set `OPENSHAO_TEXREPLACE=<folder>` to a folder of DDS files named by key. When a texture the game sends matches a
file, the renderer uploads the DDS (any size, with its own mip chain) to the GPU and draws it instead of the original.
Supported DDS formats: DXT1, DXT5 and BGRA8. Files that fail to load are ignored and the original is drawn.

## Building

The renderer lives in the pinned XboxXBERecomp SDK. Apply the patch to the SDK checkout before building:

```
git -C third_party/XboxXBERecomp apply ../../patches/xboxxberecomp-d3d12-texreplace.patch
```

## Credits

- Direct3D 12 texture dump and replacement, key format and HD pack: refresh50

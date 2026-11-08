# Texture pack file format (OSTP, version 1.1)

A texture pack file holds the replacements for game textures in a single file. Its extension is `.ostp`. The game reads it directly: the BC blocks in
the file are the bytes the graphics card uses, so nothing is decoded or converted when the game runs.
[MODS.md](MODS.md) explains how a mod uses it.

- All integers are little-endian and unsigned. Sizes and offsets are 64 bits wide.
- Every section starts on a multiple of 16 bytes (the size of a BC3 block).
- The reader trusts nothing before it is checked (section 8).

## 1. Layout

```
+------------------------+  offset 0
| Header (128 bytes)     |
+------------------------+
| Metadata (JSON, UTF-8) |
+------------------------+
| Logo (RGBA, optional)  |
+------------------------+
| Index (N x 40 bytes)   |
+------------------------+
| Data (BC blocks)       |
+------------------------+  end = file_size
```

The sections follow in this order, do not overlap, and are padded with zeros.

## 2. Header

| Offset | Type | Field | Rule |
| --- | --- | --- | --- |
| 0 | u8[8] | `magic` | `"OSTPACK\0"` |
| 8 | u16 | `version_major` | The reader accepts only the major version it knows |
| 10 | u16 | `version_minor` | Compatible additions within a major version |
| 12 | u32 | `header_size` | 128 |
| 16 | u64 | `file_size` | The real size of the file |
| 24 | u64 | `meta_offset` | |
| 32 | u64 | `meta_size` | 1 byte to 1 MiB |
| 40 | u64 | `logo_offset` | 0 when there is no logo |
| 48 | u64 | `logo_size` | 0 when there is no logo, else `4 + width * height * 4`, at most 262,148 |
| 56 | u64 | `index_offset` | |
| 64 | u64 | `index_size` | Exactly `texture_count * 40` |
| 72 | u64 | `data_offset` | |
| 80 | u64 | `data_size` | |
| 88 | u32 | `texture_count` | 0 to 100,000 |
| 92 | u32 | `flags` | Reserved, 0 |
| 96 | u8[32] | `index_sha256` | SHA-256 of the index bytes |

There is no header checksum: the cross checks of section 8 and the SHA-256 of the index catch accidental damage, and installing checks the SHA-256 of the whole file.

## 3. Metadata

A JSON object.

- **Required:** `id`, `name`, `version`, `authors` (a list of text), `summary`, `min_recomp`, `features` (it must include `"hdtextures"`), `type` (`"texture"`).
- **Optional:** `description`, `website`, `license`, `credits`, and `preload`: a list of up to 4,096 hashes of 16 hexadecimal digits, the textures to have ready before the game draws them, those of the menus and of the first picture the game shows (version 1.1).
  The reader keeps only the ones the pack has, without repeats, in the order given. A value that is not a hash makes the file invalid.
- **Written by the tool that makes the file:** `texture_count`, `max_scale` (the largest factor over the original, an integer rounded up), `formats` (for example `["BC1","BC3"]`) and `format_version`.

Numbers in the metadata are integers: the project's JSON reader does not accept fractions. `id` matches `[A-Za-z0-9_-]{1,80}`, the rule of the mod system. `version` and `min_recomp` are three numbers separated by dots.
Unknown fields are ignored. `texture_count` must equal the header's. Limits: `name` 64 characters, `summary` 200, `description` 4,000, `authors` up to 16 items of up to 64 characters, `website` 256.

## 4. Logo

Optional, stored already decoded so that the launcher needs no image decoder:

| Offset | Type | Field | Rule |
| --- | --- | --- | --- |
| 0 | u16 | `width` | 1 to 256 |
| 2 | u16 | `height` | 1 to 256 |
| 4 | u8[`width * height * 4`] | pixels | RGBA, 8 bits a channel, top row first, alpha not premultiplied |

An invalid logo is dropped and the pack stays valid; the launcher shows its own mark.

## 5. Index

`texture_count` entries of 40 bytes, sorted by `hash` in increasing order, with no repeated hash.

| Offset | Type | Field | Rule |
| --- | --- | --- | --- |
| 0 | u64 | `hash` | The 64-bit SDK hash of the bytes of level 0 of the game's own texture |
| 8 | u16 | `width` | Width of the stored level 0, a multiple of 4, 4 to 8192 |
| 10 | u16 | `height` | Height of the stored level 0, a multiple of 4, 4 to 8192 |
| 12 | u16 | `orig_width` | Width of the game's own texture, 1 or more |
| 14 | u16 | `orig_height` | Height of the game's own texture, 1 or more |
| 16 | u8 | `levels` | Stored levels, 1 to 14, at most `floor(log2(max(width,height))) + 1` |
| 17 | u8 | `format` | 1 = BC1 (8 bytes a block), 2 = BC3 (16 bytes a block); anything else is refused |
| 18 | u8 | `flags` | Bit 0: the texture uses alpha; the other bits are 0 |
| 19 | u8 | `compression` | 0, no compression; anything else is refused (reserved) |
| 20 | u32 | reserved | 0 |
| 24 | u64 | `data_offset` | From the start of the data section, a multiple of 16 |
| 32 | u64 | `data_size` | The sum of the sizes of the levels (section 6) |

Proportion: `width * orig_height == height * orig_width`.

## 6. Data

For each texture, the levels one after the other, **largest first**, each a grid of BC blocks in rows with no padding between rows or levels. The size of level `l`, with `w_l = max(1, width >> l)` and `h_l = max(1, height >> l)`:

```
bytes(l) = ceil(w_l / 4) * ceil(h_l / 4) * bytes_per_block
```

A texture's `data_size` is the sum over its levels. The padding to the next multiple of 16 between textures is zero.

### Medium quality: skipping levels

Medium asks for twice the game's size. For each texture the game skips levels with:

```
skip = 0
while (width >> skip) > 2 * orig_width  and  (skip + 1) < levels:  skip += 1
```

The first level used starts at `data_offset` plus the sizes of the skipped levels. A texture that is not larger than twice the original, or that has one level, skips nothing.

## 7. Versions

`version_major` changes when something incompatible changes, and the reader refuses a major version it does not know. `version_minor` rises for compatible additions, and unknown metadata is ignored.
This is version 1.1: it adds the optional `preload` field, and a 1.0 pack stays valid without it.

## 8. Validation

The reader does these checks in order. Any failure refuses the file and none of it is used, except the logo, which is only dropped.

1. The file has at least 128 bytes, `magic` matches, `version_major` is supported, `header_size` is 128 and `flags` is 0.
2. `file_size` equals the real size.
3. Each section (metadata, logo if present, index, data) starts on a multiple of 16, has `offset + size` without 64-bit overflow and inside `file_size`, and does not overlap another.
4. Limits: `meta_size` at most 1 MiB; `texture_count` at most 100,000; `index_size == texture_count * 40`; `logo_size` at most 262,148; `data_size` above 0 when there are textures.
5. The SHA-256 of the index equals the header's.
6. Metadata: valid JSON, the required fields with the right types and the rules of section 3. Logo: `width` and `height` from 1 to 256 and `logo_size == 4 + width * height * 4`; if not, the logo is dropped.
7. Each index entry follows the table of section 5: sizes, levels, format, flags, `compression` 0, reserved fields 0, proportion, `hash` strictly increasing, `data_offset` a multiple of 16, `data_offset + data_size` inside the
   data section, `data_size` equal to the computation, and no overlap between textures.
8. When installing, once: the SHA-256 of the whole file equals the one the source gives, when it gives one.

The game reads the largest 2D image size of the graphics card. A texture past it skips levels (the same rule as Medium) until it fits, and if it still does not fit the game's own texture is used.
Sums and products of sizes are checked for overflow, the reader never reads past `file_size`, and a short read makes the file invalid. At run time, a texture whose data cannot be read stays the game's own.

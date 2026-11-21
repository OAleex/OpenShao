"""Emit portable render-state hints from local D3D12 and Vulkan recipe journals."""
import argparse
from collections import defaultdict
from pathlib import Path
import struct


MASK = (1 << 64) - 1
BASIS = 1469598103934665603
K0 = 0x9E3779B97F4A7C15
K1 = 0xE7037ED1A0B428DB


def mix(left, right):
    value = left * right
    return (value & MASK) ^ (value >> 64)


def gpu_hash(data, seed=BASIS):
    size = len(data)
    full = size & ~15
    seed ^= K0
    for left, right in struct.iter_unpack("<QQ", data[:full]):
        seed = mix(left ^ K1, right ^ seed)
    if full < size:
        tail = data[full:]
        left = int.from_bytes(tail[:8], "little")
        right = int.from_bytes(tail[-8:], "little") if len(tail) >= 8 else 0
        seed = mix(left ^ K1, right ^ seed ^ len(tail))
    return mix(seed ^ K0, size ^ K1)


def journal(path):
    data = path.read_bytes()
    magic, layout, version, recipe_size = struct.unpack_from("<QQII", data)
    if (magic, version, recipe_size) != (0x3145504950534F4F, 1, 736):
        raise ValueError(f"Unsupported recipe journal {path}")
    position = 24
    recipes = {}
    rooms = defaultdict(set)
    while position < len(data):
        kind, size, checksum = struct.unpack_from("<IIQ", data, position)
        position += 16
        payload = data[position:position + size]
        position += size
        actual = kind
        for byte in payload:
            actual = ((actual ^ byte) * 1099511628211) & MASK
        if len(payload) != size or actual != checksum:
            raise ValueError(f"Invalid recipe journal record in {path}")
        if kind == 1:
            if size != recipe_size:
                raise ValueError(f"Invalid recipe size in {path}")
            key_size, = struct.unpack_from("<I", payload, 408)
            if key_size != (layout & 0xFFFF) or not 0 < key_size <= 320 or key_size % 4:
                raise ValueError(f"Invalid key size in {path}")
            key = payload[416:416 + key_size]
            recipes[gpu_hash(key)] = (payload, key)
        elif kind == 2:
            key, room, _ = struct.unpack("<QII", payload)
            if room < 256:
                rooms[key].add(room)
        elif kind != 3:
            raise ValueError(f"Unknown recipe record {kind} in {path}")
    # Program records are deliberately not read into the output representation.
    return layout, [(recipes[key], ids) for key, ids in sorted(rooms.items())]


def scalar(value):
    return str(value) if value < 10 else f"0x{value:x}"


def fields(names, values):
    return ", ".join(f".{name} = {scalar(value)}" for name, value in zip(names, values) if value)


def array(values):
    values = list(values)
    while values and not values[-1]:
        values.pop()
    return "{" + ", ".join(map(scalar, values)) + "}"


def entry(recipe, key, rooms):
    mask = [0] * 8
    for room in rooms:
        mask[room // 32] |= 1 << (room % 32)
    vs, ps, program, decode, regs, pretransformed, quad, sprite = struct.unpack_from("<QQQQHBBI", recipe)
    vertex = fields(("program", "decode", "regs", "pretransformed", "quad", "sprite"),
                    (program, decode, regs, pretransformed, quad, sprite))
    light, types, *fixed_textures = struct.unpack_from("<6I", recipe, 40)
    if light or types or any(fixed_textures):
        fixed = fields(("light", "types"), (light, types))
        if any(fixed_textures):
            fixed = ", ".join(filter(None, (fixed, f".tex = {array(fixed_textures)}")))
        vertex = ", ".join(filter(None, (vertex, ".fixed = {" + fixed + "}")))
    pixel = recipe[64:408]
    programmable, = struct.unpack_from("<I", pixel)
    pixel_program = ((gpu_hash(pixel[148:160], gpu_hash(pixel[4:144])) | 1) if programmable else 0)
    tex_modes, = struct.unpack_from("<I", pixel, 144)
    pixel_fields = [fields(("programmable", "tex_modes"), (programmable, tex_modes))]
    fixed_stages = [array(struct.unpack_from("<9I", pixel, 160 + 36 * n)) for n in range(4)]
    if any(stage != "{}" for stage in fixed_stages):
        pixel_fields.append(".ff = {" + ", ".join(fixed_stages) + "}")
    bindings = [fields(("kind", "rect", "shadow", "flags"), values)
                for values in struct.iter_unpack("<4B", pixel[304:320])]
    if any(bindings):
        pixel_fields.append(".tex = {" + ", ".join("{" + v + "}" for v in bindings) + "}")
    pixel_fields.append(fields(("alphakill", "flat", "two_sided", "proj", "quad", "alphatest"),
                               struct.unpack_from("<6I", pixel, 320)))
    words = [v for v, in struct.iter_unpack("<I", key)]
    key_rows = ",\n            ".join(", ".join(map(scalar, words[i:i+12])) for i in range(0, len(words), 12))
    return (
        "    {\n"
        f"        .rooms = {array(mask)},\n"
        f"        .pixel_program = 0x{pixel_program:016x}ull,\n"
        f"        .vs_source = 0x{vs:016x}ull, .ps_source = 0x{ps:016x}ull,\n"
        "        .vertex = {" + vertex + "},\n"
        "        .pixel = {" + ", ".join(filter(None, pixel_fields)) + "},\n"
        f"        .key_size = {len(key)},\n"
        "        .key = {\n            " + key_rows + "\n        },\n"
        "    },\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--d3d12", type=Path, required=True)
    parser.add_argument("--vulkan", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = [
        "// Generated render-state hints. Original shader programs come from the installed game.\n",
        "// Regenerate with tools/generate_pipeline_catalog.py.\n\n",
        "static_assert(sizeof(gpu::PixelKey) == 344);\n\n",
    ]
    for name, path in (("D3D12", args.d3d12), ("Vulkan", args.vulkan)):
        layout, recipes = journal(path)
        if name == "D3D12":
            result.append("#ifdef _WIN32\n")
        result.append(f"constexpr uint64_t k{name}CatalogLayout = 0x{layout:016x}ull;\n")
        result.append(f"constexpr CatalogEntry k{name}Catalog[] = {{\n")
        result.extend(entry(recipe, key, ids) for (recipe, key), ids in recipes)
        result.append("};\n")
        if name == "D3D12":
            result.append("#endif\n")
        result.append("\n")
    args.output.write_text("".join(result), encoding="utf8", newline="\n")

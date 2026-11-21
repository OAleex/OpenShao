"""The backported XboxXBERecomp renderer fixes are present in the adapted sources and in the Vulkan backend."""
import argparse
import sys
from pathlib import Path

ADAPTED = {
    "d3d8.cpp": (
        ("af7d5ab SetGammaRamp lock",
         "    std::lock_guard<std::recursive_mutex> lock(g_lock);\n    if (g_backend) g_backend->SetGamma(ramp);"),
        ("9e96fe5 null viewport", "    if (v) g_state.viewport = {"),
    ),
    "d3d12.cpp": (
        ("655fdcd LOD bias range", "std::clamp(Bits(t[kTssMipLodBias]), D3D12_MIP_LOD_BIAS_MIN, D3D12_MIP_LOD_BIAS_MAX)"),
        ("6c63e71 texgen wrap", "if (fixed && ((t[kTssTexCoordIndex] >> 16 & 7) || (t[kTssTextureTransformFlags] & 7))) clamp = false;"),
        ("6c63e71 fixed function stages", "UnitTexcoords(a, stages), fixed);"),
        ("b69b4bb position register", "for (int r = kPositionRegister + 1; r < 16; r++) {"),
        ("e867755 flush reports", "void D3D12Backend::Flush() {\n    Submit(true);\n    DeliverReports();\n}"),
    ),
    "shaders.cpp": (
        ("d834100 combiner clamp", "saturate(r0.rgb), fogin.a), saturate(r0.a))"),
        ("a50a366 screen space w", "    p.w = p.w == 0 ? 1 : magclamp(p.w);"),
    ),
}

VULKAN = (
    ("6c63e71 texgen wrap", "!(fixed && ((t[kTssTexCoordIndex] >> 16 & 7) || (t[kTssTextureTransformFlags] & 7)))"),
    ("6c63e71 fixed function stages", "EnsureStageSamplers(s, stages, UnitTexcoords(a, stages), fixed, b.sampler);"),
    ("b69b4bb position register", "for (int r = kPositionRegister + 1; r < 16; r++) {"),
    ("e867755 flush reports", "        Submit(true);\n        DeliverReports();\n"),
    ("LOD bias range", "std::clamp(Bits(t[kTssMipLodBias]), -max_bias, max_bias)"),
)


def read(path):
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--adapted", required=True, type=Path)
    parser.add_argument("--vulkan", required=True, type=Path)
    args = parser.parse_args()
    missing = []
    for name, checks in ADAPTED.items():
        text = read(args.adapted / name)
        missing += [f"{name}: {label}" for label, needle in checks if text.count(needle) != 1]
    text = read(args.vulkan)
    missing += [f"vk_backend.cpp: {label}" for label, needle in VULKAN if text.count(needle) != 1]
    for line in missing:
        print("missing", line)
    print("upstream fixes:", "missing" if missing else "all present")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())

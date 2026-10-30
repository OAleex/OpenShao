"""Run the renderers' real pipeline adoption and first-use branches without a GPU."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
OPTIONS = argparse.Namespace(compiler=None, sdk=None)


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


COMMON = r'''
#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
struct ID3DBlob { void* GetBufferPointer() { return nullptr; } size_t GetBufferSize() { return 0; } };
struct ID3D12PipelineState {};
template<class T> struct ComPtr {
    T* ptr = nullptr;
    ComPtr() = default;
    ComPtr(T* value) : ptr(value) {}
    T* Get() const { return ptr; }
    explicit operator bool() const { return ptr != nullptr; }
    bool operator!=(std::nullptr_t) const { return ptr != nullptr; }
};
using VkPipeline = ID3D12PipelineState*;
using VkShaderModule = ID3DBlob*;
#define VK_NULL_HANDLE nullptr
#define IID_PPV_ARGS(value) value
#define FAILED(value) ((value) != 0)
#define LOGE(...) ((void)0)
struct PsoKey { uint64_t id; uint32_t topo = 0, atlas_guard = 0; };
uint64_t Hash(const void* value, size_t) { return static_cast<const PsoKey*>(value)->id; }
enum class Topology { Points = 1 };
struct D3D12_INPUT_ELEMENT_DESC {};
struct D3D12_GRAPHICS_PIPELINE_STATE_DESC {
    struct { D3D12_INPUT_ELEMENT_DESC* p = nullptr; uint32_t n = 0; } InputLayout;
    struct { void* p = nullptr; size_t n = 0; } GS;
};
struct State {};
struct Assembled {};
struct PixelKey {};
struct DrawShaders { ID3DBlob* vs_code = nullptr; ID3DBlob* ps_code = nullptr; };
struct { unsigned psos = 0; } g_perf;
std::vector<bool> observations;
namespace vkshader { enum class Stage { Geometry }; }
namespace openshao {
std::string AtlasBoundsHlsl() { return {}; }
namespace pipelines {
struct InFlight {};
struct InFlightScope { InFlightScope(InFlight&, uint64_t) {} };
void ObserveAssetPipeline(uint64_t, bool compiled_on_draw) { observations.push_back(compiled_on_draw); }
}}
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
'''


class PipelineFirstUseTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="openshao-first-use-")
        cls.folder = Path(cls.temporary.name)
        cls.env = os.environ.copy()
        cls.compiler = OPTIONS.compiler or os.environ.get("CXX") or shutil.which("clang++")
        if not cls.compiler:
            raise RuntimeError("A C++20 compiler is required for pipeline first-use tests")
        sdk = OPTIONS.sdk or Path(os.environ.get("OPENSHAO_RECOMP_SDK", ROOT / "third_party/XboxXBERecomp"))
        subprocess.run([sys.executable, str(ROOT / "tools/prepare_recomp_runtime.py"),
                        "--sdk", str(sdk), "--out", str(cls.folder / "adapted")], check=True)
        cls.d3d12 = (cls.folder / "adapted/d3d12.cpp").read_text(encoding="utf-8")
        cls.vulkan = (ROOT / "src/linux/vk/vk_backend.cpp").read_text(encoding="utf-8")

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def run_renderer(self, source, backend, vulkan=False):
        # Device creation and shader compilation are the external boundary. The
        # cache/adoption branch and the draw's observation branch are real code.
        import re
        flags = "\n".join(re.findall(r"    bool pso_\w+_ = false;[^\n]*", source))
        pipeline_type = "VkPipeline" if vulkan else "ID3D12PipelineState*"
        module_type = "VkShaderModule" if vulkan else "ID3DBlob*"
        ensure = function(source, f"{pipeline_type} {backend}::EnsurePipeline(")
        draw_start = source.index(f"    {pipeline_type} pso = EnsurePipeline(k, sh.vs_code, sh.ps_code);")
        draw_end = source.index("    GuestExtent size;" if vulkan else "    cl_->SetPipelineState(pso);", draw_start)
        draw = source[draw_start:draw_end]
        declarations = f'''
struct {backend} {{
    std::unordered_map<uint64_t, {'VkPipeline' if vulkan else 'ComPtr<ID3D12PipelineState>'}> psos_, pre_psos_;
    std::unordered_set<uint64_t> pre_done_;
    std::mutex pre_mutex_;
    openshao::pipelines::InFlight pre_pso_flight_;
{flags}
    bool fail_create = false, gs_ok_ = false;
    unsigned records = 0;
    ID3D12PipelineState pipeline;
    struct Disk {{
        {backend}* owner;
        int CreatePipeline(D3D12_GRAPHICS_PIPELINE_STATE_DESC*, ComPtr<ID3D12PipelineState>* out) {{
            if (owner->fail_create) return 1;
            *out = &owner->pipeline; return 0;
        }}
    }} disk_cache_{{this}};
    {pipeline_type} EnsurePipeline(const PsoKey&, {module_type}, {module_type});
    uint32_t FillInputLayout(const PsoKey&, D3D12_INPUT_ELEMENT_DESC*) {{ return 0; }}
    void FillPipelineDesc(const PsoKey&, ID3DBlob*, ID3DBlob*, D3D12_GRAPHICS_PIPELINE_STATE_DESC&) {{}}
    void ExpandPoints(D3D12_GRAPHICS_PIPELINE_STATE_DESC&) {{}}
    ID3DBlob* Compile(const std::string&, const char*, const char*, uint64_t&) {{ return nullptr; }}
    VkShaderModule Compile(const std::string&, const char*, vkshader::Stage, uint64_t&) {{ return nullptr; }}
    std::string PointHlsl() {{ return {{}}; }}
    VkPipeline MakePipeline(const PsoKey&, VkShaderModule, VkShaderModule, VkShaderModule, VkShaderModule) {{
        return fail_create ? nullptr : &pipeline;
    }}
    ComPtr<ID3D12PipelineState> AdoptPipeline(uint64_t hash) {{
        auto it = pre_psos_.find(hash);
        if (it == pre_psos_.end()) return nullptr;
        ComPtr<ID3D12PipelineState> pso = it->second;
        pre_psos_.erase(it); return pso;
    }}
    void PublishPipeline(uint64_t hash) {{ pre_done_.insert(hash); }}
    void RecordPipeline(const State&, const Assembled&, const DrawShaders&, const PixelKey&, const PsoKey&) {{ ++records; }}
    void Draw(const PsoKey& k) {{
        State s; Assembled a; DrawShaders sh; PixelKey pk;
{draw}
    }}
}};
'''
        main = f'''
int main() {{
    try {{
        {backend} renderer;
        renderer.pre_psos_[1] = &renderer.pipeline;
        renderer.Draw({{1}});
        require(renderer.records == 1, "adopted pipeline must persist on first successful use");
        require(observations == std::vector<bool>{{false}}, "adopted pipeline must be observed as ready");
        renderer.Draw({{1}});
        require(renderer.records == 1 && observations.size() == 1, "cached draws must not record or observe again");
        renderer.Draw({{2}});
        require(renderer.records == 2, "draw-created pipeline must persist on first use");
        require(observations == std::vector<bool>{{false, true}}, "draw creation must be reported separately");
        renderer.Draw({{2}});
        require(renderer.records == 2 && observations.size() == 2, "created pipeline cache hit must stay quiet");
        renderer.fail_create = true;
        renderer.Draw({{3}});
        require(renderer.records == 2 && observations.size() == 2, "failed pipeline must not count as successful use");
        renderer.Draw({{3}});
        require(renderer.records == 2 && observations.size() == 2, "cached failure must stay quiet");
        renderer.pre_psos_[4] = &renderer.pipeline;
        renderer.Draw({{4}});
        require(renderer.records == 3, "adoption after a failure must still persist");
        require(observations == std::vector<bool>{{false, true, false}}, "creation state must not leak into later adoption");
        return 0;
    }} catch (const std::exception& error) {{ std::cerr << error.what() << '\\n'; return 1; }}
}}
'''
        cpp = self.folder / f"{backend}.cpp"
        executable = self.folder / (backend + (".exe" if os.name == "nt" else ""))
        cpp.write_text(COMMON + declarations + ensure + main, encoding="utf-8")
        compile_result = subprocess.run([self.compiler, "-std=c++20", "-O0", "-fuse-ld=lld", str(cpp),
                                         "-o", str(executable)], env=self.env, capture_output=True, text=True)
        self.assertEqual(compile_result.returncode, 0, compile_result.stdout + compile_result.stderr)
        result = subprocess.run([str(executable)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_d3d12_adopted_pipeline_is_recorded_only_on_first_successful_use(self):
        self.run_renderer(self.d3d12, "D3D12Backend")

    def test_vulkan_adopted_pipeline_is_recorded_only_on_first_successful_use(self):
        self.run_renderer(self.vulkan, "VulkanBackend", vulkan=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--compiler")
    parser.add_argument("--sdk", type=Path)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])

"""Exercise the SDK Present flow with deterministic time and a controlled device."""
import argparse
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
OPTIONS = argparse.Namespace(compiler=None, sdk=None)


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError("The SDK source changed at the performance adaptation")
    return source.replace(old, new, 1)


def extract(source, signature, ending):
    start = source.index(signature)
    return source[start:source.index(ending, start) + len(ending)]


FIXTURE = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
struct PerfClock {
    using duration = std::chrono::duration<double, std::milli>;
    using time_point = std::chrono::time_point<PerfClock, duration>;
    static double value;
    static time_point now() { return time_point(duration(value)); }
};
double PerfClock::value = 0;
double PerfMs(PerfClock::time_point start) { return (PerfClock::now() - start).count(); }
void Advance(double ms) { PerfClock::value += ms; }
struct Report { double present, swap; };
std::vector<Report> reports;
bool diagnostics = true;
namespace diag { bool Perf() { return diagnostics; } }
void LogPerf();
namespace app { struct Settings { bool vsync = false; }; Settings& Get() { static Settings s; return s; } }
struct Surface {};
struct Target {};
struct Resource { void* Get() { return nullptr; } };
constexpr int D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE = 1;
constexpr int DXGI_PRESENT_ALLOW_TEARING = 1;
std::atomic<bool> g_capture{false};
bool CaptureOn() { return true; }
struct Swap {
    uint32_t GetCurrentBackBufferIndex() { return 0; }
    void Present(int, int) { Advance(5); }
};
class D3D12Backend {
public:
    bool cap_ = false, ramp_dirty_ = false, tearing_ = true;
    Resource lut_;
    int lut_state_ = 0;
    Swap swap;
    Swap* swap_ = &swap;
    uint64_t presents_ = 0, frame_ = 0, last_presented_ = 0;
    std::atomic<uint64_t> presents_seen_{0};
    unsigned settings_calls = 0;
    void Present(const Surface&);
    void FinishCapture(const Surface&) { Advance(2); }
    void StartCapture() { Advance(2); }
    void ResizeSwap() {}
    Target* EnsureTarget(const Surface&, bool) { static Target t; return &t; }
    bool LatchCpuWrite(Target&) { return false; }
    void UploadTarget(Target&, const Surface&) {}
    void Resolve(Target&) {}
    void UploadGammaRamp() {}
    void Transition(void*, int, int) {}
    void DrawFrame(Target*, uint32_t) { Advance(3); }
    void Submit(bool) {}
    void DeliverReports() {}
    void ReleaseOldTextures() {}
    void ResetFrame() { Advance(2); }
    void CountFps() {}
    void ApplySettings() { ++settings_calls; Advance(1); }
};
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
'''


class PresentPerfTest(unittest.TestCase):
    def source(self):
        compiler = OPTIONS.compiler or os.environ.get("CXX") or shutil.which("clang++")
        self.assertIsNotNone(compiler, "A C++20 compiler is required")
        sdk = OPTIONS.sdk or Path(os.environ.get("OPENSHAO_RECOMP_SDK", ROOT / "third_party/XboxXBERecomp"))
        source = (sdk / "runtime/gpu/d3d12.cpp").read_text(encoding="utf-8")
        # The unadapted SDK is the regression fixture before the new adapter exists.
        adapter = ROOT / "tools/prepare_perf_present.py"
        if adapter.exists():
            spec = importlib.util.spec_from_file_location("prepare_perf_present", adapter)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            source = module.adapt_backend(source, replace_once)
        return compiler, source

    def run_flow(self, compiler, source, reporting, assertions):
        counters = extract(source, "struct Perf {", "\n};") + "\nPerf g_perf;\n"
        scope = extract(source, "struct PerfScope {", "\n};")
        intervals = (extract(source, "void SamplePerfFrameInterval(", "\n}\n")
                     if "void SamplePerfFrameInterval(" in source else "")
        present = extract(source, "void D3D12Backend::Present(", "\n}\n")
        with tempfile.TemporaryDirectory(prefix="openshao-present-perf-") as folder:
            cpp = Path(folder) / "present.cpp"
            executable = Path(folder) / ("present.exe" if os.name == "nt" else "present")
            cpp.write_text(FIXTURE + counters + scope + intervals + reporting + present + assertions, encoding="utf-8")
            compilation = subprocess.run([compiler, "-std=c++20", "-fuse-ld=lld", str(cpp), "-o", str(executable)],
                                         capture_output=True, text=True)
            self.assertEqual(compilation.returncode, 0, compilation.stdout + compilation.stderr)
            execution = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(execution.returncode, 0, execution.stdout + execution.stderr)

    def test_completed_present_is_reported_in_its_own_interval(self):
        compiler, source = self.source()
        reporting = r'''
void LogPerf() {
    reports.push_back({g_perf.present, g_perf.swap});
    g_perf = {};
    Advance(100);  // Reporting itself must never become next interval's present.
}
'''
        assertions = r'''
int main() {
    try {
        D3D12Backend backend;
        backend.Present({});
        require(reports.size() == 1 && reports[0].present == 11,
                "report must include this frame's completed present scope");
        require(reports[0].swap == 5, "report must include this frame's swap duration");
        require(g_perf.present == 0 && g_perf.swap == 0,
                "completed present must not leak into the next interval");
        backend.cap_ = true;
        backend.Present({});
        require(reports.size() == 2 && reports[1].present == 13 && reports[1].swap == 5,
                "capture work must be counted in the same present interval");
        backend.cap_ = false;
        diagnostics = false;
        backend.Present({});
        require(reports.size() == 2 && g_perf.present == 11 && g_perf.swap == 5,
                "disabled diagnostics must retain completed timing totals");
        diagnostics = true;
        backend.Present({});
        require(reports.size() == 3 && reports[2].present == 22 && reports[2].swap == 10,
                "report boundary must include all completed frames exactly once");
        require(backend.presents_ == 4 && backend.settings_calls == 4,
                "timing adaptation must preserve present and settings execution");
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
'''
        self.run_flow(compiler, source, reporting, assertions)

    def test_frame_interval_extremes_follow_existing_report_windows(self):
        compiler, source = self.source()
        reporting = r'''
std::atomic<uint64_t> g_perf_capture_ns{0}, g_perf_wait_ns{0}, g_perf_waits{0};
std::array<std::atomic<uint64_t>, 6> g_perf_wait_kind_ns{};
std::atomic<uint64_t> g_perf_sync_block{0}, g_perf_sync_late{0};
struct IntervalReport { double maximum; uint64_t over25, over50, samples; };
std::vector<IntervalReport> intervals;
void LogInfo(const char* format, ...) {
    if (std::strncmp(format, "perf: frame intervals", 21) != 0) return;
    va_list args;
    va_start(args, format);
    const double maximum = va_arg(args, double);
    const auto over25 = va_arg(args, unsigned long long);
    const auto over50 = va_arg(args, unsigned long long);
    const auto samples = va_arg(args, unsigned long long);
    va_end(args);
    intervals.push_back({maximum, over25, over50, samples});
}
#define LOGI(...) LogInfo(__VA_ARGS__)
''' + extract(source, "void LogPerf() {", "\n}\n")
        assertions = r'''
int main() {
    try {
        D3D12Backend backend;
        auto present_at = [&](double completed) { PerfClock::value = completed - 11; backend.Present({}); };
        present_at(11);
        present_at(36);  // exactly 25 ms is not above 25
        present_at(86);  // exactly 50 ms is above 25 only
        present_at(137);
        require(intervals.empty(), "interval diagnostics must use the existing report cadence");
        present_at(1111);
        require(intervals.size() == 1, "the existing performance report must include frame interval diagnostics");
        require(intervals[0].maximum == 974 && intervals[0].over25 == 3 &&
                intervals[0].over50 == 2 && intervals[0].samples == 4,
                "report must contain independently measured maximum and threshold counts");
        present_at(1131);
        present_at(1161);
        present_at(2111);
        require(intervals.size() == 2 && intervals[1].maximum == 950 && intervals[1].over25 == 2 &&
                intervals[1].over50 == 1 && intervals[1].samples == 3,
                "reported extrema and counts must reset at the existing interval boundary");
        diagnostics = false;
        present_at(2500);
        require(intervals.size() == 2, "disabled diagnostics must not report pacing samples");
        diagnostics = true;
        present_at(3111);
        require(intervals.size() == 3 && intervals[2].maximum == 0 && intervals[2].samples == 0 &&
                intervals[2].over25 == 0 && intervals[2].over50 == 0,
                "a disabled measurement gap must not become a false long frame");
        require(backend.presents_ == 10 && backend.settings_calls == 10,
                "interval measurement must preserve present flow");
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
'''
        self.run_flow(compiler, source, reporting, assertions)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--compiler")
    parser.add_argument("--sdk", type=Path)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[sys.argv[0], *remaining])

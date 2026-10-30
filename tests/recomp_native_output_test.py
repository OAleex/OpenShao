"""Exercise the real native output loop with a controlled clock and guest renderer."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
OPTIONS = argparse.Namespace(compiler=None, sdk=None)

FIXTURE = r'''
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
namespace fixture {
inline int64_t now=0;
struct Clock {
    using time_point=std::chrono::steady_clock::time_point;
    static time_point now(){return time_point(std::chrono::nanoseconds(fixture::now));}
};
inline std::array<uint8_t,8<<20> memory{};
inline unsigned logic=0,outputs=0;
inline bool corrupt=false;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
'''

BOUNDARY = r'''
namespace kernel {
uint8_t* g_base=fixture::memory.data();
uint32_t CallGuest(X86Context& ctx,uint32_t fn,const uint32_t*,size_t) {
    if(fn==0x114CD0)sub_00114CD0(ctx,g_base);
    else fixture::Check(fn==0x3C8DE0,"Only renderer submission may cross the guest boundary");
    return 0;
}
}
namespace mem {bool IsCommitted(uint32_t p){return p>=0x10000 && p<fixture::memory.size();}}
namespace d3d {
gpu::State& State(){static gpu::State s;return s;}
std::recursive_mutex& Lock(){static std::recursive_mutex m;return m;}
}
namespace app {Settings& Get(){static Settings s;return s;}}
namespace logx {void Write(Level,const char*,...) {}}
namespace openshao::frames {int Target(){return target_rate.load();}}

X86_FUNC(__imp__sub_00011069) {
    ++fixture::logic;
    kernel::W32(0x5AFC60,kernel::R32(0x5AFC60)+1);
    auto render=ctx;sub_00114CD0(render,base);
    ctx.eax=0;ctx.esp+=4;
}
X86_FUNC(__imp__sub_00114CD0) {
    if(extra){++fixture::outputs;if(fixture::corrupt)kernel::G<uint8_t>(0x5D1AB8)^=1;}
    ctx.eax=0;ctx.esp+=4;
}
'''

MAIN = r'''
int main() {
  try {
    using fixture::Check;
    openshao::frames::Configure(120,false);
    kernel::W32(0x510B74,0x700000);kernel::W32(0x700000,0x710000);
    kernel::W32(0x639750,0x720000);kernel::W32(0x639224,0x730000);
    kernel::W32(0x505970,0xC4);kernel::W32(0x4EF42C,1);
    const auto call=[] {X86Context ctx{};ctx.esp=0x1000;sub_00011069(ctx,kernel::g_base);};
    // A busy second has no spare output slots. The next midpoint must recover
    // immediately rather than running extra simulation or entering a long cap.
    for(unsigned i=0;i<=60;++i){fixture::now=(int64_t(i)*1000000000+59)/60;call();}
    const auto logic=fixture::logic;
    fixture::now=1008333333;call();
    Check(fixture::outputs==1,"A recovered render slot must not stay locked to 60 FPS after a busy second");
    Check(fixture::logic==logic,"An extra output frame must not advance the native gameplay callback");
    Check(kernel::R32(0x5AFC60)==logic,"Visual recovery must preserve the 60 Hz simulation counter");
    fixture::now=1016666667;call();
    fixture::corrupt=true;fixture::now=1025000000;call();
    Check(faulted,"An output pass that changes gameplay state must still disable interpolation");
    Check(kernel::G<uint8_t>(0x5D1AB8)==0,"The state guard must restore the original actor bytes");
    const auto outputs=fixture::outputs;
    fixture::now=1033333334;call();
    Check(fixture::outputs==outputs && app::Get().fps_cap==60,
          "A state fault must retain the safe 60 FPS fallback");
    std::cout<<"native output recovery and state protection: ok\n";
    return 0;
  } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
'''


class NativeOutputTest(unittest.TestCase):
    def test_busy_frame_recovery_keeps_simulation_and_fault_guard(self):
        source = (ROOT / "src/native_frames.cpp").read_text(encoding="utf-8")
        source = source.replace("using Clock=std::chrono::steady_clock;", "using Clock=fixture::Clock;")
        imports = re.findall(r"X86_EXTERN_FUNC\((\w+)\);", source)
        stubs = "\n".join(f"X86_FUNC({name}) {{ctx.eax=0;ctx.esp+=4;}}"
                          for name in imports if name not in {"__imp__sub_00011069", "__imp__sub_00114CD0"})
        env = os.environ.copy()
        if os.name == "nt":
            setup = ROOT / "build/vs_env.cmd"
            result = subprocess.run(["cmd.exe", "/d", "/c", "call", str(setup)],
                                    capture_output=True, text=True, check=True)
            for line in result.stdout.splitlines():
                if "=" in line and not line.startswith("="):
                    key, value = line.split("=", 1)
                    env[key] = value
        with tempfile.TemporaryDirectory(prefix="openshao-native-output-") as temporary:
            cpp = Path(temporary) / "output.cpp"
            exe = Path(temporary) / ("output.exe" if os.name == "nt" else "output")
            cpp.write_text(FIXTURE + source + BOUNDARY + stubs + MAIN, encoding="utf-8")
            result = subprocess.run([OPTIONS.compiler, "-std=c++20", "-O0", "-fuse-ld=lld",
                                     "-I" + str(ROOT / "src"), "-I" + str(OPTIONS.sdk / "runtime"),
                                     str(cpp), "-o", str(exe)], env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--sdk", required=True, type=Path)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[__file__, *remaining])

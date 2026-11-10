"""Exercise the real native output loop with a controlled clock and guest renderer."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
OPTIONS = argparse.Namespace(compiler=None, sdk=None, generated=None)

FIXTURE = r'''
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include "gpu/simple_dirty.h"
namespace fixture {
inline int64_t now=0;
struct Clock {
    using time_point=std::chrono::steady_clock::time_point;
    static time_point now(){return time_point(std::chrono::nanoseconds(fixture::now));}
};
inline std::array<uint8_t,8<<20> memory{};
inline unsigned logic=0,outputs=0;
inline unsigned presents=0;
inline bool corrupt=false,expect_menu=false;
inline uint32_t material_wrapper=0;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
}
'''

BOUNDARY = r'''
namespace kernel {
uint8_t* g_base=fixture::memory.data();
uint32_t CallGuest(X86Context& ctx,uint32_t fn,const uint32_t*,size_t) {
    if(fn==0x740100 && fixture::material_wrapper)return fixture::material_wrapper;
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
namespace openshao::dialogue {void AuditTick() {}}  // scene audio audit lives in dialogue_restore.cpp
namespace openshao::survival {void DrawRulesBackdrop(X86Context&) {}}

X86_FUNC(__imp__sub_00011069) {
    ++fixture::logic;
    kernel::W32(0x5AFC60,kernel::R32(0x5AFC60)+1);
    auto render=ctx;sub_00114CD0(render,base);
    ctx.eax=0;ctx.esp+=4;
}
X86_FUNC(__imp__sub_00114CD0) {
    ++fixture::presents;
    if(fixture::expect_menu)fixture::Check(app::Get().fps_cap==60,
        "Every menu presentation must use the 60 FPS cap, including with VSync");
    if(extra){++fixture::outputs;if(fixture::corrupt)kernel::G<uint8_t>(0x5D1AB8)^=1;}
    ctx.eax=0;ctx.esp+=4;
}
'''

MATERIAL_FLOW = r'''
namespace materials_fixture {
constexpr uint32_t Object=0x740000, Vtable=0x740200, Wrapper=0x740300, List=0x740400, Bank=0x740500;
constexpr uint32_t MaterialA=0x741000, MaterialB=0x742000;
constexpr std::array<uint32_t,4> Fields{0x28,0x34,0x38,0x40};
using Values=std::array<uint32_t,4>;
std::vector<Values> drawn;
bool added_pass=false;
Values Read(uint32_t material) {
    return {kernel::R32(material+0x28),kernel::R32(material+0x34),
            kernel::R32(material+0x38),kernel::R32(material+0x40)};
}
void Init() {
    fixture::material_wrapper=Wrapper;
    kernel::W32(Object,Vtable);kernel::W32(Vtable+8,0x740100);
    kernel::W32(Wrapper+4,List);kernel::W32(List,1);
    kernel::W32(List+8,3);kernel::W32(List+12,Bank);
    kernel::W32(Bank+12,MaterialA);kernel::W32(Bank+16,MaterialB);
    for(size_t i=0;i<Fields.size();++i) {
        kernel::W32(MaterialA+Fields[i],uint32_t((i+1)*10));
        kernel::W32(MaterialB+Fields[i],uint32_t((i+1)*100));
    }
    kernel::W32(0x510B74,0x700000);kernel::W32(0x700000,0x710000);
    kernel::W32(0x639750,0x720000);kernel::W32(0x639224,0x730000);
    kernel::W32(0x505970,73);
}
void Step(int64_t now) {
    fixture::now=now;
    X86Context ctx{};ctx.esp=0x1000;
    sub_00011069(ctx,kernel::g_base);
}
}
X86_FUNC(__imp__sub_001221C0) {
    using namespace materials_fixture;
    fixture::Check(ctx.ecx==Object,"The original material pass must receive its object");
    const auto material=kernel::R32(Bank+4*kernel::R32(List+8));
    for(const auto field:Fields)kernel::W32(material+field,kernel::R32(material+field)+1);
    drawn.push_back(Read(material));
    ctx.esp+=4;
}
X86_FUNC(__imp__sub_00114CD0) {
    if(extra)++fixture::outputs;
    auto material=ctx;material.ecx=materials_fixture::Object;
    sub_001221C0(material,base);
    if(extra && materials_fixture::added_pass) {
        material=ctx;material.ecx=materials_fixture::Object;
        sub_001221C0(material,base);
    }
    ctx.eax=0;ctx.esp+=4;
}
X86_FUNC(__imp__sub_00011069) {
    ++fixture::logic;
    auto renderer=ctx;sub_00114CD0(renderer,base);
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

MENU_MAIN = r'''
int main() {
  try {
    using fixture::Check;
    kernel::W32(0x510B74,0x700000);kernel::W32(0x700000,0x710000);
    kernel::W32(0x639750,0x720000);kernel::W32(0x639224,0x730000);
    kernel::W32(0x505970,0xC4);kernel::W32(0x4EF42C,1);
    for(const int rate:{60,120,144,165,240})for(const bool vsync:{false,true}) {
        openshao::frames::Configure(rate,vsync);
        app::Get().vsync=vsync;
        fixture::expect_menu=true;
        app::Get().fps_cap=openshao::frames::OutputCap();
        const auto before=fixture::presents;
        X86Context ctx{};ctx.esp=0x1000;
        sub_00114CD0(ctx,kernel::g_base);
        Check(fixture::presents==before+1,"A main-menu update must present exactly once");
        Check(ctx.esp==0x1004,"The menu renderer must preserve the guest call convention");

        // The pause loop is nested inside a gameplay callback while its world is held.
        kernel::W32(0x4EF42C,0);kernel::W32(0x6C9308,1);kernel::W32(0x6C930C,0x596810);
        fixture::now+=300000000;ctx={};ctx.esp=0x1000;
        const auto paused=fixture::presents;
        sub_00011069(ctx,kernel::g_base);
        Check(fixture::presents==paused+1,"A nested pause-menu update must present exactly once");
        Check(openshao::frames::Target()==rate && app::Get().vsync==vsync,
              "Menu pacing must preserve the player's gameplay target and VSync preference");

        fixture::expect_menu=false;kernel::W32(0x4EF42C,0);kernel::W32(0x6C9308,0);
        fixture::now+=16666667;ctx={};ctx.esp=0x1000;
        sub_00011069(ctx,kernel::g_base);
        if(rate>60) {
            const auto outputs=fixture::outputs,logic=fixture::logic;
            fixture::now+=8333333;ctx={};ctx.esp=0x1000;
            sub_00011069(ctx,kernel::g_base);
            Check(fixture::outputs==outputs+1 && fixture::logic==logic,
                  "Leaving a menu must restore extra gameplay frames without advancing simulation");
        }
    }
    std::cout<<"menu pacing and gameplay restoration: ok\n";
    return 0;
  } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
'''

POSE_MAIN = r'''
namespace pose_fixture {
constexpr uint32_t Model=0x740000,Bones=0x750000;
void Transform(uint32_t address,float x) {
    const std::array<float,20> values{1,0,0,0,0,1,0,0,0,0,1,0,x,0,0,1,x,0,0,1};
    CopyIn(address,values.data(),sizeof values);
}
void Update(uint32_t caller) {
    X86Context ctx{};ctx.esp=0x1000;
    kernel::W32(ctx.esp,caller);kernel::W32(ctx.esp+4,Model);
    sub_00107880(ctx,kernel::g_base);
    fixture::Check(ctx.esp==0x1004,"The original pose update must preserve its cdecl return");
}
void Capture(float x,float chest,float angle=0) {
    Transform(Model+0x60,x);
    Transform(Bones+0x60,1);
    Transform(Bones+0x390+0x60,chest);
    kernel::G<float>(Bones+0x390+0x60)=std::cos(angle);
    kernel::G<float>(Bones+0x390+0x64)=std::sin(angle);
    kernel::G<float>(Bones+0x390+0x70)=-std::sin(angle);
    kernel::G<float>(Bones+0x390+0x74)=std::cos(angle);
    kernel::G<uint8_t>(Bones+0x210)=0;
    kernel::G<uint8_t>(Bones+0x390+0x210)=0;
    ++tick;Update(0x12B8E4);
}
void CheckPose(float chest,float root,const char* reason) {
    fixture::Check(std::abs(kernel::G<float>(Bones+0x390+0x50)-chest)<.0001f &&
                   std::abs(kernel::G<float>(Model+0xA0)-root)<.0001f,reason);
}
}
int main() {
  try {
    using namespace pose_fixture;
    kernel::W32(Model+0x28,Bones);kernel::W32(Model+0x30,2);
    kernel::W32(Bones,Model+0x50);kernel::W32(Bones+0x390,Bones);
    active=true;
    for(const bool moving:{false,true}) {
        poses.clear();tick=0;
        Capture(100,2);Capture(moving?104.f:100.f,6);
        CheckPose(moving?111.f:107.f,moving?104.f:100.f,"The native fixture must produce the expected current pose");
        const auto& pose=poses.at(Model);
        for(const float fraction:{0.f,.25f,.5f,.75f}) {
            alpha=fraction;WritePose(Model,pose,true);
            const float expected=moving?103.f+8.f*fraction:103.f+4.f*fraction;
            const float root=moving?100.f+4.f*fraction:100.f;
            CheckPose(expected,root,"The visual pose must interpolate before the native renderer runs");
            // The original character render temporarily changes the root and rebuilds
            // the hierarchy from local transforms. Use the real native routine here.
            kernel::G<uint8_t>(Bones+0x210)=0;
            Update(0x12C173);
            CheckPose(expected,root,"A native render rebuild must not snap the interpolated torso to the current pose");
            RestorePoses();
            CheckPose(moving?111.f:107.f,moving?104.f:100.f,"Rendering must restore the current simulation pose");
            fixture::Check(kernel::G<float>(Bones+0x390+0xA0)==6,
                           "Rendering must restore the current local animation transform");
        }
    }
    // Idle bug: a local bone rotation oscillates while the root is still; native redraw
    // must keep intermediate orientations, not rebuild from the newest keyframe.
    poses.clear();tick=0;
    Capture(100,2,0);Capture(100,2,.2f);
    const auto& idle=poses.at(Model);
    for(const float fraction:{0.f,.25f,.5f,.75f,1.f}) {
        alpha=fraction;WritePose(Model,idle,true);
        const float cosine=1.f+(std::cos(.2f)-1.f)*fraction;
        const float sine=std::sin(.2f)*fraction;
        const auto check_rotation=[&] {
            fixture::Check(std::abs(kernel::G<float>(Bones+0x390+0x60)-cosine)<.0001f &&
                           std::abs(kernel::G<float>(Bones+0x390+0x64)-sine)<.0001f &&
                           std::abs(kernel::G<float>(Bones+0x390+0x10)-cosine)<.0001f &&
                           std::abs(kernel::G<float>(Bones+0x390+0x14)-sine)<.0001f,
                           "Native idle redraw must keep the interpolated bone orientation");
        };
        check_rotation();
        kernel::G<uint8_t>(Bones+0x390+0x210)=0;
        Update(0x12C173);
        check_rotation();
        RestorePoses();
        fixture::Check(std::abs(kernel::G<float>(Bones+0x390+0x60)-std::cos(.2f))<.0001f,
                       "Idle drawing must restore the current animation orientation");
    }
    active=false;
    std::cout<<"native pose interpolation survives render rebuilds: ok\n";
    return 0;
  } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
'''

POSE_FLOW = r'''
namespace pose_flow {
constexpr uint32_t Model=0x740000,Bones=0x750000;
std::vector<float> drawn;
void Transform(uint32_t address,float x) {
    const std::array<float,20> values{1,0,0,0,0,1,0,0,0,0,1,0,x,0,0,1,x,0,0,1};
    CopyIn(address,values.data(),sizeof values);
}
}
X86_FUNC(__imp__sub_00011069) {
    ++fixture::logic;
    kernel::W32(0x5AFC60,fixture::logic);
    pose_flow::Transform(pose_flow::Bones+0x390+0x60,float(fixture::logic));
    kernel::G<uint8_t>(pose_flow::Bones+0x390+0x210)=0;
    auto render=ctx;sub_00114CD0(render,base);
    ctx.eax=0;ctx.esp+=4;
}
X86_FUNC(__imp__sub_00114CD0) {
    // CEngineRenderer::Render -> UpdateCharacters -> cPxCharacter::Update.
    // Pose capture therefore happens INSIDE the render, after its entry hook.
    auto update=ctx;update.esp=0x2000;
    kernel::W32(0x2000,0x12B8E4);kernel::W32(0x2004,pose_flow::Model);
    kernel::G<uint8_t>(pose_flow::Bones+0x210)=0;
    sub_00107880(update,base);
    pose_flow::drawn.push_back(kernel::G<float>(pose_flow::Bones+0x390+0x50));
    ++fixture::presents;
    if(extra)++fixture::outputs;
    ctx.eax=0;ctx.esp+=4;
}
int main() {
  try {
    using namespace pose_flow;
    kernel::W32(0x510B74,0x700000);kernel::W32(0x700000,0x710000);
    kernel::W32(0x639750,0x720000);kernel::W32(0x639224,0x730000);
    kernel::W32(0x505970,0x60);kernel::W32(0x4EF42C,0);
    kernel::W32(Model+0x28,Bones);kernel::W32(Model+0x30,2);
    kernel::W32(Bones,Model+0x50);kernel::W32(Bones+0x390,Bones);
    Transform(Model+0x60,100);Transform(Bones+0x60,1);
    for(const int rate:{60,120,144,165,240}) {
        openshao::frames::Configure(rate,false);
        // Reset the simulation clock without replacing its implementation.
        fixture::now+=300000000;
        drawn.clear();
        const auto origin=fixture::now;
        const auto first_logic=fixture::logic;
        for(int output=0;output<rate*2;++output) {
            fixture::now=origin+int64_t(output)*1000000000/rate;
            X86Context ctx{};ctx.esp=0x1000;sub_00011069(ctx,kernel::g_base);
            const float current=101.f+float(fixture::logic);
            fixture::Check(std::abs(kernel::G<float>(Bones+0x390+0x50)-current)<.001f &&
                           kernel::G<float>(Bones+0x390+0xA0)==float(fixture::logic),
                           "Every output must restore the exact simulation pose before the next update");
            if(output>(rate+59)/60 && rate>60) {
                const float expected=101.f+float(first_logic)+float(output)*60.f/rate;
                fixture::Check(std::abs(drawn.back()-expected)<.002f,
                    "The complete native render sequence must advance smoothly on simulation and extra frames");
            }
        }
    }
    std::cout<<"full native render cadence has no forward/backward pose jumps: ok\n";
    return 0;
  } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
'''


def original_pose_functions():
    names = {"00107880", "0010DEB0", "0010DED0", "000FF9A4"}
    found = {}
    for path in OPTIONS.generated.glob("x86_recomp.*.cpp"):
        source = path.read_text(encoding="utf-8")
        for name in names - found.keys():
            start = source.find(f"X86_FUNC(__imp__sub_{name}) {{")
            if start >= 0:
                found[name] = source[start:source.index("\n}", start) + 2]
        if names == found.keys():
            break
    if names != found.keys():
        raise RuntimeError("Generated pose functions are missing: " + ", ".join(sorted(names - found.keys())))
    native = '#include "cpu/x86_ops.h"\n'
    for name in ("0010DEB0", "0010DED0", "000FF9A4", "00147EF0"):
        native += f"X86_EXTERN_FUNC(sub_{name});\n"
    for name in ("0010DEB0", "0010DED0", "000FF9A4", "00107880"):
        native += found[name] + "\n"
        if name != "00107880":
            native += f"X86_WEAK_FUNC(sub_{name}, __imp__sub_{name})\n"
    native += 'X86_FUNC(sub_00147EF0) {fixture::Check(false,"This fixture has no attached models");}\n'
    return native


class NativeOutputTest(unittest.TestCase):
    def test_busy_frame_recovery_keeps_simulation_and_fault_guard(self):
        for rate in (120,144,165,240):
            with self.subTest(rate=rate):
                self.run_fixture(MAIN.replace("Configure(120,false)",f"Configure({rate},false)"))

    def test_menus_stay_at_sixty_and_restore_the_gameplay_target(self):
        self.run_fixture(MENU_MAIN)

    def test_interpolated_pose_survives_the_native_render_rebuild(self):
        self.run_fixture(POSE_MAIN, original_pose_functions())

    def test_cleared_render_marker_does_not_disable_active_gameplay(self):
        self.run_fixture(r'''
int main() {
  try {
    kernel::W32(0x639224,0x730000);
    kernel::W32(0x639750,0x720000);
    kernel::W32(0x510B74,0x700000);kernel::W32(0x700000,0x710000);
    kernel::W32(0x4EF42C,0);
    active=true;in_gameplay=true;
    X86Context ctx{};ctx.esp=0x1000;
    sub_00114CD0(ctx,kernel::g_base);
    fixture::Check(draws==1,"A cleared renderer marker must not classify active gameplay as a menu");
    return 0;
  } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
''')

    def test_complete_render_sequence_does_not_alternate_current_and_older_poses(self):
        self.run_fixture(POSE_FLOW, original_pose_functions(), renderer_in_main=True)

    def test_material_replay_uses_the_native_animation_state_and_restores_it(self):
        self.run_fixture(MATERIAL_FLOW + r'''
int main() {
  try {
    using namespace materials_fixture;
    Init();openshao::frames::Configure(240,false);
    Step(0);Step(4166667);Step(8333334);
    fixture::Check(drawn.size()==3 && drawn[0]==Values{11,21,31,41} &&
                   drawn[1]==drawn[0] && drawn[2]==drawn[0],
        "Extra passes must use the native animation inputs from the contiguous material bank");
    fixture::Check(Read(MaterialA)==Values{11,21,31,41} && fixture::logic==1,
        "Extra passes must restore the scroll stamp, UV offsets and texture cursor");
    return 0;
  } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
''', renderer_in_main=True)

    def test_material_replay_recovers_after_a_changed_pass_in_the_same_room(self):
        self.run_fixture(MATERIAL_FLOW + r'''
int main() {
  try {
    using namespace materials_fixture;
    Init();openshao::frames::Configure(240,false);
    Step(0);
    kernel::W32(List+8,4);
    added_pass=true;
    Step(4166667);
    fixture::Check(drawn[1]==Values{101,201,301,401},
        "A different material must never receive the previous material's animation values");
    const auto restored=Read(MaterialB);
    added_pass=false;
    Step(16666667);Step(20833334);
    fixture::Check(fixture::outputs==2 && fixture::logic==2 && kernel::R32(0x505970)==73,
        "A fresh native frame must allow interpolation to resume in the same room");
    fixture::Check(restored==Values{100,200,300,400},
        "A mismatched output pass must restore the live material without advancing simulation");
    fixture::Check(drawn.back()==Values{101,201,301,401} && Read(MaterialB)==drawn.back(),
        "Interpolation must use and restore the newly captured material");
    return 0;
  } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
''', renderer_in_main=True)

    def test_restored_states_reach_the_render_worker_with_dirty_tracking(self):
        self.run_fixture(r'''
namespace mem {
uint32_t AllocSystem(uint32_t,uint32_t){return 0;}
uint32_t VirtualFromPhysical(uint32_t p){return p;}
}
struct Observer final : gpu::Backend {
    std::vector<std::array<uint32_t,3>> seen;
    bool Init(uint32_t,uint32_t)override{return true;}
    void Clear(const gpu::State&,uint32_t,const uint8_t*,uint32_t,uint32_t,float,uint32_t)override{}
    void Draw(const gpu::State& s,uint32_t,uint32_t,uint32_t,const uint8_t*)override{
        seen.push_back({s.simple[gpu::kMBlendColor/4],s.simple[gpu::kMCountEnable/4],s.simple.back()});
    }
    void Copy(const gpu::Surface&,const gpu::Surface&,uint32_t,uint32_t,uint32_t)override{}
    void Present(const gpu::Surface&)override{}
    void Sync(uint32_t,uint32_t)override{}
    void SyncLate(uint32_t)override{}
    void Flush()override{}
    void SetGamma(const uint8_t*)override{}
};
int main() {
  try {
    auto observer=std::make_unique<Observer>();auto* recorded=observer.get();
    auto worker=gpu::CreateThreadedBackend(std::move(observer));
    gpu::g_simple_tracked=true;gpu::MarkAllSimple();
    const auto draw=[&]{worker->Draw(d3d::State(),6,0,3,nullptr);worker->WaitFence(worker->Fence());};
    draw();
    auto restored=d3d::State();
    restored.simple[gpu::kMBlendColor/4]=0xFF123456;
    restored.simple[gpu::kMCountEnable/4]=1;
    restored.simple.back()=0xABCDEF01;
    RestoreGpu(restored);draw();
    fixture::Check(recorded->seen.back()==std::array<uint32_t,3>{0xFF123456,1,0xABCDEF01},
        "Restored interpolation state must reach the worker, including visibility tests and the final state block");
    RestoreGpu(gpu::State{});draw();
    fixture::Check(recorded->seen.back()==std::array<uint32_t,3>{0,0,0},
        "Returning from an output pass must restore the original state on the worker");
    gpu::g_simple_tracked=false;
    d3d::State().simple[gpu::kMBlendColor/4]=0xFFEEDDCC;draw();
    fixture::Check(recorded->seen.back()[0]==0xFFEEDDCC,
        "Untracked state producers must retain the full-comparison fallback");
    worker.reset();return 0;
  } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
''', threaded=True)

    def run_fixture(self, main, native="", renderer_in_main=False, threaded=False):
        source = (ROOT / "src/native_frames.cpp").read_text(encoding="utf-8")
        source = source.replace("using Clock=std::chrono::steady_clock;", "using Clock=fixture::Clock;")
        imports = re.findall(r"X86_EXTERN_FUNC\((\w+)\);", source)
        supplied = {"__imp__sub_00011069", "__imp__sub_00114CD0"}
        supplied.update(re.findall(r"X86_FUNC\((__imp__\w+)\)", native + main))
        stubs = "\n".join(f"X86_FUNC({name}) {{ctx.eax=0;ctx.esp+=4;}}"
                          for name in imports if name not in supplied)
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
            boundary = BOUNDARY
            if renderer_in_main:
                boundary = boundary[:boundary.index('X86_FUNC(__imp__sub_00011069)')]
            thread_source = ""
            if threaded:
                thread_source = (OPTIONS.sdk / "runtime/gpu/render_thread.cpp").read_text(encoding="utf-8")
                thread_source = thread_source.replace('#include <windows.h>',
                    '#ifdef _WIN32\n#include <windows.h>\n#else\n#include <pthread.h>\n#endif')
                start = thread_source.index('        using SetDescription =')
                end = thread_source.index('        for (;;) {', start)
                thread_source = thread_source[:start] + '#ifdef _WIN32\n' + thread_source[start:end] + '#endif\n' + thread_source[end:]
            cpp.write_text("\n".join((FIXTURE, source, boundary, stubs, native, thread_source, main)), encoding="utf-8")
            result = subprocess.run([OPTIONS.compiler, "-std=c++20", "-O0", "-march=x86-64-v2", "-fuse-ld=lld",
                                     "-I" + str(ROOT / "src/sdk_compat"), "-I" + str(ROOT / "src"),
                                     "-I" + str(OPTIONS.sdk / "runtime"),
                                     str(cpp), "-o", str(exe)], env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--sdk", required=True, type=Path)
    parser.add_argument("--generated", required=True, type=Path)
    OPTIONS, remaining = parser.parse_known_args()
    unittest.main(argv=[__file__, *remaining])

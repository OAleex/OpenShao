#include "strip_orientation.h"
#include "pipeline_assets.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <bit>
#include <cmath>
#include <limits>

static std::array<uint8_t, 0x10000> memory{};
static uint32_t room = 0xA7;
static uint32_t load_phase = 0, load_record = 0;
// The hook's parser/ABI test has no asynchronous compilation work. The actual
// queue and load gate have their own dependency/completion regression tests.
namespace openshao::pipelines {
void RoomLoading(uint32_t){}
void BeginPipelineLoad(){}
void SealPipelineLoad(){}
bool PipelineLoadPending(){return false;}
void EndPipelineLoad(){}
}
static void Check(bool value, const char* text) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
}
struct X86Context { uint32_t esp=0x1000, eax=0, ecx=0x2000; };
namespace kernel {
uint32_t R32(uint32_t address) { if (address == 0x505970) return room; if(address==0x695304)return load_phase; if(address==0x695310)return load_record; uint32_t value; Check(address <= memory.size()-4, "valid guest read"); std::memcpy(&value,memory.data()+address,4); return value; }
void W32(uint32_t address,uint32_t value) { Check(address <= memory.size()-4, "valid guest write"); std::memcpy(memory.data()+address,&value,4); }
void* HostPtr(uint32_t address) { Check(address < memory.size(), "valid guest data span"); return memory.data()+address; }
}
namespace openshao {
bool RestoreKnownWastelandHouseOpacity(uint32_t source_room, std::span<uint8_t> archive) {
    if(source_room==0xBB && archive.size()==4 && archive[0]==0xE2) {
        archive[0]=0xD2;return true;
    }
    if(source_room!=0xAA || archive.size()!=4 || archive[0]!=0xE1)return false;
    archive[0]=0xD1;return true;
}
bool RestoreKnownArenaSoulMaterials(uint32_t source_room, std::span<uint8_t> scene) {
    if(source_room!=0xA9 || scene.size()!=4 || scene[0]!=0xEF)return false;
    scene[0]=0xCD;return true;
}
unsigned KnownBackgroundStripSourceId(std::span<const uint8_t> vertices,std::span<const uint8_t> source) {
    return vertices[0] == 0xDD && source[0] == 0x80 ? 1 : 0;
}
bool KnownStripSourceNeedsTwoSides(std::span<const uint8_t> vertices,std::span<const uint8_t> source) {
    Check(vertices.size()==16 && source.size()==6, "loaded content dimensions reach the catalog");
    return vertices[0]==0xAA && source[0]==0x80;
}
bool KnownRoomStripSourceNeedsTwoSides(uint32_t source_room, std::span<const uint8_t> vertices,
                                     std::span<const uint8_t> source) {
    Check(source_room == room, "loaded room reaches the sidedness classifier");
    return (source_room == 0xAA && vertices[0] == 0xCE) ||
           KnownStripSourceNeedsTwoSides(vertices, source);
}
bool KnownStripSourceNeedsNoDepthWrite(std::span<const uint8_t> vertices,std::span<const uint8_t> source) {
    Check(vertices.size()==16 && source.size()==6, "loaded transparent source dimensions reach the catalog");
    return vertices[0]==0xCC && source[0]==0x80;
}
bool KnownRoomStripSourceNeedsNoDepthWrite(uint32_t source_room,std::span<const uint8_t> vertices,
                                         std::span<const uint8_t> source) {
    Check(source_room==room,"loaded room reaches the transparent depth classifier");
    return (source_room==0xA3 && vertices[0]==0xCF && source[0]==0x80) ||
           KnownStripSourceNeedsNoDepthWrite(vertices,source);
}
bool RepairKnownStripOrientation(std::span<const uint8_t> vertices,std::span<uint8_t> source) {
    if (vertices[0]!=0xBB) return false;
    source[1]|=0x40; return true;
}
bool RepairKnownRoomStripOrientation(uint32_t source_room, std::span<const uint8_t> vertices,
                                    std::span<uint8_t> source) {
    Check(source_room == room, "loaded room reaches the orientation classifier");
    if (source_room == 0xA9 && vertices[0] == 0xBE) {
        source[1] |= 0x40;
        return true;
    }
    return RepairKnownStripOrientation(vertices, source);
}
}
#define X86_FUNC(name) void name(X86Context& ctx,uint8_t* base)
#define X86_EXTERN_FUNC(name) X86_FUNC(name)
#define LOG_FIRST(...) ((void)0)
static unsigned creates=0, stitches=0;
static unsigned load_calls=0;
static uint32_t callback_material=0;
static bool fail_create=false, expect_repair=false;
static unsigned alpha_adds = 0;
static float submitted_distance = 0;
X86_FUNC(__imp__sub_0017AA40) {
    ++load_calls;callback_material=kernel::R32(0x4500);
    ctx.eax=123;ctx.ecx=0x4321;ctx.esp+=4;
}
X86_FUNC(__imp__sub_00120790) {
    ++alpha_adds;
    Check(kernel::R32(ctx.esp+4) == 0x6000, "alpha record pointer is preserved");
    submitted_distance = std::bit_cast<float>(kernel::R32(ctx.esp+8));
    ctx.eax=123;ctx.ecx=0x4321;ctx.esp+=12;
}
X86_FUNC(__imp__sub_00133110) {
    ++creates;ctx.eax=fail_create?0:0x5000;ctx.ecx=0x1234;ctx.esp+=36;
    kernel::W32(0x5048,0xA5);
    kernel::W32(0x504C,0xA6);
}
X86_FUNC(__imp__sub_00131070) {
    ++stitches;
    Check(bool(memory[0x4101]&0x40)==expect_repair,"repair precedes original count/write execution");
    ctx.eax=42;ctx.ecx=0x5678;ctx.esp+=20;
}
#include "strip_orientation_hooks.inl"

int main() {
    room=0xAA;load_phase=3;load_record=0x4400;
    kernel::W32(0x4408,0x4500);kernel::W32(0x440C,4);kernel::W32(0x4500,0xE1);
    X86Context house_load;sub_0017AA40(house_load,nullptr);
    Check(callback_material==0xD1,"house opacity recovery precedes the original texture parse callback");
    Check(load_calls==1 && house_load.eax==123 && house_load.ecx==0x4321 && house_load.esp==0x1004,
          "texture loader wrapper preserves the original guest ABI");
    for(auto [other_room,phase]:std::array<std::pair<uint32_t,uint32_t>,3>{{{0xAA,2},{0xA3,3},{0xBB,3}}}) {
        room=other_room;load_phase=phase;kernel::W32(0x4500,0xE1);
        X86Context other_load;sub_0017AA40(other_load,nullptr);
        Check(callback_material==0xE1,"other rooms and incomplete texture loads retain their original bytes");
    }
    room=0xAA;load_phase=3;kernel::W32(0x4500,0xF1);
    X86Context modified_archive;sub_0017AA40(modified_archive,nullptr);
    Check(callback_material==0xF1,"modified texture archive content is preserved");
    room=0xBB;load_phase=3;kernel::W32(0x4500,0xE2);
    X86Context survival_house;sub_0017AA40(survival_house,nullptr);
    Check(callback_material==0xD2,"Survival 28 house opacity recovery precedes the original texture parse callback");
    Check(survival_house.eax==123 && survival_house.ecx==0x4321 && survival_house.esp==0x1004,
          "Survival 28 texture loading preserves the original guest ABI");
    load_phase=2;kernel::W32(0x4500,0xE2);
    X86Context partial_survival_house;sub_0017AA40(partial_survival_house,nullptr);
    Check(callback_material==0xE2,"incomplete Survival 28 texture loads retain their original bytes");
    load_calls=0;
    room=0xA9;load_phase=3;load_record=0x4400;
    kernel::W32(0x4408,0x4500);kernel::W32(0x440C,4);kernel::W32(0x4500,0xEF);
    X86Context loaded;sub_0017AA40(loaded,nullptr);
    Check(callback_material==0xCD,"soul state recovery precedes the original scene parse callback");
    Check(load_calls==1 && loaded.eax==123 && loaded.ecx==0x4321 && loaded.esp==0x1004,
          "scene loader wrapper preserves the original guest ABI");
    for(auto [other_room,phase]:std::array<std::pair<uint32_t,uint32_t>,3>{{{0xA4,3},{0xA9,2},{0xA8,3}}}) {
        room=other_room;load_phase=phase;kernel::W32(0x4500,0xEF);
        X86Context other_load;sub_0017AA40(other_load,nullptr);
        Check(callback_material==0xEF,"other rooms and incomplete loads retain their original bytes");
    }
    room=0xA9;load_phase=3;kernel::W32(0x4500,0xAF);
    X86Context modified_load;sub_0017AA40(modified_load,nullptr);
    Check(callback_material==0xAF,"modified scene content is preserved");
    load_phase=0;load_record=0;room=0xA7;
    kernel::W32(0x1004,0x3000);kernel::W32(0x2004,0x3000);
    kernel::W32(0x3008,6);kernel::W32(0x3010,1);kernel::W32(0x3040,0x4000);kernel::W32(0x3044,0x3200);
    kernel::W32(0x3204,16);kernel::W32(0x320C,3);kernel::W32(0x3210,0x4100);
    memory[0x4000]=0xAA;memory[0x4100]=0x80;
    X86Context created;sub_00133110(created,nullptr);
    Check(kernel::R32(0x5048)==0x800000A5u,"original object sidedness flag preserves other flags");
    Check(creates==1 && created.eax==0x5000 && created.ecx==0x1234 && created.esp==0x1024,"object wrapper preserves original ABI");
    Check(kernel::R32(0x504C)==0xA6,"two-sided source preserves original depth metadata");
    memory[0x4000]=0xCC;
    X86Context transparent;sub_00133110(transparent,nullptr);
    Check(kernel::R32(0x504C)==0xA7,"transparent source uses the original no-depth-write object flag");
    Check(kernel::R32(0x5048)==0xA5,"transparent source preserves original culling metadata");
    Check(transparent.eax==0x5000 && transparent.ecx==0x1234 && transparent.esp==0x1024,"transparent object wrapper preserves original ABI");
    room=0xA3;memory[0x4000]=0xCF;
    X86Context light_shaft;sub_00133110(light_shaft,nullptr);
    Check(kernel::R32(0x504C)==0xA7,"A3 translucent shaft cannot write a hard depth silhouette over the portal");
    Check(kernel::R32(0x5048)==0xA5,"shaft correction retains original culling");
    room=0x29;
    X86Context other_shaft;sub_00133110(other_shaft,nullptr);
    Check(kernel::R32(0x504C)==0xA6,"another room retains its original shaft depth metadata");
    room=0xA7;
    memory[0x4000]=0;
    X86Context unknown;sub_00133110(unknown,nullptr);
    Check(kernel::R32(0x5048)==0xA5,"unknown object keeps its original sidedness");
    Check(kernel::R32(0x504C)==0xA6,"unknown object keeps its original depth metadata");
    room=0xAA;memory[0x4000]=0xCE;
    X86Context cliff;sub_00133110(cliff,nullptr);
    Check(kernel::R32(0x5048)==0x800000A5u,"known Wasteland cliff supports the arena's reverse views");
    Check(kernel::R32(0x504C)==0xA6,"cliff visibility preserves original depth metadata");
    room=0x29;
    X86Context campaign_cliff;sub_00133110(campaign_cliff,nullptr);
    Check(kernel::R32(0x5048)==0xA5,"campaign cliff retains its original sidedness");
    room=0xA7;
    memory[0x4000]=0xAA;fail_create=true;
    X86Context failed;sub_00133110(failed,nullptr);
    Check(failed.eax==0 && kernel::R32(0x5048)==0xA5,"failed creation performs no object mutation");
    fail_create=false;memory[0x4000]=0xBB;expect_repair=true;
    kernel::W32(0x1008,0x3200);
    X86Context counted;sub_00131070(counted,nullptr);
    Check(stitches==1 && counted.eax==42 && counted.ecx==0x5678 && counted.esp==0x1014,"stitch wrapper preserves original ABI");
    Check(memory[0x4101]==0x40,"count pass sees corrected source metadata");
    memory[0x4000]=0;memory[0x4101]=0;expect_repair=false;
    X86Context unchanged;sub_00131070(unchanged,nullptr);
    Check(stitches==2 && memory[0x4101]==0,"unknown source still executes the original stitcher");
    room=0xA9;memory[0x4000]=0xBE;expect_repair=true;
    X86Context scoped;sub_00131070(scoped,nullptr);
    Check(memory[0x4101]==0x40,"arena-specific source repair precedes native stitching");
    for (uint32_t other_room : {0x04u,0xA4u,0xA8u}) {
        room=other_room;memory[0x4101]=0;expect_repair=false;
        X86Context unrelated;sub_00131070(unrelated,nullptr);
        Check(memory[0x4101]==0,"shared campaign and accepted arena sources preserve their orientation");
    }
    room=0xA7;
    kernel::W32(0x1004,0x3000);memory[0x4000]=0xDD;
    X86Context sky;sub_00133110(sky,nullptr);
    Check(kernel::R32(0x5048)==0xA5 && kernel::R32(0x504C)==0xA6,
          "sky priority leaves original culling and depth flags unchanged");
    kernel::W32(0x6000,1);kernel::W32(0x6004,0x7000);kernel::W32(0x7158,0x5010);
    const auto submit = [](float key) {
        kernel::W32(0x1004,0x6000);kernel::W32(0x1008,std::bit_cast<uint32_t>(key));
        X86Context call;sub_00120790(call,nullptr);
        Check(call.eax==123 && call.ecx==0x4321 && call.esp==0x100C,"alpha sorting wrapper preserves original guest ABI");
        Check(kernel::R32(0x1008)==std::bit_cast<uint32_t>(key),"alpha distance caller argument is restored");
        return submitted_distance;
    };
    const float near_sky=submit(8111),far_sky=submit(9420);
    Check(near_sky>1.0e11f && far_sky>near_sky,"known background enters a finite ordered priority band");
    Check(std::isfinite(submit(8.0e8f)),"squared source distances remain finite");
    room=0xA0;Check(submit(8111)==8111,"protected campaign map keeps its original ordering");
    room=0xA7;kernel::W32(0x6000,3);Check(submit(8111)==8111,"sprite records keep their original ordering");
    kernel::W32(0x6000,2);Check(submit(8111)>1.0e11f,"transformed source draw units use the same known owner");
    Check(submit(-1)==-1 && std::isinf(submit(std::numeric_limits<float>::infinity())),"invalid source distances are preserved");
    kernel::W32(0x1004,0x3000);memory[0x4000]=0;
    X86Context rebuilt;sub_00133110(rebuilt,nullptr);
    Check(submit(8111)==8111,"unknown content rebuilt at an old owner address loses background priority");
    Check(alpha_adds==9,"every sorting call executes the original alpha-list implementation exactly once");
    std::puts("PASS: source object flags, count-before-write ordering, unknown content and original guest ABI");
}

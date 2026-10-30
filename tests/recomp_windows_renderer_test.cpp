// Verify selection, fallback resource order and reuse across device resets.
#include "gpu/backend.h"
#include "launcher_settings.h"
#include "host.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace openshao { Options options; }
namespace app { void* Window(){return nullptr;} }
namespace {
openshao::launcher::Settings settings;
std::vector<std::string> events;
bool vulkan_ok=true, d3d_ok=true;
void Require(bool good,const char* message){if(!good)throw std::runtime_error(message);}
struct Probe final : gpu::Backend {
    bool vulkan;
    explicit Probe(bool v):vulkan(v){events.push_back(v?"v-create":"d-create");}
    ~Probe(){events.push_back(vulkan?"v-release":"d-release");}
    bool Init(uint32_t w,uint32_t h) override {
        Require(w==640 && h==480,"the selected renderer must receive the original dimensions");
        events.push_back(vulkan?"v-init":"d-init");return vulkan?vulkan_ok:d3d_ok;
    }
    void Clear(const gpu::State&,uint32_t,const uint8_t*,uint32_t,uint32_t,float,uint32_t)override{}
    void Draw(const gpu::State&,uint32_t,uint32_t,uint32_t,const uint8_t*)override{events.push_back(vulkan?"v-draw":"d-draw");}
    void Copy(const gpu::Surface&,const gpu::Surface&,uint32_t,uint32_t,uint32_t)override{}
    void Present(const gpu::Surface&)override{events.push_back(vulkan?"v-present":"d-present");}
    void Sync(uint32_t,uint32_t)override{}
    void SyncLate(uint32_t)override{}
    void Flush()override{}
    void SetGamma(const uint8_t*)override{}
    uint32_t Fence()override{return vulkan?17:23;}
    bool Busy(uint32_t)override{return true;}
};
}
namespace openshao::launcher { const Settings& Current(){return settings;} }
namespace gpu {
std::unique_ptr<Backend> CreateD3D12Backend(){return std::make_unique<Probe>(false);}
std::unique_ptr<Backend> CreateVulkanBackend(){return std::make_unique<Probe>(true);}
}

int main(int argc,char** argv){
    try {
        const std::string scenario=argc>1?argv[1]:"default";
        openshao::options.background=true;
        if(scenario!="default")settings.renderer="vulkan";
        vulkan_ok=scenario!="fallback" && scenario!="failure";
        d3d_ok=scenario!="failure";
        auto backend=gpu::CreateBackend();
        const bool expected_vulkan=scenario=="vulkan";
        Require(backend && backend->Init(640,480)==(scenario!="failure"),"renderer initialization must reflect its selected or fallback device");
        if(scenario=="fallback" || scenario=="failure") {
            Require(events==std::vector<std::string>{"v-create","v-init","v-release","d-create","d-init"},
                    "failed Vulkan resources must be released before starting D3D12");
        }
        if(scenario!="failure") {
            gpu::State s{};gpu::Surface front{};
            backend->Draw(s,6,0,4,nullptr);backend->Present(front);
            Require(events[events.size()-2]==(expected_vulkan?"v-draw":"d-draw") &&
                    events.back()==(expected_vulkan?"v-present":"d-present") &&
                    backend->Fence()==(expected_vulkan?17u:23u) && backend->Busy(1),
                    "draws, presentation and resource fences must reach the active renderer");
        }
        backend.reset();events.clear();
        settings.renderer=expected_vulkan?"d3d12":"vulkan";
        backend=gpu::CreateBackend();
        Require(events.front()==(expected_vulkan?"v-create":"d-create"),
                "a device reset must retain the session API even if saved preferences change");
        std::puts("PASS: Windows renderer selection and fallback");
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}

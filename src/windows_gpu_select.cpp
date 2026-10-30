// Keep the chosen API stable across guest device resets. A failed Vulkan
// initialization releases its resources before trying D3D12 for this session.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <optional>
#include "app/app.h"
#include "core/log.h"
#include "gpu/backend.h"
#include "host.h"
#include "launcher.h"

namespace gpu {
std::unique_ptr<Backend> CreateD3D12Backend();
std::unique_ptr<Backend> CreateVulkanBackend();
namespace {
std::optional<bool> selected_vulkan;
class Fallback final : public Backend {
public:
    explicit Fallback(std::unique_ptr<Backend> inner):inner_(std::move(inner)){}
    bool Init(uint32_t width,uint32_t height)override {
        if(inner_ && inner_->Init(width,height))return true;
        inner_.reset();
        selected_vulkan=false;
        LOGE("gpu: Vulkan initialization failed; trying Direct3D 12 for this session");
        if(!openshao::options.background)
            MessageBoxW(static_cast<HWND>(app::Window()),
                L"Vulkan could not start on this PC. OpenShao will try Direct3D 12 for this session.\n\n"
                L"You can choose Direct3D 12 in the launcher. The log contains the Vulkan error.",
                L"OpenShao",MB_OK|MB_ICONINFORMATION);
        inner_=CreateD3D12Backend();
        return inner_ && inner_->Init(width,height);
    }
    void Clear(const State& s,uint32_t n,const uint8_t* r,uint32_t f,uint32_t c,float z,uint32_t st)override{inner_->Clear(s,n,r,f,c,z,st);}
    void Draw(const State& s,uint32_t p,uint32_t b,uint32_t n,const uint8_t* i)override{inner_->Draw(s,p,b,n,i);}
    void Copy(const Surface& s,const Surface& d,uint32_t n,uint32_t r,uint32_t p)override{inner_->Copy(s,d,n,r,p);}
    void Present(const Surface& s)override{inner_->Present(s);}
    void Sync(uint32_t d,uint32_t n)override{inner_->Sync(d,n);}
    void SyncLate(uint32_t d)override{inner_->SyncLate(d);}
    bool DeliverLate(uint32_t d)override{return inner_->DeliverLate(d);}
    void RecordLate(uint32_t d)override{inner_->RecordLate(d);}
    void Flush()override{inner_->Flush();}
    void SetGamma(const uint8_t* ramp)override{inner_->SetGamma(ramp);}
    void ClearReport()override{inner_->ClearReport();}
    void GetReport(uint32_t r)override{inner_->GetReport(r);}
    uint32_t Fence()override{return inner_->Fence();}
    bool FencePending(uint32_t f)override{return inner_->FencePending(f);}
    void WaitFence(uint32_t f)override{inner_->WaitFence(f);}
    bool Busy(uint32_t r)override{return inner_->Busy(r);}
    void WaitResource(uint32_t r)override{inner_->WaitResource(r);}
private:
    std::unique_ptr<Backend> inner_;
};
}
std::unique_ptr<Backend> CreateBackend(){
    if(!selected_vulkan)selected_vulkan=openshao::launcher::Current().renderer=="vulkan";
    LOGR("gpu: selected %s",*selected_vulkan?"Vulkan":"Direct3D 12");
    if(*selected_vulkan)return std::make_unique<Fallback>(CreateVulkanBackend());
    return CreateD3D12Backend();
}
}

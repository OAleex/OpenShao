// Offscreen WARP regression. Synthetic atlas only; no game files or window.
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "gpu/shaders.h"
#include "raster_compat.h"
#include "gpu/vertex.h"
using Microsoft::WRL::ComPtr;

static bool Ok(HRESULT hr, const char* operation) {
    if (SUCCEEDED(hr)) return true;
    std::fprintf(stderr, "%s failed: %08X\n", operation, unsigned(hr));
    return false;
}
static ComPtr<ID3DBlob> Compile(const std::string& source, const char* profile) {
    ComPtr<ID3DBlob> code, error;
    if (!Ok(D3DCompile(source.data(), source.size(), nullptr, nullptr, nullptr,
                      "main", profile, D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, &error), "compile")) {
        if (error) std::fprintf(stderr, "%s\n", static_cast<const char*>(error->GetBufferPointer()));
    }
    return code;
}
static int RunCase(bool pretransformed, bool quad, float scale=1, int fallback=0, int menu=0) {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    if (!Ok(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                             D3D11_SDK_VERSION, &device, nullptr, &context), "WARP")) return 2;
    auto source = gpu::FixedVertexHlsl((1u << 0) | (1u << 3) | (1u << 9), pretransformed, 0, {});
    if (quad) source=gpu::QuadHlsl(source);
    auto vs_code = Compile(source, "vs_5_0");
    std::string pixel_source =
        "Texture2D atlas : register(t0); SamplerState pointSampler : register(s0);"
        "struct VOut {float4 pos:SV_Position;float4 d0:COLOR0;float4 d1:COLOR1;"
        "float4 b0:COLOR2;float4 b1:COLOR3;float4 tc0:TEXCOORD0;float4 tc1:TEXCOORD1;"
        "float4 tc2:TEXCOORD2;float4 tc3:TEXCOORD3;\n    float fogamt : TEXCOORD4;\n};"
        "float4 main(VOut i, bool front : SV_IsFrontFace):SV_Target {return atlas.Sample(pointSampler,i.tc0.xy);}";
    if (quad) pixel_source=gpu::QuadHlsl(pixel_source);
    auto ps_code=Compile(pixel_source,"ps_5_0");
    if (!vs_code || !ps_code) return 2;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11GeometryShader> gs;
    if (!Ok(device->CreateVertexShader(vs_code->GetBufferPointer(), vs_code->GetBufferSize(), nullptr, &vs), "VS") ||
        !Ok(device->CreatePixelShader(ps_code->GetBufferPointer(), ps_code->GetBufferSize(), nullptr, &ps), "PS")) return 2;
    if (quad) {
        auto gs_code=Compile(openshao::AtlasBoundsHlsl(),"gs_5_0");
        if (!gs_code || !Ok(device->CreateGeometryShader(gs_code->GetBufferPointer(),gs_code->GetBufferSize(),nullptr,&gs),"GS")) return 2;
    }
    D3D11_INPUT_ELEMENT_DESC elements[] = {
        {"ATTR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"ATTR",3,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"ATTR",9,DXGI_FORMAT_R32G32B32A32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0}};
    ComPtr<ID3D11InputLayout> layout;
    if (!Ok(device->CreateInputLayout(elements, 3, vs_code->GetBufferPointer(), vs_code->GetBufferSize(), &layout), "layout")) return 2;
    // Upper frame rows 0..6, row 7 is a white neighbor; console centers stop at y=6.78125.
    std::array<uint32_t,256> pixels{};
    for (int x=0; x<16; ++x) { pixels[x]=0xFF4080C0; pixels[7*16+x]=0xFFFFFFFF; }
    if (menu) {
        pixels.fill(0xFF4080C0);
        for (int y=0;y<16;++y) pixels[y*16+15]=0xFFFFFFFF;
        if (menu==4) pixels.fill(0xFF4080C0);
        if (menu==6) {
            pixels.fill(0xFF4080C0);
            for (int y=0;y<16;++y) for (int x=13;x<16;++x) pixels[y*16+x]=0;
        }
        if (menu==7) {
            pixels.fill(0xFF4080C0);
            for (int x=0;x<16;++x) pixels[15*16+x]=0xFFFFFFFF;
        }
    }
    D3D11_TEXTURE2D_DESC td{};
    td.Width=16; td.Height=16; td.MipLevels=1; td.ArraySize=1;
    td.Format=DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count=1;
    td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA texture_data{pixels.data(),16*4,0};
    ComPtr<ID3D11Texture2D> atlas, target, readback;
    ComPtr<ID3D11ShaderResourceView> srv;
    if (!Ok(device->CreateTexture2D(&td,&texture_data,&atlas),"atlas") ||
        !Ok(device->CreateShaderResourceView(atlas.Get(),nullptr,&srv),"SRV")) return 2;
    td.Width=UINT(32*scale); td.Height=UINT(16*scale); td.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11RenderTargetView> rtv;
    if (!Ok(device->CreateTexture2D(&td,nullptr,&target),"target") ||
        !Ok(device->CreateRenderTargetView(target.Get(),nullptr,&rtv),"RTV")) return 2;
    td.BindFlags=0; td.Usage=D3D11_USAGE_STAGING; td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    if (!Ok(device->CreateTexture2D(&td,nullptr,&readback),"readback")) return 2;
    float vertices[8][12] = {
        {4,4,.5f,1, 1,1,1,1, .25f,0,0,1},
        {28,4,.5f,1, 1,1,1,1, .75f,0,0,1},
        {4,12,.5f,1, 1,1,1,1, .25f,7.75f/16,0,1},
        {28,12,.5f,1, 1,1,1,1, .75f,7.75f/16,0,1}};
    if (menu) for (int i=0;i<4;++i) {
        vertices[i][0]=(i&1)?32.f:0.f; vertices[i][1]=(i&2)?16.f:0.f;
        vertices[i][3]=0; // retail immediate XYZRHW menu sprites use RHW zero
        vertices[i][8]=(i&1)?1.f:0.f; vertices[i][9]=(i&2)?1.f:0.f;
        if (menu==2) { vertices[i][8]-=3; vertices[i][9]-=2; }
        if (menu==3) { vertices[i][8]=-2-vertices[i][8]; vertices[i][9]=-1-vertices[i][9]; }
        if (menu==5) {
            // Measured UVs from the one retail fire-animation frame that flickers.
            vertices[i][8]=(i&1)?-2.000021457672119f:-3.000000238418579f;
            vertices[i][9]=(i&2)?-1.9998126029968262f:-3.f;
        }
        if (menu==7) {
            // Brightness frame: texture rotated clockwise onto the right edge, V from -8 to -9 across X.
            vertices[i][8]=(i&2)?1.f:0.f;
            vertices[i][9]=(i&1)?-9.f:-8.f;
        }
    }
    if (menu==4) for (int i=0;i<4;++i) {
        std::memcpy(vertices[i+4],vertices[i],sizeof vertices[i]);
        vertices[i][0]=(i&1)?14.35f:0.f;
        vertices[i+4][0]=(i&1)?32.f:14.4f;
    }
    if (menu==6) for (int i=0;i<4;++i) {
        std::memcpy(vertices[i+4],vertices[i],sizeof vertices[i]);
        vertices[i][0]=(i&1)?16.f:0.f;
        vertices[i+4][0]=(i&1)?32.f:13.f;
        vertices[i+4][8]=vertices[i+4][9]=.25f;
    }
    if (fallback==1) for (auto& v:vertices) { v[8]*=3; v[9]*=3; } // wrapping UVs
    if (fallback==2) { vertices[2][0]+=3; vertices[3][0]+=3; } // slanted sprite
    if (fallback==3) for (auto& v:vertices) v[11]=.5f; // projected coordinates
    if (fallback==4) for (auto& v:vertices) v[3]=.5f; // homogeneous W differs from one
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth=sizeof(vertices); bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vb_data{vertices,0,0};
    ComPtr<ID3D11Buffer> vb, cb;
    if (!Ok(device->CreateBuffer(&bd,&vb_data,&vb),"vertices")) return 2;
    gpu::DrawConstants constants{};
    constants.surf[0]=32; constants.surf[1]=16; constants.surf[2]=1;
    constants.wvp[0]=2.0f/32; constants.wvp[3]=-1;
    constants.wvp[5]=-2.0f/16; constants.wvp[7]=1;
    constants.wvp[10]=constants.wvp[15]=1;
    bd.ByteWidth=sizeof(constants); bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA cb_data{&constants,0,0};
    if (!Ok(device->CreateBuffer(&bd,&cb_data,&cb),"constants")) return 2;
    D3D11_SAMPLER_DESC sd{};
    sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD=D3D11_FLOAT32_MAX;
    if (menu) {
        gpu::Assembled a{}; a.pretransformed=true; a.vertex_count=4; a.regs=(1u<<0)|(1u<<9);
        a.elements[0]={DXGI_FORMAT_R32G32B32A32_FLOAT,0,0};
        a.elements[9]={DXGI_FORMAT_R32G32B32A32_FLOAT,0,32};
        a.buffers[0]={reinterpret_cast<const uint8_t*>(vertices),sizeof vertices,48};
        sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU=sd.AddressV=sd.AddressW=openshao::UnitSpriteTexcoords(a)?D3D11_TEXTURE_ADDRESS_CLAMP:D3D11_TEXTURE_ADDRESS_WRAP;
    }
    ComPtr<ID3D11SamplerState> sampler;
    if (!Ok(device->CreateSamplerState(&sd,&sampler),"sampler")) return 2;
    D3D11_BLEND_DESC blend_desc{};
    auto& blend=blend_desc.RenderTarget[0];
    blend.BlendEnable=true; blend.SrcBlend=D3D11_BLEND_SRC_ALPHA;
    blend.DestBlend=D3D11_BLEND_INV_SRC_ALPHA; blend.BlendOp=D3D11_BLEND_OP_ADD;
    blend.SrcBlendAlpha=D3D11_BLEND_ONE; blend.DestBlendAlpha=D3D11_BLEND_ZERO;
    blend.BlendOpAlpha=D3D11_BLEND_OP_ADD; blend.RenderTargetWriteMask=7; // HUD writes RGB, preserves target alpha
    ComPtr<ID3D11BlendState> blend_state;
    if (!Ok(device->CreateBlendState(&blend_desc,&blend_state),"blend")) return 2;
    D3D11_RASTERIZER_DESC raster_desc{};
    raster_desc.FillMode=D3D11_FILL_SOLID; raster_desc.CullMode=D3D11_CULL_NONE;
    raster_desc.DepthClipEnable=true;
    ComPtr<ID3D11RasterizerState> raster;
    if (!Ok(device->CreateRasterizerState(&raster_desc,&raster),"raster")) return 2;
    const float green[]={0,1,0,1};
    context->ClearRenderTargetView(rtv.Get(),green);
    ID3D11RenderTargetView* rtv_ptr=rtv.Get();
    context->OMSetRenderTargets(1,&rtv_ptr,nullptr);
    context->OMSetBlendState(blend_state.Get(),nullptr,~0u);
    context->RSSetState(raster.Get());
    D3D11_VIEWPORT viewport{0,0,32*scale,16*scale,0,1};
    context->RSSetViewports(1,&viewport);
    context->IASetInputLayout(layout.Get());
    ID3D11Buffer* vb_ptr=vb.Get(); UINT stride=48, offset=0;
    context->IASetVertexBuffers(0,1,&vb_ptr,&stride,&offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->VSSetShader(vs.Get(),nullptr,0);
    context->GSSetShader(gs.Get(),nullptr,0);
    ID3D11Buffer* cb_ptr=cb.Get(); context->VSSetConstantBuffers(1,1,&cb_ptr);
    context->GSSetConstantBuffers(1,1,&cb_ptr);
    context->PSSetShader(ps.Get(),nullptr,0);
    ID3D11ShaderResourceView* srv_ptr=srv.Get(); context->PSSetShaderResources(0,1,&srv_ptr);
    ID3D11SamplerState* sampler_ptr=sampler.Get(); context->PSSetSamplers(0,1,&sampler_ptr);
    context->Draw(4,0);
    if (menu==4 || menu==6) context->Draw(4,4);
    context->CopyResource(readback.Get(),target.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (!Ok(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped),"map")) return 2;
    if (menu) {
        if (menu==6) {
            uint32_t seam=0;
            const unsigned x=unsigned(std::ceil(13*scale-.5f)),y=unsigned(8*scale);
            std::memcpy(&seam,static_cast<const uint8_t*>(mapped.pData)+y*mapped.RowPitch+x*4,4);
            context->Unmap(readback.Get(),0);
            if (seam!=0xFF4080C0) { std::fprintf(stderr,"FAIL: transparent tile tail exposes a vertical seam at scale=%g, got %08X\n",scale,seam); return 1; }
            return 0;
        }
        if (menu==4) {
            bool covered=true;
            for (unsigned y=0;y<unsigned(16*scale);++y) for (unsigned x=0;x<unsigned(32*scale);++x) {
                uint32_t pixel; std::memcpy(&pixel,static_cast<const uint8_t*>(mapped.pData)+y*mapped.RowPitch+x*4,4);
                covered &= pixel==0xFF4080C0;
            }
            context->Unmap(readback.Get(),0);
            if (!covered) { std::fputs("FAIL: fractional menu tile junction leaves an uncovered line when upscaled\n",stderr); return 1; }
            return 0;
        }
        uint32_t corner=0, left=0;
        std::memcpy(&corner,mapped.pData,4);
        std::memcpy(&left,static_cast<const uint8_t*>(mapped.pData)+UINT(8*scale)*mapped.RowPitch,4);
        context->Unmap(readback.Get(),0);
        // A mirrored image legitimately begins with its last (white) texel.
        const auto expected=(menu==3 || menu==7)?0xFFFFFFFF:0xFF4080C0;
        if (corner!=expected || left!=expected) {
            std::fprintf(stderr,"Menu mode=%d scale=%g: outer border/first texel got %08X/%08X\n",menu,scale,corner,left);
            return 1;
        }
        return 0;
    }
    if (fallback) {
        const UINT row_bytes=UINT(32*scale)*4, height=UINT(16*scale);
        std::vector<uint8_t> guarded(row_bytes*height);
        for (UINT y=0;y<height;++y)
            std::memcpy(guarded.data()+y*row_bytes,static_cast<const uint8_t*>(mapped.pData)+y*mapped.RowPitch,row_bytes);
        context->Unmap(readback.Get(),0);
        context->ClearRenderTargetView(rtv.Get(),green);
        context->GSSetShader(nullptr,nullptr,0);
        context->Draw(4,0);
        context->CopyResource(readback.Get(),target.Get());
        if (!Ok(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped),"baseline map")) return 2;
        bool identical=true;
        for (UINT y=0;y<height;++y)
            identical &= !std::memcmp(guarded.data()+y*row_bytes,static_cast<const uint8_t*>(mapped.pData)+y*mapped.RowPitch,row_bytes);
        context->Unmap(readback.Get(),0);
        if (!identical) { std::fprintf(stderr,"Atlas guard changed unsupported geometry case %d\n",fallback); return 1; }
        std::puts("PASS: atlas guard preserves unsupported sprite geometry and texture coordinates");
        return 0;
    }
    uint32_t bottom=0,top=0;
    UINT sample_x=UINT(16*scale), bottom_y=UINT(std::ceil(12.5f*scale-.5f)-1);
    UINT top_y=UINT(std::ceil(4.5f*scale-.5f));
    std::memcpy(&bottom,static_cast<const uint8_t*>(mapped.pData)+bottom_y*mapped.RowPitch+sample_x*4,4);
    std::memcpy(&top,static_cast<const uint8_t*>(mapped.pData)+top_y*mapped.RowPitch+sample_x*4,4);
    context->Unmap(readback.Get(),0);
    if (bottom!=0xFF00FF00 || top!=0xFF4080C0) {
        std::fprintf(stderr,"Atlas edge (screen=%d quad=%d scale=%g): expected green FF00FF00, got %08X; top %08X\n",pretransformed,quad,scale,bottom,top);
        return 1;
    }
    std::puts("PASS: console pixel centers do not sample the neighboring atlas piece; top border retained");
    return 0;
}
int main() {
    int result=0;
    // Immediate menu UVs are FLOAT4; wrapping, projection and 3D must remain available.
    float uv[3][4]={{0,0,0,1},{1,0,0,1},{1,1,0,1}};
    float positions[3][4]={{0,0,.5f,0},{32,0,.5f,0},{32,16,.5f,0}};
    gpu::Assembled sprite{}; sprite.pretransformed=true; sprite.vertex_count=3; sprite.regs=(1u<<0)|(1u<<9);
    sprite.elements[9]={DXGI_FORMAT_R32G32B32A32_FLOAT,0,0};
    sprite.buffers[0]={reinterpret_cast<const uint8_t*>(uv),sizeof uv,16};
    sprite.elements[0]={DXGI_FORMAT_R32G32B32A32_FLOAT,1,0};
    sprite.buffers[1]={reinterpret_cast<const uint8_t*>(positions),sizeof positions,16};
    if (!openshao::UnitSpriteTexcoords(sprite)) return 1;
    for (auto& v:uv) { v[0]-=3; v[1]-=2; }
    if (!openshao::UnitSpriteTexcoords(sprite)) { std::fputs("FAIL: menu's single negative UV tile must be recognized\n",stderr); result=1; }
    for (auto& v:uv) { v[0]+=3; v[1]+=2; }
    const float animation_uv[3][4]={{-3.000000238418579f,-3.f,0,1},
        {-2.000021457672119f,-3.f,0,1},{-2.000021457672119f,-1.9998126029968262f,0,1}};
    std::memcpy(uv,animation_uv,sizeof uv);
    if (!openshao::UnitSpriteTexcoords(sprite)) { std::fputs("FAIL: one retail animation frame disables the sprite edge guard\n",stderr); result=1; }
    const float neutral_uv[3][4]={{0,0,0,1},{1,0,0,1},{1,1,0,1}};
    std::memcpy(uv,neutral_uv,sizeof uv);
    uv[0][0]=-1; if (openshao::UnitSpriteTexcoords(sprite)) return 1; uv[0][0]=0;
    uv[2][0]=3; if (openshao::UnitSpriteTexcoords(sprite)) return 1; uv[2][0]=1;
    uv[0][3]=.5f; if (openshao::UnitSpriteTexcoords(sprite)) return 1; uv[0][3]=1;
    positions[1][1]=4; if (openshao::UnitSpriteTexcoords(sprite)) return 1; positions[1][1]=0;
    sprite.pretransformed=false; if (openshao::UnitSpriteTexcoords(sprite)) return 1;
    for (bool screen : {true,false}) for (bool quad : {false,true}) {
        int code=RunCase(screen,quad);
        if (code) result=code;
    }
    for (bool screen : {true,false}) for (float scale : {1.5f,2.0f}) {
        int code=RunCase(screen,true,scale);
        if (code) result=code;
    }
    for (int fallback : {1,2,3,4}) {
        int code=RunCase(true,true,2,fallback);
        if (code) result=code;
    }
    for (float scale : {1.5f,2.f,2.25f}) for (int menu : {1,2,3}) {
        int code=RunCase(true,true,scale,0,menu);
        if (code) result=code;
    }
    for (float scale : {1.5f,2.f,2.1f,2.25f}) if (RunCase(true,true,scale,0,5)) result=1;
    for (float scale : {1.5f,2.f,2.1f,2.25f}) if (RunCase(true,true,scale,0,6)) result=1;
    for (float scale : {1.f,1.5f,2.f,2.25f}) if (RunCase(true,true,scale,0,7)) result=1;
    if (RunCase(true,true,2.25f,0,4)) result=1;
    if (!result) std::puts("PASS: menu FLOAT4 sprites clamp without wrapping; upscaled outer borders stay covered");
    return result;
}

"""Add checked font uploads to the pinned Dear ImGui Direct3D 12 backend."""
import argparse
from pathlib import Path

def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError('Pinned ImGui backend changed at: '+old[:80])
    return text.replace(old,new,1)

def prepare(source):
    source=replace_once(source,'#include "imgui_impl_dx12.h"','#include <backends/imgui_impl_dx12.h>')
    begin=source.index('static void ImGui_ImplDX12_CreateFontsTexture()')
    end=source.index('\nbool    ImGui_ImplDX12_CreateDeviceObjects()',begin)
    font=source[begin:end]
    font=replace_once(font,'static void ImGui_ImplDX12_CreateFontsTexture()',
                      'static bool ImGui_ImplDX12_CreateFontsTexture()')
    resources=[('ID3D12Resource','pTexture'),('ID3D12Resource','uploadBuffer'),
               ('ID3D12Fence','fence'),('ID3D12CommandQueue','cmdQueue'),
               ('ID3D12CommandAllocator','cmdAlloc'),('ID3D12GraphicsCommandList','cmdList')]
    for kind,name in resources:
        font=replace_once(font,f'        {kind}* {name} = nullptr;\n','')
        font=font.replace(f'{name}->Release();',f'SafeRelease({name});')
    owned=''.join(f'    {kind}* {name} = nullptr;\n' for kind,name in resources)
    owned+='''    HANDLE event = nullptr;
    const auto cleanup = [&](void*) {
        SafeRelease(cmdList); SafeRelease(cmdAlloc); SafeRelease(cmdQueue);
        SafeRelease(fence); SafeRelease(uploadBuffer); SafeRelease(pTexture);
        if (event) CloseHandle(event);
    };
    const std::unique_ptr<void, decltype(cleanup)> guard(reinterpret_cast<void*>(1), cleanup);
    const auto failed = [](HRESULT result, const char* operation) {
        if (SUCCEEDED(result)) return false;
        LOGE("Direct3D 12 UI: %s failed (HRESULT %08lX)", operation, result);
        return true;
    };
'''
    font=replace_once(font,'{\n    // Build texture atlas','{\n'+owned+'    // Build texture atlas')
    font=replace_once(font,'    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);',
        '''    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (!pixels || width <= 0 || height <= 0 || width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION) {
        LOGE("Direct3D 12 UI: invalid font atlas %dx%d", width, height);
        return false;
    }''')
    font=replace_once(font,'''        bd->pd3dDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pTexture));''',
        '''        if (failed(bd->pd3dDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pTexture)), "font texture allocation")) return false;''')
    if 'IM_ASSERT(SUCCEEDED(hr));' not in font:raise ValueError('Missing font upload HRESULT checks')
    font=font.replace('IM_ASSERT(SUCCEEDED(hr));','if (failed(hr, "font upload")) return false;')
    font=replace_once(font,'        HANDLE event = CreateEvent(0, 0, 0, 0);',
                      '        event = CreateEvent(0, 0, 0, 0);')
    font=replace_once(font,'        IM_ASSERT(event != nullptr);',
        '        if (!event) { LOGE("Direct3D 12 UI: font upload event failed (Windows error %lu)", GetLastError()); return false; }')
    font=replace_once(font,'        fence->SetEventOnCompletion(1, event);\n        WaitForSingleObject(event, INFINITE);',
        '''        if (failed(fence->SetEventOnCompletion(1, event), "font upload completion")) return false;
        if (WaitForSingleObject(event, 10000) != WAIT_OBJECT_0) {
            LOGE("Direct3D 12 UI: font upload timed out or failed (device HRESULT %08lX)", bd->pd3dDevice->GetDeviceRemovedReason());
            return false;
        }''')
    font=replace_once(font,'        CloseHandle(event);','        CloseHandle(event); event = nullptr;')
    font=replace_once(font,'        bd->pFontTextureResource = pTexture;',
                      '        bd->pFontTextureResource = pTexture; pTexture = nullptr;')
    font=replace_once(font,'    io.Fonts->SetTexID((ImTextureID)bd->hFontSrvGpuDescHandle.ptr);',
                      '    io.Fonts->SetTexID((ImTextureID)bd->hFontSrvGpuDescHandle.ptr);\n    return true;')
    source=source[:begin]+font+source[end:]
    source=replace_once(source,'    ImGui_ImplDX12_CreateFontsTexture();\n\n    return true;',
                        '    return ImGui_ImplDX12_CreateFontsTexture();')
    source=replace_once(source,'''        bd->pd3dDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&bd->pRootSignature));
        blob->Release();''',
        '''        const HRESULT root_result = bd->pd3dDevice->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&bd->pRootSignature));
        blob->Release();
        if (FAILED(root_result)) { LOGE("Direct3D 12 UI: root signature failed (HRESULT %08lX)", root_result); return false; }''')
    return '#include <memory>\n#include "core/log.h"\n'+source

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--source',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(prepare(args.source.read_text(encoding='utf-8')),encoding='utf-8')

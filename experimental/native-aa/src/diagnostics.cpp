#include "diagnostics.hpp"
#include <d3d11.h>
#include <dxgi.h>

namespace aot::postprocess {
namespace {
void describe(std::FILE* out, const char* name, ID3D11Resource* resource) {
    if (!resource) { std::fprintf(out, "%s=none\n", name); return; }
    ID3D11Texture2D* texture = nullptr;
    if (FAILED(resource->QueryInterface(IID_ID3D11Texture2D, reinterpret_cast<void**>(&texture)))) {
        std::fprintf(out, "%s=not_texture2d\n", name); return;
    }
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    std::fprintf(out, "%s=%ux%u format=%u samples=%u quality=%u mip_levels=%u bind=%u\n",
                 name, desc.Width, desc.Height, unsigned(desc.Format), desc.SampleDesc.Count,
                 desc.SampleDesc.Quality, desc.MipLevels, desc.BindFlags);
    texture->Release();
}
}
bool snapshot_targets(std::FILE* out, ID3D11DeviceContext* context,
                      IDXGISwapChain* swapchain, const char* marker) {
    if (!out || !context || !swapchain || !marker) return false;
    ID3D11Device* context_device = nullptr;
    ID3D11Device* swapchain_device = nullptr;
    context->GetDevice(&context_device);
    const HRESULT hr = swapchain->GetDevice(IID_ID3D11Device, reinterpret_cast<void**>(&swapchain_device));
    const bool same_device = SUCCEEDED(hr) && context_device && context_device == swapchain_device;
    std::fprintf(out, "marker=%s same_device=%d context_type=%u\n", marker, int(same_device), unsigned(context->GetType()));
    if (context_device) context_device->Release();
    if (swapchain_device) swapchain_device->Release();
    if (!same_device || context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) return false;
    DXGI_SWAP_CHAIN_DESC swap_desc{};
    const HRESULT desc_hr = swapchain->GetDesc(&swap_desc);
    if (SUCCEEDED(desc_hr))
        std::fprintf(out, "swapchain_desc=%ux%u format=%u samples=%u windowed=%d buffers=%u\n",
                     swap_desc.BufferDesc.Width, swap_desc.BufferDesc.Height, unsigned(swap_desc.BufferDesc.Format),
                     swap_desc.SampleDesc.Count, int(swap_desc.Windowed), swap_desc.BufferCount);
    else std::fprintf(out, "swapchain_desc_error=%08x\n", unsigned(desc_hr));
    ID3D11Texture2D* buffer = nullptr;
    const HRESULT buffer_hr = swapchain->GetBuffer(0, IID_ID3D11Texture2D, reinterpret_cast<void**>(&buffer));
    if (SUCCEEDED(buffer_hr)) {
        describe(out, "backbuffer", buffer); buffer->Release();
    } else std::fprintf(out, "backbuffer_error=%08x\n", unsigned(buffer_hr));
    ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    ID3D11DepthStencilView* depth = nullptr;
    context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, &depth);
    for (unsigned i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
        if (!targets[i]) continue;
        D3D11_RENDER_TARGET_VIEW_DESC view{}; targets[i]->GetDesc(&view);
        std::fprintf(out, "rtv%u view_format=%u dimension=%u\n", i, unsigned(view.Format), unsigned(view.ViewDimension));
        ID3D11Resource* resource = nullptr; targets[i]->GetResource(&resource);
        char name[16]; std::snprintf(name, sizeof name, "rtv%u", i);
        describe(out, name, resource);
        if (resource) resource->Release();
        targets[i]->Release();
    }
    if (depth) {
        ID3D11Resource* resource = nullptr; depth->GetResource(&resource);
        describe(out, "depth", resource);
        if (resource) resource->Release();
        depth->Release();
    }
    UINT count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
    context->RSGetViewports(&count, viewports);
    for (UINT i = 0; i < count; ++i)
        std::fprintf(out, "viewport%u=%g,%g %gx%g\n", i, double(viewports[i].TopLeftX), double(viewports[i].TopLeftY),
                     double(viewports[i].Width), double(viewports[i].Height));
    return SUCCEEDED(desc_hr) && SUCCEEDED(buffer_hr);
}
}

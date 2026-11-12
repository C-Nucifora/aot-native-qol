#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "renderer.hpp"
#include "constants.hpp"
#include <d3d11_1.h>
#include <cstring>
#include <new>
#include <utility>
#include "../third_party/smaa/AreaTex.h"
#include "../third_party/smaa/SearchTex.h"

namespace aot::postprocess {
namespace {
template<class T> struct Com {
    T* p = nullptr;
    ~Com() { reset(); }
    Com() = default;
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
    Com(Com&& v) noexcept : p(std::exchange(v.p, nullptr)) {}
    Com& operator=(Com&& v) noexcept { if (this != &v) { reset(); p = std::exchange(v.p, nullptr); } return *this; }
    void reset() { if (p) p->Release(); p = nullptr; }
    T** put() { reset(); return &p; }
    T* operator->() const { return p; }
};
struct Image { Com<ID3D11Texture2D> texture; Com<ID3D11ShaderResourceView> srv; Com<ID3D11RenderTargetView> rtv; };
bool image(ID3D11Device* d, UINT w, UINT h, DXGI_FORMAT f, Image& out, const void* data = nullptr, UINT pitch = 0) {
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w; td.Height = h; td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
    td.Format = f; td.Usage = data ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | (data ? 0 : D3D11_BIND_RENDER_TARGET);
    D3D11_SUBRESOURCE_DATA initial{data, pitch, 0};
    return SUCCEEDED(d->CreateTexture2D(&td, data ? &initial : nullptr, out.texture.put())) &&
           SUCCEEDED(d->CreateShaderResourceView(out.texture.p, nullptr, out.srv.put())) &&
           (data || SUCCEEDED(d->CreateRenderTargetView(out.texture.p, nullptr, out.rtv.put())));
}
bool bytecode(const Bytecode& b) { return b.data && b.size >= 32 && std::memcmp(b.data, "DXBC", 4) == 0; }
bool sampler_slots(const Bytecode& b, bool require_sampler) {
    return (!require_sampler || b.linear_sampler >= 0 || b.point_sampler >= 0) &&
           b.linear_sampler >= -1 && b.linear_sampler < 16 && b.point_sampler >= -1 && b.point_sampler < 16 &&
           (b.linear_sampler < 0 || b.point_sampler < 0 || b.linear_sampler != b.point_sampler);
}
struct Frame {
    UINT width = 0, height = 0; DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    Com<ID3D11Texture2D> scene;
    Com<ID3D11ShaderResourceView> encoded, linear;
    Image edges, weights, blended, sharpened;
};
struct StateScope {
    ID3D11DeviceContext1* context;
    Com<ID3DDeviceContextState> previous;
    StateScope(ID3D11DeviceContext1* c, ID3DDeviceContextState* state) : context(c) { if(c) c->SwapDeviceContextState(state, previous.put()); }
    ~StateScope() { if(context) { context->ClearState(); context->SwapDeviceContextState(previous.p, nullptr); } }
};
}
struct Renderer::Impl {
    const char* error = "not initialized";
    bool ready = false;
    Com<ID3D11Device> device;
    Com<ID3D11DeviceContext> recorder;
    Com<ID3DDeviceContextState> state;
    Com<ID3D11VertexShader> edge_vs, weight_vs, blend_vs, fullscreen_vs;
    Com<ID3D11PixelShader> edge_ps, weight_ps, blend_ps, copy_ps, cas_ps;
    Com<ID3D11Buffer> metrics, cas_constants, output_constants;
    Com<ID3D11SamplerState> linear_sampler, point_sampler;
    Com<ID3D11RasterizerState> rasterizer;
    Com<ID3D11DepthStencilState> depth;
    Image area, search;
    Frame frame;
    Bytecode edge_binding, weight_binding, blend_binding;
    bool fail(const char* message) { error = message; return false; }
    bool constant(UINT size, Com<ID3D11Buffer>& out) {
        D3D11_BUFFER_DESC bd{}; bd.ByteWidth = size; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        return SUCCEEDED(device->CreateBuffer(&bd, nullptr, out.put()));
    }
    void samplers(ID3D11DeviceContext* c, const Bytecode& b) {
        if (b.linear_sampler >= 0) c->PSSetSamplers(UINT(b.linear_sampler), 1, &linear_sampler.p);
        if (b.point_sampler >= 0) c->PSSetSamplers(UINT(b.point_sampler), 1, &point_sampler.p);
    }
    bool prepare(UINT w, UINT h, DXGI_FORMAT typeless, DXGI_FORMAT unorm, DXGI_FORMAT srgb) {
        if (frame.width == w && frame.height == h && frame.format == typeless) return true;
        Frame next; next.width = w; next.height = h; next.format = typeless;
        D3D11_TEXTURE2D_DESC td{}; td.Width = w; td.Height = h; td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
        td.Format = typeless; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&td, nullptr, next.scene.put()))) return fail("scene-copy creation failed");
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{}; sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1;
        sd.Format = unorm;
        if (FAILED(device->CreateShaderResourceView(next.scene.p, &sd, next.encoded.put()))) return fail("encoded scene view failed");
        sd.Format = srgb;
        if (FAILED(device->CreateShaderResourceView(next.scene.p, &sd, next.linear.put()))) return fail("linear scene view failed");
        if (!image(device.p, w, h, DXGI_FORMAT_R8G8_UNORM, next.edges) ||
            !image(device.p, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, next.weights) ||
            !image(device.p, w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, next.blended)) return fail("intermediate creation failed");
        frame = std::move(next); return true;
    }
};
Renderer::Renderer() noexcept : impl_(new(std::nothrow) Impl) {}
Renderer::~Renderer() = default;
const char* Renderer::last_error() const noexcept { return impl_ ? impl_->error : "allocation failed"; }
void Renderer::release() noexcept { impl_.reset(new(std::nothrow) Impl); }
bool Renderer::initialize(ID3D11Device* d, const ShaderPack& shaders) noexcept {
    release();
    if (!impl_ || !d) return false;
    auto& p = *impl_;
    d->AddRef(); p.device.p = d;
    Com<ID3D11Device1> d1;
    if (d->GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0 ||
        FAILED(d->QueryInterface(IID_ID3D11Device1, reinterpret_cast<void**>(d1.put())))) return p.fail("D3D11.1 state isolation unavailable");
    const auto level = d->GetFeatureLevel();
    const UINT state_flags = (d->GetCreationFlags() & D3D11_CREATE_DEVICE_SINGLETHREADED) ? D3D11_1_CREATE_DEVICE_CONTEXT_STATE_SINGLETHREADED : 0;
    if (FAILED(d1->CreateDeviceContextState(state_flags, &level, 1, D3D11_SDK_VERSION, IID_ID3D11Device, nullptr, p.state.put()))) return p.fail("isolated context state creation failed");
    const Bytecode* vs[] = {&shaders.edge_vs,&shaders.weight_vs,&shaders.blend_vs,&shaders.fullscreen_vs};
    Com<ID3D11VertexShader>* vo[] = {&p.edge_vs,&p.weight_vs,&p.blend_vs,&p.fullscreen_vs};
    for (unsigned i = 0; i < 4; ++i)
        if (!bytecode(*vs[i]) || FAILED(d->CreateVertexShader(vs[i]->data, vs[i]->size, nullptr, vo[i]->put()))) return p.fail("vertex DXBC rejected");
    const Bytecode* ps[] = {&shaders.edge_ps,&shaders.weight_ps,&shaders.blend_ps,&shaders.copy_ps,&shaders.cas_ps};
    Com<ID3D11PixelShader>* po[] = {&p.edge_ps,&p.weight_ps,&p.blend_ps,&p.copy_ps,&p.cas_ps};
    for (unsigned i = 0; i < 5; ++i)
        if (!bytecode(*ps[i]) || FAILED(d->CreatePixelShader(ps[i]->data, ps[i]->size, nullptr, po[i]->put()))) return p.fail("pixel DXBC rejected");
    if (!sampler_slots(shaders.edge_ps, true) || !sampler_slots(shaders.weight_ps, true) || !sampler_slots(shaders.blend_ps, true)) return p.fail("missing reflected sampler slots");
    p.edge_binding = shaders.edge_ps; p.weight_binding = shaders.weight_ps; p.blend_binding = shaders.blend_ps;
    // Bytecode is no longer needed after object creation; retain only binding metadata.
    p.edge_binding.data = p.weight_binding.data = p.blend_binding.data = nullptr;
    if (!p.constant(16,p.metrics) || !p.constant(48,p.cas_constants) || !p.constant(16,p.output_constants)) return p.fail("constant-buffer creation failed");
    D3D11_SAMPLER_DESC sd{}; sd.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; sd.MaxLOD = D3D11_FLOAT32_MAX; sd.MaxAnisotropy = 1;
    if (FAILED(d->CreateSamplerState(&sd,p.linear_sampler.put()))) return p.fail("linear sampler creation failed");
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    if (FAILED(d->CreateSamplerState(&sd,p.point_sampler.put()))) return p.fail("point sampler creation failed");
    D3D11_RASTERIZER_DESC rd{}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    if (FAILED(d->CreateRasterizerState(&rd,p.rasterizer.put()))) return p.fail("rasterizer creation failed");
    D3D11_DEPTH_STENCIL_DESC dd{}; dd.DepthEnable = FALSE; dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; dd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    if (FAILED(d->CreateDepthStencilState(&dd,p.depth.put()))) return p.fail("depth state creation failed");
    if (!image(d,AREATEX_WIDTH,AREATEX_HEIGHT,DXGI_FORMAT_R8G8_UNORM,p.area,areaTexBytes,AREATEX_PITCH) ||
        !image(d,SEARCHTEX_WIDTH,SEARCHTEX_HEIGHT,DXGI_FORMAT_R8_UNORM,p.search,searchTexBytes,SEARCHTEX_PITCH)) return p.fail("lookup upload failed");
    p.ready = true; p.error = nullptr; return true;
}
bool Renderer::render(ID3D11DeviceContext* c, ID3D11RenderTargetView* scene, Options options) noexcept {
    if (!impl_) return false;
    auto& p = *impl_;
    if (!p.ready || !p.device.p || !p.state.p || !c || !scene) return p.fail("renderer unavailable");
    if (!options.verified_srgb_scene) return p.fail("scene encoding not verified");
    const float sharpness = options.cas ? options.cas_sharpness : 0.15f;
    const auto constants = make_constants(1,1,sharpness);
    if (!constants) return p.fail("invalid CAS strength");
    ID3D11DeviceContext* destination = c;
    const bool deferred = c->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED;
    if (!deferred && c->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) return p.fail("unsupported context type");
    Com<ID3D11Device> actual; c->GetDevice(actual.put());
    if (actual.p != p.device.p || FAILED(p.device->GetDeviceRemovedReason())) return p.fail("device changed or removed");
    Com<ID3D11DeviceContext1> c1;
    if (!deferred && FAILED(c->QueryInterface(IID_ID3D11DeviceContext1,reinterpret_cast<void**>(c1.put())))) return p.fail("context state swapping unavailable");
    Com<ID3D11Resource> resource; scene->GetResource(resource.put());
    Com<ID3D11Texture2D> texture;
    if (!resource.p || FAILED(resource->QueryInterface(IID_ID3D11Texture2D,reinterpret_cast<void**>(texture.put())))) return p.fail("scene is not texture2d");
    Com<ID3D11Device> resource_device; texture->GetDevice(resource_device.put());
    if (resource_device.p != p.device.p) return p.fail("scene device mismatch");
    D3D11_TEXTURE2D_DESC td{}; texture->GetDesc(&td);
    D3D11_RENDER_TARGET_VIEW_DESC original{}; scene->GetDesc(&original);
    if (!td.Width || !td.Height || td.SampleDesc.Count != 1 || td.ArraySize != 1 || td.MipLevels != 1 ||
        original.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D || original.Texture2D.MipSlice != 0) return p.fail("unsupported scene layout or samples");
    DXGI_FORMAT typeless,unorm,srgb;
    switch (td.Format) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        typeless=DXGI_FORMAT_R8G8B8A8_TYPELESS; unorm=DXGI_FORMAT_R8G8B8A8_UNORM; srgb=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        typeless=DXGI_FORMAT_B8G8R8A8_TYPELESS; unorm=DXGI_FORMAT_B8G8R8A8_UNORM; srgb=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; break;
    default: return p.fail("unsupported scene format");
    }
    if (original.Format != unorm && original.Format != srgb) return p.fail("unsupported output view");
    if (!p.prepare(td.Width,td.Height,typeless,unorm,srgb)) return false;
    if (options.cas && !p.frame.sharpened.texture.p) {
        Image next;
        if (!image(p.device.p,td.Width,td.Height,DXGI_FORMAT_R16G16B16A16_FLOAT,next)) return p.fail("CAS target creation failed");
        p.frame.sharpened = std::move(next);
    }
    if (!options.cas) p.frame.sharpened = Image{};
    if (deferred) {
        if (!p.recorder.p && FAILED(p.device->CreateDeferredContext(0,p.recorder.put()))) return p.fail("private deferred context creation failed");
        c = p.recorder.p;
    }
    // All fallible preparation precedes copying back to the game's scene.
    // Deferred native state is never read/changed: only the private recorder is
    // configured, then ExecuteCommandList(TRUE) merges its self-contained list.
    const auto values = make_constants(td.Width,td.Height,sharpness);
    StateScope scope(c1.p,p.state.p);
    c->ClearState(); c->SetPredication(nullptr,FALSE);
    c->CopyResource(p.frame.scene.p,texture.p);
    c->UpdateSubresource(p.metrics.p,0,nullptr,values->smaa_metrics.data(),0,0);
    c->UpdateSubresource(p.cas_constants.p,0,nullptr,&values->cas,0,0);
    const std::uint32_t encode[4] = {original.Format == unorm ? 1u : 0u,0,0,0};
    c->UpdateSubresource(p.output_constants.p,0,nullptr,encode,0,0);
    D3D11_VIEWPORT vp{0,0,float(td.Width),float(td.Height),0,1};
    c->RSSetViewports(1,&vp); c->RSSetState(p.rasterizer.p);
    c->OMSetDepthStencilState(p.depth.p,0);
    c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    c->VSSetConstantBuffers(0,1,&p.metrics.p); c->PSSetConstantBuffers(0,1,&p.metrics.p);
    ID3D11ShaderResourceView* views[5]{};
    auto pass = [&](ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11RenderTargetView* output) {
        ID3D11ShaderResourceView* empty[5]{};
        c->PSSetShaderResources(0,5,empty); c->OMSetRenderTargets(1,&output,nullptr);
        c->PSSetShaderResources(0,5,views); c->VSSetShader(vs,nullptr,0); c->PSSetShader(ps,nullptr,0); c->Draw(3,0);
    };
    const float zero[4]{}; c->ClearRenderTargetView(p.frame.edges.rtv.p,zero);
    views[0]=p.frame.encoded.p; p.samplers(c,p.edge_binding); pass(p.edge_vs.p,p.edge_ps.p,p.frame.edges.rtv.p);
    views[0]=nullptr; views[1]=p.frame.edges.srv.p; views[2]=p.area.srv.p; views[3]=p.search.srv.p;
    p.samplers(c,p.weight_binding); pass(p.weight_vs.p,p.weight_ps.p,p.frame.weights.rtv.p);
    views[0]=p.frame.linear.p; views[1]=views[2]=views[3]=nullptr; views[4]=p.frame.weights.srv.p;
    p.samplers(c,p.blend_binding); pass(p.blend_vs.p,p.blend_ps.p,p.frame.blended.rtv.p);
    views[0]=p.frame.blended.srv.p; views[4]=nullptr;
    if (options.cas) {
        c->PSSetConstantBuffers(0,1,&p.cas_constants.p); pass(p.fullscreen_vs.p,p.cas_ps.p,p.frame.sharpened.rtv.p);
        views[0]=p.frame.sharpened.srv.p;
    }
    c->PSSetConstantBuffers(0,1,&p.output_constants.p); pass(p.fullscreen_vs.p,p.copy_ps.p,scene);
    if (deferred) {
        Com<ID3D11CommandList> list;
        if (FAILED(c->FinishCommandList(FALSE,list.put()))) {
            p.recorder.reset();
            return p.fail("private AA command-list finalization failed");
        }
        destination->ExecuteCommandList(list.p,TRUE);
    }
    p.error=nullptr; return true;
}
}

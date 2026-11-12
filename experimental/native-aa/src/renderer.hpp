#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
namespace aot::postprocess {
struct Bytecode {
    const void* data = nullptr;
    std::size_t size = 0;
    // Populate from shader reflection at build time. -1 means unused.
    int linear_sampler = -1;
    int point_sampler = -1;
};
struct ShaderPack {
    Bytecode edge_vs, edge_ps, weight_vs, weight_ps, blend_vs, blend_ps;
    Bytecode fullscreen_vs, copy_ps, cas_ps;
};
struct Options { bool verified_srgb_scene = false; bool cas = false; float cas_sharpness = 0.15f; };
// No hooks or timing patches. Call only at a verified scene/UI execution boundary.
// Supports single-sample full-size SDR RGBA8/BGRA8 scene textures only.
// Deferred callbacks merge a private self-contained command list with native
// context state restoration; no FinishCommandList is issued on the game context.
class Renderer {
public:
    Renderer() noexcept;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    bool initialize(ID3D11Device*, const ShaderPack&) noexcept;
    bool render(ID3D11DeviceContext*, ID3D11RenderTargetView*, Options = {}) noexcept;
    // Call on the render thread before resize/device teardown. No game resources retained.
    void release() noexcept;
    const char* last_error() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}

#pragma once
#include <cstdio>
struct ID3D11DeviceContext;
struct IDXGISwapChain;
namespace aot::postprocess {
// Borrowed live API pointers supplied by a verified render-thread callback.
// Getters only; this does not install hooks or establish callback ordering.
bool snapshot_targets(std::FILE* output, ID3D11DeviceContext* context,
                      IDXGISwapChain* swapchain, const char* marker);
}

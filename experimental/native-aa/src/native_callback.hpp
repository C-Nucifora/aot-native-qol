#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
struct ID3D11DeviceContext;
struct IDXGISwapChain;
namespace aot::postprocess {
// AoT's existing ordered callback-command ABI, x64 only. No hook addresses here.
struct NativeCallbackArguments { ID3D11DeviceContext* context; void* executor; };
using NativeCallback = void (CALLBACK*)(NativeCallbackArguments*, void*);
using NativeEnqueueCallback = void (WINAPI*)(void* manager, NativeCallback, void* user);
// Caller keeps this record, FILE and swapchain alive until queued callback finishes.
// Do not enqueue unless the parent has verified build, CPU insertion boundary,
// queue capacity/lifetime and the resolved emitter. This header installs nothing.
struct DiagnosticCallbackInput { std::FILE* output; IDXGISwapChain* swapchain; const char* marker; };
void CALLBACK diagnostic_callback(NativeCallbackArguments*, void*) noexcept;
}

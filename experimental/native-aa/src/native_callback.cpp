#include "native_callback.hpp"
#include "diagnostics.hpp"
#include <d3d11.h>
namespace aot::postprocess {
static_assert(sizeof(void*) == 8);
void CALLBACK diagnostic_callback(NativeCallbackArguments* args, void* opaque) noexcept {
    auto* input = static_cast<DiagnosticCallbackInput*>(opaque);
    if (!args || !args->context || !input || !input->output) return;
    std::fprintf(input->output,"ordered_callback context_type=%u executor=%p\n",
                 unsigned(args->context->GetType()),args->executor);
    // Deferred contexts do not support the required getters/state isolation.
    // Diagnostic refusal is explicit; do not silently switch to an overlay.
    if (args->context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) {
        std::fprintf(input->output,"scene_snapshot=refused_deferred_context\n");
        return;
    }
    snapshot_targets(input->output,args->context,input->swapchain,input->marker);
}
}

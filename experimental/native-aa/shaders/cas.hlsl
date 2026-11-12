// Original integration wrapper. Algorithm: third_party/cas.
// Input and output are display-referred linear, 0..1; no scaling.
#define A_GPU 1
#define A_HLSL 1
#include "../third_party/cas/ffx_a.h"
Texture2D<float4> scene : register(t0);
cbuffer CasConstants : register(b0) { uint4 const0; uint4 const1; uint2 extent; uint2 padding; };
AF3 CasLoad(ASU2 p) { return scene.Load(int3(clamp(p, int2(0, 0), int2(extent) - 1), 0)).rgb; }
void CasInput(inout AF1 r, inout AF1 g, inout AF1 b) {}
#include "../third_party/cas/ffx_cas.h"
float4 CasPS(float4 position : SV_Position) : SV_Target {
    float3 result;
    CasFilter(result.r, result.g, result.b, uint2(position.xy), const0, const1, true);
    return float4(result, scene.Load(int3(int2(position.xy), 0)).a);
}

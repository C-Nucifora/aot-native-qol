// Original integration wrapper. Algorithm and lookup data: third_party/smaa.
// Edge source uses an UNORM view of the tone-mapped encoded scene.
// Neighborhood source uses an SRGB view; output is display-referred linear.
cbuffer SmaaConstants : register(b0) { float4 metrics; };
#define SMAA_RT_METRICS metrics
#define SMAA_HLSL_4_1
#define SMAA_PRESET_ULTRA
#include "../third_party/smaa/SMAA.hlsl"

Texture2D scene : register(t0);
Texture2D edges : register(t1);
Texture2D area : register(t2);
Texture2D search : register(t3);
Texture2D weights : register(t4);

struct EdgeVertex { float4 position : SV_Position; float2 uv : TEXCOORD0; float4 offsets[3] : TEXCOORD1; };
struct WeightVertex { float4 position : SV_Position; float2 uv : TEXCOORD0; float2 pixel : TEXCOORD1; float4 offsets[3] : TEXCOORD2; };
struct BlendVertex { float4 position : SV_Position; float2 uv : TEXCOORD0; float4 offset : TEXCOORD1; };

float2 FullscreenUv(uint vertex) { return float2((vertex << 1) & 2, vertex & 2); }
float4 FullscreenPosition(float2 uv) { return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1); }

EdgeVertex EdgeVS(uint vertex : SV_VertexID) {
    EdgeVertex o;
    o.uv = FullscreenUv(vertex); o.position = FullscreenPosition(o.uv);
    SMAAEdgeDetectionVS(o.uv, o.offsets);
    return o;
}
float2 EdgePS(EdgeVertex i) : SV_Target { return SMAAColorEdgeDetectionPS(i.uv, i.offsets, scene); }
WeightVertex WeightVS(uint vertex : SV_VertexID) {
    WeightVertex o;
    o.uv = FullscreenUv(vertex); o.position = FullscreenPosition(o.uv);
    SMAABlendingWeightCalculationVS(o.uv, o.pixel, o.offsets);
    return o;
}
float4 WeightPS(WeightVertex i) : SV_Target {
    return SMAABlendingWeightCalculationPS(i.uv, i.pixel, i.offsets, edges, area, search, float4(0, 0, 0, 0));
}
BlendVertex BlendVS(uint vertex : SV_VertexID) {
    BlendVertex o;
    o.uv = FullscreenUv(vertex); o.position = FullscreenPosition(o.uv);
    SMAANeighborhoodBlendingVS(o.uv, o.offset);
    return o;
}
float4 BlendPS(BlendVertex i) : SV_Target {
    float4 value = SMAANeighborhoodBlendingPS(i.uv, i.offset, scene, weights);
    value.a = scene.Load(int3(int2(i.position.xy), 0)).a;
    return value;
}

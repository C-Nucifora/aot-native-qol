// Original final-copy shader. Encode only when the destination view is UNORM.
cbuffer OutputConstants : register(b0) { uint encode_srgb; uint3 padding; };
Texture2D<float4> scene : register(t0);
struct Vertex { float4 position : SV_Position; };
Vertex FullscreenVS(uint vertex : SV_VertexID) {
    float2 uv = float2((vertex << 1) & 2, vertex & 2);
    Vertex o; o.position = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1); return o;
}
float4 CopyPS(Vertex i) : SV_Target {
    float4 value = scene.Load(int3(int2(i.position.xy), 0));
    if (encode_srgb != 0) {
        float3 rgb = saturate(value.rgb);
        value.rgb = float3(rgb.r <= 0.0031308 ? 12.92 * rgb.r : 1.055 * pow(rgb.r, 1.0 / 2.4) - 0.055,
                           rgb.g <= 0.0031308 ? 12.92 * rgb.g : 1.055 * pow(rgb.g, 1.0 / 2.4) - 0.055,
                           rgb.b <= 0.0031308 ? 12.92 * rgb.b : 1.055 * pow(rgb.b, 1.0 / 2.4) - 0.055);
    }
    return value;
}

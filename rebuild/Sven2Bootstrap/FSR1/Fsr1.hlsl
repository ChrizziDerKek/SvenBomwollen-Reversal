// Integration glue for AMD's unmodified, MIT-licensed FSR1 FP32 implementation.
#define A_GPU 1
#define A_HLSL 1
#include "ffx_a.h"

Texture2D<float4> image : register(t0);
SamplerState imageSampler : register(s0);
cbuffer Settings : register(b0) {
    float2 outputSize;
    float sharpness;
    uint filterMode;
    float2 viewportOrigin;
    float2 padding;
};

#if defined(PASS_EASU)
#define FSR_EASU_F 1
AF4 FsrEasuRF(AF2 p) { return image.GatherRed(imageSampler,p); }
AF4 FsrEasuGF(AF2 p) { return image.GatherGreen(imageSampler,p); }
AF4 FsrEasuBF(AF2 p) { return image.GatherBlue(imageSampler,p); }
#endif
#if defined(PASS_RCAS)
#define FSR_RCAS_F 1
AF4 FsrRcasLoadF(ASU2 p) { return image.Load(int3(clamp(p,int2(0,0),int2(outputSize)-1),0)); }
void FsrRcasInputF(inout AF1 r,inout AF1 g,inout AF1 b) {}
#endif
#include "ffx_fsr1.h"

float4 vertex(uint id : SV_VertexID) : SV_Position {
    float2 p=float2((id<<1)&2,id&2);
    return float4(p*float2(2,-2)+float2(-1,1),0,1);
}

float4 pixel(float4 position : SV_Position) : SV_Target {
    uint2 p=uint2(position.xy-viewportOrigin);
#if defined(PASS_EASU)
    uint4 c0,c1,c2,c3;
    FsrEasuCon(c0,c1,c2,c3,800.0,600.0,800.0,600.0,outputSize.x,outputSize.y);
    float3 color;
    FsrEasuF(color,p,c0,c1,c2,c3);
    return float4(color,1);
#elif defined(PASS_RCAS)
    uint4 c;
    FsrRcasCon(c,sharpness);
    float3 color;
    FsrRcasF(color.r,color.g,color.b,p,c);
    return float4(saturate(color),1);
#else
    return float4(image.SampleLevel(imageSampler,(float2(p)+0.5)/outputSize,0).rgb,1);
#endif
}

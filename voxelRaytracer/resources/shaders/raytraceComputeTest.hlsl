RWTexture2D<float4> OutputTexture : register(u0);

Texture2D noiseTexture : register(t0);
StructuredBuffer<int> octreeData : register(t1);

cbuffer constantBuffer : register(b0)
{
    float4 maxThreadIter;
    
    float4 camPosition;
    float4 camDirection;
    
    float4 camUpperLeftCorner;
    float4 camPixelOffsetHorizontal;
    float4 camPixelOffsetVertical;
    
    int frameCount;
    int sampleCount;
}

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    float2 WindowLocal = ((float2) DTid.xy / maxThreadIter.xy);  
    
    float4 outColor = float4(0, 0, 0, 1);
    
    OutputTexture[DTid.xy] = outColor;
    return;
}
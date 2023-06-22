RWTexture2D<float4> OutputTexture : register(u0);

cbuffer constantBuffer : register(b0)
{
    float4 maxThreadIter;
    float4 fillColor;
}

[numthreads(8, 8, 1)]

void main( uint3 DTid : SV_DispatchThreadID )
{
    float2 WindowLocal = ((float2) DTid.xy / maxThreadIter.xy);
    
    OutputTexture[DTid.xy] = float4(WindowLocal, 0, 1);
}
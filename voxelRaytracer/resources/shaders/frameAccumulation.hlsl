RWTexture2D<float4> inputTexture : register(u0);
RWTexture2D<float4> OutputTexture : register(u1);

cbuffer constantBuffer : register(b0)
{
    int framesAccumulated;
    bool shouldAcummulate;
}

[numthreads(8, 4, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    OutputTexture[DTid.xy] = inputTexture[DTid.xy] / framesAccumulated;
    
    inputTexture[DTid.xy] = inputTexture[DTid.xy] * shouldAcummulate;
}
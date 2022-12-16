RWTexture2D<float4> OutputTexture : register(u0);

cbuffer constantBuffer : register(b0)
{
    float4 maxThreadIter;
    
    float4 camPosition;
    float4 camDirection;
    
    float4 camUpperLeftCorner;
    float4 camPixelOffsetHorizontal;
    float4 camPixelOffsetVertical;
}


struct RayStruct
{
    float3 origin;
    float3 direction;
};

RayStruct createRay(float2 windowPos);
float4 sampleRay(RayStruct aRay);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    float2 WindowLocal = ((float2) DTid.xy / maxThreadIter.xy);
    
    //float3 origin = (camUpperLeftCorner + camPixelOffsetHorizontal * WindowLocal.x + camPixelOffsetVertical * WindowLocal.y).xyz;
    //float3 direction = normalize(origin);
    //origin += camPosition;
    
    //OutputTexture[DTid.xy] = float4(direction, 1.0f);
    
    RayStruct myRay = createRay(WindowLocal);
    
    OutputTexture[DTid.xy] = sampleRay(myRay);
}

RayStruct createRay(float2 windowPos)
{
    RayStruct myRay;
    
    myRay.origin = (camUpperLeftCorner + camPixelOffsetHorizontal * windowPos.x + camPixelOffsetVertical * windowPos.y).xyz;
    myRay.direction = normalize(myRay.origin);
    myRay.origin += camPosition;
    
    return myRay;
}

float4 sampleRay(RayStruct aRay)
{
    
    return float4(aRay.direction, 1.0f);
}
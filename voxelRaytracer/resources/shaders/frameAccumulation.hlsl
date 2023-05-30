RWTexture2D<float4> inputTexture : register(u0);
RWTexture2D<float4> OutputTexture : register(u1);

cbuffer constantBuffer : register(b0)
{
    int framesAccumulated;
    bool shouldAcummulate;
}

float3 LessThan(float3 f, float value)
{
    return float3(
        (f.x < value) ? 1.0f : 0.0f,
        (f.y < value) ? 1.0f : 0.0f,
        (f.z < value) ? 1.0f : 0.0f);
}
 
float3 LinearToSRGB(float3 rgb)
{
    rgb = clamp(rgb, 0.0f, 1.0f);
     
    return lerp(
        pow(rgb, float3(1.0f / 2.4f, 1.0f / 2.4f, 1.0f / 2.4f)) * 1.055f - 0.055f,
        rgb * 12.92f,
        LessThan(rgb, 0.0031308f)
    );
}
 
float3 SRGBToLinear(float3 rgb)
{
    rgb = clamp(rgb, 0.0f, 1.0f);
    
    return lerp(
        pow(((rgb + 0.055f) / 1.055f), float3(2.4f, 2.4f, 2.4f)),
        rgb / 12.92f,
        LessThan(rgb, 0.04045f)
    );
}

// ACES tone mapping curve fit to go from HDR to LDR
//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

#define EXPOSURE 0.5f

[numthreads(8, 4, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    float3 outColor = (inputTexture[DTid.xy] / framesAccumulated).xyz;
    
    //exposure
    outColor = outColor * EXPOSURE;
    
    //tone mapping
    outColor = ACESFilm(outColor);
    
    //srgb
    outColor = LinearToSRGB(outColor);
       
    //frame accumulation
    OutputTexture[DTid.xy] = float4(outColor, 1.f);
    inputTexture[DTid.xy] = inputTexture[DTid.xy] * shouldAcummulate;
}
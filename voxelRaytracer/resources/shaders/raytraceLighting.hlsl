//#include "rayTraversal/DDA/DDATraversal.hlsl"
#include "rayTraversal/octree/octreeTraversal2stackless.hlsl"

RWTexture2D<float4> OutputTexture : register(u0);

StructuredBuffer<int> noiseTexture : register(t0);

cbuffer constantBuffer : register(b0)
{
    const float4 maxThreadIter;
    
    const float4 camPosition;
    const float4 camDirection;
    
    const float4 camUpperLeftCorner;
    const float4 camPixelOffsetHorizontal;
    const float4 camPixelOffsetVertical;
    
    const int frameSeed;
    const int sampleCount;
}

struct AtlasItem
{
    float4 colorAndRoughness;
    float4 specularAndPercent;
    
    int isLight;
};

StructuredBuffer<AtlasItem> voxelAtlas : register(t5);
//

Texture2D skydomeTexture : register(t6);
SamplerState skydomeSampler : register(s0);

#define eps 1./ 1080.f

#define FLOAT_MAX 3.402823466e+38F
#define INT_MAX 2147483647

#define CHUNK_SIZE_1 4
#define CHUNK_SIZE_2 4
#define TOP_LEVEL_SCALE 16

struct RandomOut
{
    float randomNum;
    int seed;
};

struct RandomState
{
    uint z0;
    uint z1;
    uint z2;
    uint z3;
};

struct BounceResult
{
    RayStruct bounceRay;
    float3 colorMultiplier;
};

//BounceResult bounceResultDefault()
//{
//    BounceResult result;
//    result.bounceRay = 
    
//    return result;
//}

float3 sampleSkydome(float3 aDirection);

BounceResult generateBounce(const float3 hitPoint, const float3 hitNormal, const AtlasItem aItem, const float3 incommingRayDirection, inout RandomState rs);

RayStruct createRay(const float2 windowPos);
RayStruct createRayAA(const float2 aWindowPos, const float2 aWindowSize, RandomState aRandomState);

int xorShift32(int aSeed);
int wangHash(int aSeed);

void updateRandom(inout RandomState rs);
float3 random1(RandomState rs);

RandomState initialize(int2 aDTid, int windowSizeX, int aFrameSeed);

float3 randomInUnitSphere(const float3 r);

#define RAY_BOUNCES 3

float3 LessThan(const float3 f, const float value)
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


[numthreads(8, 4, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const float2 WindowLocal = ((float2) DTid.xy / maxThreadIter.xy);
    
    RandomState rs = initialize(DTid.xy, maxThreadIter.x, frameSeed);
    
    RayStruct myRay = createRayAA(WindowLocal, maxThreadIter.xy, rs);
    
    float3 myOutColor = float3(1, 1, 1);
    
    bool bounceStopped = false;
    for (int i = 0; (i < RAY_BOUNCES) && !bounceStopped; i++)
    {
        const HitResult result = traverseRay(myRay);
        
        if (result.hitDistance != FLOAT_MAX && !(result.hitNormal.x == 0 && result.hitNormal.y == 0 && result.hitNormal.z == 0))
        {
            //hit voxel
            const AtlasItem myItem = voxelAtlas[result.itemIndex];
            
            const float3 myHitPoint = myRay.origin + myRay.direction * result.hitDistance;
            BounceResult myResult = generateBounce(myHitPoint, result.hitNormal, myItem, myRay.direction, rs);
            myRay = myResult.bounceRay;
            
            myOutColor *= myResult.colorMultiplier;
            
            //check if light
            if (myItem.isLight)
            {
                bounceStopped = true;
                continue;
            }
        }
        else
        {
            //hit nothing -> sample skyDome
            myOutColor *= (SRGBToLinear(sampleSkydome(myRay.direction)) * 2.f);
            bounceStopped = true;
        }
    }
    
    OutputTexture[DTid.xy] += float4(myOutColor, 1) * (int) bounceStopped;
}

#define PI 3.1415926535

float3 sampleSkydome(float3 aDirection)
{
    float2 uv = float2(atan2(aDirection.z, aDirection.x) / (2.0 * PI) + 0.5, acos(-aDirection.y) / PI);
    return skydomeTexture.SampleLevel(skydomeSampler, uv, 0).xyz;
}

RayStruct createRay(const float2 windowPos)
{
    RayStruct myRay;
    
    myRay.origin = (camUpperLeftCorner + camPixelOffsetHorizontal * windowPos.x + camPixelOffsetVertical * windowPos.y).xyz;
    myRay.direction = normalize(myRay.origin);
    myRay.origin += camPosition.xyz;
    
    //myRay.invDirection = 1.f / myRay.direction;
    
    myRay.rayDelta = 1. / max(abs(myRay.direction), eps);
    
    return myRay;
}

RayStruct createRayAA(const float2 aWindowPos, const float2 aWindowSize, RandomState aRandomState)
{
    RayStruct myRay;
    
    const float2 invSize = 1.f / aWindowSize;
    
    const float2 offset = frac(0.00002328 * float2(aRandomState.z0, aRandomState.z1)) * invSize - (0.5 * invSize);
    const float2 myWindowPos = aWindowPos + offset;
    
    myRay.origin = (camUpperLeftCorner + camPixelOffsetHorizontal * myWindowPos.x + camPixelOffsetVertical * myWindowPos.y).xyz;
    myRay.direction = normalize(myRay.origin);
    myRay.origin += camPosition.xyz;
    
    //myRay.invDirection = 1.f / myRay.direction;
    
    myRay.rayDelta = 1.f / max(abs(myRay.direction), eps);
    
    return myRay;
}

BounceResult generateBounce(const float3 hitPoint, const float3 hitNormal, const AtlasItem aItem, const float3 incommingRayDirection, inout RandomState rs)
{
    BounceResult myResult;
    
    myResult.bounceRay.origin = hitPoint;
    
    updateRandom(rs);
    
    // calculate whether we are going to do a diffuse or specular reflection ray 
    const float rand01 = float(rs.z0) / INT_MAX;
    const float doSpecular = (rand01 < aItem.specularAndPercent.w) ? 1.0f : 0.0f;
 
    //diffusion ray
    updateRandom(rs);
    const float3 random = random1(rs);

    const float3 diffuseRayDirection = normalize(hitNormal + randomInUnitSphere(random));
    
    //specular ray
    const float3 specularRayDirection = normalize(lerp(reflect(incommingRayDirection, hitNormal), diffuseRayDirection, aItem.colorAndRoughness.w * aItem.colorAndRoughness.w));
    
    //decide what ray to use
    myResult.bounceRay.direction = lerp(diffuseRayDirection, specularRayDirection, doSpecular);
    myResult.bounceRay.rayDelta = 1. / max(abs(myResult.bounceRay.direction), eps);
        
    // update the colorMultiplier
    myResult.colorMultiplier = lerp(aItem.colorAndRoughness.xyz, aItem.specularAndPercent.xyz, doSpecular);

    return myResult;
}

int xorShift32(int aSeed)
{
    int x = aSeed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

int wangHash(int aSeed)
{
    aSeed = (aSeed ^ 61) ^ (aSeed >> 16);
    aSeed *= 9;
    aSeed = aSeed ^ (aSeed >> 4);
    aSeed *= 0x27d4eb2d;
    aSeed = aSeed ^ (aSeed >> 15);
    return aSeed;
}

void updateRandom(inout RandomState rs)
{
    rs.z0 = xorShift32(rs.z0);
    rs.z1 = xorShift32(rs.z1);
    rs.z2 = xorShift32(rs.z2);
    rs.z3 = xorShift32(rs.z3);
}

float3 random1(RandomState rs)
{
    float3 result;
    
    result.x = frac(0.00002328 * float(rs.z0));
    result.y = frac(0.00002328 * float(rs.z1));
    result.z = frac(0.00002328 * float(rs.z2));
    
    return result;
}

float3 randomInUnitSphere(const float3 r)
{
    float3 p;
    p = 2.0 * r - float3(1.0, 1.0, 1.0);
    while (dot(p, p) > 1.0)
        p *= 0.7;
    return p;
}

RandomState initialize(int2 aDTid, int windowSizeX, int aFrameSeed)
{
    RandomState output;
        
    int random1 = noiseTexture[aDTid.x + aDTid.y * windowSizeX];
    int random2 = xorShift32(random1);
    int random3 = xorShift32(random2);
    int random4 = xorShift32(random3);
    
    output.z0 = random1 * 10000 + aFrameSeed;
    output.z1 = random2 * 10000 + aFrameSeed;
    output.z2 = random3 * 10000 + aFrameSeed;
    output.z3 = random4 * 10000 + aFrameSeed;
    
    return output;
}
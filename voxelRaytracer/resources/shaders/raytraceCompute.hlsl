RWTexture2D<float4> OutputTexture : register(u0);
Texture3D<uint> sceneData : register(t0);

cbuffer constantBuffer : register(b0)
{
    float4 maxThreadIter;
    
    float4 camPosition;
    float4 camDirection;
    
    float4 camUpperLeftCorner;
    float4 camPixelOffsetHorizontal;
    float4 camPixelOffsetVertical;

    float4 padding[10];
}

#define SIZE_X 32
#define SIZE_Y 32
#define SIZE_Z 32

#define VOXEL_SIZE 10
#define eps 1./ 1080.f
#define FAR 100

struct RayStruct
{
    float3 origin;
    float3 direction;
};

RayStruct createRay(float2 windowPos);
uint SampleScene(int3 aCoord);
float traverseVoxel(RayStruct aRay, float3 aDelta);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    float2 WindowLocal = ((float2) DTid.xy / maxThreadIter.xy);  
    RayStruct myRay = createRay(WindowLocal);
    
    float3 delta = 1. / max(abs(myRay.direction), eps);
    float currentDist = 0.f;
    
    for (int i = 0; i < FAR; i++)
    {
        float3 deltaPos = myRay.origin + myRay.direction * currentDist;
        int3 newVoxelCoord = int3(int(deltaPos.x), int(deltaPos.y), int(deltaPos.z));
        
        if (SampleScene(newVoxelCoord) > 0)
        {
            float brightness = (1 - (float) i / FAR) * 0.8 + 0.2;
            
            //OutputTexture[DTid.xy] = float4(1, 1, 1, 1);
            OutputTexture[DTid.xy] = float4(brightness, brightness, brightness, 1);
            return;
        }
        
        RayStruct tempRay;
        tempRay.origin = deltaPos;
        tempRay.direction = myRay.direction;
        
        currentDist += traverseVoxel(tempRay, delta);
    }
    
    OutputTexture[DTid.xy] = float4(myRay.direction, 1);
}

RayStruct createRay(float2 windowPos)
{
    RayStruct myRay;
    
    myRay.origin = (camUpperLeftCorner + camPixelOffsetHorizontal * windowPos.x + camPixelOffsetVertical * windowPos.y).xyz;
    myRay.direction = normalize(myRay.origin);
    myRay.origin += camPosition;
    
    return myRay;
}

uint SampleScene(int3 aCoord)
{
    
    if ((aCoord.x < 0 || aCoord.x > 32) || (aCoord.y < 0 || aCoord.y > 32) || (aCoord.z < 0 || aCoord.z > 32))
    {
        return 0;
    }

    return sceneData.Load(int4(aCoord, 0));
}

float traverseVoxel(RayStruct aRay, float3 aDelta)
{
    float3 t;
    
    // get x dist
    t.x = aRay.direction.x < 0. ? (aRay.origin.x - floor(aRay.origin.x)) * aDelta.x
                    : (ceil(aRay.origin.x) - aRay.origin.x) * aDelta.x;
                    
    // get y dist
    t.y = aRay.direction.y < 0. ? (aRay.origin.y - floor(aRay.origin.y)) * aDelta.y
                    : (ceil(aRay.origin.y) - aRay.origin.y) * aDelta.y;
    
    // get z dist
    t.z = aRay.direction.z < 0. ? (aRay.origin.z - floor(aRay.origin.z)) * aDelta.z
                    : (ceil(aRay.origin.z) - aRay.origin.z) * aDelta.z;

    return min(t.x, min(t.y, t.z)) + 0.001f;
}
RWTexture2D<float4> OutputTexture : register(u0);

Texture2D noiseTexture : register(t0);

cbuffer constantBuffer : register(b0)
{
    float4 maxThreadIter;
    
    float4 camPosition;
    float4 camDirection;
    
    float4 camUpperLeftCorner;
    float4 camPixelOffsetHorizontal;
    float4 camPixelOffsetVertical;
    
    int frameSeed;
    int sampleCount;
}

cbuffer voxelGridConstantBuffer : register(b2)
{
    int sizeX;
    int sizeY;
    int sizeZ;

    int layer1ChunkSize;
    int layer2ChunkSize;
}

// voxel grid structures
struct GridItem
{
    float filled;
    float3 color;
};

GridItem itemDefault()
{
    GridItem result;
    result.color = float3(0, 0, 0);
    
    return result;
}

struct Layer2Chunk
{
    GridItem items[4 * 4 * 4];
};

struct Layer1Chunk
{
    int itemIndices[4 * 4 * 4];
};

StructuredBuffer<int> topLevelGrid : register(t2);
StructuredBuffer<Layer1Chunk> Level1Grid : register(t3);
StructuredBuffer<Layer2Chunk> Level2Grid : register(t4);
//

#define eps 1./ 1080.f
#define FLOAT_MAX 3.402823466e+38F

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

struct RayStruct
{
    float3 origin;
    float3 direction;
    
    float3 rayDelta;
};

struct HitResult
{
    float hitDistance;
    float3 hitNormal;
    GridItem item;
    
    int loopCount;
};

HitResult hitResultDefault()
{
    HitResult result;
    result.hitDistance = FLOAT_MAX;
    result.loopCount = 0;
    result.hitNormal = float3(0, 0, 0);
    result.item = itemDefault();
    return result;
}

//RayStruct generateDiffuseRay(float3 hitPosition, float3 hitNormal, int aSeed);

HitResult traverseRay(RayStruct aRay);

RayStruct createRay(const float2 windowPos);
RayStruct createRayAA(const float2 aWindowPos, const float2 aWindowSize, RandomState aRandomState);

float3 reflectionRay(float3 aDirectionIn, float3 aNormal);

int xorShift32(int aSeed);
int wangHash(int aSeed);

void updateRandom(inout RandomState rs);
float3 random1(RandomState rs);

RandomState initialize(int2 aDTid, int aFrameSeed);

float3 randomInUnitSphere(float3 r);

[numthreads(8, 4, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    const float2 WindowLocal = ((float2) DTid.xy / maxThreadIter.xy);
    //const RandomState rs = initialize(DTid.xy, frameSeed);
    
    //const RayStruct myRay = createRayAA(WindowLocal, maxThreadIter.xy, rs);
    const RayStruct myRay = createRay(WindowLocal);
    
    const HitResult result = traverseRay(myRay);

    if (result.hitDistance != FLOAT_MAX)
    {
        const float brightness = (result.hitDistance / 200.f);
        OutputTexture[DTid.xy] += float4(brightness, brightness, brightness, 1);
    }
    else
    {
        OutputTexture[DTid.xy] += float4(result.item.color, 1);
    }
}

float2 intersectAABB(RayStruct aRay, float3 boxMin, float3 boxMax)
{
    float3 tMin = (boxMin - aRay.origin) / aRay.direction;
    float3 tMax = (boxMax - aRay.origin) / aRay.direction;
    float3 t1 = min(tMin, tMax);
    float3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    return float2(tNear, tFar);
};

struct chunkTraverseResult
{
    HitResult hit;
    int loopCount;
};

HitResult traverseGrid(const RayStruct aRay);
chunkTraverseResult traverseChunk(const RayStruct aRay, const int3 aMinBounds, const int aChunkIndex, const int3 aStepDirection);

HitResult traverseTopLevel(const RayStruct aRay);
chunkTraverseResult traverseLevel1(const RayStruct aRay, const int3 aMinBounds, const int aChunkIndex, const int3 aStepDirection);
chunkTraverseResult traverseLevel2(const RayStruct aRay, const int3 aMinBounds, const int aChunkIndex, const int3 aStepDirection);

HitResult traverseRay(RayStruct aRay)
{
    float2 result = intersectAABB(aRay, float3(0, 0, 0), float3(sizeX, sizeY, sizeZ));
    
    if (result.x < result.y && result.y > 0)
    {
        float3 intersectPos = aRay.origin;
        float RayOriginOffset = 0.f;
        
        if (result.x > 0)
        {
            RayOriginOffset += result.x + 0.01f;
            intersectPos += aRay.direction * (result.x + 0.01f);
        }
        
        aRay.origin = intersectPos;
        
        HitResult hit = traverseTopLevel(aRay);
        hit.hitDistance += RayOriginOffset;
        return hit;
    }
   
    return hitResultDefault();
}


int sampleTopLevelGrid(const int aX, const int aY, const int aZ)
{
    int countX = sizeX / (layer1ChunkSize * layer2ChunkSize);
    int countY = sizeY / (layer1ChunkSize * layer2ChunkSize);
    //int countZ = sizeZ / (layer1ChunkSize * layer2ChunkSize);
    
    return topLevelGrid[aX + (aY * countX) + (aZ * countX * countY)];
}

int sampleLevel1Grid(const int aIndex, const int aX, const int aY, const int aZ)
{
    return Level1Grid[aIndex].itemIndices[aX + (aY * 4) + (aZ * 4 * 4)]; // replace constants
}

GridItem sampleLevel2Grid(const int aIndex, const int aX, const int aY, const int aZ)
{
    return Level2Grid[aIndex].items[aX + (aY * 4) + (aZ * 4 * 4)]; // replace constants
}

HitResult traverseTopLevel(const RayStruct aRay)
{
    int countX = sizeX / (layer1ChunkSize * layer2ChunkSize);
    int countY = sizeY / (layer1ChunkSize * layer2ChunkSize);
    int countZ = sizeZ / (layer1ChunkSize * layer2ChunkSize);
    
    int scale = layer1ChunkSize * layer2ChunkSize;
    
    float3 scaledDelta = scale / aRay.direction;

    int3 minBounds = int3(0, 0, 0);
    int3 currentIndex = floor((aRay.origin - minBounds) / scale);

    //get x dist    
    float3 tMax = float3(0, 0, 0);
    
    tMax.x = (abs(aRay.origin.x / scale - floor(aRay.origin.x / scale) - (aRay.direction.x > 0.)) * scale) * aRay.rayDelta.x;
    
    //get y dist    
    tMax.y = (abs(aRay.origin.y / scale - floor(aRay.origin.y / scale) - (aRay.direction.y > 0.)) * scale) * aRay.rayDelta.y;
    
    //get z dist    
    tMax.z = (abs(aRay.origin.z / scale - floor(aRay.origin.z / scale) - (aRay.direction.z > 0.)) * scale) * aRay.rayDelta.z;

    int3 stepDirection = int3(0, 0, 0);
    
    stepDirection.x = (aRay.direction.x > 0) - (aRay.direction.x < 0);
    stepDirection.y = (aRay.direction.y > 0) - (aRay.direction.y < 0);
    stepDirection.z = (aRay.direction.z > 0) - (aRay.direction.z < 0);
        
    float distance = 0.f;
    int loop = 0;
    bool done = false;
    while (!done)
    {
        loop++;
        
        //if (loop > 100)
        //{
        //    HitResult myResult = hitResultDefault();
        //    myResult.item.color = float3(0, 1, 0);
        //    return myResult;
        //}
        
        if (currentIndex.x >= countX || currentIndex.x < 0 ||
            currentIndex.y >= countY || currentIndex.y < 0 ||
            currentIndex.z >= countZ || currentIndex.z < 0)
        {
            HitResult myResult = hitResultDefault();
            myResult.item.color = float3(loop, loop, loop) / 100.f;
            return myResult;
        }
        
        int myIndex = sampleTopLevelGrid(currentIndex.x, currentIndex.y, currentIndex.z);
        if (myIndex != -1)
        {
            RayStruct myRay;
            myRay.origin = aRay.origin + aRay.direction * (distance + 0.01f);
            myRay.direction = aRay.direction;
            myRay.rayDelta = aRay.rayDelta;
            
            int3 myBounds = minBounds + currentIndex * (layer1ChunkSize * layer2ChunkSize);

            chunkTraverseResult myResult = traverseLevel1(myRay, myBounds, myIndex, stepDirection);
            loop += myResult.loopCount;
            
            if (myResult.hit.hitDistance != FLOAT_MAX)
            {
                myResult.hit.item.color = float3(1, 1, 1);
                myResult.hit.hitDistance += distance;
                return myResult.hit;
            }
        }

        if (tMax.x < tMax.y)
        {
            if (tMax.x < tMax.z)
            {
                // X
                distance = tMax.x;
                tMax.x = tMax.x + scaledDelta.x * stepDirection.x;
                currentIndex.x += stepDirection.x;
            }
            else
            {
                // Z
                distance = tMax.z;
                tMax.z = tMax.z + scaledDelta.z * stepDirection.z;
                currentIndex.z += stepDirection.z;
            }
        }
        else
        {
            if (tMax.y < tMax.z)
            {
                // Y
                distance = tMax.y;
                tMax.y = tMax.y + scaledDelta.y * stepDirection.y;
                currentIndex.y += stepDirection.y;
            }
            else
            {
                // Z
                distance = tMax.z;
                tMax.z = tMax.z + scaledDelta.z * stepDirection.z;
                currentIndex.z += stepDirection.z;
            }
        }
    }
    
    HitResult myResult = hitResultDefault();
    return myResult;
}

chunkTraverseResult traverseLevel1(const RayStruct aRay, const int3 aMinBounds, const int aChunkIndex, const int3 aStepDirection)
{
    int scale = layer2ChunkSize;
    
    float3 scaledDelta = scale / aRay.direction;

    int3 currentIndex = floor((aRay.origin - aMinBounds) / scale);

    //get x dist    
    float3 tMax = float3(0, 0, 0);
    
    tMax.x = (abs(aRay.origin.x / scale - floor(aRay.origin.x / scale) - (aRay.direction.x > 0.)) * scale) * aRay.rayDelta.x;
    
    //get y dist    
    tMax.y = (abs(aRay.origin.y / scale - floor(aRay.origin.y / scale) - (aRay.direction.y > 0.)) * scale) * aRay.rayDelta.y;
    
    //get z dist    
    tMax.z = (abs(aRay.origin.z / scale - floor(aRay.origin.z / scale) - (aRay.direction.z > 0.)) * scale) * aRay.rayDelta.z;
     
    float distance = 0.f;
    int loop = 0;
    bool done = false;
    while (!done)
    {
        loop++;
        
        //if (loop > 100)
        //{
        //    HitResult myResult = hitResultDefault();
        //    myResult.item.color = float3(0, 1, 0);
        //    return myResult;
        //}
        
        if (currentIndex.x >= layer1ChunkSize || currentIndex.x < 0 ||
            currentIndex.y >= layer1ChunkSize || currentIndex.y < 0 ||
            currentIndex.z >= layer1ChunkSize || currentIndex.z < 0)
        {
            chunkTraverseResult myResult;
            myResult.loopCount = loop;
            
            myResult.hit = hitResultDefault();
            myResult.hit.item.color = float3(loop, loop, loop) / 100.f;
            return myResult;
        }
        
        int myIndex = sampleLevel1Grid(aChunkIndex, currentIndex.x, currentIndex.y, currentIndex.z);
        if (myIndex != -1)
        {
            RayStruct myRay;
            myRay.origin = aRay.origin + aRay.direction * (distance + 0.01f);
            myRay.direction = aRay.direction;
            myRay.rayDelta = aRay.rayDelta;
            
            int3 myBounds = aMinBounds + currentIndex * layer2ChunkSize;

            chunkTraverseResult myResult = traverseLevel2(myRay, myBounds, myIndex, aStepDirection);
            loop += myResult.loopCount;
            
            if (myResult.hit.hitDistance != FLOAT_MAX)
            {
                myResult.loopCount = loop;
                
                myResult.hit.item.color = float3(1, 1, 1);
                myResult.hit.hitDistance += distance;
                return myResult;
            }
        }

        if (tMax.x < tMax.y)
        {
            if (tMax.x < tMax.z)
            {
                // X
                distance = tMax.x;
                tMax.x = tMax.x + scaledDelta.x * aStepDirection.x;
                currentIndex.x += aStepDirection.x;
            }
            else
            {
                // Z
                distance = tMax.z;
                tMax.z = tMax.z + scaledDelta.z * aStepDirection.z;
                currentIndex.z += aStepDirection.z;
            }
        }
        else
        {
            if (tMax.y < tMax.z)
            {
                // Y
                distance = tMax.y;
                tMax.y = tMax.y + scaledDelta.y * aStepDirection.y;
                currentIndex.y += aStepDirection.y;
            }
            else
            {
                // Z
                distance = tMax.z;
                tMax.z = tMax.z + scaledDelta.z * aStepDirection.z;
                currentIndex.z += aStepDirection.z;
            }
        }
    }
    
    chunkTraverseResult myResult;
    myResult.loopCount = loop;
    myResult.hit = hitResultDefault();
    return myResult;
}

chunkTraverseResult traverseLevel2(const RayStruct aRay, const int3 aMinBounds, const int aChunkIndex, const int3 aStepDirection)
{
    float3 aDelta = 1.f / aRay.direction;
    
    int3 currentIndex = floor((aRay.origin - aMinBounds) / 1);

    //get x dist    
    float3 tMax = float3(0, 0, 0);
    
    tMax.x = abs(aRay.origin.x - floor(aRay.origin.x) - (aRay.direction.x > 0.)) * aRay.rayDelta.x;
    
    //get y dist    
    tMax.y = abs(aRay.origin.y - floor(aRay.origin.y) - (aRay.direction.y > 0.)) * aRay.rayDelta.y;
    
    //get z dist    
    tMax.z = abs(aRay.origin.z - floor(aRay.origin.z) - (aRay.direction.z > 0.)) * aRay.rayDelta.z;

    float distance = 0.f;
    int loop = 0;
    bool done = false;
    while (!done)
    {
        loop++;
        
        if (currentIndex.x >= layer2ChunkSize || currentIndex.x < 0 ||
            currentIndex.y >= layer2ChunkSize || currentIndex.y < 0 ||
            currentIndex.z >= layer2ChunkSize || currentIndex.z < 0)
        {
            chunkTraverseResult myResult;
            myResult.loopCount = loop;
            
            myResult.hit = hitResultDefault();
            myResult.hit.item.color = float3(loop, loop, loop) / 100.f;
            return myResult;
        }
       
        float myFilled = sampleLevel2Grid(aChunkIndex, currentIndex.x, currentIndex.y, currentIndex.z).filled;
        if (myFilled > 0)
        {
            chunkTraverseResult myResult;
            myResult.loopCount = loop;
            
            myResult.hit = hitResultDefault();
            myResult.hit.item.color = float3(1, 1, 1);
            myResult.hit.hitDistance = distance;
            return myResult;
        }

        if (tMax.x < tMax.y)
        {
            if (tMax.x < tMax.z)
            {
                // X
                distance = tMax.x;
                tMax.x = tMax.x + aDelta.x * aStepDirection.x;
                currentIndex.x += aStepDirection.x;
            }
            else
            {
                // Z
                distance = tMax.z;
                tMax.z = tMax.z + aDelta.z * aStepDirection.z;
                currentIndex.z += aStepDirection.z;
            }
        }
        else
        {
            if (tMax.y < tMax.z)
            {
                // Y
                distance = tMax.y;
                tMax.y = tMax.y + aDelta.y * aStepDirection.y;
                currentIndex.y += aStepDirection.y;
            }
            else
            {
                // Z
                distance = tMax.z;
                tMax.z = tMax.z + aDelta.z * aStepDirection.z;
                currentIndex.z += aStepDirection.z;
            }
        }
    }
    
    chunkTraverseResult myResult;
    myResult.loopCount = loop;
    
    myResult.hit = hitResultDefault();
    return myResult;
}

RayStruct createRay(const float2 windowPos)
{
    RayStruct myRay;
    
    myRay.origin = (camUpperLeftCorner + camPixelOffsetHorizontal * windowPos.x + camPixelOffsetVertical * windowPos.y).xyz;
    myRay.direction = normalize(myRay.origin);
    myRay.origin += camPosition.xyz;
    
    myRay.rayDelta = 1. / max(abs(myRay.direction), eps);
    
    return myRay;
}

RayStruct createRayAA(const float2 aWindowPos, const float2 aWindowSize, RandomState aRandomState)
{
    RayStruct myRay;
    
    float2 invSize = 1.f / aWindowSize;
    
    float2 offset = frac(0.00002328 * float2(aRandomState.z0, aRandomState.z1)) * invSize.x - (0.5 * invSize.x);
    float2 myWindowPos = aWindowPos + offset;
    
    myRay.origin = (camUpperLeftCorner + camPixelOffsetHorizontal * myWindowPos.x + camPixelOffsetVertical * myWindowPos.y).xyz;
    myRay.direction = normalize(myRay.origin);
    myRay.origin += camPosition.xyz;
    
    myRay.rayDelta = 1. / max(abs(myRay.direction), eps);
    
    return myRay;
}

float3 reflectionRay(float3 aDirectionIn, float3 aNormal)
{
    float3 reflectionRayDir = aDirectionIn - (aNormal * 2 * dot(aDirectionIn, aNormal));
    return normalize(reflectionRayDir);
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

float3 randomInUnitSphere(float3 r)
{
    float3 p;
    p = 2.0 * r - float3(1.0, 1.0, 1.0);
    while (dot(p, p) > 1.0)
        p *= 0.7;
    return p;
}

RandomState initialize(int2 aDTid, int aFrameSeed)
{
    RandomState output;
    
    int width, height, numLevels;
    noiseTexture.GetDimensions(0, width, height, numLevels);
    
    float4 random = noiseTexture.Load(int3((aDTid.xy) % int2(width, height), 0));
    
    output.z0 = random.x * 1000000 + aFrameSeed;
    output.z1 = random.y * 1000000 + aFrameSeed;
    output.z2 = random.z * 1000000 + aFrameSeed;
    output.z3 = random.w * 1000000 + aFrameSeed;
    
    return output;
}
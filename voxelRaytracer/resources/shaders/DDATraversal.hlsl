RWTexture2D<float4> OutputTexture : register(u0);

Texture2D noiseTexture : register(t0);

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

cbuffer voxelGridConstantBuffer : register(b2)
{
    const uint4 voxelGridSize;
    const uint4 topLevelChunkSize;
    
    const uint voxelAtlasOffset;
}

// voxel grid structures

// grid item
// 8 bit index to atlas (0 == not filled)
struct Layer2Chunk
{
    int items[4 * 4];
};

struct Layer1Chunk
{
    int itemIndices[4 * 4 * 4];
};

struct VoxelItem
{
    float4 color;
};

StructuredBuffer<int> topLevelGrid : register(t2);
StructuredBuffer<Layer1Chunk> Level1Grid : register(t3);
StructuredBuffer<Layer2Chunk> Level2Grid : register(t4);

struct AtlasItem
{
    float3 color;
    float padding;
    
    int isLight;
};

StructuredBuffer<AtlasItem> voxelAtlas : register(t5);
//

Texture2D skydomeTexture : register(t6);
SamplerState skydomeSampler : register(s0);

#define eps 1./ 1080.f
#define FLOAT_MAX 3.402823466e+38F

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

struct RayStruct
{
    float3 origin;
    float3 direction;
    
    //float3 invDirection;
    
    float3 rayDelta;
};

struct HitResult
{
    float hitDistance;
    float3 hitNormal;
    int itemIndex;
    
    int loopCount;
};

HitResult hitResultDefault()
{
    HitResult result;
    result.hitDistance = FLOAT_MAX;
    result.loopCount = 0;
    result.hitNormal = float3(0, 0, 0);
    result.itemIndex = 0;
    return result;
}

float3 sampleSkydome(float3 aDirection);

RayStruct generateDiffuseRay(float3 hitPoint, float3 hitNormal, inout RandomState rs);

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

#define RAY_BOUNCES 2

[numthreads(8, 4, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const float2 WindowLocal = ((float2) DTid.xy / maxThreadIter.xy);
    
    RandomState rs = initialize(DTid.xy, frameSeed);
    RayStruct myRay = createRayAA(WindowLocal, maxThreadIter.xy, rs);
    
    float3 myOutColor = float3(1, 1, 1);
    
    bool bounceStopped = false;
    for (int i = 0; (i < RAY_BOUNCES) && !bounceStopped; i++)
    {
        const HitResult result = traverseRay(myRay);
        
        if (result.hitDistance != FLOAT_MAX && !(result.hitNormal.x == 0 && result.hitNormal.y == 0 && result.hitNormal.z == 0))
        {
            //hit voxel

            //OutputTexture[DTid.xy] += float4(result.hitNormal, 1);
            //return;
            
            const AtlasItem myItem = voxelAtlas[result.itemIndex - 1];
            
            myOutColor *= myItem.color;
            
            //check if light
            if (myItem.isLight)
            {
                bounceStopped = true;
                continue;
            }
            
            float3 myHitPoint = myRay.origin + myRay.direction * result.hitDistance;
            myRay = generateDiffuseRay(myHitPoint, result.hitNormal, rs);
        }
        else
        {
            //hit nothing -> sample skyDome
            myOutColor *= sampleSkydome(myRay.direction);
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

struct AABBResult
{
    float2 intersectDists;
    float3 intersectNormal;
};

float2 intersectAABB(const RayStruct aRay, const float3 boxMin, const float3 boxMax)
{
    const float3 tMin = (boxMin - aRay.origin) / aRay.direction;
    const float3 tMax = (boxMax - aRay.origin) / aRay.direction;
    
    const float3 t1 = min(tMin, tMax);
    const float3 t2 = max(tMin, tMax);
    const float tNear = max(max(t1.x, t1.y), t1.z);
    const float tFar = min(min(t2.x, t2.y), t2.z);
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
chunkTraverseResult traverseLevel1(const RayStruct aRay, const int3 aMinBounds, const int aChunkIndex, int aData);
chunkTraverseResult traverseLevel2(const RayStruct aRay, const int3 aMinBounds, const int aChunkIndex, int aData);

HitResult traverseRay(RayStruct aRay)
{
    const float2 result = intersectAABB(aRay, float3(0, 0, 0), voxelGridSize.xyz);
    
    if (result.x < result.y && result.y > 0)
    {
        const float RayOriginOffset = max(result.x + 0.0001f, 0.f);
        const float3 intersectPos = aRay.origin + aRay.direction * RayOriginOffset;
        
        aRay.origin = intersectPos;
        
        HitResult hit = traverseTopLevel(aRay);

        hit.hitDistance += RayOriginOffset;
        return hit;
    }
   
    return hitResultDefault();
}

//bit values

// x bits for padding

// 3 bits for z index
// 3 bits for y index
// 3 bits for x index

// 1 bit for normal z
// 1 bit for normal y
// 1 bit for normal x

// 2 bits for dir Z
// 2 bits for dir Y
// 2 bits for dir X

#define GET_DIR_X(value) ((value) & 0x3) 
#define GET_DIR_Y(value) ((value >> 2) & 0x3) 
#define GET_DIR_Z(value) ((value >> 4) & 0x3) 

#define GET_NORMAL_X(value) ((value >> 6) & 0x1) 
#define GET_NORMAL_Y(value) ((value >> 7) & 0x1)  
#define GET_NORMAL_Z(value) ((value >> 8) & 0x1) 

#define GET_INDEX_X(value) ((value >> 9) & 0x7) 
#define GET_INDEX_Y(value) ((value >> 12) & 0x7)  
#define GET_INDEX_Z(value) ((value >> 15) & 0x7) 

#define OFFSET_DIR_X(value) (value & 0x3) 
#define OFFSET_DIR_Y(value) ((value & 0x3) << 2) 
#define OFFSET_DIR_Z(value) ((value & 0x3) << 4) 

#define OFFSET_NORMAL_X(value) ((value & 0x1) << 6)
#define OFFSET_NORMAL_Y(value) ((value & 0x1) << 7)  
#define OFFSET_NORMAL_Z(value) ((value & 0x1) << 8) 

#define OFFSET_INDEX_X(value) ((value & 0x7) << 9)
#define OFFSET_INDEX_Y(value) ((value & 0x7) << 12)  
#define OFFSET_INDEX_Z(value) ((value & 0x7) << 15) 

#define INDEX_FLAG(value) (value & (0x1FF << 9)) 
#define NORMAL_FLAG(value) (value & (0x7 << 6)) 
#define DIR_FLAG(value) (value & 0x3F) 

#define LEVEL2_EXIT_FLAG 0x00024800
#define LEVEL1_EXIT_FLAG 0x00024800

int sampleTopLevelGrid(const int aX, const int aY, const int aZ)
{
    return topLevelGrid[aX + (aY * topLevelChunkSize.x) + (aZ * topLevelChunkSize.x * topLevelChunkSize.y)];
}

int sampleLevel1Grid(const int aIndex, const int aX, const int aY, const int aZ)
{
    return Level1Grid[aIndex].itemIndices[aX + (aY * 4) + (aZ * 4 * 4)]; // replace constants
}

int sampleLevel2Grid(const int aIndex, const int aX, const int aY, const int aZ)
{
    int combinedItems = Level2Grid[aIndex].items[aY + (aZ * 4)];
    
    return (combinedItems >> (aX * 8)) & 0xFF; // replace constants
}

//top level defines
#define GET_TOP_INDEX_X(value) ((value >> 9) & 0x3F) 
#define GET_TOP_INDEX_Y(value) ((value >> 15) & 0x3F)  
#define GET_TOP_INDEX_Z(value) ((value >> 21) & 0x3F) 

#define OFFSET_TOP_INDEX_X(value) ((value & 0x3F) << 9)
#define OFFSET_TOP_INDEX_Y(value) ((value & 0x3F) << 15)  
#define OFFSET_TOP_INDEX_Z(value) ((value & 0x3F) << 21) 

#define IS_INDEX_NEGATIVE(value) (!((value & 0x3F) ^ 0x3F))

#define TOP_INDEX_FLAG(value) (value & (0xFFFFFF << 6)) 

HitResult traverseTopLevel(const RayStruct aRay)
{
    const float3 scaledDelta = TOP_LEVEL_SCALE / aRay.direction;
    
    const int3 currentIndex = floor((aRay.origin) / TOP_LEVEL_SCALE);

    //data
    int data = OFFSET_TOP_INDEX_X(currentIndex.x) + OFFSET_TOP_INDEX_Y(currentIndex.y) + OFFSET_TOP_INDEX_Z(currentIndex.z);
    
    // 1 int
    // {
    
    // 6 bits index X
    // 6 bits index Y
    // 6 bits index Z
    
    // 1 bit normal X
    // 1 bit normal Y
    // 1 bit normal Z
    
    // 2 bits dir X
    // 2 bits dir Y
    // 2 bits dir Z
    // }

    float3 tMax = float3(0, 0, 0);
    
    //get x dist
    tMax.x = (abs(aRay.origin.x / TOP_LEVEL_SCALE - floor(aRay.origin.x / TOP_LEVEL_SCALE) - (aRay.direction.x > 0.)) * TOP_LEVEL_SCALE) * aRay.rayDelta.x;
    
    //get y dist
    tMax.y = (abs(aRay.origin.y / TOP_LEVEL_SCALE - floor(aRay.origin.y / TOP_LEVEL_SCALE) - (aRay.direction.y > 0.)) * TOP_LEVEL_SCALE) * aRay.rayDelta.y;
    
    //get z dist
    tMax.z = (abs(aRay.origin.z / TOP_LEVEL_SCALE - floor(aRay.origin.z / TOP_LEVEL_SCALE) - (aRay.direction.z > 0.)) * TOP_LEVEL_SCALE) * aRay.rayDelta.z;

    data = TOP_INDEX_FLAG(data) +
            OFFSET_DIR_X((aRay.direction.x > 0) - (aRay.direction.x < 0) + 1) +
            OFFSET_DIR_Y((aRay.direction.y > 0) - (aRay.direction.y < 0) + 1) +
            OFFSET_DIR_Z((aRay.direction.z > 0) - (aRay.direction.z < 0) + 1);
    
    float distance = 0.f;
    int loop = 0;
    while (true)
    {
        loop++;
        
        //if (loop > 100)
        //{
        //    HitResult myResult = hitResultDefault();
        //    myResult.item.color = float3(0, 1, 0);
        //    return myResult;
        //}
        
        if (GET_TOP_INDEX_X(data) >= topLevelChunkSize.x || IS_INDEX_NEGATIVE(GET_TOP_INDEX_X(data)) || // logic could get combined here for negatives
            GET_TOP_INDEX_Y(data) >= topLevelChunkSize.y || IS_INDEX_NEGATIVE(GET_TOP_INDEX_Y(data)) ||
            GET_TOP_INDEX_Z(data) >= topLevelChunkSize.z || IS_INDEX_NEGATIVE(GET_TOP_INDEX_Z(data)))
        {
            HitResult myResult = hitResultDefault();
            return myResult;
        }
        
        const int myIndex = sampleTopLevelGrid(GET_TOP_INDEX_X(data), GET_TOP_INDEX_Y(data), GET_TOP_INDEX_Z(data));
        if (myIndex != -1)
        {
            RayStruct myRay;
            myRay.origin = aRay.origin + aRay.direction * (distance + 0.0001f);
            myRay.direction = aRay.direction;
            myRay.rayDelta = aRay.rayDelta;
            
            const int3 myBounds = int3(GET_TOP_INDEX_X(data), GET_TOP_INDEX_Y(data), GET_TOP_INDEX_Z(data)) * TOP_LEVEL_SCALE;

            chunkTraverseResult myResult = traverseLevel1(myRay, myBounds, myIndex, data);
            loop += myResult.loopCount;
            
            if (myResult.hit.hitDistance != FLOAT_MAX)
            {
                myResult.hit.hitDistance += distance;
                return myResult.hit;
            }
        }

        distance = min(min(tMax.x, tMax.y), tMax.z);
        
        if (distance == tMax.x)
        {
            //X
            const int stepDir = (GET_DIR_X(data) - 1);
            
            tMax.x = tMax.x + scaledDelta.x * stepDir;
       
            data = DIR_FLAG(data) + 
                    OFFSET_TOP_INDEX_X(GET_TOP_INDEX_X(data) + stepDir) + 
                    OFFSET_TOP_INDEX_Y(GET_TOP_INDEX_Y(data)) + 
                    OFFSET_TOP_INDEX_Z(GET_TOP_INDEX_Z(data)) +
                    OFFSET_NORMAL_X(1);

        }
        else if (distance == tMax.y)
        {
            //Y
            const int stepDir = (GET_DIR_Y(data) - 1);
            
            tMax.y = tMax.y + scaledDelta.y * stepDir;

            data = DIR_FLAG(data) + 
                    OFFSET_TOP_INDEX_X(GET_TOP_INDEX_X(data)) + 
                    OFFSET_TOP_INDEX_Y(GET_TOP_INDEX_Y(data) + stepDir) + 
                    OFFSET_TOP_INDEX_Z(GET_TOP_INDEX_Z(data)) +
                    OFFSET_NORMAL_Y(1);
        }
        else //(distance == tMax.z)
        {
            //Z
            const int stepDir = (GET_DIR_Z(data) - 1);
            
            tMax.z = tMax.z + scaledDelta.z * stepDir;
            
            data = DIR_FLAG(data) + 
                    OFFSET_TOP_INDEX_X(GET_TOP_INDEX_X(data)) + 
                    OFFSET_TOP_INDEX_Y(GET_TOP_INDEX_Y(data)) + 
                    OFFSET_TOP_INDEX_Z(GET_TOP_INDEX_Z(data) + stepDir) +
                    OFFSET_NORMAL_Z(1);
        }
    }
    
    HitResult myResult = hitResultDefault();
    return myResult;
}

chunkTraverseResult traverseLevel1(const RayStruct aRay, const int3 aMinBounds, const int aChunkIndex, int aData)
{
    const float3 scaledDelta = CHUNK_SIZE_2 / aRay.direction;

    const int3 currentIndex = floor((aRay.origin - aMinBounds) / CHUNK_SIZE_2);
    aData = DIR_FLAG(aData) + NORMAL_FLAG(aData) + OFFSET_INDEX_X(currentIndex.x) + OFFSET_INDEX_Y(currentIndex.y) + OFFSET_INDEX_Z(currentIndex.z);
    
    //get x dist    
    float3 tMax = float3(0, 0, 0);
    
    tMax.x = (abs(aRay.origin.x / CHUNK_SIZE_2 - floor(aRay.origin.x / CHUNK_SIZE_2) - (aRay.direction.x > 0.)) * CHUNK_SIZE_2) * aRay.rayDelta.x;
    
    //get y dist    
    tMax.y = (abs(aRay.origin.y / CHUNK_SIZE_2 - floor(aRay.origin.y / CHUNK_SIZE_2) - (aRay.direction.y > 0.)) * CHUNK_SIZE_2) * aRay.rayDelta.y;
    
    //get z dist    
    tMax.z = (abs(aRay.origin.z / CHUNK_SIZE_2 - floor(aRay.origin.z / CHUNK_SIZE_2) - (aRay.direction.z > 0.)) * CHUNK_SIZE_2) * aRay.rayDelta.z;
     
    float distance = 0.f;
    int loop = 0;
    while (true)
    {
        loop++;
        
        //if (loop > 100)
        //{
        //    HitResult myResult = hitResultDefault();
        //    myResult.item.color = float3(0, 1, 0);
        //    return myResult;
        //}
        
        if (aData & LEVEL1_EXIT_FLAG)
        {
            chunkTraverseResult myResult;
            myResult.loopCount = loop;
            
            myResult.hit = hitResultDefault();
            return myResult;
        }
        
        const int myIndex = sampleLevel1Grid(aChunkIndex, GET_INDEX_X(aData), GET_INDEX_Y(aData), GET_INDEX_Z(aData));
        if (myIndex != -1)
        {
            RayStruct myRay;
            myRay.origin = aRay.origin + aRay.direction * (distance + 0.0001f);
            myRay.direction = aRay.direction;
            myRay.rayDelta = aRay.rayDelta;
            
            const int3 myBounds = aMinBounds + int3(GET_INDEX_X(aData), GET_INDEX_Y(aData), GET_INDEX_Z(aData)) * CHUNK_SIZE_2;

            chunkTraverseResult myResult = traverseLevel2(myRay, myBounds, myIndex, aData);
            
            loop += myResult.loopCount;
            
            if (myResult.hit.hitDistance != FLOAT_MAX)
            {
                myResult.loopCount = loop;

                myResult.hit.hitDistance += distance;
                return myResult;
            }
        }

        distance = min(min(tMax.x, tMax.y), tMax.z);
        
        if (distance == tMax.x)
        {
            //X
            const int stepDir = (GET_DIR_X(aData) - 1);
            
            tMax.x = tMax.x + scaledDelta.x * stepDir;
         
            aData = DIR_FLAG(aData) +
                    OFFSET_INDEX_X(GET_INDEX_X(aData) + stepDir) +
                    OFFSET_INDEX_Y(GET_INDEX_Y(aData)) +
                    OFFSET_INDEX_Z(GET_INDEX_Z(aData))
                    + OFFSET_NORMAL_X(1);
        }
        else if (distance == tMax.y)
        {
            //Y
            const int stepDir = (GET_DIR_Y(aData) - 1);
            
            tMax.y = tMax.y + scaledDelta.y * stepDir;
            
            aData = DIR_FLAG(aData) +
                    OFFSET_INDEX_X(GET_INDEX_X(aData)) +
                    OFFSET_INDEX_Y(GET_INDEX_Y(aData) + stepDir) +
                    OFFSET_INDEX_Z(GET_INDEX_Z(aData))
                    + OFFSET_NORMAL_Y(1);
        }
        else //(distance == tMax.z)
        {
            //Z
            const int stepDir = (GET_DIR_Z(aData) - 1);
            
            tMax.z = tMax.z + scaledDelta.z * stepDir;
            
            aData = DIR_FLAG(aData) +
                    OFFSET_INDEX_X(GET_INDEX_X(aData)) +
                    OFFSET_INDEX_Y(GET_INDEX_Y(aData)) +
                    OFFSET_INDEX_Z(GET_INDEX_Z(aData) + stepDir)
                    + OFFSET_NORMAL_Z(1);
        }
    }
    
    chunkTraverseResult myResult;
    myResult.loopCount = loop;
    myResult.hit = hitResultDefault();
    return myResult;
}

chunkTraverseResult traverseLevel2(const RayStruct aRay, const int3 aMinBounds, const int aChunkIndex, int aData)
{
    const float3 delta = 1.f / aRay.direction;
    
    const int3 currentIndex = floor(aRay.origin - aMinBounds);
    aData = DIR_FLAG(aData) + NORMAL_FLAG(aData) + OFFSET_INDEX_X(currentIndex.x) + OFFSET_INDEX_Y(currentIndex.y) + OFFSET_INDEX_Z(currentIndex.z);
       
    //get x dist    
    float3 tMax = float3(0, 0, 0);
    
    tMax.x = abs(aRay.origin.x - floor(aRay.origin.x) - (aRay.direction.x > 0.)) * aRay.rayDelta.x;
    
    //get y dist    
    tMax.y = abs(aRay.origin.y - floor(aRay.origin.y) - (aRay.direction.y > 0.)) * aRay.rayDelta.y;
    
    //get z dist    
    tMax.z = abs(aRay.origin.z - floor(aRay.origin.z) - (aRay.direction.z > 0.)) * aRay.rayDelta.z;

    float distance = 0.f;
    int loop = 0;
    while (true)
    {
        loop++;
        
        if (aData & LEVEL2_EXIT_FLAG)
        {
            chunkTraverseResult myResult;
            myResult.loopCount = loop;
            
            myResult.hit = hitResultDefault();
            return myResult;
        }
       
        const int myIndex = sampleLevel2Grid(aChunkIndex, GET_INDEX_X(aData), GET_INDEX_Y(aData), GET_INDEX_Z(aData));
        if (myIndex > 0)
        {
            chunkTraverseResult myResult;
            myResult.loopCount = loop;
            
            myResult.hit = hitResultDefault();
            myResult.hit.itemIndex = myIndex;
            myResult.hit.hitDistance = distance;
            
            const int stepDirX = (GET_DIR_X(aData) - 1);
            const int stepDirY = (GET_DIR_Y(aData) - 1);
            const int stepDirZ = (GET_DIR_Z(aData) - 1);
            
            myResult.hit.hitNormal = float3(GET_NORMAL_X(aData) * float(-stepDirX),
                                            GET_NORMAL_Y(aData) * float(-stepDirY),
                                            GET_NORMAL_Z(aData) * float(-stepDirZ));
            return myResult;
        }

        distance = min(min(tMax.x, tMax.y), tMax.z);
        
        if (distance == tMax.x)
        {
            //X
            const int stepDir = (GET_DIR_X(aData) - 1);
            
            tMax.x = tMax.x + delta.x * stepDir;
            
            aData = DIR_FLAG(aData) +
                    OFFSET_INDEX_X(GET_INDEX_X(aData) + stepDir) +
                    OFFSET_INDEX_Y(GET_INDEX_Y(aData)) +
                    OFFSET_INDEX_Z(GET_INDEX_Z(aData))
                    + OFFSET_NORMAL_X(1);
        }
        else if (distance == tMax.y)
        {
            //Y
            const int stepDir = (GET_DIR_Y(aData) - 1);
            
            tMax.y = tMax.y + delta.y * stepDir;
            
            aData = DIR_FLAG(aData) +
                    OFFSET_INDEX_X(GET_INDEX_X(aData)) +
                    OFFSET_INDEX_Y(GET_INDEX_Y(aData) + stepDir) +
                    OFFSET_INDEX_Z(GET_INDEX_Z(aData))
                    + OFFSET_NORMAL_Y(1);
        }
        else //(distance == tMax.z)
        {
            //Z
            const int stepDir = (GET_DIR_Z(aData) - 1);
            
            tMax.z = tMax.z + delta.z * stepDir;
            
            aData = DIR_FLAG(aData) +
                    OFFSET_INDEX_X(GET_INDEX_X(aData)) +
                    OFFSET_INDEX_Y(GET_INDEX_Y(aData)) +
                    OFFSET_INDEX_Z(GET_INDEX_Z(aData) + stepDir)
                    + OFFSET_NORMAL_Z(1);
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
    
    //myRay.invDirection = 1.f / myRay.direction;
    
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
    
    //myRay.invDirection = 1.f / myRay.direction;
    
    myRay.rayDelta = 1.f / max(abs(myRay.direction), eps);
    
    return myRay;
}

float3 reflectionRay(float3 aDirectionIn, float3 aNormal)
{
    float3 reflectionRayDir = aDirectionIn - (aNormal * 2 * dot(aDirectionIn, aNormal));
    return normalize(reflectionRayDir);
}

RayStruct generateDiffuseRay(float3 hitPoint, float3 hitNormal, inout RandomState rs)
{
    RayStruct myRay;
    
    //diffusion
    updateRandom(rs);
    float3 random = random1(rs);
    
    float3 rayDirection = normalize(hitNormal + randomInUnitSphere(random));
    
    myRay.direction = rayDirection;
    myRay.origin = hitPoint;
    myRay.rayDelta = 1. / max(abs(myRay.direction), eps);
    
    return myRay;
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
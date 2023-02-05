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

#define FLOAT_MAX 3.402823466e+38F
#define eps 1./ 1080.f
#define OCTREE_DEPTH_LEVELS 5
#define MAX_STEPS 80
#define MAX_BOUNCES 5

struct RayStruct
{
    float3 origin;
    float3 direction;
    
    float3 rayDelta;
};

struct NodeData
{
    int parentOffset;
    int childOffset;
    bool filled;
    int color;
};

struct HitResult
{
    float hitDistance;
    int color;
    int loopCount;
    float3 hitNormal;
};

struct VoxelTraverseResult
{
    float distance;
    float3 normal;
};

HitResult hitResultDefault()
{
    HitResult result;
    result.hitDistance = FLOAT_MAX;
    result.color - 1;
    result.loopCount = 0;
    result.hitNormal = float3(0, 0, 0);
    return result;
}

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

//RayStruct generateDiffuseRay(float3 hitPosition, float3 hitNormal, int aSeed);

RayStruct createRay(float2 windowPos);

float3 reflectionRay(float3 aDirectionIn, float3 aNormal);

VoxelTraverseResult traverseVoxel(RayStruct aRay, float aScale = 1.f);


HitResult traverseOctree(RayStruct aRay);
float2 intersectAABB(RayStruct aRay, float3 boxMin, float3 boxMax);

NodeData getNodeAtOffset(int aOffset);
HitResult traverseNode(NodeData aNode, RayStruct aRay, int aScale);

float4 colorIndexToColor(int aIndex);

int xorShift32(int aSeed);
int wangHash(int aSeed);
//RandomOut random(float aMin, float aMax, int aSeed);
void updateRandom(inout RandomState rs);
float3 random1(RandomState rs);

RandomState initialize(int2 aDTid, int aOffset);

float3 randomInUnitSphere(float3 r);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    float2 WindowLocal = ((float2) DTid.xy / maxThreadIter.xy);  
    RandomState rs = initialize(DTid.xy, frameCount);
    
    float4 outColor = float4(0, 0, 0, 1);
    
    HitResult hitResults[MAX_BOUNCES];
    int stackPointer = 0;
    
    for (int i = 0; i < sampleCount; i++)
    {
        RayStruct myRay = createRay(WindowLocal);
    
        bool stop = false;
        while (!stop)
        {
            HitResult hit = traverseOctree(myRay);
        
            if (hit.hitDistance == FLOAT_MAX)
            {
                ////hit light
                //float4 myOutColor = float4(0.1, 0.1, 0.3, 1.0);
            
                //for (int i = stackPointer - 1; i > 0; i--)
                //{
                //    HitResult myHit = hitResults[i];
                
                //    myOutColor *= colorIndexToColor(myHit.color);
                //}
            
                //outColor = myOutColor;
                stop = true;
                continue;
            }
        
            hitResults[stackPointer] = hit;
            stackPointer++;
        
            if (hit.color < 3)
            {
                //hit light
                float4 myOutColor = colorIndexToColor(hit.color);
            
                for (int i = stackPointer - 1; i > 0; i--)
                {
                    HitResult myHit = hitResults[i];
                
                    myOutColor *= colorIndexToColor(myHit.color);
                }
            
                outColor += myOutColor;
                stop = true;
                continue;
            }
        
            if (stackPointer == MAX_BOUNCES)
            {
                stop = true;
                continue;
            }
            
            if (hit.color < 4)
            {
                //reflection
                float3 rayDirection = reflectionRay(myRay.direction, hit.hitNormal);
                float3 hitPoint = myRay.origin + myRay.direction * hit.hitDistance;
        
                myRay.direction = rayDirection;
                myRay.origin = hitPoint;
                myRay.rayDelta = 1. / max(abs(myRay.direction), eps);
                continue;
            }
        
            //diffusion
            updateRandom(rs);
            float3 random = random1(rs);
        
            float3 rayDirection = normalize(hit.hitNormal + randomInUnitSphere(random));
            float3 hitPoint = myRay.origin + myRay.direction * hit.hitDistance;
        
            myRay.direction = rayDirection;
            myRay.origin = hitPoint;
            myRay.rayDelta = 1. / max(abs(myRay.direction), eps);
        }
        
        stackPointer = 0;
    }
    
    OutputTexture[DTid.xy] += outColor / sampleCount;
}

RayStruct createRay(float2 windowPos)
{
    RayStruct myRay;
    
    myRay.origin = (camUpperLeftCorner + camPixelOffsetHorizontal * windowPos.x + camPixelOffsetVertical * windowPos.y).xyz;
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

VoxelTraverseResult traverseVoxel(RayStruct aRay, float aScale)
{
    VoxelTraverseResult results[3];
    
    float3 delta = aRay.rayDelta;
    
    //get x dist    
    results[0].distance = (abs(aRay.origin.x / aScale - floor(aRay.origin.x / aScale) - (aRay.direction.x > 0.)) * aScale) * delta.x;
    results[0].normal = float3(((aRay.direction.x < 0.) << 1) - 1, 0, 0);
    
    //get y dist    
    results[1].distance = (abs(aRay.origin.y / aScale - floor(aRay.origin.y / aScale) - (aRay.direction.y > 0.)) * aScale) * delta.y;
    results[1].normal = float3(0, ((aRay.direction.y < 0.) << 1) - 1, 0);
    
    //get z dist    
    results[2].distance = (abs(aRay.origin.z / aScale - floor(aRay.origin.z / aScale) - (aRay.direction.z > 0.)) * aScale) * delta.z;
    results[2].normal = float3(0, 0, ((aRay.direction.z < 0.) << 1) - 1);
    
    VoxelTraverseResult min = results[0];
    if (results[1].distance < min.distance) min = results[1];
    if (results[2].distance < min.distance) min = results[2];
    
    min.distance += 0.0001f;
    
    return min;
}

HitResult traverseOctree(RayStruct aRay)
{
    float2 result = intersectAABB(aRay, float3(0, 0, 0), float3(16, 16, 16));
    
    if (result.x < result.y && result.y > 0)
    {
        float3 intersectPos = aRay.origin;
        float RayOriginOffset = 0.f;
        
        if (result.x > 0)
        {
            RayOriginOffset += result.x + 0.01f;
            intersectPos += aRay.direction * (result.x + 0.01f);
        }
        
        RayStruct myRay;
        myRay.origin = intersectPos;
        myRay.direction = aRay.direction;
        myRay.rayDelta = aRay.rayDelta;
        
        NodeData myNode = getNodeAtOffset(0);
        
        HitResult hit = traverseNode(myNode, myRay, 16);
        hit.hitDistance += RayOriginOffset;
        return hit;
    }
    
    return hitResultDefault();
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

NodeData getNodeAtOffset(int aOffset)
{
    NodeData myData;
    
    int node = octreeData[aOffset];
    
    // 24 bits octree traveral & 8 bits color data
	    // 10 bits parent offset data -> offset 0
	    // 13 bits children offset data -> offset 10
	    // 1 bit filled in bool -> offset 23
	    // 8 bits voxel data -> offset 24
    
    myData.parentOffset = (node & 0x3FF);
    myData.childOffset = (node & 0x7FFC00) >> 10;
    myData.filled = (node & 0x800000) >> 23;
    myData.color = (node & 0xFF000000) >> 24;
    
    return myData;
}

struct TraversalItem
{
    NodeData aNode;
    int aScale;
};

int3 removeOffsetAtScale(int3 aOctantCorner, int aScale)
{
    aOctantCorner.x &= ~aScale;
    aOctantCorner.y &= ~aScale;
    aOctantCorner.z &= ~aScale;

    return aOctantCorner;
}

int3 addOffsetAtScale(int3 aOctantCorner, int3 aOffset, int aScale)
{
    aOctantCorner.x ^= (-aOffset.x ^ aOctantCorner.x) & (aScale >> 1);
    aOctantCorner.y ^= (-aOffset.y ^ aOctantCorner.y) & (aScale >> 1);
    aOctantCorner.z ^= (-aOffset.z ^ aOctantCorner.z) & (aScale >> 1);
    
    return aOctantCorner;
}

int3 calculateOctantFromOffsetPosition(float3 aPosition, int aScale)
{
    int3 result;
    
    result.x = aPosition.x > (aScale >> 1);
    result.y = aPosition.y > (aScale >> 1);
    result.z = aPosition.z > (aScale >> 1);
    
    return result;
}

int calculateOffsetFromOctant(int3 aOctant)
{
    return aOctant.x + (aOctant.y << 1) + (aOctant.z << 2);
}

bool isPositionInOctantAtScale(float3 aPosition, int3 aOctantCorner, int aScale)
{
    return aPosition.x - aOctantCorner.x > aScale || aPosition.x - aOctantCorner.x < 0 ||
           aPosition.y - aOctantCorner.y > aScale || aPosition.y - aOctantCorner.y < 0 ||
           aPosition.z - aOctantCorner.z > aScale || aPosition.z - aOctantCorner.z < 0;
}

HitResult traverseNode(NodeData aNode, RayStruct aRay, int aScale)
{
    TraversalItem traversalStack[OCTREE_DEPTH_LEVELS];
    int stackPointer = 1;
    
    TraversalItem myInitialItem;
    myInitialItem.aNode = aNode;
    myInitialItem.aScale = aScale;
    
    traversalStack[0] = myInitialItem;
       
    int3 octantCorner = int3(0, 0, 0);
    
    HitResult result = hitResultDefault();
    
    //get initial normal
    {
        RayStruct myRay = aRay;
        myRay.direction = -myRay.direction;
        
        result.hitNormal = -traverseVoxel(myRay, 16.f).normal;
    }
    
    RayStruct myRay = aRay;
    float distance = 0.0f;
    while (stackPointer > 0)
    {
        result.loopCount++;
        
        NodeData currentNode = traversalStack[stackPointer - 1].aNode;
        int currentScale = traversalStack[stackPointer - 1].aScale;
        
        if (!currentNode.filled)
        {
            //move ray to go to outside node, since we don't want to check for it again
            VoxelTraverseResult traverseResult = traverseVoxel(myRay, currentScale);
            
            result.hitNormal = traverseResult.normal;
            distance += traverseResult.distance;
            
            myRay.origin = aRay.origin + myRay.direction * distance;
            
            octantCorner = removeOffsetAtScale(octantCorner, currentScale);
            
            stackPointer--;
            continue;
        }
        
        if (currentScale == 1)
        {
            result.color = currentNode.color;
            result.hitDistance = distance - 0.00011f;
            
            return result;
        }
        
        if (isPositionInOctantAtScale(myRay.origin, octantCorner, currentScale))
        {
            //we fell outside the current node, so we have to go one up
            octantCorner = removeOffsetAtScale(octantCorner, currentScale);
            
            stackPointer--;
            continue;
        }
        
        //calculate the octant the ray is in for the current node
        int3 currentOctant = calculateOctantFromOffsetPosition(myRay.origin - octantCorner, currentScale);
        int currentOctantOffset = calculateOffsetFromOctant(currentOctant);
        
        octantCorner = addOffsetAtScale(octantCorner, currentOctant, currentScale);
        
        //traverse inside new node
        TraversalItem myItem;
        myItem.aNode = getNodeAtOffset(currentNode.childOffset + currentOctantOffset);
        myItem.aScale = currentScale >> 1;
        traversalStack[stackPointer++] = myItem;
    }
    
    //no intersection found when traversing the ray
    return result;
}

float4 colorIndexToColor(int aIndex)
{
    switch (aIndex)
    {
        case 1:
                {
                return float4(3, 1.5, 3, 1);
            }
            
        case 2:
                {
                return float4(3, 3, 1.5, 1);
            }
            
        case 3:
                {
                return float4(0.5, 0.5, 0.5, 1);
            }
            
        case 4:
                {
                return float4(0.8, 0.8, 0.8, 1);
            }
    }
    
    return float4(0, 0, 0, 1);

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

//RandomOut random(float aMin, float aMax, int aSeed)
//{
//    RandomOut result;
    
//    result.seed = xorShift32(aSeed);
    
//    result.randomNum = aMin + ((float) result.seed / 0xffffffff) * (aMax - aMin);
    
//    return result;
//}

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

RandomState initialize(int2 aDTid, int aOffset)
{
    RandomState output;
    
    int width, height, numLevels;
    noiseTexture.GetDimensions(0, width, height, numLevels);
    
    float4 random = noiseTexture.Load(int3((aDTid.xy) % int2(width, height), 0));
    
    output.z0 = random.x * 1000000 * (aOffset + 1);
    output.z1 = random.y * 1000000 * (aOffset + 1);
    output.z2 = random.z * 1000000 * (aOffset + 1);
    output.z3 = random.w * 1000000 * (aOffset + 1);
    
    return output;
}
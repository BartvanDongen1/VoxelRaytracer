RWTexture2D<float4> OutputTexture : register(u0);

Texture2D noiseTexture : register(t0);

// octree structure
struct OctreeNode
{
    unsigned int childrenIndex;
    unsigned int children;
    
    float padding[2];
};

struct OctreeNodes
{
    OctreeNode nodes[8];
};

struct OctreeItem
{
    float3 color;
    
    float padding;
};

struct OctreeItems
{
    OctreeItem items[8];
};

StructuredBuffer<OctreeNodes> flatOctreeNodes : register(t1);
StructuredBuffer<OctreeItems> flatOctreeItems : register(t1);
//

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
    
    int octreeLayerCount;
    
    int octreeSize;
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
    float3 hitNormal;
    OctreeItem item;
    
    int loopCount;
};

struct VoxelTraverseResult
{
    float distance;
    float3 normal;
};

OctreeItem itemDefault()
{
    OctreeItem result;
    result.color = float3(0, 0, 0);
    
    return result;
}

HitResult hitResultDefault()
{
    HitResult result;
    result.hitDistance = FLOAT_MAX;
    result.loopCount = 0;
    result.hitNormal = float3(0, 0, 0);
    result.item = itemDefault();
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
HitResult ray_octree_traversal(RayStruct aRay);

RayStruct createRay(float2 windowPos);
RayStruct createRayAA(float2 aWindowPos, float2 aWindowSize, RandomState aRandomState);

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
    float2 WindowLocal = ((float2) DTid.xy / maxThreadIter.xy);  
    RandomState rs = initialize(DTid.xy, frameSeed);
    
    RayStruct myRay = createRayAA(WindowLocal, maxThreadIter.xy, rs);
    
    HitResult result = ray_octree_traversal(myRay);

    if (result.hitDistance != FLOAT_MAX)
    {
        float3 color = result.item.color;
        OutputTexture[DTid.xy] += float4(color, 1);
    }
    else
    {
        float3 color = result.item.color;
        OutputTexture[DTid.xy] += float4(color, 1);
    }
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

RayStruct createRayAA(float2 aWindowPos, float2 aWindowSize, RandomState aRandomState)
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

int first_node(float tx0, float ty0, float tz0, float txm, float tym, float tzm)
{
// optimization 2
    //unsigned int answerX = ((tym < tx0) << 1) | (tzm < tx0); // PLANE YZ
    //unsigned int answerY = ((txm < ty0) << 2) | (tzm < ty0); // PLANE XZ
    //unsigned int answerZ = ((txm < tz0) << 2) | ((tym < tz0) << 1); // PLANE XY
    
    //unsigned int outputs[8];
    //outputs[0] = answerZ; // tx0 < ty0, tx0 < tz0, ty0 < tz0, z biggest
    //outputs[1] = answerZ; // tx0 > ty0, tx0 < tz0, ty0 < tz0, z biggest
    //outputs[2] = answerZ; // tx0 < ty0, tx0 > tz0, ty0 < tz0, edge case (use z)
    //outputs[3] = answerX; // tx0 > ty0, tx0 > tz0, ty0 < tz0, x biggest
    //outputs[4] = answerY; // tx0 < ty0, tx0 < tz0, ty0 > tz0, y biggest
    //outputs[5] = answerZ; // tx0 > ty0, tx0 < tz0, ty0 > tz0, edge case (use z)
    //outputs[6] = answerY; // tx0 < ty0, tx0 > tz0, ty0 > tz0, y biggest
    //outputs[7] = answerX; // tx0 > ty0, tx0 > tz0, ty0 > tz0, x biggest
    
    //int index = (int) (tx0 > ty0) + ((int) (tx0 > tz0) << 1) + ((int) (ty0 > tz0) << 2);
    
    //return outputs[index];

// optimization 1
    unsigned int answer = 0; // initialize to 00000000
    
    // select the entry plane and set bits
    if (tx0 > ty0)
    {
        if (tx0 > tz0)
        {
            // PLANE YZ
            answer |= ((tym < tx0) << 1);
            answer |= (tzm < tx0);
            return answer;
        }
    }
    else
    {
        if (ty0 > tz0)
        {
            // PLANE XZ
            answer |= ((txm < ty0) << 2);
            answer |= (tzm < ty0);
            return answer;
        }
    }
    
    // PLANE XY
    answer |= ((txm < tz0) << 2);
    answer |= ((tym < tz0) << 1);
    return answer;
    
// unoptimized
    //unsigned int answer = 0; // initialize to 00000000
    
    //// select the entry plane and set bits
    //if (tx0 > ty0)
    //{
    //    if (tx0 > tz0)
    //    { // PLANE YZ
    //        if (tym < tx0)
    //        {
    //            answer |= 2; // set bit at position 1
    //        }
    //        if (tzm < tx0)
    //        {
    //            answer |= 1; // set bit at position 0
    //        }
    //        return answer;
    //    }
    //}
    //else
    //{
    //    if (ty0 > tz0)
    //    { // PLANE XZ
    //        if (txm < ty0)
    //        {
    //            answer |= 4; // set bit at position 2
    //        }
    //        if (tzm < ty0)
    //        {
    //            answer |= 1; // set bit at position 0
    //        }
    //        return answer;
    //    }
    //}
    
    //// PLANE XY
    //if (txm < tz0)
    //{
    //    answer |= 4; // set bit at position 2
    //}
    
    //if (tym < tz0)
    //{
    //    answer |= 2; // set bit at position 1
    //}
    
    //return answer;
}

int new_node(float txm, int x, float tym, int y, float tzm, int z)
{
// optimization 2
    int outputs[8];
    outputs[0] = z; // txm > tym, tym > tzm, tzm > txm, use z for edge case
    outputs[1] = x; // txm < tym, tym > tzm, tzm > txm, x smallest
    outputs[2] = y; // txm > tym, tym < tzm, tzm > txm, y smallest
    outputs[3] = x; // txm < tym, tym < tzm, tzm > txm, x smallest
    outputs[4] = z; // txm > tym, tym > tzm, tzm < txm, z smallest
    outputs[5] = z; // txm < tym, tym > tzm, tzm < txm, z smallest
    outputs[6] = y; // txm > tym, tym < tzm, tzm < txm, y smallest
    outputs[7] = z; // txm < tym, tym < tzm, tzm < txm, use z for edge case
    
    int index = (int) (txm < tym) + ((int) (tym < tzm) << 1) + ((int) (tzm < txm) << 2);
    
    return outputs[index];
    
// optimization 1
    //int output = 0;
    //output += (int) (txm < tym) * (int) (txm < tzm) * x;
    //output += (int) (!(txm < tym)) * (int) (tym < tzm) * y;
    //output += (int) (!(txm < tym)) * (int) (!(tym < tzm)) * z;
    //output += (int) (txm < tym) * (int) (!(txm < tzm)) * z;
    //return output;
    
// unoptimized
    //if (txm < tym)
    //{
    //    if (txm < tzm)
    //    {
    //        return x;
    //    } // YZ plane
    //}
    //else
    //{
    //    if (tym < tzm)
    //    {
    //        return y;
    //    } // XZ plane
    //}
    //return z; // XY plane;
}

//32 byte struct
struct TraversalItem
{
    float tx0;
    float ty0;
    float tz0;
    
    float tx1;
    float ty1;
    float tz1;

    int inLoopAndcurrNode; // 5th bit = inloop bool, 1st 4 bits = currNode
 
    int nodeValues; // highest 8 bits are child flags, lower 24 bits are child index
};

#define CHILD_INDEX(value) (value & 0x00FFFFFF) 
#define CHILDREN_FLAGS(value) (value >> 24)

OctreeNode getNodeAtIndexAndOffset(int aIndex, int aOffset)
{
    OctreeNodes nodes = flatOctreeNodes[aIndex];
    
    return nodes.nodes[aOffset];
}

OctreeItem getItemAtIndexAndOffset(int aIndex, int aOffset)
{
    OctreeItems items = flatOctreeItems[aIndex];
    
    return items.items[aOffset];
}

TraversalItem initializeTraversalItem(float tx0, float ty0, float tz0, float tx1, float ty1, float tz1, OctreeNode aNode)
{
    TraversalItem myReturnItem;
    
    myReturnItem.nodeValues = (aNode.children << 24) + aNode.childrenIndex;
    
    myReturnItem.tx0 = tx0;
    myReturnItem.ty0 = ty0;
    myReturnItem.tz0 = tz0;
    
    myReturnItem.tx1 = tx1;
    myReturnItem.ty1 = ty1;
    myReturnItem.tz1 = tz1;

    myReturnItem.inLoopAndcurrNode = 0;
    
    return myReturnItem;
}

HitResult proc_subtree(float tx0, float ty0, float tz0, float tx1, float ty1, float tz1, unsigned int a)
{
    int loopCount;
    
    //setup stack
    TraversalItem traversalStack[MAX_STACK_SIZE];
    int stackPointer = 1;
    
    TraversalItem myInitialItem = initializeTraversalItem(tx0, ty0, tz0, tx1, ty1, tz1, getNodeAtIndexAndOffset(0, 0)); // top level node is at 0,0
    traversalStack[0] = myInitialItem;
    
    // stack loop
    while (stackPointer > 0)
    {        
        loopCount++;
        
        //if (loopCount > 200)
        //{
        //    HitResult hit = hitResultDefault();
        //    hit.loopCount = loopCount;
        //    hit.item.color = float3(0, 1, 0);
            
        //    return hit;
        //}
        
        int itemIndex = stackPointer - 1;
        TraversalItem currentItem = traversalStack[itemIndex];

        float myTxm = 0.5 * (currentItem.tx0 + currentItem.tx1);
        float myTym = 0.5 * (currentItem.ty0 + currentItem.ty1);
        float myTzm = 0.5 * (currentItem.tz0 + currentItem.tz1);

        // skip computation if it's already been done
        if (!(bool) (currentItem.inLoopAndcurrNode & 0x10)) // mask bool with 0b10000 -> 0x10
        {
            if (currentItem.tx1 < 0 || currentItem.ty1 < 0 || currentItem.tz1 < 0)
            {
                stackPointer--;
                continue;
            }

            if (stackPointer == octreeLayerCount)
            {
                HitResult hit = hitResultDefault();
                hit.hitDistance = 10;
                
                OctreeItem myReturnItem;
                myReturnItem.color = float3(1, 1, 1);
                
                hit.item = myReturnItem;
                
                //reached leaf node
                return hit;
            }

            currentItem.inLoopAndcurrNode = first_node(currentItem.tx0, currentItem.ty0, currentItem.tz0, myTxm, myTym, myTzm);
            
            currentItem.inLoopAndcurrNode |= 0x10; // flag bool with 0b10000 -> 0x10
        }
        
        if ((currentItem.inLoopAndcurrNode & 0xF) == 8)
        {
            stackPointer--;
            continue;
        }

        int index = (currentItem.inLoopAndcurrNode & 0xF) ^ a;
        
        bool flag1 = currentItem.inLoopAndcurrNode & 0x1;
        bool flag2 = (currentItem.inLoopAndcurrNode & 0x2) >> 1;
        bool flag3 = (currentItem.inLoopAndcurrNode & 0x4) >> 2;
        
        float mytx1 = myTxm * (int) (!flag3) + currentItem.tx1 * (int) flag3;
        float myty1 = myTym * (int) (!flag2) + currentItem.ty1 * (int) flag2;
        float mytz1 = myTzm * (int) (!flag1) + currentItem.tz1 * (int) flag1;
        
        if (CHILDREN_FLAGS(currentItem.nodeValues) & (1 << index))
        {
            //if child exists     
            OctreeNode myNode = getNodeAtIndexAndOffset(CHILD_INDEX(currentItem.nodeValues), index);
            
            float mytx0 = currentItem.tx0 * (int) (!flag3) + myTxm * (int) flag3;
            float myty0 = currentItem.ty0 * (int) (!flag2) + myTym * (int) flag2;
            float mytz0 = currentItem.tz0 * (int) (!flag1) + myTzm * (int) flag1;
            
            // traverse inside new node
            TraversalItem myItem = initializeTraversalItem(mytx0, myty0, mytz0, mytx1, myty1, mytz1, myNode);
            traversalStack[stackPointer++] = myItem;
        }
        
        int myX = ((currentItem.inLoopAndcurrNode & 0xF) + 4) * (int) (!flag3) + 8 * (int) flag3;
        int myY = ((currentItem.inLoopAndcurrNode & 0xF) + 2) * (int) (!flag2) + 8 * (int) flag2;
        int myZ = ((currentItem.inLoopAndcurrNode & 0xF) + 1) * (int) (!flag1) + 8 * (int) flag1;
        
        currentItem.inLoopAndcurrNode = new_node(mytx1, myX, myty1, myY, mytz1, myZ) | (currentItem.inLoopAndcurrNode & 0x10); // make sure to keep the hidden bool when assigning values
        
        // update the current item
        traversalStack[itemIndex] = currentItem;
        
        //go to next item in loop
        continue;
    }
    
    return hitResultDefault();
}

HitResult ray_octree_traversal(RayStruct aRay)
{
    int octreeSize = 1 << (octreeLayerCount - 1);
    int3 octreePosition = int3(0, 0, 0);
    
    int3 octreeCenter = octreePosition + (int3(octreeSize, octreeSize, octreeSize) * 0.5);
    
    int3 octreeMin = octreePosition;
    int3 octreeMax = octreePosition + int3(octreeSize, octreeSize, octreeSize);
    
    unsigned int a = 0;

    // fixes for rays with negative direction
    if (aRay.direction[0] < 0)
    {
        aRay.origin[0] = octreeCenter[0] * 2 - aRay.origin[0]; //camera origin fix
        aRay.direction[0] = -aRay.direction[0];
        a |= 4; //bitwise OR (latest bits are XYZ)
    }
    if (aRay.direction[1] < 0)
    {
        aRay.origin[1] = octreeCenter[1] * 2 - aRay.origin[1];
        aRay.direction[1] = -aRay.direction[1];
        a |= 2;
    }
    if (aRay.direction[2] < 0)
    {
        aRay.origin[2] = octreeCenter[2] * 2 - aRay.origin[2];
        aRay.direction[2] = -aRay.direction[2];
        a |= 1;
    }

    float divx = 1.f / aRay.direction[0]; // IEEE stability fix
    float divy = 1.f / aRay.direction[1];
    float divz = 1.f / aRay.direction[2];

    float tx0 = (octreeMin[0] - aRay.origin[0]) * divx;
    float tx1 = (octreeMax[0] - aRay.origin[0]) * divx;
    float ty0 = (octreeMin[1] - aRay.origin[1]) * divy;
    float ty1 = (octreeMax[1] - aRay.origin[1]) * divy;
    float tz0 = (octreeMin[2] - aRay.origin[2]) * divz;
    float tz1 = (octreeMax[2] - aRay.origin[2]) * divz;
    
    if (max(max(tx0, ty0), tz0) < min(min(tx1, ty1), tz1) && min(min(tx1, ty1), tz1) > 0)
    {
        return proc_subtree(tx0, ty0, tz0, tx1, ty1, tz1, a);
    }
    
    HitResult noHit = hitResultDefault();
    return noHit;
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
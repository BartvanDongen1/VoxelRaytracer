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
int ray_octree_traversal(RayStruct aRay);


RayStruct createRay(float2 windowPos);
RayStruct createRayAA(float2 aWindowPos, float2 aWindowSize, RandomState aRandomState);

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

RandomState initialize(int2 aDTid, int aFrameSeed);

float3 randomInUnitSphere(float3 r);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    float2 WindowLocal = ((float2) DTid.xy / maxThreadIter.xy);  
    RandomState rs = initialize(DTid.xy, frameSeed);
    
    float4 outColor = float4(0, 0, 0, 1);
    
    HitResult hitResults[MAX_BOUNCES];
    int stackPointer = 0;
    
    RayStruct myRay = createRay(WindowLocal);
    
    int result = ray_octree_traversal(myRay);
    
    //switch (result)
    //{
    //    case 0:
    //        OutputTexture[DTid.xy] += float4(0, 0, 0, 1);
    //        break;
    //    case 1:
    //        OutputTexture[DTid.xy] += float4(0, 0, 0, 1);
    //        break;
    //    case 2:
    //        OutputTexture[DTid.xy] += float4(0, 0, 1, 1);
    //        break;
    //    case 3:
    //        OutputTexture[DTid.xy] += float4(0, 1, 0, 1);
    //        break;
    //    case 4:
    //        OutputTexture[DTid.xy] += float4(0, 1, 1, 1);
    //        break;
    //    case 5:
    //        OutputTexture[DTid.xy] += float4(1, 0, 0, 1);
    //        break;
    //    case 6:
    //        OutputTexture[DTid.xy] += float4(1, 0, 1, 1);
    //        break;
    //    case 7:
    //        OutputTexture[DTid.xy] += float4(1, 1, 0, 1);
    //        break;
    //    case 8:
    //        OutputTexture[DTid.xy] += float4(1, 1, 1, 1);
    //        break;
    //}
    
    if (result == 101)
    {
        OutputTexture[DTid.xy] += float4(1, 0, 0, 1);
        return;
    }
    
    if (result == 102)
    {
        OutputTexture[DTid.xy] += float4(0, 1, 0, 1);
        return;
    }
    
    float brightness = result / 100.f;
    
    OutputTexture[DTid.xy] += float4(brightness, brightness, brightness, 1);
    
    //switch (result)
    //{
    //    case 0:
    //        OutputTexture[DTid.xy] += float4(0, 0, 0, 1);
    //        break;
    //    case 1:
    //        OutputTexture[DTid.xy] += float4(1, 0, 0, 1);
    //        break;
    //    case 2:
    //        OutputTexture[DTid.xy] += float4(1, 1, 1, 1);
    //        break;
    //}
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

//unsigned int a; // first 8 bits used

int first_node(float tx0, float ty0, float tz0, float txm, float tym, float tzm)
{
    unsigned int answer = 0; // initialize to 00000000
    
    // select the entry plane and set bits
    if (tx0 > ty0)
    {
        if (tx0 > tz0)
        { // PLANE YZ
            if (tym < tx0)
            {
                answer |= 2; // set bit at position 1
            }
            if (tzm < tx0)
            {
                answer |= 1; // set bit at position 0
            }
            return answer;
        }
    }
    else
    {
        if (ty0 > tz0)
        { // PLANE XZ
            if (txm < ty0)
            {
                answer |= 4; // set bit at position 2
            }
            if (tzm < ty0)
            {
                answer |= 1; // set bit at position 0
            }
            return answer;
        }
    }
    
    // PLANE XY
    if (txm < tz0)
    {
        answer |= 4; // set bit at position 2
    }
    
    if (tym < tz0)
    {
        answer |= 2; // set bit at position 1
    }
    
    return answer;
}

int new_node(float txm, int x, float tym, int y, float tzm, int z)
{
    if (txm < tym)
    {
        if (txm < tzm)
        {
            return x;
        } // YZ plane
    }
    else
    {
        if (tym < tzm)
        {
            return y;
        } // XZ plane
    }
    return z; // XY plane;
}

struct TraversalItem
{
    float tx0;
    float ty0;
    float tz0;
    float tx1;
    float ty1;
    float tz1;
    
    int scale;
    
    float txm, tym, tzm;
    int currNode;
    
    bool inLoop;
    
    OctreeNode node;
};

OctreeNode getNodeAtIndexAndOffset(int aIndex, int aOffset)
{
    OctreeNodes nodes = flatOctreeNodes[aIndex];
    
    return nodes.nodes[aOffset];
}

TraversalItem initializeTraversalItem(float tx0, float ty0, float tz0, float tx1, float ty1, float tz1, int aScale, OctreeNode aNode)
{
    TraversalItem myReturnItem;
    myReturnItem.node = aNode;
    myReturnItem.tx0 = tx0;
    myReturnItem.ty0 = ty0;
    myReturnItem.tz0 = tz0;
    myReturnItem.tx1 = tx1;
    myReturnItem.ty1 = ty1;
    myReturnItem.tz1 = tz1;
    
    myReturnItem.scale = aScale;
    
    myReturnItem.txm = 0.f;
    myReturnItem.tym = 0.f;
    myReturnItem.tzm = 0.f;
    
    myReturnItem.currNode = 0;
    
    myReturnItem.inLoop = false;
    
    return myReturnItem;
}

int proc_subtree(float tx0, float ty0, float tz0, float tx1, float ty1, float tz1, unsigned int a)
{
    int loopCount;
    
    //setup stack
    TraversalItem traversalStack[10];
    int stackPointer = 1;
    
    int octreeScale = 1 << (octreeLayerCount - 1);

    TraversalItem myInitialItem = initializeTraversalItem(tx0, ty0, tz0, tx1, ty1, tz1, octreeScale, getNodeAtIndexAndOffset(0, 0)); // top level node is at 0,0
    traversalStack[0] = myInitialItem;
    
    // stack loop
    while (stackPointer > 0)
    {
        if (stackPointer > 2)
        {
            return 100;
        }
        
        loopCount++;
        
        int itemIndex = stackPointer - 1;
        TraversalItem currentItem = traversalStack[itemIndex];
        const OctreeNode currentNode = currentItem.node;
        
        // skip computation if it's already been done
        if (!currentItem.inLoop)
        {
            if (currentItem.tx1 < 0 || currentItem.ty1 < 0 || currentItem.tz1 < 0)
            {
                stackPointer--;
                continue;
            }

            if (currentItem.scale == 1)
            {
                //reached leaf node
                return 100;
            }
        
            currentItem.txm = 0.5 * (currentItem.tx0 + currentItem.tx1);
            currentItem.tym = 0.5 * (currentItem.ty0 + currentItem.ty1);
            currentItem.tzm = 0.5 * (currentItem.tz0 + currentItem.tz1);
        
            currentItem.currNode = first_node(currentItem.tx0, currentItem.ty0, currentItem.tz0, currentItem.txm, currentItem.tym, currentItem.tzm);
            
            currentItem.inLoop = true;
        }
        
        int index = currentItem.currNode ^ a;
        
        if (currentNode.childrenIndex >= octreeSize)
        {
            return 101;
        }
        
       switch (currentItem.currNode)
       {
           case 0:
           { 
                   if (currentNode.children & (1 << index))
                   {
                   //if child exists
                       OctreeNode myNode = getNodeAtIndexAndOffset(currentNode.childrenIndex, index);
           
                   // traverse inside new node
                       TraversalItem myItem = initializeTraversalItem(currentItem.tx0, currentItem.ty0, currentItem.tz0, currentItem.txm, currentItem.tym, currentItem.tzm, currentItem.scale / 2, myNode);
                       traversalStack[stackPointer++] = myItem;
                   }
           
                   currentItem.currNode = new_node(currentItem.txm, 4, currentItem.tym, 2, currentItem.tzm, 1);
                   
               // update the current item
                   traversalStack[itemIndex] = currentItem;
                   break;
               }
           
           case 1:
           {
                   if (currentNode.children & (1 << index))
                   {
                   //if child exists
                       OctreeNode myNode = getNodeAtIndexAndOffset(currentNode.childrenIndex, index);
           
                   // traverse inside new node
                       TraversalItem myItem = initializeTraversalItem(currentItem.tx0, currentItem.ty0, currentItem.tzm, currentItem.txm, currentItem.tym, currentItem.tz1, currentItem.scale / 2, myNode);
                       traversalStack[stackPointer++] = myItem;
                   }
           
                   currentItem.currNode = new_node(currentItem.txm, 5, currentItem.tym, 3, currentItem.tz1, 8);
                   
               // update the current item
                   traversalStack[itemIndex] = currentItem;
                   break;
               }
           
           case 2:
           {
                   if (currentNode.children & (1 << index))
                   {
                   //if child exists
                       OctreeNode myNode = getNodeAtIndexAndOffset(currentNode.childrenIndex, index);
           
                   // traverse inside new node
                       TraversalItem myItem = initializeTraversalItem(currentItem.tx0, currentItem.tym, currentItem.tz0, currentItem.txm, currentItem.ty1, currentItem.tzm, currentItem.scale / 2, myNode);
                       traversalStack[stackPointer++] = myItem;
                   }
           
                   currentItem.currNode = new_node(currentItem.txm, 6, currentItem.ty1, 8, currentItem.tzm, 3);
                   
               // update the current item
                   traversalStack[itemIndex] = currentItem;
                   break;
               }
           
           case 3:
           {
                   if (currentNode.children & (1 << index))
                   {
                   //if child exists
                       OctreeNode myNode = getNodeAtIndexAndOffset(currentNode.childrenIndex, index);
           
                   // traverse inside new node
                       TraversalItem myItem = initializeTraversalItem(currentItem.tx0, currentItem.tym, currentItem.tzm, currentItem.txm, currentItem.ty1, currentItem.tz1, currentItem.scale / 2, myNode);
                       traversalStack[stackPointer++] = myItem;
                   }
           
                   currentItem.currNode = new_node(currentItem.txm, 7, currentItem.ty1, 8, currentItem.tz1, 8);
                   
               // update the current item
                   traversalStack[itemIndex] = currentItem;
                   break;
               }
           
           case 4:
           { 
                   if (currentNode.children & (1 << index))
                   {
                   //if child exists
                       OctreeNode myNode = getNodeAtIndexAndOffset(currentNode.childrenIndex, index);
           
                   // traverse inside new node
                       TraversalItem myItem = initializeTraversalItem(currentItem.txm, currentItem.ty0, currentItem.tz0, currentItem.tx1, currentItem.tym, currentItem.tzm, currentItem.scale / 2, myNode);
                       traversalStack[stackPointer++] = myItem;
                   }

                   currentItem.currNode = new_node(currentItem.tx1, 8, currentItem.tym, 6, currentItem.tzm, 5);
                   
               // update the current item
                   traversalStack[itemIndex] = currentItem;
                   break;
               }
           
           case 5:
           { 
                   if (currentNode.children & (1 << index))
                   {
                   //if child exists            
                       OctreeNode myNode = getNodeAtIndexAndOffset(currentNode.childrenIndex, index);
           
                   // traverse inside new node
                       TraversalItem myItem = initializeTraversalItem(currentItem.txm, currentItem.ty0, currentItem.tzm, currentItem.tx1, currentItem.tym, currentItem.tz1, currentItem.scale / 2, myNode);
                       traversalStack[stackPointer++] = myItem;
                   }
           
                   currentItem.currNode = new_node(currentItem.tx1, 8, currentItem.tym, 7, currentItem.tz1, 8);
                   
               // update the current item
                   traversalStack[itemIndex] = currentItem;
                   break;
               }
           
           case 6:
           { 
                   if (currentNode.children & (1 << index))
                   {
                   //if child exists
                       OctreeNode myNode = getNodeAtIndexAndOffset(currentNode.childrenIndex, index);
           
                   // traverse inside new node
                       TraversalItem myItem = initializeTraversalItem(currentItem.txm, currentItem.tym, currentItem.tz0, currentItem.tx1, currentItem.ty1, currentItem.tzm, currentItem.scale / 2, myNode);
                       traversalStack[stackPointer++] = myItem;
                   }
           
                   currentItem.currNode = new_node(currentItem.tx1, 8, currentItem.ty1, 8, currentItem.tzm, 7);
                   
               // update the current item
                   traversalStack[itemIndex] = currentItem;
                   break;
               }
           
           case 7:
           { 
                   if (currentNode.children & (1 << index))
                   {
                   //if child exists
                       OctreeNode myNode = getNodeAtIndexAndOffset(currentNode.childrenIndex, index);
           
                   // traverse inside new node
                       TraversalItem myItem = initializeTraversalItem(currentItem.txm, currentItem.tym, currentItem.tzm, currentItem.tx1, currentItem.ty1, currentItem.tz1, currentItem.scale / 2, myNode);
                       traversalStack[stackPointer++] = myItem;
                   }
           
                   currentItem.currNode = 8;
                   
               // update the current item
                   traversalStack[itemIndex] = currentItem;
                   break;
               }
           case 8:
           {
                   stackPointer--;
           
                   break;
               }
        }
        //go to next item in loop
        continue;
    }
    
    return loopCount;
}

int ray_octree_traversal(RayStruct aRay)
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
        return ((int) proc_subtree(tx0, ty0, tz0, tx1, ty1, tz1, a)) + 1;
    }
    return 0;
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
RWTexture2D<float4> OutputTexture : register(u0);

Texture3D<uint> sceneData : register(t0);
StructuredBuffer<int> octreeData : register(t1);

cbuffer constantBuffer : register(b0)
{
    float4 maxThreadIter;
    
    float4 camPosition;
    float4 camDirection;
    
    float4 camUpperLeftCorner;
    float4 camPixelOffsetHorizontal;
    float4 camPixelOffsetVertical;
}

#define eps 1./ 1080.f
#define OCTREE_DEPTH_LEVELS 5

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

RayStruct createRay(float2 windowPos);
float traverseVoxel(RayStruct aRay, float aScale = 1.f);

int traverseOctree(RayStruct aRay);
float2 intersectAABB(RayStruct aRay, float3 boxMin, float3 boxMax);

NodeData getNodeAtOffset(int aOffset);
int traverseNode(NodeData aNode, RayStruct aRay, int aScale);


[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    float2 WindowLocal = ((float2) DTid.xy / maxThreadIter.xy);  
    RayStruct myRay = createRay(WindowLocal);
    
    //check if ray intersects octree
    int color = traverseOctree(myRay);
        
    if (color != -1)
    {
        OutputTexture[DTid.xy] = float4(1, 1, 1, 1);
        return;
    }
    
    OutputTexture[DTid.xy] = float4(myRay.direction, 1);
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

float traverseVoxel(RayStruct aRay, float aScale)
{
    float3 t;
    
    float3 delta = aRay.rayDelta;
    
    //get x dist
    float absValueX = abs(aRay.origin.x);
    
    float lowX = absValueX - absValueX % aScale;
    float highX = lowX + aScale;
    
    float resultX = aRay.direction.x < 0. ? lowX : highX;
    t.x = abs(resultX - aRay.origin.x) * delta.x;
    
    //get y dist
    float absValueY = abs(aRay.origin.y);

    float lowY = absValueY - absValueY % aScale;
    float highY = lowY + aScale;
    
    float resultY = aRay.direction.y < 0. ? lowY : highY;
    t.y = abs(resultY - aRay.origin.y) * delta.y;
 
    //get z dist
    float absValueZ = abs(aRay.origin.z);

    float lowZ = absValueZ - absValueZ % aScale;
    float highZ = lowZ + aScale;
    
    float resultZ = aRay.direction.z < 0. ? lowZ : highZ;
    t.z = abs(resultZ - aRay.origin.z) * delta.z;
    
    //// get x dist
    //t.x = aRay.direction.x < 0. ? ((aRay.origin.x / aScale - floor(aRay.origin.x / aScale)) * aScale) * delta.x
    //                : ((ceil(aRay.origin.x / aScale) - aRay.origin.x / aScale) * aScale) * delta.x;
                    
    //// get y dist
    //t.y = aRay.direction.y < 0. ? ((aRay.origin.y / aScale - floor(aRay.origin.y / aScale)) * aScale) * delta.y
    //                : ((ceil(aRay.origin.y / aScale) - aRay.origin.y / aScale) * aScale) * delta.y;
    
    //// get z dist
    //t.z = aRay.direction.z < 0. ? ((aRay.origin.z / aScale - floor(aRay.origin.z / aScale)) * aScale) * delta.z
    //                : ((ceil(aRay.origin.z / aScale) - aRay.origin.z / aScale) * aScale) * delta.z;

    return min(t.x, min(t.y, t.z)) + 0.001f;
}

int traverseOctree(RayStruct aRay)
{
    float2 result = intersectAABB(aRay, float3(0, 0, 0), float3(16, 16, 16));
    
    if (result.x < result.y && result.x > 0)
    {
        NodeData myNode = getNodeAtOffset(0);
        
        float3 intersectPos = aRay.origin + aRay.direction * result.x;
        RayStruct myRay;
        myRay.origin = intersectPos + aRay.direction * 0.01f;
        myRay.direction = aRay.direction;
        myRay.rayDelta = aRay.rayDelta;
        
        return traverseNode(myNode, myRay, 16);
    }
    
    return -1;
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

int traverseNode(NodeData aNode, RayStruct aRay, int aScale)
{
    TraversalItem traversalStack[OCTREE_DEPTH_LEVELS];
    int stackPointer = 1;
    
    TraversalItem myInitialItem;
    myInitialItem.aNode = aNode;
    myInitialItem.aScale = aScale;
    
    traversalStack[0] = myInitialItem;
       
    int3 octantCorner = int3(0, 0, 0);
    
    RayStruct myRay = aRay;
    while (stackPointer > 0)
    {
        NodeData currentNode = traversalStack[stackPointer - 1].aNode;
        int currentScale = traversalStack[stackPointer - 1].aScale;
        
        if (!currentNode.filled)
        {
            //move ray to go to outside node, since we don't want to check for it again
            float deltaDistance = traverseVoxel(myRay, currentScale);
            myRay.origin += myRay.direction * deltaDistance;
            
            octantCorner = removeOffsetAtScale(octantCorner, currentScale);

            stackPointer--;
            continue;
        }
        
        if (currentScale == 1)
        {
            return 1;
            //return currentNode.color;
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
    return -1;
}
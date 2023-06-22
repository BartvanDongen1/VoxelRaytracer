//#include "octreeDefaults.hlsli"
#include "octree/octreeDefaults.hlsli"

struct VoxelTraverseResult
{
    float distance;
    float3 normal;
};

VoxelTraverseResult traverseVoxel(RayStruct aRay, float aScale = 1.f);

float2 intersectAABB(RayStruct aRay, float3 boxMin, float3 boxMax);

HitResult traverseNode(RayStruct aRay);

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
    if (results[1].distance < min.distance)
        min = results[1];
    if (results[2].distance < min.distance)
        min = results[2];
    
    min.distance += 0.0001f;
    
    return min;
}

HitResult ray_octree_traversal(RayStruct aRay)
{
    int scale = 1 << (octreeLayerCount - 1);
    
    float2 result = intersectAABB(aRay, float3(0, 0, 0), float3(scale, scale, scale));
    
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

        HitResult hit = traverseNode(myRay);
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

OctreeNode getNodeAtIndexAndOffset(const int aIndex, const int aOffset)
{
    return flatOctreeNodes[aIndex].nodes[aOffset];
}

struct TraversalItem
{
    OctreeNode aNode;
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

bool isPositionNotInOctantAtScale(float3 aPosition, int3 aOctantCorner, int aScale)
{
    return aPosition.x - aOctantCorner.x > aScale || aPosition.x - aOctantCorner.x < 0 ||
           aPosition.y - aOctantCorner.y > aScale || aPosition.y - aOctantCorner.y < 0 ||
           aPosition.z - aOctantCorner.z > aScale || aPosition.z - aOctantCorner.z < 0;
}

HitResult traverseNode(RayStruct aRay)
{
    TraversalItem traversalStack[MAX_STACK_SIZE];
    int stackPointer = 1;
    
    TraversalItem myInitialItem;
    myInitialItem.aNode = getNodeAtIndexAndOffset(0, 0);
    myInitialItem.aScale = 1 << (octreeLayerCount - 1);
    traversalStack[0] = myInitialItem;
    
    int3 octantCorner = int3(0, 0, 0);
    
    RayStruct myRay = aRay;
    float distance = 0.0f;
    
    int loopCount = 0;
    while (stackPointer > 0)
    {
        loopCount++;
        
        OctreeNode currentNode = traversalStack[stackPointer - 1].aNode;
        int currentScale = traversalStack[stackPointer - 1].aScale;
        
        if (currentScale == 1)
        {
            HitResult result = hitResultDefault();
            result.hitDistance = distance - 0.00011f;
            
            return result;
        }
        
        // calculate first octant intersection
        int3 initialOctant = calculateOctantFromOffsetPosition(myRay.origin - octantCorner, currentScale);
        int initialOctantOffset = calculateOffsetFromOctant(initialOctant);
        
        if (isPositionNotInOctantAtScale(myRay.origin, octantCorner, currentScale))
        {
            //we fell outside the current node, so we have to go one up
            octantCorner = removeOffsetAtScale(octantCorner, currentScale);
            
            stackPointer--;
            continue;
        }
        
        if (currentNode.children & (1 << initialOctantOffset))
        {
            // filled
            octantCorner = addOffsetAtScale(octantCorner, initialOctant, currentScale);
            
            //traverse inside new node
            TraversalItem myItem;
            myItem.aNode = getNodeAtIndexAndOffset(currentNode.childrenIndex, initialOctantOffset);
            myItem.aScale = currentScale >> 1;
            traversalStack[stackPointer++] = myItem;
            continue;
        }
        
        int loop2 = 0;
        bool done = false;
        while (!isPositionNotInOctantAtScale(myRay.origin, octantCorner, currentScale) && !done)
        {
            loop2++;
                        
            //find new octant
            VoxelTraverseResult traverseResult = traverseVoxel(myRay, currentScale >> 1);
            distance += traverseResult.distance;
            
            myRay.origin = aRay.origin + myRay.direction * distance;
            
            // calculate first octant intersection
            int3 currentOctant = calculateOctantFromOffsetPosition(myRay.origin - octantCorner, currentScale);
            int currentOctantOffset = calculateOffsetFromOctant(currentOctant);
            
            if (currentNode.children & (1 << currentOctantOffset))
            {
                // filled
                octantCorner = addOffsetAtScale(octantCorner, initialOctant, currentScale);
            
                //traverse inside new node
                TraversalItem myItem;
                myItem.aNode = getNodeAtIndexAndOffset(currentNode.childrenIndex, initialOctantOffset);
                myItem.aScale = currentScale >> 1;
                traversalStack[stackPointer++] = myItem;
                
                done = true;
            }
        }
        
        if (!done)
        {
            // fell outside of the node
            octantCorner = removeOffsetAtScale(octantCorner, currentScale);
            stackPointer--;
        }
    }
    
    return hitResultDefault();
}
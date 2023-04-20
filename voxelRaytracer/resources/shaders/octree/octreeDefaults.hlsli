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

cbuffer octreeConstantBuffer : register(b1)
{
    int octreeLayerCount;
    int octreeSize;
}

struct HitResult
{
    float hitDistance;
    float3 hitNormal;
    OctreeItem item;
    
    int loopCount;
};

OctreeItem itemDefault()
{
    OctreeItem result;
    result.color = float3(0, 0, 0);
    
    return result;
}

#define FLOAT_MAX 3.402823466e+38F

HitResult hitResultDefault()
{
    HitResult result;
    result.hitDistance = FLOAT_MAX;
    result.loopCount = 0;
    result.hitNormal = float3(0, 0, 0);
    result.item = itemDefault();
    return result;
}
 
struct RayStruct
{
    float3 origin;
    float3 direction;
    
    float3 rayDelta;
};

HitResult ray_octree_traversal(RayStruct aRay);
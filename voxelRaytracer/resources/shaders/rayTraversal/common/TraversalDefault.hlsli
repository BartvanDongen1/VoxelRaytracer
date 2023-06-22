// octree structure
struct OctreeNode
{
    unsigned int childrenIndex;
    unsigned int children;
    unsigned int parentIndex;
    unsigned int parentOctant; // first 3 bits used for parent octant, 8 bits for item index
};

#define GET_OCTREE_PARENT_OCTANT(data) (data & 0x7)
#define GET_OCTREE_ITEM_INDEX(data) (data & (0xFF << 3))


struct OctreeNodes
{
    OctreeNode nodes[8];
};

StructuredBuffer<OctreeNodes> flatOctreeNodes : register(t1);

cbuffer octreeConstantBuffer : register(b1)
{
    int octreeLayerCount;
    int octreeSize;
}

// DDA structure

cbuffer voxelGridConstantBuffer : register(b2)
{
    const uint4 voxelGridSize;
    const uint4 topLevelChunkSize;
    
    const uint voxelAtlasOffset;
}

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

StructuredBuffer<int> topLevelGrid : register(t2);
StructuredBuffer<Layer1Chunk> Level1Grid : register(t3);
StructuredBuffer<Layer2Chunk> Level2Grid : register(t4);

//common traversal

#define FLOAT_MAX 3.402823466e+38F

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

struct RayStruct
{
    float3 origin;
    float3 direction;
    
    float3 rayDelta;
};

HitResult traverseRay(RayStruct aRay);
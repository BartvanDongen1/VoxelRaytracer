//#include "octreeDefaults.hlsli"
#include "octree/octreeDefaults.hlsli"

int first_node(const float tx0, const float ty0, const float tz0, const float txm, const float tym, const float tzm)
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

int new_node(const float txm, const int x, const float tym, const int y, const float tzm, const int z)
{
// optimization 2
    const int outputs[8] =
    {
        z,  // txm > tym, tym > tzm, tzm > txm, use z for edge case
        x,  // txm < tym, tym > tzm, tzm > txm, x smallest
        y,  // txm > tym, tym < tzm, tzm > txm, y smallest
        x,  // txm < tym, tym < tzm, tzm > txm, x smallest
        z,  // txm > tym, tym > tzm, tzm < txm, z smallest
        z,  // txm < tym, tym > tzm, tzm < txm, z smallest
        y,  // txm > tym, tym < tzm, tzm < txm, y smallest
        z   // txm < tym, tym < tzm, tzm < txm, use z for edge case
    };
    
    //outputs[0] = z; // txm > tym, tym > tzm, tzm > txm, use z for edge case
    //outputs[1] = x; // txm < tym, tym > tzm, tzm > txm, x smallest
    //outputs[2] = y; // txm > tym, tym < tzm, tzm > txm, y smallest
    //outputs[3] = x; // txm < tym, tym < tzm, tzm > txm, x smallest
    //outputs[4] = z; // txm > tym, tym > tzm, tzm < txm, z smallest
    //outputs[5] = z; // txm < tym, tym > tzm, tzm < txm, z smallest
    //outputs[6] = y; // txm > tym, tym < tzm, tzm < txm, y smallest
    //outputs[7] = z; // txm < tym, tym < tzm, tzm < txm, use z for edge case
    
    const int index = (int) (txm < tym) + ((int) (tym < tzm) << 1) + ((int) (tzm < txm) << 2);
    
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

    int inLoopAndcurrNode; // 10th - 6th bits = size, 5th bit = inloop bool, 1st 4 bits = currNode
 
    int nodeValues; // highest 8 bits are child flags, lower 24 bits are child index
};

#define CHILD_INDEX(value) (value & 0x00FFFFFF) 
#define CHILDREN_FLAGS(value) (value >> 24)

#define GET_SIZE(value) ((value >> 5) & 0x1F)
#define MASK_SIZE(value) (value & (0x1F << 5))
#define SIZE_OFFSET(value) (value << 5)

#define MASK_LOOP_BOOL(value) (value & (0x1 << 4))

OctreeNode getNodeAtIndexAndOffset(const int aIndex, const int aOffset)
{
    return flatOctreeNodes[aIndex].nodes[aOffset];
}

OctreeItem getItemAtIndexAndOffset(const int aIndex, const int aOffset)
{
    return flatOctreeItems[aIndex].items[aOffset];
}

TraversalItem initializeTraversalItem(const float tx0, const float ty0, const float tz0, const float tx1, const float ty1, const float tz1, const OctreeNode aNode, const int size)
{
    TraversalItem myReturnItem;
    
    myReturnItem.nodeValues = (aNode.children << 24) + aNode.childrenIndex;
    
    myReturnItem.tx0 = tx0;
    myReturnItem.ty0 = ty0;
    myReturnItem.tz0 = tz0;
    
    myReturnItem.tx1 = tx1;
    myReturnItem.ty1 = ty1;
    myReturnItem.tz1 = tz1;

    myReturnItem.inLoopAndcurrNode = SIZE_OFFSET(size);
    
    return myReturnItem;
}

HitResult proc_subtree(const float tx0, const float ty0, const float tz0, const float tx1, const float ty1, const float tz1, const unsigned int a)
{
    int loopCount;
    
    //setup stack
    TraversalItem traversalStack[MAX_STACK_SIZE];
    int stackPointer = 1;
    
    TraversalItem myInitialItem = initializeTraversalItem(tx0, ty0, tz0, tx1, ty1, tz1, getNodeAtIndexAndOffset(0, 0), octreeLayerCount); // top level node is at 0,0
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
        
        const int itemIndex = stackPointer - 1;
        TraversalItem currentItem = traversalStack[itemIndex];
        
        const float myTxm = 0.5 * (currentItem.tx0 + currentItem.tx1);
        const float myTym = 0.5 * (currentItem.ty0 + currentItem.ty1);
        const float myTzm = 0.5 * (currentItem.tz0 + currentItem.tz1);

        // skip computation if it's already been done
        if (!(bool) MASK_LOOP_BOOL(currentItem.inLoopAndcurrNode)) // mask bool with 0b10000 -> 0x10
        {
            if (currentItem.tx1 < 0 || currentItem.ty1 < 0 || currentItem.tz1 < 0)
            {
                stackPointer--;
                continue;
            }

            if (GET_SIZE(currentItem.inLoopAndcurrNode) == 1)
            {
                HitResult hit = hitResultDefault();
                
                //const float maxValue = min(min(myTxm, myTym), myTzm);
                //float dist = 0.f;
                
                //if (maxValue == myTxm)
                //{
                //    dist = sqrt(myTym * myTym + myTzm * myTzm);
                //}
                //else if (maxValue == myTym)
                //{
                //    dist = sqrt(myTxm * myTxm + myTzm * myTzm);
                //}
                //else
                //{
                //    dist = sqrt(myTxm * myTxm + myTym * myTym);
                //}
                
                //hit.hitDistance = sqrt(currentItem.tx0 * currentItem.tx0 + currentItem.ty0 * currentItem.ty0 + currentItem.tz0 * currentItem.tz0);
                hit.hitDistance = max(max(currentItem.tx0, currentItem.ty0), currentItem.tz0);
                
                OctreeItem myReturnItem;
                myReturnItem.color = float3(1, 1, 1);
                
                hit.item = myReturnItem;
                
                //reached leaf node
                return hit;
            }

            currentItem.inLoopAndcurrNode = first_node(currentItem.tx0, currentItem.ty0, currentItem.tz0, myTxm, myTym, myTzm) | 0x10 | MASK_SIZE(currentItem.inLoopAndcurrNode);
        }

        const int index = (currentItem.inLoopAndcurrNode & 0xF) ^ a;
        
        const bool flag1 = currentItem.inLoopAndcurrNode & 0x1;
        const bool flag2 = (currentItem.inLoopAndcurrNode & 0x2) >> 1;
        const bool flag3 = (currentItem.inLoopAndcurrNode & 0x4) >> 2;
        
        const float mytx1 = myTxm * (int) (!flag3) + currentItem.tx1 * (int) flag3;
        const float myty1 = myTym * (int) (!flag2) + currentItem.ty1 * (int) flag2;
        const float mytz1 = myTzm * (int) (!flag1) + currentItem.tz1 * (int) flag1;
        
        const int myX = ((currentItem.inLoopAndcurrNode & 0xF) + 4) * (int) (!flag3) + 8 * (int) flag3;
        const int myY = ((currentItem.inLoopAndcurrNode & 0xF) + 2) * (int) (!flag2) + 8 * (int) flag2;
        const int myZ = ((currentItem.inLoopAndcurrNode & 0xF) + 1) * (int) (!flag1) + 8 * (int) flag1;
        
        const int newNodeIndex = new_node(mytx1, myX, myty1, myY, mytz1, myZ);
        currentItem.inLoopAndcurrNode = newNodeIndex | MASK_LOOP_BOOL(currentItem.inLoopAndcurrNode) | MASK_SIZE(currentItem.inLoopAndcurrNode); // make sure to keep the hidden bool when assigning values

        // we can preemptively throw away the current node to prevent extra computation if currnode == 8
        stackPointer = stackPointer - (int) (newNodeIndex == 8);

        // update the current item
        traversalStack[itemIndex] = currentItem;
        
        if (CHILDREN_FLAGS(currentItem.nodeValues) & (1 << index))
        {
            //if child exists     
            const OctreeNode myNode = getNodeAtIndexAndOffset(CHILD_INDEX(currentItem.nodeValues), index);
            
            const float mytx0 = currentItem.tx0 * (int) (!flag3) + myTxm * (int) flag3;
            const float myty0 = currentItem.ty0 * (int) (!flag2) + myTym * (int) flag2;
            const float mytz0 = currentItem.tz0 * (int) (!flag1) + myTzm * (int) flag1;
            
            // traverse inside new node
            const TraversalItem myItem = initializeTraversalItem(mytx0, myty0, mytz0, mytx1, myty1, mytz1, myNode, GET_SIZE(currentItem.inLoopAndcurrNode) - 1);
            traversalStack[stackPointer++] = myItem;
        }
        
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

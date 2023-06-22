RWTexture2D<float4> OutputTexture : register(u0);

cbuffer constantBuffer : register(b0)
{
    float4 maxThreadIter;
    
    float4 camPosition;
    float4 camDirection;
    
    float4 camUpperLeftCorner;
    float4 camPixelOffsetHorizontal;
    float4 camPixelOffsetVertical;
}

struct RayStruct
{
    float3 origin;
    float3 direction;
};

//////////////////////////////////////////////////////
// Octree Basic [Commented] By Yusef28
// Purpose: Demonstrates A simplified Octree Shader Implentation
// Well commented and fit for some educational purposes
// License: Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

#define pi 3.1415926535
#define eps 1./ 1080.f
#define MAX_LEVEL 8.
#define FAR 100.

//  1 out, 3 in... from dave hoskins for the quad/octree
float hash13(float3 p3)
{
    float3 temp = p3 * .1031;
    
    p3 = temp - floor(temp);
    p3 += dot(p3, p3.zyx + 31.32);
    
    float3 temp3 = (p3.x + p3.y) * p3.z;
    
    return temp3 - floor(temp3);
}

// SHORTER, NEATER, BRANCHLESS, UNIQUE, SINGULAR PURPOSE
// This quad/octree map function returns the largest scaling factor 
//for a given posision
// the inverse of that is the cell size
float mapQ(float3 p)
{
    float s = 0.5;
    for (float i = 1.; i < MAX_LEVEL; i++)
    {
        s *= 2.;
    //if we don't pass the random check, add max to index so we break
        i += step(hash13(floor(p * s)), 0.5) * MAX_LEVEL;
    }
    return s;
}

//This DDA algo is based on the lodev Raycasting tutorial
// https://lodev.org/cgtutor/raycasting.html
//and the modification is from my quadtree traversal
//shader: https://www.shadertoy.com/view/7dVSRh
//I explain the idea in the comments.
//basically though we find the first axis the ray will 
//hit when traveling in the ray direction and return the 
//distance to that axis.
//Firsst we get the "scale factor" from the qaudtree map so
//we know what size box we are in. Then we find our distances
//for each axis and return the smallest. More detail is given in
//the link above.
float calcT(float3 p, float3 rd, float3 delta)
{
    float s = mapQ(p);
    float3 t;
    
    t.x = rd.x < 0. ? ((p.x * s - floor(p.x * s)) / s) * delta.x
                    : ((ceil(p.x * s) - p.x * s) / s) * delta.x;
                    
    t.y = rd.y < 0. ? ((p.y * s - floor(p.y * s)) / s) * delta.y
                    : ((ceil(p.y * s) - p.y * s) / s) * delta.y;
    
    t.z = rd.z < 0. ? ((p.z * s - floor(p.z * s)) / s) * delta.z
                    : ((ceil(p.z * s) - p.z * s) / s) * delta.z;

                    //+0.01 to get rid of a bunch of artifacts
    return min(t.x, min(t.y, t.z)) +0.01;
}


RayStruct createRay(float2 windowPos);
float4 sampleRay(RayStruct aRay);
float4 traversOctree(RayStruct aRay);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    float2 WindowLocal = ((float2) DTid.xy / maxThreadIter.xy);  
    RayStruct myRay = createRay(WindowLocal);
    
    //get the delta which is explained in the lodev tutorial
    //on raycasting: https://lodev.org/cgtutor/raycasting.html
    float3 delta = 1. / max(abs(myRay.direction), eps);
    float t = 0.;
    
    //It all comes together in this small iteration loop.
    //We are updating t just like in raymarching.
    //The only "sdf" we use though is the "hole" rect tunnel
    //We only render a block if our hash returns < 0.4 AND
    // the hit point of the block is outside of the sdf (hole > 0.001)
    for (float i = 0.; i < FAR; i++)
    {
        float3 pos = myRay.origin + myRay.direction * t;
        float ss = mapQ(pos);

        //create the hole / shield so blocks stop
        //hitting me in the face
        float3 shi = abs(floor(pos - myRay.origin)) - 1.;
        //the hole. Basically a vertical rectangular/square tunnel
        float hole = max(shi.x, shi.z);
        
        //n = calcN(pos, rd, delta);
        //check the hole AND check the random missing squares
        //if we are out of the hole AND not hitting a missint square
        //then break!
        if (hole > 0.001 && hash13(floor((pos) * ss)) > .4)
            break;
        
        //updating t with the modified DDA algo.
        t += calcT(pos, myRay.direction, delta);
    }

    float temp7 = clamp(1.f - t / 7.f, 0.f, 1.f);
    float3 temp8 = float3(4.f, 4.f, 4.f);
    float3 temp9 = float3(temp7, temp7, temp7) * float3(1.3f, 1.3f, 1.3f);
    float3 temp10 = pow(temp9, temp8);
    
    OutputTexture[DTid.xy] = float4(temp10, 1.f);
}

RayStruct createRay(float2 windowPos)
{
    RayStruct myRay;
    
    myRay.origin = (camUpperLeftCorner + camPixelOffsetHorizontal * windowPos.x + camPixelOffsetVertical * windowPos.y).xyz;
    myRay.direction = normalize(myRay.origin);
    myRay.origin += camPosition;
    
    return myRay;
}
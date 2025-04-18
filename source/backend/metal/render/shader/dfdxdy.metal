#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct constBuffer
{
    int4 size;
};

struct destBuffer
{
    float data[1];
};

struct sourceBuffer0
{
    float data[1];
};

struct destBuffer1
{
    float data[1];
};

constant uint3 gl_WorkGroupSize [[maybe_unused]] = uint3(256u, 1u, 1u);

kernel void main0(device destBuffer& uDx [[buffer(0)]], device destBuffer1& uDy [[buffer(1)]], const device sourceBuffer0& uInput [[buffer(2)]], constant constBuffer& uConstant [[buffer(3)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    int pos = int(gl_GlobalInvocationID.x);
    int4 size = uConstant.size;
    int total = ((size.x * size.y) * size.z) * size.w;
    if (pos < total)
    {
        int x = pos % size.x;
        int tmp = pos / size.x;
        int y = tmp % size.y;
        tmp /= size.y;
        int z = tmp % size.z;
        int n = tmp / size.z;
        int outPos = (((((n * size.x) * size.y) * size.z) + (x * size.z)) + ((y * size.x) * size.z)) + z;
        int xDPos = (((((n * size.x) * size.y) * size.z) + ((x + 1) * size.z)) + ((y * size.x) * size.z)) + z;
        int yDPos = (((((n * size.x) * size.y) * size.z) + (x * size.z)) + (((y + 1) * size.x) * size.z)) + z;
        if (x < (size.x - 1))
        {
            uDx.data[outPos] = uInput.data[xDPos] - uInput.data[outPos];
        }
        else
        {
            uDx.data[outPos] = 0.0;
        }
        if (y < (size.y - 1))
        {
            uDy.data[outPos] = uInput.data[yDPos] - uInput.data[outPos];
        }
        else
        {
            uDy.data[outPos] = 0.0;
        }
    }
}


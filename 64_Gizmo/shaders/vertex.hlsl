#include "common.hlsl"

[[vk::push_constant]] struct PushConstants pc;

[[vk::binding(0, 1)]]
cbuffer CameraData
{
    SBasicViewParameters params;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.color = input.color;
    output.position = mul(params.MVP, input.position);

    return output;
}
#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

layout(binding = 0) uniform entity_group_uniforms
{
    vec3 Color;
} EntityGroupUniforms;

#if VERTEX_SHADER

layout(location = 0) in float InPosX;
layout(location = 1) in float InPosY;
layout(location = 2) in float InPosZ;

out gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
};

void main()
{
    // NOTE: We assume input vertices here are already transformed since we are rendering points
    gl_Position = vec4(InPosX, InPosY, InPosZ, 1);
    gl_PointSize = 2.0f;
}

#endif

#if FRAGMENT_SHADER

layout(location = 0) out vec4 OutColor;

void main()
{
    OutColor = vec4(EntityGroupUniforms.Color, 1);
}

#endif

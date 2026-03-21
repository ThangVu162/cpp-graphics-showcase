#version 330 core

layout(location = 0) in vec4 osg_Vertex;
layout(location = 2) in vec3 osg_Normal;

uniform samplerBuffer instanceData;
uniform mat4 osg_ModelViewProjectionMatrix;
uniform mat4 osg_ModelViewMatrix;
uniform mat3 osg_NormalMatrix;

out vec3 vNormal;
out vec3 vFragPos;
out vec4 vColor;

void main()
{
    int base = gl_InstanceID * 5;

    vec4 c0 = texelFetch(instanceData, base + 0);
    vec4 c1 = texelFetch(instanceData, base + 1);
    vec4 c2 = texelFetch(instanceData, base + 2);
    vec4 c3 = texelFetch(instanceData, base + 3);
    mat4 modelMat = mat4(c0, c1, c2, c3);

    vColor      = texelFetch(instanceData, base + 4);
    vec4 worldPos   = modelMat * osg_Vertex;
    vNormal     = normalize(osg_NormalMatrix * mat3(modelMat) * osg_Normal);
    vFragPos    = vec3(osg_ModelViewMatrix * worldPos);
    gl_Position = osg_ModelViewProjectionMatrix * worldPos;
}

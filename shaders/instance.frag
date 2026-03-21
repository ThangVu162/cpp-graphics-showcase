#version 330 core

in vec3 vNormal;
in vec3 vFragPos;
in vec4 vColor;

out vec4 fragColor;

void main()
{
    vec3 lightDir  = normalize(vec3(1.0, 2.0, 1.5));
    vec3 N         = normalize(vNormal);

    float ambient  = 0.2;
    float diffuse  = max(dot(N, lightDir), 0.0) * 0.7;

    vec3 viewDir   = normalize(-vFragPos);
    vec3 halfVec   = normalize(lightDir + viewDir);
    float specular = pow(max(dot(N, halfVec), 0.0), 48.0) * 0.25;

    vec3 col = vColor.rgb * (ambient + diffuse) + vec3(specular);
    fragColor = vec4(col, 1.0);
}

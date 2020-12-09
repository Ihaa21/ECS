
// TODO: Rename to phong point light
vec3 PhongLighting(vec3 Diffuse, vec3 Specular, float SpecularPower, vec3 Pos, vec3 Normal, vec3 CameraPos, vec3 LightPos,
                   vec3 LightIntensity)
{
    vec3 L = normalize(LightPos - Pos);
    vec3 V = normalize(CameraPos - Pos);
    vec3 R = normalize(reflect(-L, Normal));

    float dotLN = dot(L, Normal);
    float dotRV = dot(R, V);

    if (dotLN < 0)
    {
        // NOTE: Light not visible
        return vec3(0, 0, 0);
    }

    if (dotRV < 0)
    {
        return LightIntensity * (Diffuse * dotLN);
    }

    return LightIntensity * (Diffuse * dotLN + Specular * pow(dotRV, SpecularPower));
}

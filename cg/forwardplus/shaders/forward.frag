#version 430 core
layout(location = 0) out vec4 fragColor;
in vec3 posWS;

const int MAX_LIGHTS = 200;

struct Light {
    vec4 posWS;
    vec4 color;
    float radius;
    float pad[3];
};

layout(std140, binding = 1) uniform LightUBO {
    Light lights[MAX_LIGHTS];
};

struct TileLightList {
    int count;
    int indices[64];
};

layout(std430, binding = 0) readonly buffer TileBuffer {
    TileLightList lists[];
};

uniform ivec2 uScreenSize;

void main()
{
    // 基础漫反射颜色（物体本身颜色）
    vec3 baseColor = vec3(0.9, 0.85, 0.8);
    vec3 finalColor = baseColor * 0.1;

    // 法线固定向上，简单效果
    vec3 N = vec3(0.0, 1.0, 0.0);

    ivec2 tilePos = ivec2(gl_FragCoord.xy) / 16;
    uint tileID = tilePos.y * (uScreenSize.x / 16) + tilePos.x;
    TileLightList list = lists[tileID];

    for (int i = 0; i < list.count; i++)
    {
        int lid = list.indices[i];
        Light L = lights[lid];

        vec3 Lv = L.posWS.xyz - posWS;
        float dist = length(Lv);

        if (dist > L.radius)
            continue;

        float ndl = max(dot(normalize(Lv), N), 0.0);
        float att = 1.0 - dist / L.radius;
        att *= att;

        finalColor += baseColor * L.color.rgb * ndl * att * 0.4;
    }

    fragColor = vec4(finalColor, 1.0);
}
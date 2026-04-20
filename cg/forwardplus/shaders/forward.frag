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

void main() {
    // 物体基础色
    vec3 baseColor = vec3(0.92, 0.9, 0.85);
    
    // 环境光
    vec3 finalColor = baseColor * 0.15;

    // 固定向上法线
    vec3 N = normalize(vec3(0.0, 1.0, 0.0));

    // 当前像素所属瓦片
    ivec2 tile = ivec2(gl_FragCoord.xy) / 16;
    uint tileID = tile.y * (uScreenSize.x / 16) + tile.x;
    TileLightList list = lists[tileID];

    // 只遍历当前瓦片内的光源 ✅ Forward+ 核心
    for (int i = 0; i < list.count; i++) {
        int lid = list.indices[i];
        Light L = lights[lid];

        vec3 Lvec = L.posWS.xyz - posWS;
        float dist = length(Lvec);

        if (dist > L.radius)
            continue;

        // 漫反射光照
        float ndl = max(dot(normalize(Lvec), N), 0.0);
        float att = 1.0 - (dist / L.radius);
        att *= att;

        // 叠加彩色光照
        finalColor += baseColor * L.color.rgb * ndl * att * 0.6;
    }

    fragColor = vec4(finalColor, 1.0);
}
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
    vec3 N = normalize(vec3(0.0, 1.0, 0.0));
    vec3 color = vec3(0.1); // 基础环境光

    ivec2 tilePos = ivec2(gl_FragCoord.xy) / 16;
    uint tileID = tilePos.x + tilePos.y * (uScreenSize.x / 16);
    TileLightList list = lists[tileID];

    for (int i = 0; i < list.count; i++) {
        int lid = list.indices[i];
        Light L = lights[lid];

        vec3 Lv = L.posWS.xyz - posWS;
        float dist = length(Lv);
        if (dist > L.radius) continue;

        float att = 1.0 - (dist / L.radius);
        color += L.color.rgb * max(dot(N, normalize(Lv)), 0.0) * att * 0.5;
    }

    fragColor = vec4(color, 1.0);
}
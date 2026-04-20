#version 430 core
layout(location = 0) out vec4 fragColor;

// 输入：片元世界空间坐标
in vec3 posWS;

// 最大光源数量
const int MAX_LIGHTS = 200;

// 点光源结构体：std140 对齐规则
struct Light {
    vec4 posVS;   // 光源视空间位置 (w=1 表示点光源)
    vec4 color;   // 光源颜色 (w=强度)
    float radius; // 光照影响半径
    float pad[3]; // 补位，满足std140对齐
};

// 光源UBO：存储所有光源数据
layout(std140, binding = 1) uniform LightUBO {
    Light lights[MAX_LIGHTS];
};

// 分块光源列表：每个分块最多64个光源
struct TileLightList {
    int count;       // 当前分块有效光源数量
    int indices[64];  // 光源索引列表
};

// 分块SSBO：计算着色器生成的光源索引数据
layout(std430, binding = 0) readonly buffer TileBuffer {
    TileLightList lists[];
};

// 全局参数
uniform ivec2 uScreenSize;  // 屏幕分辨率
uniform mat4 uView;         // 视图矩阵
uniform mat4 uProj;         // 新增：投影矩阵（视空间裁剪必备）

void main() {
    // 基础颜色 + 环境光 → 提高亮度
    vec3 baseColor = vec3(0.82, 0.82, 0.8);
    vec3 finalColor = baseColor * 0.3; // 环境光变亮

    // 表面法线（世界空间）
    vec3 N = normalize(vec3(0.0, 1.0, 0.0));
    vec3 N_view = normalize(mat3(uView) * N);

    // 预计算片元视空间坐标
    vec3 fragVS = (uView * vec4(posWS, 1.0)).xyz;

    // 1. 计算分块索引
    ivec2 tileXY = ivec2(gl_FragCoord.xy) / 16;
    int tileCountX = (uScreenSize.x + 15) / 16;
    uint tileIndex = uint(tileXY.y) * uint(tileCountX) + uint(tileXY.x);

    if (tileIndex >= lists.length()) {
        fragColor = vec4(finalColor, 1.0);
        return;
    }

    // 2. 获取当前分块的光源列表
    TileLightList lightList = lists[tileIndex];

    // 3. 遍历分块内的有效光源
    for (int i = 0; i < min(lightList.count, 64); ++i) {
        int lightID = lightList.indices[i];
        if (lightID < 0 || lightID >= MAX_LIGHTS) continue;

        Light light = lights[lightID];
        vec3 lightVS = light.posVS.xyz;

        vec3 L = lightVS - fragVS;
        float dist = length(L);

        if (dist > light.radius || dist < 0.01) continue;
        L /= dist;

        float NdotL = max(dot(N_view, L), 0.0);

        // 软衰减，范围大
        float attenuation = 1.0 - smoothstep(0.0, light.radius, dist);

        // 亮度正常 → 不暗、不爆
        finalColor += baseColor * light.color.rgb * NdotL * attenuation * 0.5;
    }

    // 去掉压暗的色调映射！！！这就是你变暗的元凶
    // finalColor = finalColor / (finalColor + 0.8);

    // 最终颜色输出
    fragColor = vec4(finalColor, 1.0);
}
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
    // 基础颜色 + 环境光
    vec3 baseColor = vec3(0.82, 0.82, 0.8);
    vec3 finalColor = baseColor * 0.12; // 环境光

    // 表面法线（示例：平面朝上，可替换为GBuffer法线）
    vec3 N = normalize(vec3(0.0, 1.0, 0.0));

    // 1. 计算当前片元所属的分块（16x16像素为一个分块）
    ivec2 tileXY = ivec2(gl_FragCoord.xy) / 16;
    // 分块总数量
    int tileCountX = uScreenSize.x / 16;
    // 分块线性索引
    uint tileIndex = uint(tileXY.y) * uint(tileCountX) + uint(tileXY.x);

    // 2. 获取当前分块的光源列表
    TileLightList lightList = lists[tileIndex];

    // 3. 遍历分块内的有效光源
    for (int i = 0; i < lightList.count; ++i) {
        // 获取光源索引 + 光源数据
        int lightID = lightList.indices[i];
        Light light = lights[lightID];

        // -------------------------- 核心优化：视空间直接计算 --------------------------
        // 原代码：视空间→世界空间 计算距离，性能低
        // 优化后：片元世界坐标 → 视空间坐标，直接和光源视空间坐标计算距离
        vec3 fragVS = (uView * vec4(posWS, 1.0)).xyz;
        vec3 lightVS = light.posVS.xyz;

        // 光照方向 + 距离（视空间直接计算，无逆矩阵开销）
        vec3 L = lightVS - fragVS;
        float dist = length(L);

        // 超出光照半径，跳过
        if (dist > light.radius) continue;

        // 法线·光线方向 漫反射计算
        float NdotL = max(dot(N, L / dist), 0.0);

        // 光照衰减：平滑衰减公式（比原线性衰减更自然）
        float attenuation = 1.0 - smoothstep(0.0, light.radius, dist);
        attenuation *= attenuation;

        // 光照叠加
        finalColor += baseColor * light.color.rgb * NdotL * attenuation * 0.5;
    }

    // 最终颜色输出
    fragColor = vec4(finalColor, 1.0);
}
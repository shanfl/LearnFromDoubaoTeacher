#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>

using namespace glm;
using namespace std;

const int TILE_SIZE = 16;
const int MAX_LIGHTS = 200;
const int MAX_LIGHTS_PER_TILE = 64;

struct Light {
    vec4 posWS;
    vec4 color;
    float radius;
    float pad[3];
};

struct TileLightList {
    int count;
    int indices[MAX_LIGHTS_PER_TILE];
};

float cubeVertices[] = {
    -1,-1,-1, 1,-1,-1, 1, 1,-1, -1, 1,-1,
    -1,-1, 1, 1,-1, 1, 1, 1, 1, -1, 1, 1,
};

unsigned int cubeIndices[] = {
    0,1,2,2,3,0,
    4,5,6,6,7,4,
    0,4,7,7,3,0,
    1,5,6,6,2,1,
    3,2,6,6,7,3,
    0,1,5,5,4,0
};

string readFile(const string& path) {
    ifstream file(path);
    if (!file.is_open()) {
        cerr << "ERROR: 无法读取文件 " << path << endl;
        return "";
    }
    return string((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
}

GLuint createShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, 1024, NULL, log);
        cerr << "Shader 编译失败:\n" << log << endl;
    }
    return shader;
}

GLuint createProgram(vector<GLuint> shaders) {
    GLuint program = glCreateProgram();
    for (auto s : shaders)
        glAttachShader(program, s);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(program, 1024, NULL, log);
        cerr << "Program 链接失败:\n" << log << endl;
    }
    return program;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    const int winW = 1280;
    const int winH = 720;
    GLFWwindow* window = glfwCreateWindow(winW, winH, "Forward+ 3D Light Culling", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glViewport(0, 0, winW, winH);
    glEnable(GL_DEPTH_TEST);

    // 深度纹理
    GLuint depthTex;
    glGenTextures(1, &depthTex);
    glBindTexture(GL_TEXTURE_2D, depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, winW, winH, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int tilesX = winW / TILE_SIZE;
    int tilesY = winH / TILE_SIZE;
    GLuint tileSSBO;
    glGenBuffers(1, &tileSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, tileSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, tilesX * tilesY * sizeof(TileLightList), nullptr, GL_DYNAMIC_DRAW);

    // 光源（世界空间）
    vector<Light> lights(MAX_LIGHTS);
    for (int i = 0; i < MAX_LIGHTS; i++) {
        lights[i].posWS = vec4(rand() % 60 - 30, 5.0f, rand() % 60 - 30, 1);
        lights[i].color = vec4(
            0.2f + (rand() % 5) * 0.2f,
            0.2f + (rand() % 5) * 0.2f,
            0.2f + (rand() % 5) * 0.2f,
            1.0f);
        lights[i].radius = 20.0f;
    }

    // 光源 UBO
    GLuint lightUBO;
    glGenBuffers(1, &lightUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, lightUBO);
    glBufferData(GL_UNIFORM_BUFFER, MAX_LIGHTS * sizeof(Light), lights.data(), GL_DYNAMIC_DRAW);

    // 立方体 VAO
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 加载 Shader
    string depth_vert = readFile("shaders/depth.vert");
    string depth_frag = readFile("shaders/depth.frag");
    string light_cull_comp = readFile("shaders/light_cull.comp");
    string forward_vert = readFile("shaders/forward.vert");
    string forward_frag = readFile("shaders/forward.frag");

    GLuint dvs = createShader(GL_VERTEX_SHADER, depth_vert.c_str());
    GLuint dfs = createShader(GL_FRAGMENT_SHADER, depth_frag.c_str());
    GLuint depthProg = createProgram({ dvs, dfs });

    GLuint ccs = createShader(GL_COMPUTE_SHADER, light_cull_comp.c_str());
    GLuint cullProg = createProgram({ ccs });

    GLuint fvs = createShader(GL_VERTEX_SHADER, forward_vert.c_str());
    GLuint ffs = createShader(GL_FRAGMENT_SHADER, forward_frag.c_str());
    GLuint forwardProg = createProgram({ fvs, ffs });

    // 渲染循环
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        mat4 proj = perspective(radians(60.0f), (float)winW / winH, 0.1f, 200.0f);
        mat4 view = lookAt(vec3(18, 12, 18), vec3(0, 0, 0), vec3(0, 1, 0));
        mat4 pv = proj * view;

        // ------------------------------
        // 1. 深度预渲染
        // ------------------------------
        glUseProgram(depthProg);
        glUniformMatrix4fv(glGetUniformLocation(depthProg, "uProjView"), 1, GL_FALSE, value_ptr(pv));
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glClear(GL_DEPTH_BUFFER_BIT);

        for (int i = 0; i < 25; i++) {
            mat4 model = translate(mat4(1.0f), vec3(i * 2.5f - 30, 0, 0));
            glUniformMatrix4fv(glGetUniformLocation(depthProg, "uModel"), 1, GL_FALSE, value_ptr(model));
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        // ------------------------------
        // 2. 【真正 3D 空间光源剔除】
        // ------------------------------
        glUseProgram(cullProg);

        // 传递矩阵
        glUniformMatrix4fv(glGetUniformLocation(cullProg, "uProj"), 1, GL_FALSE, value_ptr(proj));
        glUniformMatrix4fv(glGetUniformLocation(cullProg, "uInvProj"), 1, GL_FALSE, value_ptr(inverse(proj)));
        glUniform2i(glGetUniformLocation(cullProg, "uScreenSize"), winW, winH);
        glUniform1i(glGetUniformLocation(cullProg, "uLightCount"), MAX_LIGHTS);

        // 绑定深度图
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthTex);
        glUniform1i(glGetUniformLocation(cullProg, "uDepth"), 0);

        // 绑定 SSBO + UBO
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, tileSSBO);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, lightUBO);

        // 执行计算
        glDispatchCompute(tilesX, tilesY, 1);

        // ------------------------------
        // 3. 最终渲染
        // ------------------------------
        glUseProgram(forwardProg);
        glUniformMatrix4fv(glGetUniformLocation(forwardProg, "uProjView"), 1, GL_FALSE, value_ptr(pv));
        glUniform2i(glGetUniformLocation(forwardProg, "uScreenSize"), winW, winH);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, tileSSBO);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, lightUBO);

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (int i = 0; i < 25; i++) {
            mat4 model = translate(mat4(1.0f), vec3(i * 2.5f - 30, 0, 0));
            glUniformMatrix4fv(glGetUniformLocation(forwardProg, "uModel"), 1, GL_FALSE, value_ptr(model));
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>

using namespace glm;
using namespace std;

const int TILE_SIZE = 16;
const int MAX_LIGHTS = 200;

struct Light {
    vec4 posWS;    // 改名！这里存世界坐标，不是视空间
    vec4 color;
    float radius;
    float pad[3];
};

struct TileLightList {
    int count;
    int indices[64];
};

float cubeVertices[] = {
    -1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1,
    -1,-1,1, 1,-1,1, 1,1,1, -1,1,1,
};

unsigned int cubeIndices[] = {
    0,1,2, 2,3,0,
    4,5,6, 6,7,4,
    0,4,7, 7,3,0,
    1,5,6, 6,2,1,
    3,2,6, 6,7,3,
    0,1,5, 5,4,0
};

string readFile(const string& path) {
    ifstream f(path);
    if (!f) { cerr << "ERROR: 无法读取 " << path << endl; return ""; }
    return string((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
}

GLuint createShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    int ok;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(sh, 1024, NULL, log); cerr << log << endl; }
    return sh;
}

GLuint createProgram(vector<GLuint> shaders) {
    GLuint p = glCreateProgram();
    for (auto s : shaders) glAttachShader(p, s);
    glLinkProgram(p);
    int ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[1024]; glGetProgramInfoLog(p, 1024, NULL, log); cerr << log << endl; }
    return p;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    int w = 1280, h = 720;
    GLFWwindow* win = glfwCreateWindow(w, h, "Forward+ 彩色光源", NULL, NULL);
    glfwMakeContextCurrent(win);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glViewport(0, 0, w, h);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);

    int tilesX = (w + TILE_SIZE - 1) / TILE_SIZE;
    int tilesY = (h + TILE_SIZE - 1) / TILE_SIZE;

    GLuint tileSSBO;
    glGenBuffers(1, &tileSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, tileSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, tilesX * tilesY * sizeof(TileLightList), NULL, GL_DYNAMIC_DRAW);

    // 光源数据：永远存世界坐标，绝不覆盖
    vector<Light> lights(MAX_LIGHTS);
    for (int i = 0; i < MAX_LIGHTS; ++i) {
        lights[i].posWS = vec4(rand() % 60 - 30, 2.0f, rand() % 60 - 30, 1);
        lights[i].radius = 50.0f;       // 大范围
        lights[i].color = vec4(0.2f, 0.2f, 0.2f, 1.0f); // 低强度
        lights[i].pad[0] = lights[i].pad[1] = lights[i].pad[2] = 0;
    }

    // 三个显眼彩色光源
    lights[0].posWS = vec4(-10, 2, -10, 1);
    lights[0].color = vec4(1, 0.2, 0.2, 1);

    lights[1].posWS = vec4(0, 2, 10, 1);
    lights[1].color = vec4(0.2, 1, 0.2, 1);

    lights[2].posWS = vec4(10, 2, -10, 1);
    lights[2].color = vec4(0.2, 0.2, 1, 1);

    GLuint lightUBO;
    glGenBuffers(1, &lightUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, lightUBO);
    glBufferData(GL_UNIFORM_BUFFER, MAX_LIGHTS * sizeof(Light), lights.data(), GL_DYNAMIC_DRAW);

    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
    glEnableVertexAttribArray(0);

    string dv = readFile("shaders/depth.vert");
    string df = readFile("shaders/depth.frag");
    string cs = readFile("shaders/light_cull.comp");
    string fv = readFile("shaders/forward.vert");
    string ff = readFile("shaders/forward.frag");

    GLuint depthProg = createProgram({ createShader(GL_VERTEX_SHADER, dv.c_str()), createShader(GL_FRAGMENT_SHADER, df.c_str()) });
    GLuint cullProg = createProgram({ createShader(GL_COMPUTE_SHADER, cs.c_str()) });
    GLuint fwdProg = createProgram({ createShader(GL_VERTEX_SHADER, fv.c_str()), createShader(GL_FRAGMENT_SHADER, ff.c_str()) });

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();

        mat4 proj = perspective(radians(60.0f), (float)w / h, 0.1f, 200.0f);
        mat4 view = lookAt(vec3(20, 12, 20), vec3(0, 0, 0), vec3(0, 1, 0));
        mat4 pv = proj * view;

        // 每一帧用原始世界坐标计算视空间，不破坏原数据
        vector<Light> frameLights = lights;
        for (int i = 0; i < MAX_LIGHTS; ++i) {
            vec4 ws = frameLights[i].posWS;
            frameLights[i].posWS = view * ws; // 传到GPU的是视空间
        }

        glBindBuffer(GL_UNIFORM_BUFFER, lightUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, MAX_LIGHTS * sizeof(Light), frameLights.data());

        // 深度预渲染
        glUseProgram(depthProg);
        glUniformMatrix4fv(glGetUniformLocation(depthProg, "uProjView"), 1, 0, value_ptr(pv));
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glClear(GL_DEPTH_BUFFER_BIT);

        for (int i = 0; i < 25; ++i) {
            mat4 m = translate(mat4(1.0f), vec3(i * 2.5f - 30, 0, 0));
            glUniformMatrix4fv(glGetUniformLocation(depthProg, "uModel"), 1, 0, value_ptr(m));
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        // 光源剔除计算
        glUseProgram(cullProg);
        glUniformMatrix4fv(glGetUniformLocation(cullProg, "uProj"), 1, 0, value_ptr(proj));
        glUniform2i(glGetUniformLocation(cullProg, "uScreenSize"), w, h);
        glUniform1i(glGetUniformLocation(cullProg, "uLightCount"), MAX_LIGHTS);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, tileSSBO);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, lightUBO);
        glDispatchCompute(tilesX, tilesY, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // 最终渲染
        glUseProgram(fwdProg);
        glUniformMatrix4fv(glGetUniformLocation(fwdProg, "uProjView"), 1, 0, value_ptr(pv));
        glUniformMatrix4fv(glGetUniformLocation(fwdProg, "uView"), 1, 0, value_ptr(view));
        glUniform2i(glGetUniformLocation(fwdProg, "uScreenSize"), w, h);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, tileSSBO);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, lightUBO);

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (int i = 0; i < 25; ++i) {
            mat4 m = translate(mat4(1.0f), vec3(i * 2.5f - 30, 0, 0));
            glUniformMatrix4fv(glGetUniformLocation(fwdProg, "uModel"), 1, 0, value_ptr(m));
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        }

        glfwSwapBuffers(win);
    }

    glfwTerminate();
    return 0;
}
#include "PerfFramework.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <windows.h>
#include <psapi.h>

#include "PerfTimer.h"

#define GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX 0x9048
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049

using namespace std;
using namespace glm;

static GLFWwindow* window = nullptr;

size_t GetFreeMemNvidia() {
    // TODO: AMD version w/ GL_ATI_meminfo
    GLint currMem = 0;
    glGetIntegerv(GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX, &currMem);
    return (size_t)currMem * 1024;
}

size_t GetMainMemUsage() {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    return pmc.PrivateUsage;
}

PerfRecord RunPerf(std::function<void()> setupFn, std::function<void()> drawFn, std::function<void()> teardownFn) {
    int discardFrames = FRAMES_TO_DISCARD;
    int frameCount = FRAMES_TO_RECORD;

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    window = glfwCreateWindow(1920, 1080, "Voxel Perf", NULL, NULL);

    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    glewInit();
    glfwSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.12f, 0.6f, 1.0f);

    size_t gpuFreeBefore = GetFreeMemNvidia();
    size_t memUsedBefore = GetMainMemUsage();
    CheckGLErrors();

    setupFn();
    CheckGLErrors();

    size_t gpuFreeAfter = GetFreeMemNvidia();
    size_t memUsedAfter = GetMainMemUsage();
    CheckGLErrors();

    double totalFrameTime = 0.0;
    double totalRecordedFrames = frameCount;

    PerfTimer timer;
    timer.Start();
    while (frameCount > 0) {
        double frameTime = timer.Stop();
        timer.Start();
        if (discardFrames == 0) {
            if (frameCount > 0) {
                totalFrameTime += frameTime * 1000.0f;
                frameCount--;
            }
        } else {
            discardFrames--;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        drawFn();
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    teardownFn();

    glfwDestroyWindow(window);
    glfwTerminate();

    PerfRecord record;

    record.averageFrameTimeMs = totalFrameTime / totalRecordedFrames;
    record.gpuMemUsed = gpuFreeBefore - gpuFreeAfter;
    record.mainMemUsed = memUsedAfter - memUsedBefore;

    return record;
}

void CheckGLErrors() {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        cerr << "OpenGL error: " << err << endl;
    }
}

mat4 MakeModelView() {
    mat4 view = lookAt(vec3(-16, 0, -16), vec3(0, 0, 0), vec3(0, 1, 0));
    //* glm::rotate((float)glfwGetTime(), vec3(0, 1, 0));
    return view;
}

mat4 MakeProjection() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float fovY = radians(45.0f);
    mat4 projection = perspective(fovY, (float)width / (float)height, 1.0f, 10000.0f);
    return projection;
}

mat4 MakeMvp() {
    return MakeProjection() * MakeModelView();
}

GLuint MakeShaderProgram(std::vector<std::pair<std::string, GLenum>> shaders) {
    GLuint program = glCreateProgram();

    for (auto& s : shaders) {
        string src = ReadTextFile(s.first);
        GLuint shaderId = glCreateShader(s.second);
        const GLchar* srcGlChar = (const GLchar*)src.c_str();
        glShaderSource(shaderId, 1, &srcGlChar, NULL);
        glCompileShader(shaderId);
        glAttachShader(program, shaderId);
        glDeleteShader(shaderId);
        CheckGLErrors();
    }

    glLinkProgram(program);
    return program;
}

std::string ReadTextFile(std::string filename) {
    ifstream textStream(filename, std::ifstream::in);

    if (textStream.fail()) {
        cerr << "Failed to open file \"" << filename << "\"";
        return{};
    }

    string str(static_cast<stringstream const&>(stringstream() << textStream.rdbuf()).str());
    return str;
}
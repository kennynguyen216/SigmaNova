#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "shader.h"
#include <glm/glm.hpp>
#include <iostream>
#include "camera.h"

namespace {
constexpr int window_width = 800;
constexpr int window_height = 600;

void process_input(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

void framebuffer_size_callback(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
}
}

int main()
{
    float vertices[] = {
        -1.0f, -1.0f, 0.0f,  // bottom left
         1.0f, -1.0f, 0.0f,  // bottom right
         1.0f,  1.0f, 0.0f,  // top right
        -1.0f,  1.0f, 0.0f   // top left
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    if (!glfwInit()) {
        std::cout << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "SigmaNova", nullptr, nullptr);
    if (window == nullptr) {
        std::cout << "Failed to create GLFW window" << std:: endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std:: endl;
        return -1;
    }

    unsigned int vbo;
    unsigned int vao;
    unsigned int ebo;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    shader fullscreen_shader("shaders/fullscreen.vert", "shaders/fullscreen.frag");

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    camera scene_camera;

    while (!glfwWindowShouldClose(window)) {
        process_input(window);
        scene_camera.process_input(window);

        glClearColor(0.02f, 0.04f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        fullscreen_shader.use();
        fullscreen_shader.set_float("u_time", static_cast<float>(glfwGetTime()));
        fullscreen_shader.set_vec2("u_resolution", window_width, window_height);
        fullscreen_shader.set_vec3("u_camera_pos", scene_camera.position());
        fullscreen_shader.set_vec3("u_camera_forward", scene_camera.forward());
        fullscreen_shader.set_vec3("u_camera_right", scene_camera.right());
        fullscreen_shader.set_vec3("u_camera_up", scene_camera.up());




        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(fullscreen_shader.id);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

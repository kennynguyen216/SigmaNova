// glad must be included before GLFW so GLFW doesn't pull in the system OpenGL headers first
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "camera.h"
#include "shader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <vector>

namespace {
constexpr int window_width = 800;
constexpr int window_height = 600;
constexpr float camera_fov_degrees = 55.0f;

camera* active_camera = nullptr;
bool first_mouse = true;
bool mouse_dragging = false;
float last_mouse_x = static_cast<float>(window_width) * 0.5f;
float last_mouse_y = static_cast<float>(window_height) * 0.5f;

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

void mouse_callback(GLFWwindow*, double x_pos, double y_pos)
{
    float mouse_x = static_cast<float>(x_pos);
    float mouse_y = static_cast<float>(y_pos);

    if (first_mouse) {
        last_mouse_x = mouse_x;
        last_mouse_y = mouse_y;
        first_mouse = false;
    }

    float x_offset = mouse_x - last_mouse_x;
    float y_offset = last_mouse_y - mouse_y;

    last_mouse_x = mouse_x;
    last_mouse_y = mouse_y;

    if (mouse_dragging && active_camera != nullptr) {
        active_camera->process_mouse(x_offset, y_offset);
    }
}

void mouse_button_callback(GLFWwindow*, int button, int action, int)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT && button != GLFW_MOUSE_BUTTON_RIGHT) {
        return;
    }

    if (action == GLFW_PRESS) {
        mouse_dragging = true;
        first_mouse = true;
    }

    if (action == GLFW_RELEASE) {
        mouse_dragging = false;
    }
}

float fabric_height(float x, float z)
{
    float radius = std::sqrt(x * x + z * z);
    return -1.35f + 0.025f * radius * radius - 0.45f / (radius + 0.9f);
}

std::vector<float> make_fabric_grid(float size, int divisions)
{
    std::vector<float> vertices;
    float half_size = size * 0.5f;
    float step = size / static_cast<float>(divisions);

    for (int z_step = 0; z_step <= divisions; ++z_step) {
        float z = -half_size + static_cast<float>(z_step) * step;

        for (int x_step = 0; x_step < divisions; ++x_step) {
            float x_start = -half_size + static_cast<float>(x_step) * step;
            float x_end = x_start + step;

            vertices.insert(vertices.end(), { x_start, fabric_height(x_start, z), z });
            vertices.insert(vertices.end(), { x_end, fabric_height(x_end, z), z });
        }
    }

    for (int x_step = 0; x_step <= divisions; ++x_step) {
        float x = -half_size + static_cast<float>(x_step) * step;

        for (int z_step = 0; z_step < divisions; ++z_step) {
            float z_start = -half_size + static_cast<float>(z_step) * step;
            float z_end = z_start + step;

            vertices.insert(vertices.end(), { x, fabric_height(x, z_start), z_start });
            vertices.insert(vertices.end(), { x, fabric_height(x, z_end), z_end });
        }
    }

    return vertices;
}
}

int main()
{
    float vertices[] = {
        -1.0f, -1.0f, 0.0f, // bottom left
        1.0f, -1.0f, 0.0f, // bottom right
        1.0f, 1.0f, 0.0f, // top right
        -1.0f, 1.0f, 0.0f // top left
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
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    // Uncapped frame rate so the title-bar ms/frame readout reflects real shader
    // cost. Set to 1 to re-enable vsync once perf work is done.
    glfwSwapInterval(0);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
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
    shader grid_shader("shaders/grid.vert", "shaders/grid.frag");

    std::vector<float> grid_vertices = make_fabric_grid(9.0f, 54);
    unsigned int grid_vbo;
    unsigned int grid_vao;

    glGenVertexArrays(1, &grid_vao);
    glGenBuffers(1, &grid_vbo);

    glBindVertexArray(grid_vao);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(grid_vertices.size() * sizeof(float)), grid_vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    camera scene_camera;
    active_camera = &scene_camera;
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    float last_frame_time = static_cast<float>(glfwGetTime());
    float perf_accum_time = 0.0f;
    int perf_frame_count = 0;

    while (!glfwWindowShouldClose(window)) {
        // time between frames, so movement speeds stay the same at any frame rate
        float current_time = static_cast<float>(glfwGetTime());
        float delta_time = current_time - last_frame_time;
        last_frame_time = current_time;

        // average frame time in the title bar, refreshed twice a second
        perf_accum_time += delta_time;
        perf_frame_count += 1;
        if (perf_accum_time >= 0.5f) {
            float average_ms = perf_accum_time * 1000.0f / static_cast<float>(perf_frame_count);
            char title[128];
            std::snprintf(title, sizeof(title), "SigmaNova | %.2f ms/frame (%.0f FPS)",
                average_ms, static_cast<float>(perf_frame_count) / perf_accum_time);
            glfwSetWindowTitle(window, title);
            perf_accum_time = 0.0f;
            perf_frame_count = 0;
        }

        process_input(window);
        scene_camera.process_input(window, delta_time);

        // query the real framebuffer size so the shader's aspect ratio stays correct after a resize
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

        glClearColor(0.002f, 0.004f, 0.012f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        grid_shader.use();
        glm::mat4 view = glm::lookAt(scene_camera.position(), scene_camera.position() + scene_camera.forward(), scene_camera.up());
        glm::mat4 projection = glm::perspective(glm::radians(camera_fov_degrees), static_cast<float>(framebuffer_width) / static_cast<float>(framebuffer_height), 0.1f, 100.0f);
        grid_shader.set_mat4("u_view", view);
        grid_shader.set_mat4("u_projection", projection);
        grid_shader.set_vec3("u_color", glm::vec3(0.82f));

        glBindVertexArray(grid_vao);
        glDrawArrays(GL_LINES, 0, static_cast<int>(grid_vertices.size() / 3));

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        fullscreen_shader.use();
        fullscreen_shader.set_float("u_time", current_time);
        fullscreen_shader.set_float("u_fov_y", glm::radians(camera_fov_degrees));
        fullscreen_shader.set_vec2("u_resolution", static_cast<float>(framebuffer_width), static_cast<float>(framebuffer_height));
        fullscreen_shader.set_vec3("u_camera_pos", scene_camera.position());
        fullscreen_shader.set_vec3("u_camera_forward", scene_camera.forward());
        fullscreen_shader.set_vec3("u_camera_right", scene_camera.right());
        fullscreen_shader.set_vec3("u_camera_up", scene_camera.up());

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glDisable(GL_BLEND);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteVertexArrays(1, &grid_vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &grid_vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(fullscreen_shader.id);
    glDeleteProgram(grid_shader.id);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

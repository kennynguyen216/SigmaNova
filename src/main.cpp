// glad must be included before GLFW so GLFW doesn't pull in the system OpenGL headers first
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "camera.h"
#include "shader.h"
#include <algorithm>
#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string_view>
#include <vector>

namespace {
constexpr int window_width = 800;
constexpr int window_height = 600;
constexpr float camera_fov_degrees = 55.0f;
constexpr float event_flash_start = 2.8f;
float gas_noise_speed = 1.0f;
float gas_noise_scale = 1.0f;
float gas_emission_strength = 6.0f;
float gas_absorption_strength = 1.15f;
float gas_edge_raggedness = 0.45f;
float gas_halo_strength = 0.5f;
bool supernova_event_active = false;
bool space_was_pressed = false;
float supernova_event_start_time = 0.0f;
constexpr int hud_rect_vertex_count = 6;
constexpr int hud_rect_float_count = hud_rect_vertex_count * 2;

using glyph_rows = std::array<std::string_view, 7>;

camera* active_camera = nullptr;
bool first_mouse = true;
bool mouse_dragging = false;
float last_mouse_x = static_cast<float>(window_width) * 0.5f;
float last_mouse_y = static_cast<float>(window_height) * 0.5f;

void adjust_control(GLFWwindow* window, int lower_key, int raise_key, float& value, float speed, float min_value, float max_value, float delta_time)
{
    if (glfwGetKey(window, lower_key) == GLFW_PRESS) {
        value -= speed * delta_time;
    }

    if (glfwGetKey(window, raise_key) == GLFW_PRESS) {
        value += speed * delta_time;
    }

    value = std::clamp(value, min_value, max_value);
}

float control_fraction(float value, float min_value, float max_value)
{
    return std::clamp((value - min_value) / (max_value - min_value), 0.0f, 1.0f);
}

float event_shake_strength(float event_time)
{
    float flash_age = event_time - event_flash_start;
    if (flash_age < 0.0f || flash_age > 1.15f) {
        return 0.0f;
    }

    float snap_on = std::clamp(flash_age / 0.08f, 0.0f, 1.0f);
    float decay = std::exp(-flash_age * 3.2f);
    return 0.045f * snap_on * decay;
}

glyph_rows glyph_for(char c)
{
    switch (c) {
    case 'A':
        return { "01110", "10001", "10001", "11111", "10001", "10001", "10001" };
    case 'B':
        return { "11110", "10001", "10001", "11110", "10001", "10001", "11110" };
    case 'C':
        return { "01111", "10000", "10000", "10000", "10000", "10000", "01111" };
    case 'D':
        return { "11110", "10001", "10001", "10001", "10001", "10001", "11110" };
    case 'E':
        return { "11111", "10000", "10000", "11110", "10000", "10000", "11111" };
    case 'G':
        return { "01111", "10000", "10000", "10011", "10001", "10001", "01111" };
    case 'H':
        return { "10001", "10001", "10001", "11111", "10001", "10001", "10001" };
    case 'I':
        return { "11111", "00100", "00100", "00100", "00100", "00100", "11111" };
    case 'J':
        return { "00111", "00010", "00010", "00010", "10010", "10010", "01100" };
    case 'K':
        return { "10001", "10010", "10100", "11000", "10100", "10010", "10001" };
    case 'L':
        return { "10000", "10000", "10000", "10000", "10000", "10000", "11111" };
    case 'M':
        return { "10001", "11011", "10101", "10101", "10001", "10001", "10001" };
    case 'N':
        return { "10001", "11001", "10101", "10011", "10001", "10001", "10001" };
    case 'O':
        return { "01110", "10001", "10001", "10001", "10001", "10001", "01110" };
    case 'P':
        return { "11110", "10001", "10001", "11110", "10000", "10000", "10000" };
    case 'R':
        return { "11110", "10001", "10001", "11110", "10100", "10010", "10001" };
    case 'S':
        return { "01111", "10000", "10000", "01110", "00001", "00001", "11110" };
    case 'T':
        return { "11111", "00100", "00100", "00100", "00100", "00100", "00100" };
    case 'V':
        return { "10001", "10001", "10001", "10001", "10001", "01010", "00100" };
    case 'X':
        return { "10001", "10001", "01010", "00100", "01010", "10001", "10001" };
    case 'Z':
        return { "11111", "00001", "00010", "00100", "01000", "10000", "11111" };
    case '/':
        return { "00001", "00010", "00010", "00100", "01000", "01000", "10000" };
    default:
        return { "00000", "00000", "00000", "00000", "00000", "00000", "00000" };
    }
}

void process_input(GLFWwindow* window, float delta_time, float current_time)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    bool space_pressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (space_pressed && !space_was_pressed) {
        supernova_event_active = true;
        supernova_event_start_time = current_time;
    }
    space_was_pressed = space_pressed;

    adjust_control(window, GLFW_KEY_Z, GLFW_KEY_X, gas_emission_strength, 4.0f, 0.0f, 15.0f, delta_time);
    adjust_control(window, GLFW_KEY_C, GLFW_KEY_V, gas_absorption_strength, 1.0f, 0.0f, 3.0f, delta_time);
    adjust_control(window, GLFW_KEY_B, GLFW_KEY_N, gas_edge_raggedness, 0.45f, 0.05f, 0.9f, delta_time);
    adjust_control(window, GLFW_KEY_K, GLFW_KEY_L, gas_noise_speed, 0.75f, 0.0f, 3.0f, delta_time);
    adjust_control(window, GLFW_KEY_O, GLFW_KEY_P, gas_noise_scale, 0.75f, 0.2f, 3.0f, delta_time);
    adjust_control(window, GLFW_KEY_H, GLFW_KEY_J, gas_halo_strength, 0.75f, 0.0f, 2.0f, delta_time);
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
    glfwSwapInterval(1);

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

    shader sky_shader("shaders/fullscreen.vert", "shaders/sky.frag");
    shader fullscreen_shader("shaders/fullscreen.vert", "shaders/fullscreen.frag");
    shader grid_shader("shaders/grid.vert", "shaders/grid.frag");
    shader hud_shader("shaders/hud.vert", "shaders/hud.frag");

    std::vector<float> grid_vertices = make_fabric_grid(9.0f, 36);
    unsigned int grid_vbo;
    unsigned int grid_vao;

    glGenVertexArrays(1, &grid_vao);
    glGenBuffers(1, &grid_vbo);

    glBindVertexArray(grid_vao);
    glBindBuffer(GL_ARRAY_BUFFER, grid_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(grid_vertices.size() * sizeof(float)), grid_vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    unsigned int hud_vbo;
    unsigned int hud_vao;

    glGenVertexArrays(1, &hud_vao);
    glGenBuffers(1, &hud_vbo);

    glBindVertexArray(hud_vao);
    glBindBuffer(GL_ARRAY_BUFFER, hud_vbo);
    glBufferData(GL_ARRAY_BUFFER, hud_rect_float_count * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
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
            char title[256];
            std::snprintf(title, sizeof(title),
                "SigmaNova | %.2f ms/frame (%.0f FPS) | Event %.1f | E %.2f A %.2f Edge %.2f Noise %.2f/%.2f Halo %.2f",
                average_ms,
                static_cast<float>(perf_frame_count) / perf_accum_time,
                supernova_event_active ? current_time - supernova_event_start_time : 0.0f,
                gas_emission_strength,
                gas_absorption_strength,
                gas_edge_raggedness,
                gas_noise_speed,
                gas_noise_scale,
                gas_halo_strength);
            glfwSetWindowTitle(window, title);
            perf_accum_time = 0.0f;
            perf_frame_count = 0;
        }

        process_input(window, delta_time, current_time);
        scene_camera.process_input(window, delta_time);

        float event_time = supernova_event_active ? current_time - supernova_event_start_time : 0.0f;
        float shake_strength = supernova_event_active ? event_shake_strength(event_time) : 0.0f;
        float shake_x = std::sin(current_time * 83.0f) * shake_strength;
        float shake_y = std::sin(current_time * 121.0f + 1.7f) * shake_strength;
        glm::vec3 render_camera_pos = scene_camera.position() + scene_camera.right() * shake_x + scene_camera.up() * shake_y;
        glm::vec3 render_camera_forward = glm::normalize(scene_camera.forward() + scene_camera.right() * shake_x * 0.35f + scene_camera.up() * shake_y * 0.35f);

        // query the real framebuffer size so the shader's aspect ratio stays correct after a resize
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);

        glClearColor(0.002f, 0.004f, 0.012f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        sky_shader.use();
        sky_shader.set_float("u_fov_y", glm::radians(camera_fov_degrees));
        sky_shader.set_vec2("u_resolution", static_cast<float>(framebuffer_width), static_cast<float>(framebuffer_height));
        sky_shader.set_vec3("u_camera_forward", render_camera_forward);
        sky_shader.set_vec3("u_camera_right", scene_camera.right());
        sky_shader.set_vec3("u_camera_up", scene_camera.up());

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glEnable(GL_DEPTH_TEST);

        grid_shader.use();
        glm::mat4 view = glm::lookAt(render_camera_pos, render_camera_pos + render_camera_forward, scene_camera.up());
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
        fullscreen_shader.set_vec3("u_camera_pos", render_camera_pos);
        fullscreen_shader.set_vec3("u_camera_forward", render_camera_forward);
        fullscreen_shader.set_vec3("u_camera_right", scene_camera.right());
        fullscreen_shader.set_vec3("u_camera_up", scene_camera.up());
        fullscreen_shader.set_float("u_noise_speed", gas_noise_speed);
        fullscreen_shader.set_float("u_noise_scale", gas_noise_scale);
        fullscreen_shader.set_float("u_emission_strength", gas_emission_strength);
        fullscreen_shader.set_float("u_absorption_strength", gas_absorption_strength);
        fullscreen_shader.set_float("u_edge_raggedness", gas_edge_raggedness);
        fullscreen_shader.set_float("u_halo_strength", gas_halo_strength);
        fullscreen_shader.set_float("u_event_active", supernova_event_active ? 1.0f : 0.0f);
        fullscreen_shader.set_float("u_event_time", event_time);

        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glDisable(GL_BLEND);

        auto draw_hud_rect = [&](float left, float top, float width, float height, glm::vec4 color) {
            float right = left + width;
            float bottom = top - height;
            float rect_vertices[hud_rect_float_count] = {
                left, bottom,
                right, bottom,
                right, top,
                right, top,
                left, top,
                left, bottom
            };

            hud_shader.set_vec4("u_color", color);
            glBindVertexArray(hud_vao);
            glBindBuffer(GL_ARRAY_BUFFER, hud_vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(rect_vertices), rect_vertices);
            glDrawArrays(GL_TRIANGLES, 0, hud_rect_vertex_count);
        };

        auto draw_hud_text = [&](std::string_view text, float left, float top, float pixel_size, glm::vec4 color) {
            float cursor_x = left;

            for (char c : text) {
                if (c == ' ') {
                    cursor_x += pixel_size * 4.0f;
                    continue;
                }

                glyph_rows glyph = glyph_for(c);
                for (int y = 0; y < static_cast<int>(glyph.size()); ++y) {
                    for (int x = 0; x < static_cast<int>(glyph[y].size()); ++x) {
                        if (glyph[y][x] == '1') {
                            draw_hud_rect(
                                cursor_x + static_cast<float>(x) * pixel_size,
                                top - static_cast<float>(y) * pixel_size,
                                pixel_size * 0.82f,
                                pixel_size * 0.82f,
                                color);
                        }
                    }
                }

                cursor_x += pixel_size * 6.0f;
            }
        };

        auto draw_control_bar = [&](int row, std::string_view label, float fraction, glm::vec3 color) {
            constexpr float label_left = -0.94f;
            constexpr float bar_left = -0.56f;
            constexpr float top = 0.91f;
            constexpr float width = 0.42f;
            constexpr float height = 0.035f;
            constexpr float gap = 0.025f;

            float row_top = top - static_cast<float>(row) * (height + gap);
            draw_hud_text(label, label_left, row_top - 0.003f, 0.0045f, glm::vec4(0.88f, 0.90f, 0.92f, 0.92f));
            draw_hud_rect(bar_left, row_top, width, height, glm::vec4(0.02f, 0.02f, 0.025f, 0.68f));
            draw_hud_rect(bar_left, row_top, width * fraction, height, glm::vec4(color, 0.88f));
        };

        hud_shader.use();
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        draw_control_bar(0, "Z/X EMISSION", control_fraction(gas_emission_strength, 0.0f, 15.0f), glm::vec3(1.0f, 0.42f, 0.08f));
        draw_control_bar(1, "C/V ABSORB", control_fraction(gas_absorption_strength, 0.0f, 3.0f), glm::vec3(0.35f, 0.58f, 1.0f));
        draw_control_bar(2, "B/N EDGE", control_fraction(gas_edge_raggedness, 0.05f, 0.9f), glm::vec3(1.0f, 0.18f, 0.32f));
        draw_control_bar(3, "K/L SPEED", control_fraction(gas_noise_speed, 0.0f, 3.0f), glm::vec3(0.42f, 1.0f, 0.42f));
        draw_control_bar(4, "O/P SCALE", control_fraction(gas_noise_scale, 0.2f, 3.0f), glm::vec3(0.24f, 0.95f, 1.0f));
        draw_control_bar(5, "H/J HALO", control_fraction(gas_halo_strength, 0.0f, 2.0f), glm::vec3(1.0f, 0.78f, 0.25f));

        glDisable(GL_BLEND);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteVertexArrays(1, &grid_vao);
    glDeleteVertexArrays(1, &hud_vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &grid_vbo);
    glDeleteBuffers(1, &hud_vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(sky_shader.id);
    glDeleteProgram(fullscreen_shader.id);
    glDeleteProgram(grid_shader.id);
    glDeleteProgram(hud_shader.id);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

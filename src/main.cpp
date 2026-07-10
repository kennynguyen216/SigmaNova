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
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr int window_width = 800;
constexpr int window_height = 600;
constexpr float camera_fov_degrees = 55.0f;
constexpr float event_swell_duration = 2.0f;
constexpr float event_collapse_duration = 0.48f;
constexpr float event_singularity_hold_duration = 0.4f;
constexpr float event_flash_start = event_swell_duration + event_collapse_duration + event_singularity_hold_duration;
// When the world-space glare front reaches the camera. Must match GLARE_HIT in
// fullscreen.frag (FLASH_START + 0.42): the shake is the light hitting us.
constexpr float event_glare_hit = event_flash_start + 0.42f;
constexpr float event_recede_start = event_glare_hit + 0.28f;
constexpr float event_recede_end = event_recede_start + 2.2f; // keep in sync with RECEDE_END in fullscreen.frag
constexpr bool show_fabric_grid = false;
constexpr float bloom_resolution_scale = 0.5f;
constexpr float minimum_volume_scale = 0.5f;
constexpr float gpu_frame_target_ms = 1000.0f / 60.0f;
constexpr int gpu_query_ring_size = 4;
float gas_noise_speed = 1.0f;
float gas_noise_scale = 1.0f;
float gas_emission_strength = 6.0f;
float gas_absorption_strength = 1.15f;
float gas_edge_raggedness = 0.45f;
float gas_halo_strength = 0.5f;
bool supernova_event_active = false;
bool space_was_pressed = false;
float supernova_event_start_time = 0.0f;
constexpr int hud_vertex_float_count = 6; // position.xy + color.rgba

enum class gpu_pass : std::size_t {
    scene,
    volume,
    bloom,
    composite,
    hud,
    total,
    count
};

constexpr std::size_t gpu_pass_count = static_cast<std::size_t>(gpu_pass::count);

struct runtime_options {
    bool benchmark = false;
    bool vsync = true;
    bool dynamic_resolution = true;
    float volume_scale = 0.75f;
};

struct gpu_profiler {
    std::array<std::array<std::array<unsigned int, 2>, gpu_pass_count>, gpu_query_ring_size> queries {};
    std::array<bool, gpu_query_ring_size> written {};
    std::array<double, gpu_pass_count> latest_ms {};
    std::array<float, gpu_query_ring_size> recorded_cpu_ms {};
    std::array<float, gpu_query_ring_size> recorded_event_time {};
    float sample_cpu_ms = 0.0f;
    float sample_event_time = -1.0f;
    int write_slot = 0;
    bool has_sample = false;
    bool recording = true;

    void initialize()
    {
        glGenQueries(static_cast<GLsizei>(gpu_query_ring_size * gpu_pass_count * 2), &queries[0][0][0]);
    }

    void begin_frame()
    {
        has_sample = false;
        recording = true;
        if (!written[write_slot]) {
            return;
        }

        int available = GL_FALSE;
        glGetQueryObjectiv(queries[write_slot][static_cast<std::size_t>(gpu_pass::total)][1], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available != GL_TRUE) {
            recording = false;
            return;
        }

        for (std::size_t pass = 0; pass < gpu_pass_count; ++pass) {
            GLuint64 start = 0;
            GLuint64 end = 0;
            glGetQueryObjectui64v(queries[write_slot][pass][0], GL_QUERY_RESULT, &start);
            glGetQueryObjectui64v(queries[write_slot][pass][1], GL_QUERY_RESULT, &end);
            latest_ms[pass] = static_cast<double>(end - start) / 1'000'000.0;
        }
        sample_cpu_ms = recorded_cpu_ms[write_slot];
        sample_event_time = recorded_event_time[write_slot];
        has_sample = true;
    }

    void begin(gpu_pass pass)
    {
        if (recording) {
            glQueryCounter(queries[write_slot][static_cast<std::size_t>(pass)][0], GL_TIMESTAMP);
        }
    }

    void end(gpu_pass pass)
    {
        if (recording) {
            glQueryCounter(queries[write_slot][static_cast<std::size_t>(pass)][1], GL_TIMESTAMP);
        }
    }

    void end_frame(float cpu_ms, float event_time)
    {
        if (!recording) {
            return;
        }
        recorded_cpu_ms[write_slot] = cpu_ms;
        recorded_event_time[write_slot] = event_time;
        written[write_slot] = true;
        write_slot = (write_slot + 1) % gpu_query_ring_size;
    }

    double milliseconds(gpu_pass pass) const
    {
        return latest_ms[static_cast<std::size_t>(pass)];
    }

    void shutdown()
    {
        glDeleteQueries(static_cast<GLsizei>(gpu_query_ring_size * gpu_pass_count * 2), &queries[0][0][0]);
    }
};

struct benchmark_bucket {
    const char* name;
    std::array<double, gpu_pass_count> gpu_ms_sum {};
    double cpu_ms_sum = 0.0;
    int samples = 0;

    void add(const gpu_profiler& profiler)
    {
        for (std::size_t pass = 0; pass < gpu_pass_count; ++pass) {
            gpu_ms_sum[pass] += profiler.latest_ms[pass];
        }
        cpu_ms_sum += profiler.sample_cpu_ms;
        ++samples;
    }
};

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

float smoothstep(float edge0, float edge1, float value)
{
    float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float event_shake_strength(float event_time)
{
    // The shake is the blast front striking the camera, so it is keyed to the
    // impact moment, not the detonation: a hard snap on arrival, then a decay.
    float hit_age = event_time - event_glare_hit;
    if (hit_age < 0.0f || hit_age > 1.15f) {
        return 0.0f;
    }

    float snap_on = std::clamp(hit_age / 0.06f, 0.0f, 1.0f);
    float decay = std::exp(-hit_age * 3.2f);
    return 0.055f * snap_on * decay;
}

float event_camera_pull_distance(float event_time)
{
    // Let the blast fill the screen first, then slowly dolly backward as the
    // remnant expands so its scale stays readable without fighting user input.
    float reveal = smoothstep(event_flash_start + 0.25f, event_flash_start + 6.5f, event_time);
    return 2.25f * reveal;
}

float event_speed_blur_strength(float event_time)
{
    float blast_on = smoothstep(event_flash_start - 0.02f, event_flash_start + 0.12f, event_time);
    float blast_off = 1.0f - smoothstep(event_flash_start + 0.22f, event_flash_start + 0.48f, event_time);
    return 0.65f * blast_on * blast_off;
}

float event_bloom_strength(float event_time)
{
    // Bloom is beautiful on the blast and remnant, but during the whiteout
    // recovery it reads as leftover blur. Suppress it only while the screen is
    // fading from white, then ease it back once the remnant has taken over.
    float suppress_on = smoothstep(event_glare_hit - 0.08f, event_glare_hit + 0.04f, event_time);
    float suppress_off = smoothstep(event_recede_end + 0.08f, event_recede_end + 0.55f, event_time);
    float suppression = suppress_on * (1.0f - suppress_off);
    return 0.85f * (1.0f - suppression);
}

float event_whiteout_strength(float event_time)
{
    // Grow into a full-screen white flash, hold briefly, then fade the screen
    // overlay itself. This is the flashbang recovery: not a longer white hold.
    float engulf = smoothstep(event_flash_start + 0.12f, event_glare_hit, event_time);
    float recover = 1.0f - smoothstep(event_recede_start, event_recede_end, event_time);
    return engulf * recover;
}

float event_flash_hud_visibility(float event_time)
{
    float flash_age = event_time - event_flash_start;
    if (flash_age < -0.05f || flash_age > 3.2f) {
        return 1.0f;
    }

    float fade_out = 1.0f - std::clamp((flash_age + 0.05f) / 0.12f, 0.0f, 1.0f);
    float fade_in = std::clamp((flash_age - 2.4f) / 0.8f, 0.0f, 1.0f);
    return std::max(fade_out, fade_in);
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

void resize_color_texture(unsigned int texture, int width, int height)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void check_framebuffer(const char* name)
{
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "Framebuffer incomplete: " << name << std::endl;
    }
}

runtime_options parse_runtime_options(int argc, char** argv)
{
    runtime_options options;
    for (int i = 1; i < argc; ++i) {
        std::string_view argument(argv[i]);
        if (argument == "--benchmark" || argument == "--no-vsync") {
            options.vsync = false;
        }
        if (argument == "--benchmark") {
            options.benchmark = true;
            options.dynamic_resolution = false;
        } else if (argument == "--full-resolution") {
            options.volume_scale = 1.0f;
            options.dynamic_resolution = false;
        } else if (argument == "--fixed-resolution") {
            options.dynamic_resolution = false;
        }
    }
    return options;
}

void append_hud_vertex(std::vector<float>& vertices, float x, float y, const glm::vec4& color)
{
    vertices.insert(vertices.end(), { x, y, color.r, color.g, color.b, color.a });
}

void append_hud_rect(std::vector<float>& vertices, float left, float top, float width, float height, const glm::vec4& color)
{
    float right = left + width;
    float bottom = top - height;
    append_hud_vertex(vertices, left, bottom, color);
    append_hud_vertex(vertices, right, bottom, color);
    append_hud_vertex(vertices, right, top, color);
    append_hud_vertex(vertices, right, top, color);
    append_hud_vertex(vertices, left, top, color);
    append_hud_vertex(vertices, left, bottom, color);
}
}

int main(int argc, char** argv)
{
    runtime_options options = parse_runtime_options(argc, argv);
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
    // Presentation mode uses VSync for pacing. --benchmark/--no-vsync uncaps
    // presentation; GPU timestamp queries remain the authoritative GPU metric.
    glfwSwapInterval(options.vsync ? 1 : 0);

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
    shader bright_shader("shaders/fullscreen.vert", "shaders/bright_extract.frag");
    shader blur_shader("shaders/fullscreen.vert", "shaders/blur.frag");
    shader bloom_composite_shader("shaders/fullscreen.vert", "shaders/bloom_composite.frag");
    shader volume_composite_shader("shaders/fullscreen.vert", "shaders/volume_composite.frag");

    bright_shader.use();
    bright_shader.set_int("u_scene_texture", 0);
    blur_shader.use();
    blur_shader.set_int("u_image", 0);
    bloom_composite_shader.use();
    bloom_composite_shader.set_int("u_scene_texture", 0);
    bloom_composite_shader.set_int("u_bloom_texture", 1);
    volume_composite_shader.use();
    volume_composite_shader.set_int("u_volume_texture", 0);

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
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, hud_vertex_float_count * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, hud_vertex_float_count * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    unsigned int scene_fbo;
    unsigned int scene_texture;
    unsigned int scene_depth_rbo;
    unsigned int volume_fbo;
    unsigned int volume_texture;
    unsigned int bright_fbo;
    unsigned int bright_texture;
    unsigned int blur_fbo[2];
    unsigned int blur_texture[2];

    glGenFramebuffers(1, &scene_fbo);
    glGenTextures(1, &scene_texture);
    glGenRenderbuffers(1, &scene_depth_rbo);
    glGenFramebuffers(1, &volume_fbo);
    glGenTextures(1, &volume_texture);
    glGenFramebuffers(1, &bright_fbo);
    glGenTextures(1, &bright_texture);
    glGenFramebuffers(2, blur_fbo);
    glGenTextures(2, blur_texture);

    int render_target_width = 0;
    int render_target_height = 0;
    int bloom_target_width = 0;
    int bloom_target_height = 0;
    int volume_target_width = 0;
    int volume_target_height = 0;
    float volume_resolution_scale = options.volume_scale;

    auto resize_render_targets = [&](int width, int height) {
        if (width <= 0 || height <= 0) {
            return;
        }

        if (width != render_target_width || height != render_target_height) {
            render_target_width = width;
            render_target_height = height;
            resize_color_texture(scene_texture, width, height);
            glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scene_texture, 0);
            glBindRenderbuffer(GL_RENDERBUFFER, scene_depth_rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, scene_depth_rbo);
            check_framebuffer("scene");
        }

        int desired_bloom_width = std::max(1, static_cast<int>(std::lround(static_cast<float>(width) * bloom_resolution_scale)));
        int desired_bloom_height = std::max(1, static_cast<int>(std::lround(static_cast<float>(height) * bloom_resolution_scale)));
        if (desired_bloom_width != bloom_target_width || desired_bloom_height != bloom_target_height) {
            bloom_target_width = desired_bloom_width;
            bloom_target_height = desired_bloom_height;
            resize_color_texture(bright_texture, bloom_target_width, bloom_target_height);
            glBindFramebuffer(GL_FRAMEBUFFER, bright_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bright_texture, 0);
            check_framebuffer("bright extract");

            for (int i = 0; i < 2; ++i) {
                resize_color_texture(blur_texture[i], bloom_target_width, bloom_target_height);
                glBindFramebuffer(GL_FRAMEBUFFER, blur_fbo[i]);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blur_texture[i], 0);
                check_framebuffer("blur");
            }
        }

        int desired_volume_width = std::max(1, static_cast<int>(std::lround(static_cast<float>(width) * volume_resolution_scale)));
        int desired_volume_height = std::max(1, static_cast<int>(std::lround(static_cast<float>(height) * volume_resolution_scale)));
        if (desired_volume_width != volume_target_width || desired_volume_height != volume_target_height) {
            volume_target_width = desired_volume_width;
            volume_target_height = desired_volume_height;
            resize_color_texture(volume_texture, volume_target_width, volume_target_height);
            glBindFramebuffer(GL_FRAMEBUFFER, volume_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, volume_texture, 0);
            check_framebuffer("volume");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    };

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    camera scene_camera;
    active_camera = &scene_camera;
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    float last_frame_time = static_cast<float>(glfwGetTime());
    float perf_accum_time = 0.0f;
    int perf_frame_count = 0;
    float gpu_frame_ema_ms = 0.0f;
    int dynamic_resolution_cooldown = 0;
    gpu_profiler profiler;
    profiler.initialize();
    std::vector<float> hud_vertices;
    hud_vertices.reserve(16'384);
    float benchmark_start_time = last_frame_time;
    std::array<benchmark_bucket, 4> benchmark_buckets {{
        { "idle_star" },
        { "pre_flash" },
        { "flash_and_reveal" },
        { "settled_remnant" }
    }};

    while (!glfwWindowShouldClose(window)) {
        // time between frames, so movement speeds stay the same at any frame rate
        float current_time = static_cast<float>(glfwGetTime());
        float delta_time = current_time - last_frame_time;
        last_frame_time = current_time;

        profiler.begin_frame();
        if (profiler.has_sample) {
            float gpu_ms = static_cast<float>(profiler.milliseconds(gpu_pass::total));
            gpu_frame_ema_ms = gpu_frame_ema_ms <= 0.0f ? gpu_ms : gpu_frame_ema_ms * 0.9f + gpu_ms * 0.1f;

            if (options.dynamic_resolution && dynamic_resolution_cooldown <= 0) {
                float previous_scale = volume_resolution_scale;
                if (gpu_frame_ema_ms > gpu_frame_target_ms * 1.08f) {
                    volume_resolution_scale -= 0.0625f;
                } else if (gpu_frame_ema_ms < gpu_frame_target_ms * 0.78f) {
                    volume_resolution_scale += 0.0625f;
                }
                volume_resolution_scale = std::clamp(volume_resolution_scale, minimum_volume_scale, 1.0f);
                if (volume_resolution_scale != previous_scale) {
                    dynamic_resolution_cooldown = 30;
                }
            }

            if (options.benchmark) {
                std::size_t bucket = 0;
                if (profiler.sample_event_time >= event_recede_end) {
                    bucket = 3;
                } else if (profiler.sample_event_time >= event_flash_start) {
                    bucket = 2;
                } else if (profiler.sample_event_time >= 0.0f) {
                    bucket = 1;
                }
                benchmark_buckets[bucket].add(profiler);
            }
        }
        dynamic_resolution_cooldown = std::max(dynamic_resolution_cooldown - 1, 0);

        // average frame time in the title bar, refreshed twice a second
        perf_accum_time += delta_time;
        perf_frame_count += 1;
        if (perf_accum_time >= 0.5f) {
            float average_ms = perf_accum_time * 1000.0f / static_cast<float>(perf_frame_count);
            char title[320];
            std::snprintf(title, sizeof(title),
                "SigmaNova | CPU %.2f ms %.0f FPS | GPU %.2f ms (vol %.2f bloom %.2f) | scale %.2f%s | Event %.1f | %s",
                average_ms,
                static_cast<float>(perf_frame_count) / perf_accum_time,
                gpu_frame_ema_ms,
                profiler.milliseconds(gpu_pass::volume),
                profiler.milliseconds(gpu_pass::bloom),
                volume_resolution_scale,
                options.dynamic_resolution ? " dynamic" : " fixed",
                supernova_event_active ? current_time - supernova_event_start_time : 0.0f,
                options.vsync ? "VSync" : "uncapped");
            glfwSetWindowTitle(window, title);
            perf_accum_time = 0.0f;
            perf_frame_count = 0;
        }

        process_input(window, delta_time, current_time);
        scene_camera.process_input(window, delta_time);

        if (options.benchmark && !supernova_event_active && current_time - benchmark_start_time >= 3.0f) {
            supernova_event_active = true;
            supernova_event_start_time = current_time;
        }

        float event_time = supernova_event_active ? current_time - supernova_event_start_time : 0.0f;
        if (options.benchmark && supernova_event_active && event_time >= 10.0f) {
            glfwSetWindowShouldClose(window, true);
        }
        float shake_strength = supernova_event_active ? event_shake_strength(event_time) : 0.0f;
        float camera_pull = supernova_event_active ? event_camera_pull_distance(event_time) : 0.0f;
        float hud_visibility = supernova_event_active ? 0.0f : 1.0f;
        float shake_x = std::sin(current_time * 83.0f) * shake_strength;
        float shake_y = std::sin(current_time * 121.0f + 1.7f) * shake_strength;
        glm::vec3 render_camera_pos = scene_camera.position() - scene_camera.forward() * camera_pull + scene_camera.right() * shake_x + scene_camera.up() * shake_y;
        glm::vec3 render_camera_forward = glm::normalize(scene_camera.forward() + scene_camera.right() * shake_x * 0.35f + scene_camera.up() * shake_y * 0.35f);

        // query the real framebuffer size so the shader's aspect ratio stays correct after a resize
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        resize_render_targets(framebuffer_width, framebuffer_height);

        profiler.begin(gpu_pass::total);
        profiler.begin(gpu_pass::scene);

        glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
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

        glm::mat4 view = glm::lookAt(render_camera_pos, render_camera_pos + render_camera_forward, scene_camera.up());
        glm::mat4 projection = glm::perspective(glm::radians(camera_fov_degrees), static_cast<float>(framebuffer_width) / static_cast<float>(framebuffer_height), 0.1f, 100.0f);

        if (show_fabric_grid) {
            glEnable(GL_DEPTH_TEST);

            grid_shader.use();
            grid_shader.set_mat4("u_view", view);
            grid_shader.set_mat4("u_projection", projection);
            grid_shader.set_vec3("u_color", glm::vec3(0.82f));

            glBindVertexArray(grid_vao);
            glDrawArrays(GL_LINES, 0, static_cast<int>(grid_vertices.size() / 3));
        }

        profiler.end(gpu_pass::scene);
        profiler.begin(gpu_pass::volume);

        glBindFramebuffer(GL_FRAMEBUFFER, volume_fbo);
        glViewport(0, 0, volume_target_width, volume_target_height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        fullscreen_shader.use();
        fullscreen_shader.set_float("u_time", current_time);
        fullscreen_shader.set_float("u_fov_y", glm::radians(camera_fov_degrees));
        fullscreen_shader.set_vec2("u_resolution", static_cast<float>(volume_target_width), static_cast<float>(volume_target_height));
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

        glBindFramebuffer(GL_FRAMEBUFFER, scene_fbo);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        volume_composite_shader.use();
        volume_composite_shader.set_vec2("u_output_resolution", static_cast<float>(framebuffer_width), static_cast<float>(framebuffer_height));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, volume_texture);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glDisable(GL_BLEND);
        profiler.end(gpu_pass::volume);
        profiler.begin(gpu_pass::bloom);

        glBindFramebuffer(GL_FRAMEBUFFER, bright_fbo);
        glViewport(0, 0, bloom_target_width, bloom_target_height);
        glClear(GL_COLOR_BUFFER_BIT);
        bright_shader.use();
        bright_shader.set_vec2("u_resolution", static_cast<float>(bloom_target_width), static_cast<float>(bloom_target_height));
        bright_shader.set_float("u_threshold", 0.48f);
        bright_shader.set_float("u_soft_knee", 0.22f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, scene_texture);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        blur_shader.use();
        blur_shader.set_vec2("u_texel_size", 1.0f / static_cast<float>(bloom_target_width), 1.0f / static_cast<float>(bloom_target_height));
        bool horizontal_blur = true;
        bool first_blur_pass = true;
        int last_blur_target = 0;
        constexpr int blur_pass_count = 6;

        for (int pass = 0; pass < blur_pass_count; ++pass) {
            int target = horizontal_blur ? 0 : 1;
            glBindFramebuffer(GL_FRAMEBUFFER, blur_fbo[target]);
            glClear(GL_COLOR_BUFFER_BIT);
            blur_shader.set_int("u_horizontal", horizontal_blur ? 1 : 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, first_blur_pass ? bright_texture : blur_texture[1 - target]);
            glBindVertexArray(vao);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

            last_blur_target = target;
            horizontal_blur = !horizontal_blur;
            first_blur_pass = false;
        }

        profiler.end(gpu_pass::bloom);
        profiler.begin(gpu_pass::composite);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        bloom_composite_shader.use();
        bloom_composite_shader.set_vec2("u_resolution", static_cast<float>(framebuffer_width), static_cast<float>(framebuffer_height));
        bloom_composite_shader.set_float("u_bloom_strength", supernova_event_active ? event_bloom_strength(event_time) : 0.85f);
        bloom_composite_shader.set_float("u_speed_blur_strength", supernova_event_active ? event_speed_blur_strength(event_time) : 0.0f);
        bloom_composite_shader.set_float("u_whiteout_strength", supernova_event_active ? event_whiteout_strength(event_time) : 0.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, scene_texture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, blur_texture[last_blur_target]);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glActiveTexture(GL_TEXTURE0);
        profiler.end(gpu_pass::composite);
        profiler.begin(gpu_pass::hud);

        hud_vertices.clear();
        auto draw_hud_rect = [&](float left, float top, float width, float height, glm::vec4 color) {
            append_hud_rect(hud_vertices, left, top, width, height, color);
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

        if (hud_visibility > 0.01f) {
            draw_control_bar(0, "Z/X EMISSION", control_fraction(gas_emission_strength, 0.0f, 15.0f), glm::vec3(1.0f, 0.42f, 0.08f) * hud_visibility);
            draw_control_bar(1, "C/V ABSORB", control_fraction(gas_absorption_strength, 0.0f, 3.0f), glm::vec3(0.35f, 0.58f, 1.0f) * hud_visibility);
            draw_control_bar(2, "B/N EDGE", control_fraction(gas_edge_raggedness, 0.05f, 0.9f), glm::vec3(1.0f, 0.18f, 0.32f) * hud_visibility);
            draw_control_bar(3, "K/L SPEED", control_fraction(gas_noise_speed, 0.0f, 3.0f), glm::vec3(0.42f, 1.0f, 0.42f) * hud_visibility);
            draw_control_bar(4, "O/P SCALE", control_fraction(gas_noise_scale, 0.2f, 3.0f), glm::vec3(0.24f, 0.95f, 1.0f) * hud_visibility);
            draw_control_bar(5, "H/J HALO", control_fraction(gas_halo_strength, 0.0f, 2.0f), glm::vec3(1.0f, 0.78f, 0.25f) * hud_visibility);
        }

        if (!hud_vertices.empty()) {
            glBindVertexArray(hud_vao);
            glBindBuffer(GL_ARRAY_BUFFER, hud_vbo);
            glBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(hud_vertices.size() * sizeof(float)),
                hud_vertices.data(),
                GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(hud_vertices.size() / hud_vertex_float_count));
        }

        glDisable(GL_BLEND);
        profiler.end(gpu_pass::hud);
        profiler.end(gpu_pass::total);
        profiler.end_frame(
            delta_time * 1000.0f,
            supernova_event_active ? event_time : -1.0f);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    if (options.benchmark) {
        std::ofstream benchmark_file("reports/benchmark-latest.csv");
        benchmark_file << "phase,samples,cpu_ms,gpu_total_ms,gpu_scene_ms,gpu_volume_ms,gpu_bloom_ms,gpu_composite_ms,gpu_hud_ms,volume_scale,bloom_scale\n";
        for (const benchmark_bucket& bucket : benchmark_buckets) {
            if (bucket.samples == 0) {
                continue;
            }
            double divisor = static_cast<double>(bucket.samples);
            benchmark_file
                << bucket.name << ','
                << bucket.samples << ','
                << bucket.cpu_ms_sum / divisor << ','
                << bucket.gpu_ms_sum[static_cast<std::size_t>(gpu_pass::total)] / divisor << ','
                << bucket.gpu_ms_sum[static_cast<std::size_t>(gpu_pass::scene)] / divisor << ','
                << bucket.gpu_ms_sum[static_cast<std::size_t>(gpu_pass::volume)] / divisor << ','
                << bucket.gpu_ms_sum[static_cast<std::size_t>(gpu_pass::bloom)] / divisor << ','
                << bucket.gpu_ms_sum[static_cast<std::size_t>(gpu_pass::composite)] / divisor << ','
                << bucket.gpu_ms_sum[static_cast<std::size_t>(gpu_pass::hud)] / divisor << ','
                << volume_resolution_scale << ','
                << bloom_resolution_scale << '\n';
        }
        std::cout << "Benchmark metrics written to reports/benchmark-latest.csv\n";
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteVertexArrays(1, &grid_vao);
    glDeleteVertexArrays(1, &hud_vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &grid_vbo);
    glDeleteBuffers(1, &hud_vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteFramebuffers(1, &scene_fbo);
    glDeleteFramebuffers(1, &volume_fbo);
    glDeleteFramebuffers(1, &bright_fbo);
    glDeleteFramebuffers(2, blur_fbo);
    glDeleteTextures(1, &scene_texture);
    glDeleteTextures(1, &volume_texture);
    glDeleteTextures(1, &bright_texture);
    glDeleteTextures(2, blur_texture);
    glDeleteRenderbuffers(1, &scene_depth_rbo);
    glDeleteProgram(sky_shader.id);
    glDeleteProgram(fullscreen_shader.id);
    glDeleteProgram(grid_shader.id);
    glDeleteProgram(hud_shader.id);
    glDeleteProgram(bright_shader.id);
    glDeleteProgram(blur_shader.id);
    glDeleteProgram(bloom_composite_shader.id);
    glDeleteProgram(volume_composite_shader.id);
    profiler.shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

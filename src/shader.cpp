#include "shader.h"

#include <glad/glad.h>

#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <sstream>

shader::shader(const char* vertex_path, const char* fragment_path)
{
    std::string vertex_code = read_file(vertex_path);
    std::string fragment_code = read_file(fragment_path);

    const char* vertex_shader_source = vertex_code.c_str();
    const char* fragment_shader_source = fragment_code.c_str();

    unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_source, nullptr);
    glCompileShader(vertex_shader);
    check_compile_errors(vertex_shader, "VERTEX");

    unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
    glCompileShader(fragment_shader);
    check_compile_errors(fragment_shader, "FRAGMENT");

    id = glCreateProgram();
    glAttachShader(id, vertex_shader);
    glAttachShader(id, fragment_shader);
    glLinkProgram(id);
    check_compile_errors(id, "PROGRAM");

    // once linked into the program, the individual shader objects are no longer needed
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
}

void shader::use() const
{
    glUseProgram(id);
}

int shader::uniform_location(const std::string& name) const
{
    auto cached = uniform_location_cache_.find(name);
    if (cached != uniform_location_cache_.end()) {
        return cached->second;
    }

    int location = glGetUniformLocation(id, name.c_str());
    uniform_location_cache_.emplace(name, location);
    return location;
}

void shader::set_int(const std::string& name, int value) const
{
    glUniform1i(uniform_location(name), value);
}

void shader::set_float(const std::string& name, float value) const
{
    glUniform1f(uniform_location(name), value);
}

void shader::set_vec2(const std::string& name, float x, float y) const
{
    glUniform2f(uniform_location(name), x, y);
}

void shader::set_vec3(const std::string& name, glm::vec3 value) const
{
    glUniform3f(uniform_location(name), value.x, value.y, value.z);
}

void shader::set_vec4(const std::string& name, glm::vec4 value) const
{
    glUniform4f(uniform_location(name), value.x, value.y, value.z, value.w);
}

void shader::set_mat4(const std::string& name, const glm::mat4& value) const
{
    glUniformMatrix4fv(uniform_location(name), 1, GL_FALSE, glm::value_ptr(value));
}

std::string shader::read_file(const char* path)
{
    std::ifstream file(path);

    if (!file.is_open()) {
        std::cout << "Failed to open the file: " << path << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

void shader::check_compile_errors(unsigned int shader_id, const std::string& type)
{
    int success;
    char info_log[1024];

    if (type != "PROGRAM") {
        glGetShaderiv(shader_id, GL_COMPILE_STATUS, &success);

        if (!success) {
            glGetShaderInfoLog(shader_id, 1024, nullptr, info_log);
            std::cout << "ERROR::SHADER::" << type << "::COMPILATION_FAILED\n"
                      << info_log << std::endl;
        }
    } else {
        glGetProgramiv(shader_id, GL_LINK_STATUS, &success);

        if (!success) {
            glGetProgramInfoLog(shader_id, 1024, nullptr, info_log);
            std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                      << info_log << std::endl;
        }
    }
}

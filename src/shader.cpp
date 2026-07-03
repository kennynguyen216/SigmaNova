#include "shader.h"

#include <glad/glad.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <glm/glm.hpp>

void shader::use() const
{
    glUseProgram(id);
}

void shader::set_float(const std::string& name, float value) const
{
    glUniform1f(glGetUniformLocation(id, name.c_str()), value);
}

void shader::set_vec2(const std::string& name, float x, float y) const
{
    glUniform2f(glGetUniformLocation(id, name.c_str()), x, y);
}

void shader::set_vec3(const std::string& name, glm::vec3 value) const 
{
    glUniform3f(glGetUniformLocation(id, name.c_str()), value.x, value.y, value.z);
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

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
}

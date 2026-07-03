#pragma once

#include <glm/glm.hpp>
#include <string>

class shader {
public:
    unsigned int id;

    shader(const char* vertex_path, const char* fragment_path);

    void use() const;

    void set_float(const std::string& name, float value) const;
    void set_vec2(const std::string& name, float x, float y) const;
    void set_vec3(const std::string& name, glm::vec3 value) const;

private:
    static std::string read_file(const char* path);
    static void check_compile_errors(unsigned int shader_id, const std::string& type);
};

#pragma once

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

class shader {
public:
    unsigned int id;

    shader(const char* vertex_path, const char* fragment_path);

    void use() const;

    void set_float(const std::string& name, float value) const;
    void set_vec2(const std::string& name, float x, float y) const;
    void set_vec3(const std::string& name, glm::vec3 value) const;
    void set_vec4(const std::string& name, glm::vec4 value) const;
    void set_mat4(const std::string& name, const glm::mat4& value) const;

private:
    // glGetUniformLocation is a string lookup into the driver; uniform names
    // never change after link, so each is resolved once and memoized.
    int uniform_location(const std::string& name) const;

    mutable std::unordered_map<std::string, int> uniform_location_cache_;

    static std::string read_file(const char* path);
    static void check_compile_errors(unsigned int shader_id, const std::string& type);
};

#pragma once

#include <string>

class Shader
{
public:
    unsigned int ID;

    Shader(const char* vertexPath, const char* fragmentPath);

    void use() const;

    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, float x, float y) const;

private:
    static std::string readFile(const char* path);
    static void checkCompileErrors(unsigned int shader, const std::string& type);
};

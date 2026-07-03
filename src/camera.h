#pragma once

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

class camera {
    public:
    camera();
    void update();
    void process_input(GLFWwindow* window);
    glm::vec3 position() const; 
    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::vec3 up() const;
    

    private:
    float angle_;
    float distance_;

    glm::vec3 target_;
    glm::vec3 world_up_;

    glm::vec3 position_;
    glm::vec3 forward_;
    glm::vec3 right_;
    glm::vec3 up_;
};
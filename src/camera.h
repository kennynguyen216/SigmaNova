#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

class camera {
public:
    camera();
    void update();
    void process_input(GLFWwindow* window, float delta_time);
    void process_mouse(float x_offset, float y_offset);
    glm::vec3 position() const;
    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::vec3 up() const;

private:
    //float angle_ = 0.0f;
    //float distance_ = 3.0f;

    //glm::vec3 target_ { 0.0f, 0.0f, 0.0f };
    float yaw_;
    float pitch_;
    float movement_speed_;
    float mouse_sens_;
    glm::vec3 world_up_ { 0.0f, 1.0f, 0.0f };
    glm::vec3 position_ {};
    glm::vec3 forward_ {};
    glm::vec3 right_ {};
    glm::vec3 up_ {};
};

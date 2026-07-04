#include "camera.h"

#include <algorithm>
#include <cmath>

camera::camera()
{
    yaw_ = -90.0f;
    pitch_ = 0.0f;
    movement_speed_ = 3.0f;
    mouse_sens_ = 0.1f;
    position_ = glm::vec3(0.0f, 1.0f, 5.0f);
    update();
}

void camera::update()
{
    forward_.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    forward_.y = sin(glm::radians(pitch_));
    forward_.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    forward_ = glm::normalize(forward_);
    right_ = glm::normalize(glm::cross(forward_, world_up_));
    up_ = glm::normalize(glm::cross(right_, forward_));
}

glm::vec3 camera::position() const
{
    return position_;
}

glm::vec3 camera::forward() const
{
    return forward_;
}

glm::vec3 camera::right() const
{
    return right_;
}

glm::vec3 camera::up() const
{
    return up_;
}

void camera::process_input(GLFWwindow* window, float delta_time)
{
    float velocity = movement_speed_ * delta_time;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        position_ -= right_ * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        position_ += right_ * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        position_ += forward_ * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        position_ -= forward_ * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS){
        position_ += world_up_ * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL)== GLFW_PRESS) {
        position_ -= world_up_ * velocity;
    }

    float look_velocity = 90.0f * delta_time;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        yaw_ -= look_velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        yaw_ += look_velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        pitch_ += look_velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        pitch_ -= look_velocity;
    }
    pitch_ = std::clamp(pitch_, -89.0f, 89.0f);

    update();
}

void camera::process_mouse(float x_offset, float y_offset)
{
    x_offset *= mouse_sens_;
    y_offset *= mouse_sens_;

    yaw_ += x_offset;
    pitch_ += y_offset;

    pitch_ = std::clamp(pitch_, -89.0f, 89.0f);

    update();
}

#include "camera.h"

#include <algorithm>
#include <cmath>

camera::camera()
{
    update();
}

void camera::update()
{
    // orbit the target on a circle in the xz plane; distance_ is the radius
    position_.x = std::sin(angle_) * distance_;
    position_.y = 0.0f;
    position_.z = std::cos(angle_) * distance_;

    forward_ = glm::normalize(target_ - position_); // direction from camera to target
    right_ = glm::normalize(glm::cross(forward_, world_up_)); // cross of forward and world up is perpendicular to both, which is the camera's right
    up_ = glm::cross(right_, forward_); // true vertical for the camera, perpendicular to right and forward
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
    // speeds are per second; scaling by delta_time keeps movement frame-rate independent
    const float rotate_speed = 1.5f; // radians per second
    const float zoom_speed = 2.0f; // world units per second

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        angle_ -= rotate_speed * delta_time;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        angle_ += rotate_speed * delta_time;
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        distance_ -= zoom_speed * delta_time;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        distance_ += zoom_speed * delta_time;
    }
    distance_ = std::max(distance_, 1.25f); // so the camera can't enter the sphere

    update();
}

#include "camera.h"
#include <cmath>
#include <algorithm>

camera::camera()
    : angle_(0.0f),
      distance_(3.0f),
      target_(0.0f, 0.0f, 0.0f),
      world_up_(0.0f, 1.0f, 0.0f)
{
    update();
}
void camera::update(){
    position_.x = std::sin(angle_) * distance_; // gives x around circle
    position_.y = 0.0f;
    position_.z = std::cos(angle_) * distance_; //gives z pos
    // distance controls radius of the cirlce
    forward_ = glm::normalize(target_ - position_); //  direction from camera to targegt
    right_ = glm::normalize(glm::cross(forward_, world_up_)); // if forward is straight and up is up and a cross product gives the right because its perpendicular to both 
    up_ = glm::cross(right_, forward_); //vertical direction perpendiculat to the across and the forward
 }

glm::vec3 camera::position() const{
        return position_;
}
glm::vec3 camera::forward() const {
        return forward_;
}
glm::vec3 camera::right() const {
        return right_;
}
glm::vec3 camera::up() const {
        return up_;
}

void camera::process_input(GLFWwindow* window)
{
    float rotate_speed = 0.00002f;
    float zoom_speed = 0.05f;

    if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS){
        angle_ -= rotate_speed;
    }
    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS){
        angle_ += rotate_speed;
    }
    if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS){
        distance_ -= zoom_speed;
    }
    if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS){
        distance_ += zoom_speed;
    }
    distance_ = std::max(distance_, 1.25f); // so the camera cant enter the sphere

    update();

}

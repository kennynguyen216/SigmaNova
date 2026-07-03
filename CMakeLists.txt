cmake_minimum_required(VERSION 3.24)

project(SigmaNova
    VERSION 0.1.0
    DESCRIPTION "Real-time C++/OpenGL supernova visualization project"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(sigma_nova
    src/main.cpp
    src/shader.cpp
    src/camera.cpp
)

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(sigma_nova
    PRIVATE
        glfw
        glad::glad
        glm::glm
        OpenGL::GL
)

if(MSVC)
    target_compile_options(sigma_nova PRIVATE /W4 /permissive-)
else()
    target_compile_options(sigma_nova PRIVATE -Wall -Wextra -Wpedantic)
endif()

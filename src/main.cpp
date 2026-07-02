#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Shader.h"

#include <iostream>


void processInput(GLFWwindow* window) {
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
        glfwSetWindowShouldClose(window,true);
    }
}

void framebuffer_size_callback(GLFWwindow*, int width, int height){
        glViewport(0, 0, width, height);
    }

int main()
{   
// fullscreen quad now 
    float vertices[] = {
    -1.0f, -1.0f, 0.0f,  // bottom left
     1.0f, -1.0f, 0.0f,  // bottom right
     1.0f,  1.0f, 0.0f,  // top right
    -1.0f,  1.0f, 0.0f   // top left
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    // this is initializng
    if (!glfwInit()) {
    std::cout << "Failed to initialize GLFW\n";
    return -1;
}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//create a 800 x 600 window named SigmaNova
    GLFWwindow* window = glfwCreateWindow(800, 600, "SigmaNova", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cout << "Failed to create GLFW window" << std:: endl;
        glfwTerminate();
        return -1;
    }
//makes the window context active 
    glfwMakeContextCurrent(window);
//loads function pointers
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cout << "Failed to initialize GLAD" << std:: endl;
        return -1;
    };
// vertex buffer object stores vertex data
// vertex buffer array remmebers how to interpret vertex data
//elemental buffer remmebers how to use the vertices
    unsigned int VBO;
    unsigned int VAO;
    unsigned int EBO;


    glGenVertexArrays(1, &VAO);
    glGenBuffers(1,&VBO);
    glGenBuffers(1, &EBO);


    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
//position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    Shader fullscreenShader("shaders/fullscreen.vert", "shaders/fullscreen.frag");

// this tells opengl to make it wire frame
//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

//this tells opengl to turn it back
//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

// if the window size changes cal this functon so opengl updates its viewport
    glfwSetFramebufferSizeCallback(window,framebuffer_size_callback);

// this keeps the window alive so
    while(!glfwWindowShouldClose(window)) {
             // constantly updates and listens for key presses
        processInput(window);
       
        // im finna render 
        // sets the screen to be a dark blue color 
        glClearColor(0.02f, 0.04f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        fullscreenShader.use();

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES,6,GL_UNSIGNED_INT, 0);


        //shows newly drawn frame
        glfwSwapBuffers(window);
        //checks if inputs or other events happened
        glfwPollEvents();
        
    }
//all the cleanup

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(fullscreenShader.ID);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

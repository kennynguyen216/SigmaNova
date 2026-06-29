#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>


void processInput(GLFWwindow *window) {
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
        glfwSetWindowShouldClose(window,true);
    }
}

void framebuffer_size_callback(GLFWwindow*, int width, int height){
        glViewport(0, 0, width, height);
    };

int main()
{   
    // this is initializng and then telling what version i want 
    glfwInit();
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





        //shows newly drawn frame
        glfwSwapBuffers(window);
        //checks if inputs or other events happened
        glfwPollEvents();
        
    }

    glfwTerminate();
    return 0;
}
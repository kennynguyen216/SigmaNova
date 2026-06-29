#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream> 
#include <sstream>
#include <string>


void processInput(GLFWwindow* window) {
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
        glfwSetWindowShouldClose(window,true);
    }
}

void framebuffer_size_callback(GLFWwindow*, int width, int height){
        glViewport(0, 0, width, height);
    }

//this is opens the triangle vert and takes all the glsl code
// and returns it as a c++ string 
std::string readFile(const char* path) 
{
    std::ifstream file(path);
    std::stringstream buffer;

    buffer << file.rdbuf();

    return buffer.str();

}



int main()
{   
// setting vertices of triangle
    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f
    };

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
    unsigned int VBO;
    unsigned int VAO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1,&VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
//lets make a triangle and start coloring ts
    std::string vertexCode = readFile("shaders/triangle.vert");
    std::string fragmentCode = readFile("shaders/triangle.frag");
    const char* vertexShaderSource = vertexCode.c_str();
    const char* fragmentShaderSource = fragmentCode.c_str();
    //attaches shader text to object. not giving exact string lengths just until null teriminator
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(fragmentShader);
    glDeleteShader(vertexShader);



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

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES,0, 3);


        //shows newly drawn frame
        glfwSwapBuffers(window);
        //checks if inputs or other events happened
        glfwPollEvents();
        
    }
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
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

    if(!file.is_open()){
        std::cout << "Failed to open the file: " << path << std::endl;
        return "";
    }
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
         0.0f,  0.5f, 0.0f
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

    // did it compile
    int success; 
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        std::cout << "ERROR:::SHADER::VERTEX::COMPILATION_FAILED\n"
                  << infoLog << std::endl;

    }


    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, nullptr,infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
                  << infoLog << std::endl;

    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_ERROR\n"
                  << infoLog << std::endl;
    }
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
//all the cleanup

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
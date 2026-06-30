#version 330 core



// decalres final color output from fragment
out vec4 FragColor;

uniform vec4 ourColor;

//in vec4 vertexColor;


void main() {
// this makes the triangle orange
    //FragColor = vec4(1.0, 0.4, 0.2, 1.0);
    //FragColor = vertexColor;
    FragColor = ourColor;



}

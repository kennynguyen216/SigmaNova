#version 330 core



// decalres final color output from fragment
out vec4 FragColor;

in vec3 ourColor;


void main() {
// this makes the triangle orange
    //FragColor = vec4(1.0, 0.4, 0.2, 1.0);
    //FragColor = vertexColor;
    FragColor = vec4(ourColor,1.0);



}

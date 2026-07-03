#version 330 core



// decalres final color output from fragment
out vec4 FragColor;
uniform float uTime;



void main() {
// this makes the triangle orange
    //FragColor = vec4(1.0, 0.4, 0.2, 1.0);
    //FragColor = vertexColor;
    
    float pulse = sin(uTime) *0.5 +0.5;
    FragColor = vec4(pulse, 0.4, 0.9, 1.0);

}

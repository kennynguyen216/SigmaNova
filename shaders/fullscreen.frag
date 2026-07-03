#version 330 core



// decalres final color output from fragment
out vec4 FragColor;
uniform float uTime;
uniform vec2 uResolution;



void main() {
// this makes the triangle orange
    //FragColor = vec4(1.0, 0.4, 0.2, 1.0);
    //FragColor = vertexColor;
    
    float pulse = sin(uTime) *0.5 +0.5;
    vec2 uv = gl_FragCoord.xy / uResolution;
    FragColor = vec4(uv.x, uv.y, pulse, 1.0);

}

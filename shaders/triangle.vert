#version 330 core

// expect a 3d input at attrivute location 0
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;

void main() {

    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0); // this can also be just vec4(aPos, 1.0)
   // vertexColor = vec4(0.5, 0.0, 0.0, 1.0);
   ourColor = aColor;


}

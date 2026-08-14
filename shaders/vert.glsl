#version 330 core
layout (location=0) in vec3 vertexPos;
layout (location=1) in vec3 vertexCol;
layout (location=2) in vec2 texPos;
uniform vec2 cameraPos;
uniform float cameraScale;
uniform vec2 windowSize;
uniform vec2 screenshotSize;
uniform vec2 cursorPos;
out vec3 col;
out vec2 texCord;

void main() {
    vec2 offsetNDC = (cameraPos / windowSize) * 2.0f;
    gl_Position = vec4((vertexPos.xy - offsetNDC * vec2(1.0, -1.0)) * cameraScale , vertexPos.z, 1.0);
    texCord = texPos;
    col = vertexCol;
}

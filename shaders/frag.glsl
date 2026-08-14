#version 330 core
in vec3 col;
in vec2 texCord;
uniform sampler2D tex;
uniform vec2 textureSize;
uniform float currentZoom;
out vec4 fragCol;
void main() {
    vec4 texCol = texture(tex, texCord);
    if (currentZoom > 8.0) {
        vec2 pixCords = texCord * textureSize;
        vec2 gridDistance = min(fract(pixCords), 1.0 - fract(pixCords));
        float lineWidth = 1.0/currentZoom;
        if (gridDistance.x < lineWidth || gridDistance.y < lineWidth) {
            vec4 gridCol = vec4(1.0, 1.0, 1.0, 0.3);
            fragCol = mix(texture(tex, texCord), gridCol, gridCol.a);
            return;
        }
    }
    fragCol = texture(tex, texCord);
}

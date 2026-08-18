#version 330 core
in vec3 col;
in vec2 texCord;
uniform sampler2D tex;
uniform vec2 textureSize;
uniform float currentZoom;
out vec4 fragCol;
void main() {
    vec4 texCol = texture(tex, texCord);

    float minZoom = 13.0;
    float maxZoom = 19.0;
    float maxGridAlpha = 0.3;

    if (currentZoom > minZoom) {
        vec2 pixCords = texCord * textureSize;
        vec2 gridDistance = min(fract(pixCords), 1.0 - fract(pixCords));

        float zoomFactor = smoothstep(minZoom, maxZoom, currentZoom);

        float lineWidth = 1.0/currentZoom;
        if (gridDistance.x < lineWidth || gridDistance.y < lineWidth) {
            float alpha = maxGridAlpha * zoomFactor;

            vec4 gridCol = vec4(1.0, 1.0, 1.0, alpha);
            fragCol = mix(texture(tex, texCord), gridCol, gridCol.a);
            return;
        }
    }
    fragCol = texture(tex, texCord);
}

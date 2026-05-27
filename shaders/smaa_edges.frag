#version 330 core

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uTexture;
uniform vec2 uResolution;
uniform vec2 uInvResolution;

float Luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 texel = uInvResolution;
    float center = Luma(texture(uTexture, vUv).rgb);
    float left = Luma(texture(uTexture, vUv - vec2(texel.x, 0.0)).rgb);
    float top = Luma(texture(uTexture, vUv - vec2(0.0, texel.y)).rgb);
    float right = Luma(texture(uTexture, vUv + vec2(texel.x, 0.0)).rgb);
    float bottom = Luma(texture(uTexture, vUv + vec2(0.0, texel.y)).rgb);

    float threshold = 0.120;
    float vertical = max(abs(center - left), abs(center - right));
    float horizontal = max(abs(center - top), abs(center - bottom));
    vec2 edge = smoothstep(vec2(threshold), vec2(threshold * 2.5), vec2(vertical, horizontal));

    fragColor = vec4(edge, 0.0, max(edge.x, edge.y));
}

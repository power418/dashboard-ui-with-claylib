#version 330 core

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uEdges;
uniform vec2 uResolution;
uniform vec2 uInvResolution;

void main() {
    vec2 texel = uInvResolution;
    vec2 edge = texture(uEdges, vUv).rg;
    vec2 left = texture(uEdges, vUv - vec2(texel.x, 0.0)).rg;
    vec2 right = texture(uEdges, vUv + vec2(texel.x, 0.0)).rg;
    vec2 top = texture(uEdges, vUv - vec2(0.0, texel.y)).rg;
    vec2 bottom = texture(uEdges, vUv + vec2(0.0, texel.y)).rg;

    float verticalWeight = edge.x * max(max(left.x, right.x), 0.35);
    float horizontalWeight = edge.y * max(max(top.y, bottom.y), 0.35);

    vec4 weights = vec4(
        verticalWeight * left.x,
        verticalWeight * right.x,
        horizontalWeight * top.y,
        horizontalWeight * bottom.y);

    float normalization = max(max(weights.x + weights.y, weights.z + weights.w), 1.0);
    fragColor = weights / normalization;
}

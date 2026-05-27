#version 330 core

in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uTexture;
uniform sampler2D uBlend;
uniform vec2 uResolution;
uniform vec2 uInvResolution;

void main() {
    vec2 texel = uInvResolution;
    vec4 center = texture(uTexture, vUv);
    vec4 weights = texture(uBlend, vUv);

    vec4 left = texture(uTexture, vUv - vec2(texel.x, 0.0));
    vec4 right = texture(uTexture, vUv + vec2(texel.x, 0.0));
    vec4 top = texture(uTexture, vUv - vec2(0.0, texel.y));
    vec4 bottom = texture(uTexture, vUv + vec2(0.0, texel.y));

    vec4 mixedColor = center;
    mixedColor += (left - center) * weights.x * 0.24;
    mixedColor += (right - center) * weights.y * 0.24;
    mixedColor += (top - center) * weights.z * 0.24;
    mixedColor += (bottom - center) * weights.w * 0.24;

    fragColor = vec4(mixedColor.rgb, center.a);
}

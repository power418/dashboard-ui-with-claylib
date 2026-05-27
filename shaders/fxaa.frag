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

    vec3 rgbNW = texture(uTexture, vUv + vec2(-1.0, -1.0) * texel).rgb;
    vec3 rgbNE = texture(uTexture, vUv + vec2( 1.0, -1.0) * texel).rgb;
    vec3 rgbSW = texture(uTexture, vUv + vec2(-1.0,  1.0) * texel).rgb;
    vec3 rgbSE = texture(uTexture, vUv + vec2( 1.0,  1.0) * texel).rgb;
    vec4 centerSample = texture(uTexture, vUv);
    vec3 rgbM = centerSample.rgb;

    float lumaNW = Luma(rgbNW);
    float lumaNE = Luma(rgbNE);
    float lumaSW = Luma(rgbSW);
    float lumaSE = Luma(rgbSE);
    float lumaM = Luma(rgbM);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float range = lumaMax - lumaMin;

    if (range < max(0.0700, lumaMax * 0.200)) {
        fragColor = centerSample;
        return;
    }

    vec2 direction;
    direction.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    direction.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float directionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * 0.0312, 0.0078125);
    float inverseDirectionAdjustment = 1.0 / (min(abs(direction.x), abs(direction.y)) + directionReduce);
    direction = clamp(direction * inverseDirectionAdjustment, vec2(-4.0), vec2(4.0)) * texel;

    vec3 rgbA = 0.5 * (
        texture(uTexture, vUv + direction * (1.0 / 3.0 - 0.5)).rgb +
        texture(uTexture, vUv + direction * (2.0 / 3.0 - 0.5)).rgb);

    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(uTexture, vUv + direction * -0.5).rgb +
        texture(uTexture, vUv + direction *  0.5).rgb);

    float lumaB = Luma(rgbB);
    vec3 resolved = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;
    resolved = mix(rgbM, resolved, 0.62);
    fragColor = vec4(resolved, centerSample.a);
}

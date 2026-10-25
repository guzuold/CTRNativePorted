// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#version 450

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D texSampler;

layout (push_constant) uniform settings {
    float gamma;
    bool hdr;
    float peak;
    float exposure;
} pp;

const float cutoff = 0.0031308, a = 1.055, b = 0.055, d = 12.92;
vec3 gamma(vec3 rgb) {
    return mix(
        a * pow(rgb, vec3(1.0 / (2.4 + 1.0 - pp.gamma))) - b,
        d * rgb / pp.gamma,
        lessThan(rgb, vec3(cutoff))
    );
}

// SPDX-License-Identifier: GPL-2.0-only
// Scalar GLSL/C++ common source: compiled into the actual fragment shader and CPU controls.
float ctr_pq_decode(float value) {
 float p=pow(clamp(value,0.0,1.0),1.0/(2523.0/32.0));
 return 10000.0*pow(max(p-3424.0/4096.0,0.0)/max(2413.0/128.0-(2392.0/128.0)*p,0.000001),1.0/(2610.0/16384.0));
}
float ctr_pq_encode(float nits) {
 float y=pow(clamp(nits/10000.0,0.0,1.0),2610.0/16384.0);
 return pow((3424.0/4096.0+(2413.0/128.0)*y)/(1.0+(2392.0/128.0)*y),2523.0/32.0);
}
float ctr_calibrate_hdr(float pq,float peak,float exposure) {
 if(peak==0.0 && exposure==1.0)return pq;
 float nits=ctr_pq_decode(pq)*exposure;
 if(peak>0.0)nits=min(nits,peak);
 return ctr_pq_encode(nits);
}

void main() {
    vec4 color_linear = texture(texSampler, uv);
    if (pp.hdr) {
        color = vec4(ctr_calibrate_hdr(color_linear.r, pp.peak, pp.exposure), ctr_calibrate_hdr(color_linear.g, pp.peak, pp.exposure), ctr_calibrate_hdr(color_linear.b, pp.peak, pp.exposure), color_linear.a);
    } else {
        color = vec4(gamma(color_linear.rgb), color_linear.a);
    }
}

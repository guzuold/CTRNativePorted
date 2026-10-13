// SPDX-License-Identifier: GPL-2.0-only
#version 450
layout(set=0,binding=0) uniform texture2D pixels;
layout(set=0,binding=1) uniform sampler filtering;
layout(location=0) out vec4 color;
void main(){color=texture(sampler2D(pixels,filtering),vec2(0.5));}

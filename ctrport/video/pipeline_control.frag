// SPDX-License-Identifier: GPL-2.0-only
#version 450
layout(location=0) in vec4 tint;
layout(location=0) out vec4 color;
layout(set=0,binding=0,std430) readonly buffer Multiplier {vec4 multiplier;};
void main(){color=tint*multiplier;}

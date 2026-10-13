// SPDX-License-Identifier: GPL-2.0-only
#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec2 color;
layout(location=0) out vec4 tint;
layout(set=0,binding=1,std430) readonly buffer Shift {vec4 shift;};
layout(set=0,binding=2,std430) readonly buffer Scale {vec4 scale;};
void main(){gl_Position=vec4(position,1)*scale+shift;tint=vec4(color,0,1);}

// SPDX-License-Identifier: GPL-2.0-only
#version 450
layout(location=0) out vec4 color0;
layout(location=1) out vec2 color1;
void main(){color0=vec4(64.0,128.0,192.0,255.0)/255.0;color1=vec2(128.0,64.0)/255.0;}

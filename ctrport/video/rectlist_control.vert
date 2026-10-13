// SPDX-License-Identifier: GPL-2.0-only
#version 450
void main(){
 const vec2 p[3]=vec2[3](vec2(-1,-1),vec2(1,-1),vec2(-1,1));
 gl_Position=vec4(p[gl_VertexIndex],0.5,1);
 gl_PointSize=1;
}

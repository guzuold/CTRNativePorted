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

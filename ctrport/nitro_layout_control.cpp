// SPDX-License-Identifier: GPL-2.0-only
#include "nitro_layout.hpp"
#include <cassert>
#include <cstdio>
int main() {
    // Patch 1.21 artwork is 1920x1080; embedded logo ends at approximately y=324.
    for(float scale:{.75f,1.f,1.25f,1.5f,1.75f,2.f}) {
        const auto c=nitro::crop(1920,1080,1280*scale,(nitro::height-nitro::header)*scale);
        assert(c.y>350 && c.y<440); // exclude logo, retain top of Crash's head
        assert(std::abs(c.y+c.h-1080)<.01f);
        assert(std::abs(c.x+c.w*.5f-960)<.01f);
        assert(c.x>=0 && c.w<=1920 && c.h>0);
        assert(std::abs(c.w/c.h-1280.f/(nitro::height-nitro::header))<.0001f);
    }
    assert(nitro::crop(0,1080,1280,590).w==0);
    assert(nitro::crop(1920,1080,0,590).h==0);
    assert(nitro::ease(-1)==0 && nitro::ease(0)==0 && nitro::ease(1)==1 && nitro::ease(2)==1);
    float previous=0;
    for(int i=0;i<=1000;++i) {float p=nitro::ease(float(i)/1000);assert(p>=previous&&p<=1);previous=p;}
    // Every destination and the full inherited functional page fit the new shell.
    for(int i=0;i<5;++i){assert(nitro::nav_left[i]+nitro::nav_width[i]<961);if(i)assert(nitro::nav_left[i]>=nitro::nav_left[i-1]+nitro::nav_width[i-1]);}
    assert(nitro::page_x(296)>=24 && nitro::page_x(990)<1280-24);
    assert(nitro::page_y(76)>nitro::header && nitro::page_y(604)<nitro::footer);
    puts("NITRO_LAYOUT_OK");
}

#include "retained_display_publication.hpp"
#include <iostream>
int main(){
 ctrport::RetainedDisplayPublication p;std::vector<unsigned char> b(64,37),out(3,9);auto sentinel=out;
 if(p.publish(1,4096,1,b,false,false)!=91 || p.publish(1,4096,1,b,true,true)!=91)return 1;
 if(p.publish(0,4096,1,b,true,false)!=60 || p.publish(1,UINT64_MAX-32,1,b,true,false)!=60)return 2;
 if(p.publish(1,4096,1,b,true,false))return 3;
 if(p.acquire(2,1,4096,64,b,out)!=88 || out!=sentinel)return 4;
 if(p.acquire(1,2,4096,64,b,out)!=88 || p.acquire(1,1,4097,64,b,out)!=88 || p.acquire(1,1,4096,63,b,out)!=88 || out!=sentinel)return 5;
 auto changed=b;changed[63]++;if(p.acquire(1,1,4096,64,changed,out)!=88 || out!=sentinel)return 6;
 if(p.acquire(1,1,4096,64,b,out) || out!=b)return 7;
 p.invalidate();out=sentinel;if(p.acquire(1,1,4096,64,b,out)!=88 || out!=sentinel)return 8;
 std::cout<<"retained display control passed: incomplete poisoned invalid identity stale generation shifted short changed bytes invalidated reject; synthetic only, no GPU or runtime binding\n";
}

#include "VPXLookaheadLimiter.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <cstdio>
int main(){
    for(float sr: {44100.f,48000.f,96000.f}){
        vpx::LookaheadLimiter limiter;limiter.reset(sr);
        for(int i=0;i<100000;++i){
            float l=(i%123==0)?10.f:std::sin(i*.031f)*1.2f;
            float r=(i%177==0)?-20.f:std::sin(i*.043f)*1.1f;
            if(i==200)l=std::numeric_limits<float>::quiet_NaN();
            if(i==201)r=std::numeric_limits<float>::infinity();
            limiter.process(l,r,(i%500<250)?.5f:.95f);
            assert(std::isfinite(l)&&std::isfinite(r));
            assert(std::fabs(l)<=.950001f&&std::fabs(r)<=.950001f);
        }
    }
    std::puts("PASS: 300000 stereo frames, NaN/Inf, overloads, 44.1/48/96 kHz");
    return 0;
}

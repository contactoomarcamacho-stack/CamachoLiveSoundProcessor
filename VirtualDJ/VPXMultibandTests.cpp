#include "VPXMultiband.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <cstdio>
int main(){
    for(float sr: {44100.f,48000.f,96000.f}){
        vpx::MultibandLR4 dsp;dsp.reset(sr);
        for(int i=0;i<200000;++i){
            float l=i%177==0?12.f:std::sin(i*.013f)*.75f;
            float r=i%229==0?-12.f:std::sin(i*.017f)*.75f;
            if(i==10)l=std::numeric_limits<float>::quiet_NaN();
            if(i==11)r=std::numeric_limits<float>::infinity();
            dsp.process(l,r);
            assert(std::isfinite(l)&&std::isfinite(r));
        }
    }
    std::puts("PASS: 600000 stereo frames LR4 multiband / subsonic, 44.1/48/96 kHz");
}

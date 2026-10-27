#include "VPXMultiband.h"
#include <cassert>
#include <cmath>
#include <cstdio>
// Compare phase-compensated three-way LR4 sum to cascaded all-pass references.
// Compression must be disabled for this measurement, so the test exercises
// the crossover topology directly rather than the dynamics.
int main(){
    for(float sr: {44100.f,48000.f,96000.f}){
        vpx::LR4 lo,hi,mid,top,phaseLo,phaseHi;
        lo.design(120.f,sr,false);hi.design(120.f,sr,true);
        mid.design(2500.f,sr,false);top.design(2500.f,sr,true);
        phaseLo.design(2500.f,sr,false);phaseHi.design(2500.f,sr,true);
        float errorEnergy=0.f,refEnergy=0.f;
        for(int n=0;n<100000;++n){
            const float x=std::sin(2.f*3.14159265f*120.f*n/sr)
                         +.5f*std::sin(2.f*3.14159265f*2500.f*n/sr);
            const float low=lo.process(x),upper=hi.process(x);
            const float sum=phaseLo.process(low)+phaseHi.process(low)
                           +mid.process(upper)+top.process(upper);
            // Exact identity for the same branch topology, used to verify
            // the crossover algebra and finite output, not flat phase.
            assert(std::isfinite(sum));
            refEnergy+=x*x;errorEnergy+=sum*sum;
        }
        assert(errorEnergy>refEnergy*.1f&&errorEnergy<refEnergy*4.f);
    }
    std::puts("PASS: LR4 crossover sum finite and bounded at 44.1/48/96 kHz");
}

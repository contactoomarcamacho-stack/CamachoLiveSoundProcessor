#include "VPXMultiband.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
using C=std::complex<double>;
static C response(const vpx::LR2& f,double hz,double sr) {
    const double w=2.*3.14159265358979323846*hz/sr;
    const C z=std::polar(1.,-w);
    return (double(f.b0)+double(f.b1)*z+double(f.b2)*z*z)/
           (1.+double(f.a1)*z+double(f.a2)*z*z);
}
static C response(const vpx::LR4& f,double hz,double sr) {
    return response(f.first,hz,sr)*response(f.second,hz,sr);
}
int main(){
    for(double sr: {44100.,48000.,96000.,192000.}){
        vpx::LR4 low,high,mid,top,phaseLo,phaseHi;
        low.design(120.f,float(sr),false);
        high.design(120.f,float(sr),true);
        mid.design(2500.f,float(sr),false);
        top.design(2500.f,float(sr),true);
        phaseLo.design(2500.f,float(sr),false);
        phaseHi.design(2500.f,float(sr),true);
        double maxMagnitudeError=0.,maxPhaseError=0.;
        // The reference is the cascade of the two LR4 all-pass sums.
        // Excludes the intentionally attenuating 28 Hz subsonic HPF.
        for(int i=0;i<4096;++i){
            const double hz=30.*std::pow((sr*.45)/30.,double(i)/4095.);
            const C a=response(low,hz,sr)+response(high,hz,sr);
            const C b=response(mid,hz,sr)+response(top,hz,sr);
            const C sum=response(low,hz,sr)*
                (response(phaseLo,hz,sr)+response(phaseHi,hz,sr))
                +response(high,hz,sr)*b;
            const C reference=a*b;
            const double magError=std::abs(std::abs(sum)-1.);
            const double phaseError=std::abs(std::arg(sum/std::max(1.e-20,std::abs(sum))
                                      / (reference/std::max(1.e-20,std::abs(reference)))));
            maxMagnitudeError=std::max(maxMagnitudeError,magError);
            maxPhaseError=std::max(maxPhaseError,phaseError);
            assert(std::isfinite(magError)&&std::isfinite(phaseError));
        }
        std::printf("SR %.0f: max magnitude deviation %.7f, max phase mismatch %.7f rad\n",
                    sr,maxMagnitudeError,maxPhaseError);
        // 0.01 linear ~= 0.087 dB. Phase comparison to cascade reference.
        if(maxMagnitudeError>0.01||maxPhaseError>0.01)return EXIT_FAILURE;
    }
    std::puts("PASS: 4096-point frequency response / SR, phase and magnitude");
}

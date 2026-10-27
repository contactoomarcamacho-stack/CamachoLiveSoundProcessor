#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace vpx {
struct LR2 {
    float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    void clear() noexcept {z1=z2=0;}
    void design(float hz,float sr,bool highpass) noexcept {
        constexpr float pi=3.14159265358979323846f;
        hz=std::clamp(hz,10.f,sr*.40f);
        const float w=2*pi*hz/sr,q=.70710678118f;
        const float cs=std::cos(w),alpha=std::sin(w)/(2*q);
        const float inv=1.f/(1.f+alpha);
        b0=(highpass?(1+cs):1-cs)*.5f*inv;
        b1=(highpass?-(1+cs):1-cs)*inv;
        b2=b0;
        a1=-2*cs*inv;a2=(1-alpha)*inv;
    }
    float process(float x) noexcept {
        const float y=b0*x+z1;
        z1=b1*x-a1*y+z2;z2=b2*x-a2*y;
        if(!std::isfinite(y)||!std::isfinite(z1)||!std::isfinite(z2)){clear();return 0.f;}
        return y;
    }
};
struct LR4 {
    LR2 first,second;
    void design(float hz,float sr,bool highpass) noexcept {
        first.design(hz,sr,highpass);second.design(hz,sr,highpass);
        clear();
    }
    void clear() noexcept {first.clear();second.clear();}
    float process(float x) noexcept {return second.process(first.process(x));}
};
class MultibandLR4 {
public:
    void reset(float sr) noexcept {
        sr_=std::clamp(sr,8000.f,384000.f);
        for(int ch=0;ch<2;++ch){
            sub_[ch].design(28.f,sr_,true);
            low_[ch].design(120.f,sr_,false);
            high120_[ch].design(120.f,sr_,true);
            mid_[ch].design(2500.f,sr_,false);
            high2500_[ch].design(2500.f,sr_,true);
            // Phase compensation: low band receives the all-pass response
            // of the second crossover, matching the high120 split branches.
            lowPhaseLP_[ch].design(2500.f,sr_,false);
            lowPhaseHP_[ch].design(2500.f,sr_,true);
        }
        for(float& e:env_)e=0.f;
        attack_=std::exp(-1.f/(.010f*sr_));
        release_=std::exp(-1.f/(.150f*sr_));
    }
    // Stereo-linked envelope for each band; no allocations or locks.
    void process(float& l,float& r) noexcept {
        const float input[2]={
            std::isfinite(l)?std::clamp(l,-16.f,16.f):0.f,
            std::isfinite(r)?std::clamp(r,-16.f,16.f):0.f
        };
        float bands[2][3]{};
        for(int ch=0;ch<2;++ch){
            const float clean=sub_[ch].process(input[ch]);
            const float low=low_[ch].process(clean);
            const float upper=high120_[ch].process(clean);
            bands[ch][0]=lowPhaseLP_[ch].process(low)+lowPhaseHP_[ch].process(low);
            bands[ch][1]=mid_[ch].process(upper);
            bands[ch][2]=high2500_[ch].process(upper);
        }
        for(int b=0;b<3;++b){
            const float peak=std::max(std::fabs(bands[0][b]),std::fabs(bands[1][b]));
            const float coef=peak>env_[b]?attack_:release_;
            env_[b]=coef*env_[b]+(1.f-coef)*peak;
            const float level=std::max(env_[b],1.e-8f);
            const float gain=level>.25118864f?std::sqrt(.25118864f/level):1.f;
            bands[0][b]*=gain;bands[1][b]*=gain;
        }
        l=bands[0][0]+bands[0][1]+bands[0][2];
        r=bands[1][0]+bands[1][1]+bands[1][2];
        if(!std::isfinite(l))l=0.f;
        if(!std::isfinite(r))r=0.f;
    }
private:
    float sr_=44100.f,attack_=0.f,release_=0.f;
    LR4 sub_[2],low_[2],high120_[2],mid_[2],high2500_[2];
    LR4 lowPhaseLP_[2],lowPhaseHP_[2];
    float env_[3]{};
};
}

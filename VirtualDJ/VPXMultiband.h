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
            for(int b=0;b<3;++b)env_[ch][b]=0.f;
        }
        const float attack=std::exp(-1.f/(.010f*sr_));
        const float release=std::exp(-1.f/(.150f*sr_));
        attack_=attack;release_=release;
    }
    // Three independent detectors; fixed crossover frequencies and conservative
    // 2:1 compression above -12 dBFS. No heap allocation in process().
    void process(float& l,float& r) noexcept {
        float* channels[2]={&l,&r};
        for(int ch=0;ch<2;++ch){
            const float x=std::isfinite(*channels[ch])?std::clamp(*channels[ch],-16.f,16.f):0.f;
            const float clean=sub_[ch].process(x);
            float bands[3]={
                low_[ch].process(clean),
                mid_[ch].process(high120_[ch].process(clean)),
                high2500_[ch].process(clean)
            };
            float out=0.f;
            for(int b=0;b<3;++b){
                const float peak=std::fabs(bands[b]);
                const float coef=peak>env_[ch][b]?attack_:release_;
                env_[ch][b]=coef*env_[ch][b]+(1.f-coef)*peak;
                const float level=std::max(env_[ch][b],1.e-8f);
                const float gain=level>.25118864f?std::sqrt(.25118864f/level):1.f;
                out+=bands[b]*gain;
            }
            *channels[ch]=std::isfinite(out)?out:0.f;
        }
    }
private:
    float sr_=44100.f,attack_=0.f,release_=0.f;
    LR4 sub_[2],low_[2],high120_[2],mid_[2],high2500_[2];
    float env_[2][3]{};
};
}

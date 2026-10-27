#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace vpx {
// 4x polyphase windowed-sinc interpolator for intersample-peak estimation.
// The FIR uses 32 taps, normalized at every fractional phase.
// A reconstruction peak detector, not an ITU-R BS.1770 certification claim.
class TruePeak4x final {
public:
    static constexpr int Taps=32;
    void reset() noexcept { for(auto& c:history_)c.fill(0.f);index_=0; }
    TruePeak4x() noexcept {
        constexpr double pi=3.14159265358979323846;
        for(int p=0;p<4;++p){
            const double phase=double(p)/4.;
            double sum=0.;
            for(int k=0;k<Taps;++k){
                const double t=double(k)-(Taps/2-1)-phase;
                const double sinc=std::fabs(t)<1.e-12?1.:std::sin(pi*t)/(pi*t);
                const double window=.42-.5*std::cos(2*pi*k/(Taps-1))+.08*std::cos(4*pi*k/(Taps-1));
                taps_[p][k]=float(sinc*window);sum+=sinc*window;
            }
            for(float& v:taps_[p])v=float(v/sum);
        }
        reset();
    }
    float push(float l,float r) noexcept {
        history_[0][index_]=std::isfinite(l)?l:0.f;
        history_[1][index_]=std::isfinite(r)?r:0.f;
        float peak=0.f;
        for(int p=0;p<4;++p){
            float outL=0.f,outR=0.f;
            for(int k=0;k<Taps;++k){
                const int j=(index_+Taps-k)%Taps;
                outL+=taps_[p][k]*history_[0][j];
                outR+=taps_[p][k]*history_[1][j];
            }
            peak=std::max(peak,std::max(std::fabs(outL),std::fabs(outR)));
        }
        index_=(index_+1)%Taps;
        return std::isfinite(peak)?peak:32.f;
    }
private:
    std::array<std::array<float,Taps>,4> taps_{};
    std::array<std::array<float,Taps>,2> history_{};
    int index_=0;
};
class LookaheadLimiter final {
public:
    static constexpr int Lookahead=256;
    static constexpr int Ring=Lookahead+1;
    void reset(float sampleRate) noexcept {
        sr_=std::clamp(sampleRate,8000.f,384000.f);
        delayL_.fill(0.f);delayR_.fill(0.f);peaks_.fill(0.f);
        detector_.reset();pos_=0;gain_=1.f;
        release_=std::exp(-1.f/(.080f*sr_));
    }
    static constexpr int latencySamples() noexcept {return Lookahead;}
    void process(float& left,float& right,float ceiling) noexcept {
        const float l=std::isfinite(left)?std::clamp(left,-32.f,32.f):0.f;
        const float r=std::isfinite(right)?std::clamp(right,-32.f,32.f):0.f;
        const float c=std::isfinite(ceiling)?std::clamp(ceiling,.01f,1.f):.90f;
        const float peak=detector_.push(l,r);
        const float delayedL=delayL_[pos_],delayedR=delayR_[pos_];
        delayL_[pos_]=l;delayR_[pos_]=r;peaks_[pos_]=peak;
        pos_=(pos_+1)%Ring;
        float futurePeak=peak;
        for(float p:peaks_)futurePeak=std::max(futurePeak,p);
        const float target=std::min(1.f,(c*.79432823f)/std::max(futurePeak,1.e-12f));
        gain_=target<gain_?target:release_*gain_+(1.f-release_)*target;
        left=std::clamp(delayedL*gain_,-c,c);
        right=std::clamp(delayedR*gain_,-c,c);
        if(!std::isfinite(left))left=0.f;
        if(!std::isfinite(right))right=0.f;
    }
private:
    float sr_=44100.f,release_=.999f,gain_=1.f;
    int pos_=0;
    TruePeak4x detector_;
    std::array<float,Ring> delayL_{},delayR_{},peaks_{};
};
}

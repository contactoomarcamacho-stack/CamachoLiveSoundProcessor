#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace vpx {
// Fixed-memory stereo-linked lookahead limiter.
// Four linear interpolation phases are a conservative peak estimator,
// NOT a standards-certified true-peak reconstruction filter.
class LookaheadLimiter final {
public:
    static constexpr int Lookahead=128;
    static constexpr int Ring=Lookahead+1;
    void reset(float sampleRate) noexcept {
        sr_=std::clamp(sampleRate,8000.f,384000.f);
        delayL_.fill(0.f);delayR_.fill(0.f);peaks_.fill(0.f);
        pos_=0;gain_=1.f;prevL_=prevR_=0.f;
        release_=std::exp(-1.f/(.080f*sr_));
    }
    static constexpr int latencySamples() noexcept {return Lookahead;}
    void process(float& left,float& right,float ceiling) noexcept {
        const float l=std::isfinite(left)?std::clamp(left,-32.f,32.f):0.f;
        const float r=std::isfinite(right)?std::clamp(right,-32.f,32.f):0.f;
        const float c=std::isfinite(ceiling)?std::clamp(ceiling,.01f,1.f):.90f;
        float peak=std::max(std::fabs(l),std::fabs(r));
        for(int phase=1;phase<4;++phase){
            const float t=phase*.25f;
            peak=std::max(peak,std::fabs(prevL_+(l-prevL_)*t));
            peak=std::max(peak,std::fabs(prevR_+(r-prevR_)*t));
        }
        prevL_=l;prevR_=r;
        const float delayedL=delayL_[pos_],delayedR=delayR_[pos_];
        delayL_[pos_]=l;delayR_[pos_]=r;peaks_[pos_]=peak;
        pos_=(pos_+1)%Ring;
        float futurePeak=peak;
        for(float p:peaks_)futurePeak=std::max(futurePeak,p);
        const float target=std::min(1.f,(c*.89125094f)/std::max(futurePeak,1.e-12f));
        gain_=target<gain_?target:release_*gain_+(1.f-release_)*target;
        left=std::clamp(delayedL*gain_,-c,c);
        right=std::clamp(delayedR*gain_,-c,c);
        if(!std::isfinite(left))left=0.f;
        if(!std::isfinite(right))right=0.f;
    }
private:
    float sr_=44100.f,release_=.999f,gain_=1.f,prevL_=0.f,prevR_=0.f;
    int pos_=0;
    std::array<float,Ring> delayL_{},delayR_{},peaks_{};
};
}

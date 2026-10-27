#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
namespace vpx {
// UI -> DSP, single-writer-per-control atomics; audio callback never locks.
struct ControlBank {
    static constexpr int Knobs=10, EqBands=6;
    std::array<std::atomic<float>,Knobs> knobs{};
    std::array<std::atomic<float>,EqBands> eq{};
    std::atomic<float> width{.5f},transient{.5f},exciter{0.f},reverb{0.f},delay{0.f};
    std::atomic<int> preset{0},dither{0},bypass{0};
    // DSP -> UI telemetry, atomics prevent races; not a coherent multi-field snapshot.
    std::atomic<float> meter{0.f},inputPeak{0.f},outputPeak{0.f},correlation{0.f};
    ControlBank() noexcept {
        for(auto& p:knobs)p.store(.5f,std::memory_order_relaxed);
        for(auto& p:eq)p.store(.5f,std::memory_order_relaxed);
        knobs[5].store(.6f,std::memory_order_relaxed);
        knobs[6].store(.25f,std::memory_order_relaxed);
        knobs[7].store(.92f,std::memory_order_relaxed);
        knobs[8].store(.3f,std::memory_order_relaxed);
        knobs[9].store(.35f,std::memory_order_relaxed);
    }
    static float safe(float v,float fallback=.5f) noexcept {
        return std::isfinite(v)?std::clamp(v,0.f,1.f):fallback;
    }
    void setKnob(int i,float v) noexcept {
        if(i>=0&&i<Knobs)knobs[i].store(safe(v),std::memory_order_relaxed);
    }
    void setEq(int i,float v) noexcept {
        if(i>=0&&i<EqBands)eq[i].store(safe(v),std::memory_order_relaxed);
    }
};
}

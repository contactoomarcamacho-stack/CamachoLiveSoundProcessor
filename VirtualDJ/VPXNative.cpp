// VPX PRO native VirtualDJ DSP, SDK v8, stereo float audio.
#include "vdjDsp8.h"
#include <algorithm>
#include <cmath>
#include <cstring>

class VPXNative final : public IVdjPluginDsp8 {
public:
    HRESULT VDJ_API OnLoad() override { return S_OK; }
    HRESULT VDJ_API OnGetPluginInfo(TVdjPluginInfo8* info) override {
        info->PluginName = "VPX PRO MASTERING PROCESSOR";
        info->Author = "VPX";
        info->Description = "VPX PRO stereo mastering DSP for VirtualDJ 8";
        info->Version = "2.0";
        info->Flags = 0;
        info->Bitmap = NULL;
        return S_OK;
    }
    ULONG VDJ_API Release() override { delete this; return 0; }
    HRESULT VDJ_API OnStart() override { envelope = 0.0f; return S_OK; }
    HRESULT VDJ_API OnStop() override { envelope = 0.0f; return S_OK; }
    HRESULT VDJ_API OnProcessSamples(float* buffer, int frames) override {
        if (!buffer || frames <= 0) return S_OK;
        const float sr = (SampleRate > 8000) ? float(SampleRate) : 44100.0f;
        const float attack = std::exp(-1.0f / (0.015f * sr));
        const float release = std::exp(-1.0f / (0.180f * sr));
        const float threshold = 0.28f; // approximately -11 dBFS
        const float ratio = 2.0f;
        const float ceiling = 0.89125f; // -1 dBFS, sample peak only
        for (int i = 0; i < frames; ++i) {
            const float l = std::isfinite(buffer[2*i]) ? buffer[2*i] : 0.0f;
            const float r = std::isfinite(buffer[2*i+1]) ? buffer[2*i+1] : 0.0f;
            const float peak = std::max(std::abs(l), std::abs(r));
            const float coef = peak > envelope ? attack : release;
            envelope = coef * envelope + (1.0f - coef) * peak;
            const float level = std::max(envelope, 1.0e-9f);
            const float gain = level > threshold ? std::pow(level / threshold, (1.0f / ratio) - 1.0f) : 1.0f;
            buffer[2*i] = std::clamp(l * gain, -ceiling, ceiling);
            buffer[2*i+1] = std::clamp(r * gain, -ceiling, ceiling);
        }
        return S_OK;
    }
private:
    float envelope = 0.0f;
};

extern "C" __declspec(dllexport) HRESULT VDJ_API DllGetClassObject(const GUID& clsid, const GUID& iid, void** object) {
    if (!object) return E_POINTER;
    *object = nullptr;
    if (std::memcmp(&clsid, &CLSID_VdjPlugin8, sizeof(GUID)) != 0 ||
        std::memcmp(&iid, &IID_IVdjPluginDsp8, sizeof(GUID)) != 0)
        return CLASS_E_CLASSNOTAVAILABLE;
    *object = new VPXNative();
    return S_OK;
}

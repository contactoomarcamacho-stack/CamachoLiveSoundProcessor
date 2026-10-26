// VPX PRO: VirtualDJ 8 native 64-bit DSP with host-rendered controls.
#include "vdjDsp8.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

class VPXNative final : public IVdjPluginDsp8 {
public:
    enum { BYPASS, INPUT, BASS, MID, TREBLE, THRESHOLD, RATIO, ATTACK, RELEASE_TIME, CEILING, OUTPUT, RESET };
    HRESULT VDJ_API OnLoad() override {
        DeclareParameterSwitch(&bypass, BYPASS, "Bypass", "Off", false);
        DeclareParameterSlider(&input, INPUT, "Input gain", "In", 0.5f);
        DeclareParameterSlider(&bass, BASS, "Bass EQ", "Bass", 0.5f);
        DeclareParameterSlider(&mid, MID, "Mid EQ", "Mid", 0.5f);
        DeclareParameterSlider(&treble, TREBLE, "Treble EQ", "High", 0.5f);
        DeclareParameterSlider(&threshold, THRESHOLD, "Compressor threshold", "Thr", 0.6f);
        DeclareParameterSlider(&ratio, RATIO, "Compression ratio", "Ratio", 0.25f);
        DeclareParameterSlider(&attack, ATTACK, "Attack", "Atk", 0.3f);
        DeclareParameterSlider(&releaseTime, RELEASE_TIME, "Release", "Rel", 0.35f);
        DeclareParameterSlider(&ceiling, CEILING, "Limiter ceiling", "Limit", 0.92f);
        DeclareParameterSlider(&output, OUTPUT, "Output gain", "Out", 0.5f);
        DeclareParameterButton(&reset, RESET, "Reset controls", "Reset");
        return S_OK;
    }
    HRESULT VDJ_API OnGetUserInterface(TVdjPluginInterface8* ui) override {
        ui->Type = VDJINTERFACE_DEFAULT;
        return S_OK;
    }
    HRESULT VDJ_API OnParameter(int id) override {
        if (id == RESET && reset) {
            input=bass=mid=treble=output=0.5f;
            threshold=0.6f; ratio=0.25f; attack=0.3f; releaseTime=0.35f; ceiling=0.92f;
            reset=0;
        }
        return S_OK;
    }
    HRESULT VDJ_API OnGetParameterString(int id, char* out, int size) override {
        if (!out || size<=0) return E_INVALIDARG;
        float value=0;
        switch(id) {
            case INPUT: value=(input-0.5f)*24; break;
            case BASS: value=(bass-0.5f)*24; break;
            case MID: value=(mid-0.5f)*24; break;
            case TREBLE: value=(treble-0.5f)*24; break;
            case OUTPUT: value=(output-0.5f)*24; break;
            case THRESHOLD: value=-30+threshold*30; break;
            case CEILING: value=-12+ceiling*11.7f; break;
            case RATIO: std::snprintf(out,size,"%.1f:1",1+ratio*7); return S_OK;
            case ATTACK: std::snprintf(out,size,"%.0f ms",2+attack*98); return S_OK;
            case RELEASE_TIME: std::snprintf(out,size,"%.0f ms",50+releaseTime*550); return S_OK;
            default: return E_NOTIMPL;
        }
        std::snprintf(out,size,"%+.1f dB",value);
        return S_OK;
    }
    HRESULT VDJ_API OnGetPluginInfo(TVdjPluginInfo8* info) override {
        info->PluginName="VPX PRO MASTERING PROCESSOR";
        info->Author="VPX";
        info->Description="Stereo EQ, compressor, peak limiter and adjustable gain";
        info->Version="3.0";
        info->Flags=0;
        info->Bitmap=NULL;
        return S_OK;
    }
    ULONG VDJ_API Release() override { delete this; return 0; }
    HRESULT VDJ_API OnStart() override { env=0; lowL=lowR=highL=highR=0; return S_OK; }
    HRESULT VDJ_API OnStop() override { env=0; return S_OK; }
    HRESULT VDJ_API OnProcessSamples(float* buffer,int frames) override {
        if (!buffer || frames<=0 || bypass) return S_OK;
        const float sr=(SampleRate>8000)?float(SampleRate):44100.0f;
        const float aLow=1-std::exp(-6.2831853f*180/sr);
        const float aHigh=1-std::exp(-6.2831853f*3800/sr);
        const float gIn=std::pow(10.0f,((input-0.5f)*24)/20);
        const float gOut=std::pow(10.0f,((output-0.5f)*24)/20);
        const float gBass=std::pow(10.0f,((bass-0.5f)*24)/20);
        const float gMid=std::pow(10.0f,((mid-0.5f)*24)/20);
        const float gHigh=std::pow(10.0f,((treble-0.5f)*24)/20);
        const float thr=std::pow(10.0f,(-30+threshold*30)/20);
        const float compRatio=1+ratio*7;
        const float aAtk=std::exp(-1/(sr*(0.002f+attack*0.098f)));
        const float aRel=std::exp(-1/(sr*(0.05f+releaseTime*0.55f)));
        const float limit=std::pow(10.0f,(-12+ceiling*11.7f)/20);
        for(int i=0;i<frames;++i) {
            float l=std::isfinite(buffer[i*2])?buffer[i*2]*gIn:0;
            float r=std::isfinite(buffer[i*2+1])?buffer[i*2+1]*gIn:0;
            lowL+=aLow*(l-lowL); lowR+=aLow*(r-lowR);
            highL+=aHigh*(l-highL); highR+=aHigh*(r-highR);
            l=lowL*gBass+(highL-lowL)*gMid+(l-highL)*gHigh;
            r=lowR*gBass+(highR-lowR)*gMid+(r-highR)*gHigh;
            const float peak=std::max(std::abs(l),std::abs(r));
            const float coef=peak>env?aAtk:aRel;
            env=coef*env+(1-coef)*peak;
            const float level=std::max(env,1.0e-9f);
            const float gain=level>thr?std::pow(level/thr,1/compRatio-1):1;
            buffer[i*2]=std::clamp(l*gain*gOut,-limit,limit);
            buffer[i*2+1]=std::clamp(r*gain*gOut,-limit,limit);
        }
        return S_OK;
    }
private:
    int bypass=0,reset=0;
    float input=.5f,bass=.5f,mid=.5f,treble=.5f,threshold=.6f,ratio=.25f,attack=.3f,releaseTime=.35f,ceiling=.92f,output=.5f;
    float env=0,lowL=0,lowR=0,highL=0,highR=0;
};
extern "C" __declspec(dllexport) HRESULT VDJ_API DllGetClassObject(const GUID& clsid,const GUID& iid,void** object) {
    if (!object) return E_POINTER;
    *object=nullptr;
    if(std::memcmp(&clsid,&CLSID_VdjPlugin8,sizeof(GUID))!=0 || std::memcmp(&iid,&IID_IVdjPluginDsp8,sizeof(GUID))!=0)
        return CLASS_E_CLASSNOTAVAILABLE;
    *object=new VPXNative();
    return S_OK;
}

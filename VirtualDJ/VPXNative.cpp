// VPX PRO: VirtualDJ 8 native 64-bit DSP with host-rendered controls.
#include "vdjDsp8.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include "VPXCustomUI.h"
#include "VPXPresets.h"
#include "VPXLookaheadLimiter.h"
#include "VPXMultiband.h"
#include "VPXControlBank.h"
#include <atomic>
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
        if (!ui) return E_POINTER;
        for(int i=0;i<10;++i)editor.params[i]=&controls.knobs[i];
        for(int i=0;i<6;++i)editor.eq[i]=&controls.eq[i];
        editor.stereoWidth=&controls.width;
        editor.transient=&controls.transient;editor.exciter=&controls.exciter;
        editor.reverb=&controls.reverb;editor.delay=&controls.delay;
        editor.inputPeak=&controls.inputPeak;editor.outputPeak=&controls.outputPeak;
        editor.preset=&controls.preset;editor.stereoCorrelation=&controls.correlation;
        editor.ditherMode=&controls.dither;editor.bypass=&controls.bypass;
        editor.meter=&controls.meter;
        ui->Type=VDJINTERFACE_DIALOG;
        ui->hWnd=editor.open(hInstance);
        return ui->hWnd ? S_OK : E_FAIL;
    }
    HRESULT VDJ_API OnParameter(int id) override {
        if (id == RESET && reset) {
            input=bass=mid=treble=output=0.5f;
            threshold=0.6f; ratio=0.25f; attack=0.3f; releaseTime=0.35f; ceiling=0.92f;
            reset=0;
            const float defaults[10]={.5f,.5f,.5f,.5f,.5f,.6f,.25f,.92f,.3f,.35f};
            for(int i=0;i<10;++i)controls.setKnob(i,defaults[i]);
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
        info->Description="Stereo EQ, compressor, linked lookahead limiter and adjustable gain";
        info->Version="4.3";
        info->Flags=0;
        info->Bitmap=NULL;
        return S_OK;
    }
    ULONG VDJ_API Release() override { if(editor.hwnd && IsWindow(editor.hwnd)) DestroyWindow(editor.hwnd); delete this; return 0; }
    HRESULT VDJ_API OnStart() override { env=0; lowL=lowR=highL=highR=0; for(auto& ch:filters)for(auto& f:ch)f={}; limiter.reset((SampleRate>=8000&&SampleRate<=384000)?float(SampleRate):44100.f); multiband.reset((SampleRate>=8000&&SampleRate<=384000)?float(SampleRate):44100.f); return S_OK; }
    HRESULT VDJ_API OnStop() override { env=0; return S_OK; }
    HRESULT VDJ_API OnProcessSamples(float* buffer,int frames) override {
        if (!buffer || frames<=0) return S_OK;
        // Host-owned float parameters remain SDK-compatible. Host automation is
        // not yet synchronized back into ControlBank; see integration notes.
        if(controls.bypass.load(std::memory_order_relaxed)) return S_OK;
        const float uiKnobs[10]={controls.knobs[0].load(),controls.knobs[1].load(),controls.knobs[2].load(),controls.knobs[3].load(),controls.knobs[4].load(),controls.knobs[5].load(),controls.knobs[6].load(),controls.knobs[7].load(),controls.knobs[8].load(),controls.knobs[9].load()};
        const float input=uiKnobs[0],treble=uiKnobs[1],output=uiKnobs[2],bass=uiKnobs[3],mid=uiKnobs[4],threshold=uiKnobs[5],ratio=uiKnobs[6],ceiling=uiKnobs[7],attack=uiKnobs[8],releaseTime=uiKnobs[9];
        const float stereoWidth=controls.width.load(),transient=controls.transient.load(),exciter=controls.exciter.load(),reverb=controls.reverb.load(),delayMix=controls.delay.load();
        const int ditherMode=controls.dither.load();
        const float sr=(SampleRate>=8000 && SampleRate<=384000)?float(SampleRate):44100.0f;
        if(sr!=lastSampleRate){lastSampleRate=sr;env=0.f;lowL=lowR=highL=highR=0.f;transientEnv=0.f;for(auto& ch:filters)for(auto& f:ch)f={};limiter.reset(sr);multiband.reset(sr);}
        const float smooth=1.f-std::exp(-1.f/(sr*.015f));
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
        const float hz[6]={60,180,500,1800,6000,12000};
        float b0[6],b1[6],b2[6],a1[6],a2[6];
        for(int k=0;k<6;++k){
            const int activePreset=std::clamp(controls.preset.load(std::memory_order_relaxed),0,vpx::presetCount-1);
            const float targetEq=activePreset==0?controls.eq[k].load(std::memory_order_relaxed):vpx::presets[activePreset].eq[k];
            const float A=std::pow(10.f,((targetEq-.5f)*24.f)/40.f);
            const float omega=6.2831853f*std::min(hz[k],sr*.45f)/sr;
            const float alpha=std::sin(omega)/2.8f;
            const float den=1.f+alpha/A;
            b0[k]=(1+alpha*A)/den; b1[k]=-2*std::cos(omega)/den;
            b2[k]=(1-alpha*A)/den; a1[k]=b1[k]; a2[k]=(1-alpha/A)/den;
        }
        for(int i=0;i<frames;++i) {
            for(int k=0;k<6;++k){
                eqB0[k]+=smooth*(b0[k]-eqB0[k]);
                eqB1[k]+=smooth*(b1[k]-eqB1[k]);
                eqB2[k]+=smooth*(b2[k]-eqB2[k]);
                eqA1[k]+=smooth*(a1[k]-eqA1[k]);
                eqA2[k]+=smooth*(a2[k]-eqA2[k]);
            }
            smoothBass+=smooth*(gBass-smoothBass);
            smoothMid+=smooth*(gMid-smoothMid);
            smoothTreble+=smooth*(gHigh-smoothTreble);
            smoothThreshold+=smooth*(thr-smoothThreshold);
            smoothRatio+=smooth*(compRatio-smoothRatio);
            smoothTransient+=smooth*(transient-smoothTransient);
            smoothExciter+=smooth*(exciter-smoothExciter);
            smoothReverb+=smooth*(reverb-smoothReverb);
            smoothDelay+=smooth*(delayMix-smoothDelay);
            smoothIn+=smooth*(gIn-smoothIn);
            smoothOut+=smooth*(gOut-smoothOut);
            smoothWidth+=smooth*(stereoWidth*2.f-smoothWidth);
            smoothLimit+=smooth*(limit-smoothLimit);
            float l=std::isfinite(buffer[i*2])?std::clamp(buffer[i*2],-16.f,16.f)*smoothIn:0.f;
            float r=std::isfinite(buffer[i*2+1])?std::clamp(buffer[i*2+1],-16.f,16.f)*smoothIn:0.f;
            lowL+=aLow*(l-lowL); lowR+=aLow*(r-lowR);
            highL+=aHigh*(l-highL); highR+=aHigh*(r-highR);
            l=lowL*smoothBass+(highL-lowL)*smoothMid+(l-highL)*smoothTreble;
            r=lowR*smoothBass+(highR-lowR)*smoothMid+(r-highR)*smoothTreble;
            float* ch[2]={&l,&r};
            for(int channel=0;channel<2;++channel) for(int k=0;k<6;++k){
                auto& f=filters[channel][k];
                const float x=*ch[channel];
                const float y=eqB0[k]*x+f.z1;
                f.z1=eqB1[k]*x-eqA1[k]*y+f.z2;
                f.z2=eqB2[k]*x-eqA2[k]*y;
                if(!std::isfinite(f.z1)||!std::isfinite(f.z2)){f.z1=0.f;f.z2=0.f;}
                *ch[channel]=y;
            }
            multiband.process(l,r);
            const float mid=.5f*(l+r),side=.5f*(l-r)*smoothWidth;
            l=mid+side; r=mid-side;
            const float peak=std::max(std::abs(l),std::abs(r));
            correlationLL=.999f*correlationLL+.001f*l*l;
            correlationRR=.999f*correlationRR+.001f*r*r;
            correlationLR=.999f*correlationLR+.001f*l*r;
            if((i&255)==0)controls.correlation.store(std::clamp(correlationLR/std::sqrt(std::max(1.e-12f,correlationLL*correlationRR)),-1.f,1.f));
            if((i&255)==0) controls.inputPeak.store(.93f*controls.inputPeak.load()+.07f*std::min(1.f,peak));
            const float tcoef=std::exp(-1.f/(sr*.025f));
            transientEnv=tcoef*transientEnv+(1-tcoef)*peak;
            const float tGain=std::clamp(1.f+(smoothTransient-.5f)*2.f*(peak-transientEnv)/(transientEnv+.1f),.25f,2.f);
            l*=tGain;r*=tGain;
            exciterLowL+=aHigh*(l-exciterLowL);exciterLowR+=aHigh*(r-exciterLowR);
            l+=smoothExciter*.35f*std::tanh((l-exciterLowL)*4.f);
            r+=smoothExciter*.35f*std::tanh((r-exciterLowR)*4.f);
            const int tap=(delayPos+delaySize-std::min(delaySize-1,(int)(sr*.22f)))%delaySize;
            const float dl=delayL[tap],dr=delayR[tap];
            delayL[delayPos]=std::clamp(l+dl*.35f,-2.f,2.f);
            delayR[delayPos]=std::clamp(r+dr*.35f,-2.f,2.f);
            delayPos=(delayPos+1)%delaySize;
            l+=dl*(smoothDelay*.45f+smoothReverb*.25f);
            r+=dr*(delayMix*.45f+reverb*.25f);
            if ((i & 255)==0) controls.meter.store(.97f*controls.meter.load()+.03f*std::min(1.0f,peak));
            const float coef=peak>env?aAtk:aRel;
            env=coef*env+(1-coef)*peak;
            const float level=std::max(env,1.0e-9f);
            const float gain=level>smoothThreshold?std::pow(level/smoothThreshold,1/smoothRatio-1):1;
            float outL=l*gain*smoothOut;
            float outR=r*gain*smoothOut;
            limiter.process(outL,outR,smoothLimit);
            if(ditherMode){const float lsb=ditherMode==1?1.f/32768.f:1.f/8388608.f;
                outL=std::clamp(outL+(random01()-random01())*lsb,-smoothLimit,smoothLimit);
                outR=std::clamp(outR+(random01()-random01())*lsb,-smoothLimit,smoothLimit);
            }
            buffer[i*2]=std::isfinite(outL)?outL:0.f;
            buffer[i*2+1]=std::isfinite(outR)?outR:0.f;
            if((i&255)==0) controls.outputPeak.store(.93f*controls.outputPeak.load()+.07f*std::min(1.f,std::max(std::abs(buffer[i*2]),std::abs(buffer[i*2+1]))));
        }
        return S_OK;
    }
private:
    vpx::ControlBank controls;
    vpx::LookaheadLimiter limiter;
    vpx::MultibandLR4 multiband;
    float lastSampleRate=0.f,smoothIn=1.f,smoothOut=1.f,smoothWidth=1.f,smoothLimit=.95f;
    float smoothBass=1.f,smoothMid=1.f,smoothTreble=1.f,smoothThreshold=.5f,smoothRatio=2.f;
    float smoothTransient=.5f,smoothExciter=0.f,smoothReverb=0.f,smoothDelay=0.f;
    float eqB0[6]={1,1,1,1,1,1},eqB1[6]={},eqB2[6]={},eqA1[6]={},eqA2[6]={};
    int bypass=0,reset=0;
    float input=.5f,bass=.5f,mid=.5f,treble=.5f,threshold=.6f,ratio=.25f,attack=.3f,releaseTime=.35f,ceiling=.92f,output=.5f;
    float env=0,lowL=0,lowR=0,highL=0,highR=0;
    float uiMeter=0;
    float transient=.5f,exciter=0.f,reverb=0.f,delayMix=0.f;
    float inputPeak=0,outputPeak=0;
    std::atomic<int> preset{0};
    int ditherMode=0;
    float stereoCorrelation=0,correlationLL=0,correlationRR=0,correlationLR=0;
    unsigned int rng=0x9e3779b9u;
    float random01(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return float(rng&0xffffffu)/16777216.f;}
    float transientEnv=0,exciterLowL=0,exciterLowR=0;
    static constexpr int delaySize=96000;
    float delayL[delaySize]={},delayR[delaySize]={};
    int delayPos=0;
    float eq[6]={.5f,.5f,.5f,.5f,.5f,.5f};
    float stereoWidth=.5f;
    struct Filter { float z1=0,z2=0; } filters[2][6];
    VPXEditor editor;
};
extern "C" __declspec(dllexport) HRESULT VDJ_API DllGetClassObject(const GUID& clsid,const GUID& iid,void** object) {
    if (!object) return E_POINTER;
    *object=nullptr;
    if(std::memcmp(&clsid,&CLSID_VdjPlugin8,sizeof(GUID))!=0 || std::memcmp(&iid,&IID_IVdjPluginDsp8,sizeof(GUID))!=0)
        return CLASS_E_CLASSNOTAVAILABLE;
    *object=new VPXNative();
    return S_OK;
}

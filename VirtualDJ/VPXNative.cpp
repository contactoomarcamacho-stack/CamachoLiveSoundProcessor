// VPX PRO: VirtualDJ 8 native 64-bit DSP with host-rendered controls.
#include "vdjDsp8.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include "VPXCustomUI.h"
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
        editor.params[0]=&input; editor.params[1]=&treble; editor.params[2]=&output;
        editor.params[3]=&bass; editor.params[4]=&mid; editor.params[5]=&threshold;
        editor.params[6]=&ratio; editor.params[7]=&ceiling;
        editor.params[8]=&attack; editor.params[9]=&releaseTime;
        for(int i=0;i<6;++i) editor.eq[i]=&eq[i];
        editor.stereoWidth=&stereoWidth;
        editor.transient=&transient; editor.exciter=&exciter;
        editor.reverb=&reverb; editor.delay=&delayMix;
        editor.inputPeak=&inputPeak; editor.outputPeak=&outputPeak;
        editor.preset=&preset;
        editor.bypass=&bypass; editor.meter=&uiMeter;
        ui->Type=VDJINTERFACE_DIALOG;
        ui->hWnd=editor.open(hInstance);
        return ui->hWnd ? S_OK : E_FAIL;
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
        info->Version="4.0";
        info->Flags=0;
        info->Bitmap=NULL;
        return S_OK;
    }
    ULONG VDJ_API Release() override { if(editor.hwnd && IsWindow(editor.hwnd)) DestroyWindow(editor.hwnd); delete this; return 0; }
    HRESULT VDJ_API OnStart() override { env=0; lowL=lowR=highL=highR=0; for(auto& ch:filters)for(auto& f:ch)f={}; return S_OK; }
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
        const float hz[6]={60,180,500,1800,6000,12000};
        float b0[6],b1[6],b2[6],a1[6],a2[6];
        for(int k=0;k<6;++k){
            const float A=std::pow(10.f,((eq[k]-.5f)*24.f)/40.f);
            const float omega=6.2831853f*hz[k]/sr;
            const float alpha=std::sin(omega)/2.8f;
            const float den=1.f+alpha/A;
            b0[k]=(1+alpha*A)/den; b1[k]=-2*std::cos(omega)/den;
            b2[k]=(1-alpha*A)/den; a1[k]=b1[k]; a2[k]=(1-alpha/A)/den;
        }
        for(int i=0;i<frames;++i) {
            float l=std::isfinite(buffer[i*2])?buffer[i*2]*gIn:0;
            float r=std::isfinite(buffer[i*2+1])?buffer[i*2+1]*gIn:0;
            lowL+=aLow*(l-lowL); lowR+=aLow*(r-lowR);
            highL+=aHigh*(l-highL); highR+=aHigh*(r-highR);
            l=lowL*gBass+(highL-lowL)*gMid+(l-highL)*gHigh;
            r=lowR*gBass+(highR-lowR)*gMid+(r-highR)*gHigh;
            float* ch[2]={&l,&r};
            for(int channel=0;channel<2;++channel) for(int k=0;k<6;++k){
                auto& f=filters[channel][k];
                const float x=*ch[channel];
                const float y=b0[k]*x+f.z1;
                f.z1=b1[k]*x-a1[k]*y+f.z2;
                f.z2=b2[k]*x-a2[k]*y;
                *ch[channel]=y;
            }
            const float mid=.5f*(l+r),side=.5f*(l-r)*(stereoWidth*2.f);
            l=mid+side; r=mid-side;
            const float peak=std::max(std::abs(l),std::abs(r));
            if((i&255)==0) inputPeak=.93f*inputPeak+.07f*std::min(1.f,peak);
            const float tcoef=std::exp(-1.f/(sr*.025f));
            transientEnv=tcoef*transientEnv+(1-tcoef)*peak;
            const float tGain=std::clamp(1.f+(transient-.5f)*2.f*(peak-transientEnv)/(transientEnv+.1f),.25f,2.f);
            l*=tGain;r*=tGain;
            exciterLowL+=aHigh*(l-exciterLowL);exciterLowR+=aHigh*(r-exciterLowR);
            l+=exciter*.35f*std::tanh((l-exciterLowL)*4.f);
            r+=exciter*.35f*std::tanh((r-exciterLowR)*4.f);
            const int tap=(delayPos+delaySize-std::min(delaySize-1,(int)(sr*.22f)))%delaySize;
            const float dl=delayL[tap],dr=delayR[tap];
            delayL[delayPos]=std::clamp(l+dl*.35f,-2.f,2.f);
            delayR[delayPos]=std::clamp(r+dr*.35f,-2.f,2.f);
            delayPos=(delayPos+1)%delaySize;
            l+=dl*(delayMix*.45f+reverb*.25f);
            r+=dr*(delayMix*.45f+reverb*.25f);
            if ((i & 255)==0) uiMeter=0.97f*uiMeter+0.03f*std::min(1.0f,peak);
            const float coef=peak>env?aAtk:aRel;
            env=coef*env+(1-coef)*peak;
            const float level=std::max(env,1.0e-9f);
            const float gain=level>thr?std::pow(level/thr,1/compRatio-1):1;
            buffer[i*2]=std::clamp(l*gain*gOut,-limit,limit);
            buffer[i*2+1]=std::clamp(r*gain*gOut,-limit,limit);
            if((i&255)==0) outputPeak=.93f*outputPeak+.07f*std::min(1.f,std::max(std::abs(buffer[i*2]),std::abs(buffer[i*2+1])));
        }
        return S_OK;
    }
private:
    int bypass=0,reset=0;
    float input=.5f,bass=.5f,mid=.5f,treble=.5f,threshold=.6f,ratio=.25f,attack=.3f,releaseTime=.35f,ceiling=.92f,output=.5f;
    float env=0,lowL=0,lowR=0,highL=0,highR=0;
    float uiMeter=0;
    float transient=.5f,exciter=0.f,reverb=0.f,delayMix=0.f;
    float inputPeak=0,outputPeak=0;
    int preset=0;
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

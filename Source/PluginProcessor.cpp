#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
juce::AudioProcessorValueTreeState::ParameterLayout CamachoProcessor::makeLayout(){
 juce::AudioProcessorValueTreeState::ParameterLayout p;
 auto add=[&](const char* id,const char* name,float a,float b,float d){p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,juce::NormalisableRange<float>(a,b,0.1f),d));};
 add("sub","Subsonic Hz",20,65,32);add("bass","Bass dB",-12,12,0);add("lowmid","Low Mid dB",-12,12,0);add("highmid","High Mid dB",-12,12,0);add("treble","Treble dB",-12,12,0);
 add("threshold","Compressor Threshold dB",-30,0,-12);add("ratio","Compressor Ratio",1,6,2);add("attack","Attack ms",5,100,20);add("release","Release ms",50,600,180);
 add("input","Input dB",-18,12,0);add("output","Output dB",-18,6,0);add("ceiling","Limiter Ceiling dB",-12,-0.3f,-1);p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));return p;
}
CamachoProcessor::CamachoProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),apvts(*this,nullptr,"PARAMETERS",makeLayout()){}
void CamachoProcessor::prepareToPlay(double sr,int block){sampleRateHz=sr;juce::dsp::ProcessSpec spec{sr,(juce::uint32)juce::jmax(1,block),(juce::uint32)getTotalNumOutputChannels()};highpass.prepare(spec);low.prepare(spec);lowmid.prepare(spec);highmid.prepare(spec);high.prepare(spec);compressor.prepare(spec);highpass.reset();low.reset();lowmid.reset();highmid.reset();high.reset();compressor.reset();limiterEnvelope=1;}
void CamachoProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&){
 juce::ScopedNoDenormals noDenormals;
 for(int c=getTotalNumInputChannels();c<getTotalNumOutputChannels();++c)buffer.clear(c,0,buffer.getNumSamples());
 if(*apvts.getRawParameterValue("bypass")>0.5f)return;
 auto v=[&](const char* id){return apvts.getRawParameterValue(id)->load();};
 auto sr=sampleRateHz;
 *highpass.state=*juce::dsp::IIR::Coefficients<float>::makeHighPass(sr,v("sub"),0.7071f);
 *low.state=*juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr,95.0f,0.7071f,juce::Decibels::decibelsToGain(v("bass")));
 *lowmid.state=*juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr,320.0f,0.8f,juce::Decibels::decibelsToGain(v("lowmid")));
 *highmid.state=*juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr,2800.0f,0.8f,juce::Decibels::decibelsToGain(v("highmid")));
 *high.state=*juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr,9500.0f,0.7071f,juce::Decibels::decibelsToGain(v("treble")));
 buffer.applyGain(juce::Decibels::decibelsToGain(v("input")));
 juce::dsp::AudioBlock<float> audio(buffer);juce::dsp::ProcessContextReplacing<float> ctx(audio);
 highpass.process(ctx);low.process(ctx);lowmid.process(ctx);highmid.process(ctx);high.process(ctx);
 compressor.setThreshold(v("threshold"));compressor.setRatio(v("ratio"));compressor.setAttack(v("attack"));compressor.setRelease(v("release"));compressor.process(ctx);
 buffer.applyGain(juce::Decibels::decibelsToGain(v("output")));
 const float ceiling=juce::Decibels::decibelsToGain(v("ceiling"));
 const float release=std::exp(-1.0f/(float)(sr*0.1));
 for(int i=0;i<buffer.getNumSamples();++i){float peak=0;for(int c=0;c<buffer.getNumChannels();++c)peak=juce::jmax(peak,std::abs(buffer.getSample(c,i)));float target=peak>ceiling?ceiling/peak:1.0f;limiterEnvelope=target<limiterEnvelope?target:release*limiterEnvelope+(1-release)*target;for(int c=0;c<buffer.getNumChannels();++c){auto* ptr=buffer.getWritePointer(c);ptr[i]=juce::jlimit(-ceiling,ceiling,ptr[i]*limiterEnvelope);}}
}
void CamachoProcessor::setPreset(int index){
 static constexpr float presets[][8]={{32,0,0,0,0,-12,2,0},{38,2,-2,1,2,-14,2.2f,-2},{35,2,-1,1,1,-15,2.5f,-2},{40,1,-2,2,2,-14,2,-2},{45,-2,-2,3,2,-12,1.8f,-1}};
 if(index<0||index>=5)return;
 const char* ids[]={"sub","bass","lowmid","highmid","treble","threshold","ratio","output"};
 for(int i=0;i<8;++i)if(auto* p=apvts.getParameter(ids[i])){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(presets[index][i]));p->endChangeGesture();}
}
void CamachoProcessor::getStateInformation(juce::MemoryBlock& dest){auto state=apvts.copyState();std::unique_ptr<juce::XmlElement> xml(state.createXml());copyXmlToBinary(*xml,dest);}
void CamachoProcessor::setStateInformation(const void* data,int size){std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data,size));if(xml&&xml->hasTagName(apvts.state.getType()))apvts.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessorEditor* CamachoProcessor::createEditor(){return new CamachoEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new CamachoProcessor();}
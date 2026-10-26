#pragma once
#include <JuceHeader.h>
class CamachoProcessor : public juce::AudioProcessor {
public:
 CamachoProcessor();
 void prepareToPlay(double,int) override;
 void releaseResources() override {}
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override {return true;}
 const juce::String getName() const override {return "VPX PRO MASTERING PROCESSOR";}
 bool acceptsMidi() const override {return false;}
 bool producesMidi() const override {return false;}
 bool isMidiEffect() const override {return false;}
 double getTailLengthSeconds() const override {return 0;}
 int getNumPrograms() override {return 1;}
 int getCurrentProgram() override {return 0;}
 void setCurrentProgram(int) override {}
 const juce::String getProgramName(int) override {return {};}
 void changeProgramName(int,const juce::String&) override {}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;
 juce::AudioProcessorValueTreeState apvts;
 static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
 void setPreset(int);
private:
 using Filter=juce::dsp::IIR::Filter<float>;
 juce::dsp::ProcessorDuplicator<Filter,juce::dsp::IIR::Coefficients<float>> highpass,low,lowmid,highmid,high;
 juce::dsp::Compressor<float> compressor;
 double sampleRateHz=44100;
 float limiterEnvelope=1;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CamachoProcessor)
};
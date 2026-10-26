#pragma once
#include "PluginProcessor.h"
class CamachoEditor:public juce::AudioProcessorEditor {
public:
 explicit CamachoEditor(CamachoProcessor&);
 void resized() override;
 void paint(juce::Graphics&) override;
private:
 CamachoProcessor& processor;
 std::vector<std::unique_ptr<juce::Slider>> sliders;
 std::vector<std::unique_ptr<juce::Label>> labels;
 std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
 juce::ComboBox presets;
 juce::ToggleButton bypass{"BYPASS"};
 std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CamachoEditor)
};
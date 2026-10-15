#pragma once
#include "Processor.h"
#include "Theme.h"

namespace tonedock {
class AudioSettings : public juce::Component,private juce::ChangeListener,private juce::Timer {
public:
    AudioSettings(juce::AudioDeviceManager&,Processor&);
    ~AudioSettings() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void refresh();void apply();void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override{repaint();}
    juce::AudioDeviceManager& manager;Processor& processor;
    juce::ComboBox driver,outputDevice,inputDevice,inputRoute,outputRoute,sampleRate,buffer;
    juce::TextButton refreshButton{ui::text("刷新设备")},controlPanel{ui::text("驱动控制面板")};
    juce::String status;bool refreshing=false;
    juce::Array<double> rates;juce::Array<int> buffers;
};
}

#pragma once
#include <JuceHeader.h>
#include "LatencyProbe.h"
#include <array>
#include <atomic>

namespace tonedock {
// Standalone audio callback: routes one chosen interface input (the guitar) to both
// processor inputs, meters every input, tracks load and runs loopback latency tests.
class StandaloneAudio : public juce::AudioIODeviceCallback {
public:
    static constexpr int maxInputs=64;
    enum class Test{idle,requested,running,done,failed};

    void setProcessor(juce::AudioProcessor* p){processor.store(p);}
    void setInputChannel(int hardwareChannel);
    int inputChannel() const{return selected.load();}
    float takePeak(int hardwareChannel);
    void startLatencyTest();
    Test testState() const{return Test(test.load());}
    // Measured round-trip latency in samples for the current device settings, or -1.
    int measuredLatency() const;
    double sampleRate() const{return rate.load();}
    int bufferSize() const{return block.load();}

    void audioDeviceIOCallbackWithContext(const float* const* inputs,int numInputs,float* const* outputs,int numOutputs,
        int numSamples,const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

private:
    std::atomic<juce::AudioProcessor*> processor{nullptr};
    std::array<std::atomic<float>,maxInputs> peaks{};
    std::array<int,maxInputs> activeToHardware{};
    std::atomic<int> activeCount{0},selected{0},selectedSlot{0},test{int(Test::idle)},measured{-1},block{0};
    std::atomic<double> rate{0};
    double measuredRate=0;int measuredBlock=0;
    LatencyProbe probe;
    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer midi;
    int findSlot(int hardwareChannel) const;
};

// Picks the lowest-latency driver available: ASIO, then WASAPI exclusive, then the rest.
void chooseBestAudioDevice(juce::AudioDeviceManager&);
// Opens every input so any of them can be chosen as the guitar input; prefers 48 kHz and 128 samples once.
void configureInputs(juce::AudioDeviceManager&,bool preferLowLatency);
// One-line status for the main window's status bar.
juce::String describeAudio(juce::AudioDeviceManager&,const StandaloneAudio&);

class AudioSetupPanel : public juce::Component,private juce::ChangeListener,private juce::Timer {
public:
    AudioSetupPanel(juce::AudioDeviceManager&,StandaloneAudio&,bool firstRun);
    ~AudioSetupPanel() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    std::function<void()> onDone;
private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refresh();
    void apply(const std::function<void(juce::AudioDeviceManager::AudioDeviceSetup&)>&);
    void startDetection();
    juce::String latencyText() const;
    juce::AudioDeviceManager& devices;
    StandaloneAudio& audio;
    bool firstRun;
    juce::ComboBox driver,device,inputDevice,rate,channel;
    juce::TextButton detect{"Auto-detect"},controlPanel{"Driver panel"},measure{"Measure"},done;
    juce::OwnedArray<juce::TextButton> buffers;
    juce::Label message;
    juce::StringArray inputNames;
    std::array<float,StandaloneAudio::maxInputs> meters{},detectPeaks{};
    juce::uint32 detectUntil=0;
    StandaloneAudio::Test lastTest=StandaloneAudio::Test::idle;
    std::vector<std::pair<juce::String,int>> sections;
    juce::Rectangle<int> meterArea,noteArea;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSetupPanel)
};
}

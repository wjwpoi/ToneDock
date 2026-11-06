#include "AudioSetup.h"
#include "Theme.h"

namespace tonedock {
namespace {
bool isGenericAsio(const juce::String& name) {
    for(auto* generic:{"ASIO4ALL","FlexASIO","Generic Low Latency","Realtek"})if(name.containsIgnoreCase(generic))return true;
    return false;
}
juce::String friendlyType(const juce::String& type) {
    if(type=="Windows Audio (Exclusive Mode)")return "WASAPI exclusive";
    if(type=="Windows Audio (Low Latency Mode)")return "WASAPI low latency";
    if(type=="Windows Audio")return "WASAPI shared";
    return type;
}
juce::String driverLabel(const juce::String& type) {
    if(type=="ASIO")return "ASIO (recommended)";
    if(type=="Windows Audio (Exclusive Mode)")return "Windows exclusive mode (WASAPI)";
    if(type=="Windows Audio (Low Latency Mode)")return "Windows low latency mode (WASAPI)";
    if(type=="Windows Audio")return "Windows shared mode (adds latency)";
    return type;
}
juce::String milliseconds(int samples,double rate){return rate>0?juce::String(samples*1000.0/rate,1)+" ms":juce::String("-");}
int estimatedRoundTrip(juce::AudioIODevice& d) {
    auto reported=d.getInputLatencyInSamples()+d.getOutputLatencyInSamples();
    return reported>0?reported:2*d.getCurrentBufferSizeSamples();
}
}

void StandaloneAudio::setInputChannel(int hardwareChannel) {
    selected.store(juce::jlimit(0,maxInputs-1,hardwareChannel));selectedSlot.store(findSlot(selected.load()));
}
int StandaloneAudio::findSlot(int hardwareChannel) const {
    auto count=activeCount.load();
    for(int i=0;i<count;++i)if(activeToHardware[size_t(i)]==hardwareChannel)return i;
    return count>0?0:-1;
}
float StandaloneAudio::takePeak(int hardwareChannel) {
    return hardwareChannel>=0 && hardwareChannel<maxInputs?peaks[size_t(hardwareChannel)].exchange(0.0f):0.0f;
}
void StandaloneAudio::startLatencyTest() {
    auto state=test.load();
    if(state!=int(Test::requested) && state!=int(Test::running))test.store(int(Test::requested));
}
int StandaloneAudio::measuredLatency() const{return measured.load();}

void StandaloneAudio::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    auto sr=device->getCurrentSampleRate();auto bs=device->getCurrentBufferSizeSamples();
    if(sr!=rate.load() || bs!=block.load())measured.store(-1);
    if(test.load()!=int(Test::requested))test.store(int(Test::idle));
    probe.cancel();rate.store(sr);block.store(bs);
    auto active=device->getActiveInputChannels();int count=0;
    for(int i=0;i<=active.getHighestBit() && count<maxInputs;++i)if(active[i])activeToHardware[size_t(count++)]=i;
    activeCount.store(count);selectedSlot.store(findSlot(selected.load()));
    scratch.setSize(2,juce::jmax(bs,256));midi.ensureSize(256);
    if(auto* p=processor.load()){p->setPlayConfigDetails(2,2,sr,bs);p->prepareToPlay(sr,bs);}
}
void StandaloneAudio::audioDeviceStopped() {
    if(auto* p=processor.load())p->releaseResources();
}
void StandaloneAudio::audioDeviceIOCallbackWithContext(const float* const* inputs,int numInputs,float* const* outputs,int numOutputs,
    int numSamples,const juce::AudioIODeviceCallbackContext&) {
    juce::ScopedNoDenormals noDenormals;
    auto count=activeCount.load();
    for(int i=0;i<numInputs && i<maxInputs;++i)if(inputs[i]!=nullptr) {
        auto level=juce::FloatVectorOperations::findMaximum(inputs[i],numSamples);
        level=juce::jmax(level,-juce::FloatVectorOperations::findMinimum(inputs[i],numSamples));
        auto& peak=peaks[size_t(i<count?activeToHardware[size_t(i)]:i)];
        if(level>peak.load(std::memory_order_relaxed))peak.store(level,std::memory_order_relaxed);
    }
    auto slot=selectedSlot.load();
    const float* source=slot>=0 && slot<numInputs?inputs[slot]:nullptr;
    if(test.load()==int(Test::requested)){probe.start(rate.load());test.store(int(Test::running));}
    auto testing=test.load()==int(Test::running);
    auto* p=processor.load();
    for(int done=0;done<numSamples;) {
        auto chunk=juce::jmin(numSamples-done,scratch.getNumSamples());
        juce::AudioBuffer<float> view(scratch.getArrayOfWritePointers(),2,chunk);
        for(int c=0;c<2;++c){if(source!=nullptr)view.copyFrom(c,0,source+done,chunk);else view.clear(c,0,chunk);}
        if(testing) {
            for(int i=0;i<chunk;++i){auto s=probe.next(source!=nullptr?source[done+i]:0.0f);view.setSample(0,i,s);view.setSample(1,i,s);}
            if(!probe.isRunning()){auto result=probe.result();measured.store(result);test.store(int(result>=0?Test::done:Test::failed));testing=false;}
        } else if(p!=nullptr) {
            const juce::ScopedLock lock(p->getCallbackLock());
            if(p->isSuspended())view.clear();else{midi.clear();p->processBlock(view,midi);}
        } else view.clear();
        for(int o=0;o<numOutputs;++o)if(outputs[o]!=nullptr) {
            if(o<2)juce::FloatVectorOperations::copy(outputs[o]+done,view.getReadPointer(o),chunk);
            else juce::FloatVectorOperations::clear(outputs[o]+done,chunk);
        }
        done+=chunk;
    }
}

void chooseBestAudioDevice(juce::AudioDeviceManager& devices) {
    for(auto* preferred:{"ASIO","Windows Audio (Exclusive Mode)","Windows Audio (Low Latency Mode)","Windows Audio","CoreAudio","JACK","ALSA"})
        for(auto* type:devices.getAvailableDeviceTypes()) {
            if(type->getTypeName()!=preferred)continue;
            type->scanForDevices();auto outputs=type->getDeviceNames(false);
            int pick=type->getDefaultDeviceIndex(false);
            // Generic ASIO wrappers are no better than WASAPI exclusive mode and often worse.
            if(type->getTypeName()=="ASIO") {
                pick=-1;for(int i=0;i<outputs.size();++i)if(!isGenericAsio(outputs[i])){pick=i;break;}
                if(pick<0)continue;
            }
            if(outputs.isEmpty())continue;
            devices.setCurrentAudioDeviceType(type->getTypeName(),true);
            auto setup=devices.getAudioDeviceSetup();
            setup.outputDeviceName=outputs[juce::jlimit(0,outputs.size()-1,pick)];
            if(type->hasSeparateInputsAndOutputs()) {
                auto inputs=type->getDeviceNames(true);
                setup.inputDeviceName=inputs.isEmpty()?juce::String():inputs[juce::jlimit(0,inputs.size()-1,type->getDefaultDeviceIndex(true))];
            } else setup.inputDeviceName=setup.outputDeviceName;
            if(devices.setAudioDeviceSetup(setup,true).isEmpty() && devices.getCurrentAudioDevice()!=nullptr)return;
        }
}

void configureInputs(juce::AudioDeviceManager& devices,bool preferLowLatency) {
    auto* device=devices.getCurrentAudioDevice();if(device==nullptr)return;
    auto setup=devices.getAudioDeviceSetup();bool changed=false;
    auto count=juce::jmin(device->getInputChannelNames().size(),StandaloneAudio::maxInputs);
    juce::BigInteger all;if(count>0)all.setRange(0,count,true);
    if(setup.useDefaultInputChannels || setup.inputChannels!=all){setup.useDefaultInputChannels=false;setup.inputChannels=all;changed=true;}
    if(preferLowLatency) {
        if(device->getAvailableSampleRates().contains(48000.0) && setup.sampleRate!=48000.0){setup.sampleRate=48000.0;changed=true;}
        auto sizes=device->getAvailableBufferSizes();sizes.sort();int best=0;
        for(auto size:sizes)if(size>=128){best=size;break;}
        if(best==0 && !sizes.isEmpty())best=sizes.getLast();
        if(best>0 && best!=setup.bufferSize){setup.bufferSize=best;changed=true;}
    }
    if(changed)devices.setAudioDeviceSetup(setup,true);
}

juce::String describeAudio(juce::AudioDeviceManager& devices,const StandaloneAudio& audio) {
    auto* d=devices.getCurrentAudioDevice();
    if(d==nullptr)return "No audio device - open Audio to choose one";
    auto rate=d->getCurrentSampleRate();auto names=d->getInputChannelNames();auto input=audio.inputChannel();
    juce::StringArray parts{friendlyType(d->getTypeName())+" "+d->getName(),
        (input<names.size() && names[input].isNotEmpty()?names[input]:"Input "+juce::String(input+1)),
        juce::String(rate/1000.0,rate==44100.0?1:0)+" kHz / "+juce::String(d->getCurrentBufferSizeSamples())+" smp"};
    auto measured=audio.measuredLatency();
    parts.add(measured>=0?milliseconds(measured,rate)+" measured":"~"+milliseconds(estimatedRoundTrip(*d),rate)+" round trip");
    parts.add("DSP "+juce::String(juce::roundToInt(devices.getCpuUsage()*100))+"%");
    if(auto xruns=d->getXRunCount();xruns>=0)parts.add("Dropouts "+juce::String(xruns));
    return parts.joinIntoString("  |  ");
}

AudioSetupPanel::AudioSetupPanel(juce::AudioDeviceManager& d,StandaloneAudio& a,bool first):devices(d),audio(a),firstRun(first) {
    for(auto* c:std::initializer_list<juce::Component*>{&driver,&device,&inputDevice,&rate,&channel,&detect,&controlPanel,&measure,&done,&message})addAndMakeVisible(c);
    auto& types=devices.getAvailableDeviceTypes();
    for(int i=0;i<types.size();++i)driver.addItem(driverLabel(types[i]->getTypeName()),i+1);
    driver.onChange=[this] {
        auto& all=devices.getAvailableDeviceTypes();auto index=driver.getSelectedId()-1;
        if(index<0 || index>=all.size() || all[index]->getTypeName()==devices.getCurrentAudioDeviceType())return;
        devices.setCurrentAudioDeviceType(all[index]->getTypeName(),true);configureInputs(devices,true);refresh();
    };
    device.onChange=[this]{auto name=device.getText();auto* type=devices.getCurrentDeviceTypeObject();
        apply([&](auto& s){s.outputDeviceName=name;if(type==nullptr || !type->hasSeparateInputsAndOutputs())s.inputDeviceName=name;});};
    inputDevice.onChange=[this]{auto name=inputDevice.getText();apply([&](auto& s){s.inputDeviceName=name;});};
    rate.onChange=[this]{auto value=double(rate.getSelectedId());if(value>0)apply([&](auto& s){s.sampleRate=value;});};
    channel.onChange=[this]{if(channel.getSelectedId()>0)audio.setInputChannel(channel.getSelectedId()-1);};
    detect.onClick=[this]{startDetection();};
    controlPanel.onClick=[this] {
        if(auto* current=devices.getCurrentAudioDevice();current!=nullptr && current->hasControlPanel() && current->showControlPanel())
            {devices.closeAudioDevice();devices.restartLastAudioDevice();}
    };
    measure.onClick=[this]{audio.startLatencyTest();message.setText("Measuring... keep the loopback cable connected.",juce::dontSendNotification);};
    done.setButtonText(firstRun?"Start playing":"Done");
    done.setColour(juce::TextButton::buttonColourId,theme::accent);done.setColour(juce::TextButton::textColourOffId,juce::Colours::white);
    done.onClick=[this]{if(onDone)onDone();};
    message.setColour(juce::Label::textColourId,theme::body);message.setJustificationType(juce::Justification::topLeft);
    devices.addChangeListener(this);refresh();startTimerHz(30);
    if(firstRun && devices.getCurrentAudioDevice()!=nullptr)startDetection();
    setSize(760,780);
}
AudioSetupPanel::~AudioSetupPanel(){devices.removeChangeListener(this);stopTimer();}
void AudioSetupPanel::changeListenerCallback(juce::ChangeBroadcaster*){refresh();}
void AudioSetupPanel::apply(const std::function<void(juce::AudioDeviceManager::AudioDeviceSetup&)>& change) {
    auto setup=devices.getAudioDeviceSetup();change(setup);
    auto error=devices.setAudioDeviceSetup(setup,true);
    if(error.isNotEmpty())message.setText(error,juce::dontSendNotification);else configureInputs(devices,false);
    refresh();
}
void AudioSetupPanel::startDetection() {
    if(devices.getCurrentAudioDevice()==nullptr)return;
    detectPeaks.fill(0);detectUntil=juce::Time::getMillisecondCounter()+5000;
    message.setText("Play a few notes on your guitar. ToneDock is listening on every input.",juce::dontSendNotification);
}
void AudioSetupPanel::refresh() {
    auto* type=devices.getCurrentDeviceTypeObject();auto* d=devices.getCurrentAudioDevice();auto setup=devices.getAudioDeviceSetup();
    auto& types=devices.getAvailableDeviceTypes();
    for(int i=0;i<types.size();++i)if(types[i]==type)driver.setSelectedId(i+1,juce::dontSendNotification);
    device.clear(juce::dontSendNotification);inputDevice.clear(juce::dontSendNotification);
    auto separate=type!=nullptr && type->hasSeparateInputsAndOutputs();
    if(type!=nullptr) {
        device.addItemList(type->getDeviceNames(false),1);device.setText(setup.outputDeviceName,juce::dontSendNotification);
        if(separate){inputDevice.addItemList(type->getDeviceNames(true),1);inputDevice.setText(setup.inputDeviceName,juce::dontSendNotification);}
    }
    inputDevice.setVisible(separate);
    rate.clear(juce::dontSendNotification);channel.clear(juce::dontSendNotification);buffers.clear();
    inputNames=d!=nullptr?d->getInputChannelNames():juce::StringArray();
    if(inputNames.size()>StandaloneAudio::maxInputs)inputNames.removeRange(StandaloneAudio::maxInputs,inputNames.size());
    if(d!=nullptr) {
        for(auto r:d->getAvailableSampleRates())rate.addItem(juce::String(r/1000.0,r==44100.0 || r==88200.0?1:0)+" kHz",int(r));
        rate.setSelectedId(int(d->getCurrentSampleRate()),juce::dontSendNotification);
        for(int i=0;i<inputNames.size();++i)channel.addItem(inputNames[i].isNotEmpty()?inputNames[i]:"Input "+juce::String(i+1),i+1);
        channel.setSelectedId(audio.inputChannel()+1,juce::dontSendNotification);
        auto sizes=d->getAvailableBufferSizes();sizes.sort();auto current=d->getCurrentBufferSizeSamples();
        juce::Array<int> shown;
        for(auto s:sizes)if(s==current || (sizes.size()<=8 && s>=16 && s<=2048) || (sizes.size()>8 && juce::Array<int>{32,64,96,128,192,256,512,1024}.contains(s)))shown.add(s);
        for(auto s:shown) {
            auto* b=buffers.add(new juce::TextButton(juce::String(s)+"  /  "+milliseconds(s,d->getCurrentSampleRate())));
            b->setToggleState(s==current,juce::dontSendNotification);
            b->onClick=[this,s]{apply([s](auto& setupToChange){setupToChange.bufferSize=s;});};
            addAndMakeVisible(b);
        }
    }
    controlPanel.setVisible(d!=nullptr && d->hasControlPanel());
    for(auto* c:std::initializer_list<juce::Component*>{&detect,&measure,&channel,&rate})c->setEnabled(d!=nullptr);
    if(d==nullptr)message.setText("No audio device is open. Choose a driver and a device.",juce::dontSendNotification);
    resized();repaint();
}
juce::String AudioSetupPanel::latencyText() const {
    auto* d=devices.getCurrentAudioDevice();if(d==nullptr)return "-";
    auto rate=d->getCurrentSampleRate();auto measured=audio.measuredLatency();
    auto text="About "+milliseconds(estimatedRoundTrip(*d),rate)+" reported by the driver";
    if(measured>=0)text=milliseconds(measured,rate)+" measured round trip";
    return text;
}
void AudioSetupPanel::timerCallback() {
    for(int i=0;i<inputNames.size();++i) {
        auto peak=audio.takePeak(i);meters[size_t(i)]=juce::jmax(peak,meters[size_t(i)]*.85f);
        if(detectUntil!=0)detectPeaks[size_t(i)]=juce::jmax(detectPeaks[size_t(i)],peak);
    }
    if(detectUntil!=0 && juce::Time::getMillisecondCounter()>detectUntil) {
        detectUntil=0;int best=-1;float loudest=.01f;
        for(int i=0;i<inputNames.size();++i)if(detectPeaks[size_t(i)]>loudest){loudest=detectPeaks[size_t(i)];best=i;}
        if(best>=0){audio.setInputChannel(best);channel.setSelectedId(best+1,juce::dontSendNotification);
            message.setText("Guitar found on "+channel.getText()+".",juce::dontSendNotification);}
        else message.setText("No guitar signal found. Check the cable and the interface gain, then press Auto-detect.",juce::dontSendNotification);
    }
    auto state=audio.testState();
    if(state!=lastTest) {
        if(state==StandaloneAudio::Test::done)message.setText("Measured. Try a smaller buffer if it still feels slow.",juce::dontSendNotification);
        if(state==StandaloneAudio::Test::failed)message.setText("No consistent click returned. Connect Out 1 to the guitar input with a cable, set the input gain to about halfway, then try again.",juce::dontSendNotification);
        lastTest=state;
    }
    measure.setEnabled(devices.getCurrentAudioDevice()!=nullptr && state!=StandaloneAudio::Test::running && state!=StandaloneAudio::Test::requested);
    repaint();
}
void AudioSetupPanel::paint(juce::Graphics& g) {
    g.fillAll(theme::background);
    g.setColour(theme::ink);g.setFont(juce::FontOptions(24.0f,juce::Font::bold));
    g.drawText(firstRun?"Welcome to ToneDock":"Audio setup",32,24,getWidth()-64,32,juce::Justification::centredLeft);
    g.setColour(theme::muted);g.setFont(juce::FontOptions(14.0f));
    auto subtitle=devices.getCurrentAudioDevice()==nullptr?"No audio interface is open yet. Connect it, then choose its driver below."
        :firstRun?"We picked the fastest driver we found. Check your guitar input, then start playing.":"Driver, guitar input, buffer and latency.";
    g.drawText(subtitle,
        32,58,getWidth()-64,22,juce::Justification::centredLeft);
    for(auto& [label,y]:sections){g.setColour(theme::body);g.setFont(juce::FontOptions(13.0f,juce::Font::bold));g.drawText(label,32,y,140,32,juce::Justification::centredLeft);}
    for(int i=0;i<juce::jmin(inputNames.size(),8);++i) {
        auto row=meterArea.withHeight(18).translated(0,i*22);auto selected=i==audio.inputChannel();
        g.setColour(selected?theme::ink:theme::muted);g.setFont(juce::FontOptions(12.0f,selected?juce::Font::bold:juce::Font::plain));
        g.drawText(inputNames[i].isNotEmpty()?inputNames[i]:"Input "+juce::String(i+1),row.removeFromLeft(130),juce::Justification::centredLeft);
        auto bar=row.reduced(0,5).toFloat();g.setColour(theme::track);g.fillRoundedRectangle(bar,3);
        auto level=juce::jlimit(0.0f,1.0f,meters[size_t(i)]);
        auto fill=juce::Decibels::gainToDecibels(level,-60.0f)/60.0f+1.0f;
        g.setColour(level>=.98f?theme::warn:selected?theme::accent:theme::accentLine);g.fillRoundedRectangle(bar.withWidth(bar.getWidth()*fill),3);
    }
    if(!sections.empty()) {
        auto latencyY=sections.back().second;g.setColour(theme::ink);g.setFont(juce::FontOptions(15.0f));
        g.drawText(latencyText(),172,latencyY,getWidth()-172-170,32,juce::Justification::centredLeft);
        g.setColour(theme::muted);g.setFont(juce::FontOptions(12.0f));
        g.drawText("To measure: connect Out 1 to the guitar input with a cable and turn your speakers down.",172,latencyY+34,getWidth()-204,20,juce::Justification::centredLeft);
    }
    if(buffers.size()==1){g.setColour(theme::muted);g.setFont(juce::FontOptions(12.0f));
        g.drawText("This driver sets its buffer in its own control panel.",buffers[0]->getRight()+12,buffers[0]->getY(),300,32,juce::Justification::centredLeft);}
    if(!noteArea.isEmpty()) {
        g.setColour(theme::card);g.fillRoundedRectangle(noteArea.toFloat(),8);
        auto type=devices.getCurrentAudioDeviceType();
        auto note=type=="Windows Audio (Exclusive Mode)"?"No ASIO driver needed. Exclusive mode gets close to ASIO latency, but other apps cannot play sound while ToneDock uses this device."
            :type.startsWith("Windows Audio")?"Shared mode adds 20-40 ms. Choose ASIO, or Windows exclusive mode if your interface has no ASIO driver."
            :"Install your interface's ASIO driver for the lowest latency.";
        g.setColour(theme::body);g.setFont(juce::FontOptions(13.0f));g.drawFittedText(note,noteArea.reduced(14,8),juce::Justification::centredLeft,3);
    }
}
void AudioSetupPanel::resized() {
    sections.clear();int x=172,w=getWidth()-x-32,y=100;
    auto row=[&](const juce::String& label){sections.push_back({label,y});};
    row("Driver");driver.setBounds(x,y,juce::jmin(w,340),32);y+=46;
    row(inputDevice.isVisible()?"Output":"Device");device.setBounds(x,y,w,32);y+=46;
    if(inputDevice.isVisible()){row("Input device");inputDevice.setBounds(x,y,w,32);y+=46;}
    row("Sample rate");rate.setBounds(x,y,160,32);y+=46;
    row("Guitar input");channel.setBounds(x,y,240,32);detect.setBounds(x+252,y,120,32);y+=42;
    meterArea={x,y,w,juce::jmin(inputNames.size(),8)*22};y+=meterArea.getHeight()+14;
    row("Buffer");int bx=x;
    for(auto* b:buffers){if(bx+124>x+w){bx=x;y+=40;}b->setBounds(bx,y,120,32);bx+=128;}
    y+=42;if(controlPanel.isVisible()){controlPanel.setBounds(x,y,140,32);y+=44;}
    row("Latency");measure.setBounds(getWidth()-32-130,y,130,32);y+=66;
    auto type=devices.getCurrentAudioDeviceType();
    noteArea=type=="ASIO" || devices.getCurrentAudioDevice()==nullptr?juce::Rectangle<int>():juce::Rectangle<int>(32,y,getWidth()-64,58);
    if(!noteArea.isEmpty())y+=72;
    message.setBounds(32,y,getWidth()-64,44);
    done.setBounds(getWidth()-32-170,getHeight()-28-40,170,40);
}
}

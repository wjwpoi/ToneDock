#include "AudioSettings.h"

namespace tonedock {
AudioSettings::AudioSettings(juce::AudioDeviceManager& d,Processor& p):manager(d),processor(p) {
    for(auto* c:std::initializer_list<juce::Component*>{&driver,&outputDevice,&inputDevice,&inputRoute,&outputRoute,&sampleRate,&buffer,&refreshButton,&controlPanel})addAndMakeVisible(c);
    driver.setComponentID("audio-driver");outputDevice.setComponentID("audio-output-device");inputDevice.setComponentID("audio-input-device");inputRoute.setComponentID("audio-input-channel");outputRoute.setComponentID("audio-output-channels");sampleRate.setComponentID("audio-sample-rate");buffer.setComponentID("audio-buffer");
    driver.onChange=[this]{if(refreshing)return;manager.setCurrentAudioDeviceType(driver.getText(),true);status.clear();refresh();};
    for(auto* combo:{&outputDevice,&inputDevice,&inputRoute,&outputRoute,&sampleRate,&buffer})combo->onChange=[this]{if(!refreshing)apply();};
    refreshButton.onClick=[this]{for(auto* t:manager.getAvailableDeviceTypes())t->scanForDevices();refresh();};
    controlPanel.onClick=[this]{if(auto* device=manager.getCurrentAudioDevice()){device->showControlPanel();manager.closeAudioDevice();manager.restartLastAudioDevice();refresh();}};
    manager.addChangeListener(this);refresh();startTimerHz(15);
}
AudioSettings::~AudioSettings(){stopTimer();manager.removeChangeListener(this);}
void AudioSettings::changeListenerCallback(juce::ChangeBroadcaster*){if(!refreshing)refresh();}
void AudioSettings::refresh() {
    juce::ScopedValueSetter<bool> guard(refreshing,true);
    driver.clear(juce::dontSendNotification);for(auto* t:manager.getAvailableDeviceTypes()){driver.addItem(t->getTypeName(),driver.getNumItems()+1);if(t->getTypeName()==manager.getCurrentAudioDeviceType())driver.setSelectedId(driver.getNumItems(),juce::dontSendNotification);}
    auto setup=manager.getAudioDeviceSetup();auto* type=manager.getCurrentDeviceTypeObject();
    auto names=[&](juce::ComboBox& combo,bool input){combo.clear(juce::dontSendNotification);if(!type)return;auto list=type->getDeviceNames(input);for(int i=0;i<list.size();++i)combo.addItem(list[i],i+1);combo.setSelectedId(list.indexOf(input?setup.inputDeviceName:setup.outputDeviceName)+1,juce::dontSendNotification);};
    names(outputDevice,false);names(inputDevice,true);inputDevice.setEnabled(type && type->hasSeparateInputsAndOutputs());
    inputRoute.clear(juce::dontSendNotification);outputRoute.clear(juce::dontSendNotification);sampleRate.clear(juce::dontSendNotification);buffer.clear(juce::dontSendNotification);rates.clear();buffers.clear();
    auto* device=manager.getCurrentAudioDevice();
    if(device){
        inputRoute.addItem(ui::text("关闭输入"),1);auto ins=device->getInputChannelNames();
        for(int i=0;i<ins.size();++i)inputRoute.addItem(juce::String(i+1)+" · "+ins[i]+"  "+ui::text("单声道"),i+2);
        auto activeIn=device->getActiveInputChannels();int inCount=activeIn.countNumberOfSetBits();
        if(inCount<=1)inputRoute.setSelectedId(inCount==0?1:activeIn.findNextSetBit(0)+2,juce::dontSendNotification);
        else {inputRoute.addItem(ui::text("自定义多通道输入"),10000);inputRoute.setSelectedId(10000,juce::dontSendNotification);}
        auto outs=device->getOutputChannelNames();for(int i=0;i+1<outs.size();i+=2)outputRoute.addItem(juce::String(i+1)+" / "+juce::String(i+2)+" · "+outs[i]+" + "+outs[i+1],i+1);
        auto activeOut=device->getActiveOutputChannels();auto first=activeOut.findNextSetBit(0);
        if(activeOut.countNumberOfSetBits()==2 && first%2==0 && activeOut[first+1])outputRoute.setSelectedId(first+1,juce::dontSendNotification);
        else{outputRoute.addItem(ui::text("自定义输出通道"),10000);outputRoute.setSelectedId(10000,juce::dontSendNotification);}
        rates=device->getAvailableSampleRates();if(!rates.contains(device->getCurrentSampleRate()))rates.add(device->getCurrentSampleRate());
        for(int i=0;i<rates.size();++i){sampleRate.addItem(juce::String(rates[i]/1000.0,1)+" kHz",i+1);if(rates[i]==device->getCurrentSampleRate())sampleRate.setSelectedId(i+1,juce::dontSendNotification);}
        buffers=device->getAvailableBufferSizes();if(!buffers.contains(device->getCurrentBufferSizeSamples()))buffers.add(device->getCurrentBufferSizeSamples());
        for(int i=0;i<buffers.size();++i){buffer.addItem(juce::String(buffers[i])+ui::text(" 采样点")+"  /  "+juce::String(1000.0*buffers[i]/device->getCurrentSampleRate(),2)+" ms",i+1);if(buffers[i]==device->getCurrentBufferSizeSamples())buffer.setSelectedId(i+1,juce::dontSendNotification);}
    }
    for(auto* c:{&inputRoute,&outputRoute,&sampleRate,&buffer})c->setEnabled(device!=nullptr);
    controlPanel.setEnabled(device && device->hasControlPanel());repaint();
}
void AudioSettings::apply() {
    auto previous=manager.getAudioDeviceSetup(),next=previous;auto* type=manager.getCurrentDeviceTypeObject();
    if(outputDevice.getSelectedId()>0)next.outputDeviceName=outputDevice.getText();
    if(type && !type->hasSeparateInputsAndOutputs())next.inputDeviceName=next.outputDeviceName;
    else if(inputDevice.getSelectedId()>0)next.inputDeviceName=inputDevice.getText();
    if(inputRoute.getSelectedId()>0 && inputRoute.getSelectedId()!=10000){next.useDefaultInputChannels=false;next.inputChannels.clear();if(inputRoute.getSelectedId()>1)next.inputChannels.setBit(inputRoute.getSelectedId()-2);}
    if(outputRoute.getSelectedId()>0 && outputRoute.getSelectedId()!=10000){next.useDefaultOutputChannels=false;next.outputChannels.clear();auto channel=outputRoute.getSelectedId()-1;next.outputChannels.setBit(channel);next.outputChannels.setBit(channel+1);}
    auto r=sampleRate.getSelectedId()-1,b=buffer.getSelectedId()-1;if(r>=0 && r<rates.size())next.sampleRate=rates[r];if(b>=0 && b<buffers.size())next.bufferSize=buffers[b];
    // A new interface has different channel counts/rates; let its driver choose safe defaults.
    if(next.inputDeviceName!=previous.inputDeviceName || next.outputDeviceName!=previous.outputDeviceName){next.sampleRate=0;next.bufferSize=0;next.useDefaultInputChannels=true;next.useDefaultOutputChannels=true;}
    auto error=manager.setAudioDeviceSetup(next,true);
    if(error.isNotEmpty()){auto restored=manager.setAudioDeviceSetup(previous,true);status=ui::text("设置失败：")+error;if(restored.isNotEmpty())status+=" / "+restored;}
    else status=ui::text("设置已生效");refresh();
}
void AudioSettings::resized() {
    int left=juce::jmax(560,getWidth()*58/100),cw=left-186;
    int row=0;for(auto* c:{&driver,&outputDevice,&inputDevice,&inputRoute,&outputRoute,&sampleRate,&buffer})c->setBounds(160,132+56*row++,cw,36);
    refreshButton.setBounds(22,getHeight()-57,130,34);controlPanel.setBounds(164,getHeight()-57,166,34);
}
void AudioSettings::paint(juce::Graphics& g) {
    using namespace ui;int w=getWidth(),h=getHeight(),left=juce::jmax(560,w*58/100),right=left+20,rw=w-right;
    g.fillAll(background);g.setColour(ink);g.setFont(font(26));g.drawText(text("音频设备"),0,0,w,38,juce::Justification::centredLeft);
    g.setColour(muted);g.setFont(font(14));g.drawText(text("选择接口与通道，声音会实时应用到当前效果链。"),0,43,w,25,juce::Justification::centredLeft);
    card(g,{0,88,left,h-158});card(g,{right,88,rw,h-158});
    g.setColour(ink);g.setFont(font(17));g.drawText(text("连接与路由"),22,98,left-44,26,juce::Justification::centredLeft);g.drawText(text("实时状态"),right+24,98,rw-48,26,juce::Justification::centredLeft);
    int row=0;for(auto label:{"驱动类型","输出设备","输入设备","乐器输入","耳机 / 音箱输出","采样率","缓冲大小"}){g.setColour(muted);g.setFont(font(14));g.drawText(text(label),22,132+56*row++,134,36,juce::Justification::centredLeft);}
    auto* device=manager.getCurrentAudioDevice();g.setColour(device?accent:muted);g.setFont(font(22));g.drawFittedText(device?device->getName():text("尚未打开设备"),right+24,143,rw-48,62,juce::Justification::topLeft,2);
    g.setColour(muted);g.setFont(font(14));g.drawText(text("输入电平"),right+24,225,rw-48,24,juce::Justification::centredLeft);meter(g,{right+24,263,rw-48,8},processor.inputPeak.load());
    g.setColour(muted);g.drawText(text("输出电平"),right+24,290,rw-48,24,juce::Justification::centredLeft);meter(g,{right+24,328,rw-48,8},processor.outputPeak.load());
    auto value=[&](const juce::String& name,const juce::String& v,int y){g.setColour(muted);g.setFont(font(14));g.drawText(name,right+24,y,rw/2,30,juce::Justification::centredLeft);g.setColour(ink);g.drawText(v,right+rw/2,y,rw/2-24,30,juce::Justification::centredRight);};
    value(text("音频处理负载"),juce::String(manager.getCpuUsage()*100,1)+" %",365);
    if(device && device->getCurrentSampleRate()>0){double rate=device->getCurrentSampleRate();value(text("驱动报告延迟"),juce::String(1000.0*(device->getInputLatencyInSamples()+device->getOutputLatencyInSamples())/rate,2)+" ms",403);}
    value(text("效果链延迟"),juce::String(processor.engine.latency())+text(" 采样点"),441);
    g.setColour(muted);g.setFont(font(12));g.drawFittedText(text("电平为效果链输入 / 输出。\n驱动延迟不代表实测往返延迟。"),right+24,489,rw-48,48,juce::Justification::topLeft,3);
    g.setColour(status.startsWith(text("设置失败"))?juce::Colour(0xffffa477):muted);g.setFont(font(13));g.drawFittedText(status,350,h-61,w-350,42,juce::Justification::centredLeft,2);
}
}

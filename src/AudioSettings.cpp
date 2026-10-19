#include "AudioSettings.h"

namespace tonedock {
AudioSettings::AudioSettings(juce::AudioDeviceManager& d,Processor& p):manager(d),processor(p) {
    for(auto* c:std::initializer_list<juce::Component*>{&driver,&outputDevice,&inputDevice,&inputRoute,&outputRoute,&sampleRate,&buffer,&refreshButton,&controlPanel})addAndMakeVisible(c);
    for(auto* combo:{&driver,&outputDevice,&inputDevice,&inputRoute,&outputRoute,&sampleRate,&buffer}){
        combo->setColour(juce::ComboBox::backgroundColourId,ui::panel);
        combo->setColour(juce::ComboBox::outlineColourId,ui::line);
    }
    refreshButton.setColour(juce::TextButton::buttonColourId,ui::selected);
    refreshButton.setColour(juce::TextButton::textColourOffId,ui::accent);
    controlPanel.setColour(juce::TextButton::buttonColourId,ui::panel);
    controlPanel.setColour(juce::TextButton::textColourOffId,ui::accent);
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
        for(int i=0;i<ins.size();++i)inputRoute.addItem(juce::String(i+1)+ui::text(" · ")+ins[i]+"  "+ui::text("单声道"),i+2);
        auto activeIn=device->getActiveInputChannels();int inCount=activeIn.countNumberOfSetBits();
        if(inCount<=1)inputRoute.setSelectedId(inCount==0?1:activeIn.findNextSetBit(0)+2,juce::dontSendNotification);
        else {inputRoute.addItem(ui::text("自定义多通道输入"),10000);inputRoute.setSelectedId(10000,juce::dontSendNotification);}
        auto outs=device->getOutputChannelNames();for(int i=0;i+1<outs.size();i+=2)outputRoute.addItem(juce::String(i+1)+" / "+juce::String(i+2)+ui::text(" · ")+outs[i]+" + "+outs[i+1],i+1);
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
    const int rightWidth=juce::jlimit(300,340,getWidth()*28/100),leftWidth=getWidth()-rightWidth-20;
    const int contentHeight=juce::jmax(492,getHeight()-148),extra=contentHeight-492;
    connectionBounds={0,80,leftWidth,200+extra/3};
    routingBounds={0,connectionBounds.getBottom()+16,leftWidth,132+extra/3};
    performanceBounds={0,routingBounds.getBottom()+16,leftWidth,128+extra-extra/3*2};
    liveBounds={leftWidth+20,80,rightWidth,contentHeight};
    const int fieldWidth=(leftWidth-64)/2;
    driver.setBounds(24,connectionBounds.getY()+74,leftWidth-48,36);
    outputDevice.setBounds(24,connectionBounds.getBottom()-54,fieldWidth,36);
    inputDevice.setBounds(40+fieldWidth,connectionBounds.getBottom()-54,fieldWidth,36);
    inputRoute.setBounds(24,routingBounds.getY()+80,fieldWidth,36);
    outputRoute.setBounds(40+fieldWidth,routingBounds.getY()+80,fieldWidth,36);
    sampleRate.setBounds(24,performanceBounds.getY()+76,fieldWidth,36);
    buffer.setBounds(40+fieldWidth,performanceBounds.getY()+76,fieldWidth,36);
    refreshButton.setBounds(0,getHeight()-44,132,36);controlPanel.setBounds(144,getHeight()-44,164,36);
}
void AudioSettings::paint(juce::Graphics& g) {
    using namespace ui;
    g.fillAll(background);g.setColour(ink);g.setFont(font(26));g.drawText(text("音频设备"),0,0,getWidth(),34,juce::Justification::centredLeft);
    g.setColour(muted);g.setFont(font(14));g.drawText(text("连接声卡，选择乐器输入与耳机 / 音箱输出。"),0,39,getWidth(),24,juce::Justification::centredLeft);
    auto section=[&](juce::Rectangle<int> bounds,const juce::String& title,int symbol){
        card(g,bounds);auto icon=juce::Rectangle<float>(float(bounds.getX()+20),float(bounds.getY()+14),32.f,32.f);
        g.setColour(selected);g.fillRoundedRectangle(icon,10);g.setColour(accent);
        float x=icon.getX(),y=icon.getY();
        if(symbol==0){
            g.drawRoundedRectangle(x+10,y+11,12,11,3,1.8f);g.drawLine(x+13,y+6,x+13,y+11,1.8f);g.drawLine(x+19,y+6,x+19,y+11,1.8f);g.drawLine(x+16,y+22,x+16,y+27,1.8f);
        }else if(symbol==1){
            g.drawLine(x+8,y+11,x+24,y+11,1.8f);g.drawLine(x+20,y+7,x+24,y+11,1.8f);g.drawLine(x+20,y+15,x+24,y+11,1.8f);
            g.drawLine(x+8,y+22,x+24,y+22,1.8f);g.drawLine(x+12,y+18,x+8,y+22,1.8f);g.drawLine(x+12,y+26,x+8,y+22,1.8f);
        }else{
            g.fillRoundedRectangle(x+8,y+18,3,7,1.5f);g.fillRoundedRectangle(x+14.5f,y+12,3,13,1.5f);g.fillRoundedRectangle(x+21,y+7,3,18,1.5f);
        }
        g.setColour(ink);g.setFont(font(16));g.drawText(title,bounds.getX()+64,bounds.getY()+16,bounds.getWidth()-88,26,juce::Justification::centredLeft);
    };
    section(connectionBounds,text("连接设备"),0);section(routingBounds,text("输入输出路由"),1);section(performanceBounds,text("音频性能"),2);
    auto label=[&](const char* name,const juce::Component& field){g.setColour(muted);g.setFont(font(13));g.drawText(text(name),field.getX(),field.getY()-23,field.getWidth(),18,juce::Justification::centredLeft);};
    label("驱动类型",driver);label("输出设备",outputDevice);label("输入设备",inputDevice);label("乐器输入",inputRoute);label("耳机 / 音箱输出",outputRoute);label("采样率",sampleRate);label("缓冲大小",buffer);

    card(g,liveBounds);const int x=liveBounds.getX(),y=liveBounds.getY(),width=liveBounds.getWidth();auto* device=manager.getCurrentAudioDevice();
    g.setColour(selected);g.fillRoundedRectangle(float(x+24),float(y+24),40.f,40.f,13.f);
    juce::Path headphones;headphones.startNewSubPath(float(x+34),float(y+44));headphones.cubicTo(float(x+34),float(y+30),float(x+54),float(y+30),float(x+54),float(y+44));
    g.setColour(accent);g.strokePath(headphones,juce::PathStrokeType(2.f));g.fillRoundedRectangle(float(x+33),float(y+42),4.f,10.f,2.f);g.fillRoundedRectangle(float(x+51),float(y+42),4.f,10.f,2.f);
    g.setColour(ink);g.setFont(font(16));g.drawText(text("实时状态"),x+76,y+23,width-100,24,juce::Justification::centredLeft);
    g.setColour(device?success:muted);g.fillEllipse(float(x+76),float(y+54),6.f,6.f);g.setFont(font(13));g.drawText(device?text("设备已连接"):text("等待连接设备"),x+88,y+47,width-112,20,juce::Justification::centredLeft);
    g.setColour(ink);g.setFont(font(18));g.drawFittedText(device?device->getName():text("尚未打开设备"),x+24,y+84,width-48,48,juce::Justification::topLeft,2);
    g.setColour(line);g.drawHorizontalLine(y+146,float(x+24),float(x+width-24));
    auto level=[&](const char* name,float peak,int top){
        g.setColour(muted);g.setFont(font(13));g.drawText(text(name),x+24,top,width-100,22,juce::Justification::centredLeft);
        g.setColour(peak>=.99f?error:muted);g.setFont(font(13));auto decibels=juce::Decibels::gainToDecibels(peak,-60.f);
        g.drawText(juce::String(decibels,1)+" dB",x+width-100,top,76,22,juce::Justification::centredRight);meter(g,{x+24,top+31,width-48,7},peak);
    };
    level("输入电平",processor.inputPeak.load(),y+164);level("输出电平",processor.outputPeak.load(),y+230);
    auto value=[&](const juce::String& name,const juce::String& v,int top){
        g.setColour(muted);g.setFont(font(13));g.drawText(name,x+24,top,146,28,juce::Justification::centredLeft);
        g.setColour(ink);g.drawText(v,x+170,top,width-194,28,juce::Justification::centredRight);
    };
    value(text("音频处理负载"),juce::String(manager.getCpuUsage()*100,1)+" %",y+292);
    juce::String latency=text("未连接");if(device && device->getCurrentSampleRate()>0)latency=juce::String(1000.0*(device->getInputLatencyInSamples()+device->getOutputLatencyInSamples())/device->getCurrentSampleRate(),2)+" ms";
    value(text("驱动报告延迟"),latency,y+326);value(text("效果链延迟"),juce::String(processor.engine.latency())+text(" 采样点"),y+360);
    auto note=liveBounds.reduced(20).withTop(liveBounds.getBottom()-84).withHeight(64);g.setColour(raised);g.fillRoundedRectangle(note.toFloat(),12);
    g.setColour(muted);g.setFont(font(13));g.drawFittedText(text("电平为效果链输入 / 输出。\n驱动延迟不代表实测往返延迟。"),note.reduced(12),juce::Justification::centredLeft,3);
    const bool failed=status.startsWith(text("设置失败"));g.setColour(failed?error:status.isEmpty()?muted:success);g.setFont(font(13));
    g.drawFittedText(status.isEmpty()?text("更改会立即应用"):status,328,getHeight()-47,getWidth()-328,40,juce::Justification::centredLeft,2);
}
}

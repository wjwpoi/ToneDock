#include "Processor.h"
#include "Editor.h"
#include <cmath>

namespace tonedock {
Services::Services() {
    auto settings=juce::JSON::parse(library.directory().getChildFile("settings.json"));
    tone3000.configurePublishableKey(settings.getProperty("publishableKey","").toString());
}
bool Services::configureKey(const juce::String& key) {
    if(!tone3000.configurePublishableKey(key.trim()))return false;
    auto* o=new juce::DynamicObject;o->setProperty("publishableKey",key.trim());
    return library.directory().getChildFile("settings.json").replaceWithText(juce::JSON::toString(juce::var(o)));
}
juce::AudioProcessorValueTreeState::ParameterLayout Processor::makeParameters() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"input",1},ui::text("Input gain"),-36.0f,24.0f,0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"output",1},ui::text("Output gain"),-60.0f,12.0f,-12.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},ui::text("Bypass"),false));
    return {p.begin(),p.end()};
}
Processor::Processor() : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
    .withOutput("Output",juce::AudioChannelSet::stereo(),true)),parameters(*this,nullptr,"ToneDock",makeParameters()) {
    inputParameter=parameters.getRawParameterValue("input");outputParameter=parameters.getRawParameterValue("output");bypassParameter=parameters.getRawParameterValue("bypass");
    for(auto& type:juce::StringArray{"Gate","NAM","Cabinet IR","EQ"})addEffect(type);
    onlineSource.resolve=[this](int id,t3k::Callback<t3k::ToneWithModels> done){services->tone3000.getToneWithAllModels(id,std::move(done));};
    onlineSource.download=[this](const t3k::Model& m,t3k::Callback<juce::File> done){services->tone3000.downloadModelFile(m,std::move(done));};
    startTimerHz(10);
}
Processor::~Processor() { stopTimer();imports.removeAllJobs(true,-1); }
void Processor::timerCallback(){if(getLatencySamples()!=engine.latency())setLatencySamples(engine.latency());}
bool Processor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()
        && (b.getMainInputChannelSet()==juce::AudioChannelSet::mono() || b.getMainInputChannelSet()==juce::AudioChannelSet::stereo());
}
void Processor::prepareToPlay(double sr,int) {
    inGain.reset(sr,.02);outGain.reset(sr,.02);
    inGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(inputParameter->load()));
    outGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(outputParameter->load()));
    engine.prepare(sr);
}
void Processor::processBlock(juce::AudioBuffer<float>& audio,juce::MidiBuffer&) {
    juce::ScopedNoDenormals denormals;
    for(int c=getTotalNumInputChannels();c<audio.getNumChannels();++c)audio.clear(c,0,audio.getNumSamples());
    if(audio.getNumChannels()<2)return;
    if(getTotalNumInputChannels()==1)audio.copyFrom(1,0,audio,0,0,audio.getNumSamples());
    inputPeak.store(juce::jmax(audio.getMagnitude(0,0,audio.getNumSamples()),audio.getMagnitude(1,0,audio.getNumSamples())));
    if(bypassParameter->load()>=.5f){outputPeak.store(inputPeak.load());return;}
    inGain.setTargetValue(juce::Decibels::decibelsToGain(inputParameter->load()));
    outGain.setTargetValue(juce::Decibels::decibelsToGain(outputParameter->load()));
    for(int i=0;i<audio.getNumSamples();++i) {
        auto gain=inGain.getNextValue();for(int c=0;c<2;++c)audio.setSample(c,i,std::isfinite(audio.getSample(c,i))?audio.getSample(c,i)*gain:0.0f);
    }
    engine.process(audio);
    for(int i=0;i<audio.getNumSamples();++i) {
        auto gain=outGain.getNextValue();for(int c=0;c<2;++c) {
            auto x=audio.getSample(c,i)*gain;audio.setSample(c,i,std::isfinite(x)?juce::jlimit(-1.0f,1.0f,x):0.0f);
        }
    }
    outputPeak.store(juce::jmax(audio.getMagnitude(0,0,audio.getNumSamples()),audio.getMagnitude(1,0,audio.getNumSamples())));
}
void Processor::setMessage(juce::String value) { std::lock_guard lock(messageMutex);notification=ui::message(value); }
juce::String Processor::message() const { std::lock_guard lock(messageMutex);return notification; }
void Processor::addEffect(const juce::String& type) {
    auto chain=engine.chain();if(chain.size()>=16){setMessage("Maximum 16 effects");return;}
    StageSpec s;s.id=juce::Uuid().toString();s.type=type;s.title=type;
    if(type=="Reverb"){s.controls->values[2]=.2f;}if(type=="Delay"){s.controls->values[1]=.3f;s.controls->values[2]=.25f;}
    chain.push_back(std::move(s));engine.setChain(std::move(chain));
}
void Processor::loadModel(const ModelEntry& entry,const juce::String& targetStageId) {
    auto chain=engine.chain();auto type=entry.kind=="ir"?juce::String("Cabinet IR"):juce::String("NAM");
    auto it=std::find_if(chain.begin(),chain.end(),[&](const auto& s){return s.id==targetStageId && s.type==type;});
    if(it==chain.end())it=std::find_if(chain.begin(),chain.end(),[&](const auto& s){return s.type==type;});
    if(it==chain.end()) {
        if(chain.size()>=16){setMessage("Remove an effect before adding this model");return;}
        StageSpec spec;spec.id=juce::Uuid().toString();spec.type=type;chain.push_back(spec);it=std::prev(chain.end());
    }
    it->file=services->library.fileFor(entry).getFullPathName();it->title=entry.name;
    engine.setChain(std::move(chain));setMessage("Model selected: "+entry.name);
}
void Processor::importFiles(juce::StringArray files) {
    setMessage("Importing and validating models...");
    imports.addJob([this,files=std::move(files)] {
        int count=0;juce::String failures;
        for(const auto& path:files)try {
            juce::File file(path);
            if(file.isDirectory()) {
                auto children=file.findChildFiles(juce::File::findFiles,true,"*.nam;*.wav;*.zip");
                if(children.size()>256)throw std::runtime_error("Folder exceeds 256 files per import");
                for(auto& child:children) {
                    if(child.hasFileExtension("zip"))count+=int(services->library.importArchive(child).size());
                    else{services->library.importFile(child);++count;}
                }
            } else if(file.hasFileExtension("zip"))count+=int(services->library.importArchive(file).size());
            else{services->library.importFile(file);++count;}
        }catch(const std::exception& e){failures+=ui::message(e.what())+"; ";}
        ++libraryVersion;setMessage("Imported / found "+juce::String(count)+" model(s)"+(failures.isNotEmpty()?". "+failures:juce::String()));
    });
}
void Processor::importDownloaded(juce::File file,juce::var provenance) {
    imports.addJob([this,file,provenance] {
        try{auto entry=services->library.importFile(file,provenance);++libraryVersion;setMessage("Downloaded to library: "+entry.name);}
        catch(const std::exception& e){file.deleteFile();setMessage("Download import failed: "+juce::String(e.what()));}
    });
}
void Processor::downloadModel(t3k::Model model,juce::var provenance,std::function<void(juce::String)> completion) {
    setMessage("Downloading "+model.name);
    services->tone3000.downloadModelFile(model,downloadCallbacks.wrap([this,provenance,completion](t3k::Result<juce::File> result){
        juce::String status;
        if(result.ok()){importDownloaded(*result,provenance);status="Download complete; validating and adding to library.";}
        else{status=ui::networkError(result.error.status,result.error.message);setMessage(status);}
        if(completion)completion(status);
    }));
}
void Processor::openToneLink(juce::String link) {
    if(auto id=parseToneLink(link)){if(services->tone3000.isConnected())loadOnlineTone(*id);else if(onOpenTone)onOpenTone(*id);else setMessage("Open the ToneDock editor to load this TONE3000 link");}
    else setMessage("Use a TONE3000 tone page or tonedock://tone/<numeric-id>");
}
void Processor::loadOnlineTone(int toneId,const juce::String& target,int modelId,OnlineCompletion completion) {
    auto failEarly=[&](juce::String s){setMessage(s);if(completion)completion(false,std::move(s));};
    auto initial=engine.chain();
    if(toneId<=0){failEarly(ui::text("无效的音色编号"));return;}
    if(target.isEmpty() && initial.size()>=16){failEarly(ui::text("Maximum 16 effects"));return;}
    if(target.isNotEmpty() && std::none_of(initial.begin(),initial.end(),[&](const auto& s){return s.id==target;})){failEarly(ui::text("目标效果器已移除，请重新拖放。"));return;}
    auto key=target.isEmpty()?"append:"+juce::Uuid().toString():target;auto serial=++onlineSerial;onlineRequests[key]=serial;
    auto active=[this,key,serial]{auto it=onlineRequests.find(key);return it!=onlineRequests.end() && it->second==serial;};
    auto finish=[this,key,active,completion](bool ok,juce::String status){
        if(!active()){if(completion)completion(false,ui::text("加载已被新的选择取代。"));return;}
        onlineRequests.erase(key);setMessage(status);if(completion)completion(ok,std::move(status));
    };
    setMessage(ui::text("正在读取在线音色…"));
    onlineSource.resolve(toneId,downloadCallbacks.wrap([this,target,modelId,key,active,finish](t3k::Result<t3k::ToneWithModels> result){
        if(!active()){finish(false,{});return;}
        if(!result.ok()){finish(false,ui::networkError(result.error.status,result.error.message));return;}
        auto tone=result->tone;
        if(tone.format!="nam" && tone.format!="ir"){finish(false,ui::text("该音色可浏览，但当前引擎仅支持 NAM 与 IR。"));return;}
        auto type=tone.format=="ir"?juce::String("Cabinet IR"):juce::String("NAM");
        auto chain=engine.chain();
        if(target.isNotEmpty()){
            auto at=std::find_if(chain.begin(),chain.end(),[&](const auto& s){return s.id==target;});
            if(at==chain.end() || at->type!=type){finish(false,ui::text("请拖到对应的 NAM / 箱体 IR 卡片，或拖到链路空白处。"));return;}
        }
        std::optional<t3k::Model> model;
        for(auto& m:result->models)if((modelId==0 || m.id==modelId) && ((tone.format=="nam" && m.extension()==".nam") || (tone.format=="ir" && m.extension()==".wav"))){model=m;break;}
        if(!model){finish(false,ui::text("这个音色没有当前可加载的模型。"));return;}
        setMessage(ui::text("正在下载并加载：")+tone.title);
        onlineSource.download(*model,downloadCallbacks.wrap([this,tone,model=*model,type,target,key,active,finish](t3k::Result<juce::File> file){
            if(!active()){finish(false,{});return;}
            if(!file.ok()){finish(false,ui::networkError(file.error.status,file.error.message));return;}
            auto* metadata=new juce::DynamicObject;metadata->setProperty("name",tone.title+" / "+model.name);metadata->setProperty("author",tone.user.creatorName());metadata->setProperty("license",tone.license);metadata->setProperty("sourceUrl",tone.url);metadata->setProperty("toneId",juce::String(tone.id));
            metadata->setProperty("category",tone.format=="ir"?"Cabinet IR":tone.gear=="amp" || tone.gear=="amp-cab"?"Amplifier":tone.gear=="pedal"?"Pedal":"Unsorted");
            juce::Array<juce::var> tags;for(auto& t:tone.tags)tags.add(t.name);for(auto& m:tone.makes)tags.add(m.name);metadata->setProperty("tags",tags);
            imports.addJob([this,file=*file,provenance=juce::var(metadata),type,target,key,active,finish]{
                std::optional<ModelEntry> entry;juce::String error;
                try{entry=services->library.importFile(file,provenance);entry->name=provenance.getProperty("name",entry->name).toString();++libraryVersion;}catch(const std::exception& e){error=juce::String::fromUTF8(e.what());}
                juce::MessageManager::callAsync(downloadCallbacks.wrap([this,entry=std::move(entry),error,type,target,key,active,finish]{
                    if(!active()){finish(false,{});return;}
                    if(!entry){finish(false,ui::text("模型校验失败：")+ui::message(error));return;}
                    auto chain=engine.chain();auto id=target;
                    if(target.isNotEmpty()){
                        auto at=std::find_if(chain.begin(),chain.end(),[&](const auto& s){return s.id==target;});
                        if(at==chain.end() || at->type!=type){finish(false,ui::text("目标效果器已移除，保留当前链路。"));return;}
                        at->file=services->library.fileFor(*entry).getFullPathName();at->title=entry->name;
                    }else{
                        if(chain.size()>=16){finish(false,ui::text("Maximum 16 effects"));return;}
                        StageSpec s;s.id=key.fromFirstOccurrenceOf("append:",false,false);id=s.id;s.type=type;s.file=services->library.fileFor(*entry).getFullPathName();s.title=entry->name;chain.push_back(std::move(s));
                    }
                    engine.setChain(std::move(chain));finish(true,ui::text("已加载在线音色：")+entry->name);if(onStageLoaded)onStageLoaded(id);
                }));
            });
        }));
    }));
}
void Processor::getStateInformation(juce::MemoryBlock& out) {
    auto* object=new juce::DynamicObject;object->setProperty("version",1);
    object->setProperty("parameters",parameters.copyState().toXmlString());
    juce::Array<juce::var> chain;for(auto& s:engine.chain())chain.add(s.toVar());object->setProperty("chain",chain);
    auto json=juce::JSON::toString(juce::var(object));out.replaceAll(json.toRawUTF8(),json.getNumBytesAsUTF8());
}
void Processor::setStateInformation(const void* data,int size) {
    if(size<=0 || size>1024*1024){setMessage("Preset is empty or too large");return;}
    auto value=juce::JSON::parse(juce::String::fromUTF8(static_cast<const char*>(data),size));
    auto chain=value.getProperty("chain",{});if(!chain.isArray() || chain.size()>16){setMessage("Invalid ToneDock preset");return;}
    try {
        std::vector<StageSpec> specs;
        for(auto& v:*chain.getArray()){auto s=StageSpec::fromVar(v);if(!effectTypes.contains(s.type))throw std::runtime_error("Unknown effect in preset");specs.push_back(std::move(s));}
        auto xml=juce::parseXML(value.getProperty("parameters","").toString());
        if(!xml)throw std::runtime_error("Missing preset parameters");
        parameters.replaceState(juce::ValueTree::fromXml(*xml));engine.setChain(std::move(specs));setMessage("Preset restored");
    }catch(const std::exception& e){setMessage(e.what());}
}
juce::AudioProcessorEditor* Processor::createEditor(){return new Editor(*this);}
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new tonedock::Processor;}

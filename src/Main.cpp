#include "Editor.h"
#include "AudioSettings.h"
#include "Catalog.h"

namespace tonedock {
class Application : public juce::JUCEApplication,private juce::Timer {
public:
    const juce::String getApplicationName() override{return "ToneDock";}
    const juce::String getApplicationVersion() override{return JUCE_APPLICATION_VERSION_STRING;}
    bool moreThanOneInstanceAllowed() override{return (getCommandLineParameters().contains("--screenshot=") || getCommandLineParameters().contains("--catalog-check=")) && !getCommandLineParameters().containsIgnoreCase("tonedock://");}
    void initialise(const juce::String& commandLine) override {
        ui::installStandaloneTranslations();
        processor=std::make_unique<Processor>();
        if(!commandLine.containsIgnoreCase("tonedock://"))for(auto& token:juce::StringArray::fromTokens(commandLine,true))if(token.startsWith("--catalog-check=")){
            probeMode=true;probeOutput=juce::File(token.fromFirstOccurrenceOf("=",false,false).unquoted());startCatalogCheck(commandLine);return;
        }
        auto session=defaultDataDirectory().getChildFile("last-session.tonedock");juce::MemoryBlock saved;
        if(session.existsAsFile() && session.getSize()<=1024*1024 && session.loadFileAsData(saved))processor->setStateInformation(saved.getData(),int(saved.getSize()));
        processor->createAudioSettings=[this]{return std::make_unique<AudioSettings>(devices,*processor);};
        window=std::make_unique<Window>(*this,*processor);
        auto tokens=juce::StringArray::fromTokens(commandLine,true);
        // A browser protocol invocation cannot enable developer CLI switches.
        if(!commandLine.containsIgnoreCase("tonedock://"))
            for(auto& token:tokens)if(token.startsWith("--screenshot="))screenshot=juce::File(token.fromFirstOccurrenceOf("=",false,false).unquoted());
        if((screenshot==juce::File() || tokens.contains("--audio-preview")) && !tokens.contains("--no-audio")) {
            auto settings=juce::parseXML(defaultDataDirectory().getChildFile("audio-device.xml"));
            auto error=devices.initialise(0,2,settings.get(),true);
            if(error.isNotEmpty())processor->setMessage("Audio device: "+error+". Choose Audio settings.");
            player.setProcessor(processor.get());devices.addAudioCallback(&player);
        }
        handleArguments(commandLine);
        if(pending.isNotEmpty()){handleArguments(pending);pending.clear();}
        if(screenshot!=juce::File()){
            if(tokens.contains("--preview-chain"))for(auto type:juce::StringArray{"Compressor","Drive","EQ","Chorus","Delay","Gain","Reverb","Gate"})processor->addEffect(type);
            if(auto* editor=dynamic_cast<Editor*>(processor->getActiveEditor())){
                if(tokens.contains("--compact"))editor->setSize(1120,780);
                if(tokens.contains("--device-page"))editor->showDevicePage(true);
                if(tokens.contains("--catalog-page"))editor->showOnlineLibrary(true);
            }
            int delay=1800;for(auto& token:tokens)if(token.startsWith("--screenshot-delay="))delay=juce::jlimit(500,30000,token.fromFirstOccurrenceOf("=",false,false).getIntValue());startTimer(delay);
        }
    }
    void shutdown() override {
        stopTimer();devices.removeAudioCallback(&player);player.setProcessor(nullptr);
        if(processor && !probeMode){juce::MemoryBlock state;processor->getStateInformation(state);juce::TemporaryFile temporary(defaultDataDirectory().getChildFile("last-session.tonedock"));
            if(temporary.getFile().replaceWithData(state.getData(),state.getSize()))temporary.overwriteTargetFileWithTemporary();}
        if(auto settings=devices.createStateXml())settings->writeTo(defaultDataDirectory().getChildFile("audio-device.xml"));
        probe.reset();window.reset();processor.reset();
    }
    void anotherInstanceStarted(const juce::String& line) override {
        if(!window){pending=line;return;}window->toFront(true);handleArguments(line);
    }
    void systemRequestedQuit() override{quit();}
private:
    void startCatalogCheck(const juce::String& commandLine){
        probeStarted=juce::Time::getMillisecondCounterHiRes();probeReport=new juce::DynamicObject;
        probeReport->setProperty("version",JUCE_APPLICATION_VERSION_STRING);probeReport->setProperty("connected",processor->services->tone3000.isConnected());
        if(!processor->services->tone3000.isConnected()){finishCatalogCheck(false,"Account not connected");return;}
        juce::String format="nam";for(auto& token:juce::StringArray::fromTokens(commandLine,true)){
            if(token.startsWith("--catalog-check-tone="))probeTone=token.fromFirstOccurrenceOf("=",false,false).getIntValue();
            if(token=="--catalog-check-format=ir")format="ir";
            if(token=="--catalog-check-pages=2")probeTargetBatches=2;
        }
        probeReport->setProperty("format",format);
        probe=std::make_unique<Catalog>(Catalog::forClient(processor->services->tone3000));
        probe->changed=[this]{
            if(probe->isLoading() || probeLoading)return;
            auto error=probe->error();probeReport->setProperty("searchHttpError",error.status);probeReport->setProperty("tonesRead",probe->tones().size());if(probeBatches==0)probeReport->setProperty("firstBatchCount",probe->tones().size());probeReport->setProperty("hasMore",probe->hasMore());
            if(error.status || error.message.isNotEmpty()){finishCatalogCheck(false,catalogErrorForReport(error));return;}
            if(probe->tones().isEmpty()){finishCatalogCheck(false,"Catalog search returned no tones");return;}
            ++probeBatches;probeReport->setProperty("batchesRead",probeBatches);if(probeBatches<probeTargetBatches && probe->hasMore()){probe->more();return;}probeLoading=true;
            int id=probeTone>0?probeTone:probe->tones()[0].id;probeReport->setProperty("loadedToneId",id);
            processor->loadOnlineTone(id,{},0,[this](bool ok,juce::String status){
                probeReport->setProperty("loadSucceeded",ok);if(!ok){finishCatalogCheck(false,status);return;}
                probeWaitingForEngine=true;
            });
        };
        CatalogQuery query;query.filters.query=format=="nam"?"Fender":"";query.filters.format=format;query.filters.sort=format=="nam"?"best-match":"trending";probe->search(std::move(query));startTimer(100);
    }
    static juce::String catalogErrorForReport(const t3k::Error& e){return ui::networkError(e.status,e.message);}
    void finishCatalogCheck(bool ok,const juce::String& message){
        stopTimer();probeReport->setProperty("passed",ok);probeReport->setProperty("message",message);
        probeReport->setProperty("chainSize",int(processor->engine.chain().size()));
        if(ok){auto chain=processor->engine.chain();probeReport->setProperty("modelExists",juce::File(chain.back().file).existsAsFile());probeReport->setProperty("modelName",chain.back().title);}
        probeOutput.getParentDirectory().createDirectory();probeOutput.replaceWithText(juce::JSON::toString(juce::var(probeReport.get())));setApplicationReturnValue(ok?0:2);quit();
    }
    class Window : public juce::DocumentWindow {
    public:
        Window(Application& a,Processor& p):DocumentWindow("ToneDock",juce::Colour(0xff111715),allButtons),app(a) {
            setUsingNativeTitleBar(true);setContentOwned(p.createEditorIfNeeded(),true);setResizable(true,true);setResizeLimits(1120,810,1920,1230);centreWithSize(getWidth(),getHeight());setVisible(true);
        }
        void closeButtonPressed() override{app.systemRequestedQuit();}
    private:Application& app;
    };
    void handleArguments(const juce::String& commandLine) {
        juce::StringArray files;
        for(auto token:juce::StringArray::fromTokens(commandLine,true)) {
            token=token.unquoted();if(token.startsWith("--"))continue;
            if(parseToneLink(token))processor->openToneLink(token);
            else if(!commandLine.containsIgnoreCase("tonedock://") && juce::File::isAbsolutePath(token) && juce::File(token).exists())files.add(token);
        }
        if(!files.isEmpty())processor->importFiles(files);
    }
    void timerCallback() override {
        if(probeMode){
            if(juce::Time::getMillisecondCounterHiRes()-probeStarted>45000){finishCatalogCheck(false,"Catalog validation timed out");return;}
            if(probeWaitingForEngine){auto status=processor->engine.status();if(status=="Ready")finishCatalogCheck(true,"Catalog search, download, validation and graph loading passed");else if(status!="Preparing audio chain...")finishCatalogCheck(false,status);}return;
        }
        stopTimer();auto* editor=processor->getActiveEditor();
        if(editor){auto image=editor->createComponentSnapshot(editor->getLocalBounds());screenshot.getParentDirectory().createDirectory();
            if(auto stream=screenshot.createOutputStream()){stream->setPosition(0);stream->truncate();juce::PNGImageFormat().writeImageToStream(image,*stream);}}
        quit();
    }
    juce::AudioDeviceManager devices;juce::AudioProcessorPlayer player;
    std::unique_ptr<Processor> processor;std::unique_ptr<Window> window;
    juce::String pending;juce::File screenshot;
    bool probeMode=false,probeLoading=false,probeWaitingForEngine=false;int probeTone=0,probeBatches=0,probeTargetBatches=1;double probeStarted=0;
    juce::File probeOutput;juce::DynamicObject::Ptr probeReport;std::unique_ptr<Catalog> probe;
};
}
START_JUCE_APPLICATION(tonedock::Application)

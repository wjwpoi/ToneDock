#include "Editor.h"

namespace tonedock {
namespace {
using ui::background;using ui::panel;using ui::ink;using ui::muted;using ui::accent;
void style(juce::TextEditor& e,const juce::String& placeholder) {
    ui::field(e,placeholder);e.setColour(juce::TextEditor::backgroundColourId,panel);
    e.setColour(juce::TextEditor::textColourId,ink);e.setColour(juce::TextEditor::outlineColourId,juce::Colour(0xff35443a));
}
juce::StringArray parameterNames(const juce::String& type) {
    if(type=="Gate")return {ui::text("Threshold"),ui::text("Release")};
    if(type=="Compressor")return {ui::text("Threshold"),ui::text("Ratio"),ui::text("Attack"),ui::text("Release")};
    if(type=="Drive")return {ui::text("Drive"),ui::text("Tone"),ui::text("Level")};
    if(type=="NAM" || type=="Gain")return {ui::text("Level")};
    if(type=="EQ")return {ui::text("Bass"),ui::text("Mid"),ui::text("Treble")};
    if(type=="Chorus")return {ui::text("Rate"),ui::text("Depth"),ui::text("Feedback"),ui::text("Mix")};
    if(type=="Delay")return {ui::text("Time"),ui::text("Feedback"),ui::text("Mix")};
    if(type=="Reverb")return {ui::text("Room"),ui::text("Damping"),ui::text("Mix"),ui::text("Width")};return {};
}
juce::String parameterText(const juce::String& t,int i,double v) {
    if(t=="NAM")return juce::String((v-.5)*24,1)+" dB";
    if(t=="Gain")return juce::String((v-.5)*48,1)+" dB";
    if(t=="EQ")return juce::String((v-.5)*24,1)+" dB";
    if(t=="Gate")return i==0?juce::String(-80+70*v,1)+" dB":juce::String(20+480*v,0)+" ms";
    if(t=="Compressor") {
        if(i==0)return juce::String(-48+48*v,1)+" dB";
        if(i==1)return juce::String(1+19*v,1)+":1";
        return juce::String(i==2?1+49*v:20+480*v,0)+" ms";
    }
    if(t=="Delay" && i==0)return juce::String(20+980*v,0)+" ms";
    if(t=="Chorus" && i==0)return juce::String(.1+4.9*v,2)+" Hz";
    if(t=="Drive") {
        if(i==0)return juce::String(1+39*v,1)+" x";
        if(i==1)return juce::String(500+9500*v,0)+" Hz";
        return juce::String((v-.5)*24,1)+" dB";
    }
    return juce::String(v*100,0)+" %";
}
}
struct Editor::ModelRows : juce::ListBoxModel {
    Editor& editor;explicit ModelRows(Editor& e):editor(e){}
    int getNumRows() override{return int(editor.filtered.size());}
    void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool selected) override {
        if(row<0 || row>=getNumRows())return;auto& m=editor.filtered[size_t(row)];
        g.setColour(selected?juce::Colour(0xff344734):panel);g.fillRoundedRectangle(4,2,float(w-8),float(h-4),6);
        g.setColour(ink);g.setFont(ui::font(14));g.drawText((m.favorite?"* ":"")+m.name,13,5,w-26,23,juce::Justification::centredLeft);
        g.setColour(muted);g.setFont(ui::font(11));g.drawText(ui::categoryLabel(m.category)+"  /  "+ui::text(m.architecture),13,29,w-26,17,juce::Justification::centredLeft);
    }
    void selectedRowsChanged(int row) override{editor.selectModel(row);}
    void listBoxItemDoubleClicked(int row,const juce::MouseEvent&) override{
        if(row>=0 && row<getNumRows()){editor.libraryList.selectRow(row);editor.loadSelectedModel();}
    }
};

class Editor::Pedal : public juce::Component {
public:
    Pedal(Editor& e,StageSpec s,int n):editor(e),stage(std::move(s)),number(n) {
        setComponentID("stage:"+stage.id);setTitle(ui::text(stage.type));
        addAndMakeVisible(power);power.setComponentID("power:"+stage.id);
        power.onClick=[this]{stage.controls->enabled.store(!stage.controls->enabled.load());repaint();};
        power.setTooltip(ui::text("旁通保留参数，再次点击恢复"));
    }
    void resized() override{power.setBounds(14,getHeight()-45,getWidth()-28,30);}
    void paint(juce::Graphics& g) override {
        bool selected=editor.selectedStageId==stage.id,enabled=stage.controls->enabled.load();
        auto colour=stage.type=="NAM"?accent:stage.type=="Drive"?juce::Colour(0xffefb383):
            stage.type=="Delay" || stage.type=="Reverb" || stage.type=="Chorus"?juce::Colour(0xff8ecbc9):juce::Colour(0xffafbbdc);
        auto area=getLocalBounds().toFloat().reduced(1.5f);
        g.setColour(selected?juce::Colour(0xff2a392d):panel);g.fillRoundedRectangle(area,12);
        g.setColour(selected?accent:ui::line);g.drawRoundedRectangle(area,12,selected?2.f:1.f);
        g.setColour(colour.withAlpha(enabled?1.f:.35f));g.fillRoundedRectangle(15,16,32,5,2);
        g.setColour(muted);g.setFont(ui::font(12));g.drawText(juce::String(number+1).paddedLeft('0',2),getWidth()-42,12,26,18,juce::Justification::centredRight);
        g.setColour(enabled?ink:muted);g.setFont(ui::font(20));g.drawText(ui::text(stage.type),15,37,getWidth()-30,29,juce::Justification::centredLeft);
        g.setColour(muted);g.setFont(ui::font(13));
        auto caption=stage.type=="NAM" || stage.type=="Cabinet IR"?
            (stage.file.isEmpty()?ui::text("从模型库加载模型"):stage.title):ui::text("内置效果 · 拖动调整顺序");
        g.drawFittedText(caption,15,75,getWidth()-30,55,juce::Justification::topLeft,3);
        power.setButtonText(ui::text(enabled?"已启用":"已旁通"));
        power.setColour(juce::TextButton::textColourOffId,enabled?colour:muted);
    }
    void mouseDown(const juce::MouseEvent&) override{editor.selectStage(number);}
    void mouseDrag(const juce::MouseEvent& e) override {
        if(e.getDistanceFromDragStart()>6 && !editor.isDragAndDropActive())editor.startDragging("stage:"+stage.id,this);
    }
private:Editor& editor;StageSpec stage;int number;juce::TextButton power;
};
class Editor::Board : public juce::Component {
public:
    explicit Board(Editor& e):editor(e){}
    void rebuild() {
        pedals.clear();for(size_t i=0;i<editor.visibleChain.size();++i){auto p=std::make_unique<Pedal>(editor,editor.visibleChain[i],int(i));addAndMakeVisible(*p);pedals.push_back(std::move(p));}
        resized();repaint();
    }
    void resized() override{for(size_t i=0;i<pedals.size();++i)pedals[i]->setBounds(12+int(i)*200,6,176,190);}
    void paint(juce::Graphics& g) override {
        g.setColour(ui::line);g.fillRect(0,97,getWidth(),2);
        g.setColour(muted);g.setFont(ui::font(18));for(size_t i=1;i<pedals.size();++i)g.drawText("›",int(i)*200-12,78,24,40,juce::Justification::centred);
        if(pedals.empty()){g.setColour(background);g.fillAll();g.setColour(muted);g.setFont(ui::font(16));g.drawText(ui::text("添加一个效果器，开始搭建你的声音"),20,75,getWidth()-40,40,juce::Justification::centred);}
    }
private:Editor& editor;std::vector<std::unique_ptr<Pedal>> pedals;
};

class Editor::ToneBrowser : public juce::Component,private juce::ListBoxModel {
public:
    explicit ToneBrowser(Processor& p,int toneId):processor(p),client(p.services->tone3000) {
        for(auto* component:std::initializer_list<juce::Component*>{&key,&connect,&browse,&disconnect,&feed,&architecture,&refresh,&previous,&next,&list,&detail,&models,&download,&source,&state})addAndMakeVisible(component);
        style(key,"TONE3000 publishable key: t3k_pub_...");key.setText(client.publishableKey());key.setReadOnly(client.publishableKey().isNotEmpty());
        connect.onClick=[this]{startFlow(t3k::FlowRequest::Kind::selectTone,0);};
        browse.onClick=[this]{startFlow(t3k::FlowRequest::Kind::selectTone,0);};
        disconnect.onClick=[this]{if(flow)flow->cancel();client.clearTokens();state.setText(ui::text("Disconnected"),juce::dontSendNotification);};
        for(auto name:juce::StringArray{"Latest","Trending","Favorites","Created","Downloaded"})feed.addItem(ui::text(name),feed.getNumItems()+1);
        feed.setSelectedId(1);feed.onChange=[this]{page=1;loadFeed();};refresh.onClick=[this]{loadFeed();};
        previous.onClick=[this]{if(page>1){--page;loadFeed();}};next.onClick=[this]{++page;loadFeed();};
        architecture.addItem(ui::text("A2 models"),1);architecture.addItem(ui::text("A1 / Custom models"),2);architecture.setSelectedId(client.selectedArchitecture()==2?1:2);
        architecture.onChange=[this]{++selectionGeneration;client.setArchitecture(architecture.getSelectedId()==1?2:0);models.clear();download.setEnabled(false);};
        list.setModel(this);list.setRowHeight(42);list.setColour(juce::ListBox::backgroundColourId,background);
        style(detail,"Select a tone to inspect its creator, license and captures");detail.setMultiLine(true);detail.setReadOnly(true);
        models.onChange=[this]{download.setEnabled(models.getSelectedId()>0);};download.setEnabled(false);
        download.onClick=[this]{downloadSelected();};source.onClick=[this]{if(current.tone.url.startsWith("https://"))juce::URL(current.tone.url).launchInDefaultBrowser();};
        setSize(900,650);
        if(toneId>0)startFlow(t3k::FlowRequest::Kind::loadTone,toneId);
        else if(client.isConnected())loadFeed();
        else state.setText(ui::text("Connect your account, then browse or open a tone link."),juce::dontSendNotification);
    }
    ~ToneBrowser() override{if(flow)flow->cancel();list.setModel(nullptr);}
    void paint(juce::Graphics& g) override{g.fillAll(background);g.setColour(accent);g.setFont(ui::font(20));g.drawText(ui::text("Powered by TONE3000"),20,12,500,28,juce::Justification::centredLeft);}
    void resized() override {
        int w=getWidth();key.setBounds(20,52,w-370,30);connect.setBounds(w-340,52,95,30);browse.setBounds(w-235,52,100,30);disconnect.setBounds(w-125,52,105,30);
        feed.setBounds(20,96,140,28);architecture.setBounds(170,96,180,28);refresh.setBounds(360,96,85,28);previous.setBounds(455,96,65,28);next.setBounds(530,96,65,28);
        list.setBounds(20,140,350,getHeight()-204);detail.setBounds(390,140,w-410,getHeight()-300);
        models.setBounds(390,getHeight()-145,w-410,30);download.setBounds(390,getHeight()-100,170,32);source.setBounds(575,getHeight()-100,180,32);
        state.setBounds(20,getHeight()-48,w-40,35);
    }
private:
    int getNumRows() override{return tones.size();}
    void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool selected) override {
        if(row<0 || row>=tones.size())return;g.setColour(selected?juce::Colour(0xff344734):panel);g.fillRect(0,1,w,h-2);
        g.setColour(ink);g.setFont(ui::font(13));g.drawText(tones[row].title,8,3,w-16,20,juce::Justification::centredLeft);
        g.setColour(muted);g.setFont(ui::font(11));g.drawText(tones[row].user.creatorName()+" / "+ui::text(tones[row].gear),8,23,w-16,16,juce::Justification::centredLeft);
    }
    void selectedRowsChanged(int row) override{if(row>=0 && row<tones.size())loadTone(tones[row].id);}
    bool readyKey() {
        if(client.publishableKey().isNotEmpty())return true;
        if(processor.services->configureKey(key.getText())){key.setReadOnly(true);return true;}
        state.setText(ui::text("Enter a publishable t3k_pub_ key from TONE3000 Settings > API Keys. No secret key is needed."),juce::dontSendNotification);return false;
    }
    void startFlow(t3k::FlowRequest::Kind kind,int id) {
        if(!readyKey())return;
        if(flow)flow->cancel();flow=t3k::makeAuthFlow(t3k::FlowMode::systemBrowser);
        t3k::FlowRequest request;request.kind=kind;request.catalog.architecture=client.selectedArchitecture();request.catalog.preview=true;
        if(id>0)request.toneId=juce::String(id);
        state.setText(ui::text("Continue in your browser. You will return here after choosing a tone."),juce::dontSendNotification);
        try{flow->start(request,scope.wrap([this](t3k::FlowResult result){
            if(result.connected()) {
                state.setText(ui::text("Connected"),juce::dontSendNotification);
                if(result.toneId.containsOnly("0123456789") && result.toneId.getIntValue()>0)loadTone(result.toneId.getIntValue());else loadFeed();
            }else state.setText(result.canceled()?ui::text("Canceled"):ui::message(result.error),juce::dontSendNotification);
        }));}catch(const std::exception& e){state.setText(ui::message(e.what()),juce::dontSendNotification);}
    }
    void loadFeed() {
        if(!client.isConnected()){state.setText(ui::text("Connect TONE3000 first"),juce::dontSendNotification);return;}
        state.setText(ui::text("Loading page ")+juce::String(page)+"...",juce::dontSendNotification);
        auto done=scope.wrap([this](t3k::Result<t3k::Page<t3k::Tone>> result){
            if(!result.ok()){state.setText(ui::networkError(result.error.status,result.error.message),juce::dontSendNotification);return;}
            tones=result->data;list.updateContent();previous.setEnabled(page>1);next.setEnabled(page<result->totalPages);
            state.setText(ui::text("Page ")+juce::String(page)+" / "+juce::String(result->totalPages),juce::dontSendNotification);
        });
        t3k::ListLibraryParams params;params.page=page;params.pageSize=30;
        if(feed.getSelectedId()==3)client.listFavoritedTones(params,done);
        else if(feed.getSelectedId()==4)client.listCreatedTones(params,done);
        else if(feed.getSelectedId()==5)client.listDownloadedTones(params,done);
        else {
            auto callback=scope.wrap([this](t3k::Result<juce::Array<t3k::Tone>> result){
                if(!result.ok()){state.setText(ui::networkError(result.error.status,result.error.message),juce::dontSendNotification);return;}
                tones=*result;list.updateContent();previous.setEnabled(false);next.setEnabled(false);state.setText(juce::String(tones.size())+ui::text(" tones"),juce::dontSendNotification);
            });
            if(feed.getSelectedId()==2)client.listTrendingTones({},callback);else client.listLatestTones(callback);
        }
    }
    void loadTone(int id) {
        auto ticket=++selectionGeneration;state.setText(ui::text("Loading tone and captures..."),juce::dontSendNotification);models.clear();download.setEnabled(false);
        client.getToneWithModels(id,scope.wrap([this,ticket](t3k::Result<t3k::ToneWithModels> result){
            if(ticket!=selectionGeneration)return;
            if(!result.ok()){state.setText(ui::networkError(result.error.status,result.error.message),juce::dontSendNotification);return;}
            current=*result;models.clear();for(auto& m:current.models)models.addItem(m.name,models.getNumItems()+1);
            detail.setText(current.tone.title+"\n\n"+ui::text("By ")+current.tone.user.creatorName()+"\n"+current.tone.user.url+"\n\n"+current.tone.description
                +"\n\n"+ui::text("Gear: ")+ui::text(current.tone.gear)+"\n"+ui::text("License: ")+t3k::licenseLabel(current.tone.license)+"\n\n"+current.tone.url);
            if(models.getNumItems()>0)models.setSelectedId(1);
            state.setText(ui::text(current.models.isEmpty()?"No supported captures for this architecture. Try A1 / Custom or A2.":"Choose a capture to download into your library."),juce::dontSendNotification);
        }));
    }
    void downloadSelected() {
        auto row=models.getSelectedId()-1;if(row<0 || row>=current.models.size())return;
        auto tone=current.tone;auto model=current.models[row];download.setEnabled(false);state.setText(ui::text("Downloading ")+model.name+"...",juce::dontSendNotification);
        auto* meta=new juce::DynamicObject;meta->setProperty("name",tone.title+" / "+model.name);meta->setProperty("author",tone.user.creatorName());
            meta->setProperty("license",tone.license);meta->setProperty("sourceUrl",tone.url);meta->setProperty("toneId",juce::String(tone.id));
            meta->setProperty("category",tone.gear=="amp" || tone.gear=="amp-cab"?"Amplifier":tone.gear=="pedal"?"Pedal":tone.format=="ir"?"Cabinet IR":"Unsorted");
            juce::Array<juce::var> tags;for(auto& t:tone.tags)tags.add(t.name);for(auto& make:tone.makes)tags.add(make.name);meta->setProperty("tags",tags);
        processor.downloadModel(model,juce::var(meta),scope.wrap([this](juce::String status){download.setEnabled(true);state.setText(ui::message(status),juce::dontSendNotification);}));
    }
    Processor& processor;t3k::Client& client;t3k::AsyncScope scope;
    std::unique_ptr<t3k::AuthFlow> flow;
    juce::TextEditor key,detail;juce::TextButton connect{ui::text("Connect")},browse{ui::text("Browse")},disconnect{ui::text("Disconnect")},refresh{ui::text("Refresh")},previous{ui::text("Prev")},next{ui::text("Next")},download{ui::text("Download to library")},source{ui::text("View on TONE3000")};
    juce::ComboBox feed,architecture,models;juce::ListBox list;juce::Label state;
    juce::Array<t3k::Tone> tones;t3k::ToneWithModels current;int page=1,selectionGeneration=0;
};


Editor::Editor(Processor& p):AudioProcessorEditor(p),processor(p) {
    setLookAndFeel(&theme);
    for(auto* c:std::initializer_list<juce::Component*>{&libraryList,&chainViewport,&search,&categoryEdit,&tagsEdit,&link,&categoryFilter,&effectChoice,&importButton,&cloudButton,&loadButton,&favoriteButton,&metadataButton,&removeModelButton,&addButton,&deleteButton,&upButton,&downButton,&bypassStageButton,&audioButton,&saveButton,&openButton,&linkButton,&inputGain,&outputGain,&bypass,&workButton,&duplicateButton})addAndMakeVisible(c);
    libraryList.setComponentID("library-list");search.setComponentID("library-search");categoryEdit.setComponentID("library-category");categoryFilter.setComponentID("library-filter");effectChoice.setComponentID("effect-choice");
    addButton.setComponentID("add-effect");deleteButton.setComponentID("delete-effect");duplicateButton.setComponentID("duplicate-effect");upButton.setComponentID("move-left");downButton.setComponentID("move-right");bypassStageButton.setComponentID("bypass-effect");audioButton.setComponentID("audio-page");workButton.setComponentID("work-page");chainViewport.setComponentID("chain-viewport");
    workButton.setClickingTogglesState(false);audioButton.setButtonText(ui::text("设备"));workButton.setToggleState(true,juce::dontSendNotification);
    upButton.setButtonText(ui::text("← 前移"));downButton.setButtonText(ui::text("后移 →"));deleteButton.setButtonText(ui::text("删除"));bypassStageButton.setButtonText(ui::text("旁通 / 启用"));
    modelRows=std::make_unique<ModelRows>(*this);libraryList.setModel(modelRows.get());libraryList.setRowHeight(61);
    board=std::make_unique<Board>(*this);chainViewport.setViewedComponent(board.get(),false);chainViewport.setScrollBarsShown(false,true);chainViewport.setScrollBarThickness(8);
    style(search,"Search names, tags, creator...");style(categoryEdit,"Category");style(tagsEdit,"Tags, comma separated");style(link,"Paste a TONE3000 tone page URL");
    search.onTextChange=[this]{refreshLibrary();};categoryFilter.onChange=[this]{refreshLibrary();};
    categoryFilter.addItem(ui::text("All models"),1);categoryFilter.addItem(ui::text("Favorites"),2);categoryFilter.setSelectedId(1);
    for(auto& t:effectTypes)effectChoice.addItem(ui::text(t),effectChoice.getNumItems()+1);effectChoice.setSelectedId(3);
    importButton.onClick=[this]{chooseImport();};cloudButton.onClick=[this]{openTone();};linkButton.onClick=[this]{processor.openToneLink(link.getText());};
    loadButton.onClick=[this]{loadSelectedModel();};
    favoriteButton.onClick=[this]{auto row=libraryList.getSelectedRow();if(row<0 || row>=int(filtered.size()))return;
        try{auto m=filtered[size_t(row)];m.favorite=!m.favorite;processor.services->library.update(m);++processor.libraryVersion;refreshLibrary();}
        catch(const std::exception& e){processor.setMessage(e.what());}};
    metadataButton.onClick=[this]{auto row=libraryList.getSelectedRow();if(row<0 || row>=int(filtered.size()))return;
        try{auto m=filtered[size_t(row)];auto category=categoryEdit.getText().trim();
            if(category!=ui::categoryLabel(m.category))m.category=ui::categoryValue(category);
            m.tags=juce::StringArray::fromTokens(tagsEdit.getText(),",","");m.tags.trim();m.tags.removeEmptyStrings();processor.services->library.update(m);++processor.libraryVersion;refreshLibrary();}
        catch(const std::exception& e){processor.setMessage(e.what());}};
    removeModelButton.onClick=[this]{auto row=libraryList.getSelectedRow();if(row<0 || row>=int(filtered.size()))return;
        try{processor.services->library.remove(filtered[size_t(row)].id);++processor.libraryVersion;refreshLibrary();}catch(const std::exception& e){processor.setMessage(e.what());}};
    addButton.onClick=[this]{auto index=effectChoice.getSelectedId()-1;if(index<0 || index>=effectTypes.size())return;
        auto before=processor.engine.chain().size();processor.addEffect(effectTypes[index]);syncChain();if(visibleChain.size()>before)selectStage(int(visibleChain.size())-1);};
    deleteButton.onClick=[this]{auto index=0;for(size_t i=0;i<visibleChain.size();++i)if(visibleChain[i].id==selectedStageId)index=int(i);
        editChain([this](auto& chain){std::erase_if(chain,[this](const auto& s){return s.id==selectedStageId;});});if(!visibleChain.empty())selectStage(juce::jmin(index,int(visibleChain.size())-1));};
    upButton.onClick=[this]{moveStage(-1);};downButton.onClick=[this]{moveStage(1);};duplicateButton.onClick=[this]{duplicateStage();};
    bypassStageButton.onClick=[this]{for(auto& s:visibleChain)if(s.id==selectedStageId)s.controls->enabled.store(!s.controls->enabled.load());repaint();};
    audioButton.onClick=[this]{showDevicePage(true);};workButton.onClick=[this]{showDevicePage(false);};
    saveButton.onClick=[this]{preset(true);};openButton.onClick=[this]{preset(false);};
    for(auto* slider:{&inputGain,&outputGain}){slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,70,24);slider->setTextValueSuffix(" dB");}
    inputAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"input",inputGain);
    outputAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"output",outputGain);
    bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"bypass",bypass);
    for(int i=0;i<4;++i){auto& k=knobs[size_t(i)];addAndMakeVisible(k);addAndMakeVisible(knobLabels[size_t(i)]);k.setComponentID("stage-knob-"+juce::String(i));k.setRange(0,1,.001);k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,110,24);
        knobLabels[size_t(i)].setFont(ui::font(14));knobLabels[size_t(i)].setJustificationType(juce::Justification::centred);k.onValueChange=[this,i]{for(auto& s:visibleChain)if(s.id==selectedStageId)s.controls->values[size_t(i)].store(float(knobs[size_t(i)].getValue()));};}
    processor.onOpenTone=[this](int id){openTone(id);};setResizable(true,true);setResizeLimits(1120,780,1920,1200);setSize(1360,880);
    refreshLibrary();syncChain();if(!visibleChain.empty())selectStage(juce::jmin(1,int(visibleChain.size())-1));startTimerHz(15);
}
Editor::~Editor(){processor.onOpenTone=nullptr;stopTimer();devicePage.reset();browser.reset();chainViewport.setViewedComponent(nullptr,false);board.reset();libraryList.setModel(nullptr);setLookAndFeel(nullptr);}
void Editor::showDevicePage(bool show) {
    if(show && !devicePage){
        if(processor.createAudioSettings)devicePage=processor.createAudioSettings();
        else {auto label=std::make_unique<juce::Label>();label->setText(ui::text("音频设备由宿主软件管理\n\n请在 DAW 的音频设置中选择接口、输入通道、采样率和缓冲大小。"),juce::dontSendNotification);label->setJustificationType(juce::Justification::centred);label->setFont(ui::font(19));devicePage=std::move(label);}
        addAndMakeVisible(*devicePage);devicePage->setComponentID("device-content");
    }
    devicePageVisible=show;
    for(int i=0;i<getNumChildComponents();++i){auto* c=getChildComponent(i);if(c==&workButton || c==&audioButton || c==&cloudButton || c==&saveButton || c==&openButton || c==devicePage.get())continue;c->setVisible(!show);}
    if(devicePage)devicePage->setVisible(show);
    workButton.setToggleState(!show,juce::dontSendNotification);audioButton.setToggleState(show,juce::dontSendNotification);
    if(!show)for(size_t i=0;i<visibleChain.size();++i)if(visibleChain[i].id==selectedStageId)selectStage(int(i));
    resized();repaint();
}
void Editor::paint(juce::Graphics& g) {
    int w=getWidth(),h=getHeight(),x=336,rw=w-360;
    g.fillAll(background);g.setColour(panel);g.fillRect(0,0,w,80);
    g.setColour(accent);g.setFont(ui::font(29));g.drawText("ToneDock",24,14,220,36,juce::Justification::centredLeft);
    g.setColour(muted);g.setFont(ui::font(12));g.drawText(ui::text("你的音色，你的声音。"),26,51,230,20,juce::Justification::centredLeft);
    g.setColour(ui::line);g.drawHorizontalLine(80,0,float(w));
    if(!devicePageVisible){
        ui::card(g,{16,98,298,h-140});ui::card(g,{x,444,rw,h-594});ui::card(g,{x,h-134,rw,82});
        g.setColour(ink);g.setFont(ui::font(19));g.drawText(ui::text("模型库"),30,110,150,28,juce::Justification::centredLeft);
        g.drawText(ui::text("效果链"),x,105,150,30,juce::Justification::centredLeft);
        g.setColour(muted);g.setFont(ui::font(13));g.drawText(juce::String(visibleChain.size())+" / 16  ·  "+ui::text("拖动排序，点击编辑参数"),x,140,400,24,juce::Justification::centredLeft);
        for(auto& s:visibleChain)if(s.id==selectedStageId){
            g.setColour(ink);g.setFont(ui::font(18));g.drawText(ui::text(s.type),x+20,457,190,30,juce::Justification::centredLeft);
            if(s.type=="NAM" || s.type=="Cabinet IR"){
                g.setColour(muted);g.setFont(ui::font(14));auto text=s.file.isEmpty()?ui::text("在左侧选择模型，点击「加载模型」\n模型会载入当前选中的效果器。"):s.title;
                if(s.type=="NAM")text+="\n\n"+ui::text("包含箱体的模型通常无需再加箱体 IR。");
                else text+="\n\n"+ui::text("WAV 脉冲响应 · 放在音箱模型之后。");
                g.drawFittedText(text,x+200,510,rw-230,juce::jmax(60,h-690),juce::Justification::topLeft,6);
            }
        }
        if(visibleChain.empty()){g.setColour(muted);g.setFont(ui::font(16));g.drawText(ui::text("选择一个效果器以编辑参数"),x+20,480,rw-40,80,juce::Justification::centred);}
        if(filtered.empty()){g.setColour(muted);g.setFont(ui::font(14));g.drawFittedText(ui::text("Drop .nam / IR .wav / .zip here\nor import from TONE3000"),36,270,258,100,juce::Justification::centred,4);}
        g.setColour(muted);g.setFont(ui::font(13));g.drawText(ui::text("输入"),x+18,h-124,60,22,juce::Justification::centredLeft);g.drawText(ui::text("输出"),x+rw/2-34,h-124,60,22,juce::Justification::centredLeft);
        ui::meter(g,{x+22,h-70,rw/2-100,5},processor.inputPeak.load());ui::meter(g,{x+rw/2-30,h-70,rw/2-160,5},processor.outputPeak.load());
    }
    g.setColour(muted);g.setFont(ui::font(12));g.drawText(displayStatus,24,h-37,w-290,27,juce::Justification::centredLeft);
    g.drawText(juce::String(processor.engine.latency())+ui::text(" samples DSP latency"),w-260,h-37,236,27,juce::Justification::centredRight);
}
void Editor::resized() {
    int w=getWidth(),h=getHeight(),x=336,rw=w-360;
    workButton.setBounds(336,24,100,34);audioButton.setBounds(446,24,90,34);openButton.setBounds(w-378,24,110,34);saveButton.setBounds(w-258,24,110,34);cloudButton.setBounds(w-138,24,114,34);
    if(devicePage)devicePage->setBounds(24,98,w-48,h-148);
    search.setBounds(30,151,270,34);categoryFilter.setBounds(30,195,270,32);libraryList.setBounds(26,239,278,h-548);
    loadButton.setBounds(30,h-300,166,32);favoriteButton.setBounds(204,h-300,96,32);categoryEdit.setBounds(30,h-258,270,32);tagsEdit.setBounds(30,h-218,270,32);
    metadataButton.setBounds(30,h-176,166,32);removeModelButton.setBounds(204,h-176,96,32);removeModelButton.setButtonText(ui::text("移出库"));importButton.setBounds(30,h-130,270,34);
    effectChoice.setBounds(w-326,108,186,34);addButton.setBounds(w-130,108,106,34);
    chainViewport.setBounds(x,216,rw,212);board->setSize(juce::jmax(rw,int(visibleChain.size())*200+12),200);
    int count=int(overview.size());for(int i=0;i<count;++i){int left=x+i*rw/juce::jmax(1,count),right=x+(i+1)*rw/juce::jmax(1,count);overview[size_t(i)]->setBounds(left,175,right-left-5,28);}
    int bx=w-504;duplicateButton.setBounds(bx,458,100,30);bypassStageButton.setBounds(bx+108,458,104,30);upButton.setBounds(bx+220,458,78,30);downButton.setBounds(bx+306,458,78,30);deleteButton.setBounds(bx+392,458,88,30);
    int kw=juce::jmin(180,(rw-40)/4),kh=juce::jmin(164,h-672);
    for(int i=0;i<4;++i){knobLabels[size_t(i)].setBounds(x+20+i*kw,503,kw,24);knobs[size_t(i)].setBounds(x+20+i*kw,529,kw,juce::jmax(100,kh));}
    link.setBounds(30,h-86,184,30);linkButton.setBounds(222,h-86,78,30);
    inputGain.setBounds(x+65,h-124,rw/2-140,36);outputGain.setBounds(x+rw/2+14,h-124,rw/2-180,36);bypass.setBounds(w-158,h-115,130,30);
}
void Editor::loadSelectedModel(){auto row=libraryList.getSelectedRow();if(row>=0 && row<int(filtered.size())){
    auto type=filtered[size_t(row)].kind=="ir"?juce::String("Cabinet IR"):juce::String("NAM");auto target=selectedStageId;
    auto selected=std::find_if(visibleChain.begin(),visibleChain.end(),[&](const auto& s){return s.id==target && s.type==type;});
    if(selected==visibleChain.end()){target.clear();for(auto& s:visibleChain)if(s.type==type){target=s.id;break;}}
    processor.loadModel(filtered[size_t(row)],target);syncChain();
    for(size_t i=0;i<visibleChain.size();++i)if(visibleChain[i].type==type && (target.isEmpty() || visibleChain[i].id==target)){selectStage(int(i));break;}
}}
void Editor::syncChain() {
    visibleChain=processor.engine.chain();board->rebuild();overview.clear();
    for(size_t i=0;i<visibleChain.size();++i){auto b=std::make_unique<juce::TextButton>(juce::String(int(i)+1).paddedLeft('0',2)+(visibleChain.size()<=8?"  "+ui::text(visibleChain[i].type):juce::String()));b->setComponentID("overview:"+visibleChain[i].id);b->setTooltip(ui::text(visibleChain[i].type)+" · "+visibleChain[i].title);b->onClick=[this,i]{selectStage(int(i));};addAndMakeVisible(*b);b->setVisible(!devicePageVisible);overview.push_back(std::move(b));}
    bool found=false;for(size_t i=0;i<visibleChain.size();++i)if(visibleChain[i].id==selectedStageId){selectStage(int(i));found=true;break;}
    if(!found){selectedStageId.clear();if(!visibleChain.empty())selectStage(0);else for(int i=0;i<4;++i){knobs[size_t(i)].setVisible(false);knobLabels[size_t(i)].setVisible(false);}}
    if(visibleChain.empty()){upButton.setEnabled(false);downButton.setEnabled(false);}
    addButton.setEnabled(visibleChain.size()<16);duplicateButton.setEnabled(!visibleChain.empty() && visibleChain.size()<16);deleteButton.setEnabled(!visibleChain.empty());bypassStageButton.setEnabled(!visibleChain.empty());resized();repaint();
}
void Editor::moveStage(int direction){editChain([&](auto& chain){for(int i=0;i<int(chain.size());++i)if(chain[size_t(i)].id==selectedStageId){auto to=i+direction;if(to>=0 && to<int(chain.size()))std::swap(chain[size_t(i)],chain[size_t(to)]);break;}});}
void Editor::duplicateStage(){juce::String id;editChain([&](auto& chain){if(chain.size()>=16)return;for(size_t i=0;i<chain.size();++i)if(chain[i].id==selectedStageId){auto copy=StageSpec::fromVar(chain[i].toVar());copy.id=juce::Uuid().toString();id=copy.id;chain.insert(chain.begin()+int(i)+1,std::move(copy));break;}});for(size_t i=0;i<visibleChain.size();++i)if(visibleChain[i].id==id)selectStage(int(i));}
void Editor::refreshLibrary() {
    try {
        auto selectedRow=libraryList.getSelectedRow();
        auto selectedId=selectedRow>=0 && selectedRow<int(filtered.size())?filtered[size_t(selectedRow)].id:juce::String();
        auto all=processor.services->library.entries();auto query=search.getText().trim();auto id=categoryFilter.getSelectedId();
        auto filter=id>=3 && id-3<categoryValues.size()?categoryValues[id-3]:juce::String();
        juce::StringArray categories;for(auto& m:all)categories.addIfNotAlreadyThere(m.category);categories.sort(true);
        categoryFilter.clear(juce::dontSendNotification);categoryFilter.addItem(ui::text("All models"),1);categoryFilter.addItem(ui::text("Favorites"),2);categoryValues=categories;for(int i=0;i<categories.size();++i)categoryFilter.addItem(ui::categoryLabel(categories[i]),i+3);
        if(id<=2)categoryFilter.setSelectedId(juce::jmax(1,id),juce::dontSendNotification);else{auto found=categoryValues.indexOf(filter);id=found>=0?found+3:1;categoryFilter.setSelectedId(id,juce::dontSendNotification);}
        filtered.clear();for(auto& m:all) {
            if(id==2 && !m.favorite)continue;if(id>2 && m.category!=filter)continue;
            if(query.isNotEmpty() && !(m.name+" "+m.category+" "+ui::categoryLabel(m.category)+" "+m.author+" "+m.tags.joinIntoString(" ")).containsIgnoreCase(query))continue;
            filtered.push_back(m);
        }
        std::sort(filtered.begin(),filtered.end(),[](const auto& a,const auto& b){return a.name.compareIgnoreCase(b.name)<0;});
        libraryList.updateContent();
        auto selected=std::find_if(filtered.begin(),filtered.end(),[&](const auto& m){return m.id==selectedId;});
        if(selected!=filtered.end()){auto row=int(std::distance(filtered.begin(),selected));libraryList.selectRow(row);selectModel(row);}
        else{libraryList.deselectAllRows();categoryEdit.clear();tagsEdit.clear();favoriteButton.setButtonText(ui::text("Favorite"));}
        version=processor.libraryVersion.load();libraryModified=processor.services->library.directory().getChildFile("library.json").getLastModificationTime().toMilliseconds();repaint();
    }catch(const std::exception& e){processor.setMessage(e.what());}
}
void Editor::selectModel(int row) {
    if(row<0 || row>=int(filtered.size()))return;auto& m=filtered[size_t(row)];categoryEdit.setText(ui::categoryLabel(m.category));tagsEdit.setText(m.tags.joinIntoString(", "));
    favoriteButton.setButtonText(ui::text(m.favorite?"Unfavorite":"Favorite"));
}

void Editor::selectStage(int row) {
    if(row<0 || row>=int(visibleChain.size()))return;auto& s=visibleChain[size_t(row)];selectedStageId=s.id;auto names=parameterNames(s.type);
    for(int i=0;i<4;++i){auto& k=knobs[size_t(i)];k.setEnabled(i<names.size());k.setVisible(!devicePageVisible && i<names.size());knobLabels[size_t(i)].setVisible(!devicePageVisible && i<names.size());knobLabels[size_t(i)].setText(i<names.size()?names[i]:"",juce::dontSendNotification);
        k.textFromValueFunction=[t=s.type,i](double value){return parameterText(t,i,value);};k.setValue(s.controls->values[size_t(i)].load(),juce::dontSendNotification);k.updateText();}
    for(size_t i=0;i<overview.size();++i)overview[i]->setToggleState(int(i)==row,juce::dontSendNotification);
    int left=12+row*200,right=left+176,view=chainViewport.getViewPositionX(),width=chainViewport.getMaximumVisibleWidth();
    if(left<view)chainViewport.setViewPosition(left,0);else if(right>view+width)chainViewport.setViewPosition(right-width+12,0);
    upButton.setEnabled(row>0);downButton.setEnabled(row+1<int(visibleChain.size()));repaint();board->repaint();
}
void Editor::editChain(const std::function<void(std::vector<StageSpec>&)>& fn){auto chain=processor.engine.chain();fn(chain);processor.engine.setChain(std::move(chain));syncChain();}
bool Editor::isInterestedInDragSource(const SourceDetails& details){return details.description.toString().startsWith("stage:");}
void Editor::itemDragMove(const SourceDetails& d){if(chainViewport.getBounds().contains(d.localPosition))chainViewport.autoScroll(d.localPosition.x-chainViewport.getX(),d.localPosition.y-chainViewport.getY(),32,18);}
void Editor::itemDropped(const SourceDetails& d) {
    if(!chainViewport.getBounds().contains(d.localPosition) || devicePageVisible)return;
    auto id=d.description.toString().fromFirstOccurrenceOf("stage:",false,false);
    auto x=d.localPosition.x-chainViewport.getX()+chainViewport.getViewPositionX();int target=juce::jmax(0,(x+88)/200);
    selectedStageId=id;editChain([&](auto& chain){auto it=std::find_if(chain.begin(),chain.end(),[&](const auto& s){return s.id==id;});if(it==chain.end())return;auto moving=*it;auto source=int(std::distance(chain.begin(),it));chain.erase(it);if(target>source)--target;chain.insert(chain.begin()+juce::jlimit(0,int(chain.size()),target),moving);});
}
void Editor::chooseImport() {
    chooser=std::make_unique<juce::FileChooser>(ui::text("Import NAM models, IRs or ZIP bundles"),juce::File(),"*.nam;*.wav;*.zip");
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::canSelectDirectories|juce::FileBrowserComponent::canSelectMultipleItems,
        scope.wrap([this](const juce::FileChooser& selected){juce::StringArray paths;for(auto& f:selected.getResults())paths.add(f.getFullPathName());if(!paths.isEmpty())processor.importFiles(paths);}));
}
void Editor::preset(bool save) {
    chooser=std::make_unique<juce::FileChooser>(ui::text(save?"Save ToneDock preset":"Open ToneDock preset"),juce::File(),"*.tonedock");
    chooser->launchAsync((save?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode)|juce::FileBrowserComponent::canSelectFiles,
        scope.wrap([this,save](const juce::FileChooser& selected){auto file=selected.getResult();if(file==juce::File())return;juce::MemoryBlock data;
            if(save){processor.getStateInformation(data);if(!file.withFileExtension("tonedock").replaceWithData(data.getData(),data.getSize()))processor.setMessage("Cannot save preset");}
            else if(file.getSize()<=1024*1024 && file.loadFileAsData(data))processor.setStateInformation(data.getData(),int(data.getSize()));}));
}
void Editor::openTone(int id) {
    juce::DialogWindow::LaunchOptions options;options.dialogTitle="ToneDock / TONE3000";options.dialogBackgroundColour=background;
    options.content.setOwned(new ToneBrowser(processor,id));options.componentToCentreAround=this;options.useNativeTitleBar=true;options.resizable=true;
    browser.reset(options.create());browser->setLookAndFeel(&theme);browser->setVisible(true);
}
void Editor::timerCallback() {
    if(version!=processor.libraryVersion.load())refreshLibrary();
    if(++refreshTick%15==0 && libraryModified!=processor.services->library.directory().getChildFile("library.json").getLastModificationTime().toMilliseconds())refreshLibrary();
    auto nextChain=processor.engine.chain();bool changed=nextChain.size()!=visibleChain.size();
    if(!changed)for(size_t i=0;i<nextChain.size();++i)if(nextChain[i].id!=visibleChain[i].id || nextChain[i].title!=visibleChain[i].title || nextChain[i].controls!=visibleChain[i].controls){changed=true;break;}
    if(changed)syncChain();
    auto state=processor.engine.status();displayStatus=state=="Ready"?processor.message():ui::message(state);if(displayStatus.isEmpty())displayStatus=ui::text("Ready / import a NAM model to begin");
    if(processor.getLatencySamples()!=processor.engine.latency())processor.setLatencySamples(processor.engine.latency());repaint();
}
}

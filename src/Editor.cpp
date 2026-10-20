#include "Editor.h"
#include "OnlineBrowser.h"

namespace tonedock {
namespace {
using ui::background;using ui::panel;using ui::ink;using ui::muted;using ui::accent;
void style(juce::TextEditor& e,const juce::String& placeholder) {
    ui::field(e,placeholder);e.setColour(juce::TextEditor::backgroundColourId,panel);
    e.setColour(juce::TextEditor::textColourId,ink);e.setColour(juce::TextEditor::outlineColourId,ui::line);
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
        g.setColour(selected?ui::selected:panel);g.fillRoundedRectangle(4,2,float(w-8),float(h-4),12);
        g.setColour(ink);g.setFont(ui::font(15));g.drawText((m.favorite?"* ":"")+m.name,16,8,w-32,27,juce::Justification::centredLeft);
        g.setColour(muted);g.setFont(ui::font(13));g.drawText(ui::categoryLabel(m.category)+"  /  "+ui::text(m.architecture),16,39,w-32,22,juce::Justification::centredLeft);
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
    void resized() override{power.setBounds(14,getHeight()-45,getWidth()-28,32);}
    void paint(juce::Graphics& g) override {
        bool selected=editor.selectedStageId==stage.id,enabled=stage.controls->enabled.load();
        auto colour=stage.type=="NAM"?accent:stage.type=="Drive"?juce::Colour(0xffc26412):
            stage.type=="Delay" || stage.type=="Reverb" || stage.type=="Chorus"?juce::Colour(0xff007b83):juce::Colour(0xff6554a4);
        auto area=getLocalBounds().toFloat().reduced(1.5f);
        g.setColour(selected?ui::selected:panel);g.fillRoundedRectangle(area,18);
        g.setColour(selected?accent:ui::line);g.drawRoundedRectangle(area,18,selected?1.8f:1.f);
        g.setColour(colour.withAlpha(enabled?1.f:.35f));g.fillRoundedRectangle(15,16,32,5,2);
        g.setColour(muted);g.setFont(ui::font(12));g.drawText(juce::String(number+1).paddedLeft('0',2),getWidth()-42,12,26,18,juce::Justification::centredRight);
        g.setColour(enabled?ink:muted);g.setFont(ui::font(19));g.drawText(ui::text(stage.type),15,37,getWidth()-30,29,juce::Justification::centredLeft);
        g.setColour(muted);g.setFont(ui::font(13));
        auto caption=stage.type=="NAM" || stage.type=="Cabinet IR"?
            (stage.file.isEmpty()?ui::text("点击选择网站音色"):stage.title):ui::text("内置效果 · 拖动调整顺序");
        auto art=editor.modelArtwork.find(stage.file);bool withArt=art!=editor.modelArtwork.end() && art->second.isNotEmpty();
        if(withArt)paintArtwork(g,{15,76,52,52},editor.artwork->get(art->second),stage.type=="Cabinet IR"?"cab":"amp");
        g.drawFittedText(caption,withArt?75:15,75,getWidth()-(withArt?90:30),55,juce::Justification::topLeft,3);
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
    void resized() override{for(size_t i=0;i<pedals.size();++i)pedals[i]->setBounds(12+int(i)*200,6,176,176);}
    void paint(juce::Graphics& g) override {
        g.setColour(ui::line);g.fillRect(0,91,getWidth(),2);
        g.setColour(muted);g.setFont(ui::font(18));for(size_t i=1;i<pedals.size();++i)g.drawText(ui::text("›"),int(i)*200-12,72,24,40,juce::Justification::centred);
        if(pedals.empty()){g.setColour(background);g.fillAll();g.setColour(muted);g.setFont(ui::font(16));g.drawText(ui::text("添加一个效果器，开始搭建你的声音"),20,75,getWidth()-40,40,juce::Justification::centred);}
    }
private:Editor& editor;std::vector<std::unique_ptr<Pedal>> pedals;
};

Editor::Editor(Processor& p,bool previewOnly):AudioProcessorEditor(p),processor(p),catalogDemo(previewOnly) {
    setLookAndFeel(&theme);
    for(auto* c:std::initializer_list<juce::Component*>{&libraryList,&chainViewport,&search,&categoryEdit,&tagsEdit,&link,&categoryFilter,&effectChoice,&importButton,&cloudButton,&loadButton,&favoriteButton,&metadataButton,&removeModelButton,&addButton,&deleteButton,&upButton,&downButton,&bypassStageButton,&audioButton,&saveButton,&openButton,&linkButton,&inputGain,&outputGain,&bypass,&workButton,&onlineButton,&duplicateButton,&libraryInfoButton,&modelBrowserButton})addAndMakeVisible(c);
    libraryList.setComponentID("library-list");search.setComponentID("library-search");categoryEdit.setComponentID("library-category");categoryFilter.setComponentID("library-filter");effectChoice.setComponentID("effect-choice");
    addButton.setComponentID("add-effect");deleteButton.setComponentID("delete-effect");duplicateButton.setComponentID("duplicate-effect");upButton.setComponentID("move-left");downButton.setComponentID("move-right");bypassStageButton.setComponentID("bypass-effect");audioButton.setComponentID("audio-page");onlineButton.setComponentID("local-models");workButton.setComponentID("work-page");chainViewport.setComponentID("chain-viewport");modelBrowserButton.setComponentID("model-browser");
    workButton.setClickingTogglesState(false);audioButton.setButtonText(ui::text("设备"));workButton.setToggleState(true,juce::dontSendNotification);
    ui::buttonStyle(modelBrowserButton,"tonal");
    upButton.setButtonText(ui::text("前移"));downButton.setButtonText(ui::text("后移"));deleteButton.setButtonText(ui::text("删除"));bypassStageButton.setButtonText(ui::text("旁通"));
    ui::buttonStyle(workButton,"nav-chain");ui::buttonStyle(onlineButton,"nav-catalog");ui::buttonStyle(audioButton,"nav-device");ui::buttonStyle(cloudButton,"tonal");for(auto* b:{&loadButton,&addButton})ui::buttonStyle(*b,"primary");ui::buttonStyle(importButton,"tonal");ui::buttonStyle(deleteButton,"danger");ui::buttonStyle(removeModelButton,"danger");libraryInfoButton.setComponentID("library-info");libraryInfoButton.onClick=[this]{libraryInfoExpanded=!libraryInfoExpanded;libraryInfoButton.setToggleState(libraryInfoExpanded,juce::dontSendNotification);resized();};linkButton.setButtonText(ui::text("打开"));
    modelRows=std::make_unique<ModelRows>(*this);libraryList.setModel(modelRows.get());libraryList.setRowHeight(76);
    addAndMakeVisible(emptyLibrary);emptyLibrary.setText(ui::text("Drop .nam / IR .wav / .zip here\nor import from TONE3000"),juce::dontSendNotification);emptyLibrary.setFont(ui::font(14));emptyLibrary.setColour(juce::Label::textColourId,muted);emptyLibrary.setJustificationType(juce::Justification::centred);emptyLibrary.setInterceptsMouseClicks(false,false);
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
    audioButton.onClick=[this]{showDevicePage(true);};workButton.onClick=[this]{showDevicePage(false);};onlineButton.onClick=[this]{showOnlineLibrary(false);};modelBrowserButton.onClick=[this]{showOnlineLibrary(true);};
    saveButton.onClick=[this]{preset(true);};openButton.onClick=[this]{preset(false);};
    for(auto* slider:{&inputGain,&outputGain}){slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,70,24);slider->setTextValueSuffix(" dB");}
    inputAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"input",inputGain);
    outputAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters,"output",outputGain);
    bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters,"bypass",bypass);
    for(int i=0;i<4;++i){auto& k=knobs[size_t(i)];addAndMakeVisible(k);addAndMakeVisible(knobLabels[size_t(i)]);k.setComponentID("stage-knob-"+juce::String(i));k.setRange(0,1,.001);k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,110,24);
        knobLabels[size_t(i)].setFont(ui::font(14));knobLabels[size_t(i)].setJustificationType(juce::Justification::centred);k.onValueChange=[this,i]{for(auto& s:visibleChain)if(s.id==selectedStageId)s.controls->values[size_t(i)].store(float(knobs[size_t(i)].getValue()));};}
    processor.onOpenTone=[this](int id){openTone(id);};processor.onStageLoaded=[this](const juce::String& id){
        auto current=selectedStageId;auto chain=processor.engine.chain();
        bool appended=std::none_of(visibleChain.begin(),visibleChain.end(),[&](const auto& stage){return stage.id==id;});
        bool keepSelection=std::any_of(chain.begin(),chain.end(),[&](const auto& stage){return stage.id==current;});
        syncChain();if(current==id || !keepSelection || appended)for(size_t i=0;i<visibleChain.size();++i)if(visibleChain[i].id==id){selectStage(int(i));break;}
    };setResizable(true,true);setResizeLimits(1120,780,1920,1200);setSize(1360,880);
    refreshLibrary();syncChain();if(!visibleChain.empty())selectStage(juce::jmin(1,int(visibleChain.size())-1));startTimerHz(15);
}
Editor::~Editor(){processor.onStageLoaded=nullptr;processor.onOpenTone=nullptr;stopTimer();devicePage.reset();browser.reset();chainViewport.setViewedComponent(nullptr,false);board.reset();libraryList.setModel(nullptr);setLookAndFeel(nullptr);}
void Editor::showDevicePage(bool show) {
    if(show && !devicePage){
        if(processor.createAudioSettings)devicePage=processor.createAudioSettings();
        else {auto label=std::make_unique<juce::Label>();label->setText(ui::text("音频设备由宿主软件管理\n\n请在 DAW 的音频设置中选择接口、输入通道、采样率和缓冲大小。"),juce::dontSendNotification);label->setJustificationType(juce::Justification::centred);label->setFont(ui::font(19));devicePage=std::move(label);}
        addAndMakeVisible(*devicePage);devicePage->setComponentID("device-content");
    }
    devicePageVisible=show;
    for(int i=0;i<getNumChildComponents();++i){auto* c=getChildComponent(i);if(c==&workButton || c==&onlineButton || c==&audioButton || c==&cloudButton || c==&saveButton || c==&openButton || c==devicePage.get() || c==browser.get())continue;c->setVisible(!show);}
    if(devicePage)devicePage->setVisible(show);
    workButton.setToggleState(!show,juce::dontSendNotification);onlineButton.setToggleState(false,juce::dontSendNotification);audioButton.setToggleState(show,juce::dontSendNotification);
    if(!show)for(size_t i=0;i<visibleChain.size();++i)if(visibleChain[i].id==selectedStageId)selectStage(int(i));
    if(visibleChain.empty())modelBrowserButton.setVisible(false);
    for(auto* c:std::initializer_list<juce::Component*>{&libraryList,&search,&categoryEdit,&tagsEdit,&link,&categoryFilter,&importButton,&loadButton,&favoriteButton,&metadataButton,&removeModelButton,&linkButton})c->setVisible(!show && !onlineVisible);
    if(browser)browser->setVisible(!show && onlineVisible);
    emptyLibrary.setVisible(!show && !onlineVisible && filtered.empty());
    resized();repaint();
}
void Editor::paint(juce::Graphics& g) {
    int w=getWidth(),h=getHeight(),x=workspaceX(),rw=w-x-24;
    g.fillAll(background);g.setColour(panel);g.fillRect(0,0,w,76);
    g.setColour(accent);g.fillRoundedRectangle(24,18,40,40,12);g.setColour(panel);ui::icon(g,{32,26,24,24},3);
    g.setColour(ink);g.setFont(ui::font(26));g.drawText("ToneDock",104,15,180,38,juce::Justification::centredLeft);
    g.setColour(muted);g.setFont(ui::font(14));g.drawText(ui::text("吉他音色工作台"),286,23,200,25,juce::Justification::centredLeft);
    g.setColour(muted);g.setFont(ui::font(12));g.drawText(JUCE_APPLICATION_VERSION_STRING,12,h-65,60,26,juce::Justification::centred);
    if(!devicePageVisible){
        if(!onlineVisible){
            ui::card(g,{104,96,280,h-136});g.setColour(ink);g.setFont(ui::font(21));g.drawText(ui::text("模型库"),124,112,150,30,juce::Justification::centredLeft);
            g.setColour(muted);g.setFont(ui::font(13));g.drawText(juce::String(filtered.size())+ui::text(" 个模型"),272,117,90,24,juce::Justification::centredRight);
        }
        g.setColour(ink);g.setFont(ui::font(25));g.drawText(ui::text("效果链"),x,102,180,34,juce::Justification::centredLeft);
        g.setColour(muted);g.setFont(ui::font(14));g.drawText(juce::String(visibleChain.size())+ui::text(" / 16 个效果 · 拖动排序，点击编辑"),x,139,juce::jmax(180,rw-20),24,juce::Justification::centredLeft);
        ui::card(g,{x,424,rw,h-574});ui::card(g,{x,h-132,rw,84});
        for(auto& stage:visibleChain)if(stage.id==selectedStageId){
            g.setColour(ink);g.setFont(ui::font(20));g.drawText(ui::text(stage.type),x+24,439,juce::jmax(100,rw-410),34,juce::Justification::centredLeft);
            if(stage.type=="NAM" || stage.type=="Cabinet IR"){
                g.setColour(muted);g.setFont(ui::font(15));auto note=stage.file.isEmpty()?ui::text("从左侧网站音色中选择，直接加载到当前效果器。"):stage.title;
                note+="\n\n"+ui::text("音色自动缓存，再次打开可直接使用。");
                g.drawFittedText(note,x+190,494,rw-214,juce::jmax(60,h-710),juce::Justification::topLeft,4);
            }
        }
        if(visibleChain.empty()){g.setColour(muted);g.setFont(ui::font(16));g.drawText(ui::text("添加效果器，开始搭建你的声音"),x+24,486,rw-48,90,juce::Justification::centred);}
        g.setColour(muted);g.setFont(ui::font(14));g.drawText(ui::text("输入"),x+22,h-121,46,30,juce::Justification::centredLeft);g.drawText(ui::text("输出"),x+rw/2-24,h-121,46,30,juce::Justification::centredLeft);
        ui::meter(g,{x+24,h-67,rw/2-90,5},processor.inputPeak.load());ui::meter(g,{x+rw/2-22,h-67,rw/2-144,5},processor.outputPeak.load());
    }
    g.setColour(muted);g.setFont(ui::font(13));g.drawText(displayStatus,104,h-34,w-388,26,juce::Justification::centredLeft);
    g.drawText(juce::String(processor.engine.latency())+ui::text(" samples DSP latency"),w-268,h-34,244,26,juce::Justification::centredRight);
}
void Editor::resized() {
    int w=getWidth(),h=getHeight(),x=workspaceX(),rw=w-x-24;
    workButton.setBounds(8,108,72,72);onlineButton.setBounds(8,196,72,72);audioButton.setBounds(8,284,72,72);
    openButton.setBounds(w-372,18,108,40);saveButton.setBounds(w-254,18,108,40);cloudButton.setBounds(w-136,18,112,40);
    if(devicePage)devicePage->setBounds(104,96,w-128,h-136);
    if(browser){browser->setBounds(104,96,392,h-136);if(onlineVisible)browser->toFront(false);}
    search.setBounds(122,154,244,40);categoryFilter.setBounds(122,208,244,38);
    int extra=libraryInfoExpanded?192:0;libraryList.setBounds(114,260,260,h-482-extra);emptyLibrary.setBounds(libraryList.getBounds().reduced(14));
    loadButton.setBounds(122,h-208-extra,244,40);favoriteButton.setBounds(122,h-158-extra,112,36);libraryInfoButton.setBounds(244,h-158-extra,122,36);importButton.setBounds(122,h-106,244,40);
    categoryEdit.setBounds(122,h-302,244,36);tagsEdit.setBounds(122,h-256,244,36);metadataButton.setBounds(122,h-210,158,36);removeModelButton.setBounds(288,h-210,78,36);removeModelButton.setButtonText(ui::text("移出库"));
    link.setBounds(122,h-164,172,36);linkButton.setBounds(302,h-164,64,36);
    for(auto* c:std::initializer_list<juce::Component*>{&categoryEdit,&tagsEdit,&metadataButton,&removeModelButton,&link,&linkButton})c->setVisible(!devicePageVisible && !onlineVisible && libraryInfoExpanded);libraryInfoButton.setVisible(!devicePageVisible && !onlineVisible);
    effectChoice.setBounds(w-308,104,178,40);addButton.setBounds(w-120,104,96,40);
    chainViewport.setBounds(x,204,rw,202);board->setSize(juce::jmax(rw,int(visibleChain.size())*200+12),188);
    int count=int(overview.size());for(int i=0;i<count;++i){int left=x+i*rw/juce::jmax(1,count),right=x+(i+1)*rw/juce::jmax(1,count);overview[size_t(i)]->setBounds(left,170,right-left-5,26);}
    int bx=w-400;duplicateButton.setBounds(bx,442,68,34);bypassStageButton.setBounds(bx+74,442,84,34);upButton.setBounds(bx+164,442,56,34);downButton.setBounds(bx+226,442,56,34);deleteButton.setBounds(bx+288,442,64,34);
    int kw=juce::jmin(172,(rw-48)/4),kh=juce::jmin(160,h-670);
    for(int i=0;i<4;++i){knobLabels[size_t(i)].setBounds(x+24+i*kw,490,kw,25);knobs[size_t(i)].setBounds(x+24+i*kw,518,kw,juce::jmax(100,kh));}
    modelBrowserButton.setBounds(x+190,h-204,170,40);
    inputGain.setBounds(x+68,h-120,rw/2-128,36);outputGain.setBounds(x+rw/2+22,h-120,rw/2-170,36);bypass.setBounds(w-150,h-117,120,32);
}
void Editor::loadSelectedModel(){auto row=libraryList.getSelectedRow();if(row>=0 && row<int(filtered.size())){
    auto type=filtered[size_t(row)].kind=="ir"?juce::String("Cabinet IR"):juce::String("NAM");auto target=selectedStageId;
    auto selected=std::find_if(visibleChain.begin(),visibleChain.end(),[&](const auto& s){return s.id==target && s.type==type;});
    if(selected==visibleChain.end()){target.clear();for(auto& s:visibleChain)if(s.type==type){target=s.id;break;}}
    processor.loadModel(filtered[size_t(row)],target);syncChain();
    for(size_t i=0;i<visibleChain.size();++i)if(visibleChain[i].type==type && (target.isEmpty() || visibleChain[i].id==target)){selectStage(int(i));break;}
}}
void Editor::syncChain() {
    visibleChain=processor.engine.chain();
    if(browser && std::none_of(visibleChain.begin(),visibleChain.end(),[this](const auto& stage){return stage.id==browser->boundStageId();}))browser->clearStageBinding();
    board->rebuild();overview.clear();
    for(size_t i=0;i<visibleChain.size();++i){auto b=std::make_unique<juce::TextButton>(juce::String(int(i)+1).paddedLeft('0',2)+(visibleChain.size()<=8?"  "+ui::text(visibleChain[i].type):juce::String()));ui::buttonStyle(*b,"compact");b->setComponentID("overview:"+visibleChain[i].id);b->setTooltip(ui::text(visibleChain[i].type)+ui::text(" · ")+visibleChain[i].title);b->onClick=[this,i]{selectStage(int(i));};addAndMakeVisible(*b);b->setVisible(!devicePageVisible);overview.push_back(std::move(b));}
    bool found=false;for(size_t i=0;i<visibleChain.size();++i)if(visibleChain[i].id==selectedStageId){selectStage(int(i));found=true;break;}
    if(!found){selectedStageId.clear();if(!visibleChain.empty())selectStage(0);else{
        for(int i=0;i<4;++i){knobs[size_t(i)].setVisible(false);knobLabels[size_t(i)].setVisible(false);}
        modelBrowserButton.setVisible(false);onlineVisible=false;if(browser){browser->clearStageBinding();browser->setVisible(false);}showDevicePage(devicePageVisible);
    }}
    if(visibleChain.empty()){upButton.setEnabled(false);downButton.setEnabled(false);}
    addButton.setEnabled(visibleChain.size()<16);duplicateButton.setEnabled(!visibleChain.empty() && visibleChain.size()<16);deleteButton.setEnabled(!visibleChain.empty());bypassStageButton.setEnabled(!visibleChain.empty());resized();repaint();
}
void Editor::moveStage(int direction){editChain([&](auto& chain){for(int i=0;i<int(chain.size());++i)if(chain[size_t(i)].id==selectedStageId){auto to=i+direction;if(to>=0 && to<int(chain.size()))std::swap(chain[size_t(i)],chain[size_t(to)]);break;}});}
void Editor::duplicateStage(){juce::String id;editChain([&](auto& chain){if(chain.size()>=16)return;for(size_t i=0;i<chain.size();++i)if(chain[i].id==selectedStageId){auto copy=StageSpec::fromVar(chain[i].toVar());copy.id=juce::Uuid().toString();id=copy.id;chain.insert(chain.begin()+int(i)+1,std::move(copy));break;}});for(size_t i=0;i<visibleChain.size();++i)if(visibleChain[i].id==id)selectStage(int(i));}
void Editor::refreshLibrary() {
    try {
        auto selectedRow=libraryList.getSelectedRow();
        auto selectedId=selectedRow>=0 && selectedRow<int(filtered.size())?filtered[size_t(selectedRow)].id:juce::String();
        auto all=processor.services->library.entries();modelArtwork.clear();for(auto& m:all)if(m.artworkUrl.isNotEmpty())modelArtwork[processor.services->library.fileFor(m).getFullPathName()]=m.artworkUrl;auto query=search.getText().trim();auto id=categoryFilter.getSelectedId();
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
        libraryList.updateContent();emptyLibrary.setVisible(filtered.empty() && !devicePageVisible && !onlineVisible);
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
    if(row<0 || row>=int(visibleChain.size()))return;auto& s=visibleChain[size_t(row)];bool changed=selectedStageId!=s.id;selectedStageId=s.id;auto names=parameterNames(s.type);
    for(int i=0;i<4;++i){auto& k=knobs[size_t(i)];k.setEnabled(i<names.size());k.setVisible(!devicePageVisible && i<names.size());knobLabels[size_t(i)].setVisible(!devicePageVisible && i<names.size());knobLabels[size_t(i)].setText(i<names.size()?names[i]:"",juce::dontSendNotification);
        k.textFromValueFunction=[t=s.type,i](double value){return parameterText(t,i,value);};k.setValue(s.controls->values[size_t(i)].load(),juce::dontSendNotification);k.updateText();}
    for(size_t i=0;i<overview.size();++i)overview[i]->setToggleState(int(i)==row,juce::dontSendNotification);
    bool modelStage=s.type=="NAM" || s.type=="Cabinet IR";
    modelBrowserButton.setVisible(!devicePageVisible && modelStage);modelBrowserButton.setButtonText(ui::text(s.type=="Cabinet IR"?"选择在线箱体":"选择在线音色"));
    if(changed && !devicePageVisible)showOnlineLibrary(modelStage);
    else if(onlineVisible && modelStage && browser)browser->bindStage(s.id,s.type=="Cabinet IR"?"ir":"nam",juce::String(row+1).paddedLeft('0',2)+"  "+ui::text(s.type));
    int left=12+row*200,right=left+176,view=chainViewport.getViewPositionX(),width=chainViewport.getMaximumVisibleWidth();
    if(left<view)chainViewport.setViewPosition(left,0);else if(right>view+width)chainViewport.setViewPosition(right-width+12,0);
    upButton.setEnabled(row>0);downButton.setEnabled(row+1<int(visibleChain.size()));repaint();board->repaint();
}
void Editor::editChain(const std::function<void(std::vector<StageSpec>&)>& fn){auto chain=processor.engine.chain();fn(chain);processor.engine.setChain(std::move(chain));syncChain();}
bool Editor::isInterestedInDragSource(const SourceDetails& details){return details.description.toString().startsWith("stage:") || details.description.toString().startsWith("t3k-tone:");}
void Editor::itemDragMove(const SourceDetails& d){if(chainViewport.getBounds().contains(d.localPosition))chainViewport.autoScroll(d.localPosition.x-chainViewport.getX(),d.localPosition.y-chainViewport.getY(),32,18);}
void Editor::itemDropped(const SourceDetails& d) {
    auto description=d.description.toString();
    if(description.startsWith("t3k-tone:")){
        if(devicePageVisible || d.localPosition.x<workspaceX())return;
        auto parts=juce::StringArray::fromTokens(description,":","");if(parts.size()==3 && parts[1].containsOnly("0123456789") && parts[2].containsOnly("0123456789"))loadOnlineAt(parts[1].getIntValue(),parts[2].getIntValue(),d.localPosition);return;
    }
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
void Editor::ensureBrowser(){
    if(browser)return;
    browser=std::make_unique<OnlineBrowser>(processor,[this](const juce::String& format){
        auto type=format=="ir"?juce::String("Cabinet IR"):juce::String("NAM");
        for(auto& s:visibleChain)if(s.id==selectedStageId && s.type==type)return s.id;return juce::String();
    },std::function<void()>{},catalogDemo);browser->setComponentID("stage-browser");addAndMakeVisible(*browser);
}
void Editor::showOnlineLibrary(bool show){
    if(show){
        auto selected=std::find_if(visibleChain.begin(),visibleChain.end(),[this](const auto& s){return s.id==selectedStageId && (s.type=="NAM" || s.type=="Cabinet IR");});
        if(selected==visibleChain.end())selected=std::find_if(visibleChain.begin(),visibleChain.end(),[](const auto& s){return s.type=="NAM" || s.type=="Cabinet IR";});
        if(selected==visibleChain.end()){
            if(visibleChain.size()>=16){processor.setMessage(ui::text("请先选择 NAM 或箱体 IR 效果器"));showDevicePage(false);return;}
            processor.addEffect("NAM");syncChain();selected=std::prev(visibleChain.end());
        }
        auto index=int(std::distance(visibleChain.begin(),selected));if(selectedStageId!=selected->id)selectStage(index);
    }
    if(show)ensureBrowser();
    if(show)for(size_t i=0;i<visibleChain.size();++i)if(visibleChain[i].id==selectedStageId){
        auto& stage=visibleChain[i];browser->bindStage(stage.id,stage.type=="Cabinet IR"?"ir":"nam",juce::String(int(i)+1).paddedLeft('0',2)+"  "+ui::text(stage.type));break;
    }
    onlineVisible=show;showDevicePage(false);
}
void Editor::showCatalogDemo(){catalogDemo=true;showOnlineLibrary(true);browser->showDemo();}
void Editor::showCatalogFilters(){showOnlineLibrary(true);browser->showFilters();}
void Editor::showCatalogDetails(){showOnlineLibrary(true);browser->showDetails();}
void Editor::focusOnlineTone(int id){showOnlineLibrary(true);if(processor.services->tone3000.isConnected() && id>0)browser->openTone(id,false);}
void Editor::openTone(int id){showOnlineLibrary(true);if(id>0)browser->openTone(id,true);}
void Editor::textDropped(const juce::String& text,int x,int y){if(auto id=parseToneLink(text))loadOnlineAt(*id,0,{x,y});}
void Editor::loadOnlineAt(int id,int modelId,juce::Point<int> position){
    if(devicePageVisible)return;juce::String target;
    for(size_t i=0;i<overview.size();++i)if(overview[i]->getBounds().contains(position)){target=visibleChain[i].id;break;}
    if(chainViewport.getBounds().contains(position)){
        auto point=position-chainViewport.getPosition()+chainViewport.getViewPosition();
        for(size_t i=0;i<visibleChain.size();++i)if(juce::Rectangle<int>(12+int(i)*200,6,176,176).contains(point)){target=visibleChain[i].id;break;}
    }
    if(target.isNotEmpty())for(size_t i=0;i<visibleChain.size();++i)if(visibleChain[i].id==target && (visibleChain[i].type=="NAM" || visibleChain[i].type=="Cabinet IR")){selectStage(int(i));break;}
    ensureBrowser();onlineVisible=true;showDevicePage(false);browser->loadTone(id,modelId,target);
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

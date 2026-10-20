#pragma once
#include "Catalog.h"
#include "CatalogMedia.h"
#include "Processor.h"
#include "AuthFlow.h"

namespace tonedock {
class OnlineBrowser : public juce::Component,private juce::ListBoxModel,private juce::ChangeListener {
public:
    explicit OnlineBrowser(Processor&,std::function<juce::String(const juce::String&)> targetForFormat={},std::function<void()> toggleLayout={},bool previewOnly=false);
    ~OnlineBrowser() override;
    void paint(juce::Graphics&) override;void resized() override;
    void openTone(int id,bool autoLoad=false,juce::String target={});
    // Drag/drop supplies its own destination; an empty target deliberately appends.
    void loadTone(int id,int modelId,juce::String target){catalog.select(id,!client.isConnected());load(id,modelId,target,false);}
    void bindStage(const juce::String& stageId,const juce::String& format,const juce::String& stageLabel);
    void clearStageBinding();
    const juce::String& boundStageId() const{return bindingStageId;}
    const juce::String& boundFormat() const{return bindingFormat;}
    void showDemo();
    void showFilters(){filtersExpanded=true;accountExpanded=false;embeddedDetail=false;resized();}
    void showDetails(){if(embedded){embeddedDetail=true;filtersExpanded=false;accountExpanded=false;resized();}}
private:
    int getNumRows() override{return int(visibleRows.size());}
    const t3k::Tone* toneAt(int row) const;
    bool acceptsTone(const t3k::Tone&) const;
    bool bindingIsValid() const;
    void updateBindingLabels();
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    juce::String getTooltipForRow(int) override;
    void selectedRowsChanged(int) override;
    void listBoxItemClicked(int row,const juce::MouseEvent&) override{selectedRowsChanged(row);}
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>&) override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override{list.repaint();repaint();}
    void searchCatalog();void update();void load(int id,int modelId=0,juce::String target={},bool useBinding=true);
    void replaceSelected();void stepCapture(int);void resetFilters();void chooseFilter(int);void rebuildChips();
    bool readyKey();void authenticate(t3k::FlowRequest::Kind,int id=0,juce::String target={},bool useBinding=true,int modelId=0);
    Processor& processor;t3k::Client& client;Catalog catalog;
    std::function<juce::String(const juce::String&)> chooseTarget;std::function<void()> toggleLayout;
    juce::SharedResourcePointer<CatalogArtwork> artwork;
    juce::TextEditor query,key,tags,makes,creators,detail;
    juce::ComboBox source,gear,format,architecture,sort,captures;
    juce::TextButton searchButton{ui::text("搜索")},more{ui::text("加载更多")},add{ui::text("新增效果")},replace{ui::text("加载 / 替换")},viewSource{ui::text("官网")},account{ui::text("账号")},filters{ui::text("筛选")},connect{ui::text("登录")},disconnect{ui::text("断开")},official{ui::text("官网选取")};
    juce::TextButton expand{ui::text("展开目录")},favorite{ui::text("收藏音色")},share{ui::text("复制链接")},clear{ui::text("清除")},tagPicker{ui::text("选标签")},makePicker{ui::text("选设备")},creatorPicker{ui::text("选作者")},previous{ui::text("上一页")},next{ui::text("下一页")},previousCapture{ui::text("‹")},nextCapture{ui::text("›")};
    juce::ToggleButton verified{ui::text("认证作者")},calibrated{ui::text("已校准")};
    juce::ListBox list;juce::Label count,state,empty,pageLabel,filterTitle,accountTitle,accountIntro,bindingTitle,bindingSubtitle;
    std::array<juce::Label,6> filterLabels;
    juce::Rectangle<int> coverBounds,headingBounds,detailBounds,resultsBounds;
    juce::Viewport filterViewport,accountViewport;juce::Component filterBody,accountBody;
    juce::Component chipStrip;juce::Viewport chipViewport;std::vector<std::unique_ptr<juce::TextButton>> chips;
    std::unique_ptr<t3k::AuthFlow> flow;
    bool accountExpanded=false,filtersExpanded=false,updating=false,started=false,demo=false,toneLoading=false;
    bool embedded=false,embeddedDetail=false;
    juce::String bindingStageId,bindingFormat,bindingStageLabel;
    std::vector<int> visibleRows;
    int bindingGeneration=0,unboundFormatId=1;
    int detailId=0,loadGeneration=0,activeToneId=0;juce::String activeStageId;
    t3k::AsyncScope callbacks;
};
}

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
    void loadTone(int id,int modelId,juce::String target){catalog.select(id,!client.isConnected());load(id,modelId,target);}
    void showDemo();
private:
    int getNumRows() override{return catalog.tones().size();}
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    juce::String getTooltipForRow(int) override;
    void selectedRowsChanged(int) override;
    void listBoxItemClicked(int row,const juce::MouseEvent&) override{selectedRowsChanged(row);}
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>&) override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override{list.repaint();repaint();}
    void searchCatalog();void update();void load(int id,int modelId=0,juce::String target={});
    void replaceSelected();void stepCapture(int);void resetFilters();void chooseFilter(int);void rebuildChips();
    bool readyKey();void authenticate(t3k::FlowRequest::Kind,int id=0,juce::String target={});
    Processor& processor;t3k::Client& client;Catalog catalog;
    std::function<juce::String(const juce::String&)> chooseTarget;std::function<void()> toggleLayout;
    juce::SharedResourcePointer<CatalogArtwork> artwork;
    juce::TextEditor query,key,tags,makes,creators,detail;
    juce::ComboBox source,gear,format,architecture,sort,captures;
    juce::TextButton searchButton{ui::text("搜索")},more{ui::text("加载更多")},add{ui::text("新增效果")},replace{ui::text("加载 / 替换")},viewSource{ui::text("官网")},account{ui::text("账号")},filters{ui::text("筛选")},connect{ui::text("登录")},disconnect{ui::text("断开")},official{ui::text("官网选取")};
    juce::TextButton expand{ui::text("展开目录")},favorite{ui::text("收藏音色")},share{ui::text("复制链接")},clear{ui::text("清除")},tagPicker{ui::text("选标签")},makePicker{ui::text("选设备")},creatorPicker{ui::text("选作者")},previous{ui::text("上一页")},next{ui::text("下一页")},previousCapture{ui::text("‹")},nextCapture{ui::text("›")};
    juce::ToggleButton verified{ui::text("认证作者")},calibrated{ui::text("已校准")};
    juce::ListBox list;juce::Label count,state,empty,pageLabel;juce::Rectangle<int> coverBounds,headingBounds;
    juce::Component chipStrip;juce::Viewport chipViewport;std::vector<std::unique_ptr<juce::TextButton>> chips;
    std::unique_ptr<t3k::AuthFlow> flow;
    bool accountExpanded=false,filtersExpanded=false,updating=false,started=false,demo=false;
    int detailId=0,loadGeneration=0,activeToneId=0;juce::String activeStageId;
    t3k::AsyncScope callbacks;
};
}

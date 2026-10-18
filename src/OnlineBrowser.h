#pragma once
#include "Catalog.h"
#include "Processor.h"
#include "AuthFlow.h"
#include "Theme.h"

namespace tonedock {
class OnlineBrowser : public juce::Component,private juce::ListBoxModel {
public:
    explicit OnlineBrowser(Processor&,std::function<juce::String(const juce::String&)> targetForFormat={});
    ~OnlineBrowser() override;
    void paint(juce::Graphics&) override;void resized() override;
    void openTone(int id,bool autoLoad=false,juce::String target={});
private:
    int getNumRows() override{return catalog.tones().size();}
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    void selectedRowsChanged(int) override;
    void listBoxItemDoubleClicked(int,const juce::MouseEvent&) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>&) override;
    void searchCatalog();void update();void load(int id,int modelId=0,juce::String target={});
    bool readyKey();void authenticate(t3k::FlowRequest::Kind,int id=0,juce::String target={});
    Processor& processor;t3k::Client& client;Catalog catalog;
    std::function<juce::String(const juce::String&)> chooseTarget;
    juce::TextEditor query,key,tags,makes,creators,detail;
    juce::ComboBox source,gear,format,architecture,sort,captures;
    juce::TextButton searchButton{ui::text("搜索")},more{ui::text("加载更多")},add{ui::text("加入效果链")},viewSource{ui::text("来源")},account{ui::text("账号")},filters{ui::text("筛选")},connect{ui::text("登录")},disconnect{ui::text("断开")},official{ui::text("官网选取")};
    juce::ToggleButton verified{ui::text("认证作者")};
    juce::ListBox list;juce::Label count,state,empty;
    std::unique_ptr<t3k::AuthFlow> flow;
    bool accountExpanded=false,filtersExpanded=false,updating=false,started=false;
    int detailId=0,loadGeneration=0;
    t3k::AsyncScope callbacks;
};
}

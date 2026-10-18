#include "OnlineBrowser.h"

namespace tonedock {
namespace {
juce::String catalogError(const t3k::Error& error){
    if(error.status==403)return ui::text("此应用暂未获得完整目录访问权限（HTTP 403）。");
    return ui::networkError(error.status,error.message);
}
juce::StringArray values(const juce::String& text){auto a=juce::StringArray::fromTokens(text,",","");a.trim();a.removeEmptyStrings();return a;}
}
OnlineBrowser::OnlineBrowser(Processor& p):processor(p),client(p.services->tone3000),catalog(Catalog::forClient(client)) {
    for(auto* c:std::initializer_list<juce::Component*>{&query,&key,&tags,&makes,&creators,&detail,&source,&gear,&format,&architecture,&sort,&captures,&searchButton,&more,&add,&viewSource,&account,&filters,&connect,&disconnect,&official,&verified,&list,&count,&state,&empty})addAndMakeVisible(c);
    setComponentID("online-browser");query.setComponentID("catalog-query");list.setComponentID("catalog-results");add.setComponentID("catalog-load");more.setComponentID("catalog-more");
    ui::field(query,"搜索名称，或粘贴 TONE3000 链接");ui::field(key,"t3k_pub_... 公开 Key");key.setText(client.publishableKey());key.setReadOnly(client.publishableKey().isNotEmpty());
    ui::field(tags,"标签，例如 clean,high-gain");ui::field(makes,"设备，例如 Fender Twin Reverb");ui::field(creators,"作者用户名，英文逗号分隔");
    ui::field(detail,"");detail.setMultiLine(true);detail.setReadOnly(true);detail.setFont(ui::font(12));detail.setIndents(6,4);
    for(auto s:{"完整目录","我的收藏","下载记录","我创建的","最新发布（10条）","热门精选"})source.addItem(ui::text(s),source.getNumItems()+1);source.setSelectedId(1);
    gear.addItem(ui::text("全部设备"),1);for(auto& g:t3k::gear::all)gear.addItem(ui::text(g),gear.getNumItems()+1);gear.setSelectedId(1);
    for(auto s:{"全部格式","NAM","IR","AIDA-X","AA Snapshot","Proteus"})format.addItem(ui::text(s),format.getNumItems()+1);format.setSelectedId(1);
    for(auto s:{"全部架构","A2","A1 / Custom"})architecture.addItem(ui::text(s),architecture.getNumItems()+1);architecture.setSelectedId(1);
    for(auto s:{"综合推荐","最新","最早","最多下载"})sort.addItem(ui::text(s),sort.getNumItems()+1);sort.setSelectedId(1);
    list.setModel(this);list.setRowHeight(64);list.setColour(juce::ListBox::backgroundColourId,ui::background);
    empty.setFont(ui::font(14));empty.setColour(juce::Label::textColourId,ui::muted);empty.setJustificationType(juce::Justification::centred);empty.setInterceptsMouseClicks(false,false);
    state.setFont(ui::font(12));state.setColour(juce::Label::textColourId,ui::muted);count.setFont(ui::font(12));count.setColour(juce::Label::textColourId,ui::muted);
    query.onReturnKey=[this]{searchCatalog();};searchButton.onClick=[this]{searchCatalog();};more.onClick=[this]{catalog.more();};
    for(auto* c:{&source,&gear,&format,&architecture,&sort})c->onChange=[this]{if(!updating)searchCatalog();};
    verified.onClick=[this]{searchCatalog();};for(auto* e:{&tags,&makes,&creators})e->onReturnKey=[this]{searchCatalog();};
    account.onClick=[this]{accountExpanded=!accountExpanded;resized();};filters.onClick=[this]{filtersExpanded=!filtersExpanded;resized();};
    connect.onClick=[this]{authenticate(t3k::FlowRequest::Kind::login);};official.onClick=[this]{authenticate(t3k::FlowRequest::Kind::selectTone);};
    disconnect.onClick=[this]{if(flow)flow->cancel();client.clearTokens();++loadGeneration;accountExpanded=true;state.setText(ui::text("Disconnected"),juce::dontSendNotification);update();resized();};
    add.onClick=[this]{auto id=catalog.selectedId();if(id>0)load(id,captures.getSelectedId());};
    viewSource.onClick=[this]{auto url=catalog.detail().tone.url;if(url.startsWith("https://"))juce::URL(url).launchInDefaultBrowser();};
    catalog.changed=[this]{update();};accountExpanded=!client.isConnected();
    if(client.isConnected())searchCatalog();else{empty.setText(ui::text("登录后，在应用内浏览完整在线目录。\n拖到右侧链路即可自动下载并加载。"),juce::dontSendNotification);update();}
}
OnlineBrowser::~OnlineBrowser(){if(flow)flow->cancel();catalog.changed=nullptr;list.setModel(nullptr);}
void OnlineBrowser::paint(juce::Graphics& g){
    ui::card(g,getLocalBounds());g.setColour(ui::ink);g.setFont(ui::font(19));g.drawText(ui::text("在线音色"),16,10,getWidth()-200,28,juce::Justification::centredLeft);
    g.setColour(ui::muted);g.setFont(ui::font(12));g.drawText("TONE3000",16,42,getWidth()-32,20,juce::Justification::centredLeft);
}
void OnlineBrowser::resized(){
    int w=getWidth(),h=getHeight(),top=accountExpanded?152:72;
    filters.setBounds(w-176,13,72,30);account.setBounds(w-96,13,80,30);
    key.setBounds(16,70,w-32,30);connect.setBounds(16,108,80,30);disconnect.setBounds(104,108,80,30);official.setBounds(192,108,w-208,30);
    for(auto* c:std::initializer_list<juce::Component*>{&key,&connect,&disconnect,&official})c->setVisible(accountExpanded);
    query.setBounds(16,top,w-106,34);searchButton.setBounds(w-82,top,66,34);
    int third=(w-44)/3;source.setBounds(16,top+44,third,30);gear.setBounds(22+third,top+44,third,30);format.setBounds(28+2*third,top+44,third,30);
    int half=(w-40)/2;architecture.setBounds(16,top+82,half,30);sort.setBounds(24+half,top+82,half,30);
    tags.setBounds(16,top+120,w-32,28);makes.setBounds(16,top+154,w-32,28);creators.setBounds(16,top+188,w-148,28);verified.setBounds(w-126,top+188,110,28);
    for(auto* c:std::initializer_list<juce::Component*>{&tags,&makes,&creators,&verified})c->setVisible(filtersExpanded);
    int listTop=top+124+(filtersExpanded?112:0),footer=h-190,bottom=footer-42;
    // Expanded account/filter sections trade result rows for controls, never overlap the footer.
    if(bottom-listTop<64){listTop=juce::jmax(top+124,bottom-64);if(filtersExpanded){tags.setVisible(false);makes.setVisible(false);creators.setVisible(false);verified.setVisible(false);}}
    list.setBounds(10,listTop,w-20,juce::jmax(1,bottom-listTop));empty.setBounds(list.getBounds().reduced(12));
    more.setBounds(16,bottom+6,122,28);count.setBounds(146,bottom+6,w-162,28);detail.setBounds(16,footer+4,w-32,62);captures.setBounds(16,footer+74,w-32,30);
    add.setBounds(16,footer+114,w-126,34);viewSource.setBounds(w-102,footer+114,86,34);state.setBounds(16,footer+153,w-32,32);
}
bool OnlineBrowser::readyKey(){
    if(client.publishableKey().isNotEmpty())return true;
    if(processor.services->configureKey(key.getText())){key.setReadOnly(true);return true;}
    accountExpanded=true;resized();state.setText(ui::text("请先填写 TONE3000 的 t3k_pub_ 公开 Key。"),juce::dontSendNotification);return false;
}
void OnlineBrowser::authenticate(t3k::FlowRequest::Kind kind,int id,juce::String target){
    if(!readyKey())return;if(flow)flow->cancel();flow=t3k::makeAuthFlow(t3k::FlowMode::systemBrowser);
    t3k::FlowRequest request;request.kind=kind;request.catalog.preview=true;if(id>0)request.toneId=juce::String(id);
    state.setText(ui::text("请在浏览器完成登录，完成后会自动返回。"),juce::dontSendNotification);
    try{flow->start(request,callbacks.wrap([this,target](t3k::FlowResult r){
        if(!r.connected()){state.setText(r.canceled()?ui::text("Canceled"):ui::message(r.error),juce::dontSendNotification);return;}
        accountExpanded=false;resized();searchCatalog();if(r.toneId.getIntValue()>0)openTone(r.toneId.getIntValue(),true,target);
    }));}catch(const std::exception& e){state.setText(ui::message(juce::String::fromUTF8(e.what())),juce::dontSendNotification);}
}
void OnlineBrowser::searchCatalog(){
    if(catalog.isLoading())return;
    if(!client.isConnected()){state.setText(ui::text("Connect TONE3000 first"),juce::dontSendNotification);return;}
    auto text=query.getText().trim();if(auto id=parseToneLink(text)){openTone(*id);return;}
    CatalogQuery q;q.source=static_cast<CatalogQuery::Source>(source.getSelectedId()-1);q.filters.query=text;q.filters.pageSize=25;
    auto gearId=gear.getSelectedId()-2;if(gearId>=0 && gearId<t3k::gear::all.size())q.filters.gears.add(t3k::gear::all[gearId]);
    const juce::StringArray formats{"","nam","ir","aida-x","aa-snapshot","proteus"};q.filters.format=formats[juce::jlimit(0,formats.size()-1,format.getSelectedId()-1)];
    q.architecture=architecture.getSelectedId()==2?2:architecture.getSelectedId()==3?1:0;
    const juce::StringArray sorts{q.filters.query.isEmpty()?"trending":"best-match","newest","oldest","downloads-all-time"};q.filters.sort=sorts[juce::jlimit(0,3,sort.getSelectedId()-1)];
    q.filters.tags=values(tags.getText());q.filters.makes=values(makes.getText());q.filters.creators=values(creators.getText());q.filters.verified=verified.getToggleState();
    started=true;catalog.search(std::move(q));
}
void OnlineBrowser::openTone(int id,bool autoLoad,juce::String target){
    if(!client.isConnected()){authenticate(t3k::FlowRequest::Kind::loadTone,id,target);return;}
    catalog.select(id);if(autoLoad){if(target.isEmpty())load(id);else processor.loadOnlineTone(id,target);}
}
void OnlineBrowser::update(){
    juce::ScopedValueSetter<bool> guard(updating,true);auto selectedId=catalog.selectedId();list.updateContent();
    for(int i=0;i<catalog.tones().size();++i)if(catalog.tones().getReference(i).id==selectedId){list.selectRow(i,true,true);break;}
    bool connected=client.isConnected();searchButton.setEnabled(connected && !catalog.isLoading());more.setEnabled(connected && catalog.hasMore() && !catalog.isLoading());
    bool full=source.getSelectedId()==1;bool canSearch=connected && !catalog.isLoading();source.setEnabled(canSearch);gear.setEnabled(canSearch);for(auto* c:std::initializer_list<juce::Component*>{&format,&architecture,&sort,&tags,&makes,&creators,&verified})c->setEnabled(full && canSearch);
    count.setText(ui::text("已显示 ")+juce::String(catalog.tones().size())+ui::text(" 个音色"),juce::dontSendNotification);
    auto& data=catalog.detail();bool ready=data.tone.id>0 && !catalog.isDetailLoading();
    if(data.tone.id!=detailId){detailId=data.tone.id;captures.clear(juce::dontSendNotification);
        if(ready){
            detail.setText(data.tone.title+"\n"+ui::text("作者：")+data.tone.user.creatorName()+"  /  "+ui::text("许可：")+t3k::licenseLabel(data.tone.license),false);
            detail.setTooltip(data.tone.description+"\n"+data.tone.url);
            for(auto& m:data.models)if(m.id>0 && ((data.tone.format=="nam" && m.extension()==".nam") || (data.tone.format=="ir" && m.extension()==".wav")))captures.addItem(m.name+(m.architectureVersion.isEmpty()?juce::String():"  ["+m.architectureVersion+"]"),m.id);
            if(captures.getNumItems()>0)captures.setSelectedId(captures.getItemId(0),juce::dontSendNotification);
        }else detail.clear();
    }
    bool playable=ready && (data.tone.format=="nam" || data.tone.format=="ir") && captures.getSelectedId()>0;add.setEnabled(playable);viewSource.setEnabled(ready);captures.setEnabled(playable);
    if(catalog.error().status || catalog.error().message.isNotEmpty())state.setText(catalogError(catalog.error()),juce::dontSendNotification);
    else if(catalog.detailError().status || catalog.detailError().message.isNotEmpty())state.setText(catalogError(catalog.detailError()),juce::dontSendNotification);
    else if(catalog.isLoading())state.setText(ui::text("正在读取目录…"),juce::dontSendNotification);
    else if(catalog.isDetailLoading())state.setText(ui::text("正在获取全部捕获…"),juce::dontSendNotification);
    else if(ready && !playable)state.setText(ui::text("该格式可浏览；当前支持直接加载 NAM / IR。"),juce::dontSendNotification);
    else state.setText(ui::text("双击音色或拖到右侧效果链，即可直接使用。"),juce::dontSendNotification);
    empty.setVisible(catalog.tones().isEmpty());
    empty.setText(ui::text(!connected?"登录后浏览完整在线目录。\n已有账号设置会自动保留。":catalog.isLoading()?"正在读取 TONE3000…":started?"没有结果。可调整筛选条件或稍后重试。":"搜索音色，或从完整目录开始浏览。"),juce::dontSendNotification);
    repaint();
}
void OnlineBrowser::paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool selected){
    if(row<0 || row>=getNumRows())return;auto& t=catalog.tones().getReference(row);
    g.setColour(selected?juce::Colour(0xff30462c):ui::panel);g.fillRoundedRectangle(4,3,float(w-8),float(h-6),7);
    g.setColour(ui::ink);g.setFont(ui::font(14));g.drawFittedText(t.title,12,8,w-24,24,juce::Justification::centredLeft,1);
    g.setColour(ui::muted);g.setFont(ui::font(12));g.drawText(t.format.toUpperCase()+" / "+ui::text(t.gear)+" / "+t.user.creatorName(),12,35,w-24,20,juce::Justification::centredLeft);
}
void OnlineBrowser::selectedRowsChanged(int row){if(!updating && row>=0 && row<getNumRows())catalog.select(catalog.tones().getReference(row).id);}
void OnlineBrowser::listBoxItemDoubleClicked(int row,const juce::MouseEvent&){if(row>=0 && row<getNumRows())load(catalog.tones().getReference(row).id);}
juce::var OnlineBrowser::getDragSourceDescription(const juce::SparseSet<int>& rows){if(rows.isEmpty())return {};auto row=rows[0];if(row<0 || row>=getNumRows())return {};auto id=catalog.tones().getReference(row).id;auto model=id==catalog.detail().tone.id?captures.getSelectedId():0;return "t3k-tone:"+juce::String(id)+":"+juce::String(model);}
void OnlineBrowser::load(int id,int model){
    auto ticket=++loadGeneration;state.setText(ui::text("正在下载、校验并加入链路…"),juce::dontSendNotification);
    processor.loadOnlineTone(id,{},model,callbacks.wrap([this,ticket](bool,juce::String result){if(ticket==loadGeneration)state.setText(result,juce::dontSendNotification);}));
}
}

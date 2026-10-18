#include "OnlineBrowser.h"

namespace tonedock {
namespace {
juce::String catalogError(const t3k::Error& e){return e.status==403?ui::text("此应用暂未获得目录访问权限（HTTP 403）。"):ui::networkError(e.status,e.message);}
juce::StringArray values(const juce::String& text){auto a=juce::StringArray::fromTokens(text,",","");a.trim();a.removeEmptyStrings();return a;}
juce::String terms(const juce::Array<t3k::Taxonomy>& list){juce::StringArray names;for(auto& t:list)names.add(t.name);return names.joinIntoString(", ");}
juce::String badge(const t3k::Tone& t){return t.format.toUpperCase()+(t.format=="nam" && t.a2ModelsCount>=0?(t.a2ModelsCount>0?" · A2":" · A1 / Custom"):juce::String());}
juce::String description(const t3k::Tone& t){return toneFacts(t)+"\n"+ui::text("发布：")+t.publishedAt.substring(0,10)+"\n\n"+ui::text("音色说明\n")+(t.description.isEmpty()?ui::text("作者尚未填写说明。"):t.description)+"\n\n"+ui::text("设备 / 型号\n")+terms(t.makes)+"\n\n"+ui::text("标签\n")+terms(t.tags)+"\n\n"+ui::text("许可证：")+t3k::licenseLabel(t.license)+"\n"+t.url;}

class FilterPicker : public juce::Component,private juce::ListBoxModel,private juce::Timer {
public:
    FilterPicker(t3k::Client& c,int k,std::function<void(juce::String)> chosen):client(c),kind(k),select(std::move(chosen)){
        setLookAndFeel(&theme);setSize(360,360);ui::field(query,"搜索并选择，可添加多个筛选项");addAndMakeVisible(query);addAndMakeVisible(list);addAndMakeVisible(status);list.setModel(this);list.setRowHeight(38);query.onTextChange=[this]{startTimer(400);};query.onReturnKey=[this]{stopTimer();fetch();};status.setColour(juce::Label::textColourId,ui::muted);fetch();
    }
    ~FilterPicker() override{stopTimer();list.setModel(nullptr);setLookAndFeel(nullptr);}
    void resized() override{query.setBounds(12,12,getWidth()-24,34);list.setBounds(8,54,getWidth()-16,getHeight()-92);status.setBounds(12,getHeight()-32,getWidth()-24,24);}
    void paint(juce::Graphics& g) override{g.fillAll(ui::panel);}
private:
    int getNumRows() override{return names.size();}
    void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool selected) override{g.setColour(selected?ui::line:ui::panel);g.fillAll();g.setColour(ui::ink);g.setFont(ui::font(14));if(row>=0 && row<names.size())g.drawText(names[row],10,0,w-20,h,juce::Justification::centredLeft);}
    void listBoxItemClicked(int row,const juce::MouseEvent&) override{if(row>=0 && row<keys.size()){select(keys[row]);if(auto* box=findParentComponentOfClass<juce::CallOutBox>())box->dismiss();}}
    void timerCallback() override{stopTimer();fetch();}
    void fetch(){auto ticket=++generation;names.clear();keys.clear();list.updateContent();status.setText(ui::text("正在读取选项…"),juce::dontSendNotification);
        if(kind==2){t3k::ListUsersParams p;p.query=query.getText().trim();p.pageSize=25;p.sort="downloads";client.listUsers(p,scope.wrap([this,ticket](auto r){if(ticket!=generation)return;if(!r.ok()){failed(r.error);return;}for(auto& u:r->data){names.add(u.creatorName()+"  @"+u.username+(u.isVerified?ui::text(" · 认证"):juce::String()));keys.add(u.username);}done();}));}
        else{t3k::ListTaxonomyParams p;p.query=query.getText().trim();p.pageSize=25;p.sort="tones";auto cb=scope.wrap([this,ticket](t3k::Result<t3k::Page<t3k::Taxonomy>> r){if(ticket!=generation)return;if(!r.ok()){failed(r.error);return;}for(auto& t:r->data){names.add(t.name+"  ("+juce::String(t.tonesCount)+")");keys.add(t.name);}done();});if(kind==0)client.listTags(p,cb);else client.listMakes(p,cb);}
    }
    void done(){list.updateContent();status.setText(ui::text("点击加入筛选；可继续选择其他项。"),juce::dontSendNotification);}
    void failed(const t3k::Error& e){status.setText(catalogError(e),juce::dontSendNotification);}
    ui::Theme theme;t3k::Client& client;int kind,generation=0;std::function<void(juce::String)> select;juce::TextEditor query;juce::ListBox list;juce::Label status;juce::StringArray names,keys;t3k::AsyncScope scope;
};
}

OnlineBrowser::OnlineBrowser(Processor& p,std::function<juce::String(const juce::String&)> targetForFormat,std::function<void()> layout,bool previewOnly):processor(p),client(p.services->tone3000),catalog(Catalog::forClient(client,p.services->library.directory())),chooseTarget(std::move(targetForFormat)),toggleLayout(std::move(layout)),demo(previewOnly){
    for(auto* c:std::initializer_list<juce::Component*>{&query,&key,&tags,&makes,&creators,&detail,&source,&gear,&format,&architecture,&sort,&captures,&searchButton,&more,&add,&replace,&viewSource,&account,&filters,&connect,&disconnect,&official,&verified,&calibrated,&list,&count,&state,&empty,&expand,&favorite,&share,&clear,&tagPicker,&makePicker,&creatorPicker,&previous,&next,&pageLabel,&previousCapture,&nextCapture})addAndMakeVisible(c);
    addAndMakeVisible(chipViewport);chipViewport.setViewedComponent(&chipStrip,false);chipViewport.setScrollBarsShown(false,true);chipViewport.setScrollBarThickness(3);
    setComponentID("online-browser");query.setComponentID("catalog-query");list.setComponentID("catalog-results");replace.setComponentID("catalog-load");more.setComponentID("catalog-more");expand.setComponentID("catalog-expand");favorite.setComponentID("catalog-favorite");sort.setComponentID("catalog-sort");detail.setComponentID("catalog-detail");captures.setComponentID("catalog-captures");
    ui::field(query,"搜索音色、设备、标签、作者，或粘贴链接");ui::field(key,"t3k_pub_... 公开 Key");key.setText(client.publishableKey());key.setReadOnly(client.publishableKey().isNotEmpty());
    ui::field(tags,"标签，英文逗号分隔");ui::field(makes,"品牌 / 型号，英文逗号分隔");ui::field(creators,"作者用户名，英文逗号分隔");ui::field(detail,"");detail.setMultiLine(true);detail.setReadOnly(true);detail.setFont(ui::font(13));detail.setIndents(8,7);
    for(auto s:{"完整目录","我的收藏","下载记录","我创建的","最新发布（10条）","热门精选","最近使用"})source.addItem(ui::text(s),source.getNumItems()+1);source.setSelectedId(1);
    gear.addItem(ui::text("全部设备"),1);for(auto& g:t3k::gear::all)gear.addItem(ui::text(g),gear.getNumItems()+1);gear.setSelectedId(1);
    for(auto s:{"全部格式","NAM","IR","AIDA-X","AA Snapshot","Proteus"})format.addItem(ui::text(s),format.getNumItems()+1);format.setSelectedId(1);
    for(auto s:{"全部架构","A2","A1 / Custom"})architecture.addItem(ui::text(s),architecture.getNumItems()+1);architecture.setSelectedId(1);
    for(auto s:{"最佳匹配","近期热门","下载最多","最新发布","最早发布"})sort.addItem(ui::text(s),sort.getNumItems()+1);sort.setSelectedId(2);
    sort.setTooltip(ui::text("近期热门使用 TONE3000 Trending 排序；下载最多使用累计下载量。"));list.setModel(this);list.setRowHeight(122);list.setColour(juce::ListBox::backgroundColourId,ui::background);list.setTooltip(ui::text("单击看详情；双击加载；拖到效果卡片替换。"));
    for(auto* l:{&empty,&count,&state,&pageLabel}){l->setFont(ui::font(12));l->setColour(juce::Label::textColourId,ui::muted);}empty.setFont(ui::font(14));empty.setJustificationType(juce::Justification::centred);empty.setInterceptsMouseClicks(false,false);state.setMinimumHorizontalScale(.9f);
    query.onReturnKey=[this]{searchCatalog();};searchButton.onClick=[this]{searchCatalog();};more.onClick=[this]{catalog.more();};previous.onClick=[this]{catalog.jump(catalog.page()-1);};next.onClick=[this]{catalog.jump(catalog.page()+1);};
    for(auto* c:{&source,&gear,&format,&architecture,&sort})c->onChange=[this]{if(!updating)searchCatalog();};verified.onClick=[this]{searchCatalog();};calibrated.onClick=[this]{searchCatalog();};for(auto* e:{&tags,&makes,&creators})e->onReturnKey=[this]{searchCatalog();};
    filters.onClick=[this]{filtersExpanded=!filtersExpanded;if(filtersExpanded)accountExpanded=false;resized();};account.onClick=[this]{accountExpanded=!accountExpanded;if(accountExpanded)filtersExpanded=false;resized();};expand.onClick=[this]{if(toggleLayout)toggleLayout();};
    clear.onClick=[this]{resetFilters();};tagPicker.onClick=[this]{chooseFilter(0);};makePicker.onClick=[this]{chooseFilter(1);};creatorPicker.onClick=[this]{chooseFilter(2);};
    connect.onClick=[this]{authenticate(t3k::FlowRequest::Kind::login);};official.onClick=[this]{authenticate(t3k::FlowRequest::Kind::selectTone);};disconnect.onClick=[this]{if(flow)flow->cancel();client.clearTokens();++loadGeneration;accountExpanded=true;catalog.clear();state.setText(ui::text("Disconnected"),juce::dontSendNotification);resized();};
    replace.onClick=[this]{replaceSelected();};add.onClick=[this]{auto id=catalog.selectedId();if(id>0)load(id,captures.getSelectedId());};
    favorite.onClick=[this]{auto& t=catalog.detail().tone;if(t.id>0)catalog.setFavorite(t.id,!t.isFavorite);};share.onClick=[this]{auto url=catalog.detail().tone.url;if(url.startsWith("https://")){juce::SystemClipboard::copyTextToClipboard(url);state.setText(ui::text("已复制音色链接"),juce::dontSendNotification);}};
    viewSource.onClick=[this]{auto url=catalog.detail().tone.url;if(url.startsWith("https://"))juce::URL(url).launchInDefaultBrowser();};
    captures.onChange=[this]{if(updating)return;auto id=catalog.detail().tone.id;if(id>0 && id==activeToneId && activeStageId.isNotEmpty())load(id,captures.getSelectedId(),activeStageId);update();};previousCapture.onClick=[this]{stepCapture(-1);};nextCapture.onClick=[this]{stepCapture(1);};
    artwork->addChangeListener(this);catalog.changed=[this]{update();};accountExpanded=!demo && !client.isConnected();if(!demo && client.isConnected())searchCatalog();else update();
}
OnlineBrowser::~OnlineBrowser(){artwork->removeChangeListener(this);if(flow)flow->cancel();catalog.changed=nullptr;list.setModel(nullptr);chipViewport.setViewedComponent(nullptr,false);}
void OnlineBrowser::paint(juce::Graphics& g){
    ui::card(g,getLocalBounds());g.setColour(ui::ink);g.setFont(ui::font(19));g.drawText(ui::text("在线音色"),16,10,juce::jmax(80,getWidth()-320),28,juce::Justification::centredLeft);g.setColour(ui::muted);g.setFont(ui::font(12));g.drawText(demo?ui::text("界面演示 · 数据为测试夹具"):ui::text("TONE3000 · 封面、热度与捕获"),16,43,getWidth()-32,20,juce::Justification::centredLeft);
    auto& t=catalog.detail().tone;if(t.id>0 && !coverBounds.isEmpty()){
        paintArtwork(g,coverBounds,artwork->get(t.images.isEmpty()?juce::String():t.images[0]),t.gear);g.setColour(ui::ink);g.setFont(ui::font(17));g.drawFittedText(t.title,headingBounds.getX(),headingBounds.getY(),headingBounds.getWidth(),46,juce::Justification::topLeft,2);
        g.setColour(ui::muted);g.setFont(ui::font(12));g.drawText(badge(t)+" · "+ui::text(t.gear),headingBounds.getX(),headingBounds.getY()+48,headingBounds.getWidth(),19,juce::Justification::centredLeft);g.drawText(ui::text("下载 ")+compactCount(t.downloadsCount)+ui::text(" · 收藏 ")+compactCount(t.favoritesCount),headingBounds.getX(),headingBounds.getY()+68,headingBounds.getWidth(),19,juce::Justification::centredLeft);
        g.drawText(t.user.creatorName()+(t.user.isVerified?ui::text(" · 认证"):juce::String())+" · "+publishedLabel(t.publishedAt),headingBounds.getX(),headingBounds.getY()+90,headingBounds.getWidth(),19,juce::Justification::centredLeft);
    }
    if(getWidth()<760){g.setColour(ui::ink);g.setFont(ui::font(12));g.drawFittedText(t.id>0?t.title:ui::text("选择音色查看捕获；展开目录可看完整详情"),16,getHeight()-182,getWidth()-32,32,juce::Justification::centredLeft,2);}
}
void OnlineBrowser::resized(){
    int w=getWidth(),h=getHeight();bool wide=w>=760;int dw=wide?juce::jlimit(320,420,w*32/100):0,lw=wide?w-dw-28:w,dx=lw+12,top=accountExpanded?150:72;
    expand.setBounds(w-296,13,96,30);filters.setBounds(w-192,13,80,30);account.setBounds(w-104,13,88,30);expand.setButtonText(ui::text(wide?"显示效果链":"展开目录"));
    key.setBounds(16,70,w-32,30);connect.setBounds(16,108,80,30);disconnect.setBounds(104,108,80,30);official.setBounds(192,108,w-208,30);for(auto* c:std::initializer_list<juce::Component*>{&key,&connect,&disconnect,&official})c->setVisible(accountExpanded);
    query.setBounds(16,top,lw-106,34);searchButton.setBounds(lw-82,top,66,34);int third=(lw-44)/3;source.setBounds(16,top+44,third,30);gear.setBounds(22+third,top+44,third,30);format.setBounds(28+2*third,top+44,third,30);
    int clearW=64,arch=(lw-112)*45/100;architecture.setBounds(16,top+82,arch,30);sort.setBounds(24+arch,top+82,lw-arch-clearW-56,30);clear.setBounds(lw-clearW-16,top+82,clearW,30);verified.setBounds(16,top+119,128,28);calibrated.setBounds(152,top+119,120,28);
    int advancedTop=top+(wide?120:154);if(wide){verified.setBounds(260,40,128,28);calibrated.setBounds(396,40,120,28);}tags.setBounds(16,advancedTop,lw-120,28);tagPicker.setBounds(lw-96,advancedTop,80,28);makes.setBounds(16,advancedTop+34,lw-120,28);makePicker.setBounds(lw-96,advancedTop+34,80,28);creators.setBounds(16,advancedTop+68,lw-120,28);creatorPicker.setBounds(lw-96,advancedTop+68,80,28);
    for(auto* c:std::initializer_list<juce::Component*>{&tags,&makes,&creators,&tagPicker,&makePicker,&creatorPicker})c->setVisible(filtersExpanded);
    int chipY=advancedTop-4+(filtersExpanded?106:0),listTop=chipY+6+(chips.empty()?0:34),bottom=wide?h-80:h-218;chipViewport.setVisible(!chips.empty());chipViewport.setBounds(16,chipY,lw-32,30);list.setBounds(10,listTop,lw-20,juce::jmax(1,bottom-listTop));empty.setBounds(list.getBounds().reduced(12));
    more.setBounds(16,bottom+5,92,28);previous.setBounds(116,bottom+5,64,28);pageLabel.setBounds(184,bottom+5,juce::jmax(45,lw-280),28);next.setBounds(lw-80,bottom+5,64,28);count.setBounds(16,h-34,lw-32,22);state.setBounds(16,h-58,lw-32,24);
    if(wide){coverBounds={dx,top,96,108};headingBounds={dx+108,top,dw-120,110};detail.setBounds(dx,top+122,dw-12,juce::jmax(50,h-158-top-122));captures.setBounds(dx+30,h-148,dw-72,30);previousCapture.setBounds(dx,h-148,26,30);nextCapture.setBounds(w-42,h-148,26,30);int bw=(dw-28)/3;favorite.setBounds(dx,h-110,bw,30);share.setBounds(dx+bw+8,h-110,bw,30);viewSource.setBounds(dx+2*bw+16,h-110,bw,30);replace.setBounds(dx,h-70,(dw-20)/2,34);add.setBounds(dx+(dw-20)/2+8,h-70,(dw-20)/2,34);}
    else{coverBounds={};headingBounds={};captures.setBounds(46,h-144,w-92,30);previousCapture.setBounds(16,h-144,26,30);nextCapture.setBounds(w-42,h-144,26,30);replace.setBounds(16,h-104,(w-40)/2,34);add.setBounds(24+(w-40)/2,h-104,(w-40)/2,34);}
    for(auto* c:std::initializer_list<juce::Component*>{&detail,&favorite,&share,&viewSource})c->setVisible(wide);repaint();
}
void OnlineBrowser::rebuildChips(){
    chips.clear();int x=0;auto addChip=[this,&x](juce::String label,std::function<void()> remove){auto b=std::make_unique<juce::TextButton>(label+ui::text(" ×"));int width=juce::jlimit(70,220,24+int(label.length())*9);b->setBounds(x,0,width,26);x+=width+6;b->setTooltip(ui::text("清除此筛选：")+label);b->onClick=[this,remove]{juce::MessageManager::callAsync(callbacks.wrap([this,remove]{{juce::ScopedValueSetter<bool> guard(updating,true);remove();}searchCatalog();}));};chipStrip.addAndMakeVisible(*b);chips.push_back(std::move(b));};
    if(gear.getSelectedId()>1)addChip(gear.getText(),[this]{gear.setSelectedId(1,juce::dontSendNotification);});
    if(source.getSelectedId()==1){if(format.getSelectedId()>1)addChip(format.getText(),[this]{format.setSelectedId(1,juce::dontSendNotification);});if(architecture.getSelectedId()>1)addChip(architecture.getText(),[this]{architecture.setSelectedId(1,juce::dontSendNotification);});if(verified.getToggleState())addChip(ui::text("认证作者"),[this]{verified.setToggleState(false,juce::dontSendNotification);});if(calibrated.getToggleState())addChip(ui::text("已校准"),[this]{calibrated.setToggleState(false,juce::dontSendNotification);});
        for(auto* field:{&tags,&makes,&creators})for(auto& v:values(field->getText()))addChip(v,[field,v]{auto a=values(field->getText());a.removeString(v);field->setText(a.joinIntoString(", "),false);});}
    chipStrip.setSize(x,28);resized();
}
void OnlineBrowser::resetFilters(){
    {juce::ScopedValueSetter<bool> guard(updating,true);query.clear();tags.clear();makes.clear();creators.clear();source.setSelectedId(1,juce::dontSendNotification);gear.setSelectedId(1,juce::dontSendNotification);format.setSelectedId(1,juce::dontSendNotification);architecture.setSelectedId(1,juce::dontSendNotification);sort.setSelectedId(2,juce::dontSendNotification);verified.setToggleState(false,juce::dontSendNotification);calibrated.setToggleState(false,juce::dontSendNotification);}searchCatalog();
}
void OnlineBrowser::chooseFilter(int kind){
    if(!client.isConnected() || demo)return;auto* anchor=kind==0?&tagPicker:kind==1?&makePicker:&creatorPicker;
    auto picker=std::make_unique<FilterPicker>(client,kind,callbacks.wrap([this,kind](juce::String value){auto* field=kind==0?&tags:kind==1?&makes:&creators;auto items=values(field->getText());items.addIfNotAlreadyThere(value);field->setText(items.joinIntoString(", "),false);searchCatalog();}));juce::CallOutBox::launchAsynchronously(std::move(picker),anchor->getBounds(),this);
}
bool OnlineBrowser::readyKey(){if(client.publishableKey().isNotEmpty())return true;if(processor.services->configureKey(key.getText())){key.setReadOnly(true);return true;}accountExpanded=true;resized();state.setText(ui::text("请先填写 TONE3000 的 t3k_pub_ 公开 Key。"),juce::dontSendNotification);return false;}
void OnlineBrowser::authenticate(t3k::FlowRequest::Kind kind,int id,juce::String target){
    if(!readyKey())return;if(flow)flow->cancel();flow=t3k::makeAuthFlow(t3k::FlowMode::systemBrowser);t3k::FlowRequest request;request.kind=kind;request.catalog.preview=true;if(id>0)request.toneId=juce::String(id);state.setText(ui::text("请在浏览器完成登录，完成后会自动返回。"),juce::dontSendNotification);
    try{flow->start(request,callbacks.wrap([this,target](t3k::FlowResult r){if(!r.connected()){state.setText(r.canceled()?ui::text("Canceled"):ui::message(r.error),juce::dontSendNotification);return;}accountExpanded=false;resized();searchCatalog();if(r.toneId.getIntValue()>0)openTone(r.toneId.getIntValue(),true,target);}));}catch(const std::exception& e){state.setText(ui::message(juce::String::fromUTF8(e.what())),juce::dontSendNotification);}
}
void OnlineBrowser::searchCatalog(){
    if(demo || catalog.isLoading())return;if(!client.isConnected() && source.getSelectedId()!=7){state.setText(ui::text("Connect TONE3000 first"),juce::dontSendNotification);return;}auto text=query.getText().trim();if(auto id=parseToneLink(text)){openTone(*id);return;}
    CatalogQuery q;q.source=static_cast<CatalogQuery::Source>(source.getSelectedId()-1);q.filters.query=text;auto gearId=gear.getSelectedId()-2;if(gearId>=0 && gearId<t3k::gear::all.size())q.filters.gears.add(t3k::gear::all[gearId]);const juce::StringArray formats{"","nam","ir","aida-x","aa-snapshot","proteus"};q.filters.format=formats[juce::jlimit(0,formats.size()-1,format.getSelectedId()-1)];q.architecture=architecture.getSelectedId()==2?2:architecture.getSelectedId()==3?1:0;
    const juce::StringArray sorts{"best-match","trending","downloads-all-time","newest","oldest"};q.filters.sort=sorts[juce::jlimit(0,4,sort.getSelectedId()-1)];q.filters.tags=values(tags.getText());q.filters.makes=values(makes.getText());q.filters.creators=values(creators.getText());q.filters.verified=verified.getToggleState();q.filters.calibrated=calibrated.getToggleState();started=true;rebuildChips();catalog.search(std::move(q));
}
void OnlineBrowser::openTone(int id,bool autoLoad,juce::String target){if(!client.isConnected()){authenticate(t3k::FlowRequest::Kind::loadTone,id,target);return;}catalog.select(id);if(autoLoad)load(id,0,target);}
void OnlineBrowser::update(){
    juce::ScopedValueSetter<bool> guard(updating,true);list.updateContent();for(int i=0;i<catalog.tones().size();++i)if(catalog.tones()[i].id==catalog.selectedId()){list.selectRow(i,true,true);break;}
    bool connected=client.isConnected() && !demo,full=source.getSelectedId()==1,canSearch=!demo && !catalog.isLoading() && (connected || source.getSelectedId()==7);searchButton.setEnabled(canSearch);source.setEnabled(!demo && !catalog.isLoading());gear.setEnabled(canSearch);more.setEnabled(canSearch && catalog.hasMore());previous.setEnabled(canSearch && catalog.page()>1);next.setEnabled(canSearch && catalog.page()<catalog.pages());filters.setEnabled(connected);clear.setEnabled(canSearch);for(auto& b:chips)b->setEnabled(canSearch);for(auto* c:std::initializer_list<juce::Component*>{&format,&architecture,&sort,&tags,&makes,&creators,&verified,&calibrated,&tagPicker,&makePicker,&creatorPicker})c->setEnabled(full && canSearch);
    count.setText(ui::text("已读取 ")+juce::String(catalog.tones().size())+ui::text(" 个音色 · 下载量为次数"),juce::dontSendNotification);pageLabel.setText(juce::String(catalog.page())+" / "+juce::String(catalog.pages()),juce::dontSendNotification);
    auto& data=catalog.detail();bool ready=data.tone.id>0 && !catalog.isDetailLoading();if(data.tone.id!=detailId || (ready && captures.getNumItems()==0)){detailId=data.tone.id;captures.clear(juce::dontSendNotification);if(ready){for(auto& m:data.models)if(m.id>0 && ((data.tone.format=="nam" && m.extension()==".nam") || (data.tone.format=="ir" && m.extension()==".wav")))captures.addItem(m.name+(m.architectureVersion.isEmpty()?juce::String():"  ["+m.architectureVersion+"]")+(m.size.isEmpty()?juce::String():"  "+m.size),m.id);if(captures.getNumItems()>0)captures.setSelectedId(captures.getItemId(0),juce::dontSendNotification);}}
    detail.setText(ready?description(data.tone):ui::text("选择一条音色查看封面、下载量、作者、说明和捕获。"),false);bool playable=connected && ready && (data.tone.format=="nam" || data.tone.format=="ir") && captures.getSelectedId()>0;add.setEnabled(playable);replace.setEnabled(playable);captures.setEnabled(playable);viewSource.setEnabled(ready && !demo);share.setEnabled(ready && !demo);favorite.setEnabled(connected && ready && !catalog.isFavoriting());favorite.setButtonText(ui::text(data.tone.isFavorite?"已收藏":"收藏音色"));favorite.setToggleState(data.tone.isFavorite,juce::dontSendNotification);favorite.setTooltip(ui::text("与 TONE3000 账号收藏同步"));captures.setTooltip(captures.getText());auto modelIndex=captures.getSelectedItemIndex();previousCapture.setEnabled(playable && modelIndex>0);nextCapture.setEnabled(playable && modelIndex+1<captures.getNumItems());
    if(catalog.error().status || catalog.error().message.isNotEmpty())state.setText(catalogError(catalog.error()),juce::dontSendNotification);else if(catalog.detailError().status || catalog.detailError().message.isNotEmpty())state.setText(catalogError(catalog.detailError()),juce::dontSendNotification);else if(catalog.isLoading())state.setText(ui::text("正在读取目录…"),juce::dontSendNotification);else if(catalog.isDetailLoading())state.setText(ui::text("正在获取全部捕获…"),juce::dontSendNotification);else if(ready && !playable && !demo)state.setText(ui::text("该格式可浏览；当前直接加载 NAM / IR。"),juce::dontSendNotification);else state.setText(ui::text("单击看详情 · 双击加载 · 拖到卡片替换"),juce::dontSendNotification);
    empty.setVisible(catalog.tones().isEmpty());empty.setText(ui::text(!connected && !demo?"登录后浏览完整在线目录。\n已有账号设置会自动保留。":catalog.isLoading()?"正在读取 TONE3000…":started?"没有结果，可清除或调整筛选。":"搜索音色，或从完整目录开始浏览。"),juce::dontSendNotification);repaint();
}
juce::String OnlineBrowser::getTooltipForRow(int row){if(row<0 || row>=getNumRows())return {};auto& t=catalog.tones().getReference(row);return t.title+"\n"+toneFacts(t)+"\n"+t.user.creatorName()+" · "+t.publishedAt.substring(0,10);}
void OnlineBrowser::paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool selected){
    if(row<0 || row>=getNumRows())return;auto& t=catalog.tones().getReference(row);g.setColour(selected?juce::Colour(0xff30462c):ui::panel);g.fillRoundedRectangle(4,3,float(w-8),float(h-6),9);paintArtwork(g,{14,13,90,96},artwork->get(t.images.isEmpty()?juce::String():t.images[0]),t.gear);
    int x=116,tw=w-x-14;g.setColour(ui::ink);g.setFont(ui::font(14));g.drawFittedText(t.title,x,10,tw,36,juce::Justification::topLeft,2);g.setColour(ui::muted);g.setFont(ui::font(11));g.drawText(ui::text(t.gear)+" · "+badge(t),x,48,tw,18,juce::Justification::centredLeft);
    g.setColour(ui::accent);g.setFont(ui::font(12));g.drawText(ui::text("下载 ")+compactCount(t.downloadsCount)+ui::text("  · 收藏 ")+compactCount(t.favoritesCount)+ui::text("  · 捕获 ")+juce::String(t.format=="ir"?t.irsCount:t.modelsCount),x,69,tw,18,juce::Justification::centredLeft);
    paintArtwork(g,{x,93,18,18},artwork->get(t.user.avatarUrl),{},true);g.setColour(ui::muted);g.setFont(ui::font(11));g.drawText(t.user.creatorName()+(t.user.isVerified?ui::text(" ✓"):juce::String())+" · "+publishedLabel(t.publishedAt),x+25,92,tw-25,20,juce::Justification::centredLeft);
}
void OnlineBrowser::selectedRowsChanged(int row){if(!updating && !demo && row>=0 && row<getNumRows())catalog.select(catalog.tones()[row].id);}
void OnlineBrowser::listBoxItemDoubleClicked(int row,const juce::MouseEvent&){if(demo || row<0 || row>=getNumRows())return;auto& t=catalog.tones().getReference(row);load(t.id,t.id==catalog.detail().tone.id?captures.getSelectedId():0,chooseTarget?chooseTarget(t.format):juce::String());}
juce::var OnlineBrowser::getDragSourceDescription(const juce::SparseSet<int>& rows){if(demo || rows.isEmpty())return {};auto row=rows[0];if(row<0 || row>=getNumRows())return {};auto id=catalog.tones()[row].id;return "t3k-tone:"+juce::String(id)+":"+juce::String(id==catalog.detail().tone.id?captures.getSelectedId():0);}
void OnlineBrowser::replaceSelected(){auto& t=catalog.detail().tone;if(t.id>0)load(t.id,captures.getSelectedId(),chooseTarget?chooseTarget(t.format):juce::String());}
void OnlineBrowser::stepCapture(int direction){auto next=captures.getSelectedItemIndex()+direction;if(next>=0 && next<captures.getNumItems())captures.setSelectedItemIndex(next,juce::sendNotificationSync);}
void OnlineBrowser::load(int id,int model,juce::String target){
    if(demo)return;auto ticket=++loadGeneration;state.setText(ui::text("正在下载、校验并加载…"),juce::dontSendNotification);processor.loadOnlineTone(id,target,model,callbacks.wrap([this,ticket,id,target](bool ok,juce::String result){if(ticket!=loadGeneration)return;if(ok){activeToneId=id;activeStageId=target;if(activeStageId.isEmpty()){auto chain=processor.engine.chain();if(!chain.empty())activeStageId=chain.back().id;}}state.setText(result,juce::dontSendNotification);}));
}
void OnlineBrowser::showDemo(){
    demo=true;accountExpanded=false;filtersExpanded=false;juce::Array<t3k::Tone> tones;t3k::ToneWithModels selected;
    for(int i=0;i<5;++i){t3k::Tone t;t.id=900000+i;t.title=ui::text(i==0?"演示音色 · 美式清音与临界失真":"演示音色 · 长名称、不同捕获与热度数据");t.format="nam";t.gear=i==2?"pedal":"amp-cab";t.downloadsCount=123456/(i+1);t.favoritesCount=2800/(i+1);t.modelsCount=8-i;t.user.displayName="UI Fixture";t.user.isVerified=i%2==0;t.publishedAt="2026-09-18T00:00:00Z";t.description=ui::text("这是界面测试夹具，用于核对封面、下载量、收藏量、捕获数、作者和长文本布局。\n非真实网站音色或试听结果。");t.license="t3k";t.images.add("https://example.invalid/fixture-"+juce::String(i));
        juce::Image image(juce::Image::RGB,180,180,true);juce::Graphics g(image);g.fillAll(i%2?juce::Colour(0xff604336):juce::Colour(0xff315349));g.setColour(juce::Colour(0xffc2ad82));g.fillRoundedRectangle(18,24,144,132,8);g.setColour(juce::Colour(0xff333533));g.fillRect(28,50,124,94);g.setColour(juce::Colour(0xffaaaa88));for(int x=34;x<146;x+=8)g.drawVerticalLine(x,56,138);g.setColour(juce::Colours::white);g.setFont(ui::font(16));g.drawText("UI FIXTURE",24,25,132,24,juce::Justification::centred);artwork->put(t.images[0],image);tones.add(t);
        if(i==0){selected.tone=t;for(int j=1;j<=8;++j){t3k::Model m;m.id=j;m.name="Fixture capture "+juce::String(j);m.architectureVersion="2";m.modelUrl="https://example.invalid/fixture.nam";selected.models.add(m);}}}
    catalog.preview(tones,selected);resized();
}
}

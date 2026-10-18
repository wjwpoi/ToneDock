#pragma once
#include "Catalog.h"

inline void testCatalog(const std::function<void(bool,const char*)>& check){
    using namespace tonedock;
    struct Pending{int page;std::optional<int> architecture;t3k::Callback<t3k::Page<t3k::Tone>> done;};
    std::vector<Pending> pending;std::vector<t3k::Callback<t3k::ToneWithModels>> details;
    Catalog catalog({[&](const CatalogQuery&,int page,std::optional<int> arch,auto done){pending.push_back({page,arch,std::move(done)});},[&](int,auto done){details.push_back(std::move(done));}});
    auto page=[](std::initializer_list<int> ids,int totalPages){t3k::Page<t3k::Tone> p;p.totalPages=totalPages;for(int id:ids){t3k::Tone t;t.id=id;t.title=juce::String(id);p.data.add(t);}return t3k::Result<t3k::Page<t3k::Tone>>::success(p);};
    CatalogQuery q;catalog.search(q);check(pending.size()==2 && pending[0].architecture==2 && !pending[1].architecture,"full catalog explicitly queries A2 and A1/Custom");
    pending[0].done(page({1,2},2));check(catalog.isLoading() && catalog.tones().isEmpty(),"catalog waits for a complete architecture batch");
    pending[1].done(page({2,3},1));check(catalog.tones().size()==3 && catalog.hasMore(),"architecture results deduplicate overlapping tones");
    catalog.more();check(pending.size()==3 && pending.back().page==2 && pending.back().architecture==2,"pagination continues only the unfinished architecture");pending[2].done(page({4},2));
    check(catalog.tones().size()==4 && !catalog.hasMore(),"catalog traverses every page rather than stopping at the latest feed");
    catalog.search(q);auto old=pending.size()-2;catalog.search(q);auto fresh=pending.size()-2;
    pending[old].done(page({99},1));pending[old+1].done(page({98},1));check(catalog.tones().isEmpty() && catalog.isLoading(),"late search responses cannot replace a new query");
    pending[fresh].done(page({5},1));pending[fresh+1].done(page({6},1));check(catalog.tones().size()==2 && catalog.tones()[0].id==5,"only the latest search is displayed");
    catalog.search(q);auto failed=pending.size()-2;pending[failed].done(page({7},2));pending[failed+1].done(t3k::Result<t3k::Page<t3k::Tone>>::failure({429,"limited"}));
    check(catalog.error().status==429 && catalog.tones().isEmpty() && catalog.hasMore(),"rate limiting never presents a partial batch as a complete catalog");
    catalog.more();check(pending[pending.size()-1].page==1 && pending[pending.size()-2].page==1,"retry preserves failed page cursors");
    pending[pending.size()-2].done(page({7},1));pending.back().done(page({8},1));
    catalog.select(7);catalog.select(8);t3k::ToneWithModels a,b;a.tone.id=7;b.tone.id=8;
    details[0](t3k::Result<t3k::ToneWithModels>::success(a));check(catalog.isDetailLoading(),"stale tone details cannot override the selected tone");details[1](t3k::Result<t3k::ToneWithModels>::success(b));check(catalog.detail().tone.id==8,"selected tone details win");
    q.source=CatalogQuery::favorites;auto before=pending.size();catalog.search(q);check(pending.size()==before+1,"personal libraries use a single bounded stream");pending.back().done(page({9},1));
    q.source=CatalogQuery::all;q.filters.format="ir";before=pending.size();catalog.search(q);check(pending.size()==before+1,"IR catalog does not duplicate architecture requests");pending.back().done(page({10},1));
    catalog.select(10);auto lateDetail=details.back();catalog.clear();t3k::ToneWithModels cleared;cleared.tone.id=10;lateDetail(t3k::Result<t3k::ToneWithModels>::success(cleared));check(catalog.tones().isEmpty() && catalog.selectedId()==0 && !catalog.hasMore() && !catalog.isDetailLoading(),"signing out clears catalog data and invalidates pending details");
    t3k::Callback<t3k::Page<t3k::Tone>> late;bool touched=false;
    {Catalog temporary({[&](const auto&,int,std::optional<int>,auto done){late=std::move(done);},[](int,auto){}});temporary.search(q);temporary.changed=[&]{touched=true;};}
    late(page({11},1));check(!touched,"destroyed catalog ignores late network callbacks");
}

inline void testOnlineLoading(const juce::File& directory,const juce::File& fixture,const juce::File& invalid,const std::function<void(bool,const char*)>& check){
    using namespace tonedock;
    auto first=directory.getChildFile("online-a.nam"),second=directory.getChildFile("online-b.nam");
    first.replaceWithText(fixture.loadFileAsString().replace("[0.5]","[0.42]"));second.replaceWithText(fixture.loadFileAsString().replace("[0.5]","[0.43]"));
    Processor processor;check(processor.engine.waitForBuild(),"online test initial graph ready");auto original=processor.engine.chain();
    struct Download{int id;t3k::Callback<juce::File> done;};std::vector<Download> pending;bool hold=false,timedOut=false,finished=false;int resolves=0;
    std::exception_ptr error;juce::String target;
    auto guarded=[&](auto fn){try{fn();}catch(...){error=std::current_exception();juce::MessageManager::getInstance()->stopDispatchLoop();}};
    processor.onlineSource.resolve=[&](int id,auto done){++resolves;t3k::ToneWithModels data;data.tone.id=id;data.tone.title="Online tone "+juce::String(id);data.tone.format=id==4?"proteus":"nam";data.tone.gear="amp";data.tone.user.displayName="Creator";data.tone.license="t3k";data.tone.url="https://www.tone3000.com/tones/test-"+juce::String(id);
        if(id!=3){t3k::Model m;m.id=id;m.toneId=id;m.name="Capture";m.modelUrl="https://www.tone3000.com/models/"+juce::String(id)+".nam";data.models.add(m);}done(t3k::Result<t3k::ToneWithModels>::success(std::move(data)));};
    processor.onlineSource.download=[&](const t3k::Model& m,auto done){if(hold)pending.push_back({m.id,std::move(done)});else done(t3k::Result<juce::File>::success(m.id==2?invalid:m.id==5?second:first));};
    std::function<void()> full,presetCancelled,deleted,race,append,bad,unsupported,noModels;
    full=[&]{guarded([&]{while(processor.engine.chain().size()<16)processor.addEffect("Gain");auto calls=resolves;processor.loadOnlineTone(1,{},0,[&,calls](bool ok,juce::String){guarded([&]{check(!ok && resolves==calls && processor.engine.chain().size()==16,"full chain rejects online loads before network or graph mutation");finished=true;juce::MessageManager::getInstance()->stopDispatchLoop();});});});};
    presetCancelled=[&]{guarded([&]{pending.clear();processor.loadOnlineTone(1,{},0,[&](bool ok,juce::String){guarded([&]{check(!ok && processor.engine.chain().size()==4,"opening a preset cancels pending online loads");full();});});juce::MemoryBlock state;processor.getStateInformation(state);processor.setStateInformation(state.getData(),int(state.getSize()));auto done=pending[0].done;done(t3k::Result<juce::File>::success(first));});};
    deleted=[&]{guarded([&]{pending.clear();processor.loadOnlineTone(1,target,0,[&](bool ok,juce::String){guarded([&]{check(!ok && processor.engine.chain().size()==4 && processor.engine.chain()[1].file.isEmpty(),"deleted online target never falls back to another NAM stage");presetCancelled();});});auto chain=processor.engine.chain();std::erase_if(chain,[&](const auto& s){return s.id==target;});processor.engine.setChain(chain);auto done=pending[0].done;done(t3k::Result<juce::File>::success(first));});};
    int raceCompletions=0;
    auto raceDone=[&](bool expected,bool ok){guarded([&]{check(ok==expected,"only the newest replacement request can commit");if(++raceCompletions==2){auto entry=processor.services->library.importFile(second);check(processor.engine.chain().back().file==processor.services->library.fileFor(entry).getFullPathName(),"late downloads cannot overwrite the newer online tone");deleted();}});};
    race=[&]{guarded([&]{hold=true;pending.clear();
        processor.loadOnlineTone(1,target,0,[&](bool ok,juce::String){raceDone(false,ok);});processor.loadOnlineTone(5,target,0,[&](bool ok,juce::String){raceDone(true,ok);});
        auto older=pending[0].done,newer=pending[1].done;newer(t3k::Result<juce::File>::success(second));older(t3k::Result<juce::File>::success(first));
    });};
    append=[&]{guarded([&]{processor.loadOnlineTone(1,{},0,[&](bool ok,juce::String){guarded([&]{auto chain=processor.engine.chain();check(ok && chain.size()==5 && chain.back().type=="NAM" && juce::File(chain.back().file).existsAsFile(),"online tone downloads, validates and joins the chain without a manual library step");target=chain.back().id;auto entries=processor.services->library.entries();auto found=std::find_if(entries.begin(),entries.end(),[](const auto& e){return e.toneId=="1";});check(found!=entries.end() && found->author=="Creator" && found->license=="t3k" && found->sourceUrl.endsWith("test-1"),"online import preserves author, license and source");race();});});});};
    bad=[&]{guarded([&]{processor.loadOnlineTone(2,{},0,[&](bool ok,juce::String){guarded([&]{check(!ok && processor.engine.chain().size()==original.size(),"corrupt online model preserves the existing chain");append();});});});};
    unsupported=[&]{guarded([&]{processor.loadOnlineTone(4,{},0,[&](bool ok,juce::String){guarded([&]{check(!ok && processor.engine.chain().size()==original.size(),"unsupported online formats remain browsable without changing audio");bad();});});});};
    noModels=[&]{guarded([&]{processor.loadOnlineTone(3,{},0,[&](bool ok,juce::String){guarded([&]{check(!ok && processor.engine.chain().size()==original.size(),"empty online captures preserve audio");unsupported();});});});};
    juce::Timer::callAfterDelay(15000,[&]{timedOut=true;juce::MessageManager::getInstance()->stopDispatchLoop();});
    juce::MessageManager::callAsync(noModels);juce::MessageManager::getInstance()->runDispatchLoop();
    if(error)std::rethrow_exception(error);check(!timedOut && finished,"asynchronous online-loading checks complete");
}

#pragma once
#include "Catalog.h"
#include "CatalogMedia.h"
#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

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
    std::vector<t3k::Callback<t3k::Nothing>> hearts;
    Catalog favorites({[](const auto&,int,std::optional<int>,auto){},[](int,auto){},[&](int,bool,auto done){hearts.push_back(std::move(done));}});
    t3k::Tone popular;popular.id=42;popular.favoritesCount=9;t3k::ToneWithModels detail;detail.tone=popular;favorites.preview({popular},detail);
    favorites.setFavorite(42,true);check(favorites.isFavoriting() && !favorites.detail().tone.isFavorite,"favorites wait for server confirmation");hearts.back()(t3k::Result<t3k::Nothing>::failure({429,"limited"}));check(!favorites.detail().tone.isFavorite && favorites.detail().tone.favoritesCount==9,"failed favorites preserve server metadata");
    favorites.setFavorite(42,true);hearts.back()(t3k::Result<t3k::Nothing>::success({}));check(favorites.detail().tone.isFavorite && favorites.tones()[0].favoritesCount==10,"successful favorites update the card and detail together");
    favorites.setFavorite(42,false);auto lateHeart=hearts.back();favorites.clear();lateHeart(t3k::Result<t3k::Nothing>::success({}));check(favorites.tones().isEmpty() && !favorites.isFavoriting(),"sign out invalidates in-flight favorite callbacks");
    std::vector<t3k::Callback<t3k::ToneWithModels>> resolutions;
    Catalog summaries({[&](const auto&,int,std::optional<int>,auto done){t3k::Page<t3k::Tone> p;p.totalPages=1;t3k::Tone t;t.id=71;t.title="Public summary";p.data.add(t);done(t3k::Result<t3k::Page<t3k::Tone>>::success(p));},[&](int,auto done){resolutions.push_back(std::move(done));}});
    CatalogQuery summaryQuery;summaryQuery.filters.format="ir";summaries.search(summaryQuery);summaries.select(71,true);
    check(resolutions.empty() && summaries.detail().tone.title=="Public summary" && !summaries.isDetailLoading() && !summaries.isDetailReady(),"public summary selection exposes metadata without authenticated detail requests");
    summaries.select(71);check(resolutions.size()==1 && summaries.isDetailLoading(),"a public summary can resolve models after connecting");
    resolutions.back()(t3k::Result<t3k::ToneWithModels>::failure({429,"limited"}));check(!summaries.isDetailReady() && summaries.detail().tone.id==71 && summaries.detailError().status==429,"failed details preserve the summary without marking it resolved");
    summaries.select(71);check(resolutions.size()==2 && summaries.isDetailLoading() && summaries.detailError().status==0,"selecting the same tone retries a failed detail request");
    t3k::ToneWithModels resolved;resolved.tone.id=71;t3k::Model capture;capture.id=7;resolved.models.add(capture);resolutions.back()(t3k::Result<t3k::ToneWithModels>::success(resolved));summaries.select(71);
    check(summaries.isDetailReady() && summaries.detail().models.size()==1 && resolutions.size()==2,"successful detail selection is cached rather than fetched again");
    summaries.clear();check(!summaries.isDetailReady(),"clearing the catalog invalidates resolved details");
}

inline void testCatalogMedia(const juce::File& directory,const juce::File& fixture,const std::function<void(bool,const char*)>& check){
    using namespace tonedock;
    check(compactCount(999)=="999" && compactCount(122800)=="122.8K" && compactCount(2000000)=="2M","catalog popularity uses readable counts without changing exact metadata");
    auto now=juce::Time::fromISO8601("2026-10-19T12:00:00Z");check(publishedLabel("2026-10-18T12:00:00Z",now)==ui::text("1 天前"),"publication ages use real timestamps");
    auto raw=juce::JSON::parse(R"({"id":42,"title":"Amp","format":"nam","gear":"amp-cab","models_count":8,"a2_models_count":3,"downloads_count":122800,"favorites_count":900,"is_favorite":true,"published_at":"2026-09-18T00:00:00Z","images":["https://example.invalid/cover.jpg"],"user":{"username":"maker","avatar_url":"https://example.invalid/avatar.png","is_verified":true},"tags":[{"id":3,"name":"clean"}],"makes":[{"id":8,"name":"Fender"}]})");
    auto tone=t3k::Tone::fromVar(raw),roundtrip=t3k::Tone::fromVar(toneSummary(tone));check(roundtrip.downloadsCount==122800 && roundtrip.favoritesCount==900 && roundtrip.images==tone.images && roundtrip.user.isVerified && roundtrip.a2ModelsCount==3 && roundtrip.tags[0].name=="clean","artwork, social statistics, badges and filter terms survive parsing and history storage");
    auto history=directory.getChildFile("history");history.createDirectory();rememberTone(tone,history);tone.id=43;rememberTone(tone,history);tone.id=42;rememberTone(tone,history);auto recent=recentTones(history);check(recent.size()==2 && recent[0].id==42 && recent[1].id==43,"recent tones are deduplicated and ordered by actual use");
    juce::Image source(juce::Image::RGB,600,300,true);juce::MemoryOutputStream stream;juce::PNGImageFormat().writeImageToStream(source,stream);auto image=CatalogArtwork::decode(stream.getMemoryBlock());check(image.getWidth()==256 && image.getHeight()==128,"artwork decoding limits cached resolution while retaining aspect ratio");
    juce::MemoryBlock bad("bad",3);check(!CatalogArtwork::decode(bad).isValid() && !CatalogArtwork::allowed("file:///private") && !CatalogArtwork::allowed("http://example.com/a.png"),"bad artwork and non-HTTPS image sources fall back safely");
    CatalogArtwork cache;for(int i=0;i<130;++i)cache.put("fixture-"+juce::String(i),image);check(!cache.get("fixture-0").isValid() && cache.get("fixture-129").isValid(),"artwork memory cache evicts old entries");
    Library library(directory.getChildFile("artwork-library"));auto entry=library.importFile(fixture);entry.category="My category";entry.favorite=true;library.update(entry);auto* provenance=new juce::DynamicObject;provenance->setProperty("artworkUrl",tone.images[0]);provenance->setProperty("creatorAvatarUrl",tone.user.avatarUrl);auto enriched=library.importFile(fixture,juce::var(provenance));check(enriched.id==entry.id && enriched.artworkUrl==tone.images[0] && enriched.category=="My category" && enriched.favorite && library.entries().size()==1,"adding online artwork enriches duplicate models without losing user edits or creating duplicates");
}

inline void testOnlineLoading(const juce::File& directory,const juce::File& fixture,const juce::File& invalid,const std::function<void(bool,const char*)>& check){
    using namespace tonedock;
    struct MessageLoop final : juce::Timer {
        bool stopped=false,timedOut=false;
        void stop(){
            stopped=true;stopTimer();
#if !JUCE_MAC
            juce::MessageManager::getInstance()->stopDispatchLoop();
#endif
        }
        void timerCallback() override{timedOut=true;stop();}
        void run(){
            startTimer(15000);
#if JUCE_MAC
            // A console test has no NSApplication. JUCE's macOS dispatch loop
            // sends [NSApp run], which returns immediately when NSApp is nil.
            // Its asynchronous queue is attached to the main CFRunLoop instead.
            while(!stopped)CFRunLoopRunInMode(kCFRunLoopDefaultMode,0.05,true);
#else
            juce::MessageManager::getInstance()->runDispatchLoop();
#endif
            stopTimer();
        }
    } loop;
    auto first=directory.getChildFile("online-a.nam"),second=directory.getChildFile("online-b.nam");
    first.replaceWithText(fixture.loadFileAsString().replace("[0.5]","[0.42]"));second.replaceWithText(fixture.loadFileAsString().replace("[0.5]","[0.43]"));
    Processor processor;check(processor.engine.waitForBuild(),"online test initial graph ready");auto original=processor.engine.chain();
    struct Download{int id;t3k::Callback<juce::File> done;};std::vector<Download> pending;bool hold=false,finished=false;int resolves=0;
    std::exception_ptr error;juce::String target,otherStageFile;
    auto guarded=[&](auto fn){try{fn();}catch(...){error=std::current_exception();loop.stop();}};
    processor.onlineSource.resolve=[&](int id,auto done){++resolves;t3k::ToneWithModels data;data.tone.id=id;data.tone.title="Online tone "+juce::String(id);data.tone.format=id==4?"proteus":"nam";data.tone.gear="amp";data.tone.user.displayName="Creator";data.tone.license="t3k";data.tone.url="https://www.tone3000.com/tones/test-"+juce::String(id);
        if(id!=3){t3k::Model m;m.id=id;m.toneId=id;m.name="Capture";m.modelUrl="https://www.tone3000.com/models/"+juce::String(id)+".nam";data.models.add(m);}done(t3k::Result<t3k::ToneWithModels>::success(std::move(data)));};
    processor.onlineSource.download=[&](const t3k::Model& m,auto done){if(hold)pending.push_back({m.id,std::move(done)});else done(t3k::Result<juce::File>::success(m.id==2?invalid:m.id==5?second:first));};
    std::function<void()> full,presetCancelled,deleted,localCancelled,multipleTargets,race,append,bad,unsupported,noModels;
    full=[&]{guarded([&]{while(processor.engine.chain().size()<16)processor.addEffect("Gain");auto calls=resolves;processor.loadOnlineTone(1,{},0,[&,calls](bool ok,juce::String){guarded([&]{check(!ok && resolves==calls && processor.engine.chain().size()==16,"full chain rejects online loads before network or graph mutation");finished=true;loop.stop();});});});};
    presetCancelled=[&]{guarded([&]{pending.clear();processor.loadOnlineTone(1,{},0,[&](bool ok,juce::String){guarded([&]{check(!ok && processor.engine.chain().size()==4,"opening a preset cancels pending online loads");full();});});juce::MemoryBlock state;processor.getStateInformation(state);processor.setStateInformation(state.getData(),int(state.getSize()));auto done=pending[0].done;done(t3k::Result<juce::File>::success(first));});};
    deleted=[&]{guarded([&]{pending.clear();processor.loadOnlineTone(1,target,0,[&](bool ok,juce::String){guarded([&]{check(!ok && processor.engine.chain().size()==4 && processor.engine.chain()[1].file==otherStageFile,"deleted online target never falls back to another NAM stage");presetCancelled();});});auto chain=processor.engine.chain();std::erase_if(chain,[&](const auto& s){return s.id==target;});processor.engine.setChain(chain);auto calls=resolves;processor.loadOnlineTone(1,target,0,[&,calls](bool ok,juce::String){guarded([&]{check(!ok && resolves==calls && processor.engine.chain().size()==4,"a deleted explicit target is rejected before network access without appending");});});auto done=pending[0].done;done(t3k::Result<juce::File>::success(first));});};
    localCancelled=[&]{guarded([&]{pending.clear();auto local=processor.services->library.importFile(first);auto chosen=processor.services->library.fileFor(local).getFullPathName();processor.loadOnlineTone(5,target,0,[&,chosen](bool ok,juce::String){guarded([&]{auto chain=processor.engine.chain();check(!ok && chain.size()==5 && chain.back().id==target && chain.back().file==chosen && chain[1].file==otherStageFile,"a local selection cancels a pending remote replacement only for the same NAM stage");deleted();});});processor.loadModel(local,target);auto done=pending[0].done;done(t3k::Result<juce::File>::success(second));});};
    int multipleCompletions=0;auto multipleDone=[&](bool ok){guarded([&]{check(ok,"different NAM stages keep independent online requests");if(++multipleCompletions==2){auto firstEntry=processor.services->library.importFile(first),secondEntry=processor.services->library.importFile(second);auto chain=processor.engine.chain();otherStageFile=processor.services->library.fileFor(secondEntry).getFullPathName();check(chain.size()==5 && chain[1].id==original[1].id && chain[1].file==otherStageFile && chain.back().id==target && chain.back().file==processor.services->library.fileFor(firstEntry).getFullPathName(),"out-of-order downloads replace only their explicit NAM UUID without adding effects");localCancelled();}});};
    multipleTargets=[&]{guarded([&]{pending.clear();processor.loadOnlineTone(5,original[1].id,0,[&](bool ok,juce::String){multipleDone(ok);});processor.loadOnlineTone(1,target,0,[&](bool ok,juce::String){multipleDone(ok);});auto earlier=pending[0].done,later=pending[1].done;later(t3k::Result<juce::File>::success(first));earlier(t3k::Result<juce::File>::success(second));});};
    int raceCompletions=0;
    auto raceDone=[&](bool expected,bool ok){guarded([&]{check(ok==expected,"only the newest replacement request can commit");if(++raceCompletions==2){auto entry=processor.services->library.importFile(second);check(processor.engine.chain().back().file==processor.services->library.fileFor(entry).getFullPathName(),"late downloads cannot overwrite the newer online tone");multipleTargets();}});};
    race=[&]{guarded([&]{hold=true;pending.clear();
        processor.loadOnlineTone(1,target,0,[&](bool ok,juce::String){raceDone(false,ok);});processor.loadOnlineTone(5,target,0,[&](bool ok,juce::String){raceDone(true,ok);});
        auto older=pending[0].done,newer=pending[1].done;newer(t3k::Result<juce::File>::success(second));older(t3k::Result<juce::File>::success(first));
    });};
    append=[&]{guarded([&]{processor.loadOnlineTone(1,{},0,[&](bool ok,juce::String){guarded([&]{auto chain=processor.engine.chain();check(ok && chain.size()==5 && chain.back().type=="NAM" && juce::File(chain.back().file).existsAsFile(),"online tone downloads, validates and joins the chain without a manual library step");target=chain.back().id;auto entries=processor.services->library.entries();auto found=std::find_if(entries.begin(),entries.end(),[](const auto& e){return e.toneId=="1";});check(found!=entries.end() && found->author=="Creator" && found->license=="t3k" && found->sourceUrl.endsWith("test-1"),"online import preserves author, license and source");race();});});});};
    bad=[&]{guarded([&]{processor.loadOnlineTone(2,{},0,[&](bool ok,juce::String){guarded([&]{check(!ok && processor.engine.chain().size()==original.size(),"corrupt online model preserves the existing chain");append();});});});};
    unsupported=[&]{guarded([&]{processor.loadOnlineTone(4,{},0,[&](bool ok,juce::String){guarded([&]{check(!ok && processor.engine.chain().size()==original.size(),"unsupported online formats remain browsable without changing audio");bad();});});});};
    noModels=[&]{guarded([&]{processor.loadOnlineTone(3,{},0,[&](bool ok,juce::String){guarded([&]{check(!ok && processor.engine.chain().size()==original.size(),"empty online captures preserve audio");unsupported();});});});};
    check(juce::MessageManager::callAsync(noModels),"online-loading test queues its first callback");loop.run();
    if(error)std::rethrow_exception(error);check(!loop.timedOut && finished,"asynchronous online-loading checks complete");
}

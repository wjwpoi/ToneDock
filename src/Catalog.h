#pragma once
#include "Client.h"
#include <array>
#include <set>

namespace tonedock {
struct CatalogQuery {
    enum Source { all, favorites, downloaded, created, latest, trending, recent } source=all;
    t3k::SearchTonesParams filters;
    // 0 combines A2 and the legacy A1/Custom set; an omitted API filter alone excludes A2.
    int architecture=0;
};
class Catalog {
public:
    using Fetch=std::function<void(const CatalogQuery&,int,std::optional<int>,t3k::Callback<t3k::Page<t3k::Tone>>)>;
    using Detail=std::function<void(int,t3k::Callback<t3k::ToneWithModels>)>;
    using Favorite=std::function<void(int,bool,t3k::Callback<t3k::Nothing>)>;
    struct Backend {Fetch fetch;Detail detail;Favorite favorite;};
    explicit Catalog(Backend);
    static Backend forClient(t3k::Client&,juce::File historyDirectory={});
    void search(CatalogQuery,int startPage=1);void more();void jump(int);void select(int toneId,bool summaryOnly=false);void clear();
    void setFavorite(int,bool);bool isFavoriting() const{return favoriting;}
    void preview(juce::Array<t3k::Tone>,t3k::ToneWithModels);
    int page() const{return readPage;}int pages() const{return totalPages;}
    const juce::Array<t3k::Tone>& tones() const{return results;}
    const t3k::ToneWithModels& detail() const{return selected;}
    bool isLoading() const{return loading;}
    bool isDetailLoading() const{return detailLoading;}
    bool isDetailReady() const{return detailReady;}
    bool hasMore() const;
    int selectedId() const{return toneId;}
    const t3k::Error& error() const{return lastError;}
    const t3k::Error& detailError() const{return lastDetailError;}
    std::function<void()> changed;
private:
    void notify(){if(changed)changed();}
    void received(int generation,int stream,t3k::Result<t3k::Page<t3k::Tone>>);
    Backend backend;CatalogQuery query;
    juce::Array<t3k::Tone> results;t3k::ToneWithModels selected;
    std::array<int,2> nextPage{{1,1}};
    std::array<bool,2> exhausted{{false,true}};
    std::array<std::optional<t3k::Page<t3k::Tone>>,2> batch;
    std::set<int> seen;
    int generation=0,selectionGeneration=0,favoriteGeneration=0,toneId=0,pending=0,readPage=0,totalPages=0;
    bool loading=false,detailLoading=false,detailReady=false,favoriting=false;
    t3k::Error lastError,lastDetailError,batchError;
    t3k::AsyncScope callbacks;
};
}

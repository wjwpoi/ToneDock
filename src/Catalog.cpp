#include "Catalog.h"
#include <algorithm>

namespace tonedock {
Catalog::Catalog(Backend b):backend(std::move(b)){}
Catalog::Backend Catalog::forClient(t3k::Client& client) {
    return {
        [&client](const CatalogQuery& q,int page,std::optional<int> architecture,t3k::Callback<t3k::Page<t3k::Tone>> done){
            if(q.source==CatalogQuery::all){auto p=q.filters;p.page=page;p.pageSize=25;p.architecture=architecture;client.searchTones(std::move(p),std::move(done));return;}
            t3k::ListLibraryParams p;p.page=page;p.pageSize=25;p.query=q.filters.query;if(q.filters.gears.size()==1)p.gear=q.filters.gears[0];
            if(q.source==CatalogQuery::favorites)client.listFavoritedTones(p,std::move(done));
            else if(q.source==CatalogQuery::downloaded)client.listDownloadedTones(p,std::move(done));
            else if(q.source==CatalogQuery::created)client.listCreatedTones(p,std::move(done));
            else {
                auto callback=[done=std::move(done)](t3k::Result<juce::Array<t3k::Tone>> r){if(!r.ok()){done(t3k::Result<t3k::Page<t3k::Tone>>::failure(r.error));return;}t3k::Page<t3k::Tone> p;p.data=*r;p.page=1;p.totalPages=1;p.total=r->size();done(t3k::Result<t3k::Page<t3k::Tone>>::success(std::move(p)));};
                if(q.source==CatalogQuery::latest)client.listLatestTones(std::move(callback));else client.listTrendingTones(p.gear,std::move(callback));
            }
        },
        [&client](int id,t3k::Callback<t3k::ToneWithModels> done){client.getToneWithAllModels(id,std::move(done));}
    };
}
bool Catalog::hasMore() const{return !exhausted[0] || !exhausted[1];}
void Catalog::clear(){++generation;++selectionGeneration;results.clear();seen.clear();selected={};toneId=0;loading=false;detailLoading=false;pending=0;exhausted={true,true};batch={};lastError={};lastDetailError={};notify();}
void Catalog::search(CatalogQuery q) {
    query=std::move(q);++generation;++selectionGeneration;toneId=0;selected={};detailLoading=false;
    results.clear();seen.clear();nextPage={1,1};exhausted={false,true};lastError={};lastDetailError={};loading=false;
    if(query.source==CatalogQuery::all && query.architecture==0 && (query.filters.format.isEmpty() || query.filters.format=="nam"))exhausted[1]=false;
    more();
}
void Catalog::more() {
    if(loading || !hasMore())return;
    loading=true;lastError={};batchError={};batch={};pending=int(!exhausted[0])+int(!exhausted[1]);auto ticket=generation;
    notify();
    for(int stream=0;stream<2;++stream)if(!exhausted[size_t(stream)]){
        std::optional<int> architecture;
        if(query.source==CatalogQuery::all && ((query.architecture==0 && stream==0) || query.architecture==2))architecture=2;
        backend.fetch(query,nextPage[size_t(stream)],architecture,callbacks.wrap([this,ticket,stream](auto r){received(ticket,stream,std::move(r));}));
    }
}
void Catalog::received(int ticket,int stream,t3k::Result<t3k::Page<t3k::Tone>> r) {
    if(ticket!=generation)return;
    if(r.ok())batch[size_t(stream)]=*r;else if(batchError.status==0 && batchError.message.isEmpty()){batchError=r.error;if(batchError.status==0 && batchError.message.isEmpty())batchError.message="Request failed";}
    if(--pending>0)return;
    loading=false;
    if(batchError.status!=0 || batchError.message.isNotEmpty()){lastError=batchError;notify();return;}
    int rows=0;for(auto& page:batch)if(page)rows=juce::jmax(rows,page->data.size());
    for(int row=0;row<rows;++row)for(auto& page:batch)if(page && row<page->data.size()){auto& tone=page->data.getReference(row);if(tone.id>0 && seen.insert(tone.id).second)results.add(tone);}
    for(size_t i=0;i<batch.size();++i)if(batch[i]){auto& p=*batch[i];exhausted[i]=p.data.isEmpty() || nextPage[i]>=p.totalPages;++nextPage[i];}
    if(query.filters.sort=="newest" || query.filters.sort=="oldest")std::stable_sort(results.begin(),results.end(),[&](const auto& a,const auto& b){return query.filters.sort=="newest"?a.publishedAt>b.publishedAt:a.publishedAt<b.publishedAt;});
    else if(query.filters.sort=="downloads-all-time")std::stable_sort(results.begin(),results.end(),[](const auto& a,const auto& b){return a.downloadsCount>b.downloadsCount;});
    notify();
}
void Catalog::select(int id) {
    if(id==toneId && (detailLoading || selected.tone.id==id))return;
    toneId=id;selected={};detailLoading=true;lastDetailError={};auto ticket=++selectionGeneration;notify();
    backend.detail(id,callbacks.wrap([this,ticket](t3k::Result<t3k::ToneWithModels> r){if(ticket!=selectionGeneration)return;detailLoading=false;if(r.ok())selected=*r;else lastDetailError=r.error;notify();}));
}
}

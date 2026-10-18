#include "CatalogMedia.h"
#include <algorithm>
#include <mutex>

namespace tonedock {
juce::String compactCount(int n){
    n=juce::jmax(0,n);if(n<1000)return juce::String(n);
    double divisor=n>=1000000?1000000.:1000.;auto value=juce::String(double(n)/divisor,1);
    if(value.endsWith(".0"))value=value.dropLastCharacters(2);return value+(n>=1000000?"M":"K");
}
juce::String publishedLabel(const juce::String& value,juce::Time now){
    if(value.isEmpty())return ui::text("日期未知");auto time=juce::Time::fromISO8601(value);
    if(time.toMilliseconds()<=0)return value.substring(0,10);
    auto days=juce::jmax(0,int((now.toMilliseconds()-time.toMilliseconds())/86400000));
    if(days==0)return ui::text("今天");if(days<30)return juce::String(days)+ui::text(" 天前");
    if(days<365)return juce::String(days/30)+ui::text(" 个月前");return juce::String(days/365)+ui::text(" 年前");
}
juce::String toneFacts(const t3k::Tone& t){
    return ui::text("下载 ")+juce::String(t.downloadsCount)+ui::text(" · 收藏 ")+juce::String(t.favoritesCount)+ui::text(" · 捕获 ")+juce::String(t.format=="ir"?t.irsCount:t.modelsCount);
}
juce::var toneSummary(const t3k::Tone& t){
    auto* o=new juce::DynamicObject;auto* u=new juce::DynamicObject;
    u->setProperty("id",t.user.id);u->setProperty("username",t.user.username);u->setProperty("display_name",t.user.displayName);u->setProperty("avatar_url",t.user.avatarUrl);u->setProperty("url",t.user.url);u->setProperty("is_verified",t.user.isVerified);o->setProperty("user",juce::var(u));
    o->setProperty("id",t.id);o->setProperty("title",t.title);o->setProperty("description",t.description);o->setProperty("gear",t.gear);o->setProperty("format",t.format);o->setProperty("license",t.license);o->setProperty("url",t.url);o->setProperty("published_at",t.publishedAt);o->setProperty("is_public",t.isPublic);
    o->setProperty("downloads_count",t.downloadsCount);o->setProperty("favorites_count",t.favoritesCount);o->setProperty("models_count",t.modelsCount);o->setProperty("a2_models_count",t.a2ModelsCount);o->setProperty("irs_count",t.irsCount);o->setProperty("is_favorite",t.isFavorite);
    auto strings=[](const juce::StringArray& a){juce::Array<juce::var> out;for(auto& s:a)out.add(s);return juce::var(out);};o->setProperty("images",strings(t.images));o->setProperty("links",strings(t.links));
    auto terms=[](const juce::Array<t3k::Taxonomy>& a){juce::Array<juce::var> out;for(auto& t:a){auto* v=new juce::DynamicObject;v->setProperty("id",t.id);v->setProperty("name",t.name);out.add(juce::var(v));}return juce::var(out);};o->setProperty("tags",terms(t.tags));o->setProperty("makes",terms(t.makes));return juce::var(o);
}
namespace {std::mutex historyMutex;}
juce::Array<t3k::Tone> recentTones(const juce::File& root){
    auto file=root.getChildFile("recent-tones.json");juce::Array<t3k::Tone> out;if(file.getSize()>2*1024*1024)return out;
    auto data=juce::JSON::parse(file);if(auto* a=data.getArray())for(auto& v:*a){auto t=t3k::Tone::fromVar(v);if(t.id>0)out.add(std::move(t));}return out;
}
void rememberTone(const t3k::Tone& tone,const juce::File& root){
    if(tone.id<=0)return;std::lock_guard guard(historyMutex);juce::InterProcessLock lock("ToneDock-history-"+juce::String(root.getFullPathName().hashCode64()));if(!lock.enter(1000))return;
    struct Exit{juce::InterProcessLock& l;~Exit(){l.exit();}} exit{lock};juce::Array<juce::var> values;values.add(toneSummary(tone));
    for(auto& t:recentTones(root))if(t.id!=tone.id && values.size()<50)values.add(toneSummary(t));juce::TemporaryFile temp(root.getChildFile("recent-tones.json"));if(temp.getFile().replaceWithText(juce::JSON::toString(juce::var(values))))temp.overwriteTargetFileWithTemporary();
}
bool CatalogArtwork::allowed(const juce::String& s){juce::URL u(s);return s.length()<4096 && u.getScheme()=="https" && u.getDomain().isNotEmpty();}
juce::Image CatalogArtwork::decode(const juce::MemoryBlock& bytes){
    if(bytes.getSize()==0 || bytes.getSize()>4*1024*1024)return {};auto image=juce::ImageFileFormat::loadFrom(bytes.getData(),bytes.getSize());
    if(!image.isValid() || image.getWidth()>4096 || image.getHeight()>4096)return {};
    if(image.getWidth()>256 || image.getHeight()>256){auto scale=256.f/float(juce::jmax(image.getWidth(),image.getHeight()));image=image.rescaled(juce::jmax(1,int(image.getWidth()*scale)),juce::jmax(1,int(image.getHeight()*scale)),juce::Graphics::mediumResamplingQuality);}return image;
}
void CatalogArtwork::put(const juce::String& url,juce::Image image){
    pending.erase(url);images[url]={image,++tick};while(images.size()>128){auto oldest=std::min_element(images.begin(),images.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});images.erase(oldest);}sendChangeMessage();
}
juce::Image CatalogArtwork::get(const juce::String& url){
    if(auto it=images.find(url);it!=images.end()){it->second.used=++tick;return it->second.image;}
    if(!allowed(url) || pending.contains(url) || pending.size()>=24)return {};pending.insert(url);
    auto directory=defaultDataDirectory().getChildFile("cache/artwork");auto target=directory.getChildFile(juce::SHA256(url.toRawUTF8(),url.getNumBytesAsUTF8()).toHexString()+".png");
    auto deliver=callbacks.wrap([this,url](juce::Image image){put(url,image);});
    workers.addJob([url,target,deliver]{
        juce::MemoryBlock bytes;juce::Image image;if(target.getSize()<=4*1024*1024 && target.loadFileAsData(bytes))image=decode(bytes);
        if(!image.isValid()){
            int status=0;auto stream=juce::URL(url).createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs(6000).withStatusCode(&status).withNumRedirectsToFollow(3));
            if(stream && status>=200 && status<300){bytes.reset();stream->readIntoMemoryBlock(bytes,4*1024*1024+1);image=decode(bytes);}
            if(image.isValid() && target.getParentDirectory().createDirectory().wasOk()){juce::TemporaryFile temp(target);if(auto out=temp.getFile().createOutputStream()){juce::PNGImageFormat().writeImageToStream(image,*out);out.reset();temp.overwriteTargetFileWithTemporary();}
                auto files=target.getParentDirectory().findChildFiles(juce::File::findFiles,false,"*.png");std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.getLastModificationTime()<b.getLastModificationTime();});while(files.size()>256){files[0].deleteFile();files.remove(0);}}
        }
        juce::MessageManager::callAsync([deliver,image]{deliver(image);});
    });return {};
}
void paintArtwork(juce::Graphics& g,juce::Rectangle<int> bounds,const juce::Image& image,const juce::String& gear,bool avatar){
    auto r=bounds.toFloat();g.setColour(ui::raised);if(avatar)g.fillEllipse(r);else g.fillRoundedRectangle(r,8);
    if(image.isValid()){juce::Graphics::ScopedSaveState guard(g);juce::Path clip;if(avatar)clip.addEllipse(r);else clip.addRoundedRectangle(r,8);g.reduceClipRegion(clip);g.drawImageWithin(image,bounds.getX(),bounds.getY(),bounds.getWidth(),bounds.getHeight(),juce::RectanglePlacement::fillDestination);return;}
    g.setColour(ui::muted.withAlpha(.7f));if(avatar){g.fillEllipse(r.withSizeKeepingCentre(r.getWidth()*.35f,r.getHeight()*.35f).translated(0,-r.getHeight()*.16f));g.fillRoundedRectangle(r.withTrimmedTop(r.getHeight()*.58f).reduced(r.getWidth()*.18f,0),r.getWidth()*.15f);return;}
    auto box=r.reduced(r.getWidth()*.22f);g.drawRoundedRectangle(box,3,1.5f);
    if(gear=="pedal"){g.fillEllipse(box.withSizeKeepingCentre(box.getWidth()*.22f,box.getWidth()*.22f).translated(0,box.getHeight()*.2f));}
    else{g.drawEllipse(box.reduced(box.getWidth()*.18f),1.5f);g.drawLine(box.getX()+3,box.getY()+5,box.getRight()-3,box.getY()+5,1.5f);}
}
}

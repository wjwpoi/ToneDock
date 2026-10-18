#pragma once
#include "Client.h"
#include "Library.h"
#include "Theme.h"
#include <map>
#include <set>

namespace tonedock {
juce::String compactCount(int);
juce::String publishedLabel(const juce::String&,juce::Time now=juce::Time::getCurrentTime());
juce::String toneFacts(const t3k::Tone&);
juce::var toneSummary(const t3k::Tone&);
void rememberTone(const t3k::Tone&,const juce::File&);
juce::Array<t3k::Tone> recentTones(const juce::File&);

// Public artwork only: no OAuth headers, with a bounded memory/disk cache.
class CatalogArtwork : public juce::ChangeBroadcaster {
public:
    CatalogArtwork()=default;
    ~CatalogArtwork(){callbacks.reset();workers.removeAllJobs(true,-1);}
    juce::Image get(const juce::String&);
    void put(const juce::String&,juce::Image);
    static juce::Image decode(const juce::MemoryBlock&);
    static bool allowed(const juce::String&);
private:
    struct Entry {juce::Image image;juce::uint64 used=0;};
    std::map<juce::String,Entry> images;
    std::set<juce::String> pending;
    juce::uint64 tick=0;
    juce::ThreadPool workers{2};
    t3k::AsyncScope callbacks;
};
void paintArtwork(juce::Graphics&,juce::Rectangle<int>,const juce::Image&,const juce::String& gear,bool avatar=false);
}

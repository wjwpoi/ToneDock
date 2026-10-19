#pragma once
#include "Processor.h"
#include "AuthFlow.h"
#include "Localization.h"
#include "Theme.h"
#include "CatalogMedia.h"

namespace tonedock {
class OnlineBrowser;
class Editor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget,
    public juce::TextDragAndDropTarget,public juce::DragAndDropContainer,public juce::DragAndDropTarget,private juce::Timer {
public:
    explicit Editor(Processor&);
    ~Editor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override{return true;}
    void filesDropped(const juce::StringArray& files,int,int) override{processor.importFiles(files);}
    bool isInterestedInTextDrag(const juce::String& text) override{return parseToneLink(text).has_value();}
    void textDropped(const juce::String& text,int,int) override;
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    void openTone(int id=0);
    void showDevicePage(bool show);
    void showOnlineLibrary(bool show);
    void showCatalogDemo();
    void prepareScreenshot(){tooltips.setMillisecondsBeforeTipAppears(60000); }
    void focusOnlineTone(int id);
    void showCatalogChain(){onlineExpanded=false;showOnlineLibrary(true);}
private:
    struct ModelRows;class Pedal;class Board;
    void timerCallback() override;
    void refreshLibrary();void selectModel(int);void selectStage(int);
    void editChain(const std::function<void(std::vector<StageSpec>&)>&);
    void syncChain();void loadSelectedModel();void moveStage(int);void duplicateStage();
    int workspaceX() const{return onlineVisible?juce::jlimit(440,520,getWidth()*36/100):336;}
    void loadOnlineAt(int toneId,int modelId,juce::Point<int>);
    void chooseImport();void preset(bool save);
    Processor& processor;
    ui::Theme theme;
    juce::SharedResourcePointer<CatalogArtwork> artwork;
    std::map<juce::String,juce::String> modelArtwork;
    juce::TooltipWindow tooltips{this,600};
    std::unique_ptr<ModelRows> modelRows;
    std::unique_ptr<Board> board;
    juce::Viewport chainViewport;
    std::vector<std::unique_ptr<juce::TextButton>> overview;
    std::unique_ptr<juce::Component> devicePage;
    juce::ListBox libraryList;
    juce::Label emptyLibrary;
    juce::TextEditor search,categoryEdit,tagsEdit,link;
    juce::ComboBox categoryFilter,effectChoice;
    juce::StringArray categoryValues;
    juce::TextButton importButton{ui::text("Import")},cloudButton{ui::text("TONE3000")},loadButton{ui::text("Load model")},favoriteButton{ui::text("Favorite")},
        metadataButton{ui::text("Save tags")},removeModelButton{ui::text("Remove")},addButton{ui::text("+ Add")},deleteButton{ui::text("Delete")},upButton{ui::text("Up")},downButton{ui::text("Down")},
        bypassStageButton{ui::text("Enable / bypass")},audioButton{ui::text("Audio settings")},saveButton{ui::text("Save preset")},openButton{ui::text("Open preset")},linkButton{ui::text("Open link")};
    juce::TextButton workButton{ui::text("效果链")},onlineButton{ui::text("在线音色")},duplicateButton{ui::text("复制效果")};
    juce::Slider inputGain,outputGain;
    juce::ToggleButton bypass{ui::text("Bypass")};
    std::array<juce::Slider,4> knobs;std::array<juce::Label,4> knobLabels;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment,outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<OnlineBrowser> browser;
    std::vector<ModelEntry> filtered;
    std::vector<StageSpec> visibleChain;
    juce::String selectedStageId,displayStatus;
    bool devicePageVisible=false;
    bool onlineVisible=false;
    bool onlineExpanded=true;
    bool catalogDemo=false;
    int version=-1;
    int refreshTick=0;
    juce::int64 libraryModified=0;
    t3k::AsyncScope scope;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Editor)
};
}

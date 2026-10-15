#pragma once
#include "Localization.h"

namespace tonedock::ui {
inline const juce::Colour background{0xff101613}, panel{0xff19221d}, raised{0xff222e26},
    line{0xff35463b}, ink{0xffedf3e9}, muted{0xff99ad9f}, accent{0xffc2ed88};

class Theme : public juce::LookAndFeel_V4 {
public:
    Theme() {
        setDefaultSansSerifTypefaceName(fontName());
        setColour(juce::ResizableWindow::backgroundColourId,background);
        setColour(juce::TextButton::buttonColourId,raised);
        setColour(juce::TextButton::buttonOnColourId,accent);
        setColour(juce::TextButton::textColourOffId,ink);
        setColour(juce::TextButton::textColourOnId,background);
        setColour(juce::ComboBox::backgroundColourId,raised);
        setColour(juce::ComboBox::textColourId,ink);
        setColour(juce::ComboBox::outlineColourId,line);
        setColour(juce::ComboBox::arrowColourId,muted);
        setColour(juce::PopupMenu::backgroundColourId,panel);
        setColour(juce::PopupMenu::textColourId,ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,line);
        setColour(juce::PopupMenu::highlightedTextColourId,accent);
        setColour(juce::Label::textColourId,ink);
        setColour(juce::TextEditor::backgroundColourId,background);
        setColour(juce::TextEditor::textColourId,ink);
        setColour(juce::TextEditor::outlineColourId,line);
        setColour(juce::TextEditor::focusedOutlineColourId,accent);
        setColour(juce::TextEditor::highlightColourId,line);
        setColour(juce::TextEditor::highlightedTextColourId,ink);
        setColour(juce::CaretComponent::caretColourId,accent);
        setColour(juce::Slider::rotarySliderFillColourId,accent);
        setColour(juce::Slider::rotarySliderOutlineColourId,line);
        setColour(juce::Slider::thumbColourId,accent);
        setColour(juce::Slider::trackColourId,accent);
        setColour(juce::Slider::backgroundColourId,line);
        setColour(juce::Slider::textBoxTextColourId,ink);
        setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        setColour(juce::ToggleButton::textColourId,ink);
        setColour(juce::ToggleButton::tickColourId,accent);
        setColour(juce::ToggleButton::tickDisabledColourId,muted);
        setColour(juce::ScrollBar::thumbColourId,line.brighter(.2f));
        setColour(juce::ListBox::backgroundColourId,background);
    }
    juce::Font getTextButtonFont(juce::TextButton&,int) override{return font(14);}
    juce::Font getComboBoxFont(juce::ComboBox&) override{return font(14);}
    juce::Font getLabelFont(juce::Label& label) override{return font(label.getFont().getHeight());}
    juce::Font getPopupMenuFont() override{return font(14);}
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour& c,bool hover,bool down) override {
        auto area=b.getLocalBounds().toFloat().reduced(.5f);
        g.setColour(c.withMultipliedAlpha(b.isEnabled()?1.f:.4f).brighter(down?.12f:hover?.06f:0.f));
        g.fillRoundedRectangle(area,7);g.setColour(b.getToggleState()?accent:line);g.drawRoundedRectangle(area,7,1);
    }
};
inline void field(juce::TextEditor& e,const juce::String& placeholder) {
    e.setFont(font(14));e.setTextToShowWhenEmpty(text(placeholder),muted);e.setIndents(10,7);
}
inline void card(juce::Graphics& g,juce::Rectangle<int> bounds) {
    g.setColour(panel);g.fillRoundedRectangle(bounds.toFloat(),12);
    g.setColour(line.withAlpha(.6f));g.drawRoundedRectangle(bounds.toFloat().reduced(.5f),12,1);
}
inline void meter(juce::Graphics& g,juce::Rectangle<int> bounds,float level) {
    auto area=bounds.toFloat();g.setColour(line);g.fillRoundedRectangle(area,3);
    // A dB scale makes quiet instrument signals readable without changing gain.
    auto fraction=juce::jlimit(0.f,1.f,(juce::Decibels::gainToDecibels(level,-60.f)+60.f)/60.f);
    g.setColour(level>=.99f?juce::Colour(0xffffa477):accent);g.fillRoundedRectangle(area.withWidth(area.getWidth()*fraction),3);
}
}

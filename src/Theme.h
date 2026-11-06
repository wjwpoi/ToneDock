#pragma once
#include <JuceHeader.h>

// Light palette shared by the editor and the standalone audio setup.
namespace tonedock::theme {
inline const juce::Colour background{0xfff6f7f5}, sidebar{0xfffbfbfa}, panel{0xffffffff}, card{0xfff1f3f0},
    border{0xffd9ddd7}, divider{0xffe3e6e1}, track{0xffe6e9e4},
    ink{0xff1b1f1d}, body{0xff3d4541}, muted{0xff5e6a63},
    accent{0xff2f6bff}, accentSoft{0xffeaf0ff}, accentLine{0xffb9ccff},
    warn{0xffe8590c}, warnSoft{0xfffff4e5};

inline void apply(juce::LookAndFeel_V4& lf) {
    lf.setColourScheme(juce::LookAndFeel_V4::getLightColourScheme());
    auto scheme=lf.getCurrentColourScheme();
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::windowBackground,background);
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::widgetBackground,panel);
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::menuBackground,panel);
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::outline,border);
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::defaultText,ink);
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::defaultFill,accent);
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::highlightedText,juce::Colours::white);
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::highlightedFill,accent);
    scheme.setUIColour(juce::LookAndFeel_V4::ColourScheme::menuText,ink);
    lf.setColourScheme(scheme);
    lf.setColour(juce::ResizableWindow::backgroundColourId,background);
    lf.setColour(juce::TextButton::buttonColourId,panel);lf.setColour(juce::TextButton::buttonOnColourId,accent);
    lf.setColour(juce::TextButton::textColourOffId,ink);lf.setColour(juce::TextButton::textColourOnId,juce::Colours::white);
    lf.setColour(juce::ComboBox::backgroundColourId,panel);lf.setColour(juce::ComboBox::textColourId,ink);
    lf.setColour(juce::ComboBox::outlineColourId,border);lf.setColour(juce::ComboBox::arrowColourId,muted);
    lf.setColour(juce::PopupMenu::backgroundColourId,panel);lf.setColour(juce::PopupMenu::textColourId,ink);
    lf.setColour(juce::PopupMenu::highlightedBackgroundColourId,accentSoft);lf.setColour(juce::PopupMenu::highlightedTextColourId,ink);
    lf.setColour(juce::Label::textColourId,ink);
    lf.setColour(juce::ToggleButton::textColourId,ink);lf.setColour(juce::ToggleButton::tickColourId,accent);
    lf.setColour(juce::ToggleButton::tickDisabledColourId,muted);
    lf.setColour(juce::Slider::rotarySliderFillColourId,accent);lf.setColour(juce::Slider::rotarySliderOutlineColourId,track);
    lf.setColour(juce::Slider::thumbColourId,accent);lf.setColour(juce::Slider::trackColourId,accent);
    lf.setColour(juce::Slider::backgroundColourId,track);lf.setColour(juce::Slider::textBoxTextColourId,ink);
    lf.setColour(juce::Slider::textBoxBackgroundColourId,panel);lf.setColour(juce::Slider::textBoxOutlineColourId,border);
    lf.setColour(juce::TextEditor::backgroundColourId,panel);lf.setColour(juce::TextEditor::textColourId,ink);
    lf.setColour(juce::TextEditor::outlineColourId,border);lf.setColour(juce::TextEditor::focusedOutlineColourId,accent);
    lf.setColour(juce::TextEditor::highlightColourId,accentSoft);
    lf.setColour(juce::ListBox::backgroundColourId,background);lf.setColour(juce::ScrollBar::thumbColourId,border);
}
}

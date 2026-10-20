#pragma once
#include "Localization.h"

namespace tonedock::ui {
inline const juce::Colour background{0xfff6f8fc}, panel{0xffffffff}, raised{0xfff1f3f4},
    line{0xffdadce0}, ink{0xff202124}, muted{0xff5f6368}, accent{0xff0b57d0},
    selected{0xffe8f0fe}, accentSoft{0xffd3e3fd}, error{0xffb3261e}, success{0xff137333};
inline void buttonStyle(juce::Button& b,const juce::String& role){b.getProperties().set("ui-role",role);}
inline void icon(juce::Graphics& g,juce::Rectangle<float> area,int kind){
    juce::Path p;
    if(kind==0){for(float x:{1.f,9.f,17.f})p.addRoundedRectangle(x,7,6,10,1.5f);p.startNewSubPath(7,12);p.lineTo(9,12);p.startNewSubPath(15,12);p.lineTo(17,12);}
    else if(kind==1){for(float x:{2.f,14.f})for(float y:{2.f,14.f})p.addRoundedRectangle(x,y,8,8,2);}
    else if(kind==2){for(float x:{4.f,12.f,20.f}){p.startNewSubPath(x,2);p.lineTo(x,22);}p.addEllipse(1,5,6,6);p.addEllipse(9,13,6,6);p.addEllipse(17,7,6,6);}
    else {p.startNewSubPath(2,13);p.lineTo(5,13);p.lineTo(8,5);p.lineTo(12,20);p.lineTo(16,4);p.lineTo(19,13);p.lineTo(22,13);}
    p.applyTransform(juce::AffineTransform::scale(area.getWidth()/24.f,area.getHeight()/24.f).translated(area.getX(),area.getY()));
    g.strokePath(p,juce::PathStrokeType(1.7f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
}
class Theme : public juce::LookAndFeel_V4 {
public:
    Theme(){
        setColourScheme(getLightColourScheme());setDefaultSansSerifTypefaceName(fontName());
        setColour(juce::ResizableWindow::backgroundColourId,background);setColour(juce::DocumentWindow::textColourId,ink);
        setColour(juce::TextButton::buttonColourId,panel);setColour(juce::TextButton::buttonOnColourId,selected);setColour(juce::TextButton::textColourOffId,ink);setColour(juce::TextButton::textColourOnId,accent);
        setColour(juce::ComboBox::backgroundColourId,panel);setColour(juce::ComboBox::textColourId,ink);setColour(juce::ComboBox::outlineColourId,line);setColour(juce::ComboBox::arrowColourId,muted);
        setColour(juce::PopupMenu::backgroundColourId,panel);setColour(juce::PopupMenu::textColourId,ink);setColour(juce::PopupMenu::highlightedBackgroundColourId,selected);setColour(juce::PopupMenu::highlightedTextColourId,accent);
        setColour(juce::Label::textColourId,ink);setColour(juce::TextEditor::backgroundColourId,panel);setColour(juce::TextEditor::textColourId,ink);setColour(juce::TextEditor::outlineColourId,line);setColour(juce::TextEditor::focusedOutlineColourId,accent);setColour(juce::TextEditor::highlightColourId,accentSoft);setColour(juce::TextEditor::highlightedTextColourId,ink);setColour(juce::CaretComponent::caretColourId,accent);
        setColour(juce::Slider::rotarySliderFillColourId,accent);setColour(juce::Slider::rotarySliderOutlineColourId,accentSoft);setColour(juce::Slider::thumbColourId,accent);setColour(juce::Slider::trackColourId,accent);setColour(juce::Slider::backgroundColourId,line);setColour(juce::Slider::textBoxTextColourId,ink);setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);setColour(juce::Slider::textBoxHighlightColourId,accentSoft);
        setColour(juce::ToggleButton::textColourId,ink);setColour(juce::ToggleButton::tickColourId,accent);setColour(juce::ToggleButton::tickDisabledColourId,muted);setColour(juce::ScrollBar::thumbColourId,juce::Colour(0xffbdc1c6));setColour(juce::ListBox::backgroundColourId,panel);setColour(juce::ListBox::outlineColourId,line);setColour(juce::ListBox::textColourId,ink);
        setColour(juce::TooltipWindow::backgroundColourId,ink);setColour(juce::TooltipWindow::textColourId,panel);setColour(juce::TooltipWindow::outlineColourId,juce::Colours::transparentBlack);setColour(juce::AlertWindow::backgroundColourId,panel);setColour(juce::AlertWindow::textColourId,ink);setColour(juce::AlertWindow::outlineColourId,line);
    }
    juce::Font getTextButtonFont(juce::TextButton&,int) override{return font(15);}
    juce::Font getComboBoxFont(juce::ComboBox&) override{return font(15);}
    juce::Font getLabelFont(juce::Label& label) override{return label.getFont();}
    juce::Font getPopupMenuFont() override{return font(15);}
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool hover,bool down) override{
        auto role=b.getProperties()["ui-role"].toString();bool nav=role.startsWith("nav"),on=b.getToggleState(),filled=role=="primary",tonal=role=="tonal";
        auto area=b.getLocalBounds().toFloat().reduced(1);if(nav)area={5,4,area.getWidth()-10,34};
        auto fill=filled?accent:tonal?accentSoft:on?selected:panel;
        if(nav || role=="plain")fill=on?accentSoft:(hover?selected:background);
        if(down)fill=fill.interpolatedWith(ink,.09f);else if(hover && !nav)fill=fill.interpolatedWith(accent,.06f);
        if(!b.isEnabled())fill=filled?line:raised;
        g.setColour(fill);g.fillRoundedRectangle(area,nav?18.f:juce::jmin(20.f,area.getHeight()/2));
        if(!nav && !filled && !tonal && !on && role!="plain"){g.setColour(line.withAlpha(b.isEnabled()?.85f:.4f));g.drawRoundedRectangle(area,juce::jmin(20.f,area.getHeight()/2),1);}
        if(b.hasKeyboardFocus(false)){g.setColour(accent);g.drawRoundedRectangle(area.reduced(2),nav?16.f:16.f,2);}
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool) override{
        auto role=b.getProperties()["ui-role"].toString();bool nav=role.startsWith("nav"),on=b.getToggleState();
        auto colour=role=="primary"?panel:role=="danger"?error:(on || role=="tonal")?accent:b.findColour(juce::TextButton::textColourOffId);
        g.setColour(colour.withMultipliedAlpha(b.isEnabled()?1.f:.4f));
        if(nav){icon(g,{float(b.getWidth()/2-12),9,24,24},role=="nav-chain"?0:role=="nav-catalog"?1:2);g.setFont(font(13));g.drawText(b.getButtonText(),0,43,b.getWidth(),24,juce::Justification::centred);return;}
        int padding=role=="compact"?4:10;g.setFont(font(role=="compact"?13:15));g.drawFittedText(b.getButtonText(),padding,0,b.getWidth()-2*padding,b.getHeight(),juce::Justification::centred,1);
    }
    void drawComboBox(juce::Graphics& g,int w,int h,bool,int,int,int,int,juce::ComboBox& box) override{
        auto area=juce::Rectangle<float>(.5f,.5f,float(w-1),float(h-1));g.setColour(box.isEnabled()?panel:raised);g.fillRoundedRectangle(area,10);g.setColour(box.hasKeyboardFocus(true)?accent:line.withAlpha(box.isEnabled()?1.f:.5f));g.drawRoundedRectangle(area,10,box.hasKeyboardFocus(true)?2.f:1.f);
        g.setColour(muted.withAlpha(box.isEnabled()?1.f:.35f));juce::Path p;p.startNewSubPath(float(w-24),float(h/2-2));p.lineTo(float(w-19),float(h/2+3));p.lineTo(float(w-14),float(h/2-2));g.strokePath(p,juce::PathStrokeType(1.6f));
    }
    void positionComboBoxText(juce::ComboBox& box,juce::Label& label) override{label.setBounds(12,1,box.getWidth()-42,box.getHeight()-2);label.setFont(font(15));}
    void fillTextEditorBackground(juce::Graphics& g,int w,int h,juce::TextEditor& e) override{g.setColour(e.findColour(juce::TextEditor::backgroundColourId));g.fillRoundedRectangle(.5f,.5f,float(w-1),float(h-1),10);}
    void drawTextEditorOutline(juce::Graphics& g,int w,int h,juce::TextEditor& e) override{g.setColour(e.hasKeyboardFocus(true)?accent:e.findColour(juce::TextEditor::outlineColourId));g.drawRoundedRectangle(.5f,.5f,float(w-1),float(h-1),10,e.hasKeyboardFocus(true)?2.f:1.f);}
};
inline void field(juce::TextEditor& e,const juce::String& placeholder){e.setFont(font(15));e.setTextToShowWhenEmpty(text(placeholder),muted);e.setIndents(12,9);}
inline void field(juce::TextEditor& e,const char* placeholder){field(e,juce::String::fromUTF8(placeholder));}
inline void card(juce::Graphics& g,juce::Rectangle<int> bounds){auto r=bounds.toFloat();g.setColour(juce::Colour(0x07000000));g.fillRoundedRectangle(r.translated(0,2),20);g.setColour(panel);g.fillRoundedRectangle(r,20);g.setColour(line.withAlpha(.45f));g.drawRoundedRectangle(r.reduced(.5f),20,1);}
inline void meter(juce::Graphics& g,juce::Rectangle<int> bounds,float level){auto area=bounds.toFloat();g.setColour(raised);g.fillRoundedRectangle(area,4);auto fraction=juce::jlimit(0.f,1.f,(juce::Decibels::gainToDecibels(level,-60.f)+60.f)/60.f);g.setColour(level>=.99f?error:accent);g.fillRoundedRectangle(area.withWidth(area.getWidth()*fraction),4);}
}

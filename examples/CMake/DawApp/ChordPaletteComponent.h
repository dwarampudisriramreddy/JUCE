#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MusicTheoryEngine.h"

class ChordPaletteComponent final : public juce::Component
{
public:
    ChordPaletteComponent();

    void setScaleInfo (const ScaleInfo& info);
    void clearSelection();
    void updateDisplay();

    std::function<void (const ChordInfo&)> onChordSelected;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    ScaleInfo scaleInfo;
    juce::Array<juce::Rectangle<int>> buttonBounds;
    int selectedIndex = -1;
};

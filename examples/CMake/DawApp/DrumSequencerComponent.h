#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "MidiEngine.h"

class DrumSequencerComponent final : public juce::Component
{
public:
    DrumSequencerComponent (MidiEngine& engine);

    void setTotalSteps (int steps);
    int getTotalSteps() const { return totalSteps; }
    void clearAll();
    void setPlayingTick (int tick);
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> getCellBounds (int row, int col) const;
    juce::Rectangle<int> getRowLabelBounds (int row) const;
    int getRowForY (int y) const;
    int getColForX (int x) const;

    MidiEngine& engine;
    int totalSteps = 32;

    static constexpr int keyWidth = 80;
    static constexpr int rowHeight = 28;
    static constexpr int cellWidth = 28;

    juce::StringArray drumNames = { "Kick", "Snare", "Hi-Hat", "Open Hat", "Clap",
                                     "H.Tom", "M.Tom", "L.Tom", "Rimshot" };
    juce::Array<juce::Colour> drumColours;

    int playingStep = -1;
};

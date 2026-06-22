#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MusicTheoryEngine.h"
#include "MidiEngine.h"

class PianoRollComponent final : public juce::Component,
                                 private juce::Timer
{
public:
    PianoRollComponent (MidiEngine& engine, bool isChordRoll = false);

    void setScaleInfo (const ScaleInfo& info) { scaleInfo = info; updateRows(); repaint(); }
    void setOctave (int oct) { octave = oct; updateRows(); repaint(); }
    int getOctave() const { return octave; }
    void setTotalSteps (int steps);
    int getTotalSteps() const { return totalSteps; }
    void addBars (int bars = 1);
    void clearAll();
    void setPlayingTick (int tick);
    void refresh() { updateRows(); repaint(); }

    std::function<void (int, int, int)> onNoteAdded;
    std::function<void (int, int)> onNoteRemoved;
    std::function<void (int, int, int)> onNoteDragged;

    struct ChordCell
    {
        bool active = false;
        juce::String chordSymbol;
        int startStep = 0;
        int length = 0;
        bool isContinuation = false;
        int startAt = 0;
    };

    std::vector<std::vector<ChordCell>>& getChordGrid() { return chordGrid; }
    void clearChordGrid() { chordGrid.clear(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void updateRows();
    juce::Rectangle<int> getCellBounds (int row, int col) const;
    juce::Rectangle<int> getRowLabelBounds (int row) const;
    int getRowForY (int y) const;
    int getColForX (int x) const;
    bool isNoteInScale (int noteIndex) const;
    void startNote (int row, int col);
    void extendNote (int row, int col);
    void removeNote (int row, int col);

    MidiEngine& engine;
    bool isChordRoll;
    ScaleInfo scaleInfo;
    int totalSteps = 32;
    int octave = 4;
    int visibleRows = 12;

    static constexpr int keyWidth = 70;
    static constexpr int rowHeight = 24;

    std::vector<std::vector<ChordCell>> chordGrid;

    struct DragState
    {
        bool dragging = false;
        int startRow = -1, startCol = -1;
        int currentRow = -1, currentCol = -1;
    } drag;

    int playingTick = -1;
    juce::Colour playheadColour { 0xff667eea };

    struct RowInfo
    {
        juce::String name;
        int chromaticIndex;
        bool isWhiteKey;
        bool inScale;
        int scaleDegree;
    };
    juce::Array<RowInfo> rows;
};

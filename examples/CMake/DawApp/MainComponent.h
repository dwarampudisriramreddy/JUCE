#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "MusicTheoryEngine.h"
#include "MidiEngine.h"
#include "PianoRollComponent.h"
#include "DrumSequencerComponent.h"
#include "ChordPaletteComponent.h"

class MainComponent final : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setupControls();
    void updateKeyAndScale();
    void clearAll();
    void togglePlayback();
    void exportMidi();

    MidiEngine midiEngine;

    // Controls
    juce::ComboBox keySelector, scaleSelector, trackSelector;
    juce::TextEditor bpmEditor;
    juce::Label theoryLabel;
    juce::TextButton playButton { "Play" }, stopButton { "Stop" }, clearButton { "Clear All" };
    juce::TextButton addBarButton { "+ Add Bar" }, exportButton { "Export MIDI" };

    // Panels
    ChordPaletteComponent chordPalette;
    PianoRollComponent chordRoll;
    PianoRollComponent melodyRoll;
    DrumSequencerComponent drumSequencer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

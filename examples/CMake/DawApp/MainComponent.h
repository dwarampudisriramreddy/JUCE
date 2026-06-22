#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
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
    void importMidi();
    void exportMidi();
    void toggleRecording();
    void toggleTheme();
    void updateOscType();

    MidiEngine midiEngine;

    // Controls
    juce::ComboBox keySelector, scaleSelector, trackSelector, oscTypeSelector;
    juce::TextEditor bpmEditor;
    juce::Label theoryLabel;
    juce::TextButton playButton { "Play" }, stopButton { "Stop" }, clearButton { "Clear" };
    juce::TextButton addBarButton { "+ Bar" }, exportButton { "Export" };
    juce::TextButton importButton { "Import MIDI" }, recordButton { "Record" }, themeButton { "Light" };

    // Volume sliders
    juce::Slider melodyVolSlider, chordVolSlider, drumVolSlider;

    // Octave controls
    juce::TextButton octUpButton { ">" }, octDownButton { "<" };
    juce::Label octLabel;

    // Panels
    ChordPaletteComponent chordPalette;
    PianoRollComponent chordRoll;
    PianoRollComponent melodyRoll;
    DrumSequencerComponent drumSequencer;

    int melodyOctave = 4;
    bool isDarkTheme = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

#include "MainComponent.h"

MainComponent::MainComponent()
    : chordRoll (midiEngine, true),
      melodyRoll (midiEngine, false),
      drumSequencer (midiEngine)
{
    setSize (1200, 800);

    setupControls();

    chordPalette.onChordSelected = [this] (const ChordInfo&) {};

    chordRoll.onNoteAdded = [this] (int, int, int) {};

    melodyRoll.onNoteAdded = [this] (int, int, int) {};

    melodyRoll.onNoteDragged = [this] (int, int, int) {};

    midiEngine.onPlaybackStopped = [this]
    {
        playButton.setButtonText ("Play");
    };

    updateKeyAndScale();
}

MainComponent::~MainComponent()
{
    midiEngine.stop();
}

void MainComponent::setupControls()
{
    addAndMakeVisible (keySelector);
    for (auto& k : MusicTheoryEngine::getNoteNames())
        keySelector.addItem (k, keySelector.getNumItems() + 1);
    keySelector.setSelectedItemIndex (0);

    addAndMakeVisible (scaleSelector);
    for (auto& s : MusicTheoryEngine::getScaleTypes())
        scaleSelector.addItem (s, scaleSelector.getNumItems() + 1);
    scaleSelector.setSelectedItemIndex (0);

    keySelector.onChange = [this] { updateKeyAndScale(); };
    scaleSelector.onChange = [this] { updateKeyAndScale(); };

    addAndMakeVisible (bpmEditor);
    bpmEditor.setText ("120");
    bpmEditor.setInputRestrictions (3, "0123456789");
    bpmEditor.onTextChange = [this]
    {
        int bpm = bpmEditor.getText().getIntValue();
        if (bpm >= 40 && bpm <= 200)
            midiEngine.setBpm (bpm);
    };

    addAndMakeVisible (trackSelector);
    trackSelector.addItem ("All", 1);
    trackSelector.addItem ("Chords", 2);
    trackSelector.addItem ("Melody", 3);
    trackSelector.addItem ("Drums", 4);
    trackSelector.setSelectedItemIndex (0);

    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (addBarButton);
    addAndMakeVisible (exportButton);
    addAndMakeVisible (theoryLabel);

    playButton.onClick = [this] { togglePlayback(); };
    stopButton.onClick = [this] { midiEngine.stop(); playButton.setButtonText ("Play"); };
    clearButton.onClick = [this] { clearAll(); };
    addBarButton.onClick = [this] { chordRoll.addBars(); melodyRoll.addBars(); drumSequencer.setTotalSteps (melodyRoll.getTotalSteps()); };
    exportButton.onClick = [this] { exportMidi(); };

    addAndMakeVisible (chordPalette);
    addAndMakeVisible (chordRoll);
    addAndMakeVisible (melodyRoll);
    addAndMakeVisible (drumSequencer);
}

void MainComponent::updateKeyAndScale()
{
    auto key = keySelector.getText();
    auto scale = scaleSelector.getText();
    auto info = MusicTheoryEngine::getScaleInfo (key, scale);

    chordPalette.setScaleInfo (info);
    chordRoll.setScaleInfo (info);
    melodyRoll.setScaleInfo (info);

    chordRoll.refresh();
    melodyRoll.refresh();

    theoryLabel.setText (MusicTheoryEngine::getTheoryInfo (key, scale),
                         juce::dontSendNotification);

    resized();
}

void MainComponent::clearAll()
{
    melodyRoll.clearAll();
    chordRoll.clearAll();
    drumSequencer.clearAll();
}

void MainComponent::togglePlayback()
{
    if (midiEngine.isPlaying())
    {
        midiEngine.stop();
        playButton.setButtonText ("Play");
    }
    else
    {
        int bpm = bpmEditor.getText().getIntValue();
        if (bpm >= 40 && bpm <= 200)
            midiEngine.setBpm (bpm);
        midiEngine.setTotalSteps (melodyRoll.getTotalSteps());
        midiEngine.play();
        playButton.setButtonText ("Playing...");
    }
}

void MainComponent::exportMidi()
{
    auto chooser = std::make_shared<juce::FileChooser> ("Export MIDI",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("composition.mid"),
        "*.mid");

    auto flags = juce::FileBrowserComponent::saveMode
               | juce::FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (flags, [this, chooser] (const juce::FileChooser&)
    {
        auto file = chooser->getResult();
        if (file == juce::File{})
            return;

        juce::MidiFile midiFile;
        double ppq = PPQ;
        midiFile.setTicksPerQuarterNote ((int) ppq);

        juce::MidiMessageSequence melodyTrack, chordTrack, drumTrack;

        int tempo = bpmEditor.getText().getIntValue();
        auto tempoMeta = juce::MidiMessage::tempoMetaEvent (60000000 / tempo);
        melodyTrack.addEvent (tempoMeta, 0);

        int totalSteps = melodyRoll.getTotalSteps();
        for (int row = 0; row < 12; ++row)
            for (int step = 0; step < totalSteps; ++step)
            {
                auto& note = midiEngine.getMelodyStep (row, step);
                if (note.active)
                {
                    auto onMsg = juce::MidiMessage::noteOn (note.channel, note.midiNote, (float) note.velocity / 127.0f);
                    auto offMsg = juce::MidiMessage::noteOff (note.channel, note.midiNote);
                    melodyTrack.addEvent (onMsg, note.startTick);
                    melodyTrack.addEvent (offMsg, note.startTick + note.durationTicks);
                }
            }

        auto& chordGrid = chordRoll.getChordGrid();
        for (int row = 0; row < (int) chordGrid.size(); ++row)
            for (int step = 0; step < (int) chordGrid[(size_t) row].size() && step < totalSteps; ++step)
            {
                auto& cell = chordGrid[(size_t) row][(size_t) step];
                if (cell.active && ! cell.isContinuation)
                {
                    auto intervals = MusicTheoryEngine::getChordIntervals (cell.chordSymbol);
                    int rootNote = MusicTheoryEngine::noteNameToMidi (cell.chordSymbol, 3);
                    int tick = cell.startStep * (PPQ / 4);
                    int durTicks = cell.length * (PPQ / 4);

                    for (auto interval : intervals)
                    {
                        int midiNote = rootNote + interval;
                        if (midiNote >= 0 && midiNote <= 127)
                        {
                            auto onMsg = juce::MidiMessage::noteOn (1, midiNote, 0.63f);
                            auto offMsg = juce::MidiMessage::noteOff (1, midiNote);
                            chordTrack.addEvent (onMsg, tick);
                            chordTrack.addEvent (offMsg, tick + durTicks);
                        }
                    }
                }
            }

        static const int drumNotes[] = { 36, 38, 42, 46, 39, 48, 47, 45, 37 };
        for (int drum = 0; drum < 9; ++drum)
            for (int step = 0; step < totalSteps; ++step)
                if (midiEngine.getDrumStep (drum, step))
                {
                    int tick = step * (PPQ / 4);
                    auto onMsg = juce::MidiMessage::noteOn (9, drumNotes[drum], 0.79f);
                    auto offMsg = juce::MidiMessage::noteOff (9, drumNotes[drum]);
                    drumTrack.addEvent (onMsg, tick);
                    drumTrack.addEvent (offMsg, tick + (PPQ / 8));
                }

        melodyTrack.sort();
        chordTrack.sort();
        drumTrack.sort();

        midiFile.addTrack (melodyTrack);
        midiFile.addTrack (chordTrack);
        midiFile.addTrack (drumTrack);

        juce::FileOutputStream out (file);
        if (out.openedOk())
            midiFile.writeTo (out);
    });
}

void MainComponent::paint (juce::Graphics& g)
{
    auto bg = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    g.fillAll (bg);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop (50);

    auto tools = bounds.removeFromTop (40);
    const int margin = 4;

    // Header controls
    auto headerArea = header.reduced (margin);
    const int comboW = 120;

    keySelector.setBounds (headerArea.removeFromLeft (comboW));
    headerArea.removeFromLeft (margin);
    scaleSelector.setBounds (headerArea.removeFromLeft (comboW));
    headerArea.removeFromLeft (margin);

    auto bpmLabel = headerArea.removeFromLeft (40);
    bpmEditor.setBounds (headerArea.removeFromLeft (60));
    headerArea.removeFromLeft (margin);

    theoryLabel.setBounds (headerArea.removeFromLeft (headerArea.getWidth() / 2));

    // Tool buttons
    auto toolArea = tools.reduced (margin);
    playButton.setBounds (toolArea.removeFromLeft (80));
    toolArea.removeFromLeft (margin);
    stopButton.setBounds (toolArea.removeFromLeft (80));
    toolArea.removeFromLeft (margin);
    clearButton.setBounds (toolArea.removeFromLeft (90));
    toolArea.removeFromLeft (margin);
    addBarButton.setBounds (toolArea.removeFromLeft (90));
    toolArea.removeFromLeft (margin);
    exportButton.setBounds (toolArea.removeFromLeft (100));
    toolArea.removeFromLeft (margin);
    trackSelector.setBounds (toolArea.removeFromLeft (120));

    // Remaining space split into vertical sections
    auto remaining = bounds.reduced (margin);

    auto chordPaletteArea = remaining.removeFromTop (56);
    chordPalette.setBounds (chordPaletteArea);

    auto chordRollArea = remaining.removeFromTop (200);
    chordRoll.setBounds (chordRollArea);

    auto melodyArea = remaining.removeFromTop (300);
    melodyRoll.setBounds (melodyArea);

    auto drumArea = remaining;
    drumSequencer.setBounds (drumArea);
}

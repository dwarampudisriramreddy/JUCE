#include "MainComponent.h"

MainComponent::MainComponent()
    : chordRoll (midiEngine, true),
      melodyRoll (midiEngine, false),
      drumSequencer (midiEngine)
{
    setSize (1200, 800);

    midiEngine.formatManager.registerBasicFormats();
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

    // Oscillator type
    addAndMakeVisible (oscTypeSelector);
    oscTypeSelector.addItem ("Sine", 1);
    oscTypeSelector.addItem ("Saw", 2);
    oscTypeSelector.addItem ("Square", 3);
    oscTypeSelector.addItem ("Triangle", 4);
    oscTypeSelector.setSelectedItemIndex (3);
    oscTypeSelector.onChange = [this] { updateOscType(); };

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
    addAndMakeVisible (importButton);
    addAndMakeVisible (recordButton);
    addAndMakeVisible (themeButton);
    addAndMakeVisible (theoryLabel);

    playButton.onClick = [this] { togglePlayback(); };
    stopButton.onClick = [this] { midiEngine.stop(); playButton.setButtonText ("Play"); };
    clearButton.onClick = [this] { clearAll(); };
    addBarButton.onClick = [this] { chordRoll.addBars(); melodyRoll.addBars(); drumSequencer.setTotalSteps (melodyRoll.getTotalSteps()); };
    exportButton.onClick = [this] { exportMidi(); };
    importButton.onClick = [this] { importMidi(); };
    recordButton.onClick = [this] { toggleRecording(); };
    themeButton.onClick = [this] { toggleTheme(); };

    // Volume sliders
    for (auto* s : { &melodyVolSlider, &chordVolSlider, &drumVolSlider })
    {
        addAndMakeVisible (s);
        s->setSliderStyle (juce::Slider::LinearBar);
        s->setRange (0.0, 1.0, 0.01);
        s->setValue (0.8);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    }
    melodyVolSlider.setValue (0.8);
    chordVolSlider.setValue (0.7);
    drumVolSlider.setValue (0.75);

    melodyVolSlider.onValueChange = [this] { midiEngine.setMelodyVolume ((float) melodyVolSlider.getValue()); };
    chordVolSlider.onValueChange = [this] { midiEngine.setChordVolume ((float) chordVolSlider.getValue()); };
    drumVolSlider.onValueChange = [this] { midiEngine.setDrumVolume ((float) drumVolSlider.getValue()); };

    // Octave controls
    addAndMakeVisible (octDownButton);
    addAndMakeVisible (octUpButton);
    addAndMakeVisible (octLabel);
    octLabel.setText ("Oct 4", juce::dontSendNotification);
    octLabel.setJustificationType (juce::Justification::centred);

    octDownButton.onClick = [this]
    {
        if (melodyOctave > 2) { melodyOctave--; octLabel.setText ("Oct " + juce::String (melodyOctave), juce::dontSendNotification); melodyRoll.setOctave (melodyOctave); }
    };
    octUpButton.onClick = [this]
    {
        if (melodyOctave < 6) { melodyOctave++; octLabel.setText ("Oct " + juce::String (melodyOctave), juce::dontSendNotification); melodyRoll.setOctave (melodyOctave); }
    };

    addAndMakeVisible (chordPalette);
    addAndMakeVisible (chordRoll);
    addAndMakeVisible (melodyRoll);
    addAndMakeVisible (drumSequencer);
}

void MainComponent::updateOscType()
{
    OscType t = OscType::triangle;
    switch (oscTypeSelector.getSelectedItemIndex())
    {
        case 0: t = OscType::sine; break;
        case 1: t = OscType::saw; break;
        case 2: t = OscType::square; break;
        case 3: t = OscType::triangle; break;
    }
    for (auto* v : midiEngine.synthVoices)
        v->setOscType (t);
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
        playButton.setButtonText ("Stop");
    }
}

void MainComponent::importMidi()
{
    auto chooser = std::make_shared<juce::FileChooser> ("Import MIDI",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.mid;*.midi");

    auto chooserFlags = juce::FileBrowserComponent::openMode
                       | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (chooserFlags, [this, chooser] (const juce::FileChooser&)
    {
        auto file = chooser->getResult();
        if (file == juce::File{}) return;

        juce::FileInputStream stream (file);
        if (stream.openedOk())
        {
            juce::MidiFile midiFile;
            if (midiFile.readFrom (stream))
            {
                midiEngine.importFromMidi (midiFile);
                chordRoll.setTotalSteps (midiEngine.getTotalTicks() / (PPQ / 4));
                melodyRoll.setTotalSteps (midiEngine.getTotalTicks() / (PPQ / 4));
                drumSequencer.setTotalSteps (midiEngine.getTotalTicks() / (PPQ / 4));
                repaint();
            }
        }
    });
}

void MainComponent::toggleRecording()
{
    if (midiEngine.isRecording())
    {
        midiEngine.stopRecording();
        recordButton.setButtonText ("Record");
    }
    else
    {
        auto chooser = std::make_shared<juce::FileChooser> ("Save Recording",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile ("recording.wav"),
            "*.wav");

        auto chooserFlags = juce::FileBrowserComponent::saveMode
                           | juce::FileBrowserComponent::warnAboutOverwriting;

        chooser->launchAsync (chooserFlags, [this, chooser] (const juce::FileChooser&)
        {
            auto file = chooser->getResult();
            if (file == juce::File{}) return;

            midiEngine.startRecording (file);
            recordButton.setButtonText ("Stop Rec");
        });
    }
}

void MainComponent::toggleTheme()
{
    isDarkTheme = ! isDarkTheme;
    themeButton.setButtonText (isDarkTheme ? "Dark" : "Light");

    if (isDarkTheme)
    {
        getLookAndFeel().setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff1a1a2e));
        getLookAndFeel().setColour (juce::Label::textColourId, juce::Colour (0xffe0e0e0));
        getLookAndFeel().setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff2d2d44));
        getLookAndFeel().setColour (juce::TextEditor::textColourId, juce::Colour (0xffe0e0e0));
        getLookAndFeel().setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff667eea));
        getLookAndFeel().setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff2d2d44));
        getLookAndFeel().setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe0e0e0));
    }
    else
    {
        getLookAndFeel().setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xfff0f0f0));
        getLookAndFeel().setColour (juce::Label::textColourId, juce::Colour (0xff333333));
        getLookAndFeel().setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xffffffff));
        getLookAndFeel().setColour (juce::TextEditor::textColourId, juce::Colour (0xff333333));
        getLookAndFeel().setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff667eea));
        getLookAndFeel().setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xffffffff));
        getLookAndFeel().setColour (juce::PopupMenu::textColourId, juce::Colour (0xff333333));
    }

    repaint();
}

void MainComponent::exportMidi()
{
    auto chooser = std::make_shared<juce::FileChooser> ("Export MIDI",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("composition.mid"),
        "*.mid");

    auto chooserFlags = juce::FileBrowserComponent::saveMode
                       | juce::FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (chooserFlags, [this, chooser] (const juce::FileChooser&)
    {
        auto file = chooser->getResult();
        if (file == juce::File{}) return;

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
    int margin = 4;

    // --- Header row ---
    auto header = bounds.removeFromTop (44);
    auto headerArea = header.reduced (margin);

    keySelector.setBounds (headerArea.removeFromLeft (100));
    headerArea.removeFromLeft (margin);
    scaleSelector.setBounds (headerArea.removeFromLeft (100));
    headerArea.removeFromLeft (margin);

    auto bpmLabel = headerArea.removeFromLeft (36);
    bpmEditor.setBounds (headerArea.removeFromLeft (52));
    headerArea.removeFromLeft (margin);

    oscTypeSelector.setBounds (headerArea.removeFromLeft (90));
    headerArea.removeFromLeft (margin);

    theoryLabel.setBounds (headerArea.removeFromLeft (headerArea.getWidth() / 2));

    // --- Tool bar ---
    auto tools = bounds.removeFromTop (36);
    auto toolArea = tools.reduced (margin);

    playButton.setBounds (toolArea.removeFromLeft (70));
    toolArea.removeFromLeft (margin);
    stopButton.setBounds (toolArea.removeFromLeft (70));
    toolArea.removeFromLeft (margin);
    clearButton.setBounds (toolArea.removeFromLeft (70));
    toolArea.removeFromLeft (margin);
    addBarButton.setBounds (toolArea.removeFromLeft (60));
    toolArea.removeFromLeft (margin);
    exportButton.setBounds (toolArea.removeFromLeft (80));
    toolArea.removeFromLeft (margin);
    importButton.setBounds (toolArea.removeFromLeft (100));
    toolArea.removeFromLeft (margin);
    recordButton.setBounds (toolArea.removeFromLeft (80));
    toolArea.removeFromLeft (margin);
    themeButton.setBounds (toolArea.removeFromLeft (65));
    toolArea.removeFromLeft (margin);
    trackSelector.setBounds (toolArea.removeFromLeft (90));

    // --- Volume bar ---
    auto volBar = bounds.removeFromTop (24);
    auto volArea = volBar.reduced (margin);

    auto volLabelWidth = 48;
    auto volWidth = 100;

    auto melVolLabel = volArea.removeFromLeft (volLabelWidth);
    melodyVolSlider.setBounds (volArea.removeFromLeft (volWidth));
    volArea.removeFromLeft (margin * 2);
    auto chordVolLabel = volArea.removeFromLeft (volLabelWidth);
    chordVolSlider.setBounds (volArea.removeFromLeft (volWidth));
    volArea.removeFromLeft (margin * 2);
    auto drumVolLabel = volArea.removeFromLeft (volLabelWidth);
    drumVolSlider.setBounds (volArea.removeFromLeft (volWidth));

    volArea.removeFromLeft (margin * 4);
    octDownButton.setBounds (volArea.removeFromLeft (30));
    octLabel.setBounds (volArea.removeFromLeft (50));
    octUpButton.setBounds (volArea.removeFromLeft (30));

    // --- Panels ---
    auto remaining = bounds.reduced (margin);

    auto chordPaletteArea = remaining.removeFromTop (52);
    chordPalette.setBounds (chordPaletteArea);

    auto chordRollArea = remaining.removeFromTop (170);
    chordRoll.setBounds (chordRollArea);

    auto melodyArea = remaining.removeFromTop (260);
    melodyRoll.setBounds (melodyArea);

    auto drumArea = remaining;
    drumSequencer.setBounds (drumArea);
}

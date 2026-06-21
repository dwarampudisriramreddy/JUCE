#include "PianoRollComponent.h"

PianoRollComponent::PianoRollComponent (MidiEngine& e, bool isChord)
    : engine (e), isChordRoll (isChord)
{
    updateRows();
    startTimerHz (30);

    if (! isChordRoll)
        engine.resizeMelody (12, totalSteps);
}

void PianoRollComponent::setTotalSteps (int steps)
{
    totalSteps = steps;
    if (! isChordRoll)
        engine.resizeMelody (12, totalSteps);
    for (auto& arr : chordGrid)
    {
        if ((int) arr.size() < totalSteps)
            arr.resize (totalSteps);
    }
    repaint();
}

void PianoRollComponent::addBars (int bars)
{
    setTotalSteps (totalSteps + bars * STEPS_PER_BAR);
}

void PianoRollComponent::clearAll()
{
    if (! isChordRoll)
    {
        for (auto& arr : engine.melodyNotes)
            for (auto& note : arr)
                note.active = false;
        engine.resizeMelody (12, totalSteps);
    }
    chordGrid.clear();
    repaint();
}

void PianoRollComponent::setPlayingTick (int tick)
{
    playingTick = tick;
}

void PianoRollComponent::timerCallback()
{
    if (engine.isPlaying())
    {
        playingTick = engine.getCurrentTick();
        repaint();
    }
    else if (playingTick >= 0)
    {
        playingTick = -1;
        repaint();
    }
}

void PianoRollComponent::updateRows()
{
    rows.clear();

    if (isChordRoll)
    {
        for (int i = 0; i < 7; ++i)
        {
            RowInfo r;
            r.name = scaleInfo.chords.size() > i ? scaleInfo.chords[i].symbol : "-";
            r.chromaticIndex = i;
            r.isWhiteKey = true;
            r.inScale = true;
            r.scaleDegree = i + 1;
            rows.add (r);
        }
        visibleRows = rows.size();
        return;
    }

    struct NoteDef { juce::String name; bool white; };
    NoteDef allNotes[] = {
        { "C5", true }, { "B4", true }, { "Bb4", false },
        { "A4", true }, { "Ab4", false }, { "G4", true },
        { "F#4", false }, { "F4", true }, { "E4", true },
        { "Eb4", false }, { "D4", true }, { "C#4", false }
    };

    auto& notes = MusicTheoryEngine::getNoteNames();

    for (auto& nd : allNotes)
    {
        RowInfo r;
        r.name = nd.name;
        auto noteBase = nd.name.upToFirstOccurrenceOf ("5", false, false)
                            .upToFirstOccurrenceOf ("4", false, false);
        r.chromaticIndex = notes.indexOf (noteBase);
        if (r.chromaticIndex < 0) r.chromaticIndex = 0;
        r.isWhiteKey = nd.white;

        if (scaleInfo.noteNames.size() > 0)
        {
            int idx = -1;
            for (int s = 0; s < scaleInfo.noteNames.size(); ++s)
            {
                if (notes.indexOf (scaleInfo.noteNames[s]) == r.chromaticIndex)
                { idx = s; break; }
            }
            r.inScale = idx >= 0;
            r.scaleDegree = idx >= 0 ? idx + 1 : -1;
        }
        else
        {
            r.inScale = true;
            r.scaleDegree = -1;
        }
        rows.add (r);
    }
    visibleRows = rows.size();
}

juce::Rectangle<int> PianoRollComponent::getCellBounds (int row, int col) const
{
    auto b = getLocalBounds().reduced (2);
    auto keyArea = b.removeFromLeft (keyWidth);
    float colW = (float) b.getWidth() / (float) totalSteps;
    return { b.getX() + (int) (col * colW), b.getY() + row * rowHeight, (int) colW + 1, rowHeight };
}

juce::Rectangle<int> PianoRollComponent::getRowLabelBounds (int row) const
{
    auto b = getLocalBounds().reduced (2);
    return { b.getX(), b.getY() + row * rowHeight, keyWidth, rowHeight };
}

int PianoRollComponent::getRowForY (int y) const
{
    auto b = getLocalBounds().reduced (2);
    return juce::jlimit (0, visibleRows - 1, (y - b.getY()) / rowHeight);
}

int PianoRollComponent::getColForX (int x) const
{
    auto b = getLocalBounds().reduced (2);
    b.removeFromLeft (keyWidth);
    float colW = (float) b.getWidth() / (float) totalSteps;
    return juce::jlimit (0, totalSteps - 1, (int) ((x - b.getX()) / colW));
}

bool PianoRollComponent::isNoteInScale (int noteIndex) const
{
    return noteIndex < rows.size() ? rows[noteIndex].inScale : true;
}

void PianoRollComponent::startNote (int row, int col)
{
    if (row < 0 || row >= visibleRows || col < 0 || col >= totalSteps)
        return;

    if (isChordRoll)
    {
        if ((int) chordGrid.size() <= row)
            chordGrid.resize (row + 1);
        if ((int) chordGrid[(size_t) row].size() <= totalSteps)
            chordGrid[(size_t) row].resize ((size_t) totalSteps);

        auto& cell = chordGrid[(size_t) row][(size_t) col];
        if (cell.active)
        {
            cell.active = false;
            cell.length = 0;
            cell.isContinuation = false;
            if (onNoteRemoved) onNoteRemoved (row, col);
        }
        else
        {
            cell.active = true;
            cell.startStep = col;
            cell.length = 1;
            cell.chordSymbol = scaleInfo.chords.size() > row ? scaleInfo.chords[row].symbol : "";
            cell.isContinuation = false;
            if (onNoteAdded) onNoteAdded (row, col, 0);
        }
        repaint();
        return;
    }

    if (! isNoteInScale (row)) return;

    auto& note = engine.melodyNotes[row][col];
    if (note.active)
    {
        note.active = false;
        if (onNoteRemoved) onNoteRemoved (row, col);
    }
    else
    {
        auto noteBase = rows[row].name.upToFirstOccurrenceOf ("5", false, false)
                                     .upToFirstOccurrenceOf ("4", false, false);
        int octave = rows[row].name.contains ("5") ? 5 : 4;
        note.active = true;
        note.startTick = col * (PPQ / 4);
        note.durationTicks = PPQ / 4;
        note.midiNote = MusicTheoryEngine::noteNameToMidi (noteBase, octave);
        note.velocity = 90;
        note.channel = 0;
        if (onNoteAdded) onNoteAdded (row, col, note.midiNote);
    }
    repaint();
}

void PianoRollComponent::extendNote (int row, int col)
{
    if (row < 0 || row >= visibleRows || col < 0 || col >= totalSteps || row != drag.startRow)
        return;

    if (isChordRoll)
    {
        int start = juce::jmin (drag.startCol, col);
        int end = juce::jmax (drag.startCol, col);
        int len = end - start + 1;

        for (int i = start; i <= end; ++i)
        {
            if ((int) chordGrid.size() <= row)
                chordGrid.resize (row + 1);
            if ((int) chordGrid[row].size() <= totalSteps)
                chordGrid[row].resize (totalSteps);

            if (i == start)
            {
                chordGrid[row][i].active = true;
                chordGrid[row][i].startStep = start;
                chordGrid[row][i].length = len;
                chordGrid[row][i].chordSymbol = scaleInfo.chords.size() > row ? scaleInfo.chords[row].symbol : "";
                chordGrid[row][i].isContinuation = false;
            }
            else
            {
                chordGrid[row][i].active = true;
                chordGrid[row][i].isContinuation = true;
                chordGrid[row][i].startAt = start;
            }
        }
        repaint();
        return;
    }

    if (! isNoteInScale (row)) return;

    int start = juce::jmin (drag.startCol, col);
    int end = juce::jmax (drag.startCol, col);
    int len = end - start + 1;

    auto noteBase = rows[row].name.upToFirstOccurrenceOf ("5", false, false)
                                 .upToFirstOccurrenceOf ("4", false, false);
    int octave = rows[row].name.contains ("5") ? 5 : 4;

    for (int i = start; i <= end; ++i)
    {
        if (i == start)
        {
            auto& note = engine.melodyNotes[row][i];
            note.active = true;
            note.startTick = start * (PPQ / 4);
            note.durationTicks = len * (PPQ / 4);
            note.midiNote = MusicTheoryEngine::noteNameToMidi (noteBase, octave);
            note.velocity = 90;
            note.channel = 0;
        }
        else
        {
            engine.melodyNotes[row][i].active = false;
        }
    }
    repaint();
}

void PianoRollComponent::removeNote (int row, int col)
{
    if (row < 0 || row >= visibleRows || col < 0 || col >= totalSteps)
        return;

    if (isChordRoll)
    {
        if (row < (int) chordGrid.size() && col < (int) chordGrid[row].size())
        {
            auto& cell = chordGrid[row][col];
            if (cell.active)
            {
                int start = cell.isContinuation ? cell.startAt : col;
                int len = cell.isContinuation ? 0 : cell.length;
                for (int i = start; i < start + len && i < totalSteps; ++i)
                {
                    chordGrid[row][i].active = false;
                    chordGrid[row][i].length = 0;
                    chordGrid[row][i].isContinuation = false;
                }
                if (onNoteRemoved) onNoteRemoved (row, col);
            }
        }
        repaint();
        return;
    }

    auto& note = engine.melodyNotes[row][col];
    if (note.active)
    {
        note.active = false;
        if (onNoteRemoved) onNoteRemoved (row, col);
    }
    repaint();
}

void PianoRollComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced (2);
    auto keyArea = bounds.removeFromLeft (keyWidth);
    float colW = (float) bounds.getWidth() / (float) totalSteps;

    auto bg = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    auto darkerBg = bg.darker (0.3f);
    auto gridLineColour = bg.brighter (0.12f);
    auto barLineColour = getLookAndFeel().findColour (juce::TextEditor::focusedOutlineColourId);
    auto noteColour = juce::Colour (0xff667eea);
    auto activeColour = isChordRoll ? juce::Colour (0xff4ecdc4) : noteColour;
    auto textColour = getLookAndFeel().findColour (juce::Label::textColourId);

    g.fillAll (darkerBg);

    for (int row = 0; row < visibleRows; ++row)
    {
        bool whiteBg = isChordRoll || rows[row].isWhiteKey;

        for (int col = 0; col < totalSteps; ++col)
        {
            auto cell = getCellBounds (row, col);

            if (isChordRoll)      g.setColour (bg);
            else if (whiteBg)     g.setColour (bg);
            else                  g.setColour (bg.darker (0.15f));

            g.fillRect (cell);
            g.setColour (gridLineColour);
            g.drawRect (cell, 1);

            if (col % STEPS_PER_BAR == 0)
            {
                g.setColour (barLineColour);
                g.fillRect (cell.getX(), cell.getY(), 2, cell.getHeight());
            }
        }
    }

    // Draw notes
    if (isChordRoll)
    {
        for (int row = 0; row < (int) chordGrid.size() && row < visibleRows; ++row)
        {
            for (int col = 0; col < (int) chordGrid[row].size() && col < totalSteps; ++col)
            {
                auto& cell = chordGrid[row][col];
                if (! cell.active) continue;

                auto cellBounds = getCellBounds (row, col);
                if (cell.isContinuation)
                {
                    g.setColour (activeColour.withAlpha (0.4f));
                    g.fillRect (cellBounds);
                }
                else
                {
                    g.setColour (activeColour);
                    g.fillRoundedRectangle (cellBounds.toFloat(), 3.0f);
                    g.setColour (juce::Colours::white);
                    g.setFont (juce::FontOptions (11.0f));
                    g.drawText (cell.chordSymbol, cellBounds, juce::Justification::centred, false);
                }
            }
        }
    }
    else
    {
        for (int row = 0; row < (int) engine.melodyNotes.size() && row < visibleRows; ++row)
        {
            for (int col = 0; col < (int) engine.melodyNotes[row].size() && col < totalSteps; ++col)
            {
                auto& note = engine.melodyNotes[row][col];
                if (! note.active) continue;

                auto cellBounds = getCellBounds (row, col);
                g.setColour (noteColour);
                g.fillRoundedRectangle (cellBounds.toFloat(), 4.0f);
                g.setColour (noteColour.brighter (0.3f));
                g.drawRoundedRectangle (cellBounds.toFloat(), 4.0f, 1.0f);
            }
        }
    }

    // Row labels
    for (int row = 0; row < visibleRows; ++row)
    {
        auto labelBounds = getRowLabelBounds (row);

        if (isChordRoll)
        {
            g.setColour (bg.brighter (0.1f));
            g.fillRect (labelBounds);
            g.setColour (textColour);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (rows[row].name, labelBounds, juce::Justification::centred, false);
        }
        else
        {
            if (rows[row].isWhiteKey)
            {
                g.setColour (juce::Colour (0xffe8e8e8));
                g.fillRect (labelBounds);
                g.setColour (juce::Colour (0xff333333));
            }
            else
            {
                g.setColour (juce::Colour (0xff2b2d31));
                g.fillRect (labelBounds);
                g.setColour (juce::Colour (0xff999999));
            }
            g.setFont (juce::FontOptions (11.0f, rows[row].inScale ? juce::Font::bold : juce::Font::plain));
            g.drawText (rows[row].name, labelBounds, juce::Justification::centredLeft, false);

            if (rows[row].inScale && rows[row].scaleDegree > 0)
            {
                g.setColour (noteColour);
                g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
                g.drawText (juce::String (rows[row].scaleDegree), labelBounds,
                           juce::Justification::centredRight, false);
            }
        }
    }

    // Bar numbers
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    for (int col = 0; col < totalSteps; col += STEPS_PER_BAR)
    {
        auto cell = getCellBounds (0, col);
        auto barNumBounds = juce::Rectangle<int> (cell.getX() - (int) colW/2 + 1, cell.getY() - 16, (int) colW, 14);
        g.setColour (noteColour);
        g.fillRoundedRectangle (barNumBounds.toFloat(), 3.0f);
        g.setColour (juce::Colours::white);
        g.drawText (juce::String ((col / STEPS_PER_BAR) + 1), barNumBounds, juce::Justification::centred, false);
    }

    // Playhead
    if (playingTick >= 0)
    {
        int step = playingTick / (PPQ / 4);
        if (step < totalSteps)
        {
            auto cell = getCellBounds (0, step);
            auto phBounds = juce::Rectangle<int> (cell.getX(), bounds.getY(), 3, bounds.getHeight());
            g.setColour (playheadColour);
            g.fillRect (phBounds);
        }
    }
}

void PianoRollComponent::resized()
{
    int h = visibleRows * rowHeight + 20;
    setSize (getWidth(), h);
}

void PianoRollComponent::mouseDown (const juce::MouseEvent& e)
{
    int row = getRowForY (e.y);
    int col = getColForX (e.x);
    if (row < 0 || row >= visibleRows || col < 0 || col >= totalSteps) return;

    drag.dragging = true;
    drag.startRow = row;
    drag.startCol = col;
    drag.currentRow = row;
    drag.currentCol = col;

    bool hasNote = false;
    if (isChordRoll)
        hasNote = row < (int) chordGrid.size() && col < (int) chordGrid[row].size() && chordGrid[row][col].active;
    else
        hasNote = engine.melodyNotes[row][col].active;

    if (! hasNote)
        startNote (row, col);
    else
        removeNote (row, col);
}

void PianoRollComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! drag.dragging) return;
    drag.currentRow = getRowForY (e.y);
    drag.currentCol = getColForX (e.x);
}

void PianoRollComponent::mouseUp (const juce::MouseEvent&)
{
    if (! drag.dragging) return;

    if (drag.startRow != drag.currentRow || drag.startCol != drag.currentCol)
    {
        extendNote (drag.startRow, drag.currentCol);
        if (! isChordRoll && onNoteDragged)
        {
            int len = juce::jmax (1, std::abs (drag.currentCol - drag.startCol) + 1);
            onNoteDragged (drag.startRow, juce::jmin (drag.startCol, drag.currentCol), len);
        }
    }
    drag.dragging = false;
}

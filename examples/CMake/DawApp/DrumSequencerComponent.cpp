#include "DrumSequencerComponent.h"

DrumSequencerComponent::DrumSequencerComponent (MidiEngine& e) : engine (e)
{
    drumColours = {
        juce::Colour (0xffff6b6b), juce::Colour (0xffff6b6b),
        juce::Colour (0xffff6b6b), juce::Colour (0xffff6b6b),
        juce::Colour (0xffff6b6b), juce::Colour (0xffff6b6b),
        juce::Colour (0xffff6b6b), juce::Colour (0xffff6b6b),
        juce::Colour (0xffff6b6b)
    };
    engine.resizeDrums (9, totalSteps);
}

void DrumSequencerComponent::setTotalSteps (int steps)
{
    totalSteps = steps;
    engine.resizeDrums (9, totalSteps);
    repaint();
}

void DrumSequencerComponent::clearAll()
{
    for (int d = 0; d < 9; ++d)
        for (int s = 0; s < totalSteps; ++s)
            engine.setDrumStep (d, s, false);
    repaint();
}

void DrumSequencerComponent::setPlayingTick (int tick)
{
    int step = tick / (PPQ / 4);
    playingStep = (step < totalSteps) ? step : -1;
}

void DrumSequencerComponent::refresh()
{
    repaint();
}

juce::Rectangle<int> DrumSequencerComponent::getCellBounds (int row, int col) const
{
    auto b = getLocalBounds().reduced (2);
    auto keyArea = b.removeFromLeft (keyWidth);
    int gridWidth = b.getWidth();
    float colW = (float) gridWidth / (float) totalSteps;
    return juce::Rectangle<int> (b.getX() + (int) (col * colW),
                                 b.getY() + row * rowHeight,
                                 (int) colW + 1, rowHeight);
}

juce::Rectangle<int> DrumSequencerComponent::getRowLabelBounds (int row) const
{
    auto b = getLocalBounds().reduced (2);
    return juce::Rectangle<int> (b.getX(), b.getY() + row * rowHeight, keyWidth, rowHeight);
}

int DrumSequencerComponent::getRowForY (int y) const
{
    auto b = getLocalBounds().reduced (2);
    int row = (y - b.getY()) / rowHeight;
    return juce::jlimit (0, 8, row);
}

int DrumSequencerComponent::getColForX (int x) const
{
    auto b = getLocalBounds().reduced (2);
    b.removeFromLeft (keyWidth);
    int gridWidth = b.getWidth();
    float colW = (float) gridWidth / (float) totalSteps;
    int col = (int) ((x - b.getX()) / colW);
    return juce::jlimit (0, totalSteps - 1, col);
}

void DrumSequencerComponent::paint (juce::Graphics& g)
{
    auto bg = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    auto darkerBg = bg.darker (0.3f);
    auto gridLineColour = bg.brighter (0.12f);
    auto barLineColour = juce::Colour (0xff667eea);
    auto activeColour = juce::Colour (0xffff6b6b);
    auto textColour = getLookAndFeel().findColour (juce::Label::textColourId);

    g.fillAll (darkerBg);

    float colW = (float) (getWidth() - keyWidth - 2) / (float) totalSteps;

    for (int row = 0; row < 9; ++row)
    {
        for (int col = 0; col < totalSteps; ++col)
        {
            auto cell = getCellBounds (row, col);
            g.setColour (bg);
            g.fillRect (cell);
            g.setColour (gridLineColour);
            g.drawRect (cell, 1);

            if (col % STEPS_PER_BAR == 0)
            {
                g.setColour (barLineColour);
                g.fillRect (cell.getX(), cell.getY(), 2, cell.getHeight());
            }

            // Draw active drum hits
            if (engine.getDrumStep (row, col))
            {
                auto activeBounds = cell.reduced (4);
                g.setColour (activeColour);
                g.fillRoundedRectangle (activeBounds.toFloat(), 3.0f);
            }
        }
    }

    // Draw row labels
    for (int row = 0; row < 9; ++row)
    {
        auto labelBounds = getRowLabelBounds (row);
        g.setColour (activeColour);
        g.fillRect (labelBounds);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (drumNames[row], labelBounds, juce::Justification::centred, false);
    }

    // Draw bar numbers
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    for (int col = 0; col < totalSteps; col += STEPS_PER_BAR)
    {
        auto cell = getCellBounds (0, col);
        auto barNumBounds = juce::Rectangle<int> (cell.getX() - colW/2 + 1, cell.getY() - 16, colW, 14);
        g.setColour (activeColour);
        g.fillRoundedRectangle (barNumBounds.toFloat(), 3.0f);
        g.setColour (juce::Colours::white);
        g.drawText (juce::String ((col / STEPS_PER_BAR) + 1),
                   barNumBounds, juce::Justification::centred, false);
    }

    // Draw playhead
    if (playingStep >= 0)
    {
        auto cell = getCellBounds (0, playingStep);
        auto b = getLocalBounds().reduced (2);
        auto playheadBounds = juce::Rectangle<int> (cell.getX(), b.getY(), 3, b.getHeight());
        g.setColour (barLineColour);
        g.fillRect (playheadBounds);
    }
}

void DrumSequencerComponent::resized()
{
    setSize (getWidth(), 9 * rowHeight + 20);
}

void DrumSequencerComponent::mouseDown (const juce::MouseEvent& e)
{
    int row = getRowForY (e.y);
    int col = getColForX (e.x);

    if (row >= 0 && row < 9 && col >= 0 && col < totalSteps)
    {
        bool current = engine.getDrumStep (row, col);
        engine.setDrumStep (row, col, ! current);
        repaint();
    }
}

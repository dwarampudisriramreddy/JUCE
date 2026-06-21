#include "ChordPaletteComponent.h"

ChordPaletteComponent::ChordPaletteComponent()
{
}

void ChordPaletteComponent::setScaleInfo (const ScaleInfo& info)
{
    scaleInfo = info;
    selectedIndex = -1;
    resized();
    repaint();
}

void ChordPaletteComponent::clearSelection()
{
    selectedIndex = -1;
    repaint();
}

void ChordPaletteComponent::updateDisplay()
{
    repaint();
}

void ChordPaletteComponent::paint (juce::Graphics& g)
{
    auto bg = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    auto textColour = getLookAndFeel().findColour (juce::Label::textColourId);
    auto accentColour = juce::Colour (0xff667eea);
    auto borderColour = bg.brighter (0.2f);

    g.fillAll (bg);

    auto bounds = getLocalBounds().reduced (4);
    buttonBounds.clear();

    if (scaleInfo.chords.size() == 0)
    {
        g.setColour (textColour.withAlpha (0.5f));
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("Select a key and scale above", bounds, juce::Justification::centred, false);
        return;
    }

    float gap = 4.0f;
    float totalWidth = bounds.getWidth() - gap;
    float btnWidth = totalWidth / 7.0f - gap;
    float btnHeight = 50.0f;

    for (int i = 0; i < scaleInfo.chords.size(); ++i)
    {
        const auto& chord = scaleInfo.chords[i];
        float x = bounds.getX() + i * (btnWidth + gap);
        float y = bounds.getY();
        auto btnRect = juce::Rectangle<float> (x, y, btnWidth, btnHeight);
        buttonBounds.add (btnRect.toNearestInt());

        bool selected = (i == selectedIndex);

        if (selected)
        {
            g.setColour (accentColour);
            g.fillRoundedRectangle (btnRect, 6.0f);
            g.setColour (accentColour.brighter (0.3f));
            g.drawRoundedRectangle (btnRect, 6.0f, 2.0f);
        }
        else
        {
            g.setColour (bg.darker (0.5f));
            g.fillRoundedRectangle (btnRect, 6.0f);
            g.setColour (borderColour);
            g.drawRoundedRectangle (btnRect, 6.0f, 1.0f);
        }

        g.setColour (selected ? juce::Colours::white : textColour);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (chord.symbol, btnRect, juce::Justification::centredTop, false);

        g.setFont (juce::FontOptions (10.0f));
        auto roman = chord.romanNumeral;
        if (chord.isDiminished) roman += "\u00B0";
        g.drawText (roman, btnRect, juce::Justification::centredBottom, false);
    }
}

void ChordPaletteComponent::resized()
{
    int height = 60;
    setSize (getWidth(), height);
}

void ChordPaletteComponent::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < buttonBounds.size(); ++i)
    {
        if (buttonBounds[i].contains (e.x, e.y))
        {
            if (selectedIndex == i)
            {
                selectedIndex = -1;
            }
            else
            {
                selectedIndex = i;
                if (onChordSelected && i < scaleInfo.chords.size())
                    onChordSelected (scaleInfo.chords[i]);
            }
            repaint();
            return;
        }
    }
    selectedIndex = -1;
    repaint();
}

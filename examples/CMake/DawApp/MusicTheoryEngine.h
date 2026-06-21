#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

struct ChordInfo
{
    juce::String symbol;
    juce::String romanNumeral;
    int rootIndex;
    bool isMinor = false;
    bool isDiminished = false;
};

struct ScaleInfo
{
    juce::String key;
    juce::String scaleType;
    juce::Array<int> intervals;
    juce::Array<juce::String> noteNames;
    juce::Array<ChordInfo> chords;
};

class MusicTheoryEngine
{
public:
    static const juce::StringArray& getNoteNames()
    {
        static const juce::StringArray notes = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        return notes;
    }

    static juce::StringArray getScaleTypes()
    {
        return { "Major", "Natural Minor", "Dorian", "Mixolydian" };
    }

    static juce::Array<int> getIntervals (const juce::String& scaleType)
    {
        if (scaleType == "Major")          return { 0, 2, 4, 5, 7, 9, 11 };
        if (scaleType == "Natural Minor")  return { 0, 2, 3, 5, 7, 8, 10 };
        if (scaleType == "Dorian")         return { 0, 2, 3, 5, 7, 9, 10 };
        if (scaleType == "Mixolydian")     return { 0, 2, 4, 5, 7, 9, 10 };
        return { 0, 2, 4, 5, 7, 9, 11 };
    }

    static ScaleInfo getScaleInfo (const juce::String& key, const juce::String& scaleType)
    {
        ScaleInfo info;
        info.key = key;
        info.scaleType = scaleType;
        info.intervals = getIntervals (scaleType);

        int keyIndex = getNoteNames().indexOf (key);
        auto intervals = info.intervals;

        for (auto interval : intervals)
            info.noteNames.add (getNoteNames()[(keyIndex + interval) % 12]);

        juce::Array<juce::String> romanNumerals = { "I", "II", "III", "IV", "V", "VI", "VII" };

        for (int i = 0; i < info.noteNames.size(); ++i)
        {
            ChordInfo chord;
            chord.rootIndex = (keyIndex + intervals[i]) % 12;
            chord.symbol = info.noteNames[i];

            int thirdInterval = (intervals[(i + 2) % 7] - intervals[i] + 12) % 12;
            int fifthInterval = (intervals[(i + 4) % 7] - intervals[i] + 12) % 12;

            chord.isMinor = (thirdInterval == 3);
            chord.isDiminished = (fifthInterval == 6);

            if (chord.isDiminished)
                chord.symbol += "dim";
            else if (chord.isMinor)
                chord.symbol += "m";

            chord.romanNumeral = romanNumerals[i];
            if (chord.isMinor || chord.isDiminished)
                chord.romanNumeral = chord.romanNumeral.toLowerCase();
            if (chord.isDiminished)
                chord.romanNumeral += "\u00B0";

            info.chords.add (chord);
        }
        return info;
    }

    static juce::Array<int> getChordIntervals (const juce::String& chordSymbol)
    {
        bool isMinor = chordSymbol.contains ("m") && !chordSymbol.contains ("maj");
        bool isDim = chordSymbol.contains ("dim");

        if (isDim)  return { 0, 3, 6 };
        if (isMinor) return { 0, 3, 7 };
        return { 0, 4, 7 };
    }

    static int noteNameToMidi (const juce::String& noteName, int octave = 4)
    {
        int index = getNoteNames().indexOf (noteName);
        if (index < 0) return 60;
        return 12 + (octave * 12) + index;
    }

    static int chromaticIndex (const juce::String& noteName)
    {
        return getNoteNames().indexOf (noteName);
    }

    static juce::String midiToNoteName (int midiNote)
    {
        int octave = (midiNote / 12) - 1;
        int index = midiNote % 12;
        return getNoteNames()[index] + juce::String (octave);
    }

    static juce::String getTheoryInfo (const juce::String& key, const juce::String& scaleType)
    {
        auto msg = "Key of " + key + " " + scaleType + ". ";
        if (scaleType == "Major")
            msg += "Common progressions: I-IV-V-I, I-V-vi-IV, ii-V-I.";
        else if (scaleType == "Natural Minor")
            msg += "Common progressions: i-iv-v-i, i-VI-VII-i.";
        else if (scaleType == "Dorian")
            msg += "Dorian has a minor quality with a raised 6th. Common in jazz and folk.";
        else if (scaleType == "Mixolydian")
            msg += "Mixolydian has a major quality with a flatted 7th. Common in rock and blues.";
        return msg;
    }
};

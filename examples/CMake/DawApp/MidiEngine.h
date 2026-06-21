#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "MusicTheoryEngine.h"

#define PPQ 480
#define STEPS_PER_BAR 8

struct NoteEvent
{
    int startTick = 0;
    int durationTicks = PPQ / 4;
    int midiNote = 60;
    int velocity = 90;
    int channel = 0;
    bool active = false;
};

struct ChordStep
{
    bool active = false;
    juce::String chordSymbol;
    int startStep = 0;
    int length = 0;
    bool isContinuation = false;
    int startAt = 0;
};

class SynthSound final : public juce::SynthesiserSound
{
public:
    SynthSound() {}
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class SynthVoice final : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<SynthSound*> (s) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int) override
    {
        currentAngle = 0.0;
        level = velocity * 0.3;
        tailOff = 0.0;

        auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        angleDelta = cyclesPerSecond / getSampleRate();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            tailOff = 1.0;
        }
        else
        {
            clearCurrentNote();
            angleDelta = 0.0;
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (angleDelta <= 0.0)
            return;

        if (tailOff > 0.0)
        {
            while (--numSamples >= 0)
            {
                auto currentSample = (float) (std::sin (currentAngle) * level * tailOff);

                for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                    outputBuffer.addSample (i, startSample, currentSample);

                currentAngle += angleDelta;
                ++startSample;

                tailOff *= 0.999;

                if (tailOff <= 0.005)
                {
                    clearCurrentNote();
                    angleDelta = 0.0;
                    break;
                }
            }
        }
        else
        {
            while (--numSamples >= 0)
            {
                auto currentSample = (float) (std::sin (currentAngle) * level);

                for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                    outputBuffer.addSample (i, startSample, currentSample);

                currentAngle += angleDelta;
                ++startSample;
            }
        }
    }

private:
    double currentAngle = 0.0, angleDelta = 0.0, level = 0.0, tailOff = 0.0;
};

class DrumVoice final : public juce::SynthesiserVoice
{
public:
    DrumVoice (int drumNote) : drumNote (drumNote) {}

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<SynthSound*> (s) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int) override
    {
        if (midiNoteNumber != drumNote)
        {
            clearCurrentNote();
            return;
        }

        isActive = true;
        phase = 0;
        sampleRate = getSampleRate();
        amplitude = velocity * 0.5;

        if (drumNote == 36) { // Kick
            freq = 150.0; freqTarget = 40.0; env = 0.8; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.2);
        }
        else if (drumNote == 38) { // Snare
            isNoise = true; env = 0.5; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.1);
        }
        else if (drumNote == 42) { // Hi-hat
            isNoise = true; env = 0.3; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.05);
        }
        else if (drumNote == 46) { // Open hat
            isNoise = true; env = 0.25; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.3);
        }
        else if (drumNote == 39) { // Clap
            isNoise = true; env = 0.6; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.15);
        }
        else if (drumNote == 48) { // High tom
            freq = 200.0; freqTarget = 100.0; env = 0.7; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.2);
        }
        else if (drumNote == 47) { // Mid tom
            freq = 150.0; freqTarget = 80.0; env = 0.7; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.25);
        }
        else if (drumNote == 45) { // Low tom
            freq = 120.0; freqTarget = 60.0; env = 0.7; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.3);
        }
        else if (drumNote == 37) { // Rimshot
            isNoise = true;
            hasTone = true; toneFreq = 800.0; toneFreqTarget = 400.0;
            env = 0.4; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.08);
        }
    }

    void stopNote (float, bool) override
    {
        clearCurrentNote();
        isActive = false;
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (! isActive)
            return;

        while (--numSamples >= 0)
        {
            float sample = 0.0f;

            if (isNoise)
            {
                sample = (random.nextFloat() * 2.0f - 1.0f) * env * amplitude;
            }
            else
            {
                if (phase == 0)
                    currentPhase = 0.0;
                sample = (float) (std::sin (currentPhase) * env * amplitude);
                currentPhase += (freq + (freqTarget - freq) * (float) phase / envLen) / sampleRate;
                ++phase;
            }

            if (hasTone)
            {
                sample += (float) (std::sin (tonePhase) * env * amplitude * 0.5f);
                tonePhase += (toneFreq + (toneFreqTarget - toneFreq) * (float) phase / envLen) / sampleRate;
            }

            for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                outputBuffer.addSample (i, startSample, sample);

            ++startSample;
            ++phase;

            float decay = 1.0f - (float) phase / (float) envLen;
            env = juce::jmax (0.0f, envTarget + (env - envTarget) * decay);

            if (phase >= envLen || env <= 0.001f)
            {
                isActive = false;
                clearCurrentNote();
                break;
            }
        }
    }

private:
    int drumNote;
    bool isActive = false, isNoise = false, hasTone = false;
    int phase = 0, envLen = 0;
    double sampleRate = 44100.0, currentPhase = 0.0, tonePhase = 0.0;
    float env = 0.0f, envTarget = 0.0f, amplitude = 0.0f;
    float freq = 0.0f, freqTarget = 0.0f, toneFreq = 0.0f, toneFreqTarget = 0.0f;
    juce::Random random;
};

class MidiEngine
{
public:
    MidiEngine()
    {
        synth.addSound (new SynthSound());
        for (int i = 0; i < 4; ++i)
            synth.addVoice (new SynthVoice());

        drums.addSound (new SynthSound());
        drums.addVoice (new DrumVoice (36));
        drums.addVoice (new DrumVoice (38));
        drums.addVoice (new DrumVoice (42));
        drums.addVoice (new DrumVoice (46));
        drums.addVoice (new DrumVoice (39));
        drums.addVoice (new DrumVoice (48));
        drums.addVoice (new DrumVoice (47));
        drums.addVoice (new DrumVoice (45));
        drums.addVoice (new DrumVoice (37));

        deviceManager.initialiseWithDefaultDevices (2, 0);
        deviceManager.addAudioCallback (&player);
        player.setSource (&audioSource);
    }

    ~MidiEngine()
    {
        deviceManager.removeAudioCallback (&player);
        player.setSource (nullptr);
        synth.clearVoices();
        drums.clearVoices();
    }

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }

    void setBpm (int newBpm) { bpm = newBpm; }
    int getBpm() const { return bpm; }

    void setTotalSteps (int steps) { totalSteps = steps; }

    void setMelodyNote (int noteIndex, int step, const NoteEvent& note)
    {
        if (noteIndex < (int) melodyNotes.size() && step < (int) melodyNotes[noteIndex].size())
            melodyNotes[noteIndex][step] = note;
    }

    void clearMelodyNote (int noteIndex, int step)
    {
        if (noteIndex < (int) melodyNotes.size() && step < (int) melodyNotes[noteIndex].size())
            melodyNotes[noteIndex][step].active = false;
    }

    void resizeMelody (int numNotes, int numSteps)
    {
        melodyNotes.resize (numNotes);
        for (auto& arr : melodyNotes)
            arr.resize (numSteps);
    }

    NoteEvent& getMelodyStep (int noteIdx, int step)
    {
        return melodyNotes[noteIdx][step];
    }

    void setDrumStep (int drumIdx, int step, bool value)
    {
        if (drumIdx < (int) drumSteps.size() && step < (int) drumSteps[drumIdx].size())
            drumSteps[drumIdx][step] = value;
    }

    bool getDrumStep (int drumIdx, int step) const
    {
        if (drumIdx < (int) drumSteps.size() && step < (int) drumSteps[drumIdx].size())
            return drumSteps[drumIdx][step];
        return false;
    }

    void resizeDrums (int numDrums, int numSteps)
    {
        drumSteps.resize (numDrums);
        for (auto& arr : drumSteps)
            arr.resize (numSteps);
    }

    static constexpr int getDrumNote (int index)
    {
        int drumNotes[] = { 36, 38, 42, 46, 39, 48, 47, 45, 37 };
        return drumNotes[index];
    }

    void play()
    {
        audioSource.prepareToPlay (44100, 512);
        stop();

        totalTicks = totalSteps * (PPQ / 4);
        currentTick = 0;
        isPlayingFlag = true;

        if (timer == nullptr)
            timer = std::make_unique<PlaybackTimer> (*this);

        timer->startTimerHz (60);
    }

    void stop()
    {
        isPlayingFlag = false;
        if (timer != nullptr)
        {
            timer->stopTimer();
            timer = nullptr;
        }
        synth.allNotesOff (16, true);
        drums.allNotesOff (16, true);
    }

    bool isPlaying() const { return isPlayingFlag; }
    int getCurrentTick() const { return currentTick; }
    int getTotalTicks() const { return totalTicks; }

    std::function<void()> onPlaybackStopped;
    std::function<void (int tick)> onTick;

    juce::Synthesiser synth, drums;
    std::vector<std::vector<NoteEvent>> melodyNotes;
    std::vector<std::vector<bool>> drumSteps;

private:
    static constexpr double getStepDurationTicks() { return PPQ / 4.0; }

    struct AudioSourceImpl : public juce::AudioSource
    {
        AudioSourceImpl (MidiEngine& e) : engine (e) {}

        void prepareToPlay (int, double) override {}
        void releaseResources() override {}

        void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
        {
            auto numSamples = bufferToFill.numSamples;
            bufferToFill.clearActiveBufferRegion();

            if (! engine.isPlayingFlag)
                return;

            double sampleRate = 44100.0;
            double ticksPerSample = getStepDurationTicks() / (60.0 / engine.bpm / 4.0 * sampleRate);

            for (int i = 0; i < numSamples; ++i)
            {
                if (! engine.isPlayingFlag)
                    break;

                int tick = (int) engine.currentTick;

                if (tick >= engine.totalTicks)
                {
                    engine.isPlayingFlag = false;
                    if (engine.onPlaybackStopped)
                        engine.onPlaybackStopped();
                    break;
                }

                int step = tick / (int) getStepDurationTicks();
                int subTick = tick % (int) getStepDurationTicks();

                if (subTick == 0 && step < engine.totalSteps)
                {
                    if (engine.onTick)
                        engine.onTick (tick);

                    for (int n = 0; n < (int) engine.melodyNotes.size(); ++n)
                    {
                        auto& melNote = engine.melodyNotes[n][step];
                        if (melNote.active && melNote.startTick == tick)
                        {
                            engine.synth.noteOn (melNote.channel,
                                                  melNote.midiNote,
                                                  (float) melNote.velocity / 127.0f);
                            int offTick = melNote.startTick + melNote.durationTicks;
                            scheduledOffs.push_back ({ offTick, melNote.channel, melNote.midiNote });
                        }
                    }

                    static constexpr int drumNotes[] = { 36, 38, 42, 46, 39, 48, 47, 45, 37 };
                    for (int d = 0; d < (int) engine.drumSteps.size(); ++d)
                    {
                        if (engine.drumSteps[d][step])
                            engine.drums.noteOn (9, drumNotes[d], 0.8f);
                    }
                }

                for (int i2 = 0; i2 < (int) scheduledOffs.size();)
                {
                    if (scheduledOffs[i2].tick <= tick)
                    {
                        engine.synth.noteOff (scheduledOffs[i2].channel,
                                              scheduledOffs[i2].note, 0.0f, false);
                        scheduledOffs.erase (scheduledOffs.begin() + i2);
                    }
                    else
                    {
                        ++i2;
                    }
                }

                engine.currentTick += ticksPerSample;
            }
        }

        MidiEngine& engine;
        struct ScheduledOff { int tick; int channel; int note; };
        std::vector<ScheduledOff> scheduledOffs;
    };

    struct PlaybackTimer : public juce::Timer
    {
        PlaybackTimer (MidiEngine& e) : engine (e) {}
        void timerCallback() override
        {
            if (! engine.isPlayingFlag)
            {
                engine.stop();
                if (engine.onPlaybackStopped)
                    engine.onPlaybackStopped();
            }
        }
        MidiEngine& engine;
    };

    int bpm = 120;
    int totalSteps = 32;
    int totalTicks = 32 * (PPQ / 4);
    double currentTick = 0;
    bool isPlayingFlag = false;

    juce::AudioDeviceManager deviceManager;
    AudioSourceImpl audioSource { *this };
    juce::AudioSourcePlayer player;

    std::unique_ptr<PlaybackTimer> timer;

};

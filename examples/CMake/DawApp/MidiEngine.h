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

enum class OscType { sine, saw, square, triangle };

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
    SynthVoice() = default;

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<SynthSound*> (s) != nullptr;
    }

    void setOscType (OscType t) { oscType = t; }

    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int) override
    {
        phase = 0.0;
        level = velocity * 0.3;
        auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        phaseDelta = cyclesPerSecond / getSampleRate();
        amp = level;

        env.attack = 0.01f;
        env.decay = 0.1f;
        env.sustain = 0.7f;
        env.release = 0.3f;
        envState = EnvState::attack;
        envVal = 0.0f;
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            prevEnvVal = envVal;
            envState = EnvState::release;
        }
        else
        {
            clearCurrentNote();
            phaseDelta = 0.0;
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<double>& outputBuffer, int startSample, int numSamples) override
    {
        juce::ignoreUnused (outputBuffer, startSample, numSamples);
    }

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (phaseDelta <= 0.0)
            return;

        while (--numSamples >= 0)
        {
            float sample = 0.0f;

            switch (oscType)
            {
                case OscType::sine:
                    sample = std::sin (phase);
                    break;
                case OscType::saw:
                    sample = (float) (2.0 * (phase / juce::MathConstants<double>::twoPi - std::floor (phase / juce::MathConstants<double>::twoPi + 0.5)));
                    break;
                case OscType::square:
                    sample = std::sin (phase) > 0.0f ? 1.0f : -1.0f;
                    break;
                case OscType::triangle:
                    sample = (float) (4.0 * std::abs (2.0 * (phase / juce::MathConstants<double>::twoPi - std::floor (phase / juce::MathConstants<double>::twoPi + 0.5))) - 1.0);
                    break;
            }

            sample *= amp * getEnv();

            for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                outputBuffer.addSample (i, startSample, sample * 0.4f);

            phase += phaseDelta;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;

            ++startSample;

            if (envState == EnvState::idle)
            {
                clearCurrentNote();
                phaseDelta = 0.0;
                break;
            }
        }
    }

private:
    enum class EnvState { attack, decay, sustain, release, idle };

    float getEnv()
    {
        float sr = (float) getSampleRate();

        switch (envState)
        {
            case EnvState::attack:
                envVal += 1.0f / (env.attack * sr);
                if (envVal >= 1.0f) { envVal = 1.0f; envState = EnvState::decay; }
                return envVal;

            case EnvState::decay:
                envVal -= (1.0f - env.sustain) / (env.decay * sr);
                if (envVal <= env.sustain) { envVal = env.sustain; envState = EnvState::sustain; }
                return envVal;

            case EnvState::sustain:
                return env.sustain;

            case EnvState::release:
                envVal -= (prevEnvVal) / (env.release * sr);
                if (envVal <= 0.001f) { envVal = 0.0f; envState = EnvState::idle; }
                return envVal;

            default:
                return 0.0f;
        }
    }

    double phase = 0.0, phaseDelta = 0.0;
    float level = 0.0f, amp = 0.0f;
    OscType oscType = OscType::sine;

    struct Env { float attack = 0.01f, decay = 0.1f, sustain = 0.7f, release = 0.3f; } env;
    EnvState envState = EnvState::idle;
    float envVal = 0.0f, prevEnvVal = 0.0f;
};

class DrumVoice final : public juce::SynthesiserVoice
{
public:
    DrumVoice (int note) : drumNote (note) {}

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

        if (drumNote == 36) {
            freq = 150.0; freqTarget = 40.0; env = 0.8; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.2);
        }
        else if (drumNote == 38) {
            isNoise = true; env = 0.5; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.1);
        }
        else if (drumNote == 42) {
            isNoise = true; env = 0.3; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.05);
        }
        else if (drumNote == 46) {
            isNoise = true; env = 0.25; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.3);
        }
        else if (drumNote == 39) {
            isNoise = true; env = 0.6; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.15);
        }
        else if (drumNote == 48) {
            freq = 200.0; freqTarget = 100.0; env = 0.7; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.2);
        }
        else if (drumNote == 47) {
            freq = 150.0; freqTarget = 80.0; env = 0.7; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.25);
        }
        else if (drumNote == 45) {
            freq = 120.0; freqTarget = 60.0; env = 0.7; envTarget = 0.01;
            envLen = (int) (sampleRate * 0.3);
        }
        else if (drumNote == 37) {
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

    void renderNextBlock (juce::AudioBuffer<double>& outputBuffer, int startSample, int numSamples) override
    {
        juce::ignoreUnused (outputBuffer, startSample, numSamples);
    }

    void renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (! isActive)
            return;

        while (--numSamples >= 0)
        {
            float sample = 0.0f;

            if (isNoise)
                sample = (random.nextFloat() * 2.0f - 1.0f) * env * amplitude;
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
        for (int i = 0; i < 8; ++i)
        {
            auto* v = new SynthVoice();
            v->setOscType (OscType::triangle);
            synthVoices.add (v);
            synth.addVoice (v);
        }
        synth.addSound (new SynthSound());

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

        reverb.setParameters ({ 0.3f, 0.2f, 0.5f, 0.5f, 0.0f, 0.0f });

        deviceManager.initialiseWithDefaultDevices (2, 0);
        deviceManager.addAudioCallback (&player);
        player.setSource (&audioSource);
    }

    ~MidiEngine()
    {
        stopRecording();
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

    void setMelodyVolume (float v) { melodyVol = v; }
    void setChordVolume (float v) { chordVol = v; }
    void setDrumVolume (float v) { drumVol = v; }
    float getMelodyVolume() const { return melodyVol; }
    float getChordVolume() const { return chordVol; }
    float getDrumVolume() const { return drumVol; }

    void setReverbEnabled (bool e) { reverbEnabled = e; }
    bool getReverbEnabled() const { return reverbEnabled; }

    // MIDI Import - populate from a MIDI file
    void importFromMidi (const juce::MidiFile& midiFile)
    {
        int numTracks = midiFile.getNumTracks();
        if (numTracks == 0) return;

        int filePPQ = midiFile.getTimeFormat();
        if (filePPQ <= 0) filePPQ = PPQ;
        double tickScale = (double) PPQ / (double) filePPQ;

        juce::MidiMessageSequence merged;
        for (int t = 0; t < numTracks; ++t)
        {
            auto* track = midiFile.getTrack (t);
            if (track == nullptr) continue;
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                auto* event = track->getEventPointer (i);
                if (event != nullptr && (event->message.isNoteOn() || event->message.isNoteOff()))
                    merged.addEvent (event->message);
            }
        }
        merged.sort();

        int numSteps = 64;
        resizeMelody (12, numSteps);
        resizeDrums (9, numSteps);

        for (int i = 0; i < merged.getNumEvents(); ++i)
        {
            auto* onEvent = merged.getEventPointer (i);
            if (onEvent == nullptr || ! onEvent->message.isNoteOn()) continue;

            int note = onEvent->message.getNoteNumber();
            int fileTick = (int) onEvent->message.getTimeStamp();
            int ourTick = (int) (fileTick * tickScale);
            int step = ourTick / (PPQ / 4);
            float vel = onEvent->message.getFloatVelocity();

            if (step >= numSteps) continue;

            // Find matching note-off
            int offFileTick = fileTick + (filePPQ / 4);
            for (int j = i + 1; j < merged.getNumEvents(); ++j)
            {
                auto* offEvent = merged.getEventPointer (j);
                if (offEvent != nullptr && offEvent->message.isNoteOff()
                    && offEvent->message.getNoteNumber() == note)
                {
                    offFileTick = (int) offEvent->message.getTimeStamp();
                    break;
                }
            }
            int offOurTick = (int) (offFileTick * tickScale);
            int durSteps = (offOurTick - ourTick) / (PPQ / 4);
            if (durSteps < 1) durSteps = 1;

            int channel = onEvent->message.getChannel();

            if (channel == 10)
            {
                // Drum channel - map to drum grid
                static const int drumMap[] = { 36, 38, 42, 46, 39, 48, 47, 45, 37 };
                for (int d = 0; d < 9; ++d)
                {
                    if (drumMap[d] == note)
                    {
                        for (int s = 0; s < durSteps && step + s < numSteps; ++s)
                            setDrumStep (d, step + s, true);
                        break;
                    }
                }
            }
            else if (note >= 48 && note <= 83)
            {
                int row = 83 - note;
                if (row >= 0 && row < 12)
            {
                auto& ev = melodyNotes[row][step];
                ev.active = true;
                ev.startTick = ourTick;
                ev.durationTicks = offOurTick - ourTick;
                ev.midiNote = note;
                ev.velocity = (int) (vel * 127.0f);
                ev.channel = 0;
            }
            }
        }

        totalSteps = numSteps;
    }

    bool isRecording() const { return isRecordingFlag; }

    void startRecording (const juce::File& file)
    {
        stopRecording();

        auto* wavFormat = formatManager.findFormatForFileExtension ("wav");
        if (wavFormat == nullptr) return;

        recordingFile = file;
        recordingBuffer.reset();
        recordingBuffer = std::make_unique<juce::AudioBuffer<float>> (2, 0);
        recordSampleCount.store (0);
        isRecordingFlag = true;
    }

    void stopRecording()
    {
        if (! isRecordingFlag) return;
        isRecordingFlag = false;

        if (recordingBuffer != nullptr && recordingBuffer->getNumSamples() > 0)
        {
            auto* wavFormat = formatManager.findFormatForFileExtension ("wav");
            if (wavFormat == nullptr) return;

            std::unique_ptr<juce::FileOutputStream> fileOut (recordingFile.createOutputStream());
            if (fileOut == nullptr) return;

            std::unique_ptr<juce::AudioFormatWriter> writer (
                wavFormat->createWriterFor (fileOut.get(), 44100.0,
                                             recordingBuffer->getNumChannels(), 16, {}, 0));
            if (writer == nullptr) return;

            fileOut.release();
            writer->writeFromAudioSampleBuffer (*recordingBuffer, 0, recordingBuffer->getNumSamples());
        }
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
    juce::OwnedArray<SynthVoice> synthVoices;
    std::vector<std::vector<NoteEvent>> melodyNotes;
    std::vector<std::vector<bool>> drumSteps;
    juce::AudioFormatManager formatManager;

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
                            // Apply volume
                            float vol = (float) melNote.velocity / 127.0f * engine.melodyVol;
                            engine.synth.noteOn (melNote.channel,
                                                  melNote.midiNote, vol);
                            int offTick = melNote.startTick + melNote.durationTicks;
                            scheduledOffs.push_back ({ offTick, melNote.channel, melNote.midiNote });
                        }
                    }

                    static constexpr int drumNotes[] = { 36, 38, 42, 46, 39, 48, 47, 45, 37 };
                    for (int d = 0; d < (int) engine.drumSteps.size(); ++d)
                    {
                        if (engine.drumSteps[d][step])
                            engine.drums.noteOn (9, drumNotes[d], 0.8f * engine.drumVol);
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

            // Apply reverb to the output
            if (engine.reverbEnabled)
            {
                auto* chanL = bufferToFill.buffer->getWritePointer (0, bufferToFill.startSample);
                auto* chanR = bufferToFill.buffer->getWritePointer (1, bufferToFill.startSample);
                for (int s = 0; s < numSamples; ++s)
                {
                    float inL = chanL[s];
                    float inR = chanR[s];
                    processReverb (inL, inR);
                    chanL[s] = inL;
                    chanR[s] = inR;
                }
            }

            // Capture for recording
            if (engine.isRecordingFlag)
            {
                int prevSize = engine.recordingBuffer->getNumSamples();
                int newSize = prevSize + numSamples;
                engine.recordingBuffer->setSize (2, newSize, false, false, true);
                for (int ch = 0; ch < 2; ++ch)
                    engine.recordingBuffer->copyFrom (ch, prevSize,
                        *bufferToFill.buffer, ch, bufferToFill.startSample, numSamples);
                engine.recordSampleCount = newSize;
            }
        }

        void processReverb (float& l, float& r)
        {
            engine.reverb.processStereo (l, r);
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

    float melodyVol = 0.8f, chordVol = 0.7f, drumVol = 0.75f;

    bool reverbEnabled = true;
    juce::Reverb reverb;

    bool isRecordingFlag = false;
    std::atomic<int> recordSampleCount { 0 };
    std::unique_ptr<juce::AudioBuffer<float>> recordingBuffer;
    juce::File recordingFile;

    juce::AudioDeviceManager deviceManager;
    AudioSourceImpl audioSource { *this };
    juce::AudioSourcePlayer player;

    std::unique_ptr<PlaybackTimer> timer;
};

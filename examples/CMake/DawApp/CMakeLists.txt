cmake_minimum_required(VERSION 3.22)
project(DAW_APP_EXAMPLE VERSION 0.0.1)

add_subdirectory(../../.. JUCE)

juce_add_gui_app(DawAppExample
    PRODUCT_NAME "Music Theory DAW")

target_sources(DawAppExample
    PRIVATE
        Main.cpp
        MainComponent.cpp
        MusicTheoryEngine.cpp
        MidiEngine.cpp
        PianoRollComponent.cpp
        DrumSequencerComponent.cpp
        ChordPaletteComponent.cpp)

target_link_libraries(DawAppExample
    PRIVATE
        juce::juce_gui_extra
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_audio_devices
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

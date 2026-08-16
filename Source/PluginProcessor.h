#pragma once

#include <JuceHeader.h>

#include "ChordDetector.h"
#include "HarmonyAdvisor.h"

#include <array>
#include <atomic>
#include <mutex>
#include <vector>

class ChordingAudioProcessor final : public juce::AudioProcessor,
                                     private juce::Timer
{
public:
    struct Settings
    {
        bool preferFlats = false;
        bool automaticKey = true;
        int keyRoot = 0;
        chording::ScaleMode keyMode = chording::ScaleMode::major;
        chording::SuggestionStyle style = chording::SuggestionStyle::basic;
        chording::Mood mood = chording::Mood::neutral;
        bool includeBorrowed = false;
    };

    ChordingAudioProcessor();
    ~ChordingAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    [[nodiscard]] chording::ChordResult getCurrentChord() const noexcept;
    [[nodiscard]] int getActiveNoteCount() const noexcept;
    [[nodiscard]] std::vector<chording::ChordResult> getProgression() const;
    [[nodiscard]] std::uint64_t getHistoryVersion() const noexcept;
    void clearProgression();
    void undoProgression();

    [[nodiscard]] Settings getSettings() const noexcept;
    void setSettings(const Settings& settings) noexcept;
    [[nodiscard]] std::uint64_t getSettingsVersion() const noexcept;

    void setCapturePaused(bool paused) noexcept;
    [[nodiscard]] bool isCapturePaused() const noexcept;

private:
    void timerCallback() override;
    void applyMidiMessage(const juce::MidiMessage& message) noexcept;
    void publishDetection() noexcept;
    void clearChannel(int zeroBasedChannel) noexcept;

    static std::uint64_t encodeChord(const chording::ChordResult&) noexcept;
    static chording::ChordResult decodeChord(std::uint64_t) noexcept;

    std::array<std::array<bool, 128>, 16> heldNotes_ {};
    std::array<std::array<bool, 128>, 16> sustainedNotes_ {};
    std::array<bool, 16> sustainPedal_ {};

    std::atomic<std::uint64_t> publishedChord_ { 0 };
    std::atomic<std::uint64_t> noteOnRevision_ { 0 };
    std::atomic<int> activeNoteCount_ { 0 };

    mutable std::mutex historyMutex_;
    std::vector<chording::ChordResult> progression_;
    std::atomic<std::uint64_t> historyVersion_ { 0 };

    chording::ChordResult candidate_;
    double candidateStartedMs_ = 0.0;
    std::uint64_t candidateRevision_ = 0;
    std::uint64_t committedRevision_ = 0;
    static constexpr double captureDelayMs = 140.0;

    std::atomic<bool> preferFlats_ { false };
    std::atomic<bool> automaticKey_ { true };
    std::atomic<int> keyRoot_ { 0 };
    std::atomic<int> keyMode_ { static_cast<int>(chording::ScaleMode::major) };
    std::atomic<int> style_ { static_cast<int>(chording::SuggestionStyle::basic) };
    std::atomic<int> mood_ { static_cast<int>(chording::Mood::neutral) };
    std::atomic<bool> includeBorrowed_ { false };
    std::atomic<bool> capturePaused_ { false };
    std::atomic<std::uint64_t> settingsVersion_ { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordingAudioProcessor)
};

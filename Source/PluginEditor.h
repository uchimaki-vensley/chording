#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

#include <array>
#include <vector>

class ChordingAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    static constexpr int defaultWidth = 1100;
    static constexpr int defaultHeight = 720;
    static constexpr int minimumWidth = 1000;
    static constexpr int minimumHeight = 660;
    static constexpr int maximumWidth = 1600;
    static constexpr int maximumHeight = 1040;

    explicit ChordingAudioProcessorEditor(ChordingAudioProcessor&);
    ~ChordingAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    class ChordingLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        ChordingLookAndFeel();
        void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                                  bool highlighted, bool down) override;
        void drawButtonText(juce::Graphics&, juce::TextButton&,
                            bool highlighted, bool down) override;
    };

    void timerCallback() override;
    void refreshModel();
    void applyControls();
    void populateKeyMenu();
    void copyProgression() const;

    static juce::String chordName(const chording::ChordResult&, bool flats);
    static juce::String keyName(const chording::KeySignature&, bool flats);
    static juce::String roleText(chording::HarmonicRole);
    static juce::String pitchList(std::uint16_t mask, bool flats);

    ChordingAudioProcessor& processor_;
    ChordingLookAndFeel lookAndFeel_;

    juce::ComboBox keyBox_;
    juce::ComboBox styleBox_;
    juce::ComboBox moodBox_;
    juce::ToggleButton borrowedToggle_;
    juce::TextButton notationButton_;
    juce::TextButton pauseButton_;
    juce::TextButton undoButton_;
    juce::TextButton clearButton_;
    juce::TextButton copyButton_;

    chording::ChordResult currentChord_;
    std::array<chording::ChordResult, 3> alternatives_ {};
    std::vector<chording::ChordResult> progression_;
    std::array<chording::ChordSuggestion, 6> suggestions_ {};
    std::array<chording::RecommendationLevel, 6> recommendationLevels_ {};
    chording::KeySignature effectiveKey_;
    ChordingAudioProcessor::Settings settings_;
    std::array<juce::Rectangle<int>, 6> suggestionRows_ {};
    juce::Rectangle<int> keyLabelArea_;
    juce::Rectangle<int> styleLabelArea_;
    juce::Rectangle<int> moodLabelArea_;
    int selectedSuggestion_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordingAudioProcessorEditor)
};

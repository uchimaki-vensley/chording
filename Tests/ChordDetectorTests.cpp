#include "ChordInputState.h"
#include "ChordDetector.h"
#include "HarmonyAdvisor.h"
#include "MidiInsertState.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace
{
std::uint16_t mask(std::initializer_list<int> notes)
{
    std::uint16_t result = 0;
    for (const auto note : notes)
        result |= static_cast<std::uint16_t>(1u << (note % 12));
    return result;
}

void expectName(const std::string& expected,
                const std::initializer_list<int> notes,
                const int bass,
                const bool flats = false)
{
    const auto chord = chording::ChordDetector::detect(mask(notes), bass);
    const auto actual = chording::ChordDetector::format(chord, flats);
    if (actual != expected)
    {
        std::cerr << "Expected " << expected << ", got " << actual << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void expectStateFromSnapshot(const chording::ChordInputSnapshot& snapshot,
                             const std::string& expectedName,
                             const int expectedNoteCount)
{
    const auto chord = chording::ChordDetector::detect(snapshot.pitchClassMask, snapshot.bass);
    const auto actualName = chording::ChordDetector::format(chord, false);
    if (actualName != expectedName || snapshot.noteCount != expectedNoteCount)
    {
        std::cerr << "Expected state " << expectedName << " with " << expectedNoteCount
                  << " notes, got " << actualName << " with " << snapshot.noteCount << " notes\n";
        std::exit(EXIT_FAILURE);
    }
}

void expectState(const chording::ChordInputState& state,
                 const std::string& expectedName,
                 const int expectedNoteCount)
{
    expectStateFromSnapshot(state.snapshot(), expectedName, expectedNoteCount);
}
}

int main()
{
    expectName("C", { 0, 4, 7 }, 0);
    expectName("Cm", { 0, 3, 7 }, 0);
    expectName("Cmaj7", { 0, 4, 7, 11 }, 0);
    expectName("C7", { 0, 4, 7, 10 }, 0);
    expectName("Cm7", { 0, 3, 7, 10 }, 0);
    expectName("Cm7b5", { 0, 3, 6, 10 }, 0);
    expectName("C/E", { 0, 4, 7 }, 4);
    expectName("Bb", { 10, 2, 5 }, 10, true);
    expectName("D7/F#", { 2, 6, 9, 0 }, 6);
    expectName("G13", { 7, 9, 11, 0, 2, 4, 5 }, 7);
    expectName("Caug", { 0, 4, 8 }, 0);
    expectName("C7b5", { 0, 4, 6, 10 }, 0);
    expectName("C7#5", { 0, 4, 8, 10 }, 0);
    expectName("C7b9", { 0, 1, 4, 7, 10 }, 0);
    expectName("C7#9", { 0, 3, 4, 7, 10 }, 0);

    chording::ChordInputState inputState;
    inputState.noteOn(0, 60);
    inputState.noteOn(0, 64);
    inputState.noteOn(0, 67);
    expectState(inputState, "C", 3);

    inputState.sustainPedalChanged(0, true);
    expectState(inputState, "C", 3);
    inputState.noteOff(0, 60);
    inputState.noteOff(0, 64);
    inputState.noteOff(0, 67);
    expectState(inputState, "--", 0);

    inputState.noteOn(0, 62);
    inputState.noteOn(0, 65);
    inputState.noteOn(0, 69);
    expectState(inputState, "Dm", 3);
    inputState.sustainPedalChanged(0, false);
    expectState(inputState, "Dm", 3);
    inputState.clearChannel(0);
    expectState(inputState, "--", 0);

    // Both pedal states must give the same results, including held notes shared
    // by channels and partial release of a chord.
    for (const bool pedalDown : {false, true})
    {
        chording::ChordInputState state;
        state.sustainPedalChanged(0, pedalDown);
        state.noteOn(0, 60);
        state.noteOn(0, 64);
        state.noteOn(0, 67);
        state.noteOn(1, 60);
        expectState(state, "C", 3);
        state.noteOff(0, 60);
        expectState(state, "C", 3);
        state.noteOff(1, 60);
        if (state.snapshot().pitchClassMask != mask({64, 67}) || state.snapshot().bass != 4)
            return EXIT_FAILURE;
        state.noteOff(0, 64);
        state.noteOff(0, 67);
        expectState(state, "--", 0);
        state.noteOn(0, 62);
        state.noteOn(0, 65);
        state.noteOn(0, 69);
        expectState(state, "Dm", 3);
        state.clearAll();
        expectState(state, "--", 0);
    }

    // Cubase MIDI Inserts expose live notes as immediate note on/off events.
    chording::MidiInsertState midiInsert;
    midiInsert.receive(0x90, 0, 60, 100, 0, 0, true);
    midiInsert.receive(0x90, 0, 64, 100, 0, -1, true);
    midiInsert.receive(0x90, 0, 67, 100, 0, -1, true);
    expectStateFromSnapshot(midiInsert.snapshot(), "C", 3);
    if (midiInsert.takeNoteOns() != 3 || midiInsert.takeNoteOns() != 0)
        return EXIT_FAILURE;
    midiInsert.receive(0xb0, 0, 64, 127, 0, 0, true);
    expectStateFromSnapshot(midiInsert.snapshot(), "C", 3);
    midiInsert.receive(0x80, 0, 60, 0, 0, -1, true);
    midiInsert.receive(0x90, 0, 64, 0, 0, -1, true);
    midiInsert.receive(0x80, 0, 67, 0, 0, -1, true);
    expectStateFromSnapshot(midiInsert.snapshot(), "--", 0);

    // Cubase can use -1 for "Any" channel and can expose a live event without
    // the immediate flag while its note length is still pending.
    midiInsert.receive(0x90, -1, 60, 100, 0, -1, false);
    midiInsert.receive(0x90, -1, 64, 100, 0, -1, false);
    midiInsert.receive(0x90, -1, 67, 100, 0, -1, false);
    expectStateFromSnapshot(midiInsert.snapshot(), "C", 3);
    midiInsert.receive(0x80, -1, 60, 0, 0, 0, false);
    midiInsert.receive(0x80, -1, 64, 0, 0, 0, false);
    midiInsert.receive(0x80, -1, 67, 0, 0, 0, false);
    expectStateFromSnapshot(midiInsert.snapshot(), "--", 0);

    // Recorded notes arrive with PPQ start and duration. The detector follows
    // the scheduled starts/ends while the original event continues to the synth.
    midiInsert.receive(0x90, 0, 62, 100, 100, 100, false);
    midiInsert.receive(0x90, 0, 65, 100, 100, 100, false);
    midiInsert.receive(0x90, 0, 69, 100, 100, 100, false);
    midiInsert.advance(100, false);
    expectStateFromSnapshot(midiInsert.snapshot(), "--", 0);
    midiInsert.advance(101, false);
    expectStateFromSnapshot(midiInsert.snapshot(), "Dm", 3);
    midiInsert.advance(201, false);
    expectStateFromSnapshot(midiInsert.snapshot(), "--", 0);
    midiInsert.receive(0x90, 0, 60, 100, 300, 100, false);
    midiInsert.receive(0xb0, 0, 123, 0, 350, 0, false);
    midiInsert.advance(301, false);
    if (midiInsert.snapshot().noteCount != 1)
        return EXIT_FAILURE;
    midiInsert.advance(351, false);
    expectStateFromSnapshot(midiInsert.snapshot(), "--", 0);
    midiInsert.clear();
    expectStateFromSnapshot(midiInsert.snapshot(), "--", 0);

    const auto singleNote = chording::ChordDetector::detect(mask({ 0 }), 0);
    if (singleNote.isValid())
        return EXIT_FAILURE;

    const std::array history {
        chording::ChordDetector::detect(mask({ 0, 4, 7 }), 0),
        chording::ChordDetector::detect(mask({ 5, 9, 0 }), 5),
        chording::ChordDetector::detect(mask({ 7, 11, 2, 5 }), 7),
        chording::ChordDetector::detect(mask({ 0, 4, 7 }), 0)
    };
    const auto key = chording::HarmonyAdvisor::estimateKey(history);
    if (key.root != 0 || key.mode != chording::ScaleMode::major)
    {
        std::cerr << "Expected C major key\n";
        return EXIT_FAILURE;
    }

    const auto suggestions = chording::HarmonyAdvisor::suggest(
        history.back(), { 0, chording::ScaleMode::major, 1.0f },
        chording::SuggestionStyle::basic, chording::Mood::neutral, false, history);
    const auto hasF = std::ranges::any_of(suggestions, [](const auto& item) { return item.chord.root == 5; });
    const auto hasG = std::ranges::any_of(suggestions, [](const auto& item) { return item.chord.root == 7; });
    if (! hasF || ! hasG)
    {
        std::cerr << "Expected F and G among basic suggestions\n";
        return EXIT_FAILURE;
    }

    const auto rockSuggestions = chording::HarmonyAdvisor::suggest(
        history.back(), { 0, chording::ScaleMode::major, 1.0f },
        chording::SuggestionStyle::rock, chording::Mood::surprise, true, history);
    const auto hasBorrowed = std::ranges::any_of(rockSuggestions, [](const auto& item)
    {
        return item.role == chording::HarmonicRole::borrowed;
    });
    if (! hasBorrowed)
    {
        std::cerr << "Expected a borrowed chord in rock/surprise mode\n";
        return EXIT_FAILURE;
    }

    for (std::uint16_t pitchMask = 0; pitchMask < 0x1000u; ++pitchMask)
    {
        for (int bass = 0; bass < 12; ++bass)
        {
            if ((pitchMask & (1u << bass)) == 0)
                continue;
            const auto candidates = chording::ChordDetector::detectAlternatives(pitchMask, bass);
            if (std::popcount(pitchMask) < 2)
            {
                if (candidates[0].isValid())
                    return EXIT_FAILURE;
                continue;
            }
            if (! candidates[0].isValid() || candidates[0].confidence < 0.0f
                || candidates[0].confidence > 1.0f
                || (pitchMask & (1u << candidates[0].root)) == 0)
            {
                std::cerr << "Invalid result for pitch mask " << pitchMask << '\n';
                return EXIT_FAILURE;
            }
            for (std::size_t left = 0; left < candidates.size(); ++left)
                for (std::size_t right = left + 1; right < candidates.size(); ++right)
                    if (candidates[left].isValid() && candidates[right].isValid()
                        && candidates[left].root == candidates[right].root
                        && candidates[left].quality == candidates[right].quality)
                        return EXIT_FAILURE;
        }
    }

    for (int root = 0; root < 12; ++root)
    {
        for (int mode = 0; mode < 2; ++mode)
        {
            for (int style = 0; style < 4; ++style)
            {
                for (int mood = 0; mood < 5; ++mood)
                {
                    const auto allSuggestions = chording::HarmonyAdvisor::suggest(
                        history.back(),
                        { root, static_cast<chording::ScaleMode>(mode), 1.0f },
                        static_cast<chording::SuggestionStyle>(style),
                        static_cast<chording::Mood>(mood), true, history);
                    for (std::size_t index = 0; index < allSuggestions.size(); ++index)
                    {
                        if (! allSuggestions[index].chord.isValid()
                            || ! std::isfinite(allSuggestions[index].score)
                            || (index > 0 && allSuggestions[index - 1].score
                                                < allSuggestions[index].score))
                            return EXIT_FAILURE;
                    }

                    const auto levels = chording::HarmonyAdvisor::recommendationLevels(allSuggestions);
                    if (levels[0] != chording::RecommendationLevel::high)
                    {
                        std::cerr << "Expected the top suggestion to be highly recommended\n";
                        return EXIT_FAILURE;
                    }
                    for (std::size_t index = 1; index < levels.size(); ++index)
                    {
                        if (levels[index] == chording::RecommendationLevel::none
                            || levels[index - 1] < levels[index]
                            || (allSuggestions[index - 1].score == allSuggestions[index].score
                                && levels[index - 1] != levels[index]))
                        {
                            std::cerr << "Expected recommendation levels to follow the ranking\n";
                            return EXIT_FAILURE;
                        }
                    }
                }
            }
        }
    }

    using chording::RecommendationLevel;
    const auto levelsFor = [](const std::array<float, 6>& scores)
    {
        std::array<chording::ChordSuggestion, 6> list {};
        for (std::size_t index = 0; index < scores.size(); ++index)
            list[index] = { { static_cast<int>(index), static_cast<int>(index),
                              chording::ChordQuality::major, 0, 1.0f },
                            chording::HarmonicRole::tonic, scores[index] };
        return chording::HarmonyAdvisor::recommendationLevels(list);
    };
    const auto expectLevels = [&](const std::array<float, 6>& scores,
                                  const std::array<RecommendationLevel, 6>& expected,
                                  const char* label)
    {
        if (levelsFor(scores) == expected)
            return true;
        std::cerr << "Unexpected recommendation levels: " << label << '\n';
        return false;
    };

    constexpr auto high = RecommendationLevel::high;
    constexpr auto medium = RecommendationLevel::medium;
    constexpr auto low = RecommendationLevel::low;
    if (! expectLevels({ 12.0f, 11.5f, 8.0f, 7.0f, 4.0f, 3.0f },
                       { high, high, medium, medium, low, low }, "three tiers")
        || ! expectLevels({ 112.0f, 111.5f, 108.0f, 107.0f, 104.0f, 103.0f },
                          { high, high, medium, medium, low, low }, "shifted scores")
        || ! expectLevels({ 9.0f, 9.0f, 9.0f, 9.0f, 9.0f, 9.0f },
                          { high, high, high, high, high, high }, "all tied")
        || ! expectLevels({ 10.0f, 9.9f, 9.8f, 9.7f, 9.6f, 9.5f },
                          { high, high, high, high, medium, medium }, "narrow spread")
        || ! expectLevels({ 20.0f, 6.0f, 5.5f, 5.0f, 4.5f, 4.0f },
                          { high, low, low, low, low, low }, "single standout"))
        return EXIT_FAILURE;

    auto partial = std::array<chording::ChordSuggestion, 6> {};
    partial[0] = { { 0, 0, chording::ChordQuality::major, 0, 1.0f },
                   chording::HarmonicRole::tonic, 10.0f };
    partial[1] = { { 7, 7, chording::ChordQuality::major, 0, 1.0f },
                   chording::HarmonicRole::dominant, 4.0f };
    partial[2].score = 20.0f;
    partial[3] = { { 5, 5, chording::ChordQuality::major, 0, 1.0f },
                   chording::HarmonicRole::predominant,
                   -std::numeric_limits<float>::infinity() };
    const auto partialLevels = chording::HarmonyAdvisor::recommendationLevels(partial);
    if (partialLevels != std::array { high, low, RecommendationLevel::none, RecommendationLevel::none,
                                      RecommendationLevel::none, RecommendationLevel::none })
    {
        std::cerr << "Expected unusable suggestions to have no recommendation level\n";
        return EXIT_FAILURE;
    }

    std::cout << "ChordDetector tests passed\n";
    return EXIT_SUCCESS;
}

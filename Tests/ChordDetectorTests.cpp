#include "ChordDetector.h"
#include "HarmonyAdvisor.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <iostream>
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
                }
            }
        }
    }

    std::cout << "ChordDetector tests passed\n";
    return EXIT_SUCCESS;
}

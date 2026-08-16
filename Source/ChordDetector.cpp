#include "ChordDetector.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <string_view>

namespace chording
{
namespace
{
struct ChordTemplate
{
    ChordQuality quality;
    std::uint16_t intervals;
    std::string_view suffix;
};

constexpr std::uint16_t bits(std::initializer_list<int> intervals)
{
    std::uint16_t value = 0;
    for (const auto interval : intervals)
        value |= static_cast<std::uint16_t>(1u << interval);
    return value;
}

// Larger voicings come first so an exact extended chord wins a score tie.
constexpr std::array templates {
    ChordTemplate { ChordQuality::dominant7Sharp11, bits({ 0, 2, 4, 6, 7, 10 }),    "7#11" },
    ChordTemplate { ChordQuality::dominant13,       bits({ 0, 2, 4, 5, 7, 9, 10 }), "13" },
    ChordTemplate { ChordQuality::dominant11,       bits({ 0, 2, 4, 5, 7, 10 }),    "11" },
    ChordTemplate { ChordQuality::dominant9,        bits({ 0, 2, 4, 7, 10 }),       "9" },
    ChordTemplate { ChordQuality::major9,           bits({ 0, 2, 4, 7, 11 }),       "maj9" },
    ChordTemplate { ChordQuality::minor9,           bits({ 0, 2, 3, 7, 10 }),       "m9" },
    ChordTemplate { ChordQuality::dominant7Flat9,   bits({ 0, 1, 4, 7, 10 }),       "7b9" },
    ChordTemplate { ChordQuality::dominant7Sharp9,  bits({ 0, 3, 4, 7, 10 }),       "7#9" },
    ChordTemplate { ChordQuality::dominant7Flat13,  bits({ 0, 4, 7, 8, 10 }),       "7b13" },
    ChordTemplate { ChordQuality::minorMajor7,      bits({ 0, 3, 7, 11 }),          "m(maj7)" },
    ChordTemplate { ChordQuality::halfDiminished7,  bits({ 0, 3, 6, 10 }),          "m7b5" },
    ChordTemplate { ChordQuality::diminished7,      bits({ 0, 3, 6, 9 }),           "dim7" },
    ChordTemplate { ChordQuality::major7,           bits({ 0, 4, 7, 11 }),          "maj7" },
    ChordTemplate { ChordQuality::dominant7,        bits({ 0, 4, 7, 10 }),          "7" },
    ChordTemplate { ChordQuality::minor7,           bits({ 0, 3, 7, 10 }),          "m7" },
    ChordTemplate { ChordQuality::add9,             bits({ 0, 2, 4, 7 }),           "add9" },
    ChordTemplate { ChordQuality::major6,           bits({ 0, 4, 7, 9 }),           "6" },
    ChordTemplate { ChordQuality::minor6,           bits({ 0, 3, 7, 9 }),           "m6" },
    ChordTemplate { ChordQuality::dominant7Flat5,   bits({ 0, 4, 6, 10 }),          "7b5" },
    ChordTemplate { ChordQuality::dominant7Sharp5,  bits({ 0, 4, 8, 10 }),          "7#5" },
    ChordTemplate { ChordQuality::major,            bits({ 0, 4, 7 }),              "" },
    ChordTemplate { ChordQuality::minor,            bits({ 0, 3, 7 }),              "m" },
    ChordTemplate { ChordQuality::diminished,       bits({ 0, 3, 6 }),              "dim" },
    ChordTemplate { ChordQuality::augmented,        bits({ 0, 4, 8 }),              "aug" },
    ChordTemplate { ChordQuality::suspended2,       bits({ 0, 2, 7 }),              "sus2" },
    ChordTemplate { ChordQuality::suspended4,       bits({ 0, 5, 7 }),              "sus4" },
    ChordTemplate { ChordQuality::majorFlat5,       bits({ 0, 4, 6 }),              "b5" },
    ChordTemplate { ChordQuality::power,            bits({ 0, 7 }),                 "5" }
};

constexpr std::array<std::string_view, 12> sharpNames {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

constexpr std::array<std::string_view, 12> flatNames {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

std::uint16_t rotateToRoot(const std::uint16_t absoluteMask, const int root) noexcept
{
    std::uint16_t relative = 0;
    for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
        if ((absoluteMask & (1u << pitchClass)) != 0)
            relative |= static_cast<std::uint16_t>(1u << ((pitchClass - root + 12) % 12));
    return relative;
}

std::string_view suffixFor(const ChordQuality quality)
{
    for (const auto& chordTemplate : templates)
        if (chordTemplate.quality == quality)
            return chordTemplate.suffix;
    return {};
}
} // namespace

bool ChordResult::isValid() const noexcept
{
    return root >= 0 && root < 12 && quality != ChordQuality::none;
}

bool ChordResult::operator==(const ChordResult& other) const noexcept
{
    return root == other.root && bass == other.bass && quality == other.quality
        && pitchClassMask == other.pitchClassMask;
}

bool ChordResult::operator!=(const ChordResult& other) const noexcept
{
    return ! (*this == other);
}

ChordResult ChordDetector::detect(const std::uint16_t pitchClassMask,
                                  const int bassPitchClass) noexcept
{
    return detectAlternatives(pitchClassMask, bassPitchClass)[0];
}

std::array<ChordResult, 3> ChordDetector::detectAlternatives(
    const std::uint16_t pitchClassMask, const int bassPitchClass) noexcept
{
    const auto mask = static_cast<std::uint16_t>(pitchClassMask & 0x0fffu);
    const auto inputCount = std::popcount(mask);
    if (inputCount < 2)
        return { ChordResult { -1, bassPitchClass, ChordQuality::none, mask, 0.0f }, {}, {} };

    struct RankedChord
    {
        float score = -std::numeric_limits<float>::infinity();
        ChordResult result;
    };
    std::array<RankedChord, 3> best {};

    for (int root = 0; root < 12; ++root)
    {
        if ((mask & (1u << root)) == 0)
            continue;

        const auto relative = rotateToRoot(mask, root);
        for (const auto& chordTemplate : templates)
        {
            const auto matched = std::popcount(static_cast<std::uint16_t>(relative & chordTemplate.intervals));
            const auto expected = std::popcount(chordTemplate.intervals);
            const auto missing = expected - matched;
            const auto extra = inputCount - matched;

            float score = static_cast<float>(matched * 4 - missing * 5 - extra * 2);
            if ((chordTemplate.intervals & 1u) != 0 && (relative & 1u) != 0)
                score += 1.5f;
            if (root == bassPitchClass)
                score += 0.75f;

            const auto precision = static_cast<float>(matched) / static_cast<float>(inputCount);
            const auto coverage = static_cast<float>(matched) / static_cast<float>(expected);
            RankedChord candidate {
                score,
                { root, bassPitchClass, chordTemplate.quality, mask,
                  std::clamp(precision * 0.45f + coverage * 0.55f, 0.0f, 1.0f) }
            };

            for (auto& ranked : best)
            {
                if (candidate.score <= ranked.score)
                    continue;
                std::swap(candidate, ranked);
            }
        }
    }

    std::array results { best[0].result, best[1].result, best[2].result };
    for (std::size_t index = 1; index < results.size(); ++index)
    {
        if (best[index].score < best[0].score - 2.25f || results[index].confidence < 0.82f)
            results[index] = { -1, bassPitchClass, ChordQuality::none, mask, 0.0f };
    }
    return results;
}

std::string ChordDetector::format(const ChordResult& chord, const bool preferFlats)
{
    if (! chord.isValid())
        return "--";

    auto name = pitchClassName(chord.root, preferFlats);
    name += suffixFor(chord.quality);

    if (chord.bass >= 0 && chord.bass != chord.root)
        name += "/" + pitchClassName(chord.bass, preferFlats);
    return name;
}

std::string ChordDetector::pitchClassName(const int pitchClass, const bool preferFlats)
{
    if (pitchClass < 0 || pitchClass >= 12)
        return "-";
    return std::string((preferFlats ? flatNames : sharpNames)[static_cast<std::size_t>(pitchClass)]);
}
} // namespace chording

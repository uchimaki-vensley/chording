#include "HarmonyAdvisor.h"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>

namespace chording
{
namespace
{
constexpr std::array majorIntervals { 0, 2, 4, 5, 7, 9, 11 };
constexpr std::array minorIntervals { 0, 2, 3, 5, 7, 8, 10 };
constexpr std::array majorQualities {
    ChordQuality::major, ChordQuality::minor, ChordQuality::minor,
    ChordQuality::major, ChordQuality::major, ChordQuality::minor,
    ChordQuality::diminished
};
constexpr std::array minorQualities {
    ChordQuality::minor, ChordQuality::diminished, ChordQuality::major,
    ChordQuality::minor, ChordQuality::minor, ChordQuality::major,
    ChordQuality::major
};

bool isMinorQuality(const ChordQuality quality) noexcept
{
    return quality == ChordQuality::minor || quality == ChordQuality::minor6
        || quality == ChordQuality::minor7 || quality == ChordQuality::minor9
        || quality == ChordQuality::minorMajor7 || quality == ChordQuality::halfDiminished7;
}

bool isMajorQuality(const ChordQuality quality) noexcept
{
    return quality == ChordQuality::major || quality == ChordQuality::major6
        || quality == ChordQuality::major7 || quality == ChordQuality::major9
        || quality == ChordQuality::add9;
}

bool isDominantQuality(const ChordQuality quality) noexcept
{
    return quality == ChordQuality::dominant7 || quality == ChordQuality::dominant9
        || quality == ChordQuality::dominant11 || quality == ChordQuality::dominant13
        || quality == ChordQuality::dominant7Flat5 || quality == ChordQuality::dominant7Sharp5
        || quality == ChordQuality::dominant7Flat9 || quality == ChordQuality::dominant7Sharp9
        || quality == ChordQuality::dominant7Sharp11 || quality == ChordQuality::dominant7Flat13;
}

int degreeFor(const int pitchClass, const KeySignature key) noexcept
{
    const auto relative = (pitchClass - key.root + 12) % 12;
    const auto& intervals = key.mode == ScaleMode::major ? majorIntervals : minorIntervals;
    for (int degree = 0; degree < 7; ++degree)
        if (intervals[static_cast<std::size_t>(degree)] == relative)
            return degree;
    return -1;
}

HarmonicRole roleForDegree(const int degree) noexcept
{
    if (degree == 0)
        return HarmonicRole::tonic;
    if (degree == 1 || degree == 3)
        return HarmonicRole::predominant;
    if (degree == 4 || degree == 6)
        return HarmonicRole::dominant;
    return HarmonicRole::relative;
}

float transitionScore(const int from, const int to) noexcept
{
    if (from < 0)
        return 0.0f;
    const auto fromRole = roleForDegree(from);
    const auto toRole = roleForDegree(to);
    if (fromRole == HarmonicRole::tonic && toRole == HarmonicRole::predominant)
        return 4.0f;
    if (fromRole == HarmonicRole::tonic && toRole == HarmonicRole::dominant)
        return 3.5f;
    if (fromRole == HarmonicRole::predominant && toRole == HarmonicRole::dominant)
        return 5.0f;
    if (fromRole == HarmonicRole::dominant && toRole == HarmonicRole::tonic)
        return 6.0f;
    if (from == 4 && to == 5)
        return 4.0f;
    return 1.0f;
}

std::uint16_t pitchMaskFor(const int root, const ChordQuality quality) noexcept
{
    auto intervals = std::array { 0, 4, 7, -1, -1 };
    switch (quality)
    {
        case ChordQuality::minor: intervals = { 0, 3, 7, -1, -1 }; break;
        case ChordQuality::diminished: intervals = { 0, 3, 6, -1, -1 }; break;
        case ChordQuality::dominant7: intervals = { 0, 4, 7, 10, -1 }; break;
        case ChordQuality::major7: intervals = { 0, 4, 7, 11, -1 }; break;
        case ChordQuality::minor7: intervals = { 0, 3, 7, 10, -1 }; break;
        case ChordQuality::halfDiminished7: intervals = { 0, 3, 6, 10, -1 }; break;
        case ChordQuality::major9: intervals = { 0, 2, 4, 7, 11 }; break;
        case ChordQuality::minor9: intervals = { 0, 2, 3, 7, 10 }; break;
        default: break;
    }

    std::uint16_t mask = 0;
    for (const auto interval : intervals)
        if (interval >= 0)
            mask |= static_cast<std::uint16_t>(1u << ((root + interval) % 12));
    return mask;
}

ChordQuality styledQuality(const ChordQuality triad, const int degree,
                           const SuggestionStyle style) noexcept
{
    if (style != SuggestionStyle::jazz)
        return degree == 4 ? ChordQuality::dominant7 : triad;
    if (degree == 4)
        return ChordQuality::dominant7;
    if (triad == ChordQuality::major)
        return ChordQuality::major7;
    if (triad == ChordQuality::minor)
        return ChordQuality::minor7;
    return ChordQuality::halfDiminished7;
}

void insertSuggestion(std::array<ChordSuggestion, 6>& suggestions,
                      ChordSuggestion candidate) noexcept
{
    for (const auto& suggestion : suggestions)
        if (suggestion.chord.root == candidate.chord.root
            && suggestion.chord.quality == candidate.chord.quality)
            return;

    for (auto& suggestion : suggestions)
    {
        if (candidate.score <= suggestion.score)
            continue;
        std::swap(candidate, suggestion);
    }
}
} // namespace

KeySignature HarmonyAdvisor::estimateKey(const std::span<const ChordResult> history) noexcept
{
    if (history.empty())
        return {};

    float bestScore = -std::numeric_limits<float>::infinity();
    float secondScore = bestScore;
    KeySignature best;

    for (int modeIndex = 0; modeIndex < 2; ++modeIndex)
    {
        const auto mode = modeIndex == 0 ? ScaleMode::major : ScaleMode::minor;
        for (int keyRoot = 0; keyRoot < 12; ++keyRoot)
        {
            const KeySignature key { keyRoot, mode, 0.0f };
            float score = 0.0f;
            for (std::size_t index = 0; index < history.size(); ++index)
            {
                const auto& chord = history[index];
                if (! chord.isValid())
                    continue;
                const auto recency = 1.0f + static_cast<float>(index) / static_cast<float>(history.size());
                const auto degree = degreeFor(chord.root, key);
                if (degree < 0)
                {
                    score -= 2.0f * recency;
                    continue;
                }

                const auto& qualities = mode == ScaleMode::major ? majorQualities : minorQualities;
                score += 2.0f * recency;
                if ((qualities[static_cast<std::size_t>(degree)] == ChordQuality::major && isMajorQuality(chord.quality))
                    || (qualities[static_cast<std::size_t>(degree)] == ChordQuality::minor && isMinorQuality(chord.quality))
                    || (degree == 4 && isDominantQuality(chord.quality))
                    || qualities[static_cast<std::size_t>(degree)] == chord.quality)
                    score += 2.5f * recency;
                if (degree == 0)
                    score += 1.0f * recency;
            }

            if (score > bestScore)
            {
                secondScore = bestScore;
                bestScore = score;
                best = key;
            }
            else if (score > secondScore)
            {
                secondScore = score;
            }
        }
    }

    best.confidence = std::clamp((bestScore - secondScore + 1.0f) / 6.0f, 0.0f, 1.0f);
    return best;
}

std::array<ChordSuggestion, 6> HarmonyAdvisor::suggest(
    const ChordResult& current, const KeySignature key,
    const SuggestionStyle style, const Mood mood, const bool includeBorrowed,
    const std::span<const ChordResult> history) noexcept
{
    std::array<ChordSuggestion, 6> suggestions {};
    for (auto& item : suggestions)
        item.score = -std::numeric_limits<float>::infinity();

    const auto& intervals = key.mode == ScaleMode::major ? majorIntervals : minorIntervals;
    const auto& qualities = key.mode == ScaleMode::major ? majorQualities : minorQualities;
    const auto currentDegree = current.isValid() ? degreeFor(current.root, key) : -1;

    for (int degree = 0; degree < 7; ++degree)
    {
        const auto root = (key.root + intervals[static_cast<std::size_t>(degree)]) % 12;
        const auto quality = styledQuality(qualities[static_cast<std::size_t>(degree)], degree, style);
        if (root == current.root && quality == current.quality)
            continue;

        auto score = 8.0f + transitionScore(currentDegree, degree);
        if (style == SuggestionStyle::pop)
        {
            constexpr std::array popOrder { 0, 4, 5, 3, 1, 2, 6 };
            const auto position = std::find(popOrder.begin(), popOrder.end(), degree) - popOrder.begin();
            score += static_cast<float>(7 - position) * 0.35f;
        }
        else if (style == SuggestionStyle::rock && (degree == 0 || degree == 3 || degree == 4))
            score += 2.0f;
        else if (style == SuggestionStyle::jazz && (degree == 1 || degree == 4))
            score += 3.0f;

        if (mood == Mood::bright && isMajorQuality(quality)) score += 2.5f;
        if (mood == Mood::melancholy && isMinorQuality(quality)) score += 2.5f;
        if (mood == Mood::tension && (degree == 4 || degree == 6)) score += 3.0f;
        if (mood == Mood::surprise && degree == 5) score += 2.0f;
        if (degree == 6) score -= 2.5f;

        const auto chordMask = pitchMaskFor(root, quality);
        const auto commonNotes = std::popcount(static_cast<std::uint16_t>(current.pitchClassMask & chordMask));
        score += static_cast<float>(commonNotes) * 0.35f;

        if (! history.empty() && history.back().root == root)
            score -= 3.0f;

        insertSuggestion(suggestions, {
            { root, root, quality, chordMask, 1.0f }, roleForDegree(degree), score
        });
    }

    if (includeBorrowed)
    {
        const auto borrowedIntervals = key.mode == ScaleMode::major
            ? std::array { 3, 8, 10 }
            : std::array { 1, 5, 0 };
        for (const auto interval : borrowedIntervals)
        {
            const auto root = (key.root + interval) % 12;
            auto score = 6.0f;
            if (style == SuggestionStyle::rock) score += 4.0f;
            if (style == SuggestionStyle::pop) score += 1.5f;
            if (mood == Mood::surprise) score += 4.0f;
            if (mood == Mood::bright) score -= 1.0f;
            const auto chordMask = pitchMaskFor(root, ChordQuality::major);
            insertSuggestion(suggestions, {
                { root, root, ChordQuality::major, chordMask, 1.0f },
                HarmonicRole::borrowed, score
            });
        }
    }

    return suggestions;
}
} // namespace chording

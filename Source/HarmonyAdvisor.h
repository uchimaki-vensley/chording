#pragma once

#include "ChordDetector.h"

#include <array>
#include <span>

namespace chording
{
enum class ScaleMode : std::uint8_t { major, minor };
enum class SuggestionStyle : std::uint8_t { basic, pop, rock, jazz };
enum class Mood : std::uint8_t { neutral, bright, melancholy, tension, surprise };
enum class HarmonicRole : std::uint8_t
{
    tonic,
    predominant,
    dominant,
    relative,
    borrowed,
    colour
};

struct KeySignature
{
    int root = 0;
    ScaleMode mode = ScaleMode::major;
    float confidence = 0.0f;
};

struct ChordSuggestion
{
    ChordResult chord;
    HarmonicRole role = HarmonicRole::colour;
    float score = 0.0f;
};

class HarmonyAdvisor
{
public:
    [[nodiscard]] static KeySignature estimateKey(std::span<const ChordResult> history) noexcept;
    [[nodiscard]] static std::array<ChordSuggestion, 6> suggest(
        const ChordResult& current,
        KeySignature key,
        SuggestionStyle style,
        Mood mood,
        bool includeBorrowed,
        std::span<const ChordResult> history) noexcept;
};
} // namespace chording

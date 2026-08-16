#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>

namespace chording
{
enum class ChordQuality : std::uint8_t
{
    none,
    power,
    major,
    minor,
    diminished,
    augmented,
    suspended2,
    suspended4,
    major6,
    minor6,
    dominant7,
    major7,
    minor7,
    minorMajor7,
    diminished7,
    halfDiminished7,
    add9,
    dominant9,
    major9,
    minor9,
    dominant11,
    dominant13,
    majorFlat5,
    dominant7Flat5,
    dominant7Sharp5,
    dominant7Flat9,
    dominant7Sharp9,
    dominant7Sharp11,
    dominant7Flat13
};

struct ChordResult
{
    int root = -1;
    int bass = -1;
    ChordQuality quality = ChordQuality::none;
    std::uint16_t pitchClassMask = 0;
    float confidence = 0.0f;

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool operator==(const ChordResult& other) const noexcept;
    [[nodiscard]] bool operator!=(const ChordResult& other) const noexcept;
};

class ChordDetector
{
public:
    [[nodiscard]] static ChordResult detect(std::uint16_t pitchClassMask,
                                            int bassPitchClass) noexcept;
    [[nodiscard]] static std::array<ChordResult, 3> detectAlternatives(
        std::uint16_t pitchClassMask, int bassPitchClass) noexcept;
    [[nodiscard]] static std::string format(const ChordResult& chord,
                                            bool preferFlats);
    [[nodiscard]] static std::string pitchClassName(int pitchClass,
                                                    bool preferFlats);
};
} // namespace chording

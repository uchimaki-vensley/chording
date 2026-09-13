#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace chording
{
struct ChordInputSnapshot
{
    std::uint16_t pitchClassMask = 0;
    int bass = -1;
    int noteCount = 0;
};

class ChordInputState
{
public:
    void noteOn(int zeroBasedChannel, int noteNumber) noexcept;
    void noteOff(int zeroBasedChannel, int noteNumber) noexcept;
    void sustainPedalChanged(int zeroBasedChannel, bool isDown) noexcept;
    void clearChannel(int zeroBasedChannel) noexcept;
    void clearAll() noexcept;

    [[nodiscard]] ChordInputSnapshot snapshot() const noexcept;

private:
    std::array<std::array<bool, 128>, 16> heldNotes_ {};
};
}

#include "ChordInputState.h"

namespace chording
{
namespace
{
bool isValidChannel(const int channel) noexcept
{
    return channel >= 0 && channel < 16;
}

bool isValidNote(const int note) noexcept
{
    return note >= 0 && note < 128;
}
}

void ChordInputState::noteOn(const int channel, const int note) noexcept
{
    if (isValidChannel(channel) && isValidNote(note))
        heldNotes_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)] = true;
}

void ChordInputState::noteOff(const int channel, const int note) noexcept
{
    if (isValidChannel(channel) && isValidNote(note))
        heldNotes_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)] = false;
}

void ChordInputState::sustainPedalChanged(int, bool) noexcept
{
    // Chord recognition follows physical keys, not notes sustained by the instrument.
}

void ChordInputState::clearChannel(const int channel) noexcept
{
    if (isValidChannel(channel))
        heldNotes_[static_cast<std::size_t>(channel)].fill(false);
}

void ChordInputState::clearAll() noexcept
{
    for (auto& channel : heldNotes_)
        channel.fill(false);
}

ChordInputSnapshot ChordInputState::snapshot() const noexcept
{
    ChordInputSnapshot result;
    for (int note = 0; note < 128; ++note)
    {
        auto held = false;
        for (const auto& channel : heldNotes_)
            held = held || channel[static_cast<std::size_t>(note)];

        if (! held)
            continue;

        if (result.bass < 0)
            result.bass = note % 12;
        result.pitchClassMask |= static_cast<std::uint16_t>(1u << (note % 12));
        ++result.noteCount;
    }
    return result;
}
}

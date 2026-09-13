#pragma once

#include "ChordInputState.h"
#include <array>
#include <cstdint>

namespace chording
{
// VST-MA playback notes carry a duration, unlike live MIDI's separate note-offs.
// Keep the two PPQ clocks independent and never retain borrowed host objects.
class MidiInsertState
{
public:
    void receive(int status, int channel, int data1, int data2,
                 std::int64_t start, std::int64_t length, bool immediate) noexcept;
    void advance(std::int64_t to, bool immediate) noexcept;
    void clear() noexcept;
    [[nodiscard]] ChordInputSnapshot snapshot() const noexcept;
    std::uint64_t takeNoteOns() noexcept;

private:
    struct Event
    {
        std::int64_t position;
        std::uint64_t order;
        int channel;
        int note;
        int action; // 1 = timed on, -1 = timed off, 0 = all notes off
    };
    struct Timeline
    {
        std::array<Event, 4096> queue{};
        std::size_t size = 0;
        std::array<std::array<unsigned, 128>, 16> held{};
    };
    bool enqueue(Timeline&, Event) noexcept;
    void apply(Timeline&, const Event&) noexcept;
    static bool later(const Event&, const Event&) noexcept;

    std::array<Timeline, 2> timelines_{};
    std::array<std::array<bool, 128>, 16> live_{};
    std::uint64_t order_ = 0;
    std::uint64_t noteOns_ = 0;
};
}

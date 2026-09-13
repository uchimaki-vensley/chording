#include "MidiInsertState.h"
#include <algorithm>
#include <limits>
#include <utility>

namespace chording
{
bool MidiInsertState::later(const Event& a, const Event& b) noexcept
{
    return a.position != b.position ? a.position > b.position : a.order > b.order;
}

bool MidiInsertState::enqueue(Timeline& timeline, Event event) noexcept
{
    if (timeline.size == timeline.queue.size())
    {
        // Fail closed for detection only. The caller still forwards the MIDI.
        clear();
        return false;
    }
    event.order = order_++;
    timeline.queue[timeline.size++] = event;
    std::push_heap(timeline.queue.begin(), timeline.queue.begin() + timeline.size, later);
    return true;
}

void MidiInsertState::apply(Timeline& timeline, const Event& event) noexcept
{
    const auto channel = static_cast<std::size_t>(event.channel);
    if (event.action == 0)
    {
        timeline.held[channel].fill(0);
        live_[channel].fill(false);
        return;
    }
    auto& count = timeline.held[channel][static_cast<std::size_t>(event.note)];
    if (event.action > 0) { ++count; ++noteOns_; }
    else if (count > 0) --count;
}

void MidiInsertState::receive(int status, int channel, int data1, int data2,
                             std::int64_t start, std::int64_t length, bool immediate) noexcept
{
    if (data1 < 0 || data1 >= 128) return;
    // Cubase may report -1 for an event on a track whose channel is "Any".
    // Chord recognition merges channels, so a stable fallback channel preserves
    // note-on/off pairing without changing the event forwarded to the instrument.
    channel = std::clamp(channel, 0, 15);
    status &= 0xf0;
    auto& timeline = timelines_[immediate ? 1u : 0u];
    if (status == 0xb0 && (data1 == 120 || data1 == 123))
    {
        const Event clearEvent{start, 0, channel, 0, 0};
        if (immediate) apply(timeline, clearEvent);
        else enqueue(timeline, clearEvent);
        return;
    }
    if (status != 0x90 && status != 0x80) return; // Includes sustain CC64.
    const bool on = status == 0x90 && data2 > 0;
    // Live note lengths may be pending (-1). Some host paths don't mark those
    // events as immediate, and standalone note-offs have no useful duration.
    if (immediate || length < 0 || !on)
    {
        live_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(data1)] = on;
        if (on) ++noteOns_;
        return;
    }
    Event event{start, 0, channel, data1, on ? 1 : -1};
    if (on && length == 0) return;
    if (immediate) apply(timeline, event);
    else if (!enqueue(timeline, event)) return;
    if (on && length > 0)
    {
        event.action = -1;
        event.position = start > std::numeric_limits<std::int64_t>::max() - length
            ? std::numeric_limits<std::int64_t>::max() : start + length;
        enqueue(timeline, event);
    }
}

void MidiInsertState::advance(std::int64_t to, bool immediate) noexcept
{
    auto& timeline = timelines_[immediate ? 1u : 0u];
    while (timeline.size > 0 && timeline.queue[0].position < to)
    {
        std::pop_heap(timeline.queue.begin(), timeline.queue.begin() + timeline.size, later);
        apply(timeline, timeline.queue[--timeline.size]);
    }
}

void MidiInsertState::clear() noexcept
{
    for (auto& timeline : timelines_)
    {
        timeline.size = 0;
        for (auto& channel : timeline.held) channel.fill(0);
    }
    for (auto& channel : live_) channel.fill(false);
    noteOns_ = 0;
}

ChordInputSnapshot MidiInsertState::snapshot() const noexcept
{
    ChordInputSnapshot result;
    for (std::size_t note = 0; note < 128; ++note)
    {
        bool held = false;
        for (std::size_t channel = 0; channel < 16; ++channel)
            held = held || live_[channel][note]
                || timelines_[0].held[channel][note] != 0 || timelines_[1].held[channel][note] != 0;
        if (!held) continue;
        if (result.bass < 0) result.bass = static_cast<int>(note % 12);
        result.pitchClassMask |= static_cast<std::uint16_t>(1u << (note % 12));
        ++result.noteCount;
    }
    return result;
}

std::uint64_t MidiInsertState::takeNoteOns() noexcept { return std::exchange(noteOns_, 0); }
}

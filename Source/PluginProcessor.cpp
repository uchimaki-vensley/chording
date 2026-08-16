#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <utility>

ChordingAudioProcessor::ChordingAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    progression_.reserve(64);
    publishedChord_.store(encodeChord({}), std::memory_order_relaxed);
    startTimerHz(30);
}

ChordingAudioProcessor::~ChordingAudioProcessor()
{
    stopTimer();
}

void ChordingAudioProcessor::prepareToPlay(double, int)
{
    for (int channel = 0; channel < 16; ++channel)
        clearChannel(channel);
    publishDetection();
}

void ChordingAudioProcessor::releaseResources()
{
    for (int channel = 0; channel < 16; ++channel)
        clearChannel(channel);
    publishDetection();
}

bool ChordingAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo())
        && input == output;
}

void ChordingAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    for (const auto metadata : midiMessages)
        applyMidiMessage(metadata.getMessage());

    publishDetection();
}

void ChordingAudioProcessor::applyMidiMessage(const juce::MidiMessage& message) noexcept
{
    const auto channel = std::clamp(message.getChannel() - 1, 0, 15);
    if (message.isNoteOn())
    {
        const auto note = message.getNoteNumber();
        heldNotes_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)] = true;
        sustainedNotes_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)] = false;
        noteOnRevision_.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    if (message.isNoteOff())
    {
        const auto note = message.getNoteNumber();
        heldNotes_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)] = false;
        sustainedNotes_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)]
            = sustainPedal_[static_cast<std::size_t>(channel)];
        return;
    }

    if (message.isController() && message.getControllerNumber() == 64)
    {
        const auto down = message.getControllerValue() >= 64;
        if (sustainPedal_[static_cast<std::size_t>(channel)] && ! down)
        {
            for (int note = 0; note < 128; ++note)
                if (! heldNotes_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)])
                    sustainedNotes_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)] = false;
        }
        sustainPedal_[static_cast<std::size_t>(channel)] = down;
        return;
    }

    if (message.isAllNotesOff() || message.isAllSoundOff())
        clearChannel(channel);
}

void ChordingAudioProcessor::clearChannel(const int channel) noexcept
{
    heldNotes_[static_cast<std::size_t>(channel)].fill(false);
    sustainedNotes_[static_cast<std::size_t>(channel)].fill(false);
    sustainPedal_[static_cast<std::size_t>(channel)] = false;
}

void ChordingAudioProcessor::publishDetection() noexcept
{
    std::uint16_t mask = 0;
    auto bass = -1;
    auto count = 0;
    for (int note = 0; note < 128; ++note)
    {
        auto active = false;
        for (int channel = 0; channel < 16; ++channel)
            active = active
                || heldNotes_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)]
                || sustainedNotes_[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)];
        if (! active)
            continue;
        if (bass < 0)
            bass = note % 12;
        mask |= static_cast<std::uint16_t>(1u << (note % 12));
        ++count;
    }

    const auto chord = chording::ChordDetector::detect(mask, bass);
    activeNoteCount_.store(count, std::memory_order_relaxed);
    publishedChord_.store(encodeChord(chord), std::memory_order_release);
}

void ChordingAudioProcessor::timerCallback()
{
    const auto current = getCurrentChord();
    const auto revision = noteOnRevision_.load(std::memory_order_relaxed);
    const auto now = juce::Time::getMillisecondCounterHiRes();

    if (current != candidate_)
    {
        candidate_ = current;
        candidateStartedMs_ = now;
        candidateRevision_ = revision;
        return;
    }

    if (! current.isValid() || capturePaused_.load(std::memory_order_relaxed)
        || candidateRevision_ == committedRevision_ || now - candidateStartedMs_ < captureDelayMs)
        return;

    {
        const std::scoped_lock lock(historyMutex_);
        if (progression_.size() == 64)
            progression_.erase(progression_.begin());
        progression_.push_back(current);
    }
    committedRevision_ = candidateRevision_;
    historyVersion_.fetch_add(1, std::memory_order_release);
}

chording::ChordResult ChordingAudioProcessor::getCurrentChord() const noexcept
{
    return decodeChord(publishedChord_.load(std::memory_order_acquire));
}

int ChordingAudioProcessor::getActiveNoteCount() const noexcept
{
    return activeNoteCount_.load(std::memory_order_relaxed);
}

std::vector<chording::ChordResult> ChordingAudioProcessor::getProgression() const
{
    const std::scoped_lock lock(historyMutex_);
    return progression_;
}

std::uint64_t ChordingAudioProcessor::getHistoryVersion() const noexcept
{
    return historyVersion_.load(std::memory_order_acquire);
}

void ChordingAudioProcessor::clearProgression()
{
    {
        const std::scoped_lock lock(historyMutex_);
        progression_.clear();
    }
    committedRevision_ = noteOnRevision_.load(std::memory_order_relaxed);
    historyVersion_.fetch_add(1, std::memory_order_release);
}

void ChordingAudioProcessor::undoProgression()
{
    {
        const std::scoped_lock lock(historyMutex_);
        if (progression_.empty())
            return;
        progression_.pop_back();
    }
    historyVersion_.fetch_add(1, std::memory_order_release);
}

ChordingAudioProcessor::Settings ChordingAudioProcessor::getSettings() const noexcept
{
    return {
        preferFlats_.load(std::memory_order_relaxed),
        automaticKey_.load(std::memory_order_relaxed),
        keyRoot_.load(std::memory_order_relaxed),
        static_cast<chording::ScaleMode>(keyMode_.load(std::memory_order_relaxed)),
        static_cast<chording::SuggestionStyle>(style_.load(std::memory_order_relaxed)),
        static_cast<chording::Mood>(mood_.load(std::memory_order_relaxed)),
        includeBorrowed_.load(std::memory_order_relaxed)
    };
}

void ChordingAudioProcessor::setSettings(const Settings& settings) noexcept
{
    preferFlats_.store(settings.preferFlats, std::memory_order_relaxed);
    automaticKey_.store(settings.automaticKey, std::memory_order_relaxed);
    keyRoot_.store(std::clamp(settings.keyRoot, 0, 11), std::memory_order_relaxed);
    keyMode_.store(static_cast<int>(settings.keyMode), std::memory_order_relaxed);
    style_.store(static_cast<int>(settings.style), std::memory_order_relaxed);
    mood_.store(static_cast<int>(settings.mood), std::memory_order_relaxed);
    includeBorrowed_.store(settings.includeBorrowed, std::memory_order_relaxed);
    settingsVersion_.fetch_add(1, std::memory_order_release);
}

std::uint64_t ChordingAudioProcessor::getSettingsVersion() const noexcept
{
    return settingsVersion_.load(std::memory_order_acquire);
}

void ChordingAudioProcessor::setCapturePaused(const bool paused) noexcept
{
    capturePaused_.store(paused, std::memory_order_relaxed);
    committedRevision_ = noteOnRevision_.load(std::memory_order_relaxed);
}

bool ChordingAudioProcessor::isCapturePaused() const noexcept
{
    return capturePaused_.load(std::memory_order_relaxed);
}

std::uint64_t ChordingAudioProcessor::encodeChord(const chording::ChordResult& chord) noexcept
{
    const auto root = chord.root < 0 ? 15u : static_cast<unsigned>(chord.root);
    const auto bass = chord.bass < 0 ? 15u : static_cast<unsigned>(chord.bass);
    const auto quality = static_cast<unsigned>(chord.quality);
    const auto confidence = static_cast<unsigned>(std::clamp(chord.confidence, 0.0f, 1.0f) * 255.0f);
    return static_cast<std::uint64_t>(chord.pitchClassMask & 0x0fffu)
        | (static_cast<std::uint64_t>(root) << 12u)
        | (static_cast<std::uint64_t>(bass) << 16u)
        | (static_cast<std::uint64_t>(quality & 0x3fu) << 20u)
        | (static_cast<std::uint64_t>(confidence & 0xffu) << 26u);
}

chording::ChordResult ChordingAudioProcessor::decodeChord(const std::uint64_t encoded) noexcept
{
    const auto root = static_cast<int>((encoded >> 12u) & 0x0fu);
    const auto bass = static_cast<int>((encoded >> 16u) & 0x0fu);
    return {
        root == 15 ? -1 : root,
        bass == 15 ? -1 : bass,
        static_cast<chording::ChordQuality>((encoded >> 20u) & 0x3fu),
        static_cast<std::uint16_t>(encoded & 0x0fffu),
        static_cast<float>((encoded >> 26u) & 0xffu) / 255.0f
    };
}

void ChordingAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    const auto settings = getSettings();
    juce::XmlElement state("CHORDING_STATE");
    state.setAttribute("version", 1);
    state.setAttribute("flats", settings.preferFlats);
    state.setAttribute("autoKey", settings.automaticKey);
    state.setAttribute("keyRoot", settings.keyRoot);
    state.setAttribute("keyMode", static_cast<int>(settings.keyMode));
    state.setAttribute("style", static_cast<int>(settings.style));
    state.setAttribute("mood", static_cast<int>(settings.mood));
    state.setAttribute("borrowed", settings.includeBorrowed);

    const auto history = getProgression();
    auto* progression = state.createNewChildElement("PROGRESSION");
    for (const auto& chord : history)
    {
        auto* item = progression->createNewChildElement("CHORD");
        item->setAttribute("root", chord.root);
        item->setAttribute("bass", chord.bass);
        item->setAttribute("quality", static_cast<int>(chord.quality));
        item->setAttribute("mask", static_cast<int>(chord.pitchClassMask));
        item->setAttribute("confidence", chord.confidence);
    }
    copyXmlToBinary(state, destination);
}

void ChordingAudioProcessor::setStateInformation(const void* data, const int size)
{
    const auto state = getXmlFromBinary(data, size);
    if (state == nullptr || ! state->hasTagName("CHORDING_STATE"))
        return;

    auto settings = getSettings();
    settings.preferFlats = state->getBoolAttribute("flats", false);
    settings.automaticKey = state->getBoolAttribute("autoKey", true);
    settings.keyRoot = state->getIntAttribute("keyRoot", 0);
    settings.keyMode = static_cast<chording::ScaleMode>(
        std::clamp(state->getIntAttribute("keyMode", 0), 0, 1));
    settings.style = static_cast<chording::SuggestionStyle>(
        std::clamp(state->getIntAttribute("style", 0), 0, 3));
    settings.mood = static_cast<chording::Mood>(
        std::clamp(state->getIntAttribute("mood", 0), 0, 4));
    settings.includeBorrowed = state->getBoolAttribute("borrowed", false);
    setSettings(settings);

    std::vector<chording::ChordResult> restored;
    if (const auto* progression = state->getChildByName("PROGRESSION"))
    {
        for (const auto* item : progression->getChildIterator())
        {
            if (! item->hasTagName("CHORD"))
                continue;
            restored.push_back({
                item->getIntAttribute("root", -1),
                item->getIntAttribute("bass", -1),
                static_cast<chording::ChordQuality>(item->getIntAttribute("quality", 0)),
                static_cast<std::uint16_t>(item->getIntAttribute("mask", 0)),
                static_cast<float>(item->getDoubleAttribute("confidence", 0.0))
            });
            if (restored.size() == 64)
                break;
        }
    }

    {
        const std::scoped_lock lock(historyMutex_);
        progression_ = std::move(restored);
    }
    historyVersion_.fetch_add(1, std::memory_order_release);
}

juce::AudioProcessorEditor* ChordingAudioProcessor::createEditor()
{
    return new ChordingAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordingAudioProcessor();
}

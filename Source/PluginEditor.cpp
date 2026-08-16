#include "PluginEditor.h"

#include <algorithm>

namespace
{
const auto background = juce::Colour::fromRGB(18, 20, 21);
const auto panel = juce::Colour::fromRGB(29, 32, 33);
const auto panelRaised = juce::Colour::fromRGB(38, 42, 43);
const auto textPrimary = juce::Colour::fromRGB(241, 239, 233);
const auto textMuted = juce::Colour::fromRGB(157, 163, 162);
const auto mint = juce::Colour::fromRGB(79, 211, 180);
const auto amber = juce::Colour::fromRGB(242, 180, 82);
const auto coral = juce::Colour::fromRGB(239, 116, 101);

template <std::size_t size>
juce::String utf8(const char8_t (&text)[size])
{
    return juce::String::fromUTF8(reinterpret_cast<const char*>(text),
                                  static_cast<int>(size - 1));
}

juce::Font font(const float size, const int style = juce::Font::plain)
{
    return juce::Font(juce::FontOptions("Yu Gothic UI", size, style));
}

void drawPanel(juce::Graphics& graphics, const juce::Rectangle<int> bounds)
{
    graphics.setColour(panel);
    graphics.fillRoundedRectangle(bounds.toFloat(), 6.0f);
    graphics.setColour(juce::Colour::fromRGB(52, 56, 57));
    graphics.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 6.0f, 1.0f);
}
} // namespace

ChordingAudioProcessorEditor::ChordingLookAndFeel::ChordingLookAndFeel()
{
    setDefaultSansSerifTypefaceName("Yu Gothic UI");
    setColour(juce::ComboBox::backgroundColourId, panelRaised);
    setColour(juce::ComboBox::textColourId, textPrimary);
    setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(61, 66, 66));
    setColour(juce::ComboBox::arrowColourId, mint);
    setColour(juce::PopupMenu::backgroundColourId, panelRaised);
    setColour(juce::PopupMenu::textColourId, textPrimary);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, mint.withAlpha(0.22f));
    setColour(juce::ToggleButton::textColourId, textPrimary);
    setColour(juce::ToggleButton::tickColourId, mint);
    setColour(juce::ToggleButton::tickDisabledColourId, textMuted);
}

void ChordingAudioProcessorEditor::ChordingLookAndFeel::drawButtonBackground(
    juce::Graphics& graphics, juce::Button& button, const juce::Colour&,
    const bool highlighted, const bool down)
{
    auto colour = button.getToggleState() ? mint.withAlpha(0.24f) : panelRaised;
    if (highlighted) colour = colour.brighter(0.08f);
    if (down) colour = colour.darker(0.12f);
    graphics.setColour(colour);
    graphics.fillRoundedRectangle(button.getLocalBounds().toFloat(), 4.0f);
    graphics.setColour(button.getToggleState() ? mint : juce::Colour::fromRGB(67, 72, 72));
    graphics.drawRoundedRectangle(button.getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
}

void ChordingAudioProcessorEditor::ChordingLookAndFeel::drawButtonText(
    juce::Graphics& graphics, juce::TextButton& button, bool, bool)
{
    graphics.setFont(font(13.0f, juce::Font::bold));
    graphics.setColour(button.getToggleState() ? mint : textPrimary);
    graphics.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(8, 2),
                            juce::Justification::centred, 1);
}

ChordingAudioProcessorEditor::ChordingAudioProcessorEditor(ChordingAudioProcessor& processor)
    : AudioProcessorEditor(&processor), processor_(processor)
{
    setLookAndFeel(&lookAndFeel_);
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(900, 560, 1200, 820);
    setSize(940, 620);

    for (auto* combo : { &keyBox_, &styleBox_, &moodBox_ })
    {
        combo->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(*combo);
    }

    styleBox_.addItem(utf8(u8"ベーシック"), 1);
    styleBox_.addItem(utf8(u8"ポップス"), 2);
    styleBox_.addItem(utf8(u8"ロック"), 3);
    styleBox_.addItem(utf8(u8"ジャズ"), 4);
    moodBox_.addItem(utf8(u8"指定なし"), 1);
    moodBox_.addItem(utf8(u8"明るい"), 2);
    moodBox_.addItem(utf8(u8"切ない"), 3);
    moodBox_.addItem(utf8(u8"緊張"), 4);
    moodBox_.addItem(utf8(u8"意外性"), 5);

    borrowedToggle_.setButtonText(utf8(u8"借用和音"));
    pauseButton_.setButtonText(utf8(u8"一時停止"));
    undoButton_.setButtonText(utf8(u8"元に戻す"));
    clearButton_.setButtonText(utf8(u8"消去"));
    copyButton_.setButtonText(utf8(u8"コピー"));

    const std::array<juce::Component*, 6> controls {
        &borrowedToggle_, &notationButton_, &pauseButton_,
        &undoButton_, &clearButton_, &copyButton_
    };
    for (auto* component : controls)
        addAndMakeVisible(*component);

    pauseButton_.setClickingTogglesState(true);
    notationButton_.setClickingTogglesState(true);

    keyBox_.onChange = [this] { applyControls(); };
    styleBox_.onChange = [this] { applyControls(); };
    moodBox_.onChange = [this] { applyControls(); };
    borrowedToggle_.onClick = [this] { applyControls(); };
    notationButton_.onClick = [this]
    {
        applyControls();
        populateKeyMenu();
    };
    pauseButton_.onClick = [this]
    {
        processor_.setCapturePaused(pauseButton_.getToggleState());
        pauseButton_.setButtonText(pauseButton_.getToggleState()
            ? utf8(u8"再開") : utf8(u8"一時停止"));
    };
    undoButton_.onClick = [this] { processor_.undoProgression(); };
    clearButton_.onClick = [this] { processor_.clearProgression(); };
    copyButton_.onClick = [this] { copyProgression(); };

    settings_ = processor_.getSettings();
    notationButton_.setToggleState(settings_.preferFlats, juce::dontSendNotification);
    notationButton_.setButtonText(settings_.preferFlats ? "FLAT" : "SHARP");
    borrowedToggle_.setToggleState(settings_.includeBorrowed, juce::dontSendNotification);
    styleBox_.setSelectedId(static_cast<int>(settings_.style) + 1, juce::dontSendNotification);
    moodBox_.setSelectedId(static_cast<int>(settings_.mood) + 1, juce::dontSendNotification);
    populateKeyMenu();
    refreshModel();
    startTimerHz(20);
}

ChordingAudioProcessorEditor::~ChordingAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void ChordingAudioProcessorEditor::populateKeyMenu()
{
    const auto selectedAutomatic = settings_.automaticKey;
    keyBox_.clear(juce::dontSendNotification);
    keyBox_.addItem(utf8(u8"自動"), 1);
    for (int root = 0; root < 12; ++root)
        keyBox_.addItem(chordName({ root, root, chording::ChordQuality::major, 0, 1.0f },
                                  settings_.preferFlats) + " major", root + 2);
    for (int root = 0; root < 12; ++root)
        keyBox_.addItem(chordName({ root, root, chording::ChordQuality::major, 0, 1.0f },
                                  settings_.preferFlats) + " minor", root + 14);

    const auto selectedId = selectedAutomatic ? 1
        : (settings_.keyMode == chording::ScaleMode::major ? settings_.keyRoot + 2
                                                           : settings_.keyRoot + 14);
    keyBox_.setSelectedId(selectedId, juce::dontSendNotification);
}

void ChordingAudioProcessorEditor::applyControls()
{
    const auto keyId = keyBox_.getSelectedId();
    settings_.automaticKey = keyId <= 1;
    if (keyId >= 2 && keyId <= 13)
    {
        settings_.keyRoot = keyId - 2;
        settings_.keyMode = chording::ScaleMode::major;
    }
    else if (keyId >= 14)
    {
        settings_.keyRoot = keyId - 14;
        settings_.keyMode = chording::ScaleMode::minor;
    }
    settings_.style = static_cast<chording::SuggestionStyle>(std::max(0, styleBox_.getSelectedId() - 1));
    settings_.mood = static_cast<chording::Mood>(std::max(0, moodBox_.getSelectedId() - 1));
    settings_.includeBorrowed = borrowedToggle_.getToggleState();
    settings_.preferFlats = notationButton_.getToggleState();
    notationButton_.setButtonText(settings_.preferFlats ? "FLAT" : "SHARP");
    processor_.setSettings(settings_);
    refreshModel();
}

void ChordingAudioProcessorEditor::timerCallback()
{
    refreshModel();
}

void ChordingAudioProcessorEditor::refreshModel()
{
    currentChord_ = processor_.getCurrentChord();
    progression_ = processor_.getProgression();
    settings_ = processor_.getSettings();
    alternatives_ = chording::ChordDetector::detectAlternatives(currentChord_.pitchClassMask,
                                                                 currentChord_.bass);

    auto keyHistory = progression_;
    if (currentChord_.isValid()
        && (keyHistory.empty() || keyHistory.back() != currentChord_))
        keyHistory.push_back(currentChord_);

    effectiveKey_ = settings_.automaticKey
        ? chording::HarmonyAdvisor::estimateKey(keyHistory)
        : chording::KeySignature { settings_.keyRoot, settings_.keyMode, 1.0f };

    const auto sourceChord = currentChord_.isValid() ? currentChord_
        : (progression_.empty() ? chording::ChordResult {} : progression_.back());
    suggestions_ = chording::HarmonyAdvisor::suggest(sourceChord, effectiveKey_, settings_.style,
                                                      settings_.mood, settings_.includeBorrowed,
                                                      progression_);
    repaint();
}

void ChordingAudioProcessorEditor::resized()
{
    auto header = getLocalBounds().reduced(24, 0).removeFromTop(64);
    header.removeFromLeft(190);

    auto keyArea = header.removeFromLeft(206).reduced(0, 14);
    keyArea.removeFromLeft(34);
    keyBox_.setBounds(keyArea);
    auto styleArea = header.removeFromLeft(174).reduced(0, 14);
    styleArea.removeFromLeft(48);
    styleBox_.setBounds(styleArea);
    auto moodArea = header.removeFromLeft(160).reduced(0, 14);
    moodArea.removeFromLeft(44);
    moodBox_.setBounds(moodArea);
    notationButton_.setBounds(header.removeFromRight(76).reduced(0, 14));

    auto bottom = getLocalBounds().reduced(24, 0).removeFromBottom(64).reduced(0, 12);
    borrowedToggle_.setBounds(bottom.removeFromLeft(110));
    bottom.removeFromLeft(12);
    pauseButton_.setBounds(bottom.removeFromLeft(92));
    bottom.removeFromLeft(8);
    undoButton_.setBounds(bottom.removeFromLeft(92));
    bottom.removeFromLeft(8);
    clearButton_.setBounds(bottom.removeFromLeft(72));
    copyButton_.setBounds(bottom.removeFromRight(92));
}

void ChordingAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(background);
    auto bounds = getLocalBounds();

    graphics.setColour(textPrimary);
    graphics.setFont(font(19.0f, juce::Font::bold));
    graphics.drawText("CHORDING", 24, 12, 160, 24, juce::Justification::centredLeft);
    graphics.setColour(textMuted);
    graphics.setFont(font(10.0f, juce::Font::bold));
    graphics.drawText("MIDI HARMONY GUIDE", 24, 36, 160, 16, juce::Justification::centredLeft);
    graphics.drawText(utf8(u8"キー"), 190, 21, 38, 22, juce::Justification::centredLeft);
    graphics.drawText(utf8(u8"スタイル"), 396, 21, 52, 22, juce::Justification::centredLeft);
    graphics.drawText(utf8(u8"印象"), 570, 21, 44, 22, juce::Justification::centredLeft);

    auto content = bounds.reduced(24, 0);
    content.removeFromTop(72);
    content.removeFromBottom(64);
    auto left = content.removeFromLeft(std::clamp(content.getWidth() * 34 / 100, 280, 350));
    content.removeFromLeft(14);
    auto right = content;
    drawPanel(graphics, left);

    graphics.setColour(textMuted);
    graphics.setFont(font(12.0f, juce::Font::bold));
    graphics.drawText(utf8(u8"現在のコード"), left.getX() + 20, left.getY() + 16,
                      left.getWidth() - 40, 22, juce::Justification::centredLeft);
    graphics.setColour(currentChord_.isValid() ? mint : textMuted);
    graphics.setFont(font(std::min(62.0f, static_cast<float>(left.getWidth()) * 0.18f), juce::Font::bold));
    graphics.drawFittedText(chordName(currentChord_, settings_.preferFlats),
                            left.reduced(20).withY(left.getY() + 52).withHeight(82),
                            juce::Justification::centredLeft, 1);

    graphics.setColour(textMuted);
    graphics.setFont(font(12.0f));
    auto alternativeText = utf8(u8"別解  ");
    auto alternativeCount = 0;
    for (std::size_t index = 1; index < alternatives_.size(); ++index)
    {
        if (! alternatives_[index].isValid())
            continue;
        if (alternativeCount++ > 0) alternativeText += "  /  ";
        alternativeText += chordName(alternatives_[index], settings_.preferFlats);
    }
    graphics.drawFittedText(alternativeCount == 0 ? utf8(u8"別解  --") : alternativeText,
                            left.getX() + 20, left.getY() + 142, left.getWidth() - 40, 22,
                            juce::Justification::centredLeft, 1);

    const auto confidence = currentChord_.isValid() ? currentChord_.confidence : 0.0f;
    auto meter = juce::Rectangle<int>(left.getX() + 20, left.getY() + 184, left.getWidth() - 40, 5);
    graphics.setColour(panelRaised);
    graphics.fillRoundedRectangle(meter.toFloat(), 2.0f);
    graphics.setColour(amber);
    graphics.fillRoundedRectangle(meter.withWidth(static_cast<int>(meter.getWidth() * confidence)).toFloat(), 2.0f);
    graphics.setColour(textMuted);
    graphics.drawText(utf8(u8"判定 ") + juce::String(static_cast<int>(confidence * 100.0f)) + "%",
                      meter.getX(), meter.getBottom() + 7, meter.getWidth(), 20,
                      juce::Justification::centredRight);

    auto keyboard = juce::Rectangle<int>(left.getX() + 20, left.getBottom() - 118,
                                         left.getWidth() - 40, 52);
    for (int pitch = 0; pitch < 12; ++pitch)
    {
        auto key = juce::Rectangle<int>(keyboard.getX() + pitch * keyboard.getWidth() / 12,
                                        keyboard.getY(), keyboard.getWidth() / 12 - 2,
                                        keyboard.getHeight());
        const auto active = (currentChord_.pitchClassMask & (1u << pitch)) != 0;
        graphics.setColour(active ? mint : panelRaised);
        graphics.fillRoundedRectangle(key.toFloat(), 3.0f);
        graphics.setColour(active ? background : textMuted);
        graphics.setFont(font(10.0f, active ? juce::Font::bold : juce::Font::plain));
        graphics.drawFittedText(juce::String(chording::ChordDetector::pitchClassName(
                                    pitch, settings_.preferFlats)), key.reduced(2),
                                juce::Justification::centred, 1);
    }
    graphics.setColour(textMuted);
    graphics.setFont(font(11.0f));
    graphics.drawText(utf8(u8"入力ノート ") + juce::String(processor_.getActiveNoteCount()),
                      left.getX() + 20, keyboard.getBottom() + 12, left.getWidth() - 40, 20,
                      juce::Justification::centredLeft);

    auto historyPanel = right.removeFromTop(std::max(142, right.getHeight() * 32 / 100));
    right.removeFromTop(14);
    auto suggestionsPanel = right;
    drawPanel(graphics, historyPanel);
    drawPanel(graphics, suggestionsPanel);

    graphics.setColour(textPrimary);
    graphics.setFont(font(14.0f, juce::Font::bold));
    graphics.drawText(utf8(u8"進行履歴"), historyPanel.getX() + 18, historyPanel.getY() + 14,
                      100, 22, juce::Justification::centredLeft);
    graphics.setColour(textMuted);
    graphics.setFont(font(11.0f));
    graphics.drawText(juce::String(progression_.size()) + " chords", historyPanel.getRight() - 100,
                      historyPanel.getY() + 14, 82, 22, juce::Justification::centredRight);

    constexpr auto chipWidth = 100;
    constexpr auto chipGap = 7;
    const auto chipLeft = historyPanel.getX() + 18;
    const auto chipTop = historyPanel.getY() + 48;
    const auto chipColumns = std::max(1, (historyPanel.getWidth() - 36 + chipGap)
                                         / (chipWidth + chipGap));
    const auto chipRows = std::max(1, (historyPanel.getHeight() - 58) / 38);
    const auto chipCapacity = static_cast<std::size_t>(chipColumns * chipRows);
    const auto firstVisible = progression_.size() > chipCapacity
        ? progression_.size() - chipCapacity : 0;
    for (std::size_t index = firstVisible; index < progression_.size(); ++index)
    {
        const auto name = chordName(progression_[index], settings_.preferFlats);
        const auto visibleIndex = static_cast<int>(index - firstVisible);
        const auto chipX = chipLeft + (visibleIndex % chipColumns) * (chipWidth + chipGap);
        const auto chipY = chipTop + (visibleIndex / chipColumns) * 38;
        auto chip = juce::Rectangle<int>(chipX, chipY, chipWidth, 30);
        graphics.setColour(panelRaised);
        graphics.fillRoundedRectangle(chip.toFloat(), 4.0f);
        graphics.setColour(index + 1 == progression_.size() ? amber : textPrimary);
        graphics.setFont(font(12.0f, juce::Font::bold));
        graphics.drawFittedText(juce::String(index + 1) + "  " + name, chip.reduced(8, 2),
                                juce::Justification::centred, 1);
    }
    if (progression_.empty())
    {
        graphics.setColour(textMuted);
        graphics.setFont(font(13.0f));
        graphics.drawText("--", historyPanel.reduced(18).withTrimmedTop(34),
                          juce::Justification::centredLeft);
    }

    graphics.setColour(textPrimary);
    graphics.setFont(font(14.0f, juce::Font::bold));
    graphics.drawText(utf8(u8"次の候補"), suggestionsPanel.getX() + 18, suggestionsPanel.getY() + 13,
                      100, 22, juce::Justification::centredLeft);
    graphics.setColour(settings_.automaticKey ? amber : mint);
    graphics.setFont(font(11.0f, juce::Font::bold));
    const auto keyPrefix = settings_.automaticKey ? utf8(u8"推定キー  ") : utf8(u8"固定キー  ");
    graphics.drawText(keyPrefix + keyName(effectiveKey_, settings_.preferFlats),
                      suggestionsPanel.getRight() - 190, suggestionsPanel.getY() + 13,
                      172, 22, juce::Justification::centredRight);

    const auto rowsTop = suggestionsPanel.getY() + 43;
    const auto availableHeight = suggestionsPanel.getBottom() - rowsTop - 12;
    const auto rowHeight = std::clamp(availableHeight / 6, 29, 40);
    for (std::size_t index = 0; index < suggestions_.size(); ++index)
    {
        auto row = juce::Rectangle<int>(suggestionsPanel.getX() + 12,
                                        rowsTop + static_cast<int>(index) * rowHeight,
                                        suggestionsPanel.getWidth() - 24, rowHeight - 3);
        suggestionRows_[index] = row;
        const auto selected = static_cast<int>(index) == selectedSuggestion_;
        graphics.setColour(selected ? mint.withAlpha(0.16f) : panelRaised);
        graphics.fillRoundedRectangle(row.toFloat(), 4.0f);
        graphics.setColour(selected ? mint : textPrimary);
        graphics.setFont(font(14.0f, juce::Font::bold));
        graphics.drawFittedText(chordName(suggestions_[index].chord, settings_.preferFlats),
                                row.getX() + 10, row.getY(), 92, row.getHeight(),
                                juce::Justification::centredLeft, 1);
        graphics.setColour(suggestions_[index].role == chording::HarmonicRole::borrowed ? coral : amber);
        graphics.setFont(font(11.0f, juce::Font::bold));
        graphics.drawText(roleText(suggestions_[index].role), row.getX() + 108, row.getY(),
                          62, row.getHeight(), juce::Justification::centredLeft);
        graphics.setColour(textMuted);
        graphics.setFont(font(11.0f));
        const auto detail = selected
            ? utf8(u8"構成音  ") + pitchList(suggestions_[index].chord.pitchClassMask, settings_.preferFlats)
            : (suggestions_[index].role == chording::HarmonicRole::dominant ? utf8(u8"解決へ向かう緊張感")
               : suggestions_[index].role == chording::HarmonicRole::predominant ? utf8(u8"次の展開を準備")
               : suggestions_[index].role == chording::HarmonicRole::borrowed ? utf8(u8"キー外の響きを加える")
               : suggestions_[index].role == chording::HarmonicRole::relative ? utf8(u8"共通音の多い移動")
               : utf8(u8"安定した着地点"));
        graphics.drawFittedText(detail, row.getX() + 174, row.getY(), row.getWidth() - 184,
                                row.getHeight(), juce::Justification::centredLeft, 1);
    }

    graphics.setColour(processor_.isCapturePaused() ? coral : mint);
    graphics.fillEllipse(static_cast<float>(getWidth() - 126), static_cast<float>(getHeight() - 38), 7.0f, 7.0f);
    graphics.setColour(textMuted);
    graphics.setFont(font(10.0f, juce::Font::bold));
    graphics.drawText(processor_.isCapturePaused() ? "CAPTURE PAUSED" : "CAPTURE LIVE",
                      getWidth() - 114, getHeight() - 44, 90, 20,
                      juce::Justification::centredLeft);
}

void ChordingAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    for (std::size_t index = 0; index < suggestionRows_.size(); ++index)
    {
        if (! suggestionRows_[index].contains(event.getPosition()))
            continue;
        selectedSuggestion_ = selectedSuggestion_ == static_cast<int>(index)
            ? -1 : static_cast<int>(index);
        repaint();
        return;
    }
}

void ChordingAudioProcessorEditor::copyProgression() const
{
    juce::String text;
    for (std::size_t index = 0; index < progression_.size(); ++index)
    {
        if (index > 0) text += " - ";
        text += chordName(progression_[index], settings_.preferFlats);
    }
    if (text.isNotEmpty())
        juce::SystemClipboard::copyTextToClipboard(text);
}

juce::String ChordingAudioProcessorEditor::chordName(const chording::ChordResult& chord,
                                                      const bool flats)
{
    return juce::String(chording::ChordDetector::format(chord, flats));
}

juce::String ChordingAudioProcessorEditor::keyName(const chording::KeySignature& key,
                                                    const bool flats)
{
    return juce::String(chording::ChordDetector::pitchClassName(key.root, flats))
        + (key.mode == chording::ScaleMode::major ? " major" : " minor");
}

juce::String ChordingAudioProcessorEditor::roleText(const chording::HarmonicRole role)
{
    switch (role)
    {
        case chording::HarmonicRole::tonic: return utf8(u8"安定");
        case chording::HarmonicRole::predominant: return utf8(u8"展開");
        case chording::HarmonicRole::dominant: return utf8(u8"緊張");
        case chording::HarmonicRole::relative: return utf8(u8"滑らか");
        case chording::HarmonicRole::borrowed: return utf8(u8"意外性");
        case chording::HarmonicRole::colour: return utf8(u8"色彩");
    }
    return {};
}

juce::String ChordingAudioProcessorEditor::pitchList(const std::uint16_t mask,
                                                      const bool flats)
{
    juce::String result;
    for (int pitch = 0; pitch < 12; ++pitch)
    {
        if ((mask & (1u << pitch)) == 0)
            continue;
        if (result.isNotEmpty()) result += " ";
        result += juce::String(chording::ChordDetector::pitchClassName(pitch, flats));
    }
    return result;
}

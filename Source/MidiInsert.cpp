#include "PluginProcessor.h"
#include "MidiInsertState.h"
#include "pluginterfaces/midi/imidieffect.h"
#include "pluginterfaces/gui/iplugui.h"
#include "pluginterfaces/base/iplugstorage.h"

#include <cstring>
#include <memory>
#include <new>

namespace
{
#pragma warning(push)
#pragma warning(disable: 4310) // SDK GUID macro narrows individual bytes to char.
constexpr char unknownId[] = INLINE_UID(0, 0, 0xC0000000, 0x00000046);
constexpr char baseId[] = INLINE_UID(0x22888DDB, 0x156E45AE, 0x8358B348, 0x08190625);
constexpr char factoryId[] = INLINE_UID(0x7A4D811C, 0x52114A1F, 0xAED9D2EE, 0x0B43BF9F);
constexpr char effectId[] = INLINE_UID(0x573C8B06, 0x17BE488E, 0xB458400C, 0xD3EE099E);
constexpr char accessorId[] = INLINE_UID(0xA2BBC853, 0x1E3548C3, 0xB6396F98, 0x521E9783);
constexpr char editorFactoryId[] = INLINE_UID(0xBB0E69FE, 0x9D7242C8, 0xB875F92A, 0xBE66FAAA);
constexpr char viewId[] = INLINE_UID(0xAA3E50FF, 0xB78840EE, 0xADCD48E8, 0x094CEDB7);
constexpr char chunkId[] = INLINE_UID(0x7041A824, 0xFC9A488A, 0x8F07DAC0, 0x5061E933);
constexpr char classId[] = INLINE_UID(0xEA8DD551, 0xAAB14830, 0x849A21C6, 0xCA9E9F63);
#pragma warning(pop)

bool matches(const char* a, const char* b) noexcept { return a && std::memcmp(a, b, 16) == 0; }

struct Model
{
    juce::ScopedJuceInitialiser_GUI gui;
    ChordingAudioProcessor processor;
};

class View final : public IPlugView
{
public:
    explicit View(std::shared_ptr<Model> model) : model_(std::move(model)) {}
    tresult PLUGIN_API queryInterface(const char* id, void** object) override
    {
        if (!object) return kInvalidArgument;
        *object = nullptr;
        if (!matches(id, unknownId) && !matches(id, viewId)) return kNoInterface;
        *object = static_cast<IPlugView*>(this); addRef(); return kResultOk;
    }
    unsigned long PLUGIN_API addRef() override { return ++references_; }
    unsigned long PLUGIN_API release() override
    {
        const auto remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    tresult PLUGIN_API attached(void* parent) override
    {
        if (!parent || editor_) return kInvalidArgument;
        try
        {
            editor_.reset(model_->processor.createEditor());
            editor_->setResizable(false, false);
            editor_->setSize(940, 620);
            editor_->addToDesktop(0, parent);
            editor_->setVisible(true);
            return kResultOk;
        }
        catch (...) { editor_.reset(); return kInternalError; }
    }
    tresult PLUGIN_API removed() override { editor_.reset(); return kResultOk; }
    tresult PLUGIN_API idle() override { return kResultOk; }
    tresult PLUGIN_API onWheel(float) override { return kResultFalse; }
    tresult PLUGIN_API onKey(char, long, long) override { return kResultFalse; }
    tresult PLUGIN_API onSize(ViewRect* rect) override
    {
        if (!rect || rect->right <= rect->left || rect->bottom <= rect->top) return kInvalidArgument;
        if (editor_) editor_->setBounds(0, 0, rect->right - rect->left, rect->bottom - rect->top);
        return kResultOk;
    }
private:
    std::atomic<unsigned long> references_{1};
    std::shared_ptr<Model> model_;
    std::unique_ptr<juce::AudioProcessorEditor> editor_;
};

class MidiInsert final : public IMidiEffect, public IEditorFactory, public IPersistentChunk
{
public:
    tresult PLUGIN_API queryInterface(const char* id, void** object) override
    {
        if (!object) return kInvalidArgument;
        *object = nullptr;
        if (matches(id, unknownId) || matches(id, baseId) || matches(id, effectId))
            *object = static_cast<IMidiEffect*>(this);
        else if (matches(id, editorFactoryId)) *object = static_cast<IEditorFactory*>(this);
        else if (matches(id, chunkId)) *object = static_cast<IPersistentChunk*>(this);
        else return kNoInterface;
        addRef(); return kResultOk;
    }
    unsigned long PLUGIN_API addRef() override { return ++references_; }
    unsigned long PLUGIN_API release() override
    {
        const auto remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    tresult PLUGIN_API initialize(FUnknown* context) override
    {
        if (!context || accessor_) return kInvalidArgument;
        void* object = nullptr;
        const auto result = context->queryInterface(accessorId, &object);
        if (result != kResultOk) return result;
        if (!object) return kNotInitialized;
        accessor_ = static_cast<IMEAccessor*>(object);
        try { model_ = std::make_shared<Model>(); }
        catch (...) { accessor_->release(); accessor_ = nullptr; return kOutOfMemory; }
        accessor_->setCanPlayInStop(true);
        return kResultOk;
    }
    tresult PLUGIN_API terminate() override
    {
        clearInput();
        if (accessor_) accessor_->release();
        accessor_ = nullptr;
        model_.reset();
        return kResultOk;
    }
    tresult PLUGIN_API getCaps(MidiEffectCaps* caps) override
    {
        if (!caps || caps->_sizeof < sizeof(MidiEffectCaps)) return kInvalidArgument;
        caps->flags = kMEIsEditable;
        return kResultOk;
    }
    tresult PLUGIN_API configure(long usage) override
    {
        return usage == kMEUsageInsert ? kResultOk : kResultFalse;
    }
    tresult PLUGIN_API receiveEvent(IMEObjectID event) override
    {
        if (!accessor_ || !model_) return kNotInitialized;
        if (!event) return kInvalidArgument;
        const auto status = static_cast<int>(accessor_->getStatus(event));
        if ((status & 0xf0) == 0x80 || (status & 0xf0) == 0x90 || (status & 0xf0) == 0xb0)
        {
            input_.receive(status, accessor_->getChannel(event), accessor_->getData1(event),
                           accessor_->getData2(event), accessor_->getStart(event),
                           accessor_->getLength(event), accessor_->isImmediateEvent(event));
            publish();
        }
        // Preserve the original opaque event, including CC64, duration, timing and SysEx.
        return accessor_->passToOutput(event);
    }
    tresult PLUGIN_API playAction(long, long to, bool immediate) override
    {
        input_.advance(to, immediate); publish(); return kResultOk;
    }
    tresult PLUGIN_API positAction(long) override { clearInput(); return kResultOk; }
    tresult PLUGIN_API swapAction(long, long) override { clearInput(); return kResultOk; }
    tresult PLUGIN_API stopAction(long) override { clearInput(); return kResultOk; }
    tresult PLUGIN_API startAction(long) override { clearInput(); return kResultOk; }
    tresult PLUGIN_API getEditorSize(const char*, ViewRect* rect) override
    {
        if (!rect) return kInvalidArgument;
        *rect = ViewRect(0, 0, 940, 620); return kResultOk;
    }
    tresult PLUGIN_API createEditor(const char*, ViewRect*, IPlugView** view) override
    {
        if (!view) return kInvalidArgument;
        *view = nullptr;
        if (!model_) return kNotInitialized;
        *view = new (std::nothrow) View(model_);
        return *view ? kResultOk : kOutOfMemory;
    }
    tresult PLUGIN_API getUID(char* uid) override
    {
        if (!uid) return kInvalidArgument;
        std::memcpy(uid, classId, 16); return kResultOk;
    }
    tresult PLUGIN_API setChunk(char* chunk, long size) override
    {
        if (!model_) return kNotInitialized;
        if (!chunk || size <= 0 || size > 1024 * 1024) return kInvalidArgument;
        try { model_->processor.setStateInformation(chunk, size); return kResultOk; }
        catch (...) { return kInternalError; }
    }
    tresult PLUGIN_API getChunk(char* chunk, long* size) override
    {
        if (!model_) return kNotInitialized;
        if (!size) return kInvalidArgument;
        try
        {
            if (!chunk) model_->processor.getStateInformation(savedState_);
            const auto required = static_cast<long>(savedState_.getSize());
            if (chunk && (*size < required || required == 0)) { *size = required; return kInvalidArgument; }
            if (chunk) std::memcpy(chunk, savedState_.getData(), savedState_.getSize());
            *size = required; return kResultOk;
        }
        catch (...) { return kInternalError; }
    }
private:
    ~MidiInsert() { terminate(); }
    void publish() noexcept
    {
        if (model_) model_->processor.publishMidiInput(input_.snapshot(), input_.takeNoteOns());
    }
    void clearInput() noexcept { input_.clear(); publish(); }
    std::atomic<unsigned long> references_{1};
    IMEAccessor* accessor_ = nullptr;
    std::shared_ptr<Model> model_;
    chording::MidiInsertState input_;
    juce::MemoryBlock savedState_;
};

class Factory final : public IPluginFactory
{
public:
    tresult PLUGIN_API queryInterface(const char* id, void** object) override
    {
        if (!object) return kInvalidArgument;
        *object = nullptr;
        if (!matches(id, unknownId) && !matches(id, factoryId)) return kNoInterface;
        *object = static_cast<IPluginFactory*>(this); addRef(); return kResultOk;
    }
    unsigned long PLUGIN_API addRef() override { return ++references_; }
    unsigned long PLUGIN_API release() override
    {
        const auto remaining = --references_;
        if (!remaining) delete this;
        return remaining;
    }
    tresult PLUGIN_API getFactoryInfo(PFactoryInfo* info) override
    {
        if (!info) return kInvalidArgument;
        *info = {};
        std::strcpy(info->vendor, "Chording");
        std::strcpy(info->url, "https://github.com/uchimaki-vensley/chording");
        return kResultOk;
    }
    long PLUGIN_API countClasses() override { return 1; }
    tresult PLUGIN_API getClassInfo(long index, PClassInfo* info) override
    {
        if (index != 0 || !info) return kInvalidArgument;
        *info = {};
        std::memcpy(info->cid, classId, 16);
        info->cardinality = PClassInfo::kManyInstances;
        std::strcpy(info->category, kMidiModuleClass);
        std::strcpy(info->name, "Chording MIDI");
        return kResultOk;
    }
    tresult PLUGIN_API createInstance(const char* cid, const char* id, void** object) override
    {
        if (!object) return kInvalidArgument;
        *object = nullptr;
        if (!matches(cid, classId)) return kInvalidArgument;
        auto* instance = new (std::nothrow) MidiInsert;
        if (!instance) return kOutOfMemory;
        const auto result = instance->queryInterface(id, object);
        instance->release(); return result;
    }
private:
    std::atomic<unsigned long> references_{1};
};
}

extern "C" IPluginFactory* PLUGIN_API GetPluginFactory() { return new (std::nothrow) Factory; }
extern "C" __declspec(dllexport) bool InitDll() { return true; }
extern "C" __declspec(dllexport) bool ExitDll() { return true; }

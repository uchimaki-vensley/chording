#include "pluginterfaces/midi/imidieffect.h"
#include "pluginterfaces/gui/iplugui.h"
#include "pluginterfaces/base/iplugstorage.h"
#include <array>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
void expect(bool condition)
{
    if (!condition) { std::cerr << "MIDI Insert contract check failed\n"; std::exit(EXIT_FAILURE); }
}

class Host final : public IMEAccessor
{
public:
    unsigned long references = 1;
    IMEObjectID received = nullptr;
    int passed = 0;
    int mutations = 0;
    tresult outputResult = kResultOk;
    tresult PLUGIN_API queryInterface(const char* interfaceId, void** object) override
    {
#pragma warning(push)
#pragma warning(disable: 4310) // SDK GUID byte encoding.
        constexpr char accessorId[] = INLINE_UID(0xA2BBC853, 0x1E3548C3, 0xB6396F98, 0x521E9783);
#pragma warning(pop)
        *object = nullptr;
        if (std::memcmp(interfaceId, accessorId, 16) != 0) return kNoInterface;
        *object = static_cast<IMEAccessor*>(this); addRef(); return kResultOk;
    }
    unsigned long PLUGIN_API addRef() override { return ++references; }
    unsigned long PLUGIN_API release() override { return --references; }
    IMEObjectID PLUGIN_API createMidiEvent(MidiStatus, long, long, long) override { ++mutations; return nullptr; }
    IMEObjectID PLUGIN_API createSysexEvent(unsigned char*, long) override { ++mutations; return nullptr; }
    IMEObjectID PLUGIN_API duplicateMidiEvent(IMEObjectID) override { ++mutations; return nullptr; }
    tresult PLUGIN_API destroyEvent(IMEObjectID) override { ++mutations; return kResultOk; }
    tresult PLUGIN_API passToOutputQueue(IMEObjectID) override { ++mutations; return kResultOk; }
    tresult PLUGIN_API passToOutput(IMEObjectID event) override { received = event; ++passed; return outputResult; }
    tresult PLUGIN_API setStart(IMEObjectID, long) override { ++mutations; return kResultOk; }
    long PLUGIN_API getStart(IMEObjectID) override { return 0; }
    tresult PLUGIN_API setLength(IMEObjectID, long) override { ++mutations; return kResultOk; }
    long PLUGIN_API getLength(IMEObjectID) override { return 0; }
    tresult PLUGIN_API setImmediateEvent(IMEObjectID, bool) override { ++mutations; return kResultOk; }
    bool PLUGIN_API isImmediateEvent(IMEObjectID) override { return true; }
    tresult PLUGIN_API setChannel(IMEObjectID, int) override { ++mutations; return kResultOk; }
    int PLUGIN_API getChannel(IMEObjectID) override { return 0; }
    MidiStatus PLUGIN_API getStatus(IMEObjectID) override { return kNoteOn; }
    tresult PLUGIN_API setStatus(IMEObjectID, MidiStatus) override { ++mutations; return kResultOk; }
    int PLUGIN_API getData1(IMEObjectID) override { return 60; }
    tresult PLUGIN_API setData1(IMEObjectID, int) override { ++mutations; return kResultOk; }
    int PLUGIN_API getData2(IMEObjectID) override { return 100; }
    tresult PLUGIN_API setData2(IMEObjectID, int) override { ++mutations; return kResultOk; }
    int PLUGIN_API getData3(IMEObjectID) override { return 0; }
    tresult PLUGIN_API setData3(IMEObjectID, int) override { ++mutations; return kResultOk; }
    int PLUGIN_API getMicroTuning(IMEObjectID) override { return 0; }
    tresult PLUGIN_API setMicroTuning(IMEObjectID, int) override { ++mutations; return kResultOk; }
    long PLUGIN_API getMidiChannel() override { return 0; }
    long PLUGIN_API getInterval() override { return 1; }
    tresult PLUGIN_API setInterval(long) override { ++mutations; return kResultOk; }
    tresult PLUGIN_API getContextInfo(long, MidiContextInfo*) override { return kNotImplemented; }
    tresult PLUGIN_API setCanPlayInStop(int) override { ++mutations; return kResultOk; }
};
}

int main()
{
    auto* factory = GetPluginFactory();
    expect(factory != nullptr && factory->countClasses() == 1);
    PClassInfo info{};
    expect(factory->getClassInfo(0, &info) == kResultOk);
    expect(std::strcmp(info.category, "Midi Module Class") == 0);
    expect(factory->getClassInfo(1, &info) == kInvalidArgument);
    expect(factory->getFactoryInfo(nullptr) == kInvalidArgument);
#pragma warning(push)
#pragma warning(disable: 4310) // SDK GUID byte encoding.
    constexpr char effectId[] = INLINE_UID(0x573C8B06, 0x17BE488E, 0xB458400C, 0xD3EE099E);
#pragma warning(pop)
    char unknownId[16]{};
    void* object = nullptr;
    expect(factory->createInstance(info.cid, unknownId, &object) == kNoInterface && object == nullptr);
    expect(factory->createInstance(info.cid, effectId, &object) == kResultOk && object != nullptr);
    auto* effect = static_cast<IMidiEffect*>(object);
    Host host;
    int opaqueEvent = 42;
    expect(effect->receiveEvent(&opaqueEvent) == kNotInitialized);
    expect(effect->initialize(nullptr) == kInvalidArgument);
    expect(effect->initialize(&host) == kResultOk && host.references == 2 && host.mutations == 1);
    expect(effect->initialize(&host) == kInvalidArgument && host.references == 2);
    MidiEffectCaps caps{sizeof(MidiEffectCaps), -1};
    expect(effect->getCaps(&caps) == kResultOk && caps.flags == kMEIsEditable);
    caps._sizeof = 0;
    expect(effect->getCaps(&caps) == kInvalidArgument);
    expect(effect->configure(kMEUsageInsert) == kResultOk);
    expect(effect->configure(kMEUsageSend) == kResultFalse);

#pragma warning(push)
#pragma warning(disable: 4310) // SDK GUID byte encoding.
    constexpr char editorFactoryId[] = INLINE_UID(0xBB0E69FE, 0x9D7242C8, 0xB875F92A, 0xBE66FAAA);
    constexpr char chunkId[] = INLINE_UID(0x7041A824, 0xFC9A488A, 0x8F07DAC0, 0x5061E933);
#pragma warning(pop)
    void* editorObject = nullptr;
    expect(effect->queryInterface(editorFactoryId, &editorObject) == kResultOk);
    auto* editorFactory = static_cast<IEditorFactory*>(editorObject);
    ViewRect editorSize;
    expect(editorFactory->getEditorSize("editor", &editorSize) == kResultOk);
    expect(editorSize.right == 940 && editorSize.bottom == 620);
    IPlugView* view = nullptr;
    expect(editorFactory->createEditor("editor", &editorSize, &view) == kResultOk && view != nullptr);
    expect(view->release() == 0);
    expect(editorFactory->release() > 0);

    void* chunkObject = nullptr;
    expect(effect->queryInterface(chunkId, &chunkObject) == kResultOk);
    auto* persistent = static_cast<IPersistentChunk*>(chunkObject);
    std::array<char, 17> uid{};
    uid.back() = 42;
    expect(persistent->getUID(uid.data()) == kResultOk && uid.back() == 42);
    long stateSize = 0;
    expect(persistent->getChunk(nullptr, &stateSize) == kResultOk && stateSize > 0);
    std::vector<char> state(static_cast<std::size_t>(stateSize));
    expect(persistent->getChunk(state.data(), &stateSize) == kResultOk);
    expect(persistent->setChunk(state.data(), stateSize) == kResultOk);
    expect(persistent->release() > 0);

    expect(effect->receiveEvent(nullptr) == kInvalidArgument);
    expect(effect->receiveEvent(&opaqueEvent) == kResultOk);
    expect(host.passed == 1 && host.received == &opaqueEvent && opaqueEvent == 42 && host.mutations == 1);
    host.outputResult = kInternalError;
    expect(effect->receiveEvent(&opaqueEvent) == kInternalError);
    expect(effect->terminate() == kResultOk && host.references == 1);
    expect(effect->terminate() == kResultOk && host.references == 1);
    expect(effect->initialize(&host) == kResultOk);
    expect(effect->release() == 0 && host.references == 1);
    expect(factory->release() == 0);
    std::cout << "MIDI Insert contract tests passed\n";
}

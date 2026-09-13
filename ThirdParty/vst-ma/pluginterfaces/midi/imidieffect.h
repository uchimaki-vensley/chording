//-----------------------------------------------------------------------------
// VST Module Architecture SDK
// Version 1.0    Date : 01/2004
//
// Category     : SDK MIDI Effect Interfaces
// Filename     : imidieffect.h
// Created by   : Matthias Juwan, Werner Kracht
// Description  : Midi Effect Interfaces
//
//-----------------------------------------------------------------------------
// LICENSE
// © 2004, Steinberg Media Technologies, All Rights Reserved
//-----------------------------------------------------------------------------
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Steinberg Media Technologies nor the names of its
//     contributors may be used to endorse or promote products derived from this
//     software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#ifndef __imidieffect__
#define __imidieffect__

#ifndef __ipluginbase__
#include "../base/ipluginbase.h"
#endif

class IMEAccessor;

//------------------------------------------------------------------------
#ifndef kMidiModuleClass
#define kMidiModuleClass "Midi Module Class"
#endif

//------------------------------------------------------------------------
typedef void* IMEObjectID; // opaque Midi Event datatype


#ifndef __midistatus_defined__
#define __midistatus_defined__
//------------------------------------------------------------------------
//  MidiStatus table
//------------------------------------------------------------------------
enum MidiStatus
{
//------------------------------------------------------------------------
	kNoteOff			= 0x80,	// note, off velocity
	kNoteOn				= 0x90,	// note, on velocity
	kPolyPressure		= 0xA0,	// note, pressure
	kController			= 0xB0,	// controller, value
	kProgramChange		= 0xC0,	// program
	kAfterTouch			= 0xD0,	// pressure
	kPitchBend			= 0xE0,	// lsb, msb

	kSysexStart			= 0xF0,	// data byte until sysexend
	kQuarterFrame		= 0xF1,	// quarter frame message
	kSongPointer		= 0xF2,	// msb, lsb
	kSongSelect			= 0xF3,	// number
	kCableSelect		= 0xF5,	// cable
	kSysexEnd			= 0xF7,	//
	kMidiClock			= 0xF8,	//
	kMidiClockStart		= 0xFA,	//
	kMidiClockContinue	= 0xFB,	//
	kMidiClockStop		= 0xFC,	//
	kActiveSensing		= 0xFE,	//
	kMidiReset			= 0xFF	//
//------------------------------------------------------------------------
};
#endif

//------------------------------------------------------------------------
//  MidiContextInfo
//------------------------------------------------------------------------
struct MidiContextInfo
{
//------------------------------------------------------------------------
	long tempo;
	long ppqPosition;
	long stopPosition;

	long performedCyclesPpq;
	long performedCyclesImmediatePpq;
	long numberOfPerformedCycles;

	long barPos;
	long barCount;
	long beatCount;
	long remainder;

	short numerator;
	short denominator;
//------------------------------------------------------------------------
};

//------------------------------------------------------------------------
//  MidiEffectCaps
//------------------------------------------------------------------------
struct MidiEffectCaps
{
	size_t _sizeof;		// sizeof MidiEffectCaps (set by host)
	long flags;			// see constants
};

//------------------------------------------------------------------------
//  constants
//------------------------------------------------------------------------
enum
{
//------------------------------------------------------------------------
	kMEIsEditable   = 1<<0,
	kMEHasInspector = 1<<1,

	kMEUsageInsert = 0,
	kMEUsageSend,
	kMEUsageOffline
//------------------------------------------------------------------------
};

//------------------------------------------------------------------------
//  IMidiEffect interface declaration
//------------------------------------------------------------------------

class IMidiEffect: public IPluginBase
{
public:
//------------------------------------------------------------------------
	virtual tresult PLUGIN_API getCaps (MidiEffectCaps*) = 0;
	virtual tresult PLUGIN_API configure (long usage) = 0;

	virtual tresult PLUGIN_API receiveEvent (IMEObjectID event) = 0;

	virtual tresult PLUGIN_API playAction (long fromPPQ, long toPPQ, bool immediate) = 0;
	virtual tresult PLUGIN_API positAction (long position) = 0;
	virtual tresult PLUGIN_API swapAction (long position, long length) = 0;
	virtual tresult PLUGIN_API stopAction (long position) = 0;
	virtual tresult PLUGIN_API startAction (long position) = 0;
//------------------------------------------------------------------------
	static const FUID iid;
};

DECLARE_CLASS_IID (IMidiEffect, 0x573C8B06, 0x17BE488E, 0xB458400C, 0xD3EE099E)


//------------------------------------------------------------------------
//  IMEAccessor interface declaration (host)
//------------------------------------------------------------------------
class IMEAccessor: public FUnknown
{
public:
//------------------------------------------------------------------------
//  Event creation and output
//------------------------------------------------------------------------
	virtual IMEObjectID PLUGIN_API createMidiEvent (MidiStatus type, long data1, long data2, long length) = 0;
	virtual IMEObjectID PLUGIN_API createSysexEvent (unsigned char* data, long size) = 0;
	virtual IMEObjectID PLUGIN_API duplicateMidiEvent (IMEObjectID event) = 0;
	virtual tresult PLUGIN_API destroyEvent (IMEObjectID event) = 0;

	virtual tresult PLUGIN_API passToOutputQueue (IMEObjectID event) = 0;
	virtual tresult PLUGIN_API passToOutput (IMEObjectID event) = 0;

//------------------------------------------------------------------------
//  Event manipulation
//------------------------------------------------------------------------
	virtual tresult	PLUGIN_API setStart (IMEObjectID event, long ppq) = 0;
	virtual long	PLUGIN_API getStart (IMEObjectID event) = 0;
	virtual tresult	PLUGIN_API setLength (IMEObjectID event, long ppq) = 0;
	virtual long	PLUGIN_API getLength (IMEObjectID event) = 0;

	virtual tresult PLUGIN_API setImmediateEvent (IMEObjectID event, bool state) = 0;
	virtual bool	PLUGIN_API isImmediateEvent (IMEObjectID event) = 0;

	virtual tresult PLUGIN_API setChannel (IMEObjectID event, int channel) = 0;
	virtual int		PLUGIN_API getChannel (IMEObjectID event) = 0;

	virtual MidiStatus PLUGIN_API getStatus (IMEObjectID event) = 0;
	virtual tresult PLUGIN_API setStatus (IMEObjectID event, MidiStatus status) = 0;

	virtual int		PLUGIN_API getData1 (IMEObjectID event) = 0;
	virtual tresult PLUGIN_API setData1 (IMEObjectID event, int data) = 0;
	virtual int		PLUGIN_API getData2 (IMEObjectID event) = 0;
	virtual tresult PLUGIN_API setData2 (IMEObjectID event, int data) = 0;
	virtual int		PLUGIN_API getData3 (IMEObjectID event) = 0;
	virtual tresult PLUGIN_API setData3 (IMEObjectID event, int data) = 0;

	virtual int		PLUGIN_API getMicroTuning (IMEObjectID event) = 0;
	virtual tresult PLUGIN_API setMicroTuning (IMEObjectID event, int cent) = 0;

//------------------------------------------------------------------------
//  Context information
//------------------------------------------------------------------------
	virtual long	PLUGIN_API getMidiChannel () = 0;
	virtual long	PLUGIN_API getInterval () = 0;
	virtual tresult	PLUGIN_API setInterval (long interval) = 0;
	virtual tresult PLUGIN_API getContextInfo (long ppq, MidiContextInfo *info) = 0;
	virtual tresult PLUGIN_API setCanPlayInStop (int state) = 0;
//------------------------------------------------------------------------
	static const FUID iid;
};

DECLARE_CLASS_IID (IMEAccessor, 0xA2BBC853, 0x1E3548C3, 0xB6396F98, 0x521E9783)

//------------------------------------------------------------------------
//  IMEAccessor2 interface declaration
//------------------------------------------------------------------------

class IMEAccessor2: public IMEAccessor
{
public:
//------------------------------------------------------------------------
	virtual tresult	PLUGIN_API receiveMidiEvent (IMEObjectID event) = 0; // send Midi event to host

	virtual int		PLUGIN_API getSysexSize (IMEObjectID event) = 0;
	virtual void*	PLUGIN_API getSysexMessage (IMEObjectID event) = 0;
//------------------------------------------------------------------------
	static const FUID iid;
};

DECLARE_CLASS_IID (IMEAccessor2, 0x17C91D3F, 0x86064D84, 0xA6CC12AB, 0x2C0996E7)

//------------------------------------------------------------------------
//  IMasterTrackInfo interface declaration  (host)
//------------------------------------------------------------------------

class IMasterTrackInfo: public FUnknown
{
public:
//------------------------------------------------------------------------
	virtual long	PLUGIN_API getPpqBase () = 0;
	virtual long	PLUGIN_API seconds2ppq (long seconds) = 0;
	virtual long	PLUGIN_API ppq2seconds (long ppq) = 0;

	virtual long	PLUGIN_API countTempoEvents () = 0;
	virtual tresult	PLUGIN_API getTempo (long index, long* ppq, long* bpm) = 0; // bpm * 100
	virtual long	PLUGIN_API getTempoIndex (long ppq) = 0;

	virtual tresult	PLUGIN_API getBar (long ppq, long* barCount, long* beatCount, long* remainder) = 0;
	virtual long	PLUGIN_API getBarPosition (long barCount, long beatCount, long remainder) = 0;

	virtual long	PLUGIN_API countSignatureEvents () = 0;
	virtual tresult	PLUGIN_API getSignature (long index, long* numerator, long* denominator) = 0;
	virtual long	PLUGIN_API getSignatureIndex (long ppq) = 0;
//------------------------------------------------------------------------
	static const FUID iid;
};

DECLARE_CLASS_IID (IMasterTrackInfo, 0x8000BF71, 0xA8F249CF, 0x8574744B, 0xEEF4211C)

//------------------------------------------------------------------------
//  ITransportInfo interface declaration  (host)
//------------------------------------------------------------------------

class ITransportInfo: public FUnknown
{
public:
//------------------------------------------------------------------------
	virtual long PLUGIN_API getSystemTimePpq () = 0;
//------------------------------------------------------------------------
	static const FUID iid;
};

DECLARE_CLASS_IID (ITransportInfo, 0xD335041A, 0x762142C6, 0x926A6F39, 0xEDCCBB7B)

#endif
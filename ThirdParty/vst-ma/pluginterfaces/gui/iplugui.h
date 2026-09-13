//-----------------------------------------------------------------------------
// VST Module Architecture SDK
// Version 1.0    Date : 01/2004
//
// Category     : SDK Core Interfaces
// Filename     : iplugui.h
// Created by   : Mario Ewald, Matthias Juwan
// Description  : Plug-In User Interface
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

#ifndef __iplugui__
#define __iplugui__

#ifndef __funknown__
#include "../base/funknown.h"
#endif

class IPlugView;
class IPlugController;

//------------------------------------------------------------------------
//  ViewRect
//------------------------------------------------------------------------

struct ViewRect
{
//------------------------------------------------------------------------
	long left;
	long top;
	long right;
	long bottom;

	ViewRect (long l = 0, long t = 0, long r = 0, long b = 0)
	: left (l), top (t), right (r), bottom (b) {}
//------------------------------------------------------------------------
};

//------------------------------------------------------------------------
//  IEditorFactory interface declaration
//------------------------------------------------------------------------

class IEditorFactory: public FUnknown
{
public:
//------------------------------------------------------------------------
	virtual tresult PLUGIN_API getEditorSize (const char* name, ViewRect* rect) = 0;
	virtual tresult PLUGIN_API createEditor (const char* name, ViewRect* rect, IPlugView** editor) = 0;
//------------------------------------------------------------------------
	static const FUID iid;
};

DECLARE_CLASS_IID (IEditorFactory, 0xBB0E69FE, 0x9D7242C8, 0xB875F92A, 0xBE66FAAA)

//------------------------------------------------------------------------
//  IUIFactory interface declaration
//------------------------------------------------------------------------

class IUIFactory: public FUnknown
{
public:
//------------------------------------------------------------------------
	virtual tresult PLUGIN_API createController (char* name, IPlugController**) = 0;
	virtual tresult PLUGIN_API getViewDescription (char* name /*in*/, char* templateName, char* xml, long* bufferSize) = 0;
//------------------------------------------------------------------------
	static const FUID iid;
};

DECLARE_CLASS_IID (IUIFactory, 0x0466B783, 0xA08611D5, 0xBECC00A0, 0xC9A636BC)

//------------------------------------------------------------------------
//  IUIFactory2 interface declaration
//------------------------------------------------------------------------

class IUIFactory2: public IUIFactory
{
public:
//------------------------------------------------------------------------
	virtual tresult PLUGIN_API getResource (const char* path, char* buffer, long* bufferSize) = 0;
//------------------------------------------------------------------------
	static const FUID iid;
};

DECLARE_CLASS_IID (IUIFactory2, 0x4B99140E, 0x67E44D4C, 0xA93F8F4F, 0xCEE79F70)

//------------------------------------------------------------------------
//  IViewFactory interface declaration
//------------------------------------------------------------------------

class IViewFactory: public FUnknown
{
public:
//------------------------------------------------------------------------
	virtual tresult PLUGIN_API createView (const char* name, ViewRect* rect, IPlugView** view /*out*/) = 0;
	virtual tresult PLUGIN_API testCondition (const char* condition) = 0;
//------------------------------------------------------------------------
	static const FUID iid;
};

DECLARE_CLASS_IID (IViewFactory, 0x5DB44BA4, 0xF1F741D5, 0xBD3121B9, 0x58BA994B)

//------------------------------------------------------------------------
//  IPlugView interface declaration
//------------------------------------------------------------------------

class IPlugView: public FUnknown
{
public:
//------------------------------------------------------------------------
	virtual tresult PLUGIN_API attached (void* parent) = 0;
	virtual tresult PLUGIN_API removed () = 0;

	virtual tresult PLUGIN_API idle () = 0;

	virtual tresult PLUGIN_API onWheel (float distance) = 0;
	virtual tresult PLUGIN_API onKey (char asciiKey, long keyMsg, long modifiers) = 0;
	virtual tresult PLUGIN_API onSize (ViewRect* newSize) = 0;
//------------------------------------------------------------------------
	static const FUID iid;
};

DECLARE_CLASS_IID (IPlugView, 0xAA3E50FF, 0xB78840EE, 0xADCD48E8, 0x094CEDB7)

#endif
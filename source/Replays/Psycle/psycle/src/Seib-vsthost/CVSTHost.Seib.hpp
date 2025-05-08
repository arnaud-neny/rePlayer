/*****************************************************************************/
/* CVSTHost.hpp: interface for CVSTHost/CEffect classes (for VST SDK 2.4r2). */
/*****************************************************************************/

/*****************************************************************************/
/* Work Derived from the LGPL host "vsthost (1.16m)".						 */
/* (http://www.hermannseib.com/english/vsthost.htm)"						 */
/* vsthost has the following lincense:										 *

Copyright (C) 2006-2007  Hermann Seib

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
******************************************************************************/

#pragma once
//If not using universalis, simulate it
#if !defined DIVERSALIS__OS__MICROSOFT && (defined _WIN64 || defined _WIN32)
	#define DIVERSALIS__OS__MICROSOFT
#elif !defined DIVERSALIS__OS__APPLE && TARGET_API_MAC_CARBON
	#define DIVERSALIS__OS__APPLE
#endif

// steinberg's headers are unable to correctly detect the platform,
// so we need to detect the platform ourselves,
// and declare steinberg's nonstandard/specific options: WIN32/MAC
#if defined DIVERSALIS__OS__MICROSOFT
	#if !defined WIN32
		#define WIN32 // steinberg's build option
	#endif
#elif defined DIVERSALIS__OS__APPLE
	#if !defined MAC
		#define MAC // steinberg's build option
	#endif
#else
	#error "internal steinberg error"
#endif

/// Tell the SDK that we want to support all the VST specs, not only VST2.4
#define VST_FORCE_DEPRECATED 0

// VST header files
#include <vst2.x/aeffectx.h>
#include <vst2.x/vstfxstore.h>
#include "JBridgeEnabler.hpp"
#include "CVSTPreset.hpp"
//////////////////////////////////////////////////////////////////////////

namespace seib {
	namespace vst {
		/*! hostCanDos strings Plug-in -> Host */
		namespace HostCanDos
		{
			extern const char* canDoSendVstEvents;
			extern const char* canDoSendVstMidiEvent;
			extern const char* canDoSendVstTimeInfo;
			extern const char* canDoReceiveVstEvents;
			extern const char* canDoReceiveVstMidiEvent;
			extern const char* DECLARE_VST_DEPRECATED(canDoReceiveVstTimeInfo);
			extern const char* canDoReportConnectionChanges;
			extern const char* canDoAcceptIOChanges;
			extern const char* canDoSizeWindow;
			extern const char* DECLARE_VST_DEPRECATED(canDoAsyncProcessing);
			extern const char* canDoOffline;
			extern const char* DECLARE_VST_DEPRECATED(canDoSupplyIdle);
			extern const char* DECLARE_VST_DEPRECATED(canDoSupportShell);
			extern const char* canDoOpenFileSelector;
			extern const char* canDoCloseFileSelector;
			extern const char* canDoStartStopProcess;
			extern const char* canDoShellCategory;
			extern const char* canDoSendVstMidiEventFlagIsRealtime;
		}

		//-------------------------------------------------------------------------------------------------------
		/*! plugCanDos strings Host -> Plug-in */
		namespace PlugCanDos
		{
			extern const char* canDoSendVstEvents;
			extern const char* canDoSendVstMidiEvent;
			extern const char* canDoReceiveVstEvents;
			extern const char* canDoReceiveVstMidiEvent;
			extern const char* canDoReceiveVstTimeInfo;
			extern const char* canDoOffline;
			extern const char* canDoMidiProgramNames;
			extern const char* canDoBypass;
		}

		class CPatchChunkInfo : public VstPatchChunkInfo
		{
		public:
			CPatchChunkInfo(const CFxProgram& fxstore);
			CPatchChunkInfo(const CFxBank& fxstore);
		};

		//-------------------------------------------------------------------------------------------------------
		// PluginLoader (From VST SDK 2.4 "minihost.cpp")
		//-------------------------------------------------------------------------------------------------------
		typedef AEffect* (*PluginEntryProc) (audioMasterCallback audioMaster);

		class PluginLoader
		{
		public:
			void* module;
			void* sFileName;

			PluginLoader ()
				: module (0)
				, sFileName (0)
			{}

			virtual ~PluginLoader ()
			{
				if(module)
				{
				#if defined DIVERSALIS__OS__MICROSOFT
					FreeLibrary ((HMODULE)module);
				#elif defined DIVERSALIS__OS__APPLE
					CFBundleUnloadExecutable ((CFBundleRef)module);
					CFRelease ((CFBundleRef)module);
				#endif
				}
				if (sFileName)
				{
				#if defined DIVERSALIS__OS__MICROSOFT
					delete[] sFileName;
				#elif defined DIVERSALIS__OS__APPLE
					///\todo:
				#endif
				}
			}

			bool loadLibrary (const char* fileName)
			{
			#if defined DIVERSALIS__OS__MICROSOFT
				module = LoadLibrary (fileName);
			#if defined DEBUG || !defined NDEBUG
				if (module == NULL) {
					LPVOID lpMsgBuf;
					FormatMessage( 
						FORMAT_MESSAGE_ALLOCATE_BUFFER | 
						FORMAT_MESSAGE_FROM_SYSTEM | 
						FORMAT_MESSAGE_IGNORE_INSERTS,
						NULL,
						GetLastError(),
						MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
						(LPTSTR) &lpMsgBuf,
						0,
						NULL 
					);
					// Process any inserts in lpMsgBuf.
					// ...
					// Display the string.
					MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
					// Free the buffer.
					LocalFree( lpMsgBuf );
				}
			#endif // DEBUG

				sFileName = new char[strlen(fileName) + 1];
				if (sFileName)
					strcpy((char*)sFileName, fileName);
			#elif defined DIVERSALIS__OS__APPLE
				CFStringRef fileNameString = CFStringCreateWithCString (NULL, fileName, kCFStringEncodingUTF8);
				if (fileNameString == 0)
					return false;
				CFURLRef url = CFURLCreateWithFileSystemPath (NULL, fileNameString, kCFURLPOSIXPathStyle, false);
				CFRelease(fileNameString);
				if (url == 0)
					return false;
				module = CFBundleCreate(NULL, url);
				CFRelease(url);
				if (module && !CFBundleLoadExecutable((CFBundleRef)module))
					return false;
			#endif
				return module != 0;
			}

			bool loadJBridgeLibrary(const char* fileName)
			{
			#if defined DIVERSALIS__OS__MICROSOFT
				char szProxyPath[MAX_PATH];
				szProxyPath[0]=0 ;
				JBridge::getJBridgeLibrary(szProxyPath, MAX_PATH);
				if ( szProxyPath[0] != '\0' )
				{
					// Load proxy DLL
					module = LoadLibrary(szProxyPath);
					//we need the original name in sFilename.
					sFileName = new char[strlen(fileName) + 1];
					if (sFileName)
						strcpy((char*)sFileName, fileName);
				}
				return module != 0;
			#else
				return false;
			#endif
			}

			bool loadPsycleBridgeLibrary(const char* fileName)
			{
				return false;
			}

			PluginEntryProc getMainEntry ()
			{
				PluginEntryProc mainProc = 0;
				#if defined DIVERSALIS__OS__MICROSOFT
				mainProc = reinterpret_cast<PluginEntryProc>(GetProcAddress ((HMODULE)module, "VSTPluginMain"));
				if (!mainProc)
					mainProc = reinterpret_cast<PluginEntryProc>(GetProcAddress ((HMODULE)module, "main"));
			#elif defined DIVERSALIS__OS__APPLE
				mainProc = (PluginEntryProc)CFBundleGetFunctionPointerForName((CFBundleRef)module, CFSTR("VSTPluginMain"));
				if (!mainProc)
					mainProc = (PluginEntryProc)CFBundleGetFunctionPointerForName((CFBundleRef)module, CFSTR("main_macho"));
			#endif
				return mainProc;
			}

			PFNBRIDGEMAIN getJBridgeMainEntry ()
			{
				PFNBRIDGEMAIN mainProc = 0;
			#if defined DIVERSALIS__OS__MICROSOFT
				mainProc = JBridge::getBridgeMainEntry((HMODULE)module);
			#endif
				return mainProc;
			}
			//-------------------------------------------------------------------------------------------------------
		};
		/*****************************************************************************/
		/* LoadedAEffect:															 */
		/*		Struct definition to ease  CEffect creation/destruction.			 */
		/* Sometimes it might be prefferable to create the CEffect *after* the       */
		/* AEffect has been loaded, in order to have different subclasses for        */
		/* different types. Yet, it might be necessary that the plugin frees the     */
		/* library once it is destroyed. This is why it requires this information	 */
		/*****************************************************************************/
		class CVSTHost;

		typedef struct LoadedAEffect LoadedAEffect;
		struct LoadedAEffect {
			AEffect *aEffect;
			PluginLoader *pluginloader;
		};



		class CEffectWnd;
		class CEffect;
		/*****************************************************************************/
		/* Crashingclass : Helper class for exceptions                               */
		/*                it is used in Psycle's implementation to indicate that the */
		/*                plugin has crashed and disable it.                         */
		/*****************************************************************************/
		class Crashingclass
		{
		public:
			Crashingclass(){};
			void SetEff(CEffect* ef){ this->ef = ef;}
			void crashed(std::exception const & e);
			CEffect* ef;
		};
		/*****************************************************************************/
		/* CEffect : class definition for audio effect objects                       */
		/*****************************************************************************/
		class CEffect
		{
		public:
			// Try to avoid using the AEffect constructor. It acts as a wrapper then, not as an object.
			CEffect(AEffect *effect);
			CEffect(LoadedAEffect &loadstruct);
			virtual ~CEffect();
		protected:
			virtual void Load(LoadedAEffect &loadstruct);
			virtual void Unload();
		protected:
			AEffect *aEffect;
			PluginLoader* ploader;
			void *sDir;
			std::string loadingChunkName;

			CEffectWnd * editorWnd;
			bool bEditOpen;
			bool bNeedIdle;
			bool bNeedEditIdle;
			bool bWantMidi;
			bool bShellPlugin;
			bool bCanBypass;
			bool bMainsState;
		public:
			Crashingclass crashclass;

			// overridables
		public:
			virtual bool LoadBank(CFxBank& fxstore);
			virtual bool LoadProgram(CFxProgram& fxstore);
			virtual CFxBank SaveBank(bool preferchunk=false);
			virtual CFxProgram SaveProgram(bool preferchunk=true);
			virtual void crashed2(std::exception const & e) {};
			virtual std::string GetNameFromSpeakerArrangement(VstSpeakerArrangement& arr, VstInt32 pin) const;
			virtual void WantsMidi(bool enable) { bWantMidi=enable; }
			virtual bool WantsMidi() const { return bWantMidi; }
			virtual void KnowsToBypass(bool enable) { bCanBypass=enable; }
			virtual bool KnowsToBypass() const { return bCanBypass; }
			virtual void NeedsIdle(bool enable) { bNeedIdle=enable; }
			virtual bool NeedsIdle() const { return bNeedIdle; }
			virtual void NeedsEditIdle(bool enable) { bNeedEditIdle=enable; }
			virtual bool NeedsEditIdle() const  { return bNeedEditIdle; }
			virtual void IsShellPlugin(bool enable) { bShellPlugin = enable; }
			virtual bool IsShellPlugin() const { return bShellPlugin; }
			virtual void SetChunkFile(const char * nativePath) { loadingChunkName = nativePath; }
			virtual void SetEditWnd(CEffectWnd* wnd) { editorWnd = wnd; }

			// Overridable AEffect-to-host calls. (you can override them at the host level
			// if your implementation needs that)
			virtual void * OnGetDirectory();
			virtual bool OnGetChunkFile(char * nativePath);
			virtual bool OnSizeEditorWindow(VstInt32 width, VstInt32 height);
			///\todo: You might need to override the OnUpdateDisplay in order to check other changes.
			virtual bool OnUpdateDisplay();
			virtual void * OnOpenWindow(VstWindow* window);
			virtual bool OnCloseWindow(VstWindow* window);
			virtual bool DECLARE_VST_DEPRECATED(IsInputConnected)(VstInt32 input) { return true; } 
			virtual bool DECLARE_VST_DEPRECATED(IsOutputConnected)(VstInt32 input) { return true; }
			// AEffect asks host about its input/outputspeakers.
			virtual VstSpeakerArrangement* OnHostInputSpeakerArrangement() const { return 0; }
			virtual VstSpeakerArrangement* OnHostOutputSpeakerArrangement() const { return 0; }
			// AEffect informs of changed IO. verify numins/outs, speakerarrangement and the likes.
			virtual bool OnIOChanged() { return false; }
			virtual bool OnBeginAutomating(VstInt32 index);
			virtual bool OnEndAutomating(VstInt32 index);
			virtual void OnSetParameterAutomated(VstInt32 index, float value);
			virtual bool OnOpenFileSelector (VstFileSelect *ptr);
			virtual bool OnCloseFileSelector (VstFileSelect *ptr);

			//////////////////////////////////////////////////////////////////////////
			// Following comes the Wrapping of the VST Interface functions.
			virtual void DECLARE_VST_DEPRECATED(Process)(float **inputs, float **outputs, VstInt32 sampleframes) throw(std::runtime_error);
			virtual void ProcessReplacing(float **inputs, float **outputs, VstInt32 sampleframes) throw(std::runtime_error);
			virtual void ProcessDouble (double** inputs, double** outputs, VstInt32 sampleFrames) throw(std::runtime_error);
			virtual void SetParameter(VstInt32 index, float parameter) throw(std::runtime_error);
			virtual float GetParameter(VstInt32 index) throw(std::runtime_error);
		public:
			// Not to be used, except if no other way.
			inline AEffect  *GetAEffect() const{ return aEffect; }
			//////////////////////////////////////////////////////////////////////////
			// AEffect Properties
			// magic is only used in the loader to verify that it is a VST plugin
			//VstInt32 magic()
			inline VstInt32 numPrograms() const	 { return aEffect->numPrograms;	}
			inline VstInt32 numParams() const	 { return aEffect->numParams;	}
			inline VstInt32 numInputs() const	 { return aEffect->numInputs;	}
			inline VstInt32 numOutputs() const	 { return aEffect->numOutputs;	}
			//flags
			inline bool HasEditor() const								{ return aEffect->flags & effFlagsHasEditor;	}
			inline bool DECLARE_VST_DEPRECATED(HasClip)() const			{ return aEffect->flags & effFlagsHasClip;		}
			inline bool DECLARE_VST_DEPRECATED(HasVu)() const			{ return aEffect->flags & effFlagsHasVu;		}
			inline bool DECLARE_VST_DEPRECATED(CanInputMono)() const	{ return aEffect->flags & effFlagsCanMono;		}
			inline bool CanProcessReplace() const						{ return aEffect->flags & effFlagsCanReplacing;	}
			inline bool ProgramIsChunk() const							{ return aEffect->flags & effFlagsProgramChunks;}
			inline bool IsSynth() const									{ return aEffect->flags & effFlagsIsSynth;		}
			inline bool HasNoTail() const								{ return aEffect->flags & effFlagsNoSoundInStop;}
			inline bool DECLARE_VST_DEPRECATED(ExternalAsync)() const	{ return aEffect->flags & effFlagsExtIsAsync;	}
			inline bool DECLARE_VST_DEPRECATED(ExternalBuffer)() const	{ return aEffect->flags & effFlagsExtHasBuffer;	}
			inline bool CanProcessDouble() const						{ return aEffect->flags & effFlagsCanDoubleReplacing;	}

			inline VstInt32 DECLARE_VST_DEPRECATED(RealQualities)() const	{ return aEffect->realQualities;	}
			inline VstInt32 DECLARE_VST_DEPRECATED(OffQualities)() const	{ return aEffect->offQualities;		}
			inline float DECLARE_VST_DEPRECATED(IORatio)() const			{ return aEffect->ioRatio;			}

			// the real plugin ID.
			inline VstInt32 uniqueId() const	{ return aEffect->uniqueID;		}
			// version() is never used (from my experience), in favour of GetVendorVersion(). Yet, it hasn't been deprecated in 2.4.
			inline VstInt32 version() const		{ return aEffect->version;		}
			inline VstInt32 initialDelay() const	{ return aEffect->initialDelay;	}

		protected:
			virtual VstIntPtr Dispatch(VstInt32 opCode, VstInt32 index=0, VstIntPtr value=0, void* ptr=0, float opt=0.) throw(std::runtime_error);
		public:
			//////////////////////////////////////////////////////////////////////////
			// plugin dispatch functions
			inline void Open() { Dispatch(effOpen); }
		protected:
			// Warning! After a "Close()", the "AEffect" is deleted and the plugin cannot be used again. (see audioeffect.cpp)
			// This is why i set it as protected, and calling it from the destructor.
			inline void Close() { Dispatch(effClose); }
		public:
			// sets the index of the program. Zero based.
			inline void SetProgram(VstIntPtr lValue) { if (lValue >= 0 && lValue < numPrograms()) Dispatch(effSetProgram, 0, lValue); }
			// returns the index of the program. Zero based.
			inline VstInt32 GetProgram() const { return const_cast<CEffect*>(this)->Dispatch(effGetProgram); }
			// size of ptr string limited to kVstMaxProgNameLen chars + \0 delimiter.
			inline void SetProgramName(const char *ptr) { Dispatch(effSetProgramName, 0, 0, const_cast<char*>(ptr) ); }
			inline void GetProgramName(char *ptr) const {
				 char s1[kVstMaxProgNameLen*3+1];
				s1[0]='\0';
				const_cast<CEffect*>(this)->Dispatch(effGetProgramName, 0, 0, s1);
				 s1[kVstMaxProgNameLen]='\0';
				 std::strcpy(ptr,s1);
			}
			// Unit of the paramter. size of ptr string limited to kVstMaxParamStrLen char + \0 delimiter
			inline void GetParamLabel(VstInt32 index, char *ptr) const {
				 char s1[kVstMaxProgNameLen*3+1];
				s1[0]='\0';
				const_cast<CEffect*>(this)->Dispatch(effGetParamLabel, index, 0, s1);
				 s1[kVstMaxParamStrLen]='\0';
				 std::strcpy(ptr,s1);
			}
			// Value of the parameter. size of ptr string limited to kVstMaxParamStrLen + \0 delimiter for safety.
			inline void GetParamDisplay(VstInt32 index, char *ptr) const {
				char s1[kVstMaxProgNameLen*3+1];
				s1[0]='\0';
				const_cast<CEffect*>(this)->Dispatch(effGetParamDisplay, index, 0, s1);
				s1[kVstMaxParamStrLen]='\0';
				std::strcpy(ptr,s1);
			}
			// Name of the parameter. size of ptr string limited to kVstMaxParamStrLen char + \0 delimiter.
			// NOTE-NOTE-NOTE-NOTE: Forget about the limit. It's *way* exceeded usually. Use the kVstMaxProgNameLen instead.
			inline void GetParamName(VstInt32 index, char *ptr) const {
				char s1[kVstMaxProgNameLen*3+1];
				s1[0]='\0';
				 const_cast<CEffect*>(this)->Dispatch(effGetParamName, index, 0, s1);
				 s1[kVstMaxProgNameLen]='\0';
				 std::strcpy(ptr,s1);
			}
			// Returns the vu value. Range [0-1] >1 -> clipped
			inline float DECLARE_VST_DEPRECATED(GetVu)() { return Dispatch(effGetVu); }
			inline void SetSampleRate(float fSampleRate,bool ignorestate=false)
			{
				bool reinit=false;
				if (bMainsState && !ignorestate) { reinit=true; MainsChanged(false); }
				Dispatch(effSetSampleRate, 0, 0, 0, fSampleRate);
				if (reinit) MainsChanged(true);
			}
			inline void SetBlockSize(VstIntPtr value,bool ignorestate=false)
			{
				bool reinit=false;
				if (bMainsState && !ignorestate) { reinit=true; MainsChanged(false); }
				Dispatch(effSetBlockSize, 0, value);
				if (reinit) MainsChanged(true);
			}
			inline void MainsChanged(bool bOn)
			{
				if (bOn != bMainsState)
				{
					bMainsState=bOn;
					if (bOn) { Dispatch(effMainsChanged, 0, bOn); StartProcess(); }
					else {  StopProcess(); Dispatch(effMainsChanged, 0, bOn); }
				}
			}
			inline bool EditGetRect(ERect **ptr) { return Dispatch(effEditGetRect, 0, 0, ptr)>0; }
			inline void EditOpen(void *ptr) { Dispatch(effEditOpen, 0, 0, ptr); bEditOpen = true; }
			inline void EditClose() { Dispatch(effEditClose); bEditOpen = false; }
			// This has to be called repeatedly from the idle process ( usually the UI thread, with idle priority )
			// The plugins usually have checks so that it skips the call if no update is required.
			inline void EditIdle() { if(bEditOpen) Dispatch(effEditIdle); bNeedEditIdle=false; }
		#if MAC
			inline void DECLARE_VST_DEPRECATED(EditDraw)(void *rectarea) { Dispatch(effEditDraw, 0, 0, rectarea); }
			inline VstInt32 DECLARE_VST_DEPRECATED(EditMouse)(VstInt32 x, VstIntPtr y) { return Dispatch(effEditMouse, x, y); }
			inline VstInt32 DECLARE_VST_DEPRECATED(EditKey)(VstInt32 value) { return Dispatch(effEditKey, 0, value); }
			inline void DECLARE_VST_DEPRECATED(EditTop)() { Dispatch(effEditTop); }
			inline void DECLARE_VST_DEPRECATED(EditSleep)() { Dispatch(effEditSleep); }
		#endif
			// 2nd check that it is a valid VST. (appart from kEffectMagic )
			inline bool DECLARE_VST_DEPRECATED(Identify)() { return (Dispatch(effIdentify) == CCONST ('N', 'v', 'E', 'f')); }
			// returns "byteSize".
			inline VstInt32 GetChunk(void **ptr, bool onlyCurrentProgram = false) const { return const_cast<CEffect*>(this)->Dispatch(effGetChunk, onlyCurrentProgram, 0, ptr); }
			// return value is not specified in the VST SDK. Don't assume anything.
			inline VstInt32 SetChunk(const void *data, VstInt32 byteSize, bool onlyCurrentProgram = false) { return Dispatch(effSetChunk, onlyCurrentProgram, byteSize, const_cast<void*>(data)); }
		// VST 2.0
			inline VstInt32 ProcessEvents(VstEvents* ptr) { return Dispatch(effProcessEvents, 0, 0, ptr); }
			inline bool CanBeAutomated(VstInt32 index) { return (bool)Dispatch(effCanBeAutomated, index); }
			// A textual description of the parameter's value. A null pointer is used to check the capability (return true).
			inline bool String2Parameter(VstInt32 index, char *text) { return (bool)Dispatch(effString2Parameter, index, 0, text); }
			inline VstInt32 DECLARE_VST_DEPRECATED(GetNumProgramCategories)() { return Dispatch(effGetNumProgramCategories); }
			// text is a string up to kVstMaxProgNameLen chars + \0 delimiter
			inline bool GetProgramNameIndexed(VstInt32 category, VstInt32 index, char* text)
			{
				char s1[kVstMaxProgNameLen*3+1]; // You know, there's always the plugin that doesn't like to follow the limits

				if (!Dispatch(effGetProgramNameIndexed, index, category, s1))
				{
					//Backup current program, since some plugins (ex: mda vocoder) restore the setting when assigning a program.
					VstInt32 cprog= GetProgram();
					float* params = new float[numParams()];
					for(VstInt32 i(0); i<numParams();i++){
						params[i] = GetParameter(i);
					}
					SetProgram(index);
					GetProgramName(text);
					SetProgram(cprog);
					for(VstInt32 i(0); i<numParams();i++){
						SetParameter(i,params[i]);
					}
					delete[] params;
					if (!*text)
						sprintf(text, "Program %d", index);
				}
				else 
				{
					s1[kVstMaxProgNameLen]='\0';
					std::strcpy(text,s1);
				}
				return true;
			}
			// copy current program to the one in index.
			inline bool DECLARE_VST_DEPRECATED(CopyProgram)(VstInt32 index) { return (bool)Dispatch(effCopyProgram, index); }
			//Input index has been (dis-)connected. The application may issue this call when implemented.
			inline void DECLARE_VST_DEPRECATED(ConnectInput)(VstInt32 index, bool state) { Dispatch(effConnectInput, index, state); }
			//Output index has been (dis-)connected. The application may issue this call when implemented.
			inline void DECLARE_VST_DEPRECATED(ConnectOutput)(VstInt32 index, bool state) { Dispatch(effConnectOutput, index, state); }
			inline bool GetInputProperties(VstInt32 index, VstPinProperties *ptr) const { return (bool)const_cast<CEffect*>(this)->Dispatch(effGetInputProperties, index, 0, ptr); }
			inline bool GetOutputProperties(VstInt32 index, VstPinProperties *ptr) const { return (bool)const_cast<CEffect*>(this)->Dispatch(effGetOutputProperties, index, 0, ptr); }
			inline VstPlugCategory GetPlugCategory() const { return (VstPlugCategory)const_cast<CEffect*>(this)->Dispatch(effGetPlugCategory); }
			// get position of dsp buffer. (to verify that it is "on time")
			inline VstInt32 DECLARE_VST_DEPRECATED(GetCurrentPosition)() { return Dispatch(effGetCurrentPosition); }
			// get the address of the dsp buffer.
			inline float* DECLARE_VST_DEPRECATED(GetDestinationBuffer)() { return reinterpret_cast<float*>(Dispatch(effGetDestinationBuffer)); }
			inline bool OfflineNotify(VstAudioFile* ptr, VstInt32 numAudioFiles, bool start) { return (bool)Dispatch(effOfflineNotify, start, numAudioFiles, ptr); }
			inline bool OfflinePrepare(VstOfflineTask *ptr, VstInt32 count) { return (bool)Dispatch(effOfflinePrepare, 0, count, ptr); }
			inline bool OfflineRun(VstOfflineTask *ptr, VstInt32 count) { return (bool)Dispatch(effOfflineRun, 0, count, ptr); }
			/// This function is for Offline processing.
			inline bool ProcessVarIo(VstVariableIo* varIo) { return (bool)Dispatch(effProcessVarIo, 0, 0, varIo); }
			inline bool SetSpeakerArrangement(VstSpeakerArrangement* pluginInput, VstSpeakerArrangement* pluginOutput) { return (bool)Dispatch(effSetSpeakerArrangement, 0, (VstIntPtr)pluginInput, pluginOutput); }
			inline void DECLARE_VST_DEPRECATED(SetBlockSizeAndSampleRate)(VstInt32 blockSize, float sampleRate) { Dispatch(effSetBlockSizeAndSampleRate, 0, blockSize, 0, sampleRate); }
			inline bool SetBypass(bool onOff) { return (bool)Dispatch(effSetBypass, 0, onOff); }
			// ptr is a string up to kVstMaxEffectNameLen chars + \0 delimiter
			inline bool GetEffectName(char *ptr) const { return (bool)const_cast<CEffect*>(this)->Dispatch(effGetEffectName, 0, 0, ptr); }
			// ptr is a string up to 256 chars + \0 delimiter
			inline bool DECLARE_VST_DEPRECATED(GetErrorText)(char *ptr) const { return (bool)const_cast<CEffect*>(this)->Dispatch(effGetErrorText, 0, 0, ptr); }
			// ptr is a string up to kVstMaxVendorStrLen chars + \0 delimiter
			inline bool GetVendorString(char *ptr) const { return (bool)const_cast<CEffect*>(this)->Dispatch(effGetVendorString, 0, 0, ptr); }
			// ptr is a string up to kVstMaxProductStrLen chars + \0 delimiter
			inline bool GetProductString(char *ptr) const { return (bool)const_cast<CEffect*>(this)->Dispatch(effGetProductString, 0, 0, ptr); }
			inline VstInt32 GetVendorVersion() const { return const_cast<CEffect*>(this)->Dispatch(effGetVendorVersion); }
			inline VstInt32 VendorSpecific(VstInt32 index, VstInt32 value, void *ptr, float opt) const { return const_cast<CEffect*>(this)->Dispatch(effVendorSpecific, index, value, ptr, opt); }
			//returns 0 -> don't know, 1 -> yes, -1 -> no. Use the PlugCanDo's strings.
			inline VstInt32 CanDoInt(const char *ptr) const { return const_cast<CEffect*>(this)->Dispatch(effCanDo, 0, 0, (void *)ptr); }
			inline bool CanDo(const char *ptr) const { return (const_cast<CEffect*>(this)->Dispatch(effCanDo, 0, 0, (void *)ptr)>0); }
			inline VstInt32 GetTailSize() { return Dispatch(effGetTailSize); }
			// Recursive Idle() call for plugin. bNeedIdle is set to true when the plugin calls "pHost->OnNeedIdle()"
			inline void DECLARE_VST_DEPRECATED(Idle)() { VstInt32 l=0; if (bNeedIdle) l = Dispatch(effIdle); if (!l) bNeedIdle=false; }
			inline VstInt32 DECLARE_VST_DEPRECATED(GetIcon)() { return Dispatch(effGetIcon); }
			inline VstInt32 DECLARE_VST_DEPRECATED(SetViewPosition)(VstInt32 x, VstInt32 y) { return Dispatch(effSetViewPosition, x, y); }
			inline VstInt32 GetParameterProperties(VstInt32 index, VstParameterProperties* ptr) { return Dispatch(effGetParameterProperties, index, 0, ptr); }
			// Seems something related to MAC ( to be used with editkey )
			inline bool DECLARE_VST_DEPRECATED(KeysRequired)() { return (bool)Dispatch(effKeysRequired); }
			inline VstInt32 GetVstVersion() const { VstInt32 v=const_cast<CEffect*>(this)->Dispatch(effGetVstVersion); if (v > 1000) return v; else return (v)?2000:1000; }
		// VST 2.1 extensions
			// Seems something related to MAC ( to be used with editkey )
			inline VstInt32 KeyDown(VstKeyCode &keyCode) { return Dispatch(effEditKeyDown, keyCode.character, keyCode.virt, 0, keyCode.modifier); }
			// Seems something related to MAC ( to be used with editkey )
			inline VstInt32 KeyUp(VstKeyCode &keyCode) { return Dispatch(effEditKeyUp, keyCode.character, keyCode.virt, 0, keyCode.modifier); }
			inline void SetKnobMode(VstInt32 value) { Dispatch(effSetEditKnobMode, 0, value); }
			inline VstInt32 GetMidiProgramName(VstInt32 channel, MidiProgramName* midiProgramName) { return Dispatch(effGetMidiProgramName, channel, 0, midiProgramName); }
			inline VstInt32 GetCurrentMidiProgram (VstInt32 channel, MidiProgramName* currentProgram) { return Dispatch(effGetCurrentMidiProgram, channel, 0, currentProgram); }
			inline VstInt32 GetMidiProgramCategory (VstInt32 channel, MidiProgramCategory* category) { return Dispatch(effGetMidiProgramCategory, channel, 0, category); }
			inline VstInt32 HasMidiProgramsChanged (VstInt32 channel) { return Dispatch(effHasMidiProgramsChanged, channel); }
			inline VstInt32 GetMidiKeyName(VstInt32 channel, MidiKeyName* keyName) { return Dispatch(effGetMidiKeyName, channel, 0, keyName); }
			inline bool BeginSetProgram() { return (bool)Dispatch(effBeginSetProgram); }
			inline bool EndSetProgram() { return (bool)Dispatch(effEndSetProgram); }
		// VST 2.3 Extensions
			inline bool GetSpeakerArrangement(VstSpeakerArrangement** pluginInput, VstSpeakerArrangement** pluginOutput) const { return (bool)const_cast<CEffect*>(this)->Dispatch(effGetSpeakerArrangement, 0, (VstIntPtr)pluginInput, pluginOutput); }
			//Called in offline (non RealTime) processing before process is called, indicates how many samples will be processed.	Actually returns value.
			inline VstInt32 SetTotalSampleToProcess (VstInt32 value) { return Dispatch(effSetTotalSampleToProcess, 0, value); }
			//Points to a char buffer of size 64, which is to be filled with the name of the plugin including the terminating 0.
			inline VstInt32 GetNextShellPlugin(char *name) { return Dispatch(effShellGetNextPlugin, 0, 0, name); }
			//Called one time before the start of process call.
			inline VstInt32 StartProcess() { return Dispatch(effStartProcess); }
			inline VstInt32 StopProcess() { return Dispatch(effStopProcess); }
			inline bool SetPanLaw(VstInt32 type, float val) { return (bool)Dispatch(effSetPanLaw, 0, type, 0, val); }
			//	0 : 	Not implemented.
			//	1 : 	The bank/program can be loaded.
			//-1  : 	The bank/program can't be loaded.
			inline VstInt32 BeginLoadBank(VstPatchChunkInfo* ptr) { return Dispatch(effBeginLoadBank, 0, 0, ptr); }
			inline VstInt32 BeginLoadProgram(VstPatchChunkInfo* ptr) { return Dispatch(effBeginLoadProgram, 0, 0, ptr); }
		// VST 2.4 Extensions
			inline bool SetProcessPrecision(VstProcessPrecision precision)	{ return Dispatch(effSetProcessPrecision,0,precision,0,0); }
			inline VstInt32	GetNumMidiInputChannels() { return Dispatch(effGetNumMidiInputChannels,0,0,0,0); }
			inline VstInt32	GetNumMidiOutputChannels() { return Dispatch(effGetNumMidiOutputChannels,0,0,0,0); }
		};

		/*****************************************************************************/
		/* CVSTHost class declaration                                                */
		/*****************************************************************************/

		class CVSTHost
		{
		public:
			CVSTHost();
			virtual ~CVSTHost();

		public:
			static CVSTHost * pHost;
			static VstTimeInfo vstTimeInfo;

		protected:
			VstInt32 lBlockSize;
			static VstInt32 quantization;
			static bool useJBridge;
			static bool usePsycleVstBridge;
			bool loadingEffect;
			VstInt32 loadingShellId;
			bool isShell;

		public:
			CEffect* LoadPlugin(const char * sName,VstInt32 shellIdx=0);
			static VstIntPtr VSTCALLBACK AudioMasterCallback (AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt);

			static void UseJBridge(bool use) { useJBridge=use; }
			static bool UseJBridge() { return useJBridge; }
			static void UsePsycleVstBridge(bool use) { usePsycleVstBridge=use; }
			static bool UsePsycleVstBridge() { return usePsycleVstBridge; }

			// overridable functions
		protected:
			virtual CEffect * CreateEffect(LoadedAEffect &loadstruct) { return new CEffect(loadstruct); }
			virtual CEffect * CreateWrapper(AEffect *effect) { return new CEffect(effect); }
			// The base class function gives kVstPpqPosValid, kVstBarsValid, kVstClockValid, kVstSmpteValid, and kVstNanosValid
			// Ensure that samplePos, sampleRate, tempo, and timesigNumerator/Denominator are correct before calling it.
			virtual void CalcTimeInfo(VstInt32 lMask = -1);
		public:
			virtual void Log(std::string message) {};
			virtual void SetSampleRate(float fSampleRate=44100.);
			virtual void SetBlockSize(VstInt32 lSize=1024);
			virtual void SetTimeSignature(VstInt32 numerator, VstInt32 denominator);
			virtual void SetCycleActive(double cycleStart, double cycleEnd);
			virtual float GetSampleRate() const { return vstTimeInfo.sampleRate; }
			virtual VstInt32 GetBlockSize() const { return lBlockSize; }

			// text is a string up to kVstMaxVendorStrLen chars + \0 delimiter
			virtual bool OnGetVendorString(char *text) const { strcpy(text, "Seib-Psycledelics"); return true; }
			// text is a string up to kVstMaxProductStrLen chars + \0 delimiter
			virtual bool OnGetProductString(char *text) const { strcpy(text, "Default CVSTHost."); return true; }
			virtual VstInt32 OnGetHostVendorVersion() const { return 1612; } // 1.16l
			virtual VstInt32 OnHostVendorSpecific(CEffect &pEffect, VstInt32 lArg1, VstInt32 lArg2, void* ptrArg, float floatArg) const { return 0; }
			virtual VstInt32 OnGetVSTVersion() const { return kVstVersion; }

			// Plugin calls this function when it has changed one parameter (from the GUI)in order for the host to record it.
			virtual void OnSetParameterAutomated(CEffect &pEffect, VstInt32 index, float value) { pEffect.OnSetParameterAutomated(index,value); }
			// onCurrentId is used normally when loading Shell type plugins. This is taken care in the AudioMaster callback.
			// the function here is called in any other cases.
			virtual VstInt32 OnCurrentId(CEffect &pEffect) const { return pEffect.uniqueId(); }
			// This callback forces an inmediate "EditIdle()" call for the plugins.
			virtual void OnIdle(CEffect &pEffect);
			virtual bool DECLARE_VST_DEPRECATED(OnInputConnected)(CEffect &pEffect, VstInt32 input) const { return pEffect.IsInputConnected(input); }
			virtual bool DECLARE_VST_DEPRECATED(OnOutputConnected)(CEffect &pEffect, VstInt32 output) const { return pEffect.IsOutputConnected(output); }
			virtual void DECLARE_VST_DEPRECATED(OnWantEvents)(CEffect &pEffect, VstInt32 filter);
			//\todo : investigate if the "flags" of vstTimeInfo should be per-plugin, instead of per-host.
			virtual VstTimeInfo *OnGetTime(CEffect &pEffect, VstInt32 lMask) { CalcTimeInfo(lMask); return &vstTimeInfo; }
			virtual bool OnProcessEvents(CEffect &pEffect, VstEvents* events) { return false; }
			// aeffectx.hpp: "VstTimenfo* in <ptr>, filter in <value>, not supported". Not Implemented in the VST SDK.
			virtual bool DECLARE_VST_DEPRECATED(OnSetTime)(CEffect &pEffect, VstInt32 filter, VstTimeInfo *timeInfo) { return false; }
			//  pos in Sample frames, return bpm* 10000
			virtual VstInt32 DECLARE_VST_DEPRECATED(OnTempoAt)(CEffect &pEffect, VstInt32 pos) const { return 0; }
			virtual VstInt32 DECLARE_VST_DEPRECATED(OnGetNumAutomatableParameters)(CEffect &pEffect) const { return 0; }
			//	0 :  	Not implemented.
			//	1 : 	Full single float precision is maintained in automation.
			//other : 	The integer value that represents +1.0.
			virtual VstInt32 DECLARE_VST_DEPRECATED(OnGetParameterQuantization)(CEffect &pEffect) const { return quantization; }
			// Tell host numInputs and/or numOutputs and/or initialDelay has changed.
			// The host could call a suspend (if the plugin was enabled (in resume state)) and then ask for getSpeakerArrangement
			// and/or check the numInputs and numOutputs and initialDelay and then call a resume.
			virtual bool OnIoChanged(CEffect &pEffect) { return pEffect.OnIOChanged(); }
			// This function is called from a plugin when it needs one (or more)call(s) to CEffect::Idle().
			// You should have a continuous call to CEffect::Idle() on your Idle Call.
			// CVSTHost::OnNeedIdle() and CEffect::Idle() take care of calling only when needed.
			virtual bool DECLARE_VST_DEPRECATED(OnNeedIdle)(CEffect &pEffect);
			virtual bool OnSizeWindow(CEffect &pEffect, VstInt32 width, VstInt32 height) { return pEffect.OnSizeEditorWindow(width, height); }
			// Will cause application to call AudioEffect's  setSampleRate/setBlockSize method (when implemented).
			virtual VstInt32 OnUpdateSampleRate(CEffect &pEffect){
				if (!loadingEffect) pEffect.SetSampleRate(vstTimeInfo.sampleRate,true);
				return vstTimeInfo.sampleRate; }
			virtual VstInt32 OnUpdateBlockSize(CEffect &pEffect) {
				pEffect.SetSampleRate(vstTimeInfo.sampleRate,true);
				return lBlockSize; }
			//	Returns the ASIO input latency values.
			virtual VstInt32 OnGetInputLatency(CEffect &pEffect) const { return 0; }
			// Returns the ASIO output latency values. To be used mostly for GUI sync with audio.
			virtual VstInt32 OnGetOutputLatency(CEffect &pEffect) const { return 0; }
			virtual CEffect *DECLARE_VST_DEPRECATED(GetPreviousPlugIn)(CEffect &pEffect,VstInt32 pinIndex) const;
			virtual CEffect *DECLARE_VST_DEPRECATED(GetNextPlugIn)(CEffect &pEffect, VstInt32 pinIndex) const;
			// asks the host if it will use this plugin with "processReplacing"
			virtual bool DECLARE_VST_DEPRECATED(OnWillProcessReplacing)(CEffect &pEffect) const { return false; }
			//	0 :  	Not supported.
			//	1 : 	Currently in user thread (gui).
			//	2 : 	Currently in audio thread or irq (where process is called).
			//	3 : 	Currently in 'sequencer' thread or irq (midi, timer etc).
			//	4 : 	Currently offline processing and thus in user thread.
			//other : 	Not defined, but probably pre-empting user thread.
			virtual VstInt32 OnGetCurrentProcessLevel(CEffect &pEffect) const { return 0; }
			//	kVstAutomationUnsupported 	not supported by Host
			//	kVstAutomationOff 	off
			//	kVstAutomationRead 	read
			//	kVstAutomationWrite 	write
			//	kVstAutomationReadWrite 	read and write
			virtual VstInt32 OnGetAutomationState(CEffect &pEffect) const { return kVstAutomationUnsupported; }
			// As already seen, a single VstOfflineTask structure can be used both to read an existing file, and to overwrite it.
			// Moreover, the offline specification states that it is possible, at any time, to read both the original samples
			// and the new ones (the "overwritten" samples). This is the reason for the readSource parameter:
			// set it to true to read the original samples and to false to read the recently written samples.
			virtual bool OnOfflineRead(CEffect &pEffect, VstOfflineTask* offline, VstOfflineOption option, bool readSource = true) { return false; }
			virtual bool OnOfflineWrite(CEffect &pEffect, VstOfflineTask* offline, VstOfflineOption option) { return false; }
			// The parameter numNewAudioFiles is the number of files that the Plug-In want to create. 
			virtual bool OnOfflineStart(CEffect &pEffect, VstAudioFile* audioFiles, VstInt32 numAudioFiles, VstInt32 numNewAudioFiles) { return false; }
			virtual VstInt32 OnOfflineGetCurrentPass(CEffect &pEffect) { return 0; }
			virtual VstInt32 OnOfflineGetCurrentMetaPass(CEffect &pEffect) { return 0; }
			// Used for variable I/O processing. ( offline processing )
			virtual void DECLARE_VST_DEPRECATED(OnSetOutputSampleRate)(CEffect &pEffect, float sampleRate) { return; }
			virtual VstSpeakerArrangement* DECLARE_VST_DEPRECATED(OnGetOutputSpeakerArrangement)(CEffect &pEffect) const { return pEffect.OnHostOutputSpeakerArrangement(); }
			// Specification says 0 -> don't know, 1 ->yes, -1 : no, but audioeffectx.cpp says "!= 0 -> true", and since plugins use audioeffectx...
			virtual bool OnCanDo(CEffect &pEffect,const char *ptr) const;
			virtual VstInt32 OnGetHostLanguage() const { return kVstLangEnglish; }
			virtual void * DECLARE_VST_DEPRECATED(OnOpenWindow)(CEffect &pEffect, VstWindow* window) { return pEffect.OnOpenWindow(window); }
			virtual bool DECLARE_VST_DEPRECATED(OnCloseWindow)(CEffect &pEffect, VstWindow* window) { return pEffect.OnCloseWindow(window); }
			virtual void * OnGetDirectory(CEffect &pEffect) { return pEffect.OnGetDirectory(); }
			//\todo: "Something has changed, update 'multi-fx' display." ???
			virtual bool OnUpdateDisplay(CEffect &pEffect) { return pEffect.OnUpdateDisplay(); }
			// VST 2.1 Extensions
			// Notifies that "OnSetParameterAutomated" is going to be called. (once per mouse clic)
			virtual bool OnBeginEdit(CEffect &pEffect,VstInt32 index) { return pEffect.OnBeginAutomating(index); }
			virtual bool OnEndEdit(CEffect &pEffect,VstInt32 index) { return pEffect.OnEndAutomating(index); }
			virtual bool OnOpenFileSelector (CEffect &pEffect, VstFileSelect *ptr) { return pEffect.OnOpenFileSelector(ptr); }
			// VST 2.2 Extensions
			virtual bool OnCloseFileSelector (CEffect &pEffect, VstFileSelect *ptr) { return pEffect.OnCloseFileSelector(ptr); }
			// open an editor for audio (defined by XML text in ptr)
			virtual bool DECLARE_VST_DEPRECATED(OnEditFile)(CEffect &pEffect, char *ptr) { return false; }
			virtual bool DECLARE_VST_DEPRECATED(OnGetChunkFile)(CEffect &pEffect, void * nativePath) { return pEffect.OnGetChunkFile(static_cast<char*>(nativePath)); }
			// VST 2.3 Extensions
			virtual VstSpeakerArrangement *DECLARE_VST_DEPRECATED(OnGetInputSpeakerArrangement)(CEffect &pEffect) const { return pEffect.OnHostInputSpeakerArrangement(); }
		};
	}
}

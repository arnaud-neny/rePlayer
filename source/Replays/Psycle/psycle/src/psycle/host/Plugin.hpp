///\file
///\brief interface file for psycle::host::Plugin
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "Machine.hpp"
#include <psycle/plugin_interface.hpp>
#include "Configuration.hpp"
#include "Player.hpp"
#include "Global.hpp"
#include <diversalis/compiler.hpp>
#include <universalis/stdlib/cstdint.hpp>
namespace psycle
{
	namespace host
	{
		extern const char* MIDI_CHAN_NAMES[16];

		/// calls that the plugin side can make to the host side
		class PluginFxCallback : public psycle::plugin_interface::CFxCallback
		{
			public:
				inline virtual void MessBox(char const* ptxt,char const* caption,unsigned int type) const { MessageBox(hWnd,ptxt,caption,type); }
				inline virtual int GetTickLength() const { return Global::player().SamplesPerRow(); }
				inline virtual int GetSamplingRate() const { return Global::player().SampleRate(); }
				inline virtual int GetBPM() const { return Global::player().bpm; }
				inline virtual int GetTPB() const { return Global::player().lpb; }
				virtual int CallbackFunc(int /*cbkID*/, int /*par1*/, int /*par2*/, void* /*par3*/);
				virtual bool FileBox(bool openMode, char* filter, char inoutName[]);
				/// unused slot kept for binary compatibility for (old) closed-source plugins on msvc++ on mswindows.
				inline virtual float * unused0(int, int) { return NULL;};
				/// unused slot kept for binary compatibility for (old) closed-source plugins on msvc++ on mswindows.
				inline virtual float * unused1(int, int) { return NULL;};

			public: ///\todo private:
				HWND hWnd;
		};

		class Plugin; // forward declaration

		/// Proxy between the host and a plugin.
		class proxy
		{
			public:
				proxy(Plugin & host, psycle::plugin_interface::CMachineInterface * plugin = 0) : host_(host), plugin_(0) { (*this)(plugin); }
				~proxy() throw() { (*this)(0); }

			///\name reference to the host side
			///\{
				private:
					Plugin const & host() const throw() { return host_; }
					Plugin       & host()       throw() { return host_; }
				private:
					Plugin       & host_;

			///\}

			///\name reference to the plugin side
			///\{
				private:
					psycle::plugin_interface::CMachineInterface const & plugin() const throw() { return *plugin_; }
					psycle::plugin_interface::CMachineInterface       & plugin()       throw() { return *plugin_; }
				private:
					psycle::plugin_interface::CMachineInterface       * plugin_;

				public:
					bool const operator()() const throw() { return plugin_; }
					inline void operator()(psycle::plugin_interface::CMachineInterface * plugin) throw(exceptions::function_error);
			///\}

			///\name protected calls from the host side to the plugin side
			///\{
				public:
					void inline callback()                                                                     throw(exceptions::function_error);
					void inline Init()                                                                         throw(exceptions::function_error);
					void inline SequencerTick()                                                                throw(exceptions::function_error);
					void inline ParameterTweak(int par, int val)                                               throw(exceptions::function_error);
					void inline Work(float * psamplesleft, float * psamplesright , int numsamples, int tracks) throw(exceptions::function_error);
					void inline Stop()                                                                         throw(exceptions::function_error);
					void inline PutData(void * pData)                                                          throw(exceptions::function_error);
					void inline GetData(void * pData)                                                          throw(exceptions::function_error);
					int  inline GetDataSize()                                                                  throw(exceptions::function_error);
					void inline Command()                                                                      throw(exceptions::function_error);
					void inline unused0(const int i)                                                           throw(exceptions::function_error);
					bool inline unused1(const int i)                                                           throw(exceptions::function_error);
					void inline MidiEvent(const int channel, const int value, const int velocity)              throw(exceptions::function_error);
					void inline unused2(uint32_t const data)                                              throw(exceptions::function_error);
					bool inline DescribeValue(char * txt, const int param, const int value)                    throw(exceptions::function_error);
					bool inline HostEvent(const int wave, const int note, const float volume)                  throw(exceptions::function_error);
					void inline SeqTick(int channel, int note, int ins, int cmd, int val)                      throw(exceptions::function_error);
					void inline unused3 ()                                                                     throw(exceptions::function_error);
					int  inline Val(int parameter)                                                             throw(exceptions::function_error);

					int const /* at least it's const! */ * Vals() { return plugin().Vals; }
			///\}
		};

		/// the host side
		class Plugin : public Machine
		{
			///\name the complicated quadristepped (!) construction
			///\{
				public:
					///\todo doc
					Plugin(int index);
					///\todo doc
					bool LoadDll(std::string psFileName);
					///\todo doc
					void Instance(std::string file_name) throw(...);
					///\todo doc
					virtual void Init();
				private:
					///\todo mswindows! humpf! mswindows!
					HINSTANCE _dll;
			///\}

			///\name two-stepped destruction
			///\{
				public:
					///\todo doc
					void Free() throw(...);
					///\todo doc
					virtual ~Plugin() throw();
			///\}

			///\name general info
			///\{
				///\todo was std::string in v1.9
				public:  virtual const char * const GetDllName() const throw() { return _psDllName.c_str(); }
				private: std::string                _psDllName;

				///\todo this was called GetBrand in in v1.9
				public:  virtual const char* const GetName(void) const { return _psName.c_str(); }
				private: std::string    _psName;

				///\todo there was no ShortName in v1.9
				public:  std::string GetShortName() throw() { return _psShortName; }
				private: char        _psShortName[16];

				///\todo this was called GetVendorName in v1.9
				public:  std::string GetAuthor() throw() { return _psAuthor; }
				private: std::string _psAuthor;

				public:  bool const & IsSynth() const throw() { return _isSynth; }
				private: bool        _isSynth;

				public: virtual bool NeedsAuxColumn() { return needsAux_; }
				private: bool		needsAux_;

				//\todo: Dynamic
				public:
				virtual const char* AuxColumnName(int idx) const { 
					/*if(needsAux_) {

					}
					else*/ {
						return MIDI_CHAN_NAMES[idx];
					}
				}
				virtual int NumAuxColumnIndexes() { return 16;}

				virtual const uint32_t GetAPIVersion() { try {return GetInfo()->APIVersion; }catch(...){return 0;} }
				virtual const uint32_t GetPlugVersion() { try { return GetInfo()->PlugVersion; }catch(...){return 0;}}

			///\}

			///\name parameter info
			///\{
				public:///\todo private: move this to the proxy class
					inline psycle::plugin_interface::CMachineInfo * GetInfo() throw() { return _pInfo; }
				private:
					///\todo move this to the proxy class
					psycle::plugin_interface::CMachineInfo * _pInfo;
			///\}

			///\name calls to the plugin side go thru the proxy
			///\{
				/// calls to the plugin side go thru the proxy
				public:  host::proxy & proxy() throw() { return proxy_; }
				private: host::proxy   proxy_;
			///\}

			///\name calls from the plugin side go thru the PluginFxCallback
			///\{
				private:
					static PluginFxCallback _callback;
				public:
					inline static PluginFxCallback * GetCallback() throw() { return &_callback; }
			///\}

			///\name signal/event processing
			///\{
				public:
					virtual int GenerateAudioInTicks( int startSample, int numSamples );
					virtual float GetAudioRange() const { return 32768.0f; }
					virtual void SetSampleRate(int sr);
					virtual void NewLine();
					virtual void Tick(int channel, PatternEntry * pEntry);
					virtual void Stop();
			///\}

			///\name parameter tweaking
			///\{
				public:
					virtual int  GetNumCols   () { return GetInfo()->numCols; }
					virtual int  GetNumParams () { return GetInfo()->numParameters; }
					virtual int GetParamType(int numparam);
					virtual void GetParamName (int numparam, char * name);
					virtual void GetParamRange(int numparam, int & minval,int & maxval);
					virtual int  GetParamValue(int numparam);
					virtual void GetParamValue(int numparam, char * parval);
					virtual bool SetParameter (int numparam, int value);
					virtual void SetCurrentProgram(int idx) {};
					virtual int GetCurrentProgram() {return 0;};
					virtual void GetCurrentProgramName(char* val) {strcpy(val,"Program 0");};
					virtual void GetIndexProgramName(int bnkidx, int prgIdx, char* val){strcpy(val,"Program 0");};
					virtual int GetNumPrograms(){ return 1;};
					virtual int GetTotalPrograms(){ return 1;};
					virtual void SetCurrentBank(int idx) {};
					virtual int GetCurrentBank() {return 0;};
					virtual void GetCurrentBankName(char* val) {strcpy(val,"Internal");};
					virtual void GetIndexBankName(int bnkidx, char* val){strcpy(val,"Internal");};
					virtual int GetNumBanks(){ return 1;};
					virtual void Tweak(CPreset const & preset);
					virtual void GetCurrentPreset(CPreset & preset);
			///\}

			///\name (de)serialization
			///\{
				public:
					virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
					virtual void SaveSpecificChunk(RiffFile * pFile);
					virtual bool Load(RiffFile * pFile); //Old fileformat
					void SkipLoad(RiffFile* pFile); //Old fileformat
			///\}
		};
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// inline implementations. we need to define body of inlined function after the class definition because of dependencies

namespace psycle
{
	namespace host
	{
		inline void proxy::operator()(psycle::plugin_interface::CMachineInterface * plugin) throw(exceptions::function_error)
		{
			delete this->plugin_; this->plugin_ = plugin;
			//if((*this)())
			if(plugin)
			{
				callback();
				//Init(); // [bohan] i can't call that here. It would be best, some other parts of psycle want to call it to. We need to get rid of the other calls.
			}
		}

		/*#if defined DIVERSALIS__COMPILER__MICROSOFT
			#pragma warning(push)
			#pragma warning(disable:4702) // unreachable code
		#endif*/

		void inline proxy:: Init()                                                                         throw(exceptions::function_error) { try { plugin().Init();                                                        } CATCH_WRAP_AND_RETHROW(host()) }
		void inline proxy:: SequencerTick()                                                                throw(exceptions::function_error) { try { plugin().SequencerTick();                                               } CATCH_WRAP_AND_RETHROW(host()) }
		void inline proxy:: ParameterTweak(int par, int val)                                               throw(exceptions::function_error) { try { plugin().ParameterTweak(par, val);                                      } CATCH_WRAP_AND_RETHROW(host()) }
		void inline proxy:: Work(float * psamplesleft, float * psamplesright , int numsamples, int tracks) throw(exceptions::function_error) { try { plugin().Work(psamplesleft, psamplesright, numsamples, tracks);         } CATCH_WRAP_AND_RETHROW(host()) }
		void inline proxy:: Stop()                                                                         throw(exceptions::function_error) { try { plugin().Stop();                                                        } CATCH_WRAP_AND_RETHROW(host()) }
		void inline proxy:: PutData(void * pData)                                                          throw(exceptions::function_error) { try { plugin().PutData(pData);                                                } CATCH_WRAP_AND_RETHROW(host()) }
		void inline proxy:: GetData(void * pData)                                                          throw(exceptions::function_error) { try { plugin().GetData(pData);                                                } CATCH_WRAP_AND_RETHROW(host()) }
		int  inline proxy:: GetDataSize()                                                                  throw(exceptions::function_error) { try { return plugin().GetDataSize();                                          } CATCH_WRAP_AND_RETHROW_WITH_FAKE_RETURN(host()) }
		void inline proxy:: Command()                                                                      throw(exceptions::function_error) { try { plugin().Command();                                                     } CATCH_WRAP_AND_RETHROW(host()) }
		void inline proxy:: unused0(const int i)                                                           throw(exceptions::function_error) { try { plugin().unused0(i);                                                    } CATCH_WRAP_AND_RETHROW(host()) }
		bool inline proxy:: unused1(const int i)                                                           throw(exceptions::function_error) { try { return const_cast<const psycle::plugin_interface::CMachineInterface &>(plugin()).unused1(i);      } CATCH_WRAP_AND_RETHROW_WITH_FAKE_RETURN(host()) }
		void inline proxy:: MidiEvent(const int channel, const int value, const int velocity)              throw(exceptions::function_error) { try { plugin().MidiEvent(channel, value, velocity);                           } CATCH_WRAP_AND_RETHROW(host()) }
		void inline proxy:: unused2(uint32_t const data)                                              throw(exceptions::function_error) { try { plugin().unused2(data);                                                 } CATCH_WRAP_AND_RETHROW(host()) }
		bool inline proxy:: DescribeValue(char * txt, const int param, const int value)                    throw(exceptions::function_error) { try { return plugin().DescribeValue(txt, param, value);                       } CATCH_WRAP_AND_RETHROW_WITH_FAKE_RETURN(host()) }
		bool inline proxy:: HostEvent(const int eventNr, const int val1, const float val2)                 throw(exceptions::function_error) { try { return plugin().HostEvent(eventNr, val1, val2);                         } CATCH_WRAP_AND_RETHROW_WITH_FAKE_RETURN(host()) }
		void inline proxy:: SeqTick(int channel, int note, int ins, int cmd, int val)                      throw(exceptions::function_error) { try { plugin().SeqTick(channel, note, ins, cmd, val);                         } CATCH_WRAP_AND_RETHROW(host()) }
		void inline proxy:: unused3()                                                                      throw(exceptions::function_error) { try { plugin().unused3();                                                     } CATCH_WRAP_AND_RETHROW(host()) }
		int  inline proxy:: Val(int parameter)                                                             throw(exceptions::function_error) { try { return plugin().Vals[parameter];                                        } CATCH_WRAP_AND_RETHROW_WITH_FAKE_RETURN(host()) }
		void inline proxy:: callback()                                                                     throw(exceptions::function_error) { try { plugin().pCB = host().GetCallback();                                    } CATCH_WRAP_AND_RETHROW(host()) }

	/*	#if defined DIVERSALIS__COMPILER__MICROSOFT
			#pragma warning(pop)
		#endif*/
	}
}

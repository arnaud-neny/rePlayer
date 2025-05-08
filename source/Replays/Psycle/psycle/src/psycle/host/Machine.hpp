///\file
///\brief interface file for psycle::host::Machine
#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"
#include "FileIO.hpp"
#include "Preset.hpp"
#include "cpu_time_clock.hpp"
#include <psycle/helpers/dsp.hpp>
#include <universalis/exception.hpp>
#include <universalis/compiler/location.hpp>
#include <universalis/stdlib/chrono.hpp>
#include <universalis/os/loggers.hpp>
#include <stdexcept>
#include <vector>

namespace psycle
{
	namespace host
	{
		class Machine; // forward declaration
		class RiffFile; // forward declaration

		/// Base class for exceptions thrown from plugins.
		class exception : public std::runtime_error
		{
			public:
				exception(std::string const & what) : std::runtime_error(what) {}
		};

		/// Classes derived from exception.
		namespace exceptions
		{
			/// Base class for exceptions caused by errors on library operation.
			class library_error : public exception
			{
				public:
					library_error(std::string const & what) : exception(what) {}
			};

			/// Classes derived from library.
			namespace library_errors
			{
				/// Exception caused by library loading failure.
				class loading_error : public library_error
				{
					public:
						loading_error(std::string const & what) : library_error(what) {}
				};

				/// Exception caused by symbol resolving failure in a library.
				class symbol_resolving_error : public library_error
				{
					public:
						symbol_resolving_error(std::string const & what) : library_error(what) {}
				};
			}

			/// Base class for exceptions caused by an error in a library function.
			class function_error : public exception
			{
				public:
					function_error(std::string const & what, std::exception const * const exception = 0) : host::exception(what), exception_(exception) {}
				public:
					std::exception const inline * const exception() const throw() { return exception_; }
				private:
					std::exception const * const        exception_;
			};
			
			///\relates function_error.
			namespace function_errors
			{
				/// Exception caused by a bad returned value from a library function.
				class bad_returned_value : public function_error
				{
					public:
						bad_returned_value(std::string const & what) : function_error(what) {}
				};

				///\internal
				namespace detail
				{
					/// Crashable type concept requirement: it must have a member function void crashed(std::exception const &) throw();
					template<typename Crashable>
					class rethrow_functor
					{
						public:
							rethrow_functor(Crashable & crashable, bool do_rethrow) : crashable_(crashable), do_rethrow_(do_rethrow) {}
							template<typename E> void operator_                (universalis::compiler::location const & location,              E const * const e = 0) const throw(function_error) { rethrow(location, e, 0); }
							template<          > void operator_<std::exception>(universalis::compiler::location const & location, std::exception const * const e    ) const throw(function_error) { rethrow(location, e, e); }
						private:
							template<typename E> void rethrow                  (universalis::compiler::location const & location,              E const * const e, std::exception const * const standard) const throw(function_error)
							{
								std::ostringstream s;
								s
									<< "An exception occured in"
									<< " module: " << location.module()
									<< ", function: " << location.function()
									<< ", file: " << location.file() << ':' << location.line()
									<< '\n';
								if(e) {
									s
										<< "exception type: " << universalis::compiler::typenameof(*e) << '\n'
										<< universalis::exceptions::string(*e);
								} else {
									s << universalis::compiler::exceptions::ellipsis_desc();
								}
								function_error const f_error(s.str(), standard);
								crashable_.crashed(f_error);
								if (do_rethrow_) { throw f_error; }
							}
							Crashable & crashable_;
							bool do_rethrow_;
					};

					template<typename Crashable>
					rethrow_functor<Crashable> make_rethrow_functor(Crashable & crashable, bool do_rethrow=true)
					{
						return rethrow_functor<Crashable>(crashable, do_rethrow);
					}
				}

				/// This macro is to be used in place of a catch statement
				/// to catch an exception of any type thrown by a machine.
				/// It performs the following operations:
				/// - It catches everything.
				/// - It converts the exception to a std::exception (if needed).
				/// - It marks the machine as crashed by calling the machine's member function void crashed(std::exception const &) throw();
				/// - It throws the converted exception.
				/// The usage is:
				/// - for the proxy between the host and a machine:
				///     try { some_machine.do_something(); } CATCH_WRAP_AND_RETHROW(some_machine)
				/// - for the host:
				///     try { machine_proxy.do_something(); } catch(std::exception) { /* don't rethrow the exception */ }
				///
				/// Note that the crashable argument can be of any type as long as it has a member function void crashed(std::exception const &) throw();
				#define CATCH_WRAP_AND_RETHROW(crashable) \
					UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW__WITH_FUNCTOR(psycle::host::exceptions::function_errors::detail::make_rethrow_functor(crashable))

				///\see CATCH_WRAP_AND_RETHROW
				#define CATCH_WRAP_STATIC(crashable) \
					UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW__WITH_FUNCTOR__NO_CLASS(psycle::host::exceptions::function_errors::detail::make_rethrow_functor(crashable, false))

				//This is to avoid a warning when using on methods that return a value.
				#define CATCH_WRAP_AND_RETHROW_WITH_FAKE_RETURN(crashable) \
					UNIVERSALIS__EXCEPTIONS__CATCH_ALL_AND_CONVERT_TO_STANDARD_AND_RETHROW__WITH_FUNCTOR(psycle::host::exceptions::function_errors::detail::make_rethrow_functor(crashable)) \
					return 0;
			}
		}

		/// Class storing the parameter description of Internal Machines.
		class CIntMachParam			
		{
		public:
			/// Short name
			const char * name;		
			/// >= 0
			int minValue;
			/// <= 65535
			int maxValue;
		};

		class ParamTranslator
		{
		public:
			typedef std::vector<int> TranslateContainer;

			ParamTranslator() {
				translate_container_.reserve(256);
				for (int i = 0; i < 256; ++i) {
					translate_container_.push_back(i);
				}
			}

			inline void set_virtual_index(int virtual_index, int machine_index) {
				translate_container_[virtual_index] = machine_index;
			}
			inline int translate(int virtual_index) const {
				if (virtual_index >=0 && virtual_index < 256) {
					return translate_container_[virtual_index]; 
				} else {
					throw std::runtime_error("Plugin Index Out of Range");
				}
			}
			inline int virtual_index(int machine_index) const {
				TranslateContainer::const_iterator it =
					std::find(translate_container_.begin(), translate_container_.end(), machine_index);
				if (it != translate_container_.end()) {
					return std::distance(translate_container_.begin(), it);
				} else {
					throw std::runtime_error("Plugin Index Out of Range");
				}
			}
    
		private:
			TranslateContainer translate_container_;
		};

		class Wire
		{
		public:
			//first = srcpin, second = dstpin.
			typedef std::pair<short,short> PinConnection;
			typedef std::vector<PinConnection> Mapping;
			Wire(Machine * const dstMac);
			Wire(const Wire &wire);
			void ConnectSource(Machine & srcMac, int outType, int outWire, const Mapping * const mapping = NULL);
			void ChangeSource(Machine & newSource, int outType, int outWire, const Mapping * const mapping = NULL);
			void ChangeDest(Wire& newWireDest);
			void Disconnect();
			void Mix(int numSamples, float lVol, float rVol) const;
			void ChangeMapping(Mapping const & newmapping);
			void SetBestMapping(bool notify=true);
			Mapping EnsureCorrectness(Mapping const & mapping);
			Mapping const & GetMapping() const { return pinMapping; }
			inline float GetVolume() const { return volume * volMultiplier; };
			inline void GetVolume(float &value) const { value = volume * volMultiplier; };
			inline void SetVolume(float value);
			inline float GetVolMultiplier() const { return volMultiplier; }
			inline Machine & GetSrcMachine() const { assert(srcMachine); return *srcMachine; };
			inline Machine & GetDstMachine() const { assert(dstMachine); return *dstMachine; };
			inline int GetSrcWireIndex() const;
			inline int GetDstWireIndex() const;
			inline bool Enabled() const { return enabled; };
			Wire & operator=(const Wire & wireOrig);
		protected:
			/// Is this wire enabled, or has been deleted?
			bool enabled;
			/// Incoming connection Machine
			Machine* srcMachine;	
			/// Incoming connection Machine vol
			float volume;
			/// Value to multiply "volume" with to have a 0.0...1.0 range
			// The reason of the volMultiplier variable is because VSTs output wave data
			// in the range -1.0 to +1.0, while natives and internals output at -32768.0 to +32768.0
			// Initially (when the format was made), Psycle did convert this in the "Work" function,
			// but since it already needs to multiply the output by "volume", I decided to remove
			// that extra conversion and use directly the volume to do so. So now the variable
			// is used simply for the Get/SetVolume.
			float volMultiplier;
			//Pin mapping ([outputs][inputs])
			Mapping pinMapping;

			/// Outgoing connection Machine
			Machine* dstMachine;	
		};

		typedef struct LegacyWire_s
		{
		///\name input ports
		///\{
			/// Incoming connections Machine number
			int _inputMachine;
			/// Incoming connections activated
			bool _inputCon;
			/// Incoming connections Machine vol
			float _inputConVol;
			/// Value to multiply _inputConVol[] with to have a 0.0...1.0 range
			// The reason of the _wireMultiplier variable is because VSTs output wave data
			// in the range -1.0 to +1.0, while natives and internals output at -32768.0 to +32768.0
			// Initially (when the format was made), Psycle did convert this in the "Work" function,
			// but since it already needs to multiply the output by inputConVol, I decided to remove
			// that extra conversion and use directly the volume to do so.
			float _wireMultiplier;
			//Pin mapping (for loading)
			Wire::Mapping pinMapping;
		///\}

		///\name output ports
		///\{
			/// Outgoing connections Machine number
			int _outputMachine;
			/// Outgoing connections activated
			bool _connection;
		///\}

		} LegacyWire;
		class WireDlg;
		class Mixer;
		class PSArray;

		class ScopeMemory {			
		public:
			ScopeMemory();
			~ScopeMemory();
												
			int scope_buf_size;
			int scopePrevNumSamples;
			int scopeBufferIndex;
			int pos;			
			std::auto_ptr<PSArray> samples_left;
			std::auto_ptr<PSArray> samples_right;
		};
		
		/// Base class for "Machines", the audio producing elements.
		class Machine
		{
			friend class WireDlg;
			friend class Wire;
			friend class Mixer;
			///\name crash handling
			///\{
				public:
					typedef std::vector<Machine*> Container;
					typedef Container::iterator iterator;
					typedef Container::const_iterator const_iterator;

					virtual iterator begin() { return dummy_container_.begin(); }
					virtual iterator end() { return dummy_container_.end(); }	
					virtual const_iterator begin() const { return dummy_container_.begin(); }
					virtual const_iterator end() const { return dummy_container_.end(); }	
					virtual bool empty() const { return true; }
					virtual int size() const { return 0; }

					// virtual void AddMachineListener(class MachineListener* listener) {}
					// virtual void RemoveMachineListener(class MachineListener* listener) {}

				private:
					Container dummy_container_;

				public:
					/// This function should be called when an exception was thrown from the machine.
					/// This will mark the machine as crashed, i.e. crashed() will return true,
					/// and it will be disabled.
					///\param e the exception that occured, converted to a std::exception if needed.
					void crashed(std::exception const & e) throw();

				public:
					/// Tells wether this machine has crashed.
					bool const inline & crashed() const throw() { return crashed_; }
					void reload() { 						
						OnReload();
						crashed_=false;
						_standby = false;
						_bypass=false;            
					}
					virtual void OnReload() {}
					void set_crashed(bool on) {                         
						crashed_ = _bypass = _mute =  on;            
					}
				private:
					bool                crashed_;
			///\}

			///\name cpu time usage measurement
			///\{
				public: void reset_time_measurement() throw() { accumulated_processing_time_ = cpu_time_clock::duration::zero(); processing_count_ = 0; } // rePlayer

				public:  cpu_time_clock::duration accumulated_processing_time() const throw() { return accumulated_processing_time_; }
				private: cpu_time_clock::duration accumulated_processing_time_;
		protected: void accumulate_processing_time(::std::chrono::nanoseconds d) throw() { // rePlayer
						if(loggers::warning() && d.count() < 0) {
							std::ostringstream s;
							s << "time went backward by: " << ::std::chrono::nanoseconds(d).count() * 1e-9 << 's'; // rePlayer
							loggers::warning()(s.str(), UNIVERSALIS__COMPILER__LOCATION);
						} else accumulated_processing_time_ += d;
					}

				public:    uint64_t processing_count() const throw() { return processing_count_; }
				protected: uint64_t processing_count_;
			///\}

#if 0 // v1.9
			///\name each machine has a type attribute so that we can make yummy switch statements
			///\{
				public:
					///\see enum MachineClass which defined somewhere outside
					typedef MachineClass class_type;
					Machine::class_type inline subclass() const throw() { return _subclass; }
				public:///\todo private:
					class_type _subclass;
			///\}

			///\name each machine has a mode attribute so that we can make yummy switch statements
			///\{
				public:
					///\see enum MachineMode which is defined somewhere outside
					typedef MachineMode mode_type;
					mode_type inline mode() const throw() { return _mode; }
				public:///\todo private:
					mode_type _mode;
			///\}

			///\name machine's numeric identifier used in the patterns and gui display
			///\{
				public:
					/// legacy
					///\todo should be unsigned but some functions return negative values to signal errors instead of throwing an exception
					//PSYCLE__STRONG_TYPEDEF(int, id_type);
					typedef int id_type;
					id_type id() const throw() { return _macIndex; }
				public:///\todo private:
					/// it's actually used as an array index, but that shouldn't be part of the interface
					id_type _macIndex;
			///\}
#else
			public:///\todo private
				static bool autoStopMachine;
				int _macIndex;
				MachineType _type;
				MachineMode _mode;
#endif

			///\name the life cycle of a mahine
			///\{
				private:
					void FillScopeBuffer(float* scope_left, float* scope_right, int numSamples,
										 int& scopePrevNumSamples, int& scopeBufferIndex);
				public:
					virtual void Init();					
					virtual void PreWork(int numSamples,bool clear, bool measure_cpu_usage);
					virtual int GenerateAudio(int numsamples, bool measure_cpu_usage);
					virtual int GenerateAudioInTicks(int startSample, int numsamples);
					virtual void NewLine() {}
					virtual void Tick(int track, PatternEntry * pData) {}
					virtual void PostNewLine() {}
					virtual void Stop() {}
			///\}
			///\name used by the single-threaded, recursive scheduler
			///\{
					/// virtual because the mixer machine has its own implementation
					virtual void recursive_process(unsigned int frames, bool measure_cpu_usage);
					void recursive_process_deps(unsigned int frames, bool mix, bool measure_cpu_usage);
			///\}
			///\name used by the multi-threaded scheduler
			///\{
					typedef std::list<Machine const*> sched_deps;

					/// tells the scheduler which machines to process before this one
					virtual void sched_inputs(sched_deps&) const;
					/// tells the scheduler which machines may be processed after this one
					virtual void sched_outputs(sched_deps&) const;
					/// called by the scheduler to ask for the actual processing of the machine
					virtual bool sched_process(unsigned int frames, bool measure_cpu_usage);
			///\}
			///\name used by the single-threaded, recursive scheduler
			///\{
				/// guard to avoid feedback loops
				bool recursive_is_processing_;
				bool recursive_processed_;
			///\}

			///\name used by the multi-threaded scheduler
			///\{
				/// The multi-threaded scheduler cannot use _worked because it's not thread-synchronised.
				/// So, we define another boolean that's modified only by the multi-threaded scheduler,
				/// with proper thread synchronisations.
				/// The multi-threaded scheduler doesn't use recursive_processed_ nor recursive_is_processing_.
				bool sched_processed_;
			///\}
			///\name (de)serialization
			///\{
				public:
					virtual bool Load(RiffFile * pFile); //old fileformat
					static Machine * LoadFileChunk(RiffFile* pFile, int index, int version,bool fullopen=true);
					virtual bool LoadSpecificChunk(RiffFile* pFile, int version);
					virtual bool LoadWireMapping(RiffFile* pFile, int version);
					virtual bool LoadParamMapping(RiffFile* pFile, int version);
					virtual void SaveDllNameAndIndex(RiffFile * pFile,int index);
					virtual void SaveFileChunk(RiffFile * pFile);
					virtual void SaveSpecificChunk(RiffFile * pFile);
					virtual bool SaveWireMapping(RiffFile* pFile);
					virtual bool SaveParamMapping(RiffFile* pFile);
					virtual void PostLoad(Machine** _pMachine);
					//Helper for PSY2/PSY3 Song loading.
					static int FindLegacyOutput(Machine* sourceMac, int macIndex);

			///\}

			///\name connections ... wires
			///\{
				public:
					virtual void OnPinChange(Wire & wire, Wire::Mapping const & newMapping) {}
					virtual void OnInputConnected(Wire & wire);
					virtual void OnOutputConnected(Wire & wire, int outType, int outWire);
					virtual void OnInputDisconnected(Wire & wire);
					virtual void OnOutputDisconnected(Wire & wire);
					virtual void ExchangeInputWires(int first,int second);
					virtual void ExchangeOutputWires(int first,int second);
					virtual void NotifyNewSendtoMixer(Machine & callerMac, Machine & senderMac);
					virtual Machine& SetMixerSendFlag(bool set);
					virtual void DeleteWires();
					virtual int FindInputWire(const Wire* pWireToFind) const;
					virtual int FindOutputWire(const Wire* pWireToFind) const;
					virtual int FindInputWire(int macIndex) const;
					virtual int FindOutputWire(int macIndex) const;
					virtual int GetFreeInputWire(int slottype=0) const;
					virtual int GetFreeOutputWire(int slottype=0) const;
					virtual int GetInputSlotTypes() const { return 1; }
					virtual int GetOutputSlotTypes() const { return 1; }
					virtual float GetAudioRange() const { return 1.0f; }
					//For song loading with PSY2 and PSY3 fileformats
					std::vector<LegacyWire> legacyWires;
					/// Incoming connections
					std::vector<Wire> inWires;
					int connectedInputs() const;
					virtual std::string GetInputPinName(int pin) const {
						if(pin==0) return "Left";
						else if(pin==1) return "Right";
						else return "Unknown";
					}
					virtual int GetNumInputPins() const { return (_mode==MACHMODE_GENERATOR)?0:2; }
					/// Outgoing connections (reference to the destination wire)
					std::vector<Wire*> outWires;
					int connectedOutputs() const;
					virtual std::string GetOutputPinName(int pin) const {
						if(pin==0) return "Left";
						else if(pin==1) return "Right";
						else return "Unknown";
					}
					virtual int GetNumOutputPins() const { return (_mode==MACHMODE_MASTER)?0:2; }
			///\}

			///\name amplification of the signal in connections/wires
			///\{
				public:
					virtual void GetWireVolume(int wireIndex, float &value) { value = inWires[wireIndex].GetVolume(); }
					virtual float GetWireVolume(int wireIndex) { return inWires[wireIndex].GetVolume(); }
					virtual void SetWireVolume(int wireIndex,float value) { inWires[wireIndex].SetVolume(value); }
					virtual bool GetDestWireVolume(int WireIndex,float &value);
					virtual bool SetDestWireVolume(int WireIndex,float value);
					virtual void OnWireVolChanged(Wire & wire) {}
			///\}
			///\name panning
			///\{
				public:
					inline int GetPan() { return _panning; }
					///\todo 3 dimensional?
					virtual void SetPan(int newpan);
					void InitializeSamplesVector(int numChans=2);
					/// numerical value of panning
					int _panning;							
					/// left chan volume
					float _lVol;							
					/// right chan volume
					float _rVol;							
					std::vector<float*> samplesV;
			///\}

			///\name general information
			///\{
				public:
					///\todo: update this to std::string.
					virtual void SetEditName(std::string const & newname) {
						std::strncpy(_editName,newname.c_str(),sizeof(_editName)-1);
						_editName[sizeof(_editName)-1]='\0';
					}
					const char * GetEditName() const { return _editName; }
				public:///\todo private:
					///\todo this was a std::string in v1.9
					char _editName[32];
				protected:
					int _auxIndex;

				public:
					virtual const char* const GetName(void) const = 0;
					virtual const char * const GetDllName() const throw() { return ""; }
					virtual int GetPluginCategory() { return 0; }
					virtual bool IsShellMaster() { return false; }
					virtual int GetShellIdx() { return 0; }
					virtual bool NeedsAuxColumn() { return false; }
					virtual const char* AuxColumnName(int idx) const {return ""; }
					virtual int NumAuxColumnIndexes() { return 0;}
					virtual int AuxColumnIndex() { return _auxIndex;}
					virtual void AuxColumnIndex(int index) { _auxIndex=index; }
			///\}

			///\name parameters
			///\{
				public:
					virtual int GetNumCols() { return _nCols; }
					virtual int GetNumParams() { return _numPars; }
					virtual int GetParamType(int numparam) { return 0; }
					virtual void GetParamName(int numparam, char * name) { name[0]='\0'; }
					virtual void GetParamRange(int numparam, int &minval, int &maxval) {minval=0; maxval=0; }
					virtual void GetParamValue(int numparam, char * parval) { parval[0]='\0'; }
					virtual int GetParamValue(int numparam) { return 0; }
          virtual void GetParamId(int numparam, std::string& id) {
            std::ostringstream s; s << numparam; id = s.str();
          }
					virtual bool SetParameter(int numparam, int value) { return false; }
          virtual void AfterTweaked(int idx) {};
					virtual void SetCurrentProgram(int idx) {};
					virtual int GetCurrentProgram() {return 0;};
					virtual void GetCurrentProgramName(char* val) {strcpy(val,"Program 0");};
					virtual void GetIndexProgramName(int bnkidx, int prgIdx, char* val){strcpy(val,"Program 0");};
					virtual int GetNumPrograms(){ return 1;}; // total programs of the bank.
					virtual int GetTotalPrograms(){ return 1;}; //total programs independently of the bank
					virtual void SetCurrentBank(int idx) {};
					virtual int GetCurrentBank() {return 0;};
					virtual void GetCurrentBankName(char* val) {strcpy(val,"Internal");};
					virtual void GetIndexBankName(int bnkidx, char* val){strcpy(val,"Internal");};          
					virtual int GetNumBanks(){ return 1;};
					virtual void Tweak(CPreset const & preset);
					virtual void GetCurrentPreset(CPreset & preset);         

				public:///\todo private:
					int _numPars;
					int _nCols;
			///\}

			///\name gui stuff
			///\{
				public:
					virtual int  GetPosX() const { return _x; }
					virtual void SetPosX(int x) {_x = x;}
					virtual int  GetPosY() const { return _y; }
					virtual void SetPosY(int y) {_y = y;}
          virtual MachineUiType::Value GetUiType() const { return MachineUiType::NATIVE; }          

				public:///\todo private:
					int _x;
					int _y;
			///\}

			///\name states
			///\{
				public:
					virtual bool Bypass() const { return _bypass; }
					virtual void Bypass(bool e) { _bypass = e; }
				public:///\todo private:
					bool _bypass;

				public:
					virtual bool Standby() const { return _standby; }
					virtual void Standby(bool e) { _standby = e; }
				public:///\todo private:
					bool _standby;

				public:
					bool Mute() const { return _mute; }
					void Mute(bool e) { _mute = e; }
				public:///\todo private:
					bool _mute;
			///\}

		public:
			Machine();
			Machine(Machine* mac);
			Machine(MachineType msubclass, MachineMode mode, int id);
			virtual ~Machine() throw();
			virtual void SetSampleRate(int sr)
			{
#if PSYCLE__CONFIGURATION__RMS_VUS
				rms.count=0;
				rms.AccumLeft=0.;
				rms.AccumRight=0.;
				rms.previousLeft=0.;
				rms.previousRight=0.;
#endif
			}

			virtual void change_buffer(std::vector<float*>& buf);
			bool _sharedBuf;

		protected:
			virtual void UpdateVuAndStanbyFlag(int numSamples);

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//\todo below are unencapsulated data members

		public://\todo private:

			virtual bool playsTrack(const int track) const;

			///\name signal measurements (and also gui stuff)
			///\{
				/// output peak level for DSP
				float _volumeCounter;					
				/// output peak level for display
#if PSYCLE__CONFIGURATION__RMS_VUS
				helpers::dsp::RMSData rms;
#endif
				int _volumeDisplay;	
				/// output peak level for display
				int _volumeMaxDisplay;
				/// output peak level for display
				int _volumeMaxCounterLife;

				/// amount of samples processed in the previous work call. It is used to copy the samples to scope buffer in PreWork
				int _scopePrevNumSamples;
				/// scope buffer writing position. 16byte aligned (i.e. aligned by 4).
				int	_scopeBufferIndex;
				/// scope buffer, left channel
				float *_pScopeBufferL;
				/// scope buffer, right channel
				float *_pScopeBufferR;

				void AddScopeMemory(const boost::shared_ptr<ScopeMemory>& scope_memory);
				void RemoveScopeMemory(const boost::shared_ptr<ScopeMemory>& scope_memory);
				bool HasScopeMemories() const { return !scope_memories_.empty(); }
				typedef std::vector<boost::shared_ptr<ScopeMemory> > ScopeMemories;				
				ScopeMemories scope_memories_;

			///\}

			///\name misc
			///\{
				/// this machine is used by a send/return mixer. (Some things cannot be done on these machines)
				bool _isMixerSend;
			///\}
			///\name various player-related states
			///\todo hardcoded limits and wastes with MAX_TRACKS
			///\{
				///\todo doc
				PatternEntry TriggerDelay[MAX_TRACKS];
				///\todo doc
				int TriggerDelayCounter[MAX_TRACKS];
				///\todo doc
				int RetriggerRate[MAX_TRACKS];
				///\todo doc
				int ArpeggioCount[MAX_TRACKS];
				///\todo doc
				bool TWSActive;
				///\todo doc
				int TWSInst[MAX_TWS];
				///\todo doc
				int TWSSamples;
				///\todo doc
				float TWSDelta[MAX_TWS];
				///\todo doc
				float TWSCurrent[MAX_TWS];
				///\todo doc
				float TWSDestination[MAX_TWS];
			///\}
			///\name gui stuff
			///\{
				/// The topleft point of a square where the wire triangle is centered when drawn. (Used to detect when to open the wire dialog)
				///\todo hardcoded limits and wastes with MAX_CONNECTIONS
				CPoint _connectionPoint[MAX_CONNECTIONS];
			///\}

				// TODO 
				int translate_param(int virtual_index ) const { 
					return param_translator_.translate(virtual_index);
				}

				inline void set_virtual_param_index(int virtual_index, int machine_index) {
					param_translator_.set_virtual_index(virtual_index, machine_index);
				}
				inline void set_virtual_param_map(const ParamTranslator& param) {
					param_translator_ = param;
				}		
				const ParamTranslator& param_translator() const {
					return param_translator_;
				}
        
		private:
				ParamTranslator param_translator_;
		};

		inline int Wire::GetSrcWireIndex() const { return (enabled) ? GetSrcMachine().FindOutputWire(this) : -1; };
		inline int Wire::GetDstWireIndex() const { return GetDstMachine().FindInputWire(this); };
		
		/// master machine.
		class Master : public Machine
		{
		friend class Song;
		public:
			Master();
			Master(int index);
			virtual void Init(void);
			virtual int GenerateAudio(int numsamples, bool measure_cpu_usage);
			virtual void UpdateVuAndStanbyFlag(int numSamples);
			virtual float GetAudioRange() const { return 32768.0f; }
			virtual const char* const GetName(void) const { return _psName; }
			virtual bool Load(RiffFile * pFile); //old fileformat
			virtual bool LoadSpecificChunk(RiffFile * pFile, int version);
			virtual void SaveSpecificChunk(RiffFile * pFile);

			float* getLeft() const { return samplesV[0]; }
			float* getRight() const { return samplesV[1]; }

			int _outDry;
			bool vuupdated;
			bool _clip;
			bool decreaseOnClip;
			int peaktime;
			float currentpeak;
			float currentrms;
			float _lMax;
			float _rMax;
			int volumeDisplayLeft;
			int volumeDisplayRight;
			static float* _pMasterSamples;

			
		protected:
			static char* _psName;		
		};
		
    
	}
}

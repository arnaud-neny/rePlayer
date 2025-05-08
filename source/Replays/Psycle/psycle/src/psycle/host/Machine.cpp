///\file
///\brief implementation file for psycle::host::Machine

#include <psycle/host/detail/project.private.hpp>
#include "Machine.hpp"
// Included for "Work()" function and wirevolumes. Maybe this could be worked out
// in a different way
#include "Song.hpp"
// Included due to the plugin caching, which should be separated from the dialog.
#include "machineloader.hpp"

#if 0//!defined WINAMP_PLUGIN
	// This one is included to update the buffers that wiredlg uses for display. 
	// Find a way to manage these buffers without its inclusion
	#include "WireDlg.hpp"
#else
const int SCOPE_BUF_SIZE = 0;
#endif //!defined WINAMP_PLUGIN

// The inclusion of the following headers is needed because of a bad design.
// The use of these subclasses in a function of the base class should be 
// moved to the Song loader.
#include "internal_machines.hpp"
#include "Sampler.hpp"
#include "XMSampler.hpp"
#include "Plugin.hpp"
#include "VstHost24.hpp"
#include "LuaHost.hpp"
#include "LuaPlugin.hpp"
#include "ladspahost.hpp"

#include <universalis/os/aligned_alloc.hpp>
#include <psycle/helpers/value_mapper.hpp>
#include "cpu_time_clock.hpp"

namespace psycle
{
	namespace host
	{
		char* Master::_psName = "Master";
		bool Machine::autoStopMachine = false;		

		Wire::Wire(Machine * const dstMac)
			:enabled(false),srcMachine(NULL),volume(1.0f),volMultiplier(1.0f),dstMachine(dstMac)
		{
		}
		Wire::Wire(const Wire &wire)
			:enabled(wire.enabled),srcMachine(wire.srcMachine)
			,volume(wire.volume),volMultiplier(wire.volMultiplier)
			,dstMachine(wire.dstMachine),pinMapping(wire.pinMapping)
		{
		}
		Wire & Wire::operator=(const Wire & wireOrig)
		{
			enabled = wireOrig.enabled;
			srcMachine = wireOrig.srcMachine;	
			volume = wireOrig.volume;
			volMultiplier = wireOrig.volMultiplier;
			pinMapping = wireOrig.pinMapping;
			dstMachine = wireOrig.dstMachine;
			return *this;
		}
		void Wire::ConnectSource(Machine & srcMac, int outType, int outWire, const Mapping * const mapping)
		{
			if (enabled) {
				srcMachine->OnOutputDisconnected(*this);
				dstMachine->OnInputDisconnected(*this);
			}
			srcMachine = &srcMac;
			volMultiplier = srcMachine->GetAudioRange()/dstMachine->GetAudioRange();
			volume = 1.0f / volMultiplier;
			if (mapping) {
				pinMapping = EnsureCorrectness(*mapping);
			}
			else {
				SetBestMapping(false);
			}
			enabled = true;
			srcMachine->OnOutputConnected(*this, outType, outWire);
			dstMachine->OnInputConnected(*this);
		}
		void Wire::ChangeSource(Machine & newSource, int outType, int outWire, const Mapping * const mapping)
		{
			if (enabled) {
				srcMachine->OnOutputDisconnected(*this);
				dstMachine->OnInputDisconnected(*this);
			}
			float vol = GetVolume();
			srcMachine = &newSource;
			volMultiplier = srcMachine->GetAudioRange()/dstMachine->GetAudioRange();
			SetVolume(vol);
			if (mapping) {
				pinMapping = EnsureCorrectness(*mapping);
			}
			else {
				SetBestMapping(false);
			}
			enabled = true;
			srcMachine->OnOutputConnected(*this, outType, outWire);
			dstMachine->OnInputConnected(*this);
		}
		void Wire::ChangeDest(Wire& newWireDest)
		{
			if(Enabled()) {
				int outWIndex = this->GetSrcWireIndex();
				int outType = 0;
				Machine* oldSrc = srcMachine;
				Disconnect();
				if (outWIndex >= MAX_CONNECTIONS) {
					outWIndex-=MAX_CONNECTIONS;
					outType=1;
				}
				newWireDest.ConnectSource(*oldSrc,outType,outWIndex,&GetMapping());
				newWireDest.SetVolume(GetVolume());
			}
		}
		void Wire::SetVolume(float value)
		{
			volume = value / volMultiplier;
			GetDstMachine().OnWireVolChanged(*this);
		}
		void Wire::Disconnect()
		{
			if(enabled) {
				srcMachine->OnOutputDisconnected(*this);
				dstMachine->OnInputDisconnected(*this);
				srcMachine = NULL;
				enabled=false;
			}
		}


		void Wire::Mix(int numSamples, float lVol, float rVol) const
		{
			for(int i(0);i<pinMapping.size();i++) {
				const PinConnection& pin = pinMapping[i];
				helpers::dsp::Add(srcMachine->samplesV[pin.first], dstMachine->samplesV[pin.second], numSamples, ((pin.first%2)?rVol:lVol) * volume);
				math::erase_all_nans_infinities_and_denormals(dstMachine->samplesV[pin.second], numSamples);
			}
		}
		void Wire::SetBestMapping(bool notify)
		{
			int srcpins = srcMachine->GetNumOutputPins();
			int dstpins = dstMachine->GetNumInputPins();
			Mapping mapping;
			for (int s=0;s< srcpins; s++) {
				for (int d=0;d< dstpins; d++) {	
					if (s==d || s%dstpins == d || d%srcpins == s) {
						mapping.push_back(PinConnection(s,d));
					}
				}
			}
			if(notify) {
				GetSrcMachine().OnPinChange(*this,mapping);
				GetDstMachine().OnPinChange(*this,mapping);
			}
			pinMapping = mapping;
		}

		void Wire::ChangeMapping(Mapping const & newmapping)
		{
			Mapping correctMapping = EnsureCorrectness(newmapping);
			GetSrcMachine().OnPinChange(*this,correctMapping);
			GetDstMachine().OnPinChange(*this,correctMapping);
			pinMapping = correctMapping;
		}
		Wire::Mapping Wire::EnsureCorrectness(Mapping const & mapping)
		{
			Mapping correctMapping;
			int inputPins = GetDstMachine().GetNumInputPins();
			int outputPins = GetSrcMachine().GetNumOutputPins();
			for (int i(0);i<mapping.size();i++) {
				if (mapping[i].first < outputPins &&
					mapping[i].second < inputPins)
				{
					correctMapping.push_back(mapping[i]);
				}
			}
			return correctMapping;
		}

		void Machine::crashed(std::exception const & e) throw() {
			///\todo gui needs to update
			crashed_ = true;
			_bypass = true;
			_mute = true;
			std::ostringstream s;
			s
				<< "Machine: " << _editName << std::endl
				<< "DLL: "<< GetDllName() << std::endl
				<< e.what() << std::endl
				<< "The machine has been set to bypassed/muted to prevent it from making the host crash." << std::endl
				<< "You should save your work to a new file, and restart the host.";
			if(loggers::exception()) loggers::exception()(s.str());
			MessageBox(NULL, s.str().c_str(), "Error", MB_OK | MB_ICONERROR);
		}

		ScopeMemory::ScopeMemory() :
				samples_left(new PSArray(SCOPE_BUF_SIZE, 0.0)),
				samples_right(new PSArray(SCOPE_BUF_SIZE, 0.0)),
				scopePrevNumSamples(0),
				scopeBufferIndex(0),
				pos(0),
				scope_buf_size(SCOPE_BUF_SIZE) {
		}												

		ScopeMemory::~ScopeMemory() {
		}

		Machine::Machine(MachineType msubclass, MachineMode mode, int id)       
		{
		//	Machine();
			_type=msubclass;
			_mode=mode;
			_macIndex=id;
			_sharedBuf=false;
			_auxIndex = 0;
		}

		Machine::Machine()
			: crashed_()
			, _macIndex(0)
			, _auxIndex(0)
			, _type(MACH_UNDEFINED)
			, _mode(MACHMODE_UNDEFINED)
			, _bypass(false)
			, _mute(false)
			, recursive_is_processing_(false)
			, _standby(false)
			, recursive_processed_(false)
			, sched_processed_(false)
			, _isMixerSend(false)
			, _lVol(1.)
			, _rVol(1.)
			, _panning(64)
			, _x(0)
			, _y(0)
			, _numPars(0)
			, _nCols(1)
			, TWSSamples(0)
			, TWSActive(false)
			, _volumeCounter(0.0f)
			, _volumeDisplay(0)
			, _volumeMaxDisplay(0)
			, _volumeMaxCounterLife(0)
			, _pScopeBufferL(0)
			, _pScopeBufferR(0)
			, _scopeBufferIndex(0)
			, _scopePrevNumSamples(0)
			, _sharedBuf(false)     
		{
			_editName[0] = '\0';

			for (int c = 0; c<MAX_TRACKS; c++)
			{
				TriggerDelay[c]._cmd = 0;
				TriggerDelayCounter[c]=0;
				RetriggerRate[c]=256;
				ArpeggioCount[c]=0;
			}
			for (int c = 0; c<MAX_TWS; c++)
			{
				TWSInst[c] = 0;
				TWSDelta[c] = 0;
				TWSCurrent[c] = 0;
				TWSDestination[c] = 0;
			}

			//Ideally, this should be used dinamically, but for now, we only check for "Enabled()"
			//Also, beware that inWires instances are referenced from outWires, and
			//the vector could reassing the array, so it is better that it be constant.
			inWires.reserve(MAX_CONNECTIONS);
			outWires.resize(MAX_CONNECTIONS);
			for (int i = 0; i<MAX_CONNECTIONS; i++)
			{
				inWires.push_back(Wire(this));
				_connectionPoint[i].x=0;
				_connectionPoint[i].y=0;
			}
#if PSYCLE__CONFIGURATION__RMS_VUS
			rms.count=0;
			rms.AccumLeft=0.;
			rms.AccumRight=0.;
			rms.previousLeft=0.;
			rms.previousRight=0.;
#endif
			reset_time_measurement();
		}
		Machine::Machine(Machine* mac)
			: crashed_()
			, _macIndex(mac->_macIndex)
			, _auxIndex(mac->_auxIndex)
			, _type(mac->_type)
			, _mode(mac->_mode)
			, _bypass(mac->_bypass)
			, _mute(mac->_mute)
			, recursive_is_processing_(false)
			, _standby(false)
			, recursive_processed_(false)
			, sched_processed_(false)
			, _isMixerSend(false)
			, samplesV(0)
			, _lVol(mac->_lVol)
			, _rVol(mac->_rVol)
			, _panning(mac->_panning)
			, _x(mac->_x)
			, _y(mac->_y)
			, _numPars(mac->_numPars)
			, _nCols(mac->_nCols)
			, TWSSamples(0)
			, TWSActive(false)
			, _volumeCounter(0.0f)
			, _volumeDisplay(0)
			, _volumeMaxDisplay(0)
			, _volumeMaxCounterLife(0)
			, _pScopeBufferL(0)
			, _pScopeBufferR(0)
			, _scopeBufferIndex(0)
			, _scopePrevNumSamples(0)
			, _sharedBuf(false)    
		{
			strcpy(_editName,mac->_editName);

			for (int c = 0; c<MAX_TRACKS; c++)
			{
				TriggerDelay[c]._cmd = 0;
				TriggerDelayCounter[c]=0;
				RetriggerRate[c]=256;
				ArpeggioCount[c]=0;
			}
			for (int c = 0; c<MAX_TWS; c++)
			{
				TWSInst[c] = 0;
				TWSDelta[c] = 0;
				TWSCurrent[c] = 0;
				TWSDestination[c] = 0;
			}
			//Ideally, this should be used dinamically, but for now, we only check for "Enabled()"
			//Also, beware that inWires instances are referenced from outWires, and
			//the vector could reassing the array, so it is better that it be constant.
			inWires.reserve(MAX_CONNECTIONS);
			outWires.resize(MAX_CONNECTIONS);
			legacyWires = mac->legacyWires;
			for (int i(0);i<MAX_CONNECTIONS;i++)
			{
				inWires.push_back(Wire(this));
				if (mac->inWires[i].Enabled()) {
					mac->inWires[i].ChangeDest(inWires[i]);
				}
				if (mac->outWires[i] && mac->outWires[i]->Enabled()) {
					mac->outWires[i]->ChangeSource(*this,0,i, &mac->outWires[i]->GetMapping());
				}
				_connectionPoint[i].x=mac->_connectionPoint[i].x;
				_connectionPoint[i].y=mac->_connectionPoint[i].y;
			}
#if PSYCLE__CONFIGURATION__RMS_VUS
			rms.count=0;
			rms.AccumLeft=0.;
			rms.AccumRight=0.;
			rms.previousLeft=0.;
			rms.previousRight=0.;
#endif
			reset_time_measurement();
		}
		Machine::~Machine() throw()
		{
			if (!_sharedBuf) {
			  for(int i(0);i<samplesV.size();i++){
				universalis::os::aligned_memory_dealloc(samplesV[i]);
			  }
			  samplesV.clear();
			}
		}
		void Machine::InitializeSamplesVector(int numChans)
		{
			for(int i(0);i<samplesV.size();i++){
				universalis::os::aligned_memory_dealloc(samplesV[i]);
			}
			samplesV.clear();
			samplesV.reserve(numChans);
			for(int i(0);i<numChans;i++){
				float* channel = NULL;
				universalis::os::aligned_memory_alloc(16, channel, STREAM_SIZE);
				helpers::dsp::Clear(channel,STREAM_SIZE);
				samplesV.push_back(channel);
			}
		}

		void Machine::Init()
		{
			// Standard gear initalization
			_mute = false;
			Standby(false);
			Bypass(false);
			recursive_is_processing_ = false;
			// Centering volume and panning
			SetPan(64);
#if PSYCLE__CONFIGURATION__RMS_VUS
			rms.AccumLeft=0.;
			rms.AccumRight=0.;
			rms.count=0;
			rms.previousLeft=0.;
			rms.previousRight=0.;
#endif
			reset_time_measurement();
		}

		void Machine::change_buffer(std::vector<float*>& buf) {
			if (!_sharedBuf) {
			   for(int i(0);i<samplesV.size();i++){
			     universalis::os::aligned_memory_dealloc(samplesV[i]);			
			   }
			}
			std::vector<float*>& sv = samplesV;
			std::vector<float*>::iterator it = sv.begin();
			std::vector<float*>::iterator bit = buf.begin();
			for ( ; it != sv.end(); ++it, ++bit) {		
				*it = *bit;
			}
			_sharedBuf = true;
		}

		void Machine::SetPan(int newPan)
		{
			if (newPan < 0)
			{
				newPan = 0;
			}
			if (newPan > 128)
			{
				newPan = 128;
			}
			_rVol = newPan * 0.015625f;
			_lVol = 2.0f-_rVol;
			if (_lVol > 1.0f)
			{
				_lVol = 1.0f;
			}
			if (_rVol > 1.0f)
			{
				_rVol = 1.0f;
			}
			_panning = newPan;
		}
		int Machine::connectedInputs() const
		{
			int count = 0;
			for(int c=0; c<MAX_CONNECTIONS; c++)
			{
				if(inWires[c].Enabled()) count++;
			}
			return count;
		}
		int Machine::connectedOutputs() const
		{
			int count = 0;
			for(int c=0; c<MAX_CONNECTIONS; c++)
			{
				if(outWires[c] != NULL && outWires[c]->Enabled()) count++;
			}
			return count;
		}
		int Machine::GetFreeInputWire(int slottype) const
		{
			for(int c=0; c<MAX_CONNECTIONS; c++)
			{
				if(!inWires[c].Enabled()) return c;
			}
			return -1;
		}
		int Machine::GetFreeOutputWire(int slottype) const
		{
			for(int c=0; c<MAX_CONNECTIONS; c++)
			{
				if(!outWires[c] || !outWires[c]->Enabled()) return c;
			}
			return -1;
		}
		int Machine::FindInputWire(const Wire* pWireToFind) const
		{
			for (int c=0; c<MAX_CONNECTIONS; c++)
			{
				if (&inWires[c] == pWireToFind) {
					return c;
				}
			}
			return -1;
		}
		int Machine::FindOutputWire(const Wire* pWireToFind) const
		{
			for (int c=0; c<MAX_CONNECTIONS; c++)
			{
				if (outWires[c] == pWireToFind) {
					return c;
				}
			}
			return -1;
		}

		int Machine::FindInputWire(int macIndex) const
		{
			for (int c=0; c<MAX_CONNECTIONS; c++)
			{
				if (inWires[c].Enabled())
				{
					if (inWires[c].GetSrcMachine()._macIndex == macIndex)
					{
						return c;
					}
				}
			}
			return -1;
		}

		int Machine::FindOutputWire(int macIndex) const
		{
			for (int c=0; c<MAX_CONNECTIONS; c++)
			{
				if (outWires[c] && outWires[c]->Enabled())
				{
					if (outWires[c]->GetDstMachine()._macIndex == macIndex)
					{
						return c;
					}
				}
			}
			return -1;
		}

		bool Machine::SetDestWireVolume(int wireIndex,float value)
		{
			if (wireIndex > MAX_CONNECTIONS || !outWires[wireIndex] || !outWires[wireIndex]->Enabled()) {
				return false;
			}

			outWires[wireIndex]->SetVolume(value);
			return true;
		}

		bool Machine::GetDestWireVolume(int wireIndex,float &value)
		{
			if (wireIndex > MAX_CONNECTIONS || !outWires[wireIndex] || !outWires[wireIndex]->Enabled()) {
				return false;
			}

			outWires[wireIndex]->GetVolume(value);
			return true;
		}

		void Machine::OnOutputConnected(Wire & wire, int outType, int outWire)
		{
			outWires[outWire]=&wire;
		}
		void Machine::OnInputConnected(Wire& wire)
		{
			if ( _isMixerSend && wire.GetSrcMachine()._type != MACH_MIXER)
			{
				Machine& sender = wire.GetSrcMachine().SetMixerSendFlag(true);
				NotifyNewSendtoMixer(*this,sender);
			}
		}
		void Machine::OnOutputDisconnected(Wire & wire)
		{
			int i = wire.GetSrcWireIndex();
			assert(i>=0);
			outWires[i]=NULL;
		}
		void Machine::OnInputDisconnected(Wire & wire)
		{
			if ( _isMixerSend && wire.GetSrcMachine()._type != MACH_MIXER)
			{
				// Chain is broken, notify the mixer so that it replaces the send machine of the send/return.
				NotifyNewSendtoMixer(*this,*this);
			}
		}
		void Machine::DeleteWires()
		{
			// Deleting the connections to/from other machines
			for(int w=0; w<MAX_CONNECTIONS; w++)
			{
				// Checking In-Wires
				if(inWires[w].Enabled())
				{
					inWires[w].Disconnect();
				}
				// Checking Out-Wires
				if(outWires[w] && outWires[w]->Enabled())
				{
					outWires[w]->Disconnect();
				}
			}
		}

		void Machine::ExchangeInputWires(int first, int second)
		{
			//Since the exchange of input wires implies copying,
			//the pointers in outWires need to be swapped too.
			if (inWires[first].Enabled()) {
				int idx = inWires[first].GetSrcWireIndex();
				inWires[first].GetSrcMachine().outWires[idx] = &inWires[second];
			}
			if (inWires[second].Enabled()) {
				int idx = inWires[second].GetSrcWireIndex();
				inWires[second].GetSrcMachine().outWires[idx] = &inWires[first];
			}
			Wire inWire = inWires[first];
			inWires[first] = inWires[second];
			inWires[second] = inWire;
		}
		void Machine::ExchangeOutputWires(int first, int second)
		{
			Wire* outWire = outWires[first];
			outWires[first] = outWires[second];
			outWires[second] = outWire;
			CPoint point = _connectionPoint[first];
			_connectionPoint[first] = _connectionPoint[second];
			_connectionPoint[second] = point;
		}
		void Machine::NotifyNewSendtoMixer(Machine & callerMac, Machine & senderMac)
		{
			//Work down the connection wires until finding the mixer.
			for (int i(0);i< MAX_CONNECTIONS; ++i) {
				if ( outWires[i] && outWires[i]->Enabled()) {
					outWires[i]->GetDstMachine().NotifyNewSendtoMixer(*this,senderMac);
				}
			}
		}
		Machine& Machine::SetMixerSendFlag(bool set)
		{
			//Work up the connection wires to add others' flag.
			Machine* dest=this;
			for (int i(0);i< MAX_CONNECTIONS; ++i) {
				if (inWires[i].Enabled() && inWires[i].GetSrcMachine()._type != MACH_MIXER) {
					dest = &inWires[i].GetSrcMachine().SetMixerSendFlag(set);
				}
			}
			_isMixerSend=set;
			return *dest;
		}
		void Machine::GetCurrentPreset(CPreset & preset)
		{
			int numParameters = GetNumParams();
			preset.Init(numParameters);
			for(int i=0; i < numParameters; ++i)
			{
				preset.SetParam(i , GetParamValue(i));
			}
		}
		void Machine::Tweak(CPreset const & preset)
		{
			CExclusiveLock lock(&Global::song().semaphore, 2, true);
			int num=preset.GetNumPars();
			for(int i(0) ; i < num ; ++i)
			{
				try
				{
					SetParameter(i, preset.GetParam(i));
				}
				catch(const std::exception &)
				{
					// o_O`
				}
				catch(...) // reinterpret_cast sucks
				{
					// o_O`
				}
			}
		}

		void Machine::AddScopeMemory(const boost::shared_ptr<ScopeMemory>& scope_memory) {			
			scope_memories_.push_back(scope_memory);
		}
		void Machine::RemoveScopeMemory(const boost::shared_ptr<ScopeMemory>& scope_memory) {
			ScopeMemories::iterator it = scope_memories_.begin();
			for ( ; it != scope_memories_.end(); ++it) {
			  if (*it == scope_memory) {
				scope_memories_.erase(it);
				break;
			  }
			}
		}

		void Machine::PreWork(int numSamples,bool clear, bool measure_cpu_usage)
		{
			sched_processed_ = recursive_processed_ = recursive_is_processing_ = false;
			cpu_time_clock::time_point t0;
			if(measure_cpu_usage) t0 = cpu_time_clock::now();
#if 0//!defined WINAMP_PLUGIN
			FillScopeBuffer(_pScopeBufferL, _pScopeBufferR, numSamples, _scopePrevNumSamples, _scopeBufferIndex);
			ScopeMemories::iterator it = scope_memories_.begin();
			for (; it != scope_memories_.end(); ++it) {
				boost::shared_ptr<ScopeMemory>& scope_memory = *it;
				FillScopeBuffer(scope_memory->samples_left->data(),
							scope_memory->samples_right->data(),
							numSamples,
							scope_memory->scopePrevNumSamples,
							scope_memory->scopeBufferIndex);
			}
#endif //!defined WINAMP_PLUGIN
			if (clear)
			{
				for(int i(0);i<samplesV.size();i++) {
					helpers::dsp::Clear(samplesV[i], numSamples);
				}
			}
			if(measure_cpu_usage) {
				cpu_time_clock::time_point const t1(cpu_time_clock::now());
				Global::song().accumulate_routing_time(t1 - t0);
			}
		}

		void Machine::FillScopeBuffer(float* scope_left, float* scope_right,
									  int numSamples, 
									  int& scopePrevNumSamples,
									  int& scopeBufferIndex) {
			if (scope_left && scope_right)
			{
				float *pL = samplesV[0];
				float *pR = (samplesV.size() == 1) ? samplesV[0] : samplesV[1];

				int i = scopePrevNumSamples;
				int diff = 0;
				if (i+scopeBufferIndex > SCOPE_BUF_SIZE)   
				{   
					int const amountSamples=(SCOPE_BUF_SIZE - scopeBufferIndex);
					helpers::dsp::Mov(pL,&scope_left[scopeBufferIndex], amountSamples);
					helpers::dsp::Mov(pR,&scope_right[scopeBufferIndex], amountSamples);
					diff = 4 - (amountSamples&0x3); // check for alignment.
					pL+=amountSamples;
					pR+=amountSamples;
					i -= amountSamples;
					scopeBufferIndex = 0;
				}
				if (diff) { //If not aligned, realign the source buffer.
					if (i <= diff) {
						std::memcpy(&scope_left[scopeBufferIndex], pL, i * sizeof(float));
						std::memcpy(&scope_right[scopeBufferIndex], pR, i * sizeof(float));
					} else {
						std::memcpy(&scope_left[scopeBufferIndex], pL, diff * sizeof(float));
						std::memcpy(&scope_right[scopeBufferIndex], pR, diff * sizeof(float));
						pL+=diff;
						pR+=diff;
						_scopeBufferIndex += diff;
						i-=diff;
						helpers::dsp::Mov(pL,&scope_left[scopeBufferIndex], i);
						helpers::dsp::Mov(pR,&scope_right[scopeBufferIndex], i);
					}
				}
				else {
					helpers::dsp::Mov(pL,&scope_left[scopeBufferIndex], i);
					helpers::dsp::Mov(pR,&scope_right[scopeBufferIndex], i);
				}
				scopeBufferIndex += i;
				if (scopeBufferIndex == SCOPE_BUF_SIZE) scopeBufferIndex = 0;
				i = 0;
			}
			scopePrevNumSamples=numSamples;
		}
// Low level process function of machines. Takes care of audio generation and routing.
// Each machine is expected to produce its output in its own _pSamplesX buffers.
void Machine::recursive_process(unsigned int frames, bool measure_cpu_usage) {
	recursive_process_deps(frames, true, measure_cpu_usage);

	cpu_time_clock::time_point t1;
	if(measure_cpu_usage) t1 = cpu_time_clock::now();

	GenerateAudio(frames, measure_cpu_usage);
	recursive_processed_ = true;

	if(measure_cpu_usage) {
		cpu_time_clock::time_point const t2(cpu_time_clock::now());
		accumulate_processing_time(t2 - t1);
	}
}

void Machine::recursive_process_deps(unsigned int frames, bool mix, bool measure_cpu_usage) {
	recursive_is_processing_ = true;
	for(int i(0); i < MAX_CONNECTIONS; ++i) {
		if(inWires[i].Enabled()) {
			Machine & pInMachine = inWires[i].GetSrcMachine();
			if(!pInMachine.recursive_processed_ && !pInMachine.recursive_is_processing_)
				pInMachine.recursive_process(frames,measure_cpu_usage);
			if(!pInMachine.Standby()) Standby(false);
			if(!_mute && !Standby() && mix && (!_isMixerSend || pInMachine._type != MACH_MIXER)) {
				cpu_time_clock::time_point t0;
				if(measure_cpu_usage) t0 = cpu_time_clock::now();
				inWires[i].Mix(frames,pInMachine._lVol,pInMachine._rVol);
				if(measure_cpu_usage) { 
					cpu_time_clock::time_point const t1(cpu_time_clock::now());
					Global::song().accumulate_routing_time(t1 - t0);
				}
			}
		}
	}
	if(mix) {
		cpu_time_clock::time_point t0;
		if(measure_cpu_usage) t0 = cpu_time_clock::now();
		for(int i(0);i<samplesV.size();i++) {
			helpers::math::erase_all_nans_infinities_and_denormals(samplesV[i], frames);
		}
		if(measure_cpu_usage) {
			cpu_time_clock::time_point const t1(cpu_time_clock::now());
			Global::song().accumulate_routing_time(t1 - t0);
		}
	}
	recursive_is_processing_ = false;
}

/// tells the scheduler which machines to process before this one
void Machine::sched_inputs(sched_deps & result) const {
	for(int c(0); c < MAX_CONNECTIONS; ++c) if(inWires[c].Enabled()) {
		result.push_back(&inWires[c].GetSrcMachine());
	}
}

/// tells the scheduler which machines may be processed after this one
void Machine::sched_outputs(sched_deps & result) const {
	for(int c(0); c < MAX_CONNECTIONS; ++c) if(outWires[c] && outWires[c]->Enabled()) {
		result.push_back(&outWires[c]->GetDstMachine());
	}
}

/// called by the scheduler to ask for the actual processing of the machine
bool Machine::sched_process(unsigned int frames, bool measure_cpu_usage) {
	cpu_time_clock::time_point t0;
	if(measure_cpu_usage) t0 = cpu_time_clock::now();

	if(!_mute) for(int i(0); i < inWires.size(); ++i) if(inWires[i].Enabled()) {
		const Machine & input_node(inWires[i].GetSrcMachine());
		if(!input_node.Standby()) Standby(false);
		// Mixer already prepares the buffers onto the sends.
		if(!Standby() && (input_node._type != MACH_MIXER || !_isMixerSend)) {
			inWires[i].Mix(frames, input_node._lVol, input_node._rVol);
		}
	}
	for(int i(0);i<samplesV.size();i++) {
		helpers::math::erase_all_nans_infinities_and_denormals(samplesV[i], frames);
	}

	cpu_time_clock::time_point t1;
	if(measure_cpu_usage) {
		t1 = cpu_time_clock::now();
		Global::song().accumulate_routing_time(t1 - t0);
	}

	GenerateAudio(frames,measure_cpu_usage);
	if(measure_cpu_usage) {
		cpu_time_clock::time_point const t2(cpu_time_clock::now());
		accumulate_processing_time(t2 - t1);
	}

	++processing_count_;

	return true;
}
int Machine::GenerateAudio(int numsamples, bool measure_cpu_usage) {
	//Current implementation limited to work in ticks. check psycle-core's in trunk for the other implementation.
	return GenerateAudioInTicks(0,numsamples);
}
int Machine::GenerateAudioInTicks(int /*startSample*/, int numsamples) {
	return 0;
}
		bool Machine::playsTrack(const int track) const {
			if (Standby() || _mode != MACHMODE_GENERATOR) {
				return false;
			}
#if PSYCLE__CONFIGURATION__RMS_VUS
			//This is made to prevent calculating it when the count has been reseted.
			if ( rms.count > 512 && TriggerDelayCounter[track] <= 0) {
				double bla = std::sqrt(std::max(rms.AccumLeft,rms.AccumRight)*(1.0/GetAudioRange())  / (double)rms.count);
				if( bla < 0.00024 )
				{
					return false;
				}
			}
#else
			if (_volumeCounter < 8.0f)	{
				return false;
			}
#endif
			return true;
		}

		void Machine::UpdateVuAndStanbyFlag(int numSamples)
		{
#if PSYCLE__CONFIGURATION__RMS_VUS
			float *pSamplesL = samplesV[0];
			float *pSamplesR;
			if(samplesV.size() == 1) {
				pSamplesR = samplesV[0];
			}
			else {
				pSamplesR = samplesV[1];
			}

			_volumeCounter = helpers::dsp::GetRMSVol(rms,pSamplesL,pSamplesR,numSamples)*(1.f/GetAudioRange());
			//Transpose scale from -40dbs...0dbs to 0 to 97pix. (actually 100px)
			int temp(helpers::math::round<int,float>((50.0f * log10f(_volumeCounter)+100.0f)));
			// clip values
			if(temp > 97) temp = 97;
			if(temp > 0)
			{
				_volumeDisplay = temp;
			}
			else if (_volumeDisplay>1 ) _volumeDisplay -=2;
			else {_volumeDisplay = 0;}

			if ( autoStopMachine && !Standby())
			{
				//This is made to prevent calculating it when the count has been reseted.
				if(rms.count >= 512) {
					double bla = std::sqrt(std::max(rms.AccumLeft,rms.AccumRight)*(1.0/GetAudioRange())  / (double)rms.count);
					if( bla < 0.00024 )
					{
						rms.count=512;
						rms.AccumLeft=0.;
						rms.AccumRight=0.;
						rms.previousLeft=0.;
						rms.previousRight=0.;
						_volumeCounter = 0.0f;
						_volumeDisplay = 0;
						Standby(true);
					}
				}
			}
#else
			_volumeCounter = helpers::dsp::GetMaxVol(pSamplesL, pSamplesR, numSamples)*(1.f/GetAudioRange());
			//Transpose scale from -40dbs...0dbs to 0 to 97pix. (actually 100px)
			int temp(helpers::math::round<int,float>((50.0f * log10f(_volumeCounter)+100.0f)));
			// clip values
			if(temp > 97) temp = 97;
			if(temp > _volumeDisplay) _volumeDisplay = temp;
			if (_volumeDisplay>0 )--_volumeDisplay;
			if ( autoStopMachine )
			{
				if (_volumeCounter < 8.0f)	{
					_volumeCounter = 0.0f;
					_volumeDisplay = 0;
					Standby(true);
				}
			}
#endif
		}

		bool Machine::LoadSpecificChunk(RiffFile* pFile, int version)
		{
			UINT size;
			pFile->Read(&size,sizeof(size)); // size of this part params to load
			UINT count;
			pFile->Read(&count,sizeof(count)); // num params to load
			for (UINT i = 0; i < count; i++)
			{
				int temp;
				pFile->Read(&temp,sizeof(temp));
				SetParameter(i,temp);
			}
			pFile->Skip(size-sizeof(count)-(count*sizeof(int)));
			return true;
		}

		Machine* Machine::LoadFileChunk(RiffFile* pFile, int index, int version,bool fullopen)
		{
			// assume version 0 for now
			bool bDeleted(false);
			Machine* pMachine;
			MachineType type;
			char dllName[256];
			pFile->Read(&type,sizeof(type));
			pFile->ReadString(dllName,256);
			switch (type)
			{
			case MACH_MASTER:
				if ( fullopen) pMachine = new Master(index);
				else pMachine = new Dummy(index);
				break;
			case MACH_SAMPLER:
				if ( fullopen ) pMachine = new Sampler(index);
				else pMachine = new Dummy(index);
				break;
			case MACH_XMSAMPLER:
				if ( fullopen ) pMachine = new XMSampler(index);
				else pMachine = new Dummy(index);
				break;
			case MACH_DUPLICATOR:
				if ( fullopen ) pMachine = new DuplicatorMac(index);
				else pMachine = new Dummy(index);
				break;
			case MACH_DUPLICATOR2:
				if ( fullopen ) pMachine = new DuplicatorMac2(index);
				else pMachine = new Dummy(index);
				break;
			case MACH_MIXER:
				if ( fullopen ) pMachine = new Mixer(index);
				else pMachine = new Dummy(index);
				break;
			case MACH_RECORDER:
				if ( fullopen ) pMachine = new AudioRecorder(index);
				else pMachine = new Dummy(index);
				break;
			case MACH_PLUGIN:
				{
					if(fullopen)
					{
						Plugin * p;
						pMachine = p = new Plugin(index);
						if(!p->LoadDll(dllName))
						{
							pMachine = new Dummy(index);
							
							delete p;
							bDeleted = true;
						}
					}
					else pMachine = new Dummy(index);
				}
				break;
			case MACH_LADSPA:
				{
					if(fullopen)
					{
						std::string sPath;
						int shellIdx=0;
						pMachine=0;
						if(!Global::machineload().lookupDllName(dllName,sPath,MACH_LADSPA,shellIdx)) 
						{
							// Check Compatibility Table.
							// Probably could be done with the dllNames lookup.
							//GetCompatible(psFileName,sPath2) // If no one found, it will return a null string.
							sPath = dllName;
						}
						if(Global::machineload().TestFilename(sPath,shellIdx))
						{
							try
							{
								pMachine =  LadspaHost::LoadPlugin(sPath,index, shellIdx);
							}
							//TODO: Warning! This is not std::exception, but universalis::stdlib::exception
							catch(const std::exception& e)
							{
								loggers::exception()(e.what());
							}
							catch(...)
							{
	#ifndef NDEBUG 
								throw;
	#else
								loggers::exception()("unknown exception");
	#endif
							}
							if (pMachine == NULL) {
#if !defined WINAMP_PLUGIN
							char sError[MAX_PATH + 100];
							sprintf(sError,"Replacing Ladspa plug-in \"%s\" with Dummy.",dllName);
                            Global().pLogCallback(sError, "Loading Error");
#endif //!defined WINAMP_PLUGIN
							pMachine = new Dummy(index);
							bDeleted = true;
							}
						}
					}
					else pMachine = new Dummy(index);
					break;
				}
			case MACH_LUA:
				{
				  if(fullopen)
					{
						std::string sPath;
						LuaPlugin *luaPlug=0;
						int shellIdx=0;

						if(!Global::machineload().lookupDllName(dllName,sPath,MACH_LUA,shellIdx)) 
						{
							// Check Compatibility Table.
							// Probably could be done with the dllNames lookup.
							//GetCompatible(psFileName,sPath2) // If no one found, it will return a null string.
							sPath = dllName;
						}
						if(Global::machineload().TestFilename(sPath,shellIdx) ) 
						{
							try
							{
								luaPlug = dynamic_cast<LuaPlugin*>(new LuaPlugin(sPath.c_str(), shellIdx));								
							}
							catch(const std::runtime_error & e)
							{
								std::ostringstream s; s << typeid(e).name() << std::endl;
								if(e.what()) s << e.what(); else s << "no message"; s << std::endl;
								loggers::exception()(s.str());
							}
							//TODO: Warning! This is not std::exception, but universalis::stdlib::exception
							catch(const std::exception & e)
							{
								loggers::exception()(e.what());
							}
							catch(...)
							{
	#ifndef NDEBUG 
								throw;
	#else
								loggers::exception()("unknown exception");
	#endif
							}
						}

						if(!luaPlug)
						{
#if !defined WINAMP_PLUGIN
							char sError[MAX_PATH + 100];
							sprintf(sError,"Replacing Lua plug-in \"%s\" with Dummy.",dllName);
                            Global().pLogCallback(sError, "Loading Error");
#endif //!defined WINAMP_PLUGIN
							pMachine = new Dummy(index);
							// ((Dummy*)pMachine)->wasVST=true;
							bDeleted = true;
						}
						else
						{
							luaPlug->_macIndex=index;
							pMachine = luaPlug;
						}
					}
					else pMachine = new Dummy(index);				
				break;
				}
			case MACH_VST:
			case MACH_VSTFX:
				{
					if(fullopen)
					{
						std::string sPath;
						vst::Plugin *vstPlug=0;
						int shellIdx=0;

						if(!Global::machineload().lookupDllName(dllName,sPath,MACH_VST,shellIdx)) 
						{
							// Check Compatibility Table.
							// Probably could be done with the dllNames lookup.
							//GetCompatible(psFileName,sPath2) // If no one found, it will return a null string.
							sPath = dllName;
						}
						if(Global::machineload().TestFilename(sPath,shellIdx) ) 
						{
							try
							{
								vstPlug = dynamic_cast<vst::Plugin*>(Global::vsthost().LoadPlugin(sPath.c_str(),shellIdx));
							}
							catch(const std::runtime_error & e)
							{
								std::ostringstream s; s << typeid(e).name() << std::endl;
								if(e.what()) s << e.what(); else s << "no message"; s << std::endl;
								loggers::exception()(s.str());
							}
							//TODO: Warning! This is not std::exception, but universalis::stdlib::exception
							catch(const std::exception & e)
							{
								loggers::exception()(e.what());
							}
							catch(...)
							{
	#ifndef NDEBUG 
								throw;
	#else
								loggers::exception()("unknown exception");
	#endif
							}
						}

						if(!vstPlug)
						{
#if !defined WINAMP_PLUGIN
							char sError[MAX_PATH + 100];
							sprintf(sError,"Replacing VST plug-in \"%s\" with Dummy.",dllName);
							Global().pLogCallback(sError, "Loading Error");
#endif //!defined WINAMP_PLUGIN
							pMachine = new Dummy(index);
							((Dummy*)pMachine)->wasVST=true;
							bDeleted = true;
						}
						else
						{
							vstPlug->_macIndex=index;
							pMachine = vstPlug;
						}
					}
					else pMachine = new Dummy(index);
				}
				break;
			default:
#if !defined WINAMP_PLUGIN
				if (type != MACH_DUMMY ) Global().pLogCallback("Please inform the devers about this message: unknown kind of machine while loading new file format", "Loading Error");
#endif //!defined WINAMP_PLUGIN
				pMachine = new Dummy(index);
				break;
			}
			pMachine->Init();
			if(!bDeleted)
			{
				//this is when "fullopen=false", since then only dummys are loaded
				pMachine->_type = type;
			}
			pFile->Read(&pMachine->_bypass,sizeof(pMachine->_bypass));
			pFile->Read(&pMachine->_mute,sizeof(pMachine->_mute));
			pFile->Read(&pMachine->_panning,sizeof(pMachine->_panning));
			pFile->Read(&pMachine->_x,sizeof(pMachine->_x));
			pFile->Read(&pMachine->_y,sizeof(pMachine->_y));
			pFile->Skip(2*sizeof(int));	// numInputs, numOutputs
			pMachine->legacyWires.resize(MAX_CONNECTIONS);
			for(int i = 0; i < MAX_CONNECTIONS; i++)
			{
				pFile->Read(&pMachine->legacyWires[i]._inputMachine,sizeof(pMachine->legacyWires[i]._inputMachine));	// Incoming connections Machine number
				pFile->Read(&pMachine->legacyWires[i]._outputMachine,sizeof(pMachine->legacyWires[i]._outputMachine));	// Outgoing connections Machine number
				pFile->Read(&pMachine->legacyWires[i]._inputConVol,sizeof(pMachine->legacyWires[i]._inputConVol));	// Incoming connections Machine vol
				pFile->Read(&pMachine->legacyWires[i]._wireMultiplier,sizeof(pMachine->legacyWires[i]._wireMultiplier));	// Value to multiply _inputConVol[] to have a 0.0...1.0 range
				pFile->Read(&pMachine->legacyWires[i]._connection,sizeof(pMachine->legacyWires[i]._connection));      // Outgoing connections activated
				pFile->Read(&pMachine->legacyWires[i]._inputCon,sizeof(pMachine->legacyWires[i]._inputCon));		// Incoming connections activated
			}
			pFile->ReadString(pMachine->_editName,sizeof(pMachine->_editName));
			if(bDeleted)
			{
				((Dummy*)pMachine)->dllName=dllName;
				std::stringstream s;
				s << "X!" << pMachine->GetEditName();
				((Dummy*)pMachine)->SetEditName(s.str());
			}
			if(!fullopen) return pMachine;

			if(pMachine->LoadSpecificChunk(pFile,version))
			{
				if (version >= 1)
				{
					//TODO: What to do on possibly wrong wire load?
					pMachine->LoadWireMapping(pFile, version);
				}
				if (version >= 2)
				{
					pMachine->LoadParamMapping(pFile, version);
				}
			}
			else {
#if !defined WINAMP_PLUGIN
				char sError[MAX_PATH + 100];
				Global().pLogCallback("Missing or Corrupted Machine Specific Chunk \"%s\" - replacing with Dummy.", "Loading Error");
#endif //!defined WINAMP_PLUGIN
				Machine* p = new Dummy(index);
				p->Init();
				p->_type=MACH_DUMMY;
				p->_mode=pMachine->_mode;
				p->_bypass=pMachine->_bypass;
				p->_mute=pMachine->_mute;
				p->_panning=pMachine->_panning;
				p->_x=pMachine->_x;
				p->_y=pMachine->_y;
				p->legacyWires.resize(MAX_CONNECTIONS);
				for(int i=0; i < MAX_CONNECTIONS; i++) {
					memcpy(&p->legacyWires[i],&pMachine->legacyWires[i],sizeof(LegacyWire));
				}				
				// dummy name goes here
				sprintf(p->_editName,"X %s",pMachine->_editName);
				p->_numPars=0;
				delete pMachine;
				pMachine=p;
			}
			
			if(index < MAX_BUSES)
			{
				pMachine->_mode = MACHMODE_GENERATOR;
			}
			else if (index < MAX_BUSES*2)
			{
				pMachine->_mode = MACHMODE_FX;
			}
			else
			{
				pMachine->_mode = MACHMODE_MASTER;
			}
			pMachine->SetPan(pMachine->_panning);
			if (pMachine->_bypass) pMachine->Bypass(true);
			return pMachine;
		}


		void Machine::SaveFileChunk(RiffFile* pFile)
		{
			pFile->Write(&_type,sizeof(_type));
			SaveDllNameAndIndex(pFile,GetShellIdx());
			pFile->Write(&_bypass,sizeof(_bypass));
			pFile->Write(&_mute,sizeof(_mute));
			pFile->Write(&_panning,sizeof(_panning));
			pFile->Write(&_x,sizeof(_x));
			pFile->Write(&_y,sizeof(_y));
			int numConnected = connectedInputs();
			pFile->Write(&numConnected,sizeof(int));							// number of Incoming connections
			numConnected = connectedOutputs();
			pFile->Write(&numConnected,sizeof(int));						// number of Outgoing connections
			for(int i = 0; i < MAX_CONNECTIONS; i++)
			{
				bool in;
				bool out;
				int unused=-1;
				if (inWires[i].Enabled()) {
					in=true;
					pFile->Write(&inWires[i].GetSrcMachine()._macIndex,sizeof(int));	// Incoming connections Machine number
				}
				else {
					in=false;
					pFile->Write(&unused,sizeof(int));	// Incoming connections Machine number
				}
				if (i < outWires.size() && outWires[i] && outWires[i]->Enabled()) {
					out=true;
					pFile->Write(&outWires[i]->GetDstMachine()._macIndex,sizeof(int));	// Outgoing connections Machine number
				}
				else {
					out=false;
					pFile->Write(&unused,sizeof(int));	// Outgoing connections Machine number
				}
				float volume; volume = inWires[i].GetVolume();
				float volMultiplier; volMultiplier = inWires[i].GetVolMultiplier();
				volume /= volMultiplier;
				pFile->Write(&volume,sizeof(float));	// Incoming connections Machine vol
				pFile->Write(&volMultiplier,sizeof(float));	// Value to multiply _inputConVol[] to have a 0.0...1.0 range
				pFile->Write(&out,sizeof(out));      // Outgoing connections activated
				pFile->Write(&in,sizeof(in));		// Incoming connections activated
			}
			std::string tmpName = _editName;
			pFile->WriteString(tmpName);
			SaveSpecificChunk(pFile);
			SaveWireMapping(pFile);
			SaveParamMapping(pFile);
		}

		void Machine::SaveSpecificChunk(RiffFile* pFile) 
		{
			UINT count = GetNumParams();
			UINT size = sizeof(count)+(count*sizeof(int));
			pFile->Write(&size,sizeof(size));
			pFile->Write(&count,sizeof(count));
			for(UINT i = 0; i < count; i++)
			{
				int temp = GetParamValue(i);
				pFile->Write(&temp,sizeof(temp));
			}
		}

		void Machine::SaveDllNameAndIndex(RiffFile* pFile,int index)
		{
			CString str = GetDllName();
			char str2[256];
			if ( str.IsEmpty()) str2[0]=0;
			else strcpy(str2,str.Mid(str.ReverseFind('\\')+1));

			if (index != 0)
			{
				char idxtext[8];
				int divisor=16777216;
				idxtext[4]=0;
				for (int i=0; i < 4; i++)
				{
					int residue = index%divisor;
					idxtext[3-i]=index/divisor;
					index = residue;
					divisor=divisor/256;
				}
				strcat(str2,idxtext);
			}
			pFile->Write(&str2,strlen(str2)+1);
		}

		bool Machine::LoadWireMapping(RiffFile* pFile, int version)
		{
			int numWires = 0;
			for (int i(0);i<MAX_CONNECTIONS;i++) {
				if (legacyWires[i]._inputCon) numWires++;
			}
			for(int countWires=0; countWires < numWires; countWires++)
			{
				int wireIdx, numPairs;
				Wire::PinConnection::first_type src;
				Wire::PinConnection::second_type dst;
				pFile->Read(wireIdx);
				if(wireIdx >= MAX_CONNECTIONS) {
					//We cannot ensure correctness from now onwards.
					return false;
				}
				pFile->Read(numPairs);
				Wire::Mapping& pinMapping = legacyWires[wireIdx].pinMapping;
				pinMapping.reserve(numPairs);
				for(int j=0; j < numPairs; j++){
					pFile->Read(src);
					pFile->Read(dst);
					pinMapping.push_back(Wire::PinConnection(src,dst));
				}
			}
			return true;
		}
		bool Machine::SaveWireMapping(RiffFile* pFile)
		{
			for(int i = 0; i < MAX_CONNECTIONS; i++)
			{
				if (!inWires[i].Enabled()) {
					continue;
				}
				const Wire::Mapping& pinMapping = inWires[i].GetMapping();
				int numPairs=pinMapping.size();
				pFile->Write(i);
				pFile->Write(numPairs);
				for(int j=0; j <pinMapping.size(); j++) {
					const Wire::PinConnection &pin = pinMapping[j];
					pFile->Write(pin.first);
					pFile->Write(pin.second);
				}
			}
			return true;
		}

		bool Machine::LoadParamMapping(RiffFile* pFile, int version)
		{
			uint8_t nummaps=0;
			if (!pFile->Expect("PMAP",4)) {
				return false;
			}
			pFile->Read(nummaps);
			for(int i=0; i < nummaps; i++)
			{
				uint8_t idx;
				uint16_t value;
				pFile->Read(idx);
				pFile->Read(value);
				set_virtual_param_index(idx,value);
			}
			return true;
		}

		bool Machine::SaveParamMapping(RiffFile* pFile)
		{
			uint8_t numMaps=0;
			for (int i=0;i<256;i++)
			{
				const int param = translate_param(i);
				if ( param < GetNumParams()) {
					numMaps++;
				}
			}
			pFile->Write("PMAP",4);
			pFile->Write(numMaps);
			for (int i=0;i<256;i++)
			{
				const int param = translate_param(i);
				if ( param < GetNumParams()) {
					const uint8_t idx = static_cast<uint8_t>(i);
					const uint16_t value = static_cast<uint16_t>(param);
					pFile->Write(idx);
					pFile->Write(value);
				}
			}
			return true;
		}

		void Machine::PostLoad(Machine** _pMachine)
		{
			for (int c(0) ; c < MAX_CONNECTIONS ; ++c)
			{
				LegacyWire& wire = legacyWires[c];
				//load bugfix: Ensure no duplicate wires could be created.
				for(int f=0;f<c;f++) {
					if (wire._inputCon && legacyWires[f]._inputCon && 
						wire._inputMachine==legacyWires[f]._inputMachine) {
							wire._inputCon=false;
					}
				}
				if (wire._inputCon
					&& wire._inputMachine >= 0 	&& wire._inputMachine < MAX_MACHINES
					&& _macIndex != wire._inputMachine && _pMachine[wire._inputMachine])
				{
					//Do not create the hidden wire from mixer send to the send machine.
					int outWire = FindLegacyOutput(_pMachine[wire._inputMachine], _macIndex);
					if (outWire != -1) {
						if (wire.pinMapping.size() > 0) {
							inWires[c].ConnectSource(*_pMachine[wire._inputMachine],0
								,FindLegacyOutput(_pMachine[wire._inputMachine], _macIndex)
								,&wire.pinMapping);
						}
						else {
							inWires[c].ConnectSource(*_pMachine[wire._inputMachine],0
								,FindLegacyOutput(_pMachine[wire._inputMachine], _macIndex));
						}
						while (wire._inputConVol*wire._wireMultiplier > 8.0f) { //psycle 1.10.1 alpha bugfix
							wire._inputConVol/=32768.f;
						}
						while (wire._inputConVol > 0.f && wire._inputConVol*wire._wireMultiplier < 0.0002f) { //psycle 1.10.1 alpha bugfix
							wire._inputConVol*=32768.f;
						}
						inWires[c].SetVolume(wire._inputConVol*wire._wireMultiplier);
					}
				}
			}
		}
		int Machine::FindLegacyOutput(Machine* sourceMac, int macIndex)
		{
			for (int c=0; c<MAX_CONNECTIONS; c++)
			{
				if (sourceMac->legacyWires[c]._connection &&
					sourceMac->legacyWires[c]._outputMachine == macIndex)
				{
					return c;
				}
			}
			return -1;
		}


		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Master



		float * Master::_pMasterSamples = 0;

		Master::Master(int index)
		{
			_macIndex = index;
			_outDry = 256;
			decreaseOnClip=false;
			_type = MACH_MASTER;
			_mode = MACHMODE_MASTER;
			strncpy(_editName, _psName, sizeof(_editName)-1);
			_editName[sizeof(_editName)-1]='\0';
			volumeDisplayLeft=0;
			volumeDisplayRight=0;
			InitializeSamplesVector();
		}

		void Master::Init(void)
		{
			Machine::Init();
			currentpeak=0.0f;
			currentrms=0.0f;
			peaktime=1;
			_lMax = 0.f;
			_rMax = 0.f;
			vuupdated = false;
			_clip = false;
		}

		int Master::GenerateAudio(int numSamples, bool measure_cpu_usage)
		{
			float mv = helpers::value_mapper::map_256_1(_outDry);
				
			float *pSamples = _pMasterSamples;
			float *pSamplesL = getLeft();
			float *pSamplesR = getRight();
			
			if(vuupdated)
			{ 
				// Auto decrease effect for the Master peak vu-meters (rms one is always showing the current value)
				_lMax *= 0.707f; 
				_rMax *= 0.707f; 
			}
			int i = numSamples;
			if(decreaseOnClip)
			{
				do
				{
					// Left channel
					if(std::fabs(*pSamples = *pSamplesL = *pSamplesL * mv) > _lMax)
					{
						_lMax = fabsf(*pSamplesL);
					}
					if(*pSamples > 32767.0f)
					{
						_outDry = helpers::math::round<int,float>((float)_outDry * 32767.0f / (*pSamples));
						mv = helpers::value_mapper::map_256_1(_outDry);
						*pSamples = *pSamplesL = 32767.0f; 
					}
					else if (*pSamples < -32767.0f)
					{
						_outDry = helpers::math::round<int,float>((float)_outDry * -32767.0f / (*pSamples));
						mv = helpers::value_mapper::map_256_1(_outDry);
						*pSamples = *pSamplesL = -32767.0f; 
					}
					pSamples++;
					pSamplesL++;
					// Right channel
					if(std::fabs(*pSamples = *pSamplesR = *pSamplesR * mv) > _rMax)
					{
						_rMax = fabsf(*pSamplesR);
					}
					if(*pSamples > 32767.0f)
					{
						_outDry = helpers::math::round<int,float>((float)_outDry * 32767.0f / (*pSamples));
						mv = helpers::value_mapper::map_256_1(_outDry);
						*pSamples = *pSamplesR = 32767.0f; 
					}
					else if (*pSamples < -32767.0f)
					{
						_outDry = helpers::math::round<int,float>((float)_outDry * -32767.0f / (*pSamples));
						mv = helpers::value_mapper::map_256_1(_outDry);
						*pSamples = *pSamplesR = -32767.0f; 
					}
					pSamples++;
					pSamplesR++;
				}
				while (--i);
			}
			else
			{
				do
				{
					// Left channel
					if(std::fabs( *pSamples++ = *pSamplesL = *pSamplesL * mv) > _lMax)
					{
						_lMax = fabsf(*pSamplesL);
					}
					pSamplesL++;
					// Right channel
					if(std::fabs(*pSamples++ = *pSamplesR = *pSamplesR * mv) > _rMax)
					{
						_rMax = fabsf(*pSamplesR);
					}
					pSamplesR++;
				}
				while (--i);
			}
			UpdateVuAndStanbyFlag(numSamples);
			if(_lMax > 32767.0f)
			{
				_clip=true;
			}
			else if (_lMax < 0.f) { _lMax = 0.f; }
			if(_rMax > 32767.0f)
			{
				_clip=true;
			}
			else if(_rMax < 0.f) { _rMax = 0.f; }
			if( _lMax > currentpeak ) currentpeak = _lMax;
			if( _rMax > currentpeak ) currentpeak = _rMax;
			
			float maxrms = std::max(rms.previousLeft, rms.previousRight);
			if(maxrms > currentrms) currentrms = maxrms;

			return numSamples;
		}

		void Master::UpdateVuAndStanbyFlag(int numSamples)
		{
#if PSYCLE__CONFIGURATION__RMS_VUS
			helpers::dsp::GetRMSVol(rms,getLeft(),getRight(),numSamples);
			float volumeLeft = rms.previousLeft*(1.f/GetAudioRange());
			float volumeRight = rms.previousRight*(1.f/GetAudioRange());
			//Transpose scale from -40dbs...0dbs to 0 to 97pix. (actually 100px)
			int temp(helpers::math::round<int,float>((50.0f * log10f(volumeLeft)+100.0f)));
			// clip values
			if(temp > 97) temp = 97;
			if(temp > 0)
			{
				volumeDisplayLeft = temp;
			}
			else if (volumeDisplayLeft>1 ) volumeDisplayLeft -=2;
			else {volumeDisplayLeft = 0;}
			temp = helpers::math::round<int,float>((50.0f * log10f(volumeRight)+100.0f));
			// clip values
			if(temp > 97) temp = 97;
			if(temp > 0)
			{
				volumeDisplayRight = temp;
			}
			else if (volumeDisplayRight>1 ) volumeDisplayRight -=2;
			else {volumeDisplayRight = 0;}
			_volumeDisplay = std::max(volumeDisplayLeft,volumeDisplayRight);
#else
			_volumeCounter = helpers::dsp::GetMaxVol(_pSamplesL, _pSamplesR, numSamples)*(1.f/GetAudioRange());
			//Transpose scale from -40dbs...0dbs to 0 to 97pix. (actually 100px)
			int temp(helpers::math::round<int,float>((50.0f * log10f(_volumeCounter)+100.0f)));
			// clip values
			if(temp > 97) temp = 97;
			if(temp > _volumeDisplay) _volumeDisplay = temp;
			if (_volumeDisplay>0 )--_volumeDisplay;
			//Cannot calculate the volume display with GetMaxVol method.
			volumeDisplayLeft = _volumeDisplay;
			volumeDisplayRight = _volumeDisplay;
#endif
		}
		bool Master::LoadSpecificChunk(RiffFile* pFile, int version)
		{
			UINT size;
			pFile->Read(&size, sizeof size ); // size of this part params to load
			pFile->Read(&_outDry,sizeof _outDry);
			pFile->Read(&decreaseOnClip, sizeof decreaseOnClip);
			return true;
		}

		void Master::SaveSpecificChunk(RiffFile* pFile)
		{
			UINT size = sizeof _outDry + sizeof decreaseOnClip;
			pFile->Write(&size, sizeof size); // size of this part params to save
			pFile->Write(&_outDry,sizeof _outDry);
			pFile->Write(&decreaseOnClip, sizeof decreaseOnClip); 
		}
		
	

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// old file format vomit. don't look at it!



		// old file format vomit. don't look at it!
		bool Machine::Load(RiffFile* pFile)
		{
			pFile->Read(&_editName,16);
			_editName[15] = 0;

			legacyWires.resize(MAX_CONNECTIONS);
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputMachine,sizeof(legacyWires[i]._inputMachine));	// Incoming connections Machine number
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._outputMachine,sizeof(legacyWires[i]._outputMachine));	// Outgoing connections Machine number
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputConVol,sizeof(legacyWires[i]._inputConVol));	// Incoming connections Machine vol
				legacyWires[i]._wireMultiplier = 1.0f;
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._connection,sizeof(legacyWires[i]._connection));      // Outgoing connections activated
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputCon,sizeof(legacyWires[i]._inputCon));		// Incoming connections activated
			}
			pFile->Read(&_connectionPoint[0], sizeof(_connectionPoint));
			pFile->Skip(2*sizeof(int)); // numInputs and numOutputs

			pFile->Read(&_panning, sizeof(_panning));
			Machine::SetPan(_panning);
			pFile->Skip(109);

			return true;
		}

		// old file format vomit. don't look at it!
		bool Master::Load(RiffFile* pFile)
		{
			pFile->Read(&_editName,16);
			_editName[15] = 0;
			legacyWires.resize(MAX_CONNECTIONS);
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputMachine,sizeof(legacyWires[i]._inputMachine));	// Incoming connections Machine number
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._outputMachine,sizeof(legacyWires[i]._outputMachine));	// Outgoing connections Machine number
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputConVol,sizeof(legacyWires[i]._inputConVol));	// Incoming connections Machine vol
				legacyWires[i]._wireMultiplier = 1.0f;
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._connection,sizeof(legacyWires[i]._connection));      // Outgoing connections activated
			}
			for(int i = 0; i < MAX_CONNECTIONS; i++) {
				pFile->Read(&legacyWires[i]._inputCon,sizeof(legacyWires[i]._inputCon));		// Incoming connections activated
			}
			pFile->Read(&_connectionPoint[0], sizeof(_connectionPoint));
			pFile->Skip(2*sizeof(int)); // numInputs and numOutputs

			pFile->Read(&_panning, sizeof(_panning));
			Machine::SetPan(_panning);
			pFile->Skip(40);

			pFile->Read(&_outDry, sizeof(int)); // outdry
			pFile->Skip(65);

			return true;
		}

   
	}
}

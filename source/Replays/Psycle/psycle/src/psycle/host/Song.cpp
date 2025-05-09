///\file
///\brief implementation file for psycle::host::Song.

#include <psycle/host/detail/project.private.hpp>
#include "Song.hpp"
#include "machineloader.hpp"

#if 0//!defined WINAMP_PLUGIN
	#include "PsycleConfig.hpp"
	#include "ProgressDialog.hpp"
	#include "InputHandler.hpp"
	#include <psycle/helpers/riff.hpp> // for Wave file loading.
	#include <psycle/helpers/amigasvx.hpp> // for Wave file loading.
	#include <psycle/helpers/appleaiff.hpp> // for Wave file loading.
	#include <psycle/helpers/riffwave.hpp> // for Wave file loading.
#else
	#include "Configuration.hpp"
	#include "player_plugins/winamp/fake_progressDialog.hpp"
#endif //!defined WINAMP_PLUGIN

#include "Machine.hpp" // It wouldn't be needed, since it is already included in "song.h"
#include "Sampler.hpp"
#include "XMSampler.hpp"
#include "Plugin.hpp"
#include "VstHost24.hpp"
#include "LuaPlugin.hpp"
#include "LuaHost.hpp"
#include "ladspahost.hpp"
#include "ladspamachine.h"

#include <psycle/helpers/datacompression.hpp>
#include <psycle/helpers/math.hpp>
#include <universalis/os/fs.hpp>
namespace loggers = universalis::os::loggers;
#include "convert_internal_machines.private.hpp"

#include "Zap.hpp"

namespace psycle
{
	namespace host
	{
		int Song::defaultPatLines = 64;

		/// Helper class for the PSY2 loader.
		class VSTLoader
		{
		public:
			bool valid;
			char dllName[128];
			int32_t numpars;
			float * pars;
		};

		void Song::SetDefaultPatLines(int lines)
		{
			defaultPatLines = lines;
		}
		bool Song::CreateMachine(MachineType type, int x, int y, char const* psPluginDll, int songIdx,int32_t shellIdx)
		{
			Machine* pMachine = CreateMachine(type, psPluginDll, songIdx, shellIdx);
			if (pMachine) {
				pMachine->_x = x;
				pMachine->_y = y;
				if(_pMachine[songIdx]) DestroyMachine(songIdx);
				_pMachine[songIdx] = pMachine;
				return true;
			}
			else {
				return false;
			}
		}
		Machine* Song::CreateMachine(MachineType type, char const* psPluginDll,int songIdx,int32_t shellIdx)
		{
			Machine* pMachine(0);
			Plugin* pPlugin(0);
			LuaPlugin *luaPlug(0);
			vst::Plugin *vstPlug(0);
			if(songIdx < 0 && (type != MACH_LUA || songIdx != AUTOID))
			{
				songIdx =	GetFreeMachine();
				if(songIdx < 0) return NULL;
			}
			switch (type)
			{
			case MACH_MASTER:
				if(_pMachine[MASTER_INDEX]) pMachine = _pMachine[MASTER_INDEX];
				else pMachine = new Master(songIdx);
				break;
			case MACH_SAMPLER:
				pMachine = new Sampler(songIdx);
				break;
			case MACH_XMSAMPLER:
				pMachine = new XMSampler(songIdx);
				break;
			case MACH_DUPLICATOR:
				pMachine = new DuplicatorMac(songIdx);
				break;
			case MACH_DUPLICATOR2:
				pMachine = new DuplicatorMac2(songIdx);
				break;
			case MACH_LUA:
				{
					if(Global::machineload().TestFilename(psPluginDll,shellIdx))
					{
						try
						{
							pMachine = luaPlug = dynamic_cast<LuaPlugin*>(new LuaPlugin(psPluginDll,songIdx));
						}
						catch(const std::exception& e)
						{
							loggers::exception()(e.what());
							zapObject(pMachine); 
						}
						catch(...)
						{
							zapObject(pMachine); 
						}
					}
					break;
				}
				case MACH_LADSPA:
				{
					if(Global::machineload().TestFilename(psPluginDll,shellIdx))
					{
						try
						{
							pMachine =  dynamic_cast<LADSPAMachine*>(LadspaHost::LoadPlugin(psPluginDll,songIdx, shellIdx));
						}
						catch(const std::exception& e)
						{
							loggers::exception()(e.what());
							zapObject(pMachine); 
						}
						catch(...)
						{
							zapObject(pMachine); 
						}
					}
					break;
				}
			case MACH_MIXER:
				pMachine = new Mixer(songIdx);
				break;
			case MACH_RECORDER:
				pMachine = new AudioRecorder(songIdx);
				break;
			case MACH_PLUGIN:
				{
					if(Global::machineload().TestFilename(psPluginDll,shellIdx))
					{
						try
						{
							pMachine = pPlugin = new Plugin(songIdx);
							pPlugin->Instance(psPluginDll);
						}
						catch(const std::exception& e)
						{
							loggers::exception()(e.what());
							zapObject(pMachine); 
						}
						catch(...)
						{
							zapObject(pMachine); 
						}
					}
					break;
				}
			case MACH_VST:
			case MACH_VSTFX:
				{
					if(Global::machineload().TestFilename(psPluginDll,shellIdx)) 
					{
						try
						{
							pMachine = vstPlug = dynamic_cast<vst::Plugin*>(Global::vsthost().LoadPlugin(psPluginDll,shellIdx));
							if(vstPlug)
							{
								vstPlug->_macIndex=songIdx;
							}
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
							zapObject(pMachine); 
						}
						catch(...)
						{
#ifndef NDEBUG 
							throw;
#else
							loggers::exception()("unknown exception");
							zapObject(pMachine);
#endif
						}
					}
					break;
				}
			case MACH_DUMMY:
				pMachine = new Dummy(songIdx);
				break;
			default:
				break;
			}
			if (pMachine) {
				pMachine->Init();
			}			
			return pMachine;
		}


		Song::Song()
			:semaphore(2,2,NULL,NULL)			
		{
			for(int i(0) ; i < MAX_PATTERNS; ++i) ppPatternData[i] = NULL;
			for(int i(0) ; i < MAX_MACHINES; ++i) _pMachine[i] = NULL;
			CreateNewPattern(0);
			for(int i(0) ; i < MAX_INSTRUMENTS ; ++i) _pInstrument[i] = new Instrument();
			Reset();
		}

		Song::~Song()
		{
			DestroyAllMachines();
			DestroyAllInstruments();
			DeleteAllPatterns();
		}

		bool Song::ReplaceMachine(Machine* origmac, MachineType type, int x, int y, char const* psPluginDll, int songIdx,int32_t shellIdx)
		{
			CExclusiveLock lock(&semaphore, 2, true);
			assert(origmac);

			Machine* newmac = CreateMachine(type,psPluginDll,songIdx,shellIdx);
			// replace all the connection info
			if (newmac)
			{
				// Rewire standard inputs/outputs.
				for (int i = 0; i < MAX_CONNECTIONS; i++)
				{
					if (origmac->inWires[i].Enabled()) {
						origmac->inWires[i].ChangeDest(newmac->inWires[i]);
					}
					if (origmac->outWires[i] && origmac->outWires[i]->Enabled()) {
						Machine* dstMac = &origmac->outWires[i]->GetDstMachine();
						int wiredest = origmac->outWires[i]->GetDstWireIndex();
						if ( dstMac->_type == MACH_MIXER && wiredest >= MAX_CONNECTIONS)
						{
							if (((Mixer*)dstMac)->ReturnValid(wiredest-MAX_CONNECTIONS))
							{
								Wire & oldwire = ((Mixer*)dstMac)->Return(wiredest-MAX_CONNECTIONS).GetWire();
								oldwire.ChangeSource(*newmac,0,i, &oldwire.GetMapping());
							}
						}
						else {
							origmac->outWires[i]->ChangeSource(*newmac,0,i,	&origmac->outWires[i]->GetMapping());
						}
					}
				}
				//In case of mixer, rewire send/returns too
				if (type == MACH_MIXER) {
					for (int i = 0; i < MAX_CONNECTIONS; i++)
					{
						if (((Mixer*)origmac)->ReturnValid(i))
						{
							Mixer &mixer = *static_cast<Mixer*>(newmac);
							mixer.InsertReturn(i);
							Wire & oldwire = ((Mixer*)origmac)->Return(i).GetWire();
							oldwire.ChangeDest(mixer.Return(i).GetWire());
						}
					}
				}

				if(_pMachine[songIdx]) DestroyMachine(songIdx);
				_pMachine[songIdx] = newmac;
				newmac->_x = x;
				newmac->_y = y;
				return true;
			}

			return false;
		}

		bool Song::ExchangeMachines(int one, int two)
		{
			CExclusiveLock lock(&semaphore, 2, true);
			Machine *mac1 = _pMachine[one];
			Machine *mac2 = _pMachine[two];

			///\todo: This has been added as a safety measure. This method with the mixer (and multi-io) does not work.
			if( (mac1  && (mac1->_isMixerSend || mac1->_type == MACH_MIXER))
				|| (mac2 && (mac2->_isMixerSend || mac2->_type == MACH_MIXER)))
			{
				char buf[128];
				std::sprintf(buf,"Cannot exchange the mixer with any other machine, or any send effect of the mixer");
				//MessageBox(0, buf, "Exchange Machine", 0);
				psycle::host::Global::pLogCallback(buf, "Exchange Machine");
				return false;
			}
			// if they are both valid
			if (mac1 && mac2)
			{
				std::vector<Wire> inWire1;
				std::vector<int> inSrcWireIdx1;
				std::vector<Wire> inWire2;
				std::vector<int> inSrcWireIdx2;
				std::vector<Wire*> outWire1;
				std::vector<Wire*> outWire2;
				Wire unused(mac1);
				for (int i = 0; i < MAX_CONNECTIONS; i++)
				{
					inWire1.push_back(mac1->inWires[i]);
					inSrcWireIdx1.push_back(mac1->inWires[i].GetSrcWireIndex());
					inWire2.push_back(mac2->inWires[i]);
					inSrcWireIdx2.push_back(mac2->inWires[i].GetSrcWireIndex());
					mac1->inWires[i].Disconnect();
					mac2->inWires[i].Disconnect();
					if (mac1->outWires[i]) {
						outWire1.push_back(mac1->outWires[i]);
						mac1->outWires[i]->Disconnect();
					}
					else {
						outWire1.push_back(&unused);
					}
					if (mac2->outWires[i]) {
						outWire2.push_back(mac2->outWires[i]);
						mac2->outWires[i]->Disconnect();
					}
					else {
						outWire2.push_back(&unused);
					}
				}

				// Finally, Rewire.
				for (int i = 0; i < MAX_CONNECTIONS; i++)
				{
					if (inWire1[i].Enabled()) {
						if (inWire1[i].GetSrcMachine()._macIndex == mac2->_macIndex) {
							// in this case, we cannot guarantee the correct wire index. we need
							// to check that it's one not included in inSrcWireIdx2
							int candidate=-1;
							for(int i=0; i< MAX_CONNECTIONS && candidate==-1; i++) {
								for(int j=0; j< MAX_CONNECTIONS; j++) {
									if (inSrcWireIdx2[j]==i) {
										break;
									}
									else if (j+1==MAX_CONNECTIONS) {
										candidate=i;
									}
								}
							}
							if (candidate!=-1) {
								mac2->inWires[i].ConnectSource(*mac1,0
									,candidate
									,&inWire1[i].GetMapping());
							}
						}
						else {
							mac2->inWires[i].ConnectSource(inWire1[i].GetSrcMachine(), 0
								,inSrcWireIdx1[i]
								,&inWire1[i].GetMapping());
						}
						mac2->inWires[i].SetVolume(inWire1[i].GetVolume());
					}
					if (inWire2[i].Enabled()) {
						if (inWire2[i].GetSrcMachine()._macIndex == mac1->_macIndex) {
							// in this case, we cannot guarantee the correct wire index. we need
							// to check that it's one not included in inSrcWireIdx1
							int candidate=-1;
							for(int i=0; i< MAX_CONNECTIONS && candidate==-1; i++) {
								for(int j=0; j< MAX_CONNECTIONS; j++) {
									if (inSrcWireIdx1[j]==i) {
										break;
									}
									else if (j+1==MAX_CONNECTIONS) {
										candidate=i;
									}
								}
							}
							if (candidate!=-1) {
								mac1->inWires[i].ConnectSource(*mac2,0
									,candidate
									,&inWire2[i].GetMapping());
							}
						}
						else {
							mac1->inWires[i].ConnectSource(inWire2[i].GetSrcMachine(), 0
								,inSrcWireIdx2[i]
								,&inWire2[i].GetMapping());
						}
						mac1->inWires[i].SetVolume(inWire2[i].GetVolume());
					}
					if (outWire1[i] != &unused) {
						if (outWire1[i]->GetDstMachine()._macIndex != mac2->_macIndex) {
							outWire1[i]->ChangeSource(*mac2, 0, i, &outWire1[i]->GetMapping());
						}
					}
					if (outWire2[i] != &unused) {
						if (outWire2[i]->GetDstMachine()._macIndex != mac1->_macIndex) {
							outWire2[i]->ChangeSource(*mac1, 0, i, &outWire2[i]->GetMapping());
						}
					}
				}

				// exchange positions
				int temp = mac1->_x;
				mac1->_x = mac2->_x;
				mac2->_x = temp;
				temp = mac1->_y;
				mac1->_y = mac2->_y;
				mac2->_y = temp;

				// Exchange the Machine number.
				_pMachine[one] = mac2;
				_pMachine[two] = mac1;
				mac1->_macIndex = two;
				mac2->_macIndex = one;
				return true;
			}
			else if (mac1)
			{
				// ok we gotta swap this one for a null one
				_pMachine[one] = NULL;
				_pMachine[two] = mac1;

				mac1->_macIndex = two;
				return true;
			}
			else if (mac2)
			{
				// ok we gotta swap this one for a null one
				_pMachine[one] = mac2;
				_pMachine[two] = NULL;

				mac2->_macIndex = one;
				return true;
			}
			return false;
		}

		void Song::DestroyAllMachines()
		{
			for(int c(0) ;  c < MAX_MACHINES; ++c)
			{
				if(_pMachine[c])
				{
					for(int j(c + 1) ; j < MAX_MACHINES; ++j)
					{
						if(_pMachine[c] == _pMachine[j])
						{
							///\todo wtf? duplicate machine? could happen if loader messes up?
							char buf[128];
							std::sprintf(buf,"%d and %d have duplicate pointers", c, j);
							//MessageBox(0, buf, "Duplicate Machine", 0);
							psycle::host::Global::pLogCallback(buf, "Duplicate Machine");
							_pMachine[j] = 0;
						}
					}
					DestroyMachine(c);
				}
				_pMachine[c] = 0;
			}
		}
		void Song::StopInstrument(int instrumentIdx)
		{
			for(int i=0; i< MAX_MACHINES; i++) {
				Machine* mac = _pMachine[i];
				//TODO: also for sampulse
				if(mac && mac->_type == MACH_SAMPLER) {
					Sampler& sam = *((Sampler*)mac);
					sam.StopInstrument(instrumentIdx);
				}
			}

		}

		void Song::ExchangeInstruments(int one, int two)
		{
			CExclusiveLock lock(&semaphore, 2, true);

			Instrument * tmpins;
			tmpins=_pInstrument[one];
			_pInstrument[one]=_pInstrument[two];
			_pInstrument[two]=tmpins;
			samples.ExchangeSamples(one,two);
		}

		void Song::DeleteInstruments()
		{
			for(int i(0) ; i < MAX_INSTRUMENTS ; ++i) DeleteInstrument(i);
		}

		void Song::DestroyAllInstruments()
		{
			for(int i(0) ; i < MAX_INSTRUMENTS ; ++i) zapObject(_pInstrument[i]);
			xminstruments.Clear();
			samples.Clear();
		}

		void Song::DeleteInstrument(int i)
		{
			_pInstrument[i]->Init();
			if (samples.IsEnabled(i)){
				samples.get(i).Init();
			}
			DeleteVirtualOfInstrument(i,false);
		}

		void Song::Reset()
		{
			srand(time(NULL));
			// Cleaning pattern allocation info
			samples.Clear();
			xminstruments.Clear();
			virtualInst.clear();
			virtualInstInv.clear();
			for(int i(0) ; i < MAX_MACHINES ; ++i)
			{
				zapObject(_pMachine[i]);
			}

			for(int i(0) ; i < MAX_PATTERNS; ++i)
			{
				// All pattern reset
				patternLines[i]=defaultPatLines;
				std::sprintf(patternName[i], "Untitled"); 
				for(int j(0) ;  j < MAX_TRACKS; j++) {
					_trackNames[i][j] = "";
				}
			}
			_trackArmedCount = 0;
			for(int i(0) ; i < MAX_TRACKS; ++i)
			{
				_trackMuted[i] = false;
				_trackArmed[i] = false;
			}
			machineSoloed = -1;
			_trackSoloed = -1;
			playLength=1;
			for(int i(0) ; i < MAX_SONG_POSITIONS; ++i)
			{
				playOrder[i]=0; // All pattern reset
				playOrderSel[i]=false;
			}
			playOrderSel[0]=true;
			//Guarantee some defaults (Player needs at least m_TicksPerBeat before Song::New())
			currentOctave=4;
			m_BeatsPerMin=125;
			m_LinesPerBeat=4;
			m_TicksPerBeat=24;
			m_ExtraTicksPerLine=0;
			waveSelected = 0;
			instSelected = 0;
			paramSelected = 0;
			auxcolSelected = 0;
		}

		void Song::New()
		{
			CExclusiveLock lock(&semaphore, 2, true);
			DoNew();
		}

		void Song::DoNew() {
			seqBus=0;
			shareTrackNames=true;
			// Song reset
			name = "Untitled";
			author = "Unnamed";
			comments = "";
			currentOctave=4;
			// General properties
			m_BeatsPerMin=125;
			m_LinesPerBeat=4;
			m_TicksPerBeat=24;
			m_ExtraTicksPerLine=0;
			// Clean up allocated machines.
			DestroyAllMachines();
			// Cleaning instruments
			DeleteInstruments();
			// Clear patterns
			DeleteAllPatterns();
			// Clear sequence
			Reset();
			waveSelected = 0;
			instSelected = 0;
			paramSelected = 0;
			auxcolSelected = 0;
			_saved=false;
			fileName ="Untitled.psy";
			CreateMachine(MACH_MASTER, 320, 200, 0, MASTER_INDEX);
		}

		void Song::ChangeTrackName(int patIdx, int trackidx, std::string name)
		{
			if(shareTrackNames)
			{
				for(int i(0); i < MAX_PATTERNS; i++) {
					_trackNames[i][trackidx] = name;
				}
			}
			else 
			{
				_trackNames[patIdx][trackidx] = name;
			}
		}
		void Song::SetTrackNameShareMode(bool shared)
		{
			if(shared) {
				for(int i(1); i < MAX_PATTERNS; i++) {
					for(int j(0); j < SONGTRACKS; j++) {
						_trackNames[i][j] = _trackNames[i][j];
					}
				}
			}
		}
		void Song::CopyNamesFrom(int patOrig,int patDest)
		{
			for(int i(0); i< SONGTRACKS; i++) {
				_trackNames[patDest][i] = _trackNames[patOrig][i];
			}
		}

		int Song::GetFreeMachine() const
		{
			int idx=-1;
			for(int tmac = 0; tmac < MAX_MACHINES; tmac++)
			{
				if(!_pMachine[tmac]) {
					idx=tmac;
					break;
				}
			}
			return idx;
		}
		bool Song::ValidateMixerSendCandidate(Machine& mac,bool rewiring)
		{
			// Basically, we dissallow a send comming from a generator as well as multiple-outs for sends.
			if ( mac._mode == MACHMODE_GENERATOR) return false;
			if ( mac.connectedInputs() > 1 || (mac.connectedOutputs() > 0 && !rewiring) ) return false;
			for (int i(0); i<MAX_CONNECTIONS; ++i)
			{
				if (mac.inWires[i].Enabled())
				{
					if (!ValidateMixerSendCandidate(mac.inWires[i].GetSrcMachine(),true)) //true because obviously it has one output
					{
						return false;
					}
				}
			}
			return true;
		}

		int Song::InsertConnectionNonBlocking(Machine* srcMac,Machine* dstMac, int srctype, int dsttype,float initialVol)
		{
			// Assert that we have two machines
			assert(srcMac); assert(dstMac);
			// Verify that the destination is not a generator
			if(dstMac->_mode == MACHMODE_GENERATOR) return -1;
			// Verify that src is not connected to dst already, and that destination is not connected to source.
			if (srcMac->FindOutputWire(dstMac->_macIndex) > -1 || dstMac->FindOutputWire(srcMac->_macIndex) > -1) return -1;
			// disallow mixer as a sender of another mixer
			if ( srcMac->_type == MACH_MIXER && dstMac->_type == MACH_MIXER && dsttype != 0) return -1;
			// If source is in a mixer chain, dissallow the new connection (there can only be one output and at much one input).
			if ( srcMac->_isMixerSend ) return -1;
			// If destination is in a mixer chain (or the mixer itself), validate the sender first
			if ( dstMac->_isMixerSend || (dstMac->_type == MACH_MIXER && dsttype == 1))
			{
				if (!ValidateMixerSendCandidate(*srcMac)) return -1;
			}

			// Try to get free indexes from each machine
			int freebus=srcMac->GetFreeOutputWire(srctype);
			int dfreebus=dstMac->GetFreeInputWire(dsttype);
			if(freebus == -1 || dfreebus == -1 ) return -1;

			// If everything went right, connect them.
			//Patch for Mixer. These methods should really be using Wire objects now, not indexes.
			if (dstMac->_type == MACH_MIXER && dfreebus >= MAX_CONNECTIONS) {
				dfreebus-=MAX_CONNECTIONS;
				Mixer &mixer = *static_cast<Mixer*>(dstMac);
				mixer.InsertReturn(dfreebus);
				Wire &wire = mixer.Return(dfreebus).GetWire();
				wire.ConnectSource(*srcMac,srctype,freebus);
				wire.SetVolume(initialVol);
			}
			else {
				dstMac->inWires[dfreebus].ConnectSource(*srcMac,srctype,freebus);
				dstMac->inWires[dfreebus].SetVolume(initialVol);
			}
			return dfreebus;
		}
		bool Song::ChangeWireDestMacNonBlocking(Machine* srcMac,Machine* newdstMac, int wiresrc,int newwiredest)
		{
			// Assert that we have two machines
			assert(srcMac); assert(newdstMac);
			// Verify that the destination is not a generator
			if (newdstMac->_mode == MACHMODE_GENERATOR) return false;
			// Verify that src is not connected to dst already, and that destination is not connected to source.
			if (srcMac->FindOutputWire(newdstMac->_macIndex) > -1 || newdstMac->FindOutputWire(srcMac->_macIndex) > -1) return false;
			// If source and dest are a mixer chain, dissallow connection if connecting as send.
			if ( srcMac->_type == MACH_MIXER && newdstMac->_type == MACH_MIXER && newwiredest >=MAX_CONNECTIONS) return false;
			// If destination is in a mixer chain (or the mixer itself), validate the sender first
			if ( newdstMac->_isMixerSend || (newdstMac->_type == MACH_MIXER && newwiredest >= MAX_CONNECTIONS))
			{
				///\todo: validate for the case where srcMac->_isMixerSend
				if (!ValidateMixerSendCandidate(*srcMac,true)) return false;
			}

			if (wiresrc == -1 || newwiredest == -1 || srcMac->outWires[wiresrc] == NULL || srcMac->outWires[wiresrc]->Enabled() == false)
				return false;

			//Patch for Mixer. These methods should really be using Wire objects now, not indexes.
			if (newdstMac->_type == MACH_MIXER && newwiredest >= MAX_CONNECTIONS) {
				newwiredest-=MAX_CONNECTIONS;
				Mixer &mixer = *static_cast<Mixer*>(newdstMac);
				mixer.InsertReturn(newwiredest);
				srcMac->outWires[wiresrc]->ChangeDest(mixer.Return(newwiredest).GetWire());
			}
			else {
				srcMac->outWires[wiresrc]->ChangeDest(newdstMac->inWires[newwiredest]);
			}
			return true;
		}
		bool Song::ChangeWireSourceMacNonBlocking(Machine* newsrcMac,Machine* dstMac, int newwiresrc, int wiredest)
		{
			// Assert that we have two machines
			assert(newsrcMac); assert(dstMac);
			// Verify that the destination is not a generator
			if(dstMac->_mode == MACHMODE_GENERATOR) return false;
			// Verify that src is not connected to dst already, and that destination is not connected to source.
			if (newsrcMac->FindOutputWire(dstMac->_macIndex) > -1 || dstMac->FindOutputWire(newsrcMac->_macIndex) > -1) return false;
			// disallow mixer as a sender of another mixer
			if ( newsrcMac->_type == MACH_MIXER && dstMac->_type == MACH_MIXER && wiredest >= MAX_CONNECTIONS) return false;
			// If source is in a mixer chain, dissallow the new connection.
			if ( newsrcMac->_isMixerSend ) return false;
			// If destination is in a mixer chain (or the mixer itself), validate the sender first
			if ( dstMac->_isMixerSend || (dstMac->_type == MACH_MIXER && wiredest >= MAX_CONNECTIONS))
			{
				if (!ValidateMixerSendCandidate(*newsrcMac,false)) return false;
			}

			if (newwiresrc == -1 || wiredest == -1)
				return false;

			if ( dstMac->_type == MACH_MIXER && wiredest >= MAX_CONNECTIONS)
			{
				if (((Mixer*)dstMac)->ReturnValid(wiredest-MAX_CONNECTIONS))
				{
					Wire & oldwire = ((Mixer*)dstMac)->Return(wiredest-MAX_CONNECTIONS).GetWire();
					oldwire.ChangeSource(*newsrcMac,0,newwiresrc, &oldwire.GetMapping());
					return true;
				}
			}
			else if (dstMac->inWires[wiredest].Enabled()) {
				dstMac->inWires[wiredest].ChangeSource(*newsrcMac,0,newwiresrc,
					&dstMac->inWires[wiredest].GetMapping());
				return true;
			}
			return false;
		}

		void Song::DestroyMachine(int mac)
		{
			Machine *iMac = _pMachine[mac];
			if(iMac)
			{				
				iMac->DeleteWires();
				DeleteVirtualsOfMachine(mac);
				if(mac == machineSoloed) machineSoloed = -1;
				// If it's a (Vst)Plugin, the destructor calls to release the underlying library
#ifndef NDEBUG 
				zapObject(_pMachine[mac]);
#else
				try
				{
					zapObject(_pMachine[mac]);
				}catch(...){};
#endif
			}
		}
		void Song::SoloMachine(int macIdx)
		{
			if (macIdx >= 0 && macIdx < MAX_MACHINES && _pMachine[macIdx] != NULL 
				&& _pMachine[macIdx]->_mode == MACHMODE_GENERATOR)
			{
				for ( int i=0;i<MAX_BUSES;i++ )
				{
					if (_pMachine[i]) {
						_pMachine[i]->_mute = true;
						_pMachine[i]->_volumeCounter=0.0f;
						_pMachine[i]->_volumeDisplay =0;
					}
				}
				_pMachine[macIdx]->_mute = false;
				machineSoloed = macIdx;
			}
			else
			{
				machineSoloed = -1;
				for ( int i=0;i<MAX_BUSES;i++ )
				{
					if (_pMachine[i]) {	_pMachine[i]->_mute = false; }
				}
			}
		}
		void Song::DeleteAllPatterns()
		{
			SONGTRACKS = 16;
			for(int i=0; i<MAX_PATTERNS; i++) RemovePattern(i);
		}

		void Song::RemovePattern(int ps)
		{
			zapArray(ppPatternData[ps]);
		}

		unsigned char * Song::CreateNewPattern(int ps)
		{
			RemovePattern(ps);
			ppPatternData[ps] = new unsigned char[MULTIPLY2];
			PatternEntry blank;
			unsigned char * pData = ppPatternData[ps];
			for(int i = 0; i < MULTIPLY2; i+= EVENT_SIZE)
			{
				memcpy(pData,&blank,EVENT_SIZE);
				pData+= EVENT_SIZE;
			}

			return ppPatternData[ps];
		}

		bool Song::AllocNewPattern(int pattern,char *name,int lines,bool adaptsize)
		{
			PatternEntry blank;
			unsigned char *toffset;
			if(adaptsize)
			{
				float step;
				if( patternLines[pattern] > lines ) 
				{
					step= (float)patternLines[pattern]/lines;
					for(int t=0;t<SONGTRACKS;t++)
					{
						toffset=_ptrack(pattern,t);
						int l;
						for(l = 1 ; l < lines; ++l)
						{
							std::memcpy(toffset + l * MULTIPLY, toffset + helpers::math::round<int,float>(l * step) * MULTIPLY,EVENT_SIZE);
						}
						while(l < patternLines[pattern])
						{
							// This wouldn't be necessary if we really allocate a new pattern.
							std::memcpy(toffset + (l * MULTIPLY), &blank, EVENT_SIZE);
							++l;
						}
					}
					patternLines[pattern] = lines; ///< This represents the allocation of the new pattern
				}
				else if(patternLines[pattern] < lines)
				{
					step= (float)lines/patternLines[pattern];
					int nl= patternLines[pattern];
					for(int t=0;t<SONGTRACKS;t++)
					{
						toffset=_ptrack(pattern,t);
						for(int l=nl-1;l>0;l--)
						{
							std::memcpy(toffset + helpers::math::round<int,float>(l * step) * MULTIPLY, toffset + l * MULTIPLY,EVENT_SIZE);
							int tz(helpers::math::round<int,float>(l * step) - 1);
							while (tz > (l - 1) * step)
							{
								std::memcpy(toffset + tz * MULTIPLY, &blank, EVENT_SIZE);
								--tz;
							}
						}
					}
					patternLines[pattern] = lines; ///< This represents the allocation of the new pattern
				}
			}
			else
			{
				int l(patternLines[pattern]);
				while(l < lines)
				{
					// This wouldn't be necessary if we really allocate a new pattern.
					for(int t(0) ; t < SONGTRACKS ; ++t)
					{
						toffset=_ptrackline(pattern,t,l);
						memcpy(toffset,&blank,EVENT_SIZE);
					}
					++l;
				}
				patternLines[pattern] = lines; ///< This represents the allocation of the new pattern
			}
			std::sprintf(patternName[pattern], name);
			return true;
		}

		void Song::AddNewTrack(int pattern, int trackIdx) 
		{
			int first, last;
			PatternEntry blank;
			if(pattern == -1)  {
				first=0;
				last=0;
				for(int i=0;i<MAX_PATTERNS;i++) {
					if(IsPatternUsed(i)) {
						last=i;
					}
				}
			}
			else {
				first=pattern;
				last=pattern;
			}
			if(SONGTRACKS < MAX_TRACKS) {
				SONGTRACKS++;
			}
			for(int pat=first; pat <=last; pat++) {
				for(int line=0;line<patternLines[pat]; line++) {
					unsigned char *offset_source= ppPatternData[pat] + (line*MULTIPLY) + (trackIdx*EVENT_SIZE);

					memmove(offset_source+EVENT_SIZE,offset_source,(SONGTRACKS-1-trackIdx)*EVENT_SIZE);
					memcpy(offset_source,&blank,EVENT_SIZE);
				}
				for(int j(SONGTRACKS-1); j > trackIdx; j--) {
					_trackNames[pat][j] = _trackNames[pat][j-1];
				}
				_trackNames[pat][trackIdx] = "";
			}
		}
		int Song::GetHighestInstrumentIndex() const
		{
			return samples.lastused();
		}
		int Song::GetHighestXMInstrumentIndex() const
		{
			return xminstruments.lastused();
		}
		int Song::GetNumInstruments() const
		{
			int used=0;
			int size = samples.size();
			for( int i=0; i<size;i++) {
				if (samples.IsEnabled(i)) {
					used++;
				}
			}
			return used;
		}

		int Song::GetHighestPatternIndexInSequence() const
		{
			int rval(0);
			for(int c(0) ; c < playLength ; ++c) if(rval < playOrder[c]) rval = playOrder[c];
			return rval;
		}

		int Song::GetNumPatterns() const {
			int used=0;
			for(int i =0; i < MAX_PATTERNS; i++) {
				if(!IsPatternEmpty(i)) used++;
			}
			return used;
		}

		int Song::GetBlankPatternUnused(int rval)
		{
			//try to find an empty pattern.
			PatternEntry blank;
			bool bTryAgain(true);
			while(bTryAgain && rval < MAX_PATTERNS)
			{
				for(int c(0) ; c < playLength ; ++c)
				{
					if(rval == playOrder[c]) 
					{
						++rval;
						c = -1;
					}
				}
				// now test to see if data is really blank
				bTryAgain = false;
				if(rval < MAX_PATTERNS)
				{
					unsigned char *offset_source(ppPatternData[rval]);
					if (offset_source == NULL) {
						AllocNewPattern(rval,"",defaultPatLines,false);	
						break;
					}
					for(int t(0) ; t < MULTIPLY2 ; t += EVENT_SIZE)
					{
						if(memcmp(offset_source+t,&blank,EVENT_SIZE) != 0 )
						{
							++rval;
							bTryAgain = true;
							t = MULTIPLY2;
						}
					}
				}
			}
			//if none found, get one not in the sequence
			if(rval >= MAX_PATTERNS)
			{
				rval = 0;
				for(int c(0) ; c < playLength ; ++c)
				{
					if(rval == playOrder[c]) 
					{
						++rval;
						c = -1;
					}
				}
				if(rval >= MAX_PATTERNS) rval = MAX_PATTERNS - 1;
				AllocNewPattern(rval,"",defaultPatLines,false);	
			}
			return rval;
		}

		int Song::GetFreeBus() const
		{
			for(int c(0) ; c < MAX_BUSES ; ++c) if(!_pMachine[c]) return c;
			return -1; 
		}

		int Song::GetFreeFxBus() const
		{
			for(int c(MAX_BUSES) ; c < MAX_BUSES * 2 ; ++c) if(!_pMachine[c]) return c;
			return -1; 
		}
		Machine* Song::GetSamplerIfExists()
		{
			int inst=-1;
			Machine *tmac = GetMachineOfBus(seqBus, inst);
			if (tmac && tmac->_type != MACH_SAMPLER) {
				tmac=NULL;
				for (int i=0; i< MAX_BUSES; i++) {
					Machine *tmac2 = _pMachine[i];
					if (tmac2 != NULL && tmac2->_type == MACH_SAMPLER) {
						tmac = tmac2;
						break;
					}
				}
			}
			return tmac;
		}
		Machine* Song::GetSampulseIfExists()
		{
			int inst=-1;
			Machine *tmac = GetMachineOfBus(seqBus, inst);
			if (tmac && tmac->_type != MACH_XMSAMPLER) {
				tmac=NULL;
				for (int i=0; i< MAX_BUSES; i++) {
					Machine *tmac2 = _pMachine[i];
					if (tmac2 != NULL && tmac2->_type == MACH_XMSAMPLER) {
						tmac = tmac2;
						break;
					}
				}
			}
			return tmac;
		}
		Machine* Song::GetMachineOfBus(int bus, int& alternateinst) const
		{
			Machine * pmac = NULL;
			alternateinst = -1;
			if (bus < 0 ) { return NULL; }
			else if (bus < MAX_MACHINES) {
				pmac = _pMachine[bus];
			}
			else if (bus < MAX_VIRTUALINSTS){ 
				macinstpair instpair = VirtualInstrument(bus);
				if (instpair.first != -1) {
					pmac = _pMachine[instpair.first];
					alternateinst = instpair.second;
				}
			}
			return pmac;
		}

		std::string Song::GetVirtualMachineName(Machine* mac, int alternateins) const
		{
			std::string result;
			if (mac->_type == MACH_SAMPLER && samples.IsEnabled(alternateins)) {
				result = samples[alternateins].WaveName();
			}
			else if (mac->_type == MACH_XMSAMPLER && xminstruments.IsEnabled(alternateins)) {
				result = xminstruments[alternateins].Name();
			}
			return result;
		}

		void Song::SetVirtualInstrument(int virtidx, int macidx, int targetins) {
			//If this virtual instrument was already mapped, remove the association
			DeleteVirtualInstrument(virtidx);

			int dummy=-1;
			Machine * mac = GetMachineOfBus(macidx, dummy);
			if (mac == NULL || (mac->_type != MACH_SAMPLER && mac->_type != MACH_XMSAMPLER)) {
				return;
			}
			bool sampulse = (mac->_type == MACH_XMSAMPLER);

			//check if this targetins was already associated with another virtidx
			DeleteVirtualOfInstrument(targetins,sampulse);

			virtualInst[virtidx]=macinstpair(macidx,targetins);
			virtualInstInv[instsampulsepair(targetins,sampulse)]=virtidx;
		}
		void Song::DeleteVirtualInstrument(int virtidx)
		{
			std::map<int, macinstpair>::iterator it = virtualInst.find(virtidx);
			if(it != virtualInst.end()) {
				// Not checking the machine type directly to ensure correct behaviour even when the machine
				// has been deleted.
				int othervirt = VirtualInstrumentInverted(it->second.second,true);
				if (virtidx == othervirt)  {
					virtualInstInv.erase(instsampulsepair(it->second.second,true));
				}
				othervirt = VirtualInstrumentInverted(it->second.second,false);
				if (virtidx == othervirt)  {
					virtualInstInv.erase(instsampulsepair(it->second.second,false));
				}
				virtualInst.erase(it);
			}
		}
		void Song::DeleteVirtualOfInstrument(int inst, bool sampulse)
		{
			std::map<instsampulsepair,int>::const_iterator it = virtualInstInv.find(instsampulsepair(inst,sampulse));
			if(it != virtualInstInv.end()) {
				virtualInst.erase(it->second);
				virtualInstInv.erase(it);
			}
		}
		void Song::DeleteVirtualsOfMachine(int macidx)
		{
			for (std::map<int, macinstpair>::const_iterator it = virtualInst.begin(); it != virtualInst.end() ; )
			{
				if (it->second.first == macidx) {
					int othervirt = VirtualInstrumentInverted(it->second.second,true);
					if (it->first == othervirt)  {
						virtualInstInv.erase(instsampulsepair(it->second.second,true));
					}
					othervirt = VirtualInstrumentInverted(it->second.second,false);
					if (it->first == othervirt)  {
						virtualInstInv.erase(instsampulsepair(it->second.second,false));
					}
					virtualInst.erase(it++);
				}
				else {
					++it;
				}
			}
		}

		void Song::SetWavPreview(XMInstrument::WaveData<> * wave) {
			CExclusiveLock lock(&semaphore, 2, true);
			wavprev.UseWave(wave);
		}
		void Song::SetWavPreview(int newinst)
		{
			CExclusiveLock lock(&semaphore, 2, true);
			if(samples.Exists(newinst)) {
				wavprev.UseWave(&samples.get(newinst));
			}
			else {
				wavprev.UseWave(NULL);
			}
		}
		//returns -1 error, else sample idx.
		int Song::AIffAlloc(LoaderHelper& loadhelp,const std::string & sFileName)
		{
			int realsampleIdx = -1;
#if 0//!defined WINAMP_PLUGIN
			AppleAIFF file;
			file.Open(sFileName);
			if (!file.isValidFile()) { file.Close(); return -1; }
			boost::filesystem::path path(sFileName);
			name = path.filename().string();
			if (name.length() > 32) {
				name = name.substr(0,32);
			}

			uint32_t datalen= file.commchunk.numSampleFrames.unsignedValue();
			XMInstrument::WaveData<>& wave = WavAlloc(loadhelp,file.commchunk.numChannels.signedValue()==2,datalen,name, realsampleIdx);
			wave.WaveSampleRate(static_cast<uint32_t>(file.commchunk.sampleRate.value()));
			Loop loop = file.instchunk.sustainLoop;
			if ( loop.playMode.signedValue() > 0) {
				wave.WaveLoopStart(file.markers.Markers[loop.beginLoop.signedValue()].position.unsignedValue());
				wave.WaveLoopEnd(file.markers.Markers[loop.endLoop.signedValue()].position.unsignedValue());
				wave.WaveLoopType(loop.playMode.signedValue() == 1 ?
					XMInstrument::WaveData<>::LoopType::NORMAL :
					XMInstrument::WaveData<>::LoopType::BIDI);
			}
			loop = file.instchunk.releaseLoop;
			if ( loop.playMode.signedValue() > 0) {
				wave.WaveSusLoopStart(file.markers.Markers[loop.beginLoop.signedValue()].position.unsignedValue());
				wave.WaveSusLoopEnd(file.markers.Markers[loop.endLoop.signedValue()].position.unsignedValue());
				wave.WaveSusLoopType(loop.playMode.signedValue() == 1 ?
					XMInstrument::WaveData<>::LoopType::NORMAL :
					XMInstrument::WaveData<>::LoopType::BIDI);
			}
			if (file.instchunk.baseNote != 0) {
				wave.WaveTune(60-file.instchunk.baseNote);
				wave.WaveFineTune(file.instchunk.detune);
				wave.WaveGlobVolume(helpers::dsp::dB2Amp(file.instchunk.gain));
			}
			int16_t* samps[]={wave.pWaveDataL(), wave.pWaveDataR()};
			CommonChunk convert16;
			convert16.numChannels.changeValue(wave.IsWaveStereo()?static_cast<uint16_t>(2):static_cast<uint16_t>(1));
			convert16.sampleSize.changeValue(static_cast<uint16_t>(16));
			file.readDeInterleavedSamples(reinterpret_cast<void**>(samps),datalen,&convert16);
			file.Close();
#endif
			return realsampleIdx;
		}
		//instmode=true then create/initialize an instrument (both, sampler and sampulse ones).
		//returns -1 error, else if instmode= true returns real inst index, else returns real sample idx.
		int Song::IffAlloc(LoaderHelper& loadhelp, bool instMode, const std::string & sFileName)
		{
			int outputIdx=-1;
#if 0//!defined WINAMP_PLUGIN
			AmigaSvx file;
			file.Open(sFileName);
			if (!file.isValidFile()) { file.Close(); return -1; }
			std::string name = file.GetName();
			if (name.empty()) {
				boost::filesystem::path path(sFileName);
				name = path.filename().string();
			}
			if (name.length() > 32) {
				name = name.substr(0,32);
			}

			//This envelope runs in millis.
			XMInstrument::Envelope volenv;
			if (instMode) {
				std::list<helpers::EGPoint> svxEnv = file.getVolumeEnvelope();
				if (svxEnv.size() > 1) {
					// Adapting to psycle's envelope
					int runpos=0;
					volenv.SetValue(0,0.0f);
					for (std::list<helpers::EGPoint>::const_iterator ite = svxEnv.begin(); ite != svxEnv.end(); ++ite) {
						if (ite->duration.signedValue() == -1) { //Sustain point.
							volenv.SustainBegin(volenv.GetTime(volenv.NumOfPoints()-1));
							volenv.SustainEnd(volenv.GetTime(volenv.NumOfPoints()-1));
						}
						else { //Normal point
							runpos += ite->duration.unsignedValue();
							volenv.SetValue(runpos,ite->dest.value());
						}
					}
				}
			}

			std::list<XMInstrument::NotePair> notes;

			//when loading as sample, we load a single sample, so we skip until the last octave (better sound).
			int notestart=0;
			int noteend=XMInstrument::NOTE_MAP_SIZE;
			int startidx=(instMode)?1:file.getformat().numOctaves;
			for (int octave=startidx;octave<=file.getformat().numOctaves;octave++) {
				uint32_t datalen= file.getLength(octave);
				XMInstrument::WaveData<>& wave = WavAlloc(loadhelp,file.stereo(),datalen,name, outputIdx);
				wave.WaveSampleRate(file.getformat().samplesPerSec.unsignedValue());
				wave.WaveGlobVolume(file.getformat().volume.value());
				if (file.getPan() < 0.5f || file.getPan() > 0.5f) {
					wave.PanEnabled(true);
					wave.PanFactor(file.getPan());
				}
				if ( file.getformat().repeatHiSamples.unsignedValue() > 0) {
					wave.WaveLoopStart(file.getLoopStart(octave));
					wave.WaveLoopEnd(file.getLoopEnd(octave));
					wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
				}
				wave.WaveTune(file.getNoteOffsetFor(octave));

				int16_t * csamples = const_cast<int16_t*>(wave.pWaveDataL());
				if (file.stereo()) {
					int16_t * rsamples = const_cast<int16_t*>(wave.pWaveDataR());
					file.readStereoConvertTo16(csamples,rsamples,datalen, octave);
				}
				else {
					file.readMonoConvertTo16(csamples,datalen, octave);
				}

				noteend=(octave<file.getformat().numOctaves) 
					? std::min(notecommands::middleC + wave.WaveTune() + 12, XMInstrument::NOTE_MAP_SIZE)
					: XMInstrument::NOTE_MAP_SIZE;
				for(int j = notestart;j < noteend;j++){
					XMInstrument::NotePair npair;
					npair.first=j;
					npair.second=outputIdx;
					notes.push_back(npair);
				}
				notestart=noteend;
			}
			if (instMode) {
				XMInstrument& inst = loadhelp.GetNextInstrument(outputIdx);
				inst.Name(name);
				if (volenv.NumOfPoints() > 0) {
					inst.AmpEnvelope() = volenv;
				}
				for(std::list<XMInstrument::NotePair>::iterator ite =  notes.begin(); ite != notes.end(); ++ite) {
					inst.NoteToSample(ite->first, *ite);
				}
				inst.ValidateEnabled();

				std::set<int> list = inst.GetWavesUsed();
				for (std::set<int>::iterator ite =list.begin(); ite != list.end(); ++ite) {
					_pInstrument[*ite]->Init();
					if (volenv.NumOfPoints() > 0) {
						_pInstrument[*ite]->SetVolEnvFromEnvelope(volenv,tickstomillis(1));
					}
				}
			}
			file.Close();
#endif
			return outputIdx;
		}

		XMInstrument::WaveData<>& Song::WavAlloc(int sampleIdx, bool bStereo, uint32_t iSamplesPerChan, const std::string & sName)
		{
			assert(iSamplesPerChan<(1<<30)); ///< FIXME: Since in some places, signed values are used, we cannot use the whole range.
			XMInstrument::WaveData<> wavetmp;
			samples.SetSample(wavetmp,sampleIdx);
			XMInstrument::WaveData<>& wave = samples.get(sampleIdx);
			wave.Init();
			wave.AllocWaveData(iSamplesPerChan,bStereo);
			wave.WaveName(sName);
			return wave;
		}
		XMInstrument::WaveData<>& Song::WavAlloc(LoaderHelper& loadhelp, bool bStereo, uint32_t iSamplesPerChan, const std::string & sName, int &instout)
		{
			assert(iSamplesPerChan<(1<<30)); ///< FIXME: Since in some places, signed values are used, we cannot use the whole range.
			XMInstrument::WaveData<>& wave = loadhelp.GetNextSample(instout);
			wave.AllocWaveData(iSamplesPerChan,bStereo);
			wave.WaveName(sName);
			return wave;
		}

		//if sampleidx = -1 then allocate on free pos, if sampleidx = PREV_WAV_INS use wav ins.
		//returns -1 error, else sample idx.
		int Song::WavAlloc(LoaderHelper& loadhelp, const std::string & sFileName)
		{ 
#if 0//!defined WINAMP_PLUGIN
			RiffWave file;
			file.Open(sFileName);
			if (!file.isValidFile()) { file.Close(); return -1; }

			// Initializes the layer.
			boost::filesystem::path path(sFileName);
			name = path.filename().string();
			if (name.length() > 32) {
				name = name.substr(0,32);
			}
			uint16_t st_type =std::min(static_cast<uint16_t>(2U),file.format().nChannels);
			uint32_t Datalen(file.numSamples());
			int realsampleIdx = -1;
			XMInstrument::WaveData<>& wave = WavAlloc(loadhelp, st_type == 2U, Datalen, name, realsampleIdx);
			wave.WaveSampleRate(file.format().nSamplesPerSec);
			int16_t* samps[]={wave.pWaveDataL(), wave.pWaveDataR()};
			WaveFormat_Data convert16(file.format().nSamplesPerSec,16,st_type,false);
			file.readDeInterleavedSamples(reinterpret_cast<void**>(samps),Datalen,&convert16);

			RiffWaveInstChunk instchunk;
			if (file.GetInstChunk(instchunk)) {
				//Midi note: note sampled, waveTune: offset for note entered. 60 = middle C
				wave.WaveTune(60-instchunk.midiNote);
				wave.WaveFineTune(instchunk.midiCents);
				wave.WaveGlobVolume(helpers::dsp::dB2Amp(instchunk.gaindB));
			}

			RiffWaveSmplChunk smplchunk;
			if (file.GetSmplChunk(smplchunk)) {
				//Midi note: note sampled, waveTune: offset for note entered. 60 = middle C
				wave.WaveTune(60-smplchunk.midiNote);
				int16_t fine = static_cast<int16_t>(smplchunk.midiPitchFr >>24) / 2.56; //Convert to cents.
				wave.WaveFineTune(fine);
				if (smplchunk.numLoops > 0) {
					for (int l=0;l<smplchunk.numLoops;l++) {
						RiffWaveSmplLoopChunk loop = smplchunk.loops[l];
						if (loop.start >= 0 && loop.start < loop.end && loop.end <= Datalen) {
							if (loop.playCount==0 || loop.playCount > 256) {
								wave.WaveLoopStart(loop.start);
								wave.WaveLoopEnd(loop.end);
								if (loop.type==1) {
									wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::BIDI);
								}
								else if (loop.type==0 || loop.type==2) {
									wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
								}
							}
							else {
								wave.WaveSusLoopStart(loop.start);
								wave.WaveSusLoopEnd(loop.end);
								if (loop.type==1) {
									wave.WaveSusLoopType(XMInstrument::WaveData<>::LoopType::BIDI);
								}
								else if (loop.type==0 || loop.type==2) {
									wave.WaveSusLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
								}
							}
						}
					}
				}
			}

			file.Close();
#else
			int realsampleIdx=-1;
#endif //!defined WINAMP_PLUGIN
			return realsampleIdx;
		}

		bool Song::Load(RiffFile* pFile, CProgressDialog& progress, bool fullopen)
		{
			CExclusiveLock lock(&semaphore, 2, true);
			char Header[16]; // 9 is enough, but using 16 to better align the code
			pFile->Read(&Header, 8);
			Header[8]=0;

			if (strcmp(Header,"PSY3SONG")==0)
			{
				progress.SetWindowText("Loading new format...");
				UINT version = 0;
				UINT size = 0;
				UINT index = 0;
				int temp;
				int solo(0);
				int chunkcount=0;
				Header[4]=0;
				size_t filesize = pFile->FileSize();
				pFile->Read(&version,sizeof(version));
				pFile->Read(&size,sizeof(size));
// 				if(version > CURRENT_FILE_VERSION)
// 				{
// #if !defined WINAMP_PLUGIN
// 					MessageBox(0,"This file is from a newer version of Psycle! This process will try to load it anyway.", "Load Warning", MB_OK | MB_ICONERROR);
// #endif //!defined WINAMP_PLUGIN
// 				}
				std::string trackername;
				std::string trackervers;
				pFile->Read(&chunkcount,sizeof(chunkcount));
				if (version >= 8) {
					pFile->ReadString(trackername);
					pFile->ReadString(trackervers);
					int bytesread = 4 + trackername.length()+trackervers.length()+2;
					pFile->Skip(size - bytesread);// Size of the current Header DATA // This ensures that any extra data is skipped.
				}

				DestroyAllMachines();
				DeleteInstruments();
				DeleteAllPatterns();
				Reset(); //added by sampler mainly to reset current pattern showed.
				while(pFile->Read(&Header, 4) && chunkcount)
				{
					progress.m_Progress.SetPos(helpers::math::round<int,float>((pFile->GetPos()*16384.0f)/filesize));
					::Sleep(1); ///< Allow screen refresh.
					// we should use the size to update the index, but for now we will skip it
					if(std::strcmp(Header,"INFO") == 0)
					{
						--chunkcount;
						pFile->Read(&version, sizeof version);
						pFile->Read(&size, sizeof size);
						size_t begins = pFile->GetPos();
						if((version&0xFFFF0000) == VERSION_MAJOR_ZERO)
						{
							char name_[129]; char author_[65]; char comments_[65536];
							pFile->ReadString(name_, sizeof name_);
							pFile->ReadString(author_, sizeof author_);
							pFile->ReadString(comments_,sizeof comments_);
							name = name_;
							author = author_;
							comments = comments_;
							//bugfix. There were songs with incorrect size.
							if(version == 0) {
								size= (UINT)(pFile->GetPos() - begins);
							}
						}
						pFile->Seek(begins + size);
					}
					else if(std::strcmp(Header,"SNGI")==0)
					{
						--chunkcount;
						pFile->Read(&version, sizeof version);
						pFile->Read(&size, sizeof size);
						size_t begins = pFile->GetPos();
						if((version&0xFFFF0000) == VERSION_MAJOR_ZERO)
						{
							// why all these temps?  to make sure if someone changes the defs of
							// any of these members, the rest of the file reads ok.  assume 
							// everything is an int, when we write we do the same thing.

							// # of tracks for whole song
							pFile->Read(&temp, sizeof temp);
							SONGTRACKS = temp;
							// bpm
							{///\todo: This was a hack added in 1.9alpha to allow decimal BPM values
								int16_t temp16(0);
								pFile->Read(&temp16, sizeof temp16);
								int BPMCoarse = temp16;
								pFile->Read(&temp16, sizeof temp16);
								m_BeatsPerMin = BPMCoarse + (temp16 / 100.0f);
							}
							// linesperbeat
							pFile->Read(&temp, sizeof temp);
							m_LinesPerBeat = temp;
							// current octave
							pFile->Read(&temp, sizeof temp);
							currentOctave = temp;
							// machineSoloed
							// we need to buffer this because destroy machine will clear it
							pFile->Read(&temp, sizeof temp);
							solo = temp;
							// trackSoloed
							pFile->Read(&temp, sizeof temp);
							_trackSoloed = temp;
							pFile->Read(&temp, sizeof temp);  
							seqBus = temp;
							pFile->Read(&temp, sizeof temp);  
							paramSelected = temp;
							pFile->Read(&temp, sizeof temp);  
							auxcolSelected = temp;
							pFile->Read(&temp, sizeof temp);  
							instSelected = temp;
							// sequence width, for multipattern
							pFile->Read(&temp,sizeof(temp));
							_trackArmedCount = 0;
							for(int i(0) ; i < SONGTRACKS; ++i)
							{
								pFile->Read(&_trackMuted[i],sizeof(_trackMuted[i]));
								// remember to count them
								pFile->Read(&_trackArmed[i],sizeof(_trackArmed[i]));
								if(_trackArmed[i]) ++_trackArmedCount;
							}
							m_TicksPerBeat=24;
							m_ExtraTicksPerLine=0;
							if(version == 0) {
								// fix for a bug existing in the song saver in the 1.7.x series
								size = (11 * sizeof(uint32_t)) + (SONGTRACKS * 2 * sizeof(bool));
							}
							if(version >= 1) {
								pFile->Read(shareTrackNames);
								if( shareTrackNames) {
									for(int t(0); t < SONGTRACKS; t++) {
										std::string name;
										pFile->ReadString(name);
										ChangeTrackName(0,t,name);
									}
								}
							}
							if (version >= 2) {
								pFile->Read(&temp, sizeof temp);
								m_TicksPerBeat = temp;
								pFile->Read(&temp, sizeof temp);
								m_ExtraTicksPerLine = temp;
							}
							if (fullopen)
							{
								///\todo: Warning! This is done here, because the plugins, when loading, need an up-to-date information.
								/// It should be coded in some way to get this information from the loading song, since doing it here
								/// is bad for the Winamp plugin (or any other multi-document situation).
								Global::player().SetBPM(BeatsPerMin(), LinesPerBeat(), ExtraTicksPerLine());
							}
						}
						pFile->Seek(begins + size);
					}
					else if(std::strcmp(Header,"SEQD")==0)
					{
						--chunkcount;
						pFile->Read(&version,sizeof version);
						pFile->Read(&size,sizeof size);
						size_t begins = pFile->GetPos();
						if((version&0xFFFF0000) == VERSION_MAJOR_ZERO)
						{
							// index, for multipattern - for now always 0
							pFile->Read(&index, sizeof index);
							if (index < MAX_SEQUENCES)
							{
								char pTemp[256];
								// play length for this sequence
								pFile->Read(&temp, sizeof temp);
								playLength = temp;
								// name, for multipattern, for now unused
								pFile->ReadString(pTemp, sizeof pTemp);
								for (int i(0) ; i < playLength; ++i)
								{
									pFile->Read(&temp, sizeof temp);
									playOrder[i] = temp;
								}
							}
						}
						pFile->Seek(begins + size);
					}
					else if(std::strcmp(Header,"PATD") == 0)
					{
						--chunkcount;
						pFile->Read(&version, sizeof version);
						pFile->Read(&size, sizeof size);
						size_t begins = pFile->GetPos();
						if((version&0xFFFF0000) == VERSION_MAJOR_ZERO)
						{
							// index
							pFile->Read(&index, sizeof index);
							if(index < MAX_PATTERNS)
							{
								unsigned int sizez77 = 0;
								// num lines
								pFile->Read(&temp, sizeof temp );
								// clear it out if it already exists
								RemovePattern(index);
								patternLines[index] = temp;
								// num tracks per pattern // eventually this may be variable per pattern, like when we get multipattern
								pFile->Read(&temp, sizeof temp );
								pFile->ReadString(patternName[index], sizeof *patternName);
								pFile->Read(&sizez77, sizeof sizez77);
								byte* pSource = new byte[sizez77];
								pFile->Read(pSource, sizez77);
								byte* pDest;
								DataCompression::BEERZ77Decomp2(pSource, &pDest);
								zapArray(pSource,pDest);
								for(int y(0) ; y < patternLines[index] ; ++y)
								{
									unsigned char* pData(_ppattern(index) + (y * MULTIPLY));
									std::memcpy(pData, pSource, SONGTRACKS * EVENT_SIZE);
									pSource += SONGTRACKS * EVENT_SIZE;
								}
								zapArray(pDest);
							}
							//\ Fix for a bug existing in the Song Saver in the 1.7.x series
							if((version == 0x0000) &&( pFile->GetPos() == begins+size+4)) size += 4;
							if(version > 0) {
								if( !shareTrackNames) {
									for(int t(0); t < SONGTRACKS; t++) {
										std::string name;
										pFile->ReadString(name);
										ChangeTrackName(index,t,name);
									}
								}
							}
						}
						pFile->Seek(begins + size);
					}
					else if(std::strcmp(Header,"MACD") == 0)
					{
						--chunkcount;
						pFile->Read(&version, sizeof version);
						pFile->Read(&size, sizeof size);
						size_t begins = pFile->GetPos();
						if((version&0xFFFF0000) == VERSION_MAJOR_ZERO)
						{
							pFile->Read(&index, sizeof index);
							if(index < MAX_MACHINES)
							{
								// we had better load it
								_pMachine[index] = Machine::LoadFileChunk(pFile, index, version, fullopen);								
								//Bugfix.
								if (fullopen) {
									if ((_pMachine[index]->_type == MACH_VST || _pMachine[index]->_type == MACH_VSTFX)
										&& dynamic_cast<vst::Plugin*>(_pMachine[index])->ProgramIsChunk() == false) {
											size = (UINT)(pFile->GetPos() - begins);
/*									} else if ((version&0xFF) == 0) {
										size = (pFile->GetPos() - begins) 
											 + sizeof(unsigned char) + 2*sizeof(int) + _pMachine[index]->GetNumParams()*sizeof(float);
*/
									}
								}
							}
						}
						pFile->Seek(begins + size);
					}
					else if(std::strcmp(Header,"INSD") == 0)
					{
						--chunkcount;
						pFile->Read(&version, sizeof version);
						pFile->Read(&size, sizeof size);
						size_t begins = pFile->GetPos();
						if((version&0xFFFF0000) == VERSION_MAJOR_ZERO)
						{
							pFile->Read(&index, sizeof index);
							if(index < MAX_INSTRUMENTS)
							{
								_pInstrument[index]->LoadFileChunk(pFile, version, samples, index, fullopen);
							}
						}
						pFile->Seek(begins + size);
					}
					else if(std::strcmp(Header,"EINS") == 0) 
					{ // Legacy for sampulse previous to Psycle 1.12
						--chunkcount;
						pFile->Read(version);
						pFile->Read(size);
						size_t begins = pFile->GetPos();
						size_t filepos=pFile->GetPos();
						//Version zero was the development version (1.7 alphas, psycle-core). Version one is the published one (1.8.5 onwards).
						if((version&0xFFFF0000) == VERSION_MAJOR_ONE)
						{
							char temp[8];
							uint32_t lowversion = (version&0xFFFF); // lowversion 0 is 1.8.5, lowversion 1 is 1.8.6
							// Instrument Data Load
							uint32_t numInstruments;
							pFile->Read(numInstruments);
							int idx;
							for(uint32_t  i = 0;i < numInstruments && filepos < begins+size;i++)
							{
								uint32_t sizeINST=0;

								pFile->Read(idx);
								pFile->Read(temp,4); temp[4]='\0';
								pFile->Read(sizeINST);
								filepos=pFile->GetPos();
								if (strcmp(temp,"INST")== 0) {
									uint32_t versionINST;
									pFile->Read(versionINST);
									if (versionINST == 1) {
										bool legacyenabled;
										pFile->Read(legacyenabled);
									}
									else {
										//versionINST 0 was not stored, so seek back.
										pFile->Seek(filepos);
										versionINST = 0;
									}
									XMInstrument inst;
									inst.Load(*pFile, versionINST, true, lowversion);
									xminstruments.SetInst(inst,idx);
								}
								if (lowversion > 0) {
									//Version 0 doesn't write the chunk size correctly
									//so we cannot correct it in case of error
									pFile->Seek(filepos+sizeINST);
									filepos=pFile->GetPos();
								}
							}
							uint32_t  numSamples;
							pFile->Read(numSamples);
							for(uint32_t  i = 0;i < numSamples && filepos < begins+size;i++)
							{
								char temp[8];
								uint32_t versionSMPD;
								uint32_t sizeSMPD=0;

								pFile->Read(idx);
								pFile->Read(temp,4); temp[4]='\0';
								pFile->Read(sizeSMPD);
								filepos=pFile->GetPos();
								if (strcmp(temp,"SMPD")== 0)
								{
									pFile->Read(versionSMPD);
									//versionSMPD 0 was not stored, so seek back.
									if (versionSMPD != 1) {
										pFile->Seek(filepos);
										versionSMPD = 0;
									}
									XMInstrument::WaveData<> wave;
									wave.Load(*pFile, versionSMPD, true);
									samples.SetSample(wave, idx);
								}
								if (lowversion > 0) {
									//Version 0 doesn't write the chunk size correctly
									//so we cannot correct it in case of error
									pFile->Seek(filepos+sizeSMPD);
									filepos=pFile->GetPos();
								}
							}
						}
						pFile->Seek(begins+size);
					}
					else if(std::strcmp(Header,"SMID") == 0)
					{
						--chunkcount;
						pFile->Read(version);
						pFile->Read(size);
						size_t begins = pFile->GetPos();
						if((version&0xFFFF0000) == VERSION_MAJOR_ZERO)
						{
							uint32_t instidx;
							pFile->Read(instidx);
							if (instidx < XMInstrument::MAX_INSTRUMENT) {
								if ( !xminstruments.Exists(instidx) ) {
									XMInstrument instrtmp;
									xminstruments.SetInst(instrtmp,instidx);
								}
								XMInstrument& instr = xminstruments.get(instidx);
								instr.Init();
								instr.Load(*pFile, version&0xFFFF);
							}
						}
						pFile->Seek(begins+size);
					}
					else if(std::strcmp(Header,"SMSB") == 0)
					{
						--chunkcount;
						pFile->Read(version);
						pFile->Read(size);
						size_t begins = pFile->GetPos();
						if((version&0xFFFF0000) == VERSION_MAJOR_ZERO)
						{
							uint32_t sampleidx;
							pFile->Read(sampleidx);
							if (sampleidx < XMInstrument::MAX_INSTRUMENT) {
								if ( !samples.Exists(sampleidx) ) {
									XMInstrument::WaveData<> wavetmp;
									samples.SetSample(wavetmp,sampleidx);
								}
								XMInstrument::WaveData<> & wave = samples.get(sampleidx);
								wave.Init();
								wave.Load(*pFile, version&0xFFFF);
							}
						}
						pFile->Seek(begins+size);
					}
					else if(std::strcmp(Header,"VIRG") == 0)
					{
						--chunkcount;
						pFile->Read(&version, sizeof version);
						pFile->Read(&size, sizeof size);
						size_t begins = pFile->GetPos();
						if((version&0xFFFF0000) == VERSION_MAJOR_ZERO)
						{
							int virtual_inst;
							int mac_idx;
							int inst_idx;
							pFile->Read(virtual_inst);
							pFile->Read(mac_idx);
							pFile->Read(inst_idx);
							Machine* mac = (mac_idx < MAX_BUSES) ? _pMachine[mac_idx] : NULL;
							if (virtual_inst >= MAX_MACHINES && virtual_inst < MAX_VIRTUALINSTS
								&& mac != NULL && (mac->_type == MACH_SAMPLER ||mac->_type == MACH_XMSAMPLER))
							{
								SetVirtualInstrument(virtual_inst,mac_idx,inst_idx);
							}
						}
						pFile->Seek(begins+size);
					}
					else 
					{
						// we are not at a valid header for some weird reason.  
						// probably there is some extra data.
						// shift back 3 bytes and try again
						pFile->Seek(pFile->GetPos()-3);
					}
				}

				// now that we have loaded all the modules, time to prepare them.
				progress.m_Progress.SetPos(16384);
				::Sleep(1);

				for (int i(0); i < MAX_MACHINES;++i) if ( _pMachine[i]) {
					_pMachine[i]->PostLoad(_pMachine);
				}
				// Clean memory.
				for(int i(0) ; i < MAX_MACHINES ; ++i) if(_pMachine[i])	{
					_pMachine[i]->legacyWires.clear();
				}

				// translate any data that is required
				machineSoloed = solo;
				// Safe measures for damaged files.
				if (!_pMachine[MASTER_INDEX] )
				{
					_pMachine[MASTER_INDEX] = new Master(MASTER_INDEX);
					_pMachine[MASTER_INDEX]->Init();				
				}
				
				if(chunkcount) return false;
				_saved=true;
				return true;
			}
			else if(std::strcmp(Header, "PSY2SONG") == 0)
			{
//				int i;
				progress.SetWindowText("Loading old format...");
				int num,sampR;
				bool _machineActive[128];
				unsigned char busEffect[64];
				unsigned char busMachine[64];
				DoNew();
				char name_[129]; char author_[65]; char comments_[65536];
				pFile->Read(name_, 32);
				pFile->Read(author_, 32);
				pFile->Read(comments_,128);
				name = name_;
				author = author_;
				comments = comments_;

				pFile->Read(&m_BeatsPerMin, sizeof m_BeatsPerMin);
				pFile->Read(&sampR, sizeof sampR);
				if( sampR <= 0)
				{
					// Shouldn't happen but has happened.
					m_LinesPerBeat= 4;
				}
				// The old format assumes we output at 44100 samples/sec, so...
				else m_LinesPerBeat = 44100 * 60 / (sampR * m_BeatsPerMin);
				m_TicksPerBeat= 24;
				m_ExtraTicksPerLine= 0;

				if (fullopen)
				{
					///\todo: Warning! This is done here, because the plugins, when loading, need an up-to-date information.
					/// It should be coded in some way to get this information from the loading song, since doing it here
					/// is bad for the Winamp plugin (or any other multi-document situation).
					Global::player().SetBPM(BeatsPerMin(), LinesPerBeat(), ExtraTicksPerLine());
				}
				pFile->Read(&currentOctave, sizeof(char));
				pFile->Read(busMachine, 64);
				pFile->Read(playOrder, 128);
				pFile->Read(&playLength, sizeof(int));
				pFile->Read(&SONGTRACKS, sizeof(int));
				// Patterns
				pFile->Read(&num, sizeof num);
				for(int i =0 ; i < num; ++i)
				{
					pFile->Read(&patternLines[i], sizeof *patternLines);
					pFile->Read(&patternName[i][0], sizeof *patternName);
					if(patternLines[i] > 0)
					{
						unsigned char * pData(CreateNewPattern(i));
						for(int c(0) ; c < patternLines[i] ; ++c)
						{
							pFile->Read(reinterpret_cast<char*>(pData), OLD_MAX_TRACKS * sizeof(PatternEntry));
							pData += MAX_TRACKS * sizeof(PatternEntry);
						}
						///\todo: tweak_effect should be converted to normal tweaks!
					}
					else
					{
						patternLines[i] = 64;
						RemovePattern(i);
					}
				}
				progress.m_Progress.SetPos(2048);
				::Sleep(1); ///< ???
				// Instruments
				pFile->Read(&instSelected, sizeof instSelected);
				int pans[OLD_MAX_INSTRUMENTS];
				char names[OLD_MAX_INSTRUMENTS][32];
				for(int i=0 ; i < OLD_MAX_INSTRUMENTS ; ++i)
				{
					pFile->Read(names[i], sizeof(names[0]));
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					pFile->Read(&_pInstrument[i]->_NNA, sizeof(_pInstrument[0]->_NNA));
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					int tmp;
					pFile->Read(tmp);
					//Truncate to 220 samples boundaries, and ensure it is not zero.
					tmp = (tmp/220)*220; if (tmp <=0) tmp=1;
					_pInstrument[i]->ENV_AT = tmp;
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					int tmp;
					pFile->Read(tmp);
					//Truncate to 220 samples boundaries, and ensure it is not zero.
					tmp = (tmp/220)*220; if (tmp <=0) tmp=1;
					_pInstrument[i]->ENV_DT = tmp;
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					pFile->Read(_pInstrument[i]->ENV_SL);
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					int tmp;
					pFile->Read(tmp);
					//Truncate to 220 samples boundaries, and ensure it is not zero. (also change default value)
					if (tmp == 16) tmp = 220;
					else { tmp = (tmp/220)*220; if (tmp <=0) tmp=1; }
					_pInstrument[i]->ENV_RT = tmp;
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					int tmp;
					pFile->Read(tmp);
					//Truncate to 220 samples boundaries, and ensure it is not zero.
					tmp = (tmp/220)*220; if (tmp <=0) tmp=1;
					_pInstrument[i]->ENV_F_AT = tmp;
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					int tmp;
					pFile->Read(tmp);
					//Truncate to 220 samples boundaries, and ensure it is not zero.
					tmp = (tmp/220)*220; if (tmp <=0) tmp=1;
					_pInstrument[i]->ENV_F_DT = tmp;
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					pFile->Read(_pInstrument[i]->ENV_F_SL);
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					int tmp;
					pFile->Read(tmp);
					//Truncate to 220 samples boundaries, and ensure it is not zero.
					tmp = (tmp/220)*220; if (tmp <=0) tmp=1;
					_pInstrument[i]->ENV_F_RT = tmp;
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					pFile->Read(&_pInstrument[i]->ENV_F_CO, sizeof(_pInstrument[0]->ENV_F_CO));
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					pFile->Read(&_pInstrument[i]->ENV_F_RQ, sizeof(_pInstrument[0]->ENV_F_RQ));
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					pFile->Read(&_pInstrument[i]->ENV_F_EA, sizeof(_pInstrument[0]->ENV_F_EA));
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					pFile->Read(&_pInstrument[i]->ENV_F_TP, sizeof(_pInstrument[0]->ENV_F_TP));
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					pFile->Read(&pans[i], sizeof(pans[0]));
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					pFile->Read(&_pInstrument[i]->_RPAN, sizeof(_pInstrument[0]->_RPAN));
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					pFile->Read(&_pInstrument[i]->_RCUT, sizeof(_pInstrument[0]->_RCUT));
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					pFile->Read(&_pInstrument[i]->_RRES, sizeof(_pInstrument[0]->_RRES));
				}
				
				progress.m_Progress.SetPos(4096);
				::Sleep(1);
				// Waves
				//
				int tmpwvsl;
				pFile->Read(&tmpwvsl, sizeof(int));

				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					for (int w=0; w<OLD_MAX_WAVES; w++)
					{
						int wltemp;
						pFile->Read(&wltemp, sizeof(wltemp));
						if (wltemp > 0)
						{
							if ( w == 0 )
							{
								XMInstrument::WaveData<> wave;
								short tmpFineTune;
								char dummy[33];
								unsigned short volume = 0;
								bool stereo =false;
								bool doloop = false;
								unsigned int loop;
								//Old format assumed 44Khz
								wave.WaveSampleRate(44100);
								wave.PanFactor(value_mapper::map_256_1(pans[i]));
								//Old wavename, not really used anyway.
								pFile->Read(dummy, 32);
								wave.WaveName(names[i]);
								pFile->Read(volume);
								wave.WaveGlobVolume(volume*0.01f);
								pFile->Read(&tmpFineTune, sizeof(short));
								//Current sample uses 100 cents. Older used +-256
								tmpFineTune = static_cast<int>(value_mapper::map_256_100(tmpFineTune));
								wave.WaveFineTune(tmpFineTune);
								pFile->Read(loop);
								wave.WaveLoopStart(loop);
								pFile->Read(loop);
								wave.WaveLoopEnd(loop);
								pFile->Read(doloop);
								wave.WaveLoopType(doloop?XMInstrument::WaveData<>::LoopType::NORMAL:XMInstrument::WaveData<>::LoopType::DO_NOT);
								pFile->Read(stereo);
								wave.AllocWaveData(wltemp,stereo);
								int16_t* data = wave.pWaveDataL();
								pFile->Read(data, wltemp*sizeof(short));
								if (stereo)
								{
									data = wave.pWaveDataR();
									pFile->Read(data, wltemp*sizeof(short));
								}
								samples.SetSample(wave, i);
							}
							else 
							{
								bool stereo;
								pFile->Skip(42+sizeof(bool));
								pFile->Read(&stereo,sizeof(bool));
								pFile->Skip(wltemp);
								if(stereo)pFile->Skip(wltemp);
							}
						}
					}
				}
				
				progress.m_Progress.SetPos(4096+2048);
				::Sleep(1);
				// VST DLLs
				//

				VSTLoader vstL[OLD_MAX_PLUGINS]; 
				for (int i=0; i<OLD_MAX_PLUGINS; i++)
				{
					pFile->Read(&vstL[i].valid,sizeof(bool));
					if( vstL[i].valid )
					{
						pFile->Read(vstL[i].dllName,sizeof(vstL[i].dllName));
						_strlwr(vstL[i].dllName);
						pFile->Read(&(vstL[i].numpars), sizeof(int32_t));
						vstL[i].pars = new float[vstL[i].numpars];

						for (int c=0; c<vstL[i].numpars; c++)
						{
							pFile->Read(&(vstL[i].pars[c]), sizeof(float));
						}
					}
				}
				
				progress.m_Progress.SetPos(8192);
				::Sleep(1);
				// Machines
				//
				pFile->Read(&_machineActive[0], sizeof(_machineActive));
				Machine* pMac[128];
				memset(pMac,0,sizeof(pMac));

				convert_internal_machines::Converter converter;

				for (int i=0; i<128; i++)
				{
					Sampler* pSampler;
					XMSampler* pXMSampler;
					Plugin* pPlugin;
					vst::Plugin * pVstPlugin(0);
					int x,y,type;
					if (_machineActive[i])
					{
						progress.m_Progress.SetPos(8192+i*(4096/128));
						::Sleep(1);

						pFile->Read(&x, sizeof(x));
						pFile->Read(&y, sizeof(y));

						pFile->Read(&type, sizeof(type));

						std::pair<int, std::string> bla(type,"");
						if(converter.plugin_names().exists(bla))
							pMac[i] = &converter.redirect(i, bla, *pFile);
						else switch (type)
						{
						case MACH_MASTER:
							pMac[i] = _pMachine[MASTER_INDEX];
							pMac[i]->Init();
							pMac[i]->Load(pFile);
							break;
						case MACH_SAMPLER:
							pMac[i] = pSampler = new Sampler(i);
							pMac[i]->Init();
							pMac[i]->Load(pFile);
							pSampler->DefaultC4(false);
							break;
						case MACH_XMSAMPLER:
							pMac[i] = pXMSampler = new XMSampler(i);
							pMac[i]->Init();
							pMac[i]->Load(pFile);
							break;
						case MACH_PLUGIN:
						{
							char sDllName[256];
							pFile->Read(sDllName, sizeof(sDllName)); // Plugin dll name
							_strlwr(sDllName);

							std::pair<int, std::string> bla2(type,sDllName);
							if(converter.plugin_names().exists(bla2))
								pMac[i] = &converter.redirect(i, bla2, *pFile);
							else {
								pMac[i] = pPlugin = new Plugin(i);
								if (pPlugin->LoadDll(sDllName)) {
									pMac[i]->Init();
									pMac[i]->Load(pFile);
								}
								else {
									pPlugin->SkipLoad(pFile);
									Machine* pOldMachine = pMac[i];
									pMac[i] = new Dummy(pOldMachine);
									pMac[i]->_macIndex=i;
									pMac[i]->Init();
									zapObject(pOldMachine);
									// Warning: It cannot be known if the missing plugin is a generator
									// or an effect. This will be guessed from the busMachine array.
								}
							}
							break;
						}
						case MACH_VST:
						case MACH_VSTFX:
							{
								std::string temp;
								char sPath[_MAX_PATH];
								char sError[128];
								bool berror=false;
								vst::Plugin* pTempMac = new vst::Plugin(0);
								unsigned char program;
								int instance;
								// The trick: We need to load the information from the file in order to know the "instance" number
								// and be able to create a plugin from the corresponding dll. Later, we will set the loaded settings to
								// the newly created plugin.
								pTempMac->PreLoad(pFile,program,instance);
								assert(instance < OLD_MAX_PLUGINS);
								int32_t shellIdx=0;
								if((!vstL[instance].valid) || (!Global::machineload().lookupDllName(vstL[instance].dllName,temp,MACH_VST,shellIdx)))
								{
									berror=true;
									sprintf(sError,"VST plug-in missing, or erroneous data in song file \"%s\"",vstL[instance].dllName);
								}
								else
								{
									strcpy(sPath,temp.c_str());
									if (!Global::machineload().TestFilename(sPath,shellIdx))
									{
										berror=true;
										sprintf(sError,"This VST plug-in is Disabled \"%s\" - replacing with Dummy.",sPath);
									}
									else
									{
										try
										{
											pMac[i] = pVstPlugin = dynamic_cast<vst::Plugin*>(Global::vsthost().LoadPlugin(sPath,shellIdx));

											if (pVstPlugin)
											{
												pVstPlugin->LoadFromMac(pTempMac);
												pVstPlugin->SetProgram(program);
												pVstPlugin->_macIndex=i;
												const int numpars = vstL[instance].numpars;
												for (int c(0) ; c < numpars; ++c)
												{
													pVstPlugin->SetParameter(c, vstL[instance].pars[c]);
												}
											}
										}
										catch(...)
										{
											berror=true;
											sprintf(sError,"Missing or Corrupted VST plug-in \"%s\" - replacing with Dummy.",sPath);
											if (pVstPlugin) { delete pVstPlugin; }
										}
									}
								}
								if (berror)
								{
#if !defined WINAMP_PLUGIN
									Global().pLogCallback(sError, "Loading Error");
#endif // !defined WINAMP_PLUGIN
									pMac[i] = new Dummy(pTempMac);
									pMac[i]->_macIndex=i;
									zapObject(pTempMac);
									if (type == MACH_VSTFX ) pMac[i]->_mode = MACHMODE_FX;
									else pMac[i]->_mode = MACHMODE_GENERATOR;
								}
							break;
							}
						case MACH_SCOPE:
						case MACH_DUMMY:
							pMac[i] = new Dummy(i);
							pMac[i]->Init();
							pMac[i]->Load(pFile);
							break;
						default:
#if !defined WINAMP_PLUGIN
							{
								char buf[MAX_PATH];
								sprintf(buf,"unknown machine type: %i",type);
								Global().pLogCallback(buf, "Loading old song");
							}
#endif // !defined WINAMP_PLUGIN
							pMac[i] = new Dummy(i);
							pMac[i]->Init();
							pMac[i]->Load(pFile);
						}

						pMac[i]->_x = x;
						pMac[i]->_y = y;
					}
				}

				// Patch 0: Some extra data added around the 1.0 release.
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					pFile->Read(&_pInstrument[i]->_loop, sizeof(_pInstrument[0]->_loop));
				}
				for (int i=0; i<OLD_MAX_INSTRUMENTS; i++)
				{
					pFile->Read(&_pInstrument[i]->_lines, sizeof(_pInstrument[0]->_lines));
				}

				// Validate the machine arrays. 
				for (int i=0; i<128; i++) // First, we add the output volumes to a Matrix for reference later
				{
					if (!_machineActive[i])
					{
						zapObject(pMac[i]);
					}
					else if (!pMac[i])
					{
						_machineActive[i] = false;
					}
				}

				// Patch 1: BusEffects (twf). Try to read it, and if it doesn't exist, generate it.
				progress.m_Progress.SetPos(8192+4096);
				::Sleep(1);
				if ( pFile->Read(&busEffect[0],sizeof(busEffect)) == false )
				{
					int j=0;
					unsigned char invmach[128];
					memset(invmach,255,sizeof(invmach));
					// The guessing procedure does not rely on the machmode because if a plugin
					// is missing, then it is always tagged as a generator.
					for (int i = 0; i < 64; i++)
					{
						if (busMachine[i] != 255) invmach[busMachine[i]]=i;
					}
					for ( int i=1;i<128;i++ ) // machine 0 is the Master machine.
					{
						if (_machineActive[i])
						{
							if (invmach[i] == 255)
							{
								busEffect[j]=i;	
								j++;
							}
						}
					}
					while(j < 64)
					{
						busEffect[j] = 255;
						j++;
					}
				}
				// Validate that there isn't any duplicated machine.
				for ( int i=0;i<64;i++ ) 
				{
					for ( int j=i+1;j<64;j++ ) 
					{
						if  (busMachine[i] == busMachine[j]) busMachine[j]=255;
						if  (busEffect[i] == busEffect[j]) busEffect[j]=255;
					}
					for (int j=0;j<64;j++)
					{
						if  (busMachine[i] == busEffect[j]) busEffect[j]=255;
					}
				}

				// Patch 1.2: Fixes erroneous machine mode when a dummy replaces a bad plugin
				// (missing dll, or when the load process failed).
				// At the same time, we validate the indexes of the busMachine and busEffects arrays.
				for ( int i=0;i<64;i++ ) 
				{
					if (busEffect[i] != 255)
					{
						if ( busEffect[i] > 128 || !_machineActive[busEffect[i]] )
							busEffect[i] = 255;
						// If there's a dummy, force it to be an effect
						else if (pMac[busEffect[i]]->_type == MACH_DUMMY ) 
						{
							pMac[busEffect[i]]->_mode = MACHMODE_FX;
						}
						// Else if the machine is a generator, move it to gens bus.
						// This can't happen, but it is here for completeness
						else if (pMac[busEffect[i]]->_mode == MACHMODE_GENERATOR)
						{
							int k=0;
							while (busEffect[k] != 255 && k<MAX_BUSES) 
							{
								k++;
							}
							busMachine[k]=busEffect[i];
							busEffect[i]=255;
						}
					}
					if (busMachine[i] != 255)
					{
						if (busMachine[i] > 128 || !_machineActive[busMachine[i]])
							busMachine[i] = 255;
						 // If there's a dummy, force it to be a Generator
						else if (pMac[busMachine[i]]->_type == MACH_DUMMY ) 
						{
							pMac[busMachine[i]]->_mode = MACHMODE_GENERATOR;
						}
						// Else if the machine is an fx, move it to FXs bus.
						// This can't happen, but it is here for completeness
						else if ( pMac[busMachine[i]]->_mode != MACHMODE_GENERATOR)
						{
							int j=0;
							while (busEffect[j] != 255 && j<MAX_BUSES) 
							{
								j++;
							}
							busEffect[j]=busMachine[i];
							busMachine[i]=255;
						}
					}
				}

				 // Patch 2: VST Chunks.
				progress.m_Progress.SetPos(8192+4096+1024);
				::Sleep(1);
				bool chunkpresent=false;
				pFile->Read(&chunkpresent,sizeof(chunkpresent));

				if ( fullopen && chunkpresent ) for ( int i=0;i<128;i++ ) 
				{
					if (_machineActive[i])
					{
						if ( pMac[i]->_type == MACH_DUMMY ) 
						{
							if (((Dummy*)pMac[i])->wasVST && chunkpresent )
							{
								// Since we don't know if the plugin saved it or not, 
								// we're stuck on letting the loading crash/behave incorrectly.
								// There should be a flag, like in the VST loading Section to be correct.
								//MessageBox(NULL,"Missing or Corrupted VST plug-in has chunk, trying not to crash.", "Loading Error", MB_OK);
								psycle::host::Global::pLogCallback("Missing or Corrupted VST plug-in has chunk, trying not to crash.", "Loading Error");
							}
						}
						else if (( pMac[i]->_type == MACH_VST ) || ( pMac[i]->_type == MACH_VSTFX))
						{
							bool chunkread = false;
							try
							{
								vst::Plugin & plugin(*static_cast<vst::Plugin*>(pMac[i]));
								if(chunkpresent) chunkread = plugin.LoadChunk(pFile);
							}
							catch(const std::exception &)
							{
								// o_O`
							}
						}
					}
				}


				//////////////////////////////////////////////////////////////////////////
				//Finished all the file loading. Now Process the data to the current structures

				// The old fileformat stored the volumes on each output, 
				// so what we have in inputConVol is really outputConVol
				// and we have to convert while recreating them.
				progress.m_Progress.SetPos(8192+4096+2048);
				::Sleep(1);
				for (int i=0; i<128; i++) // we go to fix this for each
				{
					if (_machineActive[i])		// valid machine (important, since we have to navigate!)
					{
						for (int c=0; c<MAX_CONNECTIONS; c++) // all for each of its input connections.
						{
							LegacyWire& wire = pMac[i]->legacyWires[c];
							if (wire._inputCon && wire._inputMachine > -1 && wire._inputMachine < 128
								&& pMac[wire._inputMachine])	// If there's a valid machine in this inputconnection,
							{
								Machine* pSourceMac = pMac[wire._inputMachine];
								int d = Machine::FindLegacyOutput(pSourceMac, i); // We get that machine and wire
								if ( d != -1 )
								{
									float val = pSourceMac->legacyWires[d]._inputConVol;
									if( val > 4.1f )
									{
										val*=0.000030517578125f; // BugFix
									}
									else if ( val < 0.00004f) 
									{
										val*=32768.0f; // BugFix
									}
									// and set the volume.
									if (wire.pinMapping.size() > 0) {
										pMac[i]->inWires[c].ConnectSource(*pSourceMac,0,d,&wire.pinMapping);
									}
									else {
										pMac[i]->inWires[c].ConnectSource(*pSourceMac,0,d);
									}
									pMac[i]->inWires[c].SetVolume(val*wire._wireMultiplier);
								}
							}
						}
					}
				}
				
				// Psycle no longer uses busMachine and busEffect, since the pMachine Array directly maps
				// to the real machine.
				// Due to this, we have to move machines to where they really are, 
				// and remap the inputs and outputs indexes again... ouch
				// At the same time, we validate each wire.
				progress.m_Progress.SetPos(8192+4096+2048+1024);
				::Sleep(1);
				unsigned char invmach[128];
				memset(invmach,255,sizeof(invmach));
				for (int i = 0; i < 64; i++)
				{
					if (busMachine[i] != 255) invmach[busMachine[i]]=i;
					if (busEffect[i] != 255) invmach[busEffect[i]]=i+64;
				}
				invmach[0]=MASTER_INDEX;

				for (int i = 0; i < 128; i++)
				{
					if (invmach[i] != 255)
					{
						Machine *cMac = _pMachine[invmach[i]] = pMac[i];
						cMac->_macIndex = invmach[i];
						_machineActive[i] = false; // mark as "converted"
					}
				}
				// verify that there isn't any machine that hasn't been copied into _pMachine
				// Shouldn't happen. It would mean a damaged file.
				int j=0;
				int k=64;
				for (int i=0;i < 128; i++)
				{
					if (_machineActive[i])
					{
						if ( pMac[i]->_mode == MACHMODE_GENERATOR)
						{
							while (_pMachine[j] && j<64) j++;
							_pMachine[j]=pMac[i];
						}
						else
						{
							while (_pMachine[k] && k<128) k++;
							_pMachine[k]=pMac[i];
						}
					}
				}

				progress.m_Progress.SetPos(16384);
				::Sleep(1);
				if(fullopen) converter.retweak(*this);
				for (int i(0); i < MAX_MACHINES;++i) if ( _pMachine[i]) _pMachine[i]->PostLoad(_pMachine);
				seqBus=0;
				// Clean memory.
				for(int i(0) ; i < MAX_MACHINES ; ++i) if(_pMachine[i])	{
					_pMachine[i]->legacyWires.clear();
				}
				// Clean the vst loader helpers.
				for (int i=0; i<OLD_MAX_PLUGINS; i++)
				{
					if( vstL[i].valid )
					{
						zapObject(vstL[i].pars);
					}
				}
				_saved=true;
				return true;
			}

			// load did not work
			//MessageBox(NULL,"Incorrect file format","Error",MB_OK);
			psycle::host::Global::pLogCallback("Incorrect file format", "Error");
			return false;
		}

		bool Song::Save(RiffFile* pFile,CProgressDialog& progress,bool autosave)
		{
			// NEW FILE FORMAT!!!
			// this is much more flexible, making maintenance a breeze compared to that old hell.
			// now you can just update one module without breaking the whole thing.

			int chunkcount = 3; // 3 chunks (INFO, SNGI, SEQD. SONG is not counted as a chunk) plus:
			//PATD
			for (int i = 0; i < MAX_PATTERNS; i++)
			{
				// check every pattern for validity
				if (IsPatternUsed(i))
				{
					chunkcount++;
				}
			}
			//MACD
			for (int i = 0; i < MAX_MACHINES; i++)
			{
				// check every pattern for validity
				if (_pMachine[i])
				{
					chunkcount++;
				}
			}
			//INSD
			uint32_t numSamples = 0;	
			for (int i = 0; i < MAX_INSTRUMENTS; i++)
			{
				if (samples.IsEnabled(i))
				{
					numSamples++;
				}
			}
			chunkcount+=numSamples;
			// SMID
			for(int i = 0;i < XMInstrument::MAX_INSTRUMENT;i++){
				if(xminstruments.IsEnabled(i)){
					chunkcount++;
				}
			}
			// SMSB
			chunkcount+=numSamples;
			// VIRG
			for (int i=MAX_MACHINES; i < MAX_VIRTUALINSTS; i++) {
				macinstpair instpair = VirtualInstrument(i);
				if (instpair.first != -1) {
					chunkcount++;
				}
			}

			if ( !autosave ) 
			{
				progress.m_Progress.SetRange(0,chunkcount);
				progress.m_Progress.SetStep(1);
			}

			//Used to keep the correct data size on the fields.
			UINT index = 0;
			int temp;

			/*
			===================
			FILE HEADER
			===================
			id = "PSY3SONG"; // PSY2 was 0.96 to 1.7.2
			*/
			pFile->Write("PSY3",4);
			std::size_t pos = pFile->WriteHeader("SONG", CURRENT_FILE_VERSION);

			pFile->Write(chunkcount);
			pFile->WriteString(PSYCLE__TITLE);
			pFile->WriteString(PSYCLE__VERSION);

			pFile->UpdateSize(pos);
						
			if ( !autosave ) 
			{
				progress.m_Progress.StepIt();
				::Sleep(1);
			}

			// the rest of the modules can be arranged in any order

			/*
			===================
			SONG INFO TEXT
			===================
			id = "INFO"; 
			*/
			std::size_t sizepos = pFile->WriteHeader("INFO", CURRENT_FILE_VERSION_INFO);

			pFile->WriteString(name);
			pFile->WriteString(author);
			pFile->WriteString(comments);

			pFile->UpdateSize(sizepos);

			if ( !autosave ) 
			{
				progress.m_Progress.StepIt();
				::Sleep(1);
			}

			/*
			===================
			SONG INFO
			===================
			id = "SNGI"; 
			*/
			sizepos = pFile->WriteHeader("SNGI",CURRENT_FILE_VERSION_SNGI);

			temp = SONGTRACKS;
			pFile->Write(temp);
			temp = m_BeatsPerMin;
			pFile->Write(temp);
			temp = m_LinesPerBeat;
			pFile->Write(temp);
			temp = currentOctave;
			pFile->Write(temp);
			temp = machineSoloed;
			pFile->Write(temp);
			temp = _trackSoloed;
			pFile->Write(temp);

			temp = seqBus;
			pFile->Write(temp);

			temp = paramSelected;
			pFile->Write(temp);
			temp = auxcolSelected;
			pFile->Write(temp);
			temp = instSelected;
			pFile->Write(temp);

			temp = 1; // sequence width
			pFile->Write(temp);

			for (int i = 0; i < SONGTRACKS; i++)
			{
				pFile->Write(&_trackMuted[i],sizeof(bool));
				pFile->Write(&_trackArmed[i],sizeof(bool)); // remember to count them
			}

			pFile->Write(shareTrackNames);
			if( shareTrackNames) {
				for(int t(0); t < SONGTRACKS; t++) {
					pFile->WriteString(_trackNames[0][t]);
				}
			}
			
			temp = m_TicksPerBeat;
			pFile->Write(temp);
			temp = m_ExtraTicksPerLine;
			pFile->Write(temp);

			pFile->UpdateSize(sizepos);

			if ( !autosave ) 
			{
				progress.m_Progress.StepIt();
				::Sleep(1);
			}

			/*
			===================
			SEQUENCE DATA
			===================
			id = "SEQD"; 
			*/
			for (index=0;index<MAX_SEQUENCES;index++)
			{
				std::string sequenceName = "seq0"; // This needs to be replaced when converting to Multisequence.

				sizepos = pFile->WriteHeader("SEQD", CURRENT_FILE_VERSION_SEQD);
				
				pFile->Write(index); // Sequence Track number
				temp = playLength;
				pFile->Write(temp); // Sequence length

				pFile->WriteString(sequenceName); // Sequence Name

				for (int i = 0; i < playLength; i++)
				{
					temp = playOrder[i];
					pFile->Write(temp);	// Sequence data.
				}
				pFile->UpdateSize(sizepos);
			}
			if ( !autosave ) 
			{
				progress.m_Progress.StepIt();
				::Sleep(1);
			}

			/*
			===================
			PATTERN DATA
			===================
			id = "PATD"; 
			*/

			for (int i = 0; i < MAX_PATTERNS; i++)
			{
				// check every pattern for validity
				if (IsPatternUsed(i))
				{
					// ok save it
					byte* pSource=new byte[SONGTRACKS*patternLines[i]*EVENT_SIZE];
					byte* pCopy = pSource;

					for (int y = 0; y < patternLines[i]; y++)
					{
						unsigned char* pData = ppPatternData[i]+(y*MULTIPLY);
						memcpy(pCopy,pData,EVENT_SIZE*SONGTRACKS);
						pCopy+=EVENT_SIZE*SONGTRACKS;
					}
					
					int sizez77 = DataCompression::BEERZ77Comp2(pSource, &pCopy, SONGTRACKS*patternLines[i]*EVENT_SIZE);
					zapArray(pSource);

					sizepos = pFile->WriteHeader("PATD", CURRENT_FILE_VERSION_PATD);

					index = i; // index
					pFile->Write(index);
					temp = patternLines[i];
					pFile->Write(temp);
					temp = SONGTRACKS; // eventually this may be variable per pattern
					pFile->Write(temp);

					pFile->WriteString(patternName[i]);

					pFile->Write(sizez77);
					pFile->Write(pCopy,sizez77);
					zapArray(pCopy);
					
					if( !shareTrackNames) {
						for(int t(0); t < SONGTRACKS; t++) {
							pFile->WriteString(_trackNames[i][t]);
						}
					}

					pFile->UpdateSize(sizepos);

					if ( !autosave ) 
					{
						progress.m_Progress.StepIt();
						::Sleep(1);
					}
				}
			}
			/*
			===================
			MACHINE DATA
			===================
			id = "MACD"; 
			*/
			// machine and instruments handle their save and load in their respective classes

			for (int i = 0; i < MAX_MACHINES; i++)
			{
				if (_pMachine[i])
				{
					sizepos = pFile->WriteHeader("MACD", CURRENT_FILE_VERSION_MACD);

					index = i; // index
					pFile->Write(index);

					_pMachine[i]->SaveFileChunk(pFile);
					
					pFile->UpdateSize(sizepos);

					if ( !autosave ) 
					{
						progress.m_Progress.StepIt();
						::Sleep(1);
					}
				}
			}
			/*
			===================
			Instrument DATA
			===================
			id = "INSD"; 
			*/
			for (int i = 0; i < MAX_INSTRUMENTS; i++)
			{
				if (samples.IsEnabled(i))
				{
					sizepos = pFile->WriteHeader("INSD",CURRENT_FILE_VERSION_INSD);

					index = i; // index
					pFile->Write(index);
					_pInstrument[i]->SaveFileChunk(pFile);

					pFile->UpdateSize(sizepos);

					if ( !autosave ) 
					{
						progress.m_Progress.StepIt();
						::Sleep(1);
					}
				}
			}

			/*
			===================
			Sampulse Instrument data
			===================
			id = "SMID"; 
			*/
			for (int i=0; i < XMInstrument::MAX_INSTRUMENT; i++) {
				if (xminstruments.IsEnabled(i)) {
					sizepos = pFile->WriteHeader("SMID", CURRENT_FILE_VERSION_SMID);

					index = i; // index
					pFile->Write(index);
					xminstruments[i].Save(*pFile, CURRENT_FILE_VERSION_SMID);
					
					pFile->UpdateSize(sizepos);
					if ( !autosave ) 
					{
						progress.m_Progress.StepIt();
						::Sleep(1);
					}
				}
			}
			/*
			===================
			Sampulse Instrument data
			===================
			id = "SMSB"; 
			*/
			for (int i=0; i < XMInstrument::MAX_INSTRUMENT; i++) {
				if (samples.IsEnabled(i)) {
					sizepos = pFile->WriteHeader("SMSB",CURRENT_FILE_VERSION_SMSB);

					index = i; // index
					pFile->Write(index);
					samples[i].Save(*pFile);

					pFile->UpdateSize(sizepos);
					if ( !autosave ) 
					{
						progress.m_Progress.StepIt();
						::Sleep(1);
					}
				}
			}

			/*
			===================
			Virtual Instrument (Generator)
			===================
			id = "VIRG"; 
			*/
			for (int i=MAX_MACHINES; i < MAX_VIRTUALINSTS; i++) {
				macinstpair instpair = VirtualInstrument(i);
				if (instpair.first != -1) {
					sizepos = pFile->WriteHeader("VIRG",CURRENT_FILE_VERSION_VIRG);

					index = i; // index
					pFile->Write(index);
					pFile->Write(instpair.first);
					pFile->Write(instpair.second);

					pFile->UpdateSize(sizepos);
				}
			}


			if ( !autosave ) 
			{
				progress.m_Progress.SetPos(chunkcount);
				::Sleep(1);
			}
			_saved=true;
			return true;
		}
		void Song::DoPreviews(int amount)
		{
#if 0//!defined WINAMP_PLUGIN
			//todo do better.. use a vector<InstPreview*> or something instead
			float* pL(((Master*)_pMachine[MASTER_INDEX])->getLeft());
			float* pR(((Master*)_pMachine[MASTER_INDEX])->getRight());
			if(wavprev.IsEnabled())
			{
				wavprev.Work(pL, pR, amount);
			}
#endif // !defined WINAMP_PLUGIN
		}
		void Song::DeleteMachineRewiring(int macIdx)
		{
			Machine* pMac = _pMachine[macIdx];
			// move wires before deleting. Don't do inside DeleteMachine since that is used in song internally
			if( pMac->connectedInputs() > 0 && pMac->connectedOutputs() > 0) {
				//For each input connection
				for(int i = 0; i < MAX_CONNECTIONS; i++) if(pMac->inWires[i].Enabled()) {
					Machine& srcMac = pMac->inWires[i].GetSrcMachine();
					int wiresrc = pMac->inWires[i].GetSrcWireIndex();
					bool first = true;
					//Connect it to each output connection
					for(int i = 0; i < MAX_CONNECTIONS; i++) if(pMac->outWires[i] && pMac->outWires[i]->Enabled()) {
						Machine& dstMac = pMac->outWires[i]->GetDstMachine();
						//Except if already connected
						if( dstMac.FindInputWire(srcMac._macIndex) == -1) {
							int wiredst = pMac->outWires[i]->GetDstWireIndex();
							//If first wire change, it can be moved. Else it needs a new connection.
							if(first) {
								ChangeWireDestMacNonBlocking(&srcMac,&dstMac,wiresrc,wiredst);
								first = false;
							}
							else {
								float vol = pMac->outWires[i]->GetVolume();
								int dwire = InsertConnectionNonBlocking(&srcMac, &dstMac);
								dstMac.inWires[dwire].SetVolume(vol);
							}
						}
					}
				}
			}
			DestroyMachine(macIdx);
		}
		bool Song::CloneMac(int src,int dst)
		{
#if 0//!defined WINAMP_PLUGIN
			CExclusiveLock lock(&semaphore, 2, true);
			// src has to be occupied and dst must be empty
			if (_pMachine[src] && _pMachine[dst])
			{
				return false;
			}
			if (_pMachine[dst])
			{
				int temp = src;
				src = dst;
				dst = temp;
			}
			if (!_pMachine[src])
			{
				return false;
			}
			// check to see both are same type
			if (((dst < MAX_BUSES) && (src >= MAX_BUSES))
				|| ((dst >= MAX_BUSES) && (src < MAX_BUSES)))
			{
				return false;
			}

			if ((src >= MAX_MACHINES-1) || (dst >= MAX_MACHINES-1))
			{
				return false;
			}

			// save our file
			PsycleGlobal::inputHandler().AddMacViewUndo();
			MemoryFile file;
			file.OpenMem();

			file.Write("MACD",4);
			UINT version = CURRENT_FILE_VERSION_MACD;
			file.Write(&version,sizeof(version));
			size_t pos = file.GetPos();
			UINT size = 0;
			file.Write(&size,sizeof(size));

			int index = dst; // index
			file.Write(&index,sizeof(index));

			_pMachine[src]->SaveFileChunk(&file);

			size_t pos2 = file.GetPos(); 
			size = (UINT)(pos2-pos-sizeof(size));
			file.Seek(pos);
			file.Write(&size,sizeof(size));


			// now load it
			file.Seek(0);

			char Header[5];
			file.Read(&Header, 4);
			Header[4] = 0;
			if (strcmp(Header,"MACD")==0)
			{
				file.Read(&version,sizeof(version));
				file.Read(&size,sizeof(size));
				if (version > CURRENT_FILE_VERSION_MACD)
				{
					// there is an error, this file is newer than this build of psycle
					file.Close();
					return false;
				}
				else
				{
					file.Read(&index,sizeof(index));
					index = dst;
					if (index < MAX_MACHINES)
					{
						// we had better load it
						DestroyMachine(index);
						_pMachine[index] = Machine::LoadFileChunk(&file,index,version);
					}
					else
					{
						file.Close();
						return false;
					}
				}
			}
			else
			{
				file.Close();
				return false;
			}
			file.Close();

			// randomize the dst's position

			SMachineCoords mcoords = PsycleGlobal::conf().macView().MachineCoords;

			int xs,ys;
			if (src >= MAX_BUSES)
			{
				xs = mcoords.sEffect.width;
				ys = mcoords.sEffect.height;
			}
			else 
			{
				xs = mcoords.sGenerator.width;
				ys = mcoords.sGenerator.height;
			}

			_pMachine[dst]->_x = _pMachine[dst]->_x+32;
			_pMachine[dst]->_y = _pMachine[dst]->_y+ys+8;

			int number = 1;
			char buf[sizeof(_pMachine[dst]->_editName)+4];
			strcpy (buf,_pMachine[dst]->_editName);
			char* ps = strrchr(buf,' ');
			if (ps)
			{
				number = atoi(ps);
				if (number < 1)
				{
					number =1;
				}
				else
				{
					ps[0] = 0;
					ps = strchr(_pMachine[dst]->_editName,' ');
					ps[0] = 0;
				}
			}

			for (int i = 0; i < MAX_MACHINES-1; i++)
			{
				if (i!=dst)
				{
					if (_pMachine[i])
					{
						if (strcmp(_pMachine[i]->_editName,buf)==0)
						{
							number++;
							sprintf(buf,"%s %d",_pMachine[dst]->_editName,number);
							i = -1;
						}
					}
				}
			}

			buf[sizeof(_pMachine[dst]->_editName)-1] = 0;
			strncpy(_pMachine[dst]->_editName,buf,sizeof(_pMachine[dst]->_editName)-1);
#endif //!defined WINAMP_PLUGIN

			return true;
		}


		bool Song::CloneIns(int src,int dst)
		{
#if 1//defined WINAMP_PLUGIN
			return false;
#else
			CExclusiveLock lock(&semaphore, 2, true);
			// src has to be occupied and dst must be empty
			if (samples.IsEnabled(src) && samples.IsEnabled(dst))
			{
				return false;
			}
			// also allow dst occupid and src empty. in that case, do it backwards
			if (samples.IsEnabled(dst))
			{
				int temp = src;
				src = dst;
				dst = temp;
			}
			//If this is true, both are empty
			if (!samples.IsEnabled(src))
			{
				return false;
			}
			// ok now we get down to business
			PsycleGlobal::inputHandler().AddMacViewUndo();

			// save our file

			MemoryFile file;
			file.OpenMem();

			size_t pos = file.WriteHeader("INSD",CURRENT_FILE_VERSION_INSD);
			uint32_t dummyindex = dst; // index
			file.Write(dummyindex);
			_pInstrument[src]->SaveFileChunk(&file);
			file.UpdateSize(pos);

			if (samples.IsEnabled(src)) {
				pos = file.WriteHeader("SMSB",CURRENT_FILE_VERSION_SMSB);
				file.Write(dummyindex);
				samples.get(src).Save(file);

				file.UpdateSize(pos);
			}

			// now load it
			file.Seek(0);

			bool result=false;
			char Header[5];
			uint32_t version=0;
			uint32_t size;
			file.Read(&Header, 4);
			Header[4] = 0;
			if (strcmp(Header,"INSD")==0)
			{
				file.Read(&version,sizeof(version));
				file.Read(&size,sizeof(size));
				if (version <= CURRENT_FILE_VERSION_INSD)
				{
					file.Read(dummyindex);
					// we had better load it
					_pInstrument[dst]->LoadFileChunk(&file,version, samples, dst);
					if (!file.Eof()) {
						file.Read(&Header, 4);
						Header[4] = 0;
						if (strcmp(Header,"SMSB")==0)
						{
							file.Read(&version,sizeof(version));
							file.Read(&size,sizeof(size));
							if (version <= CURRENT_FILE_VERSION_SMSB)
							{
								file.Read(dummyindex);
								if ( !samples.Exists(dst) ) {
									XMInstrument::WaveData<> wavetmp;
									samples.SetSample(wavetmp,dst);
								}
								XMInstrument::WaveData<> & wave = samples.get(dst);
								wave.Init();
								wave.Load(file, version&0xFFFF);
							}
						}
					}
					result=true;
				}
			}
			file.Close();
			return result;
#endif //!defined WINAMP_PLUGIN
		}
		
		void Song::SavePsyInstrument(const std::string& filename, int instIdx) const
		{
			if (!xminstruments.IsEnabled(instIdx)) return;
			const XMInstrument & instr = xminstruments[instIdx];

			std::set<int> sampidxs = instr.GetWavesUsed();
			std::map<unsigned char, unsigned char> sampMap;
			std::list<int> indexlist;
			const unsigned char n255 = 255u;
			unsigned char i=0;
			for (std::set<int>::const_iterator ite = sampidxs.begin(); ite != sampidxs.end(); ++ite) {
				if (samples.IsEnabled(*ite)) {
					sampMap[static_cast<unsigned char>(*ite)] = i;
					indexlist.push_back(*ite);
					i++;
				}
				else { sampMap[*ite]= n255; }
			}
			sampMap[n255]=n255;

			RiffFile file;
			file.Create(filename,true);
			file.Write("PSYI",4);
			size_t pos = file.WriteHeader("SMID", CURRENT_FILE_VERSION_SMID);

			uint32_t index = 0; // index
			file.Write(index);
			instr.Save(file, &sampMap, CURRENT_FILE_VERSION_SMID);

			file.UpdateSize(pos);
			for (std::list<int>::const_iterator ite = indexlist.begin(); ite != indexlist.end(); ++ite, index++) {
				size_t pos = file.WriteHeader("SMSB",CURRENT_FILE_VERSION_SMSB);

				file.Write(index);
				samples[*ite].Save(file);

				file.UpdateSize(pos);
			}
			file.Close();
		}
		bool Song::LoadPsyInstrument(LoaderHelper& loadhelp, const std::string& filename)
		{
			char id[5];
			uint32_t size;
			uint32_t version;
			RiffFile file;

			std::map<int, int> sampMap;
			bool preview = loadhelp.IsPreview();
			int instIdx=-1;

			if (!file.Open(filename) || !file.Expect("PSYI",4) ) {
				file.Close();
				return false;
			}
			XMInstrument& instr = loadhelp.GetNextInstrument(instIdx);
			file.Read(id,4);	id[4]='\0';
			//Old format, only in 1.11 betas
			if (strncmp(id,"EINS",4) == 0) {
				file.Read(size);
				uint32_t numIns;
				file.Read(numIns);
				//This loader only supports one instrument.
				if (numIns== 1)
				{
					uint32_t sizeINST=0;
					file.Read(id,4); id[4]='\0';
					file.Read(sizeINST);
					if (strncmp(id,"INST",4)== 0) {
						uint32_t versionINST;
						file.Read(versionINST);
						instr.Load(file, versionINST, true, 0x1);
					}
					uint32_t numSamps;
					file.Read(numSamps);
					int sample=-1;
					if (preview) { // If preview, determine which sample to  load.
						sample = instr.NoteToSample(notecommands::middleC).second;
						if (sample == notecommands::empty) {
							const std::set<int>& samps = instr.GetWavesUsed();
							if (!samps.empty()) {
								sample = *samps.begin();
							}
						}
					}
					for(uint32_t i=0; i < numSamps && !file.Eof(); i++) {
						uint32_t sizeSMPD=0;
						file.Read(id,4); id[4]='\0';
						file.Read(sizeSMPD);
						if (strcmp(id,"SMPD")== 0)
						{
							uint32_t versionSMPD=0;
							file.Read(versionSMPD);
							int waveidx=-1;
							XMInstrument::WaveData<> & wave = loadhelp.GetNextSample(waveidx);
							wave.Load(file, versionSMPD, true);
							sampMap[i]=waveidx;
							if (i == sample) {
								break;
							}
						}
					}
				}
			}
			//Current format
			else if (strncmp(id,"SMID",4) == 0) {
				file.Read(version);
				file.Read(size);
				size_t begins = file.GetPos();
				if((version&0xFFFF0000) == VERSION_MAJOR_ZERO)
				{
					uint32_t instidx; // Unused
					file.Read(instidx);
					instr.Load(file, version&0xFFFF);
				}
				int sample=-1;
				if (preview) { // If preview, determine which sample to  load.
					sample = instr.NoteToSample(notecommands::middleC).second;
					if (sample == notecommands::empty) {
						const std::set<int>& samps = instr.GetWavesUsed();
						if (!samps.empty()) {
							sample = *samps.begin();
						}
					}
				}
				file.Seek(begins+size);
				while(!file.Eof()) {
					file.Read(id,4);	id[4]='\0';
					if (strncmp(id,"SMSB",4) != 0) break;
					file.Read(version);
					file.Read(size);
					begins = file.GetPos();
					if((version&0xFFFF0000) == VERSION_MAJOR_ZERO)
					{
						uint32_t sampleidx;
						file.Read(sampleidx);
						if (sampleidx < XMInstrument::MAX_INSTRUMENT) {
							int waveidx=-1;
							XMInstrument::WaveData<> & wave = loadhelp.GetNextSample(waveidx);
							wave.Load(file, version&0xFFFF);
							sampMap[sampleidx]=waveidx;
							if (sampleidx == sample) {
								break;
							}
						}
					}
					file.Seek(begins+size);
				}
			}
			else {
				file.Close();
				return false;
			}
			//Remap 
			if(!preview) {
				for (int j=0; j<XMInstrument::NOTE_MAP_SIZE;j++) {
					XMInstrument::NotePair pair = instr.NoteToSample(j);
					if (pair.second != 255) {
						if (sampMap.find(pair.second) != sampMap.end()) {
							pair.second = sampMap[pair.second];
						}
						else { pair.second = 255; }
						instr.NoteToSample(j,pair);
					}
				}
				std::set<int> list = instr.GetWavesUsed();
				for (std::set<int>::iterator ite =list.begin(); ite != list.end(); ++ite) {
					_pInstrument[*ite]->CopyXMInstrument(instr,tickstomillis(1));
				}
			}
			file.Close();
			return true;
		}

		bool Song::IsPatternUsed(int i, bool onlyInSequence/*=false*/) const
		{
			bool bUsed = false;
			if (ppPatternData[i])
			{
				// we could also check to see if pattern is unused AND blank.
				for (int j = 0; j < playLength; j++)
				{
					if (playOrder[j] == i)
					{
						bUsed = true;
						break;
					}
				}

				if (!bUsed && !onlyInSequence)
				{
					bUsed = !IsPatternEmpty(i);
				}
			}
			return bUsed;
		}

		bool Song::IsPatternEmpty(int i) const {
			if (!ppPatternData[i]) {
				return true;
			}
			PatternEntry blank;
			unsigned char * pData = ppPatternData[i];
			for (int j = 0; j < MULTIPLY2; j+= EVENT_SIZE)
			{
				if (memcmp(pData+j,&blank,EVENT_SIZE) != 0 )
				{
					return false;
				}
			}
			return true;
		}
	}
}

/** @file 
 *  @brief implementation file
 *  $Date: 2008-02-12 00:44:11 +0100 (mar, 12 feb 2008) $
 *  $Revision: 6303 $
 */

#include <psycle/host/detail/project.private.hpp>
#include "XMSongExport.hpp"
#include "Song.hpp"
#include "Sampler.hpp"
#include "Plugin.hpp"

namespace psycle{
namespace host{


	XMSongExport::XMSongExport()
	{
	}

	XMSongExport::~XMSongExport()
	{

	}

	void XMSongExport::exportsong(const Song& song)
	{

		writeSongHeader(song);

		SavePatterns(song);
		SaveInstruments(song);

	}

	void XMSongExport::writeSongHeader(const Song &song)
	{
		macInstruments=0;
		bool hasSampler=false;
		for (int i=0; i<256;i++) {isBlitzorVst[i]=false;}
		for (int i=0; i<MAX_BUSES; i++) {
			if (song._pMachine[i] != NULL) {
				isSampler[i] = hasSampler = (song._pMachine[i]->_type == MACH_SAMPLER);
				isSampulse[i] = (song._pMachine[i]->_type == MACH_XMSAMPLER);
				if ( ! (isSampler[i] || isSampulse[i])) {
					macInstruments++;
					if (song._pMachine[i]->_type == MACH_PLUGIN) {
						const std::string str = reinterpret_cast<Plugin*>(song._pMachine[i])->GetDllName();
						if (str.find("blitz") != std::string::npos) {
							isBlitzorVst[i+1]=true;
						}
					}
					else if (song._pMachine[i]->_type == MACH_VST) {
						isBlitzorVst[i+1]=true;
					}
				}
			}
			else {
				isSampler[i] = false;
				isSampulse[i] = false;
			}
		}
		//If the xminstrument 0 is not used, do not increase the instrument number (the loader does this so exporting a loaded song adds a new empty slot)
		correctionIndex = (song.xminstruments.IsEnabled(0))?1:0;
		xmInstruments = song.GetHighestXMInstrumentIndex()+1;
		if (xmInstruments > 1 && correctionIndex==0) xmInstruments--;
		//If there is a sampler machine, we have to take the samples into account.
		int samInstruments = (hasSampler) ? song.GetHighestInstrumentIndex()+1: 0;
		
		Write(XM_HEADER, 17);//ID text
		std::string name = "PE:" + song.name.substr(0,17);
		Write(name.c_str(), 20);//Module name
		uint16_t temp = 0x1A;
		Write(&temp, 1);
		Write("FastTracker v2.00   ", 20);//Tracker name
		temp = 0x0104;
		Write(&temp, 2);//Version number

		memset(&m_Header,0,sizeof(m_Header));
		m_Header.size = sizeof(m_Header);
		m_Header.norder = song.playLength;
		m_Header.restartpos = 0;
		m_Header.channels = std::min(song.SONGTRACKS,32);
		m_Header.patterns = song.GetHighestPatternIndexInSequence()+1;
		m_Header.instruments = std::min(128,macInstruments + xmInstruments + samInstruments);
		m_Header.flags = 0x0001; //Linear frequency.
		m_Header.speed = floor(24.f/song.LinesPerBeat()) + song.ExtraTicksPerLine();
		m_Header.tempo =  song.BeatsPerMin();

		//Pattern order table
		for (int i = 0; i < song.playLength; i++) {
			m_Header.order[i] =  song.playOrder[i];
		}
		Write(&m_Header,sizeof(m_Header));
	}

	void XMSongExport::SavePatterns(const Song & song)
	{
		for (int i = 0; i < m_Header.patterns ; i++)
		{
			SaveSinglePattern(song,i);
		}
	}

	// Load instruments
	void XMSongExport::SaveInstruments(const Song& song)
	{
		for (int i=0; i<MAX_BUSES; i++) {
			if (song._pMachine[i] != NULL && song._pMachine[i]->_type != MACH_SAMPLER
					&& song._pMachine[i]->_type != MACH_XMSAMPLER) {
				SaveEmptyInstrument(song._pMachine[i]->_editName);
			}
		}
		int remaining = m_Header.instruments - macInstruments;
		for (int j=0, i=(correctionIndex==0)?1:0 ; j < xmInstruments && j < remaining; j++, i++) {
			if (song.xminstruments.IsEnabled(i)) {
				SaveSampulseInstrument(song,i);
			}
			else {
				SaveEmptyInstrument(song.xminstruments.Exists(i) ? song.xminstruments[i].Name() : "");
			}
		}
		remaining = m_Header.instruments - macInstruments - xmInstruments;
		for (int i = 0 ; i < remaining; i++ ){
			if (song.samples.IsEnabled(i)) {
				SaveSamplerInstrument(song,i);
			}
			else {
				SaveEmptyInstrument(song.samples.Exists(i) ? song.samples[i].WaveName() : "");
			}
		}
	}


	// return address of next pattern, 0 for invalid
	void XMSongExport::SaveSinglePattern(const Song & song, const int patIdx)
	{
		//Temp entries for volume on virtual generators.
		PatternEntry volumeEntries[32];

		XMPATTERNHEADER ptHeader;
		memset(&ptHeader,0,sizeof(ptHeader));
		ptHeader.size = sizeof(ptHeader);
		//ptHeader.packingtype = 0; implicit from memset.
		ptHeader.rows = std::min(256,song.patternLines[patIdx]);
		//ptHeader.packedsize = 0; implicit from memset.
		
		Write(&ptHeader,sizeof(ptHeader));
		std::size_t currentpos = GetPos();
		
		int maxtracks = std::min(song.SONGTRACKS,32);
		// check every pattern for validity
		if (song.IsPatternUsed(patIdx))
		{
			for (int i = 0; i < maxtracks; i++) {
				lastInstr[i]=-1;
			}
			addTicks=0;
			for (int j = 0; j < ptHeader.rows; j++) {
				for (int i = 0; i < maxtracks; i++) {
					extraEntry[i] = NULL;
				}
				for (int i = 0; i < song.SONGTRACKS; i++) {
					const PatternEntry* pData = reinterpret_cast<const PatternEntry*>(song._ptrackline(patIdx,i,j));
					if (pData->_note == notecommands::midicc) {
						if (pData->_inst < maxtracks) {
							extraEntry[pData->_inst] = pData;
						}
					}
					else if (pData->_note != notecommands::tweak && pData->_note != notecommands::tweakslide) {
						if (pData->_cmd == PatternCmd::EXTENDED && (pData->_parameter&0xF0) == PatternCmd::ROW_EXTRATICKS) {
							addTicks = pData->_parameter & 0x0F;
						}
					}
				}
				for (int i = 0; i < maxtracks; i++) {
					
					const PatternEntry* pData = reinterpret_cast<const PatternEntry*>(song._ptrackline(patIdx,i,j));
					if (pData->_note == notecommands::tweak || pData->_note == notecommands::tweakslide) {
						unsigned char compressed = 0x80;
						Write(&compressed,1);
						continue;
					}
					else if (pData->_note == notecommands::midicc) {
						if (pData->_inst > 0x7F) {
							unsigned char compressed = 0x80 + 2 + 8 + 16;
							Write(&compressed,1);
							Write(&pData->_inst,1);
							unsigned char val = XMCMD::MIDI_MACRO;
							Write(&val,1);
							Write(&pData->_cmd,1);
						}
						else {
							unsigned char compressed = 0x80;
							Write(&compressed,1);
						}
						continue;
					}
					
					Machine* mac = NULL;
					unsigned char instr=0;
					int instrint=0xFF;
					if (pData->_mach < MAX_BUSES) {
						mac = song._pMachine[pData->_mach];
						instrint = pData->_inst;
					}
					else if (pData->_mach >= MAX_MACHINES && pData->_mach < MAX_VIRTUALINSTS) {
						mac = song.GetMachineOfBus(pData->_mach, instrint);
						if (instrint == -1 ) instrint = 0xFF;
						if (mac != NULL && pData->_inst != 255) {
							volumeEntries[i]._cmd = (mac->_type == MACH_SAMPLER) ? SAMPLER_CMD_VOLUME : XMSampler::CMD::SENDTOVOLUME;
							volumeEntries[i]._parameter=pData->_inst;
							extraEntry[i]=&volumeEntries[i];
						}
					}
					if (mac != NULL) {
						if (mac->_type == MACH_SAMPLER) {
							if (instrint != 0xFF) instr = static_cast<unsigned char>(macInstruments + xmInstruments + instrint +1);
						}
						else if (mac->_type == MACH_XMSAMPLER) {
							if(instrint != 0xFF && 
								(pData->_note != notecommands::empty || pData->_mach < MAX_BUSES)) {
								instr = static_cast<unsigned char>(macInstruments + instrint +correctionIndex);
							}
						}
						else {
							instr = static_cast<unsigned char>(pData->_mach + 1);
						}
						if (instr != 0 ) lastInstr[i]=instr;
					}

					unsigned char note;
					if (pData->_note >= 12 && pData->_note < 108) {
						if ( mac != NULL && mac->_type == MACH_SAMPLER && ((Sampler*)mac)->isDefaultC4() == false )
						{
							note = pData->_note +1;
						} else {
							note = pData->_note - 11;
						}
					}
					else if (pData->_note == notecommands::release) {
						note = 0x61;
					} else {
						note = 0x00;
					}
					
					unsigned char vol=0;
					unsigned char type=0;
					unsigned char param=0;

					GetCommand(song, i, pData, vol, type, param);
					if (extraEntry[i] != NULL) GetCommand(song, i, extraEntry[i], vol, type, param);

					unsigned char bWriteNote = note!=0;
					unsigned char bWriteInstr = instr!=0;
					unsigned char bWriteVol = vol!=0;
					unsigned char bWriteType = type!=0;
					unsigned char bWriteParam  = param!=0;

					char compressed = 0x80 + bWriteNote + (bWriteInstr << 1) + (bWriteVol << 2)
										+ (bWriteType << 3) + ( bWriteParam << 4);

					if (compressed != 0x9F) Write(&compressed,1); // 0x9F means to write everything.
					if (bWriteNote) Write(&note,1);
					if (bWriteInstr) Write(&instr,1);
					if (bWriteVol) Write(&vol,1);
					if (bWriteType) Write(&type,1);
					if (bWriteParam) Write(&param,1);
				}
			}
			ptHeader.packedsize = static_cast<uint16_t>((GetPos() - currentpos) & 0xFFFF);
			Seek(currentpos-sizeof(ptHeader));
			Write(&ptHeader,sizeof(ptHeader));
			Skip(ptHeader.packedsize);
		}
		else {
			Write(&ptHeader,sizeof(ptHeader));
		}
	}

	void XMSongExport::GetCommand(const Song& song, int i, const PatternEntry *pData, unsigned char &vol, unsigned char &type, unsigned char &param)
	{
		int singleEffectCharacter = (pData->_cmd & 0xF0);					
		if (singleEffectCharacter == 0xF0) { //Global commands
			switch(pData->_cmd) {
				case PatternCmd::SET_TEMPO:
					if (pData->_parameter >= 0x20) {
						type = XMCMD::SETSPEED;
						param = pData->_parameter;
					}
					break;
				case PatternCmd::EXTENDED:
					switch(pData->_parameter&0xF0) {
					case PatternCmd::SET_LINESPERBEAT0:
					case PatternCmd::SET_LINESPERBEAT1:
						type = XMCMD::SETSPEED;
						param = floor(24.f/pData->_parameter) + addTicks;
						addTicks=0;
						break;
					case PatternCmd::PATTERN_LOOP:
						type = XMCMD::EXTENDED;
						param = XMCMD_E::E_PATTERN_LOOP + (pData->_parameter & 0x0F);
						break;
					case PatternCmd::PATTERN_DELAY:
						type = XMCMD::EXTENDED;
						param = XMCMD_E::E_PATTERN_DELAY + (pData->_parameter & 0x0F);
						break;
					default:
						break;
					}
					break;
				case PatternCmd::JUMP_TO_ORDER:
					type = XMCMD::POSITION_JUMP;
					param = pData->_parameter;
					break;
				case PatternCmd::BREAK_TO_LINE:
					type = XMCMD::PATTERN_BREAK;
					param = ((pData->_parameter/10)<<4) + (pData->_parameter%10);
					break;
				case PatternCmd::SET_VOLUME:
					if (pData->_inst == 255) {
						type = XMCMD::SET_GLOBAL_VOLUME;
						param = pData->_parameter >> 1;
					}
					break;
				case PatternCmd::NOTE_DELAY:
					type = XMCMD::EXTENDED;
					param = XMCMD_E::E_NOTE_DELAY | (pData->_parameter*song.TicksPerBeat()/256);
					break;
				case PatternCmd::ARPEGGIO:
					type = XMCMD::ARPEGGIO;
					param = pData->_parameter;
					break;
				default:
					break;
			}
		}
		else if(lastInstr[i] != -1 && isBlitzorVst[lastInstr[i]]) {
			if (singleEffectCharacter == 0xE0) { //Blitz and VST "special" slide up
				//int toneDest = (pData->_cmd & 0x0F); This has no mapping to module commands
				type = XMCMD::PORTAUP;
				param = pData->_parameter;
			}
			else if (singleEffectCharacter == 0xD0) { //Blitz and VST "special" slide down
				//int toneDest = (pData->_cmd & 0x0F); This has no mapping to module commands
				type = XMCMD::PORTADOWN;
				param = pData->_parameter;
			}
			else if (singleEffectCharacter == 0xC0) { //Blitz
				switch(pData->_cmd) {
					case 0xC1: // Blitz slide up
						type = XMCMD::PORTAUP;
						param = pData->_parameter;
						break;
					case 0xC2: // Blitz slide donw
						type = XMCMD::PORTADOWN;
						param = pData->_parameter;
						break;
					case 0xC3: // blitz tone portamento and VST porta
						type = XMCMD::PORTA2NOTE;
						param = pData->_parameter;
						break;
					case 0xCC: // blitz old volume
						vol = 0x10 + (pData->_parameter >> 2);
						break;
					default:
						break;
				}
			}
			else if (pData->_cmd == 0x0C) {
				vol = 0x10 + (pData->_parameter >> 2);
			}
			else if (pData->_cmd > 0) {
				type = XMCMD::ARPEGGIO;
				param = pData->_cmd;
			}
		}
		else if (lastInstr[i] != -1 && lastInstr[i] > macInstruments && lastInstr[i] <= macInstruments+xmInstruments)
		{ //Sampulse
			switch(pData->_cmd) {
				case XMSampler::CMD::ARPEGGIO:
					type = XMCMD::ARPEGGIO;
					param = pData->_parameter;
					break;
				case XMSampler::CMD::PORTAMENTO_UP:
					if (pData->_parameter < 0xF0) {
						type = XMCMD::PORTAUP;
						param = pData->_parameter;
					}
					else {
						type = XMCMD::EXTENDED;
						param = XMCMD_E::E_FINE_PORTA_UP | (pData->_parameter&0xF);
					}
					break;
				case XMSampler::CMD::PORTAMENTO_DOWN:
					if (pData->_parameter < 0xF0) {
						type = XMCMD::PORTADOWN;
						param = pData->_parameter;
					}
					else {
						type = XMCMD::EXTENDED;
						param = XMCMD_E::E_FINE_PORTA_DOWN | (pData->_parameter&0xF);
					}
					break;
				case XMSampler::CMD::PORTA2NOTE:
					type = XMCMD::PORTA2NOTE;
					param = pData->_parameter;
					break;
				case XMSampler::CMD::VIBRATO:
					type = XMCMD::VIBRATO;
					param = pData->_parameter;
					break;
				case XMSampler::CMD::TONEPORTAVOL:
					type = XMCMD::ARPEGGIO;
					param = pData->_parameter;
					break;
				case XMSampler::CMD::VIBRATOVOL:
					type = XMCMD::VIBRATOVOL;
					param = pData->_parameter;
					break;
				case XMSampler::CMD::TREMOLO:
					type = XMCMD::TREMOLO;
					param = pData->_parameter;
					break;
				case XMSampler::CMD::PANNING:
					type = XMCMD::PANNING;
					param = pData->_parameter;
					break;
				case XMSampler::CMD::OFFSET:
					type = XMCMD::OFFSET;
					param = pData->_parameter;
					break;
				case XMSampler::CMD::VOLUMESLIDE:
					if ((pData->_parameter &0xF) == 0xF) {
						type = XMCMD::EXTENDED;
						param = XMCMD_E::E_FINE_VOLUME_UP | ((pData->_parameter&0xF0) >> 4);
					}
					else if ((pData->_parameter&0xF0) == 0xF0) {
						type = XMCMD::EXTENDED;
						param = XMCMD_E::E_FINE_VOLUME_DOWN | (pData->_parameter&0xF);
					}
					else {
						type = XMCMD::VOLUMESLIDE;
						param = pData->_parameter;
					}
					break;
				case XMSampler::CMD::VOLUME:
					vol = 0x10 + ((pData->_parameter <= 0x80) ? (pData->_parameter >> 1) : 0x40);
					break;
				case XMSampler::CMD::EXTENDED:
					switch(pData->_parameter&0xF0) {
						case XMSampler::CMD_E::E_GLISSANDO_TYPE: 
							type = XMCMD::EXTENDED;
							param = XMCMD_E::E_GLISSANDO_STATUS | ((pData->_parameter==0)?0:1);
							break;
						case XMSampler::CMD_E::E_DELAYED_NOTECUT:
							type = XMCMD::EXTENDED;
							param = XMCMD_E::E_DELAYED_NOTECUT | (pData->_parameter&0xF);
							break;
						case XMSampler::CMD_E::E_NOTE_DELAY:
							type = XMCMD::EXTENDED;
							param = XMCMD_E::E_NOTE_DELAY | (pData->_parameter&0xf);
							break;
						default:
							break;
					}
					break;
				case XMSampler::CMD::RETRIG:
					type = XMCMD::EXTENDED;
					param = XMCMD_E::E_MOD_RETRIG | (pData->_parameter&0xF);
					break;
				case XMSampler::CMD::SENDTOVOLUME:
					if (pData->_parameter < 0x40) {
						vol = 0x10 + ((pData->_parameter < 0x40) ? pData->_parameter : 0x40);
					}
					else if ((pData->_parameter&0xF0) == XMSampler::CMD_VOL::VOL_VOLSLIDEUP) {
						vol = XMVOL_CMD::XMV_VOLUMESLIDEUP|(pData->_parameter&0x0F);
					}
					else if ((pData->_parameter&0xF0) == XMSampler::CMD_VOL::VOL_VOLSLIDEDOWN) {
						vol = XMVOL_CMD::XMV_VOLUMESLIDEDOWN|(pData->_parameter&0x0F);
					}
					else if ((pData->_parameter&0xF0) == XMSampler::CMD_VOL::VOL_FINEVOLSLIDEUP) {
						vol = XMVOL_CMD::XMV_FINEVOLUMESLIDEUP|(pData->_parameter&0x0F);
					}
					else if ((pData->_parameter&0xF0) == XMSampler::CMD_VOL::VOL_FINEVOLSLIDEDOWN) {
						vol = XMVOL_CMD::XMV_FINEVOLUMESLIDEDOWN|(pData->_parameter&0x0F);
					}
					else if ((pData->_parameter&0xF0) == XMSampler::CMD_VOL::VOL_PANNING) {
						vol = XMVOL_CMD::XMV_PANNING|(pData->_parameter&0x0F);
					}
					else if ((pData->_parameter&0xF0) == XMSampler::CMD_VOL::VOL_PANSLIDELEFT) {
						vol = XMVOL_CMD::XMV_PANNINGSLIDELEFT|(((pData->_parameter&0x0F) > 3)?0xF:(pData->_parameter&0x0F)<<2);
					}
					else if ((pData->_parameter&0xF0) == XMSampler::CMD_VOL::VOL_PANSLIDERIGHT) {
						vol = XMVOL_CMD::XMV_PANNINGSLIDERIGHT|(((pData->_parameter&0x0F) > 3)?0xF:(pData->_parameter&0x0F)<<2);
					}
					else if ((pData->_parameter&0xF0) == XMSampler::CMD_VOL::VOL_VIBRATO) {
						vol = XMVOL_CMD::XMV_VIBRATO|(pData->_parameter&0x0F);
					}
					else if ((pData->_parameter&0xF0) == XMSampler::CMD_VOL::VOL_TONEPORTAMENTO) {
						vol = XMVOL_CMD::XMV_PORTA2NOTE|(pData->_parameter&0x0F);
					}
					break;
				default:
					break;
			}
		}
		else if (lastInstr[i] != -1 && lastInstr[i] > macInstruments+xmInstruments)
		{ // Sampler
			switch(pData->_cmd) {
				case 0x01:
					type = XMCMD::PORTAUP;
					param = pData->_parameter >> 1; // There isn't a correct conversion. It is non-linear, but acting on speed, not period, and based on sample rate.
					break;
				case 0x02:
					type = XMCMD::PORTADOWN;
					param = pData->_parameter >> 1; // There isn't a correct conversion. It is non-linear, but acting on speed, not period, and based on sample rate.
					break;
				case 0x08:
					type = XMSampler::CMD::PANNING;
					param = pData->_parameter;
					break;
				case 0x09:
					type = XMSampler::CMD::OFFSET;
					param = pData->_parameter;  //The offset in sampler is of the whole sample size, not a fixed amount.
					break;
				case 0x0C:
					vol = 0x10 + (pData->_parameter >> 2);
					break;
				case 0x0E: {
					switch(pData->_parameter&0xF0) {
						case 0xC0:
							type = XMCMD_E::E_DELAYED_NOTECUT;
							param = (pData->_parameter*song.LinesPerBeat()) >>2;
							break;
						case 0xD0:
							type = XMCMD_E::E_NOTE_DELAY;
							param = (pData->_parameter*song.LinesPerBeat()) >> 2;
							break;
						default:
							break;
					}
					break;
				}
				case 0x15:
					type = XMCMD::RETRIG;
					param = pData->_parameter;
					break;
				default:
					break;
			}
		}
		else if (pData->_cmd == 0x0C) {
			vol = 0x10 + (pData->_parameter >> 2);
		}
	}
	

	void XMSongExport::SaveEmptyInstrument(const std::string& name)
	{
		XMINSTRUMENTHEADER insHeader;
		memset(&insHeader,0,sizeof(insHeader));
		//insHeader.type = 0; Implicit by memset
		insHeader.size = sizeof(insHeader);
		strncpy(insHeader.name,name.c_str(),22); //Names are not null terminated
		//insHeader.samples = 0; Implicit by memset
		Write(&insHeader,sizeof(insHeader));
	}
	void XMSongExport::SaveSampulseInstrument(const Song& song, int instIdx)
	{
		const XMInstrument &inst = song.xminstruments[instIdx];
		XMINSTRUMENTHEADER insHeader;
		memset(&insHeader,0,sizeof(insHeader));
		strncpy(insHeader.name,inst.Name().c_str(),22); //Names are not null terminated
		//insHeader.type = 0; Implicit by memset
		XMSAMPLEHEADER samphead;
		std::memset(&samphead, 0, sizeof(samphead));

		std::list<unsigned char> sampidxs;
		std::map<unsigned char,unsigned char> sRemap;
		unsigned char sample=0;
		for ( int i=12; i < 108; i++) {
			const XMInstrument::NotePair &pair = inst.NoteToSample(i);
			if (pair.second != 255 ) {
				std::map<unsigned char,unsigned char>::const_iterator ite = sRemap.find(pair.second);
				if ( ite == sRemap.end() ) {
					sampidxs.push_back(pair.second);
					sRemap[pair.second] = sample;
					samphead.snum[i-12] = sample;
					sample++;
				}
				else {
					samphead.snum[i-12] = ite->second;
				}
			}
		}

		//If no samples for this instrument, write it and exit.
		if (sampidxs.size() == 0) {
			insHeader.size = sizeof(insHeader);
			//insHeader.samples = 0; Implicit by memset
			Write(&insHeader,sizeof(insHeader));
		}
		else {
			insHeader.size = sizeof(insHeader) + sizeof(XMSAMPLEHEADER);
			insHeader.samples = static_cast<uint16_t>(sampidxs.size());
			Write(&insHeader,sizeof(insHeader));

			SetSampulseEnvelopes(song, instIdx,samphead);
			samphead.volfade = static_cast<uint16_t>(floor(inst.VolumeFadeSpeed()*32768.0f));
			samphead.shsize = sizeof(XMSAMPLESTRUCT);
			Write(&samphead,sizeof(samphead));

			for (std::list<unsigned char>::const_iterator ite = sampidxs.begin(); ite != sampidxs.end(); ++ite) {
				SaveSampleHeader(song, static_cast<int>(*ite));
			}
			for (std::list<unsigned char>::const_iterator ite = sampidxs.begin(); ite != sampidxs.end(); ++ite) {
				SaveSampleData(song, static_cast<int>(*ite));
			}
		}
	}



	void XMSongExport::SaveSamplerInstrument(const Song& song, int instIdx)
	{
		XMINSTRUMENTHEADER insHeader;
		memset(&insHeader,0,sizeof(insHeader));
		strncpy(insHeader.name,song.samples[instIdx].WaveName().c_str(),22); //Names are not null terminated
		//insHeader.type = 0; Implicit by memset

		//If it has samples, add the whole header.
		if (song.samples.IsEnabled(instIdx)) {
			insHeader.size = sizeof(insHeader) + sizeof(XMSAMPLEHEADER);
			insHeader.samples = 1;
			Write(&insHeader,sizeof(insHeader));

			XMSAMPLEHEADER _samph;
			std::memset(&_samph, 0, sizeof(_samph));
			SetSamplerEnvelopes(song,instIdx,_samph);
			_samph.volfade=0x400;
			_samph.shsize = sizeof(XMSAMPLESTRUCT);
			Write(&_samph,sizeof(_samph));

			SaveSampleHeader(song, instIdx);
			SaveSampleData(song, instIdx);
		}
		else {
			insHeader.size = sizeof(insHeader);
			//insHeader.samples = 0; Implicit by memset
			Write(&insHeader,sizeof(insHeader));
		}
	}

	void XMSongExport::SaveSampleHeader(const Song& song, int instIdx)
	{
		const XMInstrument::WaveData<>& wave = song.samples[instIdx];

		XMSAMPLESTRUCT stheader;
		memset(&stheader,0,sizeof(stheader));
		strncpy(stheader.name,song.samples[instIdx].WaveName().c_str(),22); //Names are not null terminated
		// stheader.res Implicitely set at zero by memset

		int tune = wave.WaveTune();
		int finetune = static_cast<int>((float)wave.WaveFineTune()*1.28);
		if (wave.WaveSampleRate() != 8363) {
			//correct the tuning
			double newtune = log10(double(wave.WaveSampleRate())/8363.0)/log10(2.0);
			double floortune = floor(newtune*12.0);
			tune += static_cast<int>(floortune);
			finetune += static_cast<int>(floor(((newtune*12.0)-floortune)*128.0));
			if (finetune > 127) { tune++; finetune -=127; }
		}

		//All samples are 16bits in Psycle.
		stheader.samplen = wave.WaveLength() *2;
		stheader.loopstart = wave.WaveLoopStart() * 2;
		stheader.looplen = (wave.WaveLoopEnd() - wave.WaveLoopStart()) * 2;
		stheader.vol = wave.WaveVolume()>>1;
		stheader.relnote = tune;
		stheader.finetune = finetune;

		uint8_t type = 0;
		if (wave.WaveLoopType()==XMInstrument::WaveData<>::LoopType::NORMAL) type = 1;
		else if (wave.WaveLoopType()==XMInstrument::WaveData<>::LoopType::BIDI) type = 2;
		type |= 0x10; // 0x10 -> 16bits
		stheader.type = type;
		stheader.pan = static_cast<uint8_t>(wave.PanFactor()*255.f);

		Write(&stheader,sizeof(stheader));
	}
	
	void XMSongExport::SaveSampleData(const Song& song,const int instrIdx)
	{
		const XMInstrument::WaveData<>& wave = song.samples[instrIdx];
		// pack sample data
		short* samples = wave.pWaveDataL();
		int length = wave.WaveLength();
		short prev=0;
		for(int j=0;j<length;j++)
		{
			short delta =  samples[j] - prev;
			//This is expected to be in little endian.
			Write(&delta,sizeof(short));
			prev = samples[j];
		} 
	}

	
	void XMSongExport::SetSampulseEnvelopes(const Song& song, int instrIdx, XMSAMPLEHEADER & sampleHeader)
	{
		sampleHeader.vtype = 0;
		const XMInstrument &inst = song.xminstruments[instrIdx];

		if ( inst.AmpEnvelope().IsEnabled() ) {
			sampleHeader.vtype = 1;
			const XMInstrument::Envelope & env = inst.AmpEnvelope();

			 // Max number of envelope points in Fasttracker format is 12.
			sampleHeader.vnum = std::min(12u, env.NumOfPoints());
			float convert = 1.f;
			if (env.Mode() == XMInstrument::Envelope::Mode::MILIS) {
				convert = (24.f*static_cast<float>(song.BeatsPerMin()))/60000.f;
			}
			// Format of FastTracker points is :
			// Point : frame number. ( 1 frame= line*(24/TPB), samplepos= frame*(samplesperrow*TPB/24))
			// Value : 0..64. , divide by 64 to use it as a multiplier.
			int idx=0;
			for (unsigned int i=0; i < env.NumOfPoints() && i < 12;i++) {
				sampleHeader.venv[idx]=static_cast<uint16_t>(env.GetTime(i)*convert); idx++;
				sampleHeader.venv[idx]=static_cast<uint16_t>(env.GetValue(i)*64.f); idx++;
			}

			if (env.SustainBegin() != XMInstrument::Envelope::INVALID) {
				sampleHeader.vtype |= 2;
				sampleHeader.vsustain = static_cast<uint16_t>(env.SustainBegin());
			}
			if (env.LoopStart() != XMInstrument::Envelope::INVALID) {
				sampleHeader.vtype |= 4;
				sampleHeader.vloops = static_cast<uint16_t>(env.LoopStart());
				sampleHeader.vloope = static_cast<uint16_t>(env.LoopEnd());
			}
		}
		if ( inst.PanEnvelope().IsEnabled() ) {
			sampleHeader.ptype = 1;
			const XMInstrument::Envelope & env = inst.PanEnvelope();

			 // Max number of envelope points in Fasttracker format is 12.
			sampleHeader.pnum = std::min(12u, env.NumOfPoints());
			float convert = 1.f;
			if (env.Mode() == XMInstrument::Envelope::Mode::MILIS) {
				convert = (24.f*static_cast<float>(song.BeatsPerMin()))/60000.f;
			}
			// Format of FastTracker points is :
			// Point : frame number. ( 1 frame= line*(24/TPB), samplepos= frame*(samplesperrow*TPB/24))
			// Value : 0..64. , divide by 64 to use it as a multiplier.
			int idx=0;
			for (unsigned int i=0; i < env.NumOfPoints() && i < 12;i++) {
				sampleHeader.penv[idx]=static_cast<uint16_t>(env.GetTime(i)*convert); idx++;
				sampleHeader.penv[idx]=static_cast<uint16_t>(32+(env.GetValue(i)*32.f)); idx++;
			}

			if (env.SustainBegin() != XMInstrument::Envelope::INVALID) {
				sampleHeader.ptype |= 2;
				sampleHeader.psustain = static_cast<uint16_t>(env.SustainBegin());
			}
			if (env.LoopStart() != XMInstrument::Envelope::INVALID) {
				sampleHeader.ptype |= 4;
				sampleHeader.ploops = static_cast<uint16_t>(env.LoopStart());
				sampleHeader.ploope = static_cast<uint16_t>(env.LoopEnd());
			}
		}
	}	
	void XMSongExport::SetSamplerEnvelopes(const Song& song, int instrIdx, XMSAMPLEHEADER & sampleHeader)
	{
		sampleHeader.vtype = 0;
		Instrument *inst = song._pInstrument[instrIdx];

		if (inst->ENV_AT != 1 || inst->ENV_DT != 1 || inst->ENV_SL != 100 || inst->ENV_RT != 220) {
			sampleHeader.vtype = 3;
			sampleHeader.vsustain = 1;

			sampleHeader.vnum = 4;
			float convert = static_cast<float>(song.BeatsPerMin())*24.f/(44100.f*60.f);
			// Format of FastTracker points is :
			// Point : frame number. ( 1 frame= line*(24/TPB), samplepos= frame*(samplesperrow*TPB/24))
			// Value : 0..64. , divide by 64 to use it as a multiplier.
			int idx=0;
			sampleHeader.venv[idx]=0; idx++;
			sampleHeader.venv[idx]=0; idx++;
			sampleHeader.venv[idx]=static_cast<uint16_t>(inst->ENV_AT*convert); idx++;
			sampleHeader.venv[idx]=64; idx++;
			sampleHeader.venv[idx]=static_cast<uint16_t>((inst->ENV_DT+inst->ENV_DT)*convert); idx++;
			sampleHeader.venv[idx]=static_cast<uint16_t>(inst->ENV_SL*0.64f); idx++;
			sampleHeader.venv[idx]=static_cast<uint16_t>((inst->ENV_RT+inst->ENV_DT+inst->ENV_RT)*convert); idx++;
			sampleHeader.venv[idx]=0; idx++;
		}
	}


}
}

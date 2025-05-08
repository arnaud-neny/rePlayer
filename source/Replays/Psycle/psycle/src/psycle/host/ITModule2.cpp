#include <psycle/host/detail/project.private.hpp>
#include "ITModule2.h"
#include "Configuration.hpp"

#include "Player.hpp"
#include "Song.hpp"
#include "XMSampler.hpp"
#if !defined WINAMP_PLUGIN
	#include "ProgressDialog.hpp"
#else
	#include "player_plugins/winamp/fake_progressDialog.hpp"
#endif //!defined WINAMP_PLUGIN

#include <algorithm>
namespace psycle
{
	namespace host
	{
		ITModule2::ITModule2(void)
		{
			embeddedData = NULL;
			for (int i=0;i<64;i++) highOffset[i]=0;
		}

		ITModule2::~ITModule2(void)
		{
			delete embeddedData;
		}
		bool ITModule2::BitsBlock::ReadBlock(OldPsyFile *pFile)
		{
			// block layout : uint16 size, <size> bytes data
			uint16_t size;
			pFile->Read(&size,2);
			pdata = new unsigned char[size];
			if (!pdata) return false;
			if (!pFile->Read(pdata,size))
			{
				delete[] pdata;
				return false;
			}
			rpos=pdata;
			rend=pdata+size;
			rembits=8;
			return true;
		}

		uint32_t ITModule2::BitsBlock::ReadBits(unsigned char bitwidth)
		{
			uint32_t val = 0;
			int b = 0;

			// If reached the end of the buffer, exit.
			if (rpos >= rend) return val;

			// while we have more bits to read than the remaining bits in this byte
			while (bitwidth > rembits) {
				// add to val, the data-bits from rpos, shifting the necessary number of bits.
				val |= *rpos++ << b;
				//if reached the end, exit.
				if (rpos >= rend) return val;
				// increment the shift
				b += rembits;
				// decrease the remaining bits to read
				bitwidth -= rembits;
				// set back the number of bits.
				rembits = 8;
			}
			// Filter the bottom-most bitwidth bytes from rpos, and shift them by b to add to the final value.
			val |= (*rpos & ((1 << bitwidth) - 1)) << b;
			// shift down the remaining bits so that they are read the next time.
			*rpos >>= bitwidth;
			// reduce the remaining bits.
			rembits -= bitwidth;

			return val;
		}
		int ITModule2::LoadInstrumentFromFile(LoaderHelper& loadhelp)
		{
			itFileH.flags=0;
			itInsHeader1x inshead;
			Read(&inshead,sizeof(itInsHeader1x));
			int instIdx = -1;
			XMInstrument& instr = loadhelp.GetNextInstrument(instIdx);
			if (inshead.trackerV < 0x200 ) {
				LoadOldITInst(inshead,instr);
			}
			else {
				LoadITInst(*reinterpret_cast<itInsHeader2x*>(&inshead),instr);
				//From ITTECH: Total length of an instrument is 547 bytes, but 554 bytes are
				//written, just to simplify the loading of the old format.
			}
			if (inshead.noS < 1 && !Eof()) {
				inshead.noS = 0;
				//The header sample count might be wrong. Let's recalculate it.
				std::size_t curpos = GetPos();
				while(Expect(&IMPS_ID,sizeof(IMPS_ID))) {
					inshead.noS++;
					Skip(sizeof(itSampleHeader)-sizeof(IMPS_ID));
				}
				Seek(curpos);
			}
			int sample=-1;
			if (loadhelp.IsPreview()) { // If preview, determine which sample to  load.
				sample = instr.NoteToSample(notecommands::middleC).second;
				if (sample == notecommands::empty) {
					const std::set<int>& samps = instr.GetWavesUsed();
					if (!samps.empty()) {
						sample = *samps.begin();
					}
				}
			}
			std::vector<unsigned char> sRemap;
			for (unsigned int i(0); i<inshead.noS; i++)
			{
				//todo: finish for preview (select appropiate sample)
				int sampIdx=-1;
				XMInstrument::WaveData<>& wave = loadhelp.GetNextSample(sampIdx);
				std::size_t curpos = GetPos();
				LoadITSample(wave);
				Seek(curpos+sizeof(itSampleHeader));
				// Only get REAL samples.
				if ( wave.WaveLength() > 0) {
					sRemap.push_back(sampIdx);
				}
				else { sRemap.push_back(MAX_INSTRUMENTS-1); }
				if (i == sample) {
					break;
				}
			}
			if (!loadhelp.IsPreview()) {
				for(int i = 0;i < XMInstrument::NOTE_MAP_SIZE;i++)
				{
					XMInstrument::NotePair npair = instr.NoteToSample(i);
					npair.second=(npair.second<inshead.noS) ? sRemap[npair.second] : 255;
					instr.NoteToSample(i,npair);
				}
			}
			instr.ValidateEnabled();
			return instIdx;
		}
		bool ITModule2::Load(Song &song,CProgressDialog& /*progress*/, bool /*fullopen*/)
		{
			int read = Read(&itFileH,sizeof(itFileH));
			this->Seek(0);
			if (read>0 && itFileH.tag == IMPM_ID ) {
				return LoadITModule(song);
			}
			read = Read(&s3mFileH,sizeof(s3mFileH));
			this->Seek(0);
			if (read >0 && s3mFileH.tag == SCRM_ID && s3mFileH.type == 0x10 ) {
				return LoadS3MModuleX(song);
			}
			return false;
		}
		bool ITModule2::LoadITModule(Song& song)
		{
			CExclusiveLock lock(&song.semaphore, 2, true);
			_pSong=&song;
			if (Read(&itFileH,sizeof(itFileH))==0 ) return false;
			if (itFileH.tag != IMPM_ID ) return false;

			song.name = itFileH.songName;
			song.author = "";
			song.comments = "Imported from Impulse Tracker Module: ";
			song.comments.append(szName);

			song.CreateMachine(MACH_XMSAMPLER, rand()/64, rand()/80, "sampulse",0);
			song.InsertConnectionNonBlocking(0,MASTER_INDEX,0,0,value_mapper::map_128_1(itFileH.mVol>128?128:itFileH.mVol));
			song.seqBus=0;
			XMSampler* sampler = ((XMSampler*)song._pMachine[0]);

			song.BeatsPerMin(itFileH.iTempo);
			song.TicksPerBeat(24);
			int extraticks=0;
			song.LinesPerBeat(XMSampler::CalcLPBFromSpeed(itFileH.iSpeed,extraticks));
			if (extraticks != 0) {
				song.ExtraTicksPerLine(extraticks);
			}

			sampler->IsAmigaSlides(itFileH.flags&Flags::LINEARSLIDES?false:true);
			sampler->GlobalVolume(itFileH.gVol);
/*
Flags:    Bit 0: On = Stereo, Off = Mono
			Bit 1: Vol0MixOptimizations - If on, no mixing occurs if
			the volume at mixing time is 0 (redundant v1.04+)
			Bit 2: On = Use instruments, Off = Use samples.
			Bit 3: On = Linear slides, Off = Amiga slides.
			Bit 4: On = Old Effects, Off = IT Effects
Differences:
		- Vibrato is updated EVERY frame in IT mode, whereas
			it is updated every non-row frame in other formats.
			Also, it is two times deeper with Old Effects ON
			- Command Oxx will set the sample offset to the END
			of a sample instead of ignoring the command under
			old effects mode.
			- (More to come, probably)
			Bit 5: On = Link Effect G's memory with Effect E/F. Also
			Gxx with an instrument present will cause the
			envelopes to be retriggered. If you change a
			sample on a row with Gxx, it'll adjust the
			frequency of the current note according to:

		  NewFrequency = OldFrequency * NewC5 / OldC5;
		  Bit 6: Use MIDI pitch controller, Pitch depth given by PWD
			  Bit 7: Request embedded MIDI configuration
			  (Coded this way to permit cross-version saving)

Special:  Bit 0: On = song message attached.
		  Song message:
		  Stored at offset given by "Message Offset" field.
			  Length = MsgLgth.
			  NewLine = 0Dh (13 dec)
			  EndOfMsg = 0

		Note: v1.04+ of IT may have song messages of up to
			8000 bytes included.
			Bit 1: Reserved
			Bit 2: Reserved
			Bit 3: MIDI configuration embedded
			Bit 4-15: Reserved

*/
			bool stereo=itFileH.flags&Flags::STEREO;
			int i,j;
			for (i=0;i<64;i++)
			{
				if (stereo && !(itFileH.chanPan[i]&ChanFlags::IS_DISABLED))
				{
					if (itFileH.chanPan[i]==ChanFlags::IS_SURROUND )
					{
						sampler->rChannel(i).DefaultPanFactorFloat(0.5f,true);
						sampler->rChannel(i).DefaultIsSurround(true);
					}
					else {
						sampler->rChannel(i).DefaultPanFactorFloat((itFileH.chanPan[i]&0x7F)/64.0f,true);
					}
				}
				else{
					sampler->rChannel(i).DefaultPanFactorFloat(0.5f,true);
				}
				sampler->rChannel(i).DefaultVolumeFloat(itFileH.chanVol[i]/64.0f);
				if ( (itFileH.chanPan[i]&ChanFlags::IS_DISABLED) ) 
				{
					sampler->rChannel(i).DefaultIsMute(true);
				}
				else {
					sampler->rChannel(i).DefaultIsMute(false);
					m_maxextracolumn=i;
				}
				sampler->rChannel(i).DefaultFilterType(dsp::F_ITLOWPASS);
			}
			if(m_maxextracolumn==63) { m_maxextracolumn=15; }

			i=0;
			for (j=0;j<itFileH.ordNum && i<MAX_SONG_POSITIONS;j++)
			{
				song.playOrder[i]=ReadUInt8(); // 254 = ++ (skip), 255 = --- (end of tune).
				if (song.playOrder[i]!= 254 &&song.playOrder[i] != 255 ) i++;
			}
			Skip(itFileH.ordNum-j);

			song.playLength=i;
			if ( song.playLength == 0) // Add at least one pattern to the sequence.
			{
				song.playLength = 1;
				song.playOrder[0]=0;
			}

			uint32_t *pointersi = new uint32_t[itFileH.insNum];
			Read(pointersi,itFileH.insNum*sizeof(uint32_t));
			uint32_t * pointerss = new uint32_t[itFileH.sampNum];
			Read(pointerss,itFileH.sampNum*sizeof(uint32_t));
			uint32_t * pointersp = new uint32_t[itFileH.patNum];
			Read(pointersp,itFileH.patNum*sizeof(uint32_t));

			if ( itFileH.special&SpecialFlags::MIDIEMBEDDED)
			{
				embeddedData = new EmbeddedMIDIData;
				short skipnum;
				Read(&skipnum,sizeof(short));
				Skip(skipnum*8); // This is some strange data. It is not documented.

				Read(embeddedData,sizeof(EmbeddedMIDIData));

				for ( int i=0; i<128; i++ )
				{
					std::string zxx = embeddedData->Zxx[i];
					std::string zxx2 = zxx.substr(0,5);
					if ( zxx2 == "F0F00")
					{
						int mode=0;
						if (embeddedData->Zxx[i][5] >= '0' && embeddedData->Zxx[i][5] <= '9')
							mode = embeddedData->Zxx[i][5] - '0';
						else if (embeddedData->Zxx[i][5] >= 'A' && embeddedData->Zxx[i][5] <= 'F')
							mode = embeddedData->Zxx[i][5] - 'A' + 0xA;

						int tmp=0;
						if (embeddedData->Zxx[i][6] >= '0' && embeddedData->Zxx[i][6] <= '9')
							tmp = (embeddedData->Zxx[i][6] - '0') * 0x10;
						else if (embeddedData->Zxx[i][6] >= 'A' && embeddedData->Zxx[i][6] <= 'F')
							tmp = (embeddedData->Zxx[i][6] - 'A' + 0xA) * 0x10;
						if (embeddedData->Zxx[i][7] >= '0' && embeddedData->Zxx[i][7] <= '9')
							tmp += (embeddedData->Zxx[i][7] - '0');
						else if (embeddedData->Zxx[i][7] >= 'A' && embeddedData->Zxx[i][7] <= 'F')
							tmp += (embeddedData->Zxx[i][7] - 'A' + 0xA);

						sampler->SetZxxMacro(i,mode,tmp);
					}
				}
			}
			else // initializing with the default midi.cfg values.
			{
				for(i = 0; i < 16;i++)
				{
					sampler->SetZxxMacro(i,1,i*8);
				}

				for(i = 16; i < 128;i++)
				{
					sampler->SetZxxMacro(i,-1,0);
				}
			}

			if ( itFileH.special&SpecialFlags::HASMESSAGE)
			{
				Seek(itFileH.msgOffset);
				char comments_[65536];
				// NewLine = 0Dh (13 dec)
				// EndOfMsg = 0
				Read(comments_,std::min(sizeof(comments_)-song.comments.length(), static_cast<std::size_t>(itFileH.msgLen)));
				comments_[65535]='\0';
				std::string temp = comments_;
#if !defined DIVERSALIS__OS__APPLE
				size_t pos = 0;
				char bla = '\r';
#ifdef DIVERSALIS__OS__MICROSOFT
				while ((pos = temp.find(bla, pos)) != std::string::npos) {
					 temp.replace(pos, 1, "\r\n");
					 pos += 2;
				}
				song.comments.append("\r\n").append(temp);
#else
				while ((pos = temp.find(bla, pos)) != std::string::npos) {
					 temp.replace(pos, 1, "\n");
					 pos += 1;
				}
				song.comments.append("\n").append(temp);
#endif
#else 
				song.comments.append("\r").append(temp);
#endif
			}

			int virtualInst = MAX_MACHINES;
			std::map<int,int> ittovirtual;
			for (i=0;i<itFileH.insNum;i++)
			{
				Seek(pointersi[i]);
				XMInstrument instr;
				if (itFileH.ffv < 0x200 ) {
					itInsHeader1x curH;
					Read(&curH,sizeof(curH));
					LoadOldITInst(curH,instr);
				}
				else {
					itInsHeader2x curH;
					Read(&curH,sizeof(curH));
					LoadITInst(curH, instr);
				}
				song.xminstruments.SetInst(instr,i);
				if (song.xminstruments.IsEnabled(i)) {
					ittovirtual[i]=virtualInst;
					song.SetVirtualInstrument(virtualInst++,0,i);
				}
			}
			for (i=0;i<itFileH.sampNum;i++)
			{
				Seek(pointerss[i]);
				XMInstrument::WaveData<> wave;
				bool created = LoadITSample(wave);
				// If this IT file doesn't use Instruments, we need to map the notes manually.
				if (created && !(itFileH.flags & Flags::USEINSTR)) 
				{
					if (song.xminstruments.IsEnabled(i) ==false) {
						XMInstrument instr;
						instr.Init();
						song.xminstruments.SetInst(instr,i);
					}
					XMInstrument& instrument = song.xminstruments.get(i);
					instrument.Name(wave.WaveName());
					XMInstrument::NotePair npair;
					npair.second=i;
					for(int j = 0;j < XMInstrument::NOTE_MAP_SIZE;j++){
						npair.first=j;
						instrument.NoteToSample(j,npair);
					}
					instrument.ValidateEnabled();
					ittovirtual[i]=virtualInst;
					song.SetVirtualInstrument(virtualInst++,0,i);
				}
				song.samples.SetSample(wave,i);
			}
			int numchans=m_maxextracolumn;
			m_maxextracolumn=0;
			for (i=0;i<itFileH.patNum;i++)
			{
				if (pointersp[i]==0)
				{
					song.AllocNewPattern(i,"unnamed",64,false);
				} else {
					Seek(pointersp[i]);
					LoadITPattern(i,numchans, ittovirtual);
				}
			}
			song.SONGTRACKS = std::max(numchans+1,(int)m_maxextracolumn);

			delete[] pointersi;
			delete[] pointerss;
			delete[] pointersp;
			
			return true;
		}

		bool ITModule2::LoadOldITInst(const itInsHeader1x& curH,XMInstrument &xins)
		{
			std::string itname(curH.sName);
			xins.Name(itname);

			xins.VolumeFadeSpeed(curH.fadeout/512.0f);

			xins.NNA((XMInstrument::NewNoteAction::Type)curH.NNA);
			if ( curH.DNC )
			{	
				xins.DCT(XMInstrument::DupeCheck::NOTE);
				xins.DCA(XMInstrument::NewNoteAction::STOP);
			}
			XMInstrument::NotePair npair;
			int i;
			for(i = 0;i < XMInstrument::NOTE_MAP_SIZE;i++){
				npair.first=curH.notes[i].first;
				npair.second=curH.notes[i].second-1;
				xins.NoteToSample(i,npair);
			}
			xins.AmpEnvelope().Init();
			if(curH.flg & EnvFlags::USE_ENVELOPE){// enable volume envelope
				xins.AmpEnvelope().IsEnabled(true);

				if(curH.flg& EnvFlags::USE_SUSTAIN){
					xins.AmpEnvelope().SustainBegin(curH.sustainS);
					xins.AmpEnvelope().SustainEnd(curH.sustainE);
				}

				if(curH.flg & EnvFlags::USE_LOOP){
					xins.AmpEnvelope().LoopStart(curH.loopS);
					xins.AmpEnvelope().LoopEnd(curH.loopE);
				}
				for(int i = 0; i < 25;i++){
					uint8_t tick = curH.nodepair[i].first;
					uint8_t value = curH.nodepair[i].second;
					if (value == 0xFF || tick == 0xFF) break;

					xins.AmpEnvelope().Append(tick,(float)value/ 64.0f);
				}
			}
			xins.PanEnvelope().Init();
			xins.PitchEnvelope().Init();
			xins.FilterEnvelope().Init();
			xins.ValidateEnabled();
			return true;
		}
		bool ITModule2::LoadITInst(const itInsHeader2x& curH, XMInstrument &xins)
		{
			std::string itname(curH.sName);
			xins.Name(itname);

			xins.NNA((XMInstrument::NewNoteAction::Type)curH.NNA);
			xins.DCT((XMInstrument::DupeCheck::Type)curH.DCT);
			switch (curH.DCA)
			{
			case DCAction::NOTEOFF:xins.DCA(XMInstrument::NewNoteAction::NOTEOFF);break;
			case DCAction::FADEOUT:xins.DCA(XMInstrument::NewNoteAction::FADEOUT);break;
			case DCAction::STOP://fallthrough
			default:xins.DCA(XMInstrument::NewNoteAction::NOTEOFF);break;
			}

			xins.Pan((curH.defPan & 0x7F)/64.0f);
			xins.PanEnabled((curH.defPan & 0x80)?false:true);
			xins.NoteModPanCenter(curH.pPanCenter);
			xins.NoteModPanSep(curH.pPanSep);
			xins.GlobVol(curH.gVol/127.0f);
			xins.VolumeFadeSpeed(curH.fadeout/1024.0f);
			xins.RandomVolume(curH.randVol/100.f);
			xins.RandomPanning(curH.randPan/64.f);
			if ( (curH.inFC&0x80) != 0)
			{
				xins.FilterType(dsp::F_ITLOWPASS);
				xins.FilterCutoff(curH.inFC&0x7F);
			}
			if ((curH.inFR&0x80) != 0)
			{
				xins.FilterType(dsp::F_ITLOWPASS);
				xins.FilterResonance(curH.inFR&0x7F);
			}


			XMInstrument::NotePair npair;
			int i;
			for(i = 0;i < XMInstrument::NOTE_MAP_SIZE;i++){
				npair.first=curH.notes[i].first;
				npair.second=curH.notes[i].second-1;
				xins.NoteToSample(i,npair);
			}

			// volume envelope
			xins.AmpEnvelope().Init();
			xins.AmpEnvelope().Mode(XMInstrument::Envelope::Mode::TICK);
			xins.AmpEnvelope().IsEnabled(curH.volEnv.flg & EnvFlags::USE_ENVELOPE);
			if(curH.volEnv.flg& EnvFlags::ENABLE_CARRY) xins.AmpEnvelope().IsCarry(true);
			if(curH.volEnv.flg& EnvFlags::USE_SUSTAIN){
				xins.AmpEnvelope().SustainBegin(curH.volEnv.sustainS);
				xins.AmpEnvelope().SustainEnd(curH.volEnv.sustainE);
			}

			if(curH.volEnv.flg & EnvFlags::USE_LOOP){
				xins.AmpEnvelope().LoopStart(curH.volEnv.loopS);
				xins.AmpEnvelope().LoopEnd(curH.volEnv.loopE);
			}

			int envelope_point_num = curH.volEnv.numP;
			if(envelope_point_num > 25){
				envelope_point_num = 25;
			}

			for(int i = 0; i < envelope_point_num;i++){
				short envtmp = curH.volEnv.nodes[i].secondlo | (curH.volEnv.nodes[i].secondhi <<8);
				xins.AmpEnvelope().Append(envtmp ,(float)curH.volEnv.nodes[i].first/ 64.0f);
			}

			// Pan envelope
			xins.PanEnvelope().Init();
			xins.PanEnvelope().Mode(XMInstrument::Envelope::Mode::TICK);
			xins.PanEnvelope().IsEnabled(curH.panEnv.flg & EnvFlags::USE_ENVELOPE);
			if(curH.panEnv.flg& EnvFlags::ENABLE_CARRY) xins.PanEnvelope().IsCarry(true);
			if(curH.panEnv.flg& EnvFlags::USE_SUSTAIN){
				xins.PanEnvelope().SustainBegin(curH.panEnv.sustainS);
				xins.PanEnvelope().SustainEnd(curH.panEnv.sustainE);
			}

			if(curH.panEnv.flg & EnvFlags::USE_LOOP){
				xins.PanEnvelope().LoopStart(curH.panEnv.loopS);
				xins.PanEnvelope().LoopEnd(curH.panEnv.loopE);
			}

			envelope_point_num = curH.panEnv.numP;
			if(envelope_point_num > 25){ // Max number of envelope points in Impulse format is 25.
				envelope_point_num = 25;
			}

			for(int i = 0; i < envelope_point_num;i++){
				short pantmp = curH.panEnv.nodes[i].secondlo | (curH.panEnv.nodes[i].secondhi <<8);
				xins.PanEnvelope().Append(pantmp,(float)(curH.panEnv.nodes[i].first)/ 32.0f);
			}

			// Pitch/Filter envelope
			xins.PitchEnvelope().Init();
			xins.PitchEnvelope().Mode(XMInstrument::Envelope::Mode::TICK);
			xins.FilterEnvelope().Init();
			xins.FilterEnvelope().Mode(XMInstrument::Envelope::Mode::TICK);

			envelope_point_num = curH.pitchEnv.numP;
			if(envelope_point_num > 25){ // Max number of envelope points in Impulse format is 25.
				envelope_point_num = 25;
			}

			if (curH.pitchEnv.flg & EnvFlags::ISFILTER)
			{
				xins.FilterType(dsp::F_ITLOWPASS);
				xins.FilterEnvelope().IsEnabled(curH.pitchEnv.flg & EnvFlags::USE_ENVELOPE);
				xins.PitchEnvelope().IsEnabled(false);
				if(curH.pitchEnv.flg& EnvFlags::ENABLE_CARRY) xins.FilterEnvelope().IsCarry(true);
				if(curH.pitchEnv.flg& EnvFlags::USE_SUSTAIN){
					xins.FilterEnvelope().SustainBegin(curH.pitchEnv.sustainS);
					xins.FilterEnvelope().SustainEnd(curH.pitchEnv.sustainE);
				}

				if(curH.pitchEnv.flg & EnvFlags::USE_LOOP){
					xins.FilterEnvelope().LoopStart(curH.pitchEnv.loopS);
					xins.FilterEnvelope().LoopEnd(curH.pitchEnv.loopE);
				}

				for(int i = 0; i < envelope_point_num;i++){
					short pitchtmp = curH.pitchEnv.nodes[i].secondlo | (curH.pitchEnv.nodes[i].secondhi <<8);
					xins.FilterEnvelope().Append(pitchtmp,(float)(curH.pitchEnv.nodes[i].first+32)/ 64.0f);
				}
			} else {
				xins.PitchEnvelope().IsEnabled(curH.pitchEnv.flg & EnvFlags::USE_ENVELOPE);
				xins.FilterEnvelope().IsEnabled(false);
				if(curH.pitchEnv.flg& EnvFlags::ENABLE_CARRY) xins.PitchEnvelope().IsCarry(true);
				if(curH.pitchEnv.flg& EnvFlags::USE_SUSTAIN){
					xins.PitchEnvelope().SustainBegin(curH.pitchEnv.sustainS);
					xins.PitchEnvelope().SustainEnd(curH.pitchEnv.sustainE);
				}

				if(curH.pitchEnv.flg & EnvFlags::USE_LOOP){
					xins.PitchEnvelope().LoopStart(curH.pitchEnv.loopS);
					xins.PitchEnvelope().LoopEnd(curH.pitchEnv.loopE);
				}

				for(int i = 0; i < envelope_point_num;i++){
					short pitchtmp = curH.pitchEnv.nodes[i].secondlo | (curH.pitchEnv.nodes[i].secondhi <<8);
					xins.PitchEnvelope().Append(pitchtmp,(float)(curH.pitchEnv.nodes[i].first)/ 32.0f);
				}
			}

			xins.ValidateEnabled();
			return true;
		}

		bool ITModule2::LoadITSample(XMInstrument::WaveData<>& _wave)
		{
			itSampleHeader curH;
			Read(&curH,sizeof(curH));
			char renamed[26];
			for(int i=0;i<25;i++){
				if(curH.sName[i]=='\0') renamed[i]=' ';
				else renamed[i] = curH.sName[i];
			}
			renamed[25]='\0';
			std::string sName = renamed;

/*		      Flg:      Bit 0. On = sample associated with header.
			Bit 1. On = 16 bit, Off = 8 bit.
			Bit 2. On = stereo, Off = mono. Stereo samples not supported yet
			Bit 3. On = compressed samples.
			Bit 4. On = Use loop
			Bit 5. On = Use sustain loop
			Bit 6. On = Ping Pong loop, Off = Forwards loop
			Bit 7. On = Ping Pong Sustain loop, Off = Forwards Sustain loop
*/
			bool bstereo=curH.flg&SampleFlags::ISSTEREO;
			bool b16Bit=curH.flg&SampleFlags::IS16BIT;
			bool bcompressed=curH.flg&SampleFlags::ISCOMPRESSED;
			bool bLoop=curH.flg&SampleFlags::USELOOP;
			bool bsustainloop=curH.flg&SampleFlags::USESUSTAIN;

			if (curH.flg&SampleFlags::HAS_SAMPLE)
			{
				_wave.Init();
				_wave.AllocWaveData(curH.length,bstereo);

				_wave.WaveLoopStart(curH.loopB);
				_wave.WaveLoopEnd(curH.loopE);
				if(bLoop) {
					if (curH.flg&SampleFlags::ISLOOPPINPONG)
					{
						_wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::BIDI);
					}
					else _wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
				} else {
					_wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::DO_NOT);
				}
				_wave.WaveSusLoopStart(curH.sustainB);
				_wave.WaveSusLoopEnd(curH.sustainE);
				if(bsustainloop)
				{
					if (curH.flg&SampleFlags::ISSUSTAINPINPONG)
					{
						_wave.WaveSusLoopType(XMInstrument::WaveData<>::LoopType::BIDI);
					}
					else _wave.WaveSusLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
				} else {
					_wave.WaveSusLoopType(XMInstrument::WaveData<>::LoopType::DO_NOT);
				}

				_wave.WaveVolume(curH.vol *2);
				_wave.WaveGlobVolume(curH.gVol /64.0f);


//				Older method. conversion from speed to tune. Replaced by using the samplerate directly
//				double tune = log10(double(curH.c5Speed)/8363.0f)/log10(2.0);
//				double maintune = floor(tune*12);
//				double finetune = floor(((tune*12)-maintune)*100);

				XMInstrument::WaveData<>::WaveForms::Type exchwave[4]={XMInstrument::WaveData<>::WaveForms::SINUS,
					XMInstrument::WaveData<>::WaveForms::SAWDOWN,
					XMInstrument::WaveData<>::WaveForms::SQUARE,
					XMInstrument::WaveData<>::WaveForms::RANDOM
				};
//				_wave.WaveTune(maintune);
//				_wave.WaveFineTune(finetune);
				_wave.WaveSampleRate(curH.c5Speed);
				_wave.WaveName(sName);
				_wave.PanEnabled(curH.dfp&0x80);
				_wave.PanFactor((curH.dfp&0x7F)/64.0f);
				_wave.VibratoAttack(curH.vibR==0?1:curH.vibR);
				_wave.VibratoSpeed(curH.vibS);
				_wave.VibratoDepth(curH.vibD);
				_wave.VibratoType(exchwave[curH.vibT&3]);

				if (curH.length > 0) {
					Seek(curH.smpData);
					if (bcompressed) {
						LoadITCompressedData(_wave.pWaveDataL(),curH.length,b16Bit,curH.cvt);
						if (bstereo) LoadITCompressedData(_wave.pWaveDataR(),curH.length,b16Bit,curH.cvt);
					}
					else LoadITSampleData(_wave,curH.length,bstereo,b16Bit,curH.cvt);
				}
				return true;
			}

			return false;
		}

		bool ITModule2::LoadITSampleData(XMInstrument::WaveData<>& _wave,uint32_t iLen,bool bstereo,bool b16Bit, unsigned char convert)
		{
			signed short wNew,wTmp;
			int offset=(convert & SampleConvert::IS_SIGNED)?0:-32768;
			int lobit=(convert & SampleConvert::IS_MOTOROLA)?8:0;
			int hibit=8-lobit;
			uint32_t j,out;

			if (b16Bit) iLen*=2;
			unsigned char * smpbuf = new unsigned char[iLen];
			Read(smpbuf,iLen);

			out=0;wNew=0;
			if (b16Bit) {
				for (j = 0; j < iLen; j+=2) {
					wTmp= ((smpbuf[j]<<lobit) | (smpbuf[j+1]<<hibit))+offset;
					wNew=(convert& SampleConvert::IS_DELTA)?wNew+wTmp:wTmp;
					*(const_cast<signed short*>(_wave.pWaveDataL()) + out) = wNew;
					out++;
				}
				if (bstereo) {
					Read(smpbuf,iLen);
					out=0;
					for (j = 0; j < iLen; j+=2) {
						wTmp= ((smpbuf[j]<<lobit) | (smpbuf[j+1]<<hibit))+offset;
						wNew=(convert& SampleConvert::IS_DELTA)?wNew+wTmp:wTmp;
						*(const_cast<signed short*>(_wave.pWaveDataR()) + out) = wNew;
						out++;
					}
				}
			} else {
				for (j = 0; j < iLen; j++) {
					wNew=(convert& SampleConvert::IS_DELTA)?wNew+smpbuf[j]:smpbuf[j];
					*(const_cast<signed short*>(_wave.pWaveDataL()) + j) = ((wNew<<8)+offset);
				}
				if (bstereo) {
					Read(smpbuf,iLen);
					for (j = 0; j < iLen; j++) {
						wNew=(convert& SampleConvert::IS_DELTA)?wNew+smpbuf[j]:smpbuf[j];
						*(const_cast<signed short*>(_wave.pWaveDataR()) + j) = ((wNew<<8)+offset);
					}
				}
			}
			delete [] smpbuf; smpbuf = 0;
			return true;
		}

		bool ITModule2::LoadITCompressedData(int16_t* pWavedata,uint32_t iLen,bool b16Bit,unsigned char convert)
		{
			unsigned char bitwidth,packsize,maxbitsize;
			uint32_t topsize, val,j;
			short d1, d2,wNew;
			char d18,d28;

			bool deltapack=(itFileH.ffv>=0x215 && convert&SampleConvert::IS_DELTA); // Impulse Tracker 2.15 added a delta packed compressed sample format
			
			if (b16Bit)	{
				topsize=0x4000;		packsize=4;	maxbitsize=16;
			} else {
				topsize=0x8000;		packsize=3;	maxbitsize=8;
			}
			
			j=0;
			while(j<iLen) // While we haven't decompressed the whole sample
			{	
				BitsBlock block;
				if (!block.ReadBlock(this)) return false;

				// Size of the block of data to process, in blocks of max size=0x8000bytes ( 0x4000 samples if 16bits)
				uint32_t blocksize=(iLen-j<topsize)?iLen-j:topsize;
				uint32_t blockpos=0;

				bitwidth = maxbitsize+1;
				d1 = d2 = 0;
				d18 = d28 = 0;

				//Start the decompression:
				while (blockpos < blocksize) {
					val = block.ReadBits(bitwidth);

					//Check if value contains a bitwidth change. If it does, change and proceed with next value.
					if (bitwidth < 7) { //Method 1:
						if (val == unsigned(1 << (bitwidth - 1))) { // if the value == topmost bit set.
							val = block.ReadBits(packsize) + 1;
							bitwidth = (val < bitwidth) ? val : val + 1;
							continue;
						}
					} else if (bitwidth < maxbitsize+1) { //Method 2
						unsigned short border = (((1<<maxbitsize)-1) >> (maxbitsize+1 - bitwidth)) - (maxbitsize/2);

						if ((val > border) && (val <= unsigned(border + maxbitsize))) {
							val -= border;
							bitwidth = val < bitwidth ? val : val + 1;
							continue;
						}
					} else if (bitwidth == maxbitsize+1) { //Method 3
						if (val & (1<<maxbitsize)) {
							bitwidth = (val + 1) & 0xFF;
							continue;
						}
					} else { //Illegal width, abort ?
						return false;
					}

					//If we reach here, val contains a value to decompress, so do it.
					{
						if (b16Bit) 
						{	
							short v; //The sample value:
							if (bitwidth < maxbitsize) {
								unsigned char shift = maxbitsize - bitwidth;
								v = (short)(val << shift);
								v >>= shift;
							}
							else
								v = (short)val;

							//And integrate the sample value
							d1 += v;
							d2 += d1;
							wNew = deltapack?d2:d1;
						}
						else
						{	
							char v; //The sample value:
							if (bitwidth < maxbitsize) {
								unsigned char shift = maxbitsize - bitwidth;
								v = (val << shift);
								v >>= shift;
							}
							else
								v = (char)val;

								d18 +=v;
								d28 +=d18;
								wNew = deltapack?d28:d18;
								wNew <<=8;
						}
					}

					//Store the decompressed value to Wave pointer.
					*(const_cast<signed short*>(pWavedata+j+blockpos)) = wNew;
					
					blockpos++;
				}
				j+=blocksize;
			}
			return false;
		}

		bool ITModule2::LoadITPattern(int patIdx, int &numchans, std::map<int,int>& ittovirtual)
		{
			unsigned char newEntry;
			unsigned char lastnote[64];
			unsigned char lastinst[64];
			unsigned char lastmach[64];
			unsigned char lastvol[64];
			unsigned char lastcom[64];
			unsigned char lasteff[64];
			unsigned char mask[64];
			std::memset(lastnote,255,sizeof(char)*64);
			std::memset(lastinst,255,sizeof(char)*64);
			std::memset(lastmach,255,sizeof(char)*64);
			std::memset(lastvol,255,sizeof(char)*64);
			std::memset(lastcom,255,sizeof(char)*64);
			std::memset(lasteff,255,sizeof(char)*64);
			std::memset(mask,255,sizeof(char)*64);

			PatternEntry pempty;
			pempty._note=notecommands::empty; pempty._mach=255;pempty._inst=255;pempty._cmd=0;pempty._parameter=0;
			PatternEntry pent=pempty;

			Skip(2); // packedSize
			int16_t rowCount=ReadInt16();
			Skip(4); // unused
			if (rowCount > MAX_LINES ) rowCount=MAX_LINES;
			else if (rowCount < 0 ) rowCount = 0;
			_pSong->AllocNewPattern(patIdx,"unnamed",rowCount,false);
			//char* packedpattern = new char[packedSize];
			//Read(packedpattern, packedSize);
			for (int row=0;row<rowCount;row++)
			{
				m_extracolumn = numchans+1;
				Read(&newEntry,1);
				while ( newEntry )
				{
					unsigned char channel=(newEntry-1)&0x3F;
					if (channel >= m_extracolumn) { m_extracolumn = channel+1;}
					if (newEntry&0x80) mask[channel]=ReadUInt8();
					unsigned char volume=255;
					if(mask[channel]&1)
					{
						unsigned char note=ReadUInt8();
						if (note==255) pent._note = notecommands::release;
						else if (note==254) pent._note=notecommands::release; //\todo: Attention ! Psycle doesn't have a note-cut note.
						else pent._note = note;
						lastnote[channel]=pent._note;
					}
					else if (mask[channel]&0x10) { 
						pent._note=lastnote[channel]; 
					}
					if (mask[channel]&2) {
						pent._inst=ReadUInt8()-1;
					}
					else if (mask[channel]&0x20) { 
						pent._inst=lastinst[channel];
					}
					if (mask[channel]&4)
					{
						unsigned char tmp=ReadUInt8();
						// Volume ranges from 0->64
						// Panning ranges from 0->64, mapped onto 128->192
						// Prepare for the following also:
						//  65->74 = Fine volume up
						//  75->84 = Fine volume down
						//  85->94 = Volume slide up
						//  95->104 = Volume slide down
						//  105->114 = Pitch Slide down
						//  115->124 = Pitch Slide up
						//  193->202 = Portamento to
						//  203->212 = Vibrato
						if ( tmp<65) {
							volume=tmp<64?tmp:63;
						}
						else if (tmp<75) {
							volume=XMSampler::CMD_VOL::VOL_FINEVOLSLIDEUP | (tmp-65);
						}
						else if (tmp<85) {
							volume=XMSampler::CMD_VOL::VOL_FINEVOLSLIDEDOWN | (tmp-75);
						}
						else if (tmp<95) {
							volume=XMSampler::CMD_VOL::VOL_VOLSLIDEUP | (tmp-85);
						}
						else if (tmp<105) {
							volume=XMSampler::CMD_VOL::VOL_VOLSLIDEDOWN | (tmp-95);
						}
						else if (tmp<115) {
							volume=XMSampler::CMD_VOL::VOL_PITCH_SLIDE_DOWN | (tmp-105);
						}
						else if (tmp<125) {
							volume=XMSampler::CMD_VOL::VOL_PITCH_SLIDE_UP | (tmp-115);
						}
						else if (tmp<193) {
							tmp= (tmp==192)?15:(tmp-128)/4;
							volume=XMSampler::CMD_VOL::VOL_PANNING | tmp;
						}
						else if (tmp<203) {
							volume=XMSampler::CMD_VOL::VOL_TONEPORTAMENTO | (tmp-193);
						}
						else if (tmp<213) {
							volume=XMSampler::CMD_VOL::VOL_VIBRATO | ( tmp-203 );
						}
						else {
							volume = 255;
						}
						lastvol[channel]=volume;
					}
					else if (mask[channel]&0x40 ) {
						volume=lastvol[channel];
					}
					if(mask[channel]&8)
					{
						unsigned char command=ReadUInt8();
						unsigned char param=ReadUInt8();
						if ( command != 0 ) pent._parameter = param;
						ParseEffect(pent,patIdx,row,command,param,channel);
						lastcom[channel]=pent._cmd;
						lasteff[channel]=pent._parameter;
					}
					else if ( mask[channel]&0x80 )
					{
						pent._cmd = lastcom[channel];
						pent._parameter = lasteff[channel];
					}

					// If empty, do not inform machine
					if (pent._note == notecommands::empty && pent._inst == 255 && volume == 255 && pent._cmd == 00 && pent._parameter == 00) {
						pent._mach = 255;
					}
					// if instrument without note, or note without instrument, cannot use virtual instrument, so use sampulse directly
					else if (( pent._note == notecommands::empty && pent._inst != 255 ) || ( pent._note < notecommands::release && pent._inst == 255)) {
						pent._mach = 0;
						if (pent._inst != 255) {
							lastinst[channel]=pent._inst;
							//We cannot use the virtual instrument, but we should remember which instrument it is.
							std::map<int,int>::const_iterator it = ittovirtual.find(pent._inst);
							if (it != ittovirtual.end()) {
								lastmach[channel]=it->second;
							}
						}
					}
					//default behaviour, let's find the virtual instrument.
					else {
						std::map<int,int>::const_iterator it = ittovirtual.find(pent._inst);
						if (it == ittovirtual.end()) {
							if (pent._inst != 255) {
								pent._mach = 0;
								lastmach[channel]=pent._mach;
								lastinst[channel]=pent._inst;
							}
							else if (lastmach[channel] != 255) {
								pent._mach = lastmach[channel];
							}
							else if (volume != 255) {
								pent._mach = 0;
							}
							else {
								pent._mach = 255;
							}
						}
						else {
							lastinst[channel]=pent._inst;
							pent._mach=it->second;
							pent._inst=255;
							lastmach[channel]=pent._mach;
						}
					}
					if (pent._mach == 0) { // fallback to the old behaviour. This will happen only if an unused instrument is present in the pattern.
						if (pent._cmd != 0 || pent._parameter != 0) {
							if(volume!=255 && m_extracolumn < MAX_TRACKS) {
								PatternEntry* pData = reinterpret_cast<PatternEntry*>(_pSong->_ptrackline(patIdx,m_extracolumn,row));
								pData->_note = notecommands::midicc;
								pData->_inst = channel;
								pData->_mach = pent._mach;
								pData->_cmd = XMSampler::CMD::SENDTOVOLUME;
								pData->_parameter = volume;
								m_extracolumn++;
							}
						}
						else if(volume < 0x40) {
							pent._cmd = XMSampler::CMD::VOLUME;
							pent._parameter = volume*2;
						}
						else if(volume!=255) {
							pent._cmd = XMSampler::CMD::SENDTOVOLUME;
							pent._parameter = volume;
						}
					}
					else {
						pent._inst = volume;
					}
					PatternEntry* pData = reinterpret_cast<PatternEntry*>(_pSong->_ptrackline(patIdx,channel,row));
					*pData = pent;
					pent=pempty;

					numchans = std::max((int)channel,numchans);

					Read(&newEntry,1);
				}
				m_maxextracolumn = std::max(m_maxextracolumn,m_extracolumn);
			}
			return true;
		}

		void ITModule2::ParseEffect(PatternEntry&pent, int patIdx, int row, int command,int param,int channel)
		{
			XMInstrument::WaveData<>::WaveForms::Type exchwave[4]={XMInstrument::WaveData<>::WaveForms::SINUS,
				XMInstrument::WaveData<>::WaveForms::SAWDOWN,
				XMInstrument::WaveData<>::WaveForms::SQUARE,
				XMInstrument::WaveData<>::WaveForms::RANDOM
			};
			switch(command){
				case ITModule2::CMD::SET_SPEED:
					{
						pent._cmd=PatternCmd::EXTENDED;
						int extraticks=0;
						pent._parameter = XMSampler::CalcLPBFromSpeed(param,extraticks);
						if (extraticks != 0 && m_extracolumn < MAX_TRACKS) {
							PatternEntry* pData = reinterpret_cast<PatternEntry*>(_pSong->_ptrackline(patIdx,m_extracolumn,row));
							pData->_note = notecommands::empty;
							pData->_inst = 255;
							pData->_mach = pent._mach;
							pData->_cmd = PatternCmd::EXTENDED;
							pData->_parameter = PatternCmd::ROW_EXTRATICKS | extraticks;
							m_extracolumn++;
						}
					}
					break;
				case ITModule2::CMD::JUMP_TO_ORDER:
					pent._cmd = PatternCmd::JUMP_TO_ORDER;
					break;
				case ITModule2::CMD::BREAK_TO_ROW:
					pent._cmd = PatternCmd::BREAK_TO_LINE;
					break;
				case ITModule2::CMD::VOLUME_SLIDE:
					pent._cmd = XMSampler::CMD::VOLUMESLIDE;
					break;
				case ITModule2::CMD::PORTAMENTO_DOWN:
					pent._cmd = XMSampler::CMD::PORTAMENTO_DOWN;
					break;
				case ITModule2::CMD::PORTAMENTO_UP:
					pent._cmd = XMSampler::CMD::PORTAMENTO_UP;
					break;
				case ITModule2::CMD::TONE_PORTAMENTO:
					pent._cmd = XMSampler::CMD::PORTA2NOTE;
					break;
				case ITModule2::CMD::VIBRATO:
					pent._cmd = XMSampler::CMD::VIBRATO;
					break;
				case ITModule2::CMD::TREMOR:
					pent._cmd = XMSampler::CMD::TREMOR;
					break;
				case ITModule2::CMD::ARPEGGIO:
					pent._cmd = XMSampler::CMD::ARPEGGIO;
					break;
				case ITModule2::CMD::VOLSLIDE_VIBRATO:
					pent._cmd = XMSampler::CMD::VIBRATOVOL;
					break;
				case ITModule2::CMD::VOLSLIDE_TONEPORTA:
					pent._cmd = XMSampler::CMD::TONEPORTAVOL;
					break;
				case CMD::SET_CHANNEL_VOLUME: // IT
					pent._cmd = XMSampler::CMD::SET_CHANNEL_VOLUME;
					break;
				case CMD::CHANNEL_VOLUME_SLIDE: // IT
					pent._cmd = XMSampler::CMD::CHANNEL_VOLUME_SLIDE;
					break;
				case CMD::SET_SAMPLE_OFFSET:
					pent._cmd = XMSampler::CMD::OFFSET | highOffset[channel];
					break;
				case ITModule2::CMD::PANNING_SLIDE: // IT
					pent._cmd = XMSampler::CMD::PANNINGSLIDE;
					break;
				case ITModule2::CMD::RETRIGGER_NOTE:
					pent._cmd = XMSampler::CMD::RETRIG;
					break;
				case ITModule2::CMD::TREMOLO:
					pent._cmd = XMSampler::CMD::TREMOLO;
					break;
				case ITModule2::CMD::S:
					switch(param & 0xf0){
						case CMD_S::S_SET_FILTER:
							pent._cmd = XMSampler::CMD::NONE;
							break;
						case CMD_S::S_SET_GLISSANDO_CONTROL:
							pent._cmd = XMSampler::CMD::EXTENDED;
							pent._parameter = XMSampler::CMD_E::E_GLISSANDO_TYPE | (param & 0xf);
							break;
						case CMD_S::S_FINETUNE:
							pent._cmd = XMSampler::CMD::NONE;
							break;
						case CMD_S::S_SET_VIBRATO_WAVEFORM:
							pent._cmd = XMSampler::CMD::EXTENDED;
							pent._parameter = XMSampler::CMD_E::E_VIBRATO_WAVE | exchwave[(param & 0x3)];
							break;
						case CMD_S::S_SET_TREMOLO_WAVEFORM:
							pent._cmd = XMSampler::CMD::EXTENDED;
							pent._parameter = XMSampler::CMD_E::E_TREMOLO_WAVE | exchwave[(param & 0x3)];
							break;
						case CMD_S::S_SET_PANBRELLO_WAVEFORM: // IT
							pent._cmd = XMSampler::CMD::EXTENDED;
							pent._parameter = XMSampler::CMD_E::E_PANBRELLO_WAVE | exchwave[(param & 0x3)];
							break;
						case CMD_S::S_FINE_PATTERN_DELAY: // IT
							break;
						case CMD_S::S7: // IT
							pent._cmd = XMSampler::CMD::EXTENDED;
							pent._parameter = XMSampler::CMD_E::EE | (param&0x0F);
							break;
						case CMD_S::S_SET_PAN:
							pent._cmd = XMSampler::CMD::EXTENDED;
							pent._parameter = XMSampler::CMD_E::E_SET_PAN | (param & 0xf);
							break;
						case CMD_S::S9: // IT
							pent._cmd = XMSampler::CMD::EXTENDED;
							pent._parameter = XMSampler::CMD_E::E9 | (param&0x0F);
							break;
						case CMD_S::S_SET_HIGH_OFFSET: // IT
							highOffset[channel] = param &0x0F;
							pent._cmd = XMSampler::CMD::NONE;
							pent._parameter = 0;
							break;
						case CMD_S::S_PATTERN_LOOP:
							pent._cmd = PatternCmd::EXTENDED;
							pent._parameter = PatternCmd::PATTERN_LOOP | (param & 0xf);
							break;
						case CMD_S::S_DELAYED_NOTE_CUT:
							pent._cmd = XMSampler::CMD::EXTENDED;
							pent._parameter = XMSampler::CMD_E::E_DELAYED_NOTECUT  | (param & 0xf);
							break;
						case CMD_S::S_NOTE_DELAY:
							pent._cmd = XMSampler::CMD::EXTENDED;
							pent._parameter = XMSampler::CMD_E::E_NOTE_DELAY | ( param & 0xf);
							break;
						case CMD_S::S_PATTERN_DELAY:
							pent._cmd = PatternCmd::EXTENDED;
							pent._parameter = PatternCmd::PATTERN_DELAY | (param & 0xf);
							break;
						case CMD_S::S_SET_MIDI_MACRO:
							pent._cmd = XMSampler::CMD::EXTENDED;
							if ( embeddedData)
							{
								std::string zxx = embeddedData->SFx[(param & 0xF)];
								std::string zxx2 = zxx.substr(0,5);
								if ( zxx2 == "F0F00")
								{
									int val = (embeddedData->SFx[(param & 0xF)][5]-'0');
									if (val < 2) {
										pent._parameter = XMSampler::CMD_E::E_SET_MIDI_MACRO | val;
									}
								}
							}
							else if ((param &0xF) < 2) {
								pent._parameter = XMSampler::CMD_E::E_SET_MIDI_MACRO | (param&0xf);
							}
							break;
					}
					break;
				case CMD::SET_SONG_TEMPO:
					pent._cmd = PatternCmd::SET_TEMPO;
					break;
				case CMD::FINE_VIBRATO:
					pent._cmd = XMSampler::CMD::FINE_VIBRATO;
					break;
				case CMD::SET_GLOBAL_VOLUME: 
					pent._cmd = XMSampler::CMD::SET_GLOBAL_VOLUME;
					break;
				case CMD::GLOBAL_VOLUME_SLIDE: // IT
					pent._cmd = XMSampler::CMD::GLOBAL_VOLUME_SLIDE;
					break;
				case CMD::SET_PANNING: // IT
					pent._cmd = XMSampler::CMD::PANNING;
					break;
				case CMD::PANBRELLO: // IT
					pent._cmd = XMSampler::CMD::PANBRELLO;
					break;
				case CMD::MIDI_MACRO:
					if ( param < 127)
					{
						pent._parameter = param;
					}
					pent._cmd = XMSampler::CMD::MIDI_MACRO;
					break;
				default:
					pent._cmd = XMSampler::CMD::NONE;
					break;
			}
		}

//////////////////////////////////////////////////////////////////////////
//     S3M Module Members


		bool ITModule2::LoadS3MModuleX(Song& song)
		{
			CExclusiveLock lock(&song.semaphore, 2, true);
			_pSong=&song;
			if (Read(&s3mFileH,sizeof(s3mFileH))==0 ) return 0;
			if (s3mFileH.tag != SCRM_ID || s3mFileH.type != 0x10 ) return 0;

			s3mFileH.songName[28]='\0';
			song.name = s3mFileH.songName;
			song.author = "";
			song.comments = "Imported from Scream Tracker 3 Module: ";
			song.comments.append(szName);

			song.CreateMachine(MACH_XMSAMPLER, rand()/64, rand()/80, "sampulse",0);
			int connectionIdx = song.InsertConnectionNonBlocking(0,MASTER_INDEX,0,0,value_mapper::map_128_1(s3mFileH.mVol&0x7F));
			song.seqBus=0;
			XMSampler* sampler = ((XMSampler*)song._pMachine[0]);

			song.BeatsPerMin(s3mFileH.iTempo);
			song.TicksPerBeat(24);
			int extraticks=0;
			song.LinesPerBeat(XMSampler::CalcLPBFromSpeed(s3mFileH.iSpeed,extraticks));
			if (extraticks != 0) {
				song.ExtraTicksPerLine(extraticks);
			}

			sampler->IsAmigaSlides(true);
			sampler->GlobalVolume((s3mFileH.gVol&0x7F)*2);
			
			uint16_t j,i=0;
			for (j=0;j<s3mFileH.ordNum;j++)
			{
				song.playOrder[i]=ReadUInt8(); // 254 = ++ (skip), 255 = --- (end of tune).
				if (song.playOrder[i]!= 254 &&song.playOrder[i] != 255 ) i++;
			}
			song.playLength=i;
			if ( song.playLength == 0) // Add at least one pattern to the sequence.
			{
				song.playLength = 1;
				song.playOrder[0]=0;
			}

			unsigned short *pointersi = new unsigned short[s3mFileH.insNum];
			Read(pointersi,s3mFileH.insNum*sizeof(unsigned short));
			unsigned short * pointersp = new unsigned short[s3mFileH.patNum];
			Read(pointersp,s3mFileH.patNum*sizeof(unsigned short));
			unsigned char chansettings[32];
			if ( s3mFileH.defPan==0xFC )
			{
				Read(chansettings,sizeof(chansettings));
			}
			bool stereo=s3mFileH.mVol&0x80;
			if (stereo) {
				// Note that in stereo, the mastermul is internally multiplied by 11/8 inside the player since
				// there is generally more room in the output stream
				sampler->SetDestWireVolume(connectionIdx,(value_mapper::map_128_1(s3mFileH.mVol&0x7F))*(11.f/8.f));
			}
			int numchans=0;
			for (i=0;i<32;i++)
			{
				if ( s3mFileH.chanSet[i]==S3MChanType::ISUNUSED) 
				{
					sampler->rChannel(i).DefaultIsMute(true);
					sampler->rChannel(i).DefaultPanFactorFloat(0.5f,true);
				}
				else
				{
					numchans=i+1; // topmost used channel.
					if(s3mFileH.chanSet[i]&S3MChanType::ISDISABLED)
					{
						sampler->rChannel(i).DefaultIsMute(true);
					}
					if (stereo && !(s3mFileH.chanSet[i]&S3MChanType::ISADLIBCHAN))
					{
						if (s3mFileH.defPan && chansettings[i]&S3MChanType::HASCUSTOMPOS)
						{
							float flttmp=XMSampler::E8VolMap[(chansettings[i]&0x0F)]/64.0f;
							sampler->rChannel(i).DefaultPanFactorFloat(flttmp,true);
						}
						else if (s3mFileH.chanSet[i]&S3MChanType::ISRIGHTCHAN)
							sampler->rChannel(i).DefaultPanFactorFloat(0.80f,true);
						else 
							sampler->rChannel(i).DefaultPanFactorFloat(0.20f,true);
					}
					else
						sampler->rChannel(i).DefaultPanFactorFloat(0.5f,true);
				}
			}
			song.SONGTRACKS=std::max(numchans,4);

			int virtualInst = MAX_MACHINES;
			std::map<int,int> s3mtovirtual;
			for (i=0;i<s3mFileH.insNum;i++)
			{
				Seek(pointersi[i]<<4);
				LoadS3MInstX(song, i);
				if (song.xminstruments.IsEnabled(i)) {
					s3mtovirtual[i]=virtualInst;
					song.SetVirtualInstrument(virtualInst++,0,i);
				}
			}
			m_maxextracolumn=song.SONGTRACKS;
			for (i=0;i<s3mFileH.patNum;i++)
			{
				Seek(pointersp[i]<<4);
				LoadS3MPatternX(i, s3mtovirtual);
			}
			song.SONGTRACKS=m_maxextracolumn;
			delete[] pointersi; pointersi = 0;
			delete[] pointersp; pointersp = 0;

			return true;
		}

		//This is used to load s3i (st3 standalone samples/instruments).
		int ITModule2::LoadS3MFileS3I(LoaderHelper& loadhelp)
		{
			s3mInstHeader curH;
			Read(&curH,sizeof(curH));
			int result=-1;

			if (curH.tag == SCRS_ID && curH.type == 1) 
			{
				int outsample=-1;
				XMInstrument::WaveData<>& wave = loadhelp.GetNextSample(outsample);
				bool res = LoadS3MSampleX(wave,reinterpret_cast<s3mSampleHeader*>(&curH));
				if (res) {
					result = outsample;
				}
			}
			else if (curH.tag == SCRI_ID && curH.type != 0)
			{
				//reinterpret_cast<s3madlibheader*>(&curH)
			}
			return result;
		}

		bool ITModule2::LoadS3MInstX(Song& song, uint16_t iSampleIdx)
		{
			bool result = false;
			s3mInstHeader curH;
			Read(&curH,sizeof(curH));
			XMInstrument instr;
			instr.Init();
			instr.Name(curH.sName);

			if (curH.tag == SCRS_ID && curH.type == 1) 
			{
				XMInstrument::WaveData<> wave;
				result = LoadS3MSampleX(wave,reinterpret_cast<s3mSampleHeader*>(&curH));
				song.samples.SetSample(wave,iSampleIdx);
				if (result) {
					instr.SetDefaultNoteMap(iSampleIdx);
					instr.ValidateEnabled();
				}
			}
			else if (curH.tag == SCRI_ID && curH.type != 0)
			{
				//reinterpret_cast<s3madlibheader*>(&curH)
			}
			song.xminstruments.SetInst(instr,iSampleIdx);
			return result;
		}

		bool ITModule2::LoadS3MSampleX(XMInstrument::WaveData<>& _wave,s3mSampleHeader *currHeader)
		{
			bool bLoop=currHeader->flags&S3MSampleFlags::LOOP;
			bool bstereo=currHeader->flags&S3MSampleFlags::STEREO;
			bool b16Bit=currHeader->flags&S3MSampleFlags::IS16BIT;

			_wave.Init();

			_wave.WaveLoopStart(currHeader->loopb);
			_wave.WaveLoopEnd(currHeader->loope);
			if(bLoop) {
				_wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::NORMAL);
			} else {
				_wave.WaveLoopType(XMInstrument::WaveData<>::LoopType::DO_NOT);
			}

			_wave.WaveVolume(currHeader->vol * 2);

			_wave.WaveSampleRate(currHeader->c2speed);

			std::string sName = currHeader->filename;
			_wave.WaveName(sName);

			int newpos=((currHeader->hiMemSeg<<16)+currHeader->lomemSeg)<<4;
			Seek(newpos);
			return LoadS3MSampleDataX(_wave,currHeader->length,bstereo,b16Bit,currHeader->packed);
		}

		bool ITModule2::LoadS3MSampleDataX(XMInstrument::WaveData<>& _wave,uint32_t iLen,bool bstereo,bool b16Bit,bool packed)
		{
			if (!packed) // Looks like the packed format never existed.
			{
				char * smpbuf;
				if(b16Bit)
				{
					smpbuf = new char[iLen*2];
					Read(smpbuf,iLen*2);
				} else 	{
					smpbuf = new char[iLen];
					Read(smpbuf,iLen);
				}
				int16_t wNew;
				int16_t offset;
				if ( s3mFileH.trackerInf==1) offset=0; // 1=[VERY OLD] signed samples, 2=unsigned samples
				else offset=-32768;

				//+2=stereo (after Length bytes for LEFT channel, another Length bytes for RIGHT channel)
                //+4=16-bit sample (intel LO-HI byteorder) (+2/+4 not supported by ST3.01)
				_wave.AllocWaveData(iLen,bstereo);
				if(b16Bit) {
					int out=0;
					for(unsigned int j=0;j<iLen*2;j+=2,out++)
					{
						wNew = (0xFF & smpbuf[j] | smpbuf[j+1]<<8)+offset;
						*(const_cast<int16_t*>(_wave.pWaveDataL()) + out) = wNew;
					}
					if (bstereo) {
						out=0;
						Read(smpbuf,iLen*2);
						for(unsigned int j=0;j<iLen*2;j+=2,out++)
						{
							wNew = (0xFF & smpbuf[j] | smpbuf[j+1]<<8) +offset;
							*(const_cast<int16_t*>(_wave.pWaveDataR()) + out) = wNew;
						}   
					}
				} else {// 8 bit sample
					for(unsigned int j=0;j<iLen;j++)
					{			
						wNew = (smpbuf[j]<<8)+offset;
						*(const_cast<int16_t*>(_wave.pWaveDataL()) + j) = wNew;
					}
					if (bstereo) {
						Read(smpbuf,iLen);
						for(unsigned int j=0;j<iLen;j++)
						{			
							wNew = (smpbuf[j]<<8)+offset;
							*(const_cast<int16_t*>(_wave.pWaveDataR()) + j) = wNew;
						}
					}
				}

				// cleanup
				delete[] smpbuf; smpbuf = 0;
				return true;
			}
			return false;
		}

		bool ITModule2::LoadS3MPatternX(uint16_t patIdx, std::map<int,int>& s3mtovirtual)
		{
			unsigned char lastmach[64];
			uint8_t newEntry;
			PatternEntry pempty;
			pempty._note=notecommands::empty; pempty._mach=255;pempty._inst=255;pempty._cmd=0;pempty._parameter=0;
			PatternEntry pent=pempty;
			std::memset(lastmach,255,sizeof(char)*64);

			_pSong->AllocNewPattern(patIdx,"unnamed",64,false);
			Skip(2);//int packedSize=ReadInt(2);
//			char* packedpattern = new char[packedsize];
//			Read(packedpattern,packedsize);
			for (int row=0;row<64;row++)
			{
				m_extracolumn=_pSong->SONGTRACKS;
				Read(&newEntry,1);
				while ( newEntry )
				{
					uint8_t channel=newEntry&31;
					uint8_t volume=255;
					if(newEntry&32)
					{
						uint8_t note=ReadUInt8();  // hi=oct, lo=note, 255=empty note,	254=key off
						if (note==254) pent._note = notecommands::release;
						else if (note==255) pent._note = notecommands::empty;
						else pent._note = ((note>>4)*12+(note&0xF)+12);  // +12 since ST3 C-4 is Psycle's C-5
						pent._inst=ReadUInt8()-1;
					}
					if(newEntry&64)
					{
						uint8_t tmp=ReadUInt8();
						volume = (tmp<64)?tmp:63;
					}
					if(newEntry&128)
					{
						uint8_t command=ReadUInt8();
						uint8_t param=ReadUInt8();
						if ( command != 0 ) pent._parameter = param;
						ParseEffect(pent,patIdx,row,command,param,channel);
						if ( pent._cmd == PatternCmd::BREAK_TO_LINE )
						{
							pent._parameter = ((pent._parameter&0xF0)>>4)*10 + (pent._parameter&0x0F);
						}
						else if ( pent._cmd == XMSampler::CMD::SET_GLOBAL_VOLUME )
						{
							pent._parameter = (pent._parameter<0x40)?pent._parameter*2:0x80;
						}
						else if ( pent._cmd == XMSampler::CMD::PANNING )
						{
							if ( pent._parameter < 0x80) pent._parameter = pent._parameter*2;
							else if ( pent._parameter == 0x80 ) pent._parameter = 255;
							else if ( pent._parameter == 0xA4) 
							{
								pent._cmd = XMSampler::CMD::EXTENDED;
								pent._parameter = XMSampler::CMD_E::E9 | 1;
							}
						}
					}
					// If empty, do not inform machine
					if (pent._note == notecommands::empty && pent._inst == 255 && volume == 255 && pent._cmd == 00 && pent._parameter == 00) {
						pent._mach = 255;
					}
					// if instrument without note, or note without instrument, cannot use virtual instrument, so use sampulse directly
					else if (( pent._note == notecommands::empty && pent._inst != 255 ) || ( pent._note < notecommands::release && pent._inst == 255)) {
						pent._mach = 0;
						if (pent._inst != 255) {
							//We cannot use the virtual instrument, but we should remember which it is.
							std::map<int,int>::const_iterator it = s3mtovirtual.find(pent._inst);
							if (it != s3mtovirtual.end()) {
								lastmach[channel]=it->second;
							}
						}
					}
					//default behaviour, let's find the virtual instrument.
					else {
						std::map<int,int>::const_iterator it = s3mtovirtual.find(pent._inst);
						if (it == s3mtovirtual.end()) {
							if (pent._inst != 255) {
								pent._mach = 0;
								lastmach[channel] = pent._mach;
							}
							else if (lastmach[channel] != 255) {
								pent._mach = lastmach[channel];
							}
							else if (volume != 255) {
								pent._mach = 0;
							}
							else {
								pent._mach = 255;
							}
						}
						else {
							pent._mach=it->second;
							pent._inst=255;
							lastmach[channel]=pent._mach;
						}
					}

					if (pent._mach == 0) { // fallback to the old behaviour. This will happen only if an unused instrument is present in the pattern.
						if(pent._cmd != 0 || pent._parameter != 0) {
							if(volume!=255) {
								PatternEntry* pData = reinterpret_cast<PatternEntry*>(_pSong->_ptrackline(patIdx,m_extracolumn,row));
								pData->_note = notecommands::midicc;
								pData->_inst = channel;
								pData->_mach = pent._mach;
								pData->_cmd = XMSampler::CMD::SENDTOVOLUME;
								pData->_parameter = volume;
								m_extracolumn++;
							}
						}
						else if(volume < 0x40) {
							pent._cmd = XMSampler::CMD::VOLUME;
							pent._parameter = volume*2;
						}
						else if(volume!=255) {
							pent._cmd = XMSampler::CMD::SENDTOVOLUME;
							pent._parameter = volume;
						}
					}
					else {
						pent._inst = volume;
					}
					if (channel < _pSong->SONGTRACKS) {
						PatternEntry* pData = reinterpret_cast<PatternEntry*>(_pSong->_ptrackline(patIdx,channel,row));
						*pData = pent;
					}
					pent=pempty;

					Read(&newEntry,1);
				}
				m_maxextracolumn = std::max(m_maxextracolumn,m_extracolumn);
			}
			return true;
		}
	}
}

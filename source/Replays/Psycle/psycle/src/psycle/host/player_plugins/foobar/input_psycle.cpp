
#include "input_psycle.hpp"
#include "FoobarGlobal.hpp"
#include "FoobarConfig.hpp"
#include <psycle/host/Song.hpp>
#include <psycle/host/Player.hpp>
#include <psycle/host/Machine.hpp>
#include <psycle/host/internal_machines.hpp>
#include <psycle/host/machineloader.hpp>
#include "FoobarDriver.hpp"
#include "fake_progressDialog.hpp"
#include "resources.hpp"
#include <psycle/helpers/math.hpp>

extern psycle::host::FoobarGlobal global_;

static input_singletrack_factory_t<input_psycle> g_input_psy_factory;

using namespace pfc::stringcvt;
using namespace psycle::helpers::math;

///////////////////////////////////////////////////////////////////////////
//  input_psycle


void input_psycle::open(service_ptr_t<file> p_filehint,const char * p_path,t_input_open_reason p_reason,abort_callback & p_abort)
{
	if (p_reason == input_open_info_write) throw exception_io_unsupported_format();//our input does not support retagging.
	m_file = p_filehint;//p_filehint may be null, hence next line
	input_open_file_helper(m_file,p_path,p_reason,p_abort);//if m_file is null, opens file with appropriate privileges for our operation (read/write for writing tags, read-only otherwise).

	psycle::host::OldPsyFile file;
	if (!file.Open(p_path))
	{
		return;
	}
	psycle::host::CProgressDialog dlg;
	dlg.ShowWindow(SW_SHOW);
	global_.song().filesize=file.FileSize();
	global_.song().Load(&file,dlg);
	file.Close();
	global_.song().fileName = p_path;
}

void input_psycle::get_info(file_info & p_info,abort_callback & p_abort)
{
	p_info.info_set_int("channels", global_.conf().audioDriver().settings().numChannels());
	p_info.info_set_int("samplerate", global_.conf().audioDriver().GetSamplesPerSec());
	p_info.info_set("encoding", "synthetized");

	double i;
	psycle::host::Song& pSong= global_.song();
	i=CalcSongLength(pSong)/1000.0;

	int m=0;
	for(int j=0;i<MAX_MACHINES;j++)
	{
		if(pSong._pMachine[j])
		{
			char tagstr[255];
			char valstr[255];
			char tmp2[20];
			switch( pSong._pMachine[j]->_type )
			{
				case psycle::host::MACH_VST: //fallthrough
				case psycle::host::MACH_VSTFX: strcpy(tmp2,"V");break;
				case psycle::host::MACH_PLUGIN: strcpy(tmp2,"N");break;
				case psycle::host::MACH_MASTER: strcpy(tmp2,"M");break;
				default: strcpy(tmp2,"I"); break;
			}
			sprintf(tagstr,"MACHINE%.02x",j);
			
			if ( pSong._pMachine[j]->_type == psycle::host::MACH_DUMMY && 
				strcmp("", pSong._pMachine[j]->GetDllName())!=0 )
			{
				sprintf(valstr,"[!]  %s",pSong._pMachine[j]->_editName);
			}
			else
			{
				sprintf(valstr,"[%s]  %s",tmp2,pSong._pMachine[j]->_editName);
			}
			
			p_info.info_set(tagstr, valstr);
			m++;
		}
	}

	p_info.meta_add("Artist",pSong.author.c_str());
	p_info.meta_add("Title",pSong.name.c_str());
	p_info.meta_add("Comment",pSong.comments.c_str());

	p_info.set_length(i);
	p_info.info_set_int("PSY_TRACKS", pSong.SongTracks());
	p_info.info_set_int("PSY_BPM", pSong.BeatsPerMin());
	p_info.info_set_int("PSY_LPB", pSong.LinesPerBeat());
	p_info.info_set_int("PSY_ORDERS", pSong.playLength);
	p_info.info_set_int("PSY_PATTERNS", pSong.GetNumPatterns());
	p_info.info_set_int("PSY_MACHINES", m);
	p_info.info_set_int("PSY_INSTRUMENTS", pSong.GetNumInstruments());
	p_info.info_set("codec", "PSY");
}


void input_psycle::decode_initialize(unsigned p_flags,abort_callback & p_abort)
{
	global_.player().Stop();//	stop();

	global_.player()._loopSong=false;
	global_.player().Start(0,0);

	global_.conf()._pOutputDriver->Enable(true);

}

bool input_psycle::decode_get_dynamic_info(file_info & p_out, double & p_timestamp_delta) 
{
	return false;
} // deals with dynamic information such as VBR bitrates

bool input_psycle::decode_run(audio_chunk & p_chunk,abort_callback & p_abort)
{
	bool worked = ((psycle::host::FoobarDriver*)&global_.conf().audioDriver())->decode_run(p_chunk,&global_.player());
	if (!worked) 
	{
		global_.song().New();
	}
	return worked;
}

bool input_psycle::decode_run_raw(audio_chunk & p_chunk, mem_block_container & p_raw, abort_callback & p_abort)
{
	throw pfc::exception_not_implemented();
}

void input_psycle::decode_seek(double p_seconds,abort_callback & p_abort)
{
	//t_uint64 sample = audio_math::time_to_samples(p_seconds, global_.conf().audioDriver().GetSamplesPerSec());

	psycle::host::Song& song = global_.song();
	int time_left = (int)p_seconds*1000;
	int patline=-1;
	int i=0;
	for (i=0;i<song.playLength;i++)
	{
		int pattern = song.playOrder[i];
		int tmp;
	
		if ((tmp = song.patternLines[pattern] * 60000/(song.BeatsPerMin() * song.LinesPerBeat())) >= time_left )
		{
			patline = time_left * (song.BeatsPerMin() * song.LinesPerBeat())/60000;
			break;
		}
		else time_left-=tmp;
	}
	global_.player().Start(i,patline);
}




//
// Internal methods
//

int input_psycle::CalcSongLength(psycle::host::Song& pSong)
{
	// take ff and fe commands into account
	
	float songLength = 0;
	int bpm = pSong.BeatsPerMin();
	int tpb = pSong.LinesPerBeat();
	for (int i=0; i <pSong.playLength; i++)
	{
		int pattern = pSong.playOrder[i];
		// this should parse each line for ffxx commands if you want it to be truly accurate
		unsigned char* const plineOffset = pSong._ppattern(pattern);
		for (int l = 0; l < pSong.patternLines[pattern]*psycle::host::MULTIPLY; l+=psycle::host::MULTIPLY)
		{
			for (int t = 0; t < pSong.SONGTRACKS*5; t+=5)
			{
				psycle::host::PatternEntry* pEntry = (psycle::host::PatternEntry*)(plineOffset+l+t);
				if(pEntry->_note < psycle::host::notecommands::tweak || pEntry->_note == psycle::host::notecommands::empty) // If This isn't a tweak (twk/tws/mcm) then do
				{
					switch(pEntry->_cmd)
					{
					case psycle::host::PatternCmd::SET_TEMPO:
						if(pEntry->_parameter != 0)
						{
							bpm=pEntry->_parameter;//+0x20; // ***** proposed change to ffxx command to allow more useable range since the tempo bar only uses this range anyway...
						}
						break;
					case psycle::host::PatternCmd::EXTENDED:
						if((pEntry->_parameter != 0) && ( (pEntry->_parameter&0xE0) == 0 )) // range from 0 to 1F for LinesPerBeat.
						{
							tpb=pEntry->_parameter;
						}
						break;
					}
				}
			}
			songLength += (60.0f/(bpm * tpb));
		}
	}
	
	return round<int>(songLength*1000.0f);
}


#include <psycle/host/Global.hpp>
#include <foobar2000/SDK/foobar2000.h>

namespace psycle {
	namespace host {
		class Song;
	}
}
// No inheritance. Our methods get called over input framework templates. See input_singletrack_impl for descriptions of what each method does.
class input_psycle
{
public:
	void open(service_ptr_t<file> p_filehint,const char * p_path,t_input_open_reason p_reason,abort_callback & p_abort);
	void get_info(file_info & p_info,abort_callback & p_abort);
	void decode_initialize(unsigned p_flags,abort_callback & p_abort);
	bool decode_run(audio_chunk & p_chunk,abort_callback & p_abort);
	bool decode_run_raw(audio_chunk & p_chunk, mem_block_container & p_raw, abort_callback & p_abort);
	void decode_seek(double p_seconds,abort_callback & p_abort);

	t_filestats get_file_stats(abort_callback & p_abort) {return m_file->get_stats(p_abort);}
	bool decode_can_seek() {return true;}
	bool decode_get_dynamic_info(file_info & p_out, double & p_timestamp_delta); // deals with dynamic information such as VBR bitrates
	bool decode_get_dynamic_info_track(file_info & p_out, double & p_timestamp_delta) {return false;} // deals with dynamic information such as track changes in live streams
	void decode_on_idle(abort_callback & p_abort) {m_file->on_idle(p_abort);}

	void retag(const file_info & p_info,abort_callback & p_abort) {throw exception_io_unsupported_format();}
	
	static bool g_is_our_content_type(const char * p_content_type) {return false;} // match against supported mime types here
	static bool g_is_our_path(const char * p_path,const char * p_extension) {return stricmp_utf8(p_extension,"psy") == 0;}
protected:
	int CalcSongLength(psycle::host::Song& pSong);
public:
	service_ptr_t<file> m_file;
};


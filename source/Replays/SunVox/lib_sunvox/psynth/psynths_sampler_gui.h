/*
This file is part of the SunVox library.
Copyright (C) 2007 - 2024 Alexander Zolotov <nightradio@gmail.com>
WarmPlace.ru

MINIFIED VERSION

License: (MIT)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.
*/

#define SMP_REC_SS_FLAG_DONT_LOAD_FILE	( 1 << 0 )
#define SMP_REC_SS_FLAG_CHECK_INPUTS	( 1 << 1 )
#define SMP_REC_SS_FLAG_CLOSE_THREAD	( 1 << 2 )
#define SMP_REC_SS_FLAG_SYNTH_THREAD	( 1 << 3 )
int sampler_rec( psynth_net* pnet, int mod_num, uint32_t flags, int start );
static bool sampler_rec_buf2file( MODULE_DATA* mdata, void* buf, sfs_file f, uint& rp )
{
    bool rv = 0;
    if( f && buf )
    {
	uint wp = atomic_load_explicit( &mdata->rec_wp, std::memory_order_acquire );
	if( wp != rp )
	{
	    uint size = 0;
	    if( wp > rp )
                size = wp - rp;
            else
                size = REC_FRAMES - rp;
	    uint frame_size = (uint)mdata->rec_channels * sizeof( PS_STYPE );
	    if( sfs_write( (int8_t*)buf + rp * frame_size, frame_size, size, f ) != size )
	    {
		static bool rec_error_msg = 0;
		if( rec_error_msg == 0 )
		{
		    slog( "Sample recording error: can't write to file\n" );
		    rec_error_msg = 1;
		}
	    }
	    else 
	    {
		rp += size;
		rp &= ( REC_FRAMES - 1 );
		rv = 1;
	    }
	}
    }
    return rv;
}
void* sampler_rec_thread( void* user_data )
{
    psynth_module* mod = (psynth_module*)user_data;
    MODULE_DATA* mdata = (MODULE_DATA*)mod->data_ptr;
    psynth_net* pnet = mod->pnet;
    int mod_num = mdata->mod_num;
#ifdef SUNVOX_GUI
    window_manager* wm = nullptr;
    sampler_visual_data* vis_data = nullptr;
    sunvox_engine* sv = (sunvox_engine*)pnet->host;
    WINDOWPTR sunvox_win = nullptr;
    if( mod->visual )
    {
	wm = mod->visual->wm;
	vis_data = (sampler_visual_data*)mod->visual->data;
    }
    if( sv ) sunvox_win = sv->win;
#endif
    bool mod_mutex_locked = 0;
    while( 1 )
    {
	char filename[ 64 ];
	sprintf( filename, "3:/sampler_%d_recording", mod_num );
#ifdef SUNVOX_GUI
	if( sunvox_win && wm ) send_event( sunvox_win, EVT_SUNVOXCMD, 0, 1, mod_num, 0, 0, 0, wm ); 
#endif
	sfs_file rec_file = sfs_open( filename, "wb" );
	if( !rec_file ) 
	{
	    slog( "REC THREAD: can't open file %s for writing\n", filename );
	    break;
	}
	uint rec_rp = 0;
	int stop_req = 0;
	while( 1 )
	{
	    stop_req = atomic_load_explicit( &mdata->rec_thread_stop_request, std::memory_order_relaxed );
	    bool received = sampler_rec_buf2file( mdata, mdata->rec_buf, rec_file, rec_rp );
	    if( stop_req ) break;
	    if( !received ) stime_sleep( 10 );
	}
	bool dont_load_file = false;
	if( stop_req >= 2 )
	    dont_load_file = true;
	sfs_close( rec_file );
	if( smutex_lock( psynth_get_mutex( mod_num, pnet ) ) ) break;
	mod_mutex_locked = 1;
	uint size = sfs_get_file_size( filename );
	if( size > 0 && !dont_load_file )
	{
	    bool convert_32_to_16 = 0;
	    int bits;
	    int chan = mdata->rec_channels;
#if !defined(PS_STYPE_FLOAT32) && !defined(PS_STYPE_INT16)
    #error Unknown PS_STYPE
#endif
#ifdef PS_STYPE_FLOAT32
	    bits = 32;
	    if( mdata->rec_16bit )
	    {
	        bits = 16;
	        size /= 2;
	        convert_32_to_16 = 1;
	    }
#endif
#ifdef PS_STYPE_INT16
	    bits = 16;
#endif
	    int freq = pnet->sampling_freq;
	    if( mdata->rec_reduced_freq )
	        freq /= 2;
	    int sample_num = -1;
#ifdef SUNVOX_GUI
	    if( vis_data )
	    {
		sample_editor_data_l* smp_ed_l = get_sample_editor_data_l( vis_data->editor_window );
		if( smp_ed_l )
		{
	    	    sample_num = smp_ed_l->sample_num;
		}
	    }
#endif
	    void* smp_data = create_raw_instrument_or_sample( 
	        filename, 
		mod_num, 
		size, 
		bits, 
		chan, 
		freq,
		pnet, 
		sample_num );
	    if( smp_data )
	    {
	        sample* smp = (sample*)psynth_get_chunk_data( mod_num, CHUNK_SMP( sample_num ), pnet );
		sfs_file f = sfs_open( filename, "rb" );
		if( f )
		{
		    if( convert_32_to_16 )
		    {
		        int16_t* dest = (int16_t*)smp_data;
		        for( uint i = 0; i < size / sizeof( int16_t ); i++ )
		        {
		    	    PS_STYPE v;
			    sfs_read( &v, sizeof( v ), 1, f );
			    PS_STYPE_TO_INT16( dest[ i ], v )
			}
		    }
		    else
		    {
		        sfs_read( smp_data, 1, size, f );
		    }
		    sfs_close( f );
		}
#ifdef PS_STYPE_INT16
		PS_STYPE* s = (PS_STYPE*)smp_data;
		for( uint i = 0; i < size / sizeof( PS_STYPE ); i++ ) 
		    PS_STYPE_TO_INT16( s[ i ], s[ i ] )
#endif
	    }
	    else
	    {
	        slog( "REC THREAD: can't create a new sample\nNot enough memory or file IO error" );
	    }
	}
	if( mod_mutex_locked ) smutex_unlock( psynth_get_mutex( mod_num, pnet ) );
	mod_mutex_locked = 0;
	sfs_remove_file( filename );
	atomic_store( &mdata->rec_thread_state, (int)0 );
#ifdef SUNVOX_GUI
	if( sunvox_win && wm ) send_event( sunvox_win, EVT_SUNVOXCMD, 0, 1, mod_num, 0, 0, 0, wm ); 
#endif
	ssemaphore_wait( &mdata->rec_thread_sem, STHREAD_TIMEOUT_INFINITE );
	if( atomic_load( &mdata->rec_thread_stop_request ) == 3 ) break;
    }
    if( mod_mutex_locked ) smutex_unlock( psynth_get_mutex( mod_num, pnet ) );
    atomic_store( &mdata->rec_thread_state, (int)0 );
    return 0;
}
int sampler_rec( psynth_net* pnet, int mod_num, uint32_t flags, int start )
{
    psynth_module* mod = &pnet->mods[ mod_num ];
    MODULE_DATA* mdata = (MODULE_DATA*)mod->data_ptr;
#ifdef SUNVOX_GUI
    if( mod->visual )
    {
	if( !mdata->rec && ( flags & SMP_REC_SS_FLAG_CHECK_INPUTS ) )
	{
	    window_manager* wm = mod->visual->wm;
	    sampler_visual_data* vis_data = (sampler_visual_data*)mod->visual->data;
	    sunvox_engine* sv = (sunvox_engine*)pnet->host;
	    WINDOWPTR sunvox_win = nullptr;
	    if( sv ) sunvox_win = sv->win;
	    bool input_exist = false;
	    for( int i = 0; i < mod->input_links_num; i++ )
	    {
		int l = mod->input_links[ i ];
		if( (unsigned)l < (unsigned)pnet->mods_num )
		{
		    input_exist = true;
		    break;
		}
	    }
	    if( input_exist == false )
	    {
		if( dialog( 0, ps_get_string( STR_PS_NOTHING_TO_RECORD ), ps_get_string( STR_PS_CONTINUE_CANCEL ), wm ) != 0 )
		    return 0;
	    }
	}
    }
#endif
    bool btn_mutex_locked = 0;
    bool mod_mutex_locked = 0;
    while( 1 )
    {
	smutex_lock( &mdata->rec_btn_mutex );
	btn_mutex_locked = 1;
	bool ignore = 0;
	int state = atomic_load( &mdata->rec_thread_state );
	switch( start )
	{
	    case 0: 
		if( !state ) ignore = 1;
		mdata->rec_pause = 0;
		break;
	    case 1: 
		mdata->rec_pause = 1;
		ignore = 1;
		break;
	    case 2: 
		if( state ) ignore = 1;
		mdata->rec_pause = 0;
		break;
	}
	if( ignore )
	{
	    if( ( flags & SMP_REC_SS_FLAG_CLOSE_THREAD ) == 0 )
		break;
	}
        if( start == 2 )
	{
	    int rec_channels = 2;
	    if( mdata->opt->rec_mono ) rec_channels = 1;
	    int rec_sample_size = sizeof( PS_STYPE );
	    int rec_buf_size = REC_FRAMES * rec_sample_size * rec_channels;
	    if( (int)smem_get_size( mdata->rec_buf ) < rec_buf_size )
		mdata->rec_buf = smem_resize( mdata->rec_buf, rec_buf_size );
	    if( ( flags & SMP_REC_SS_FLAG_SYNTH_THREAD ) == 0 )
	    {
	    	if( smutex_lock( psynth_get_mutex( mod_num, pnet ) ) == 0 )
	    	    mod_mutex_locked = 1;
	    	else
	    	    break;
	    }
	    if( mdata->opt->rec_on_play )
		psynth_set_flags( mod_num, PSYNTH_FLAG_GET_PLAY_COMMANDS, 0, pnet );
    	    if( mdata->opt->finish_rec_on_stop )
		psynth_set_flags( mod_num, PSYNTH_FLAG_GET_STOP_COMMANDS, 0, pnet );
	    mdata->rec_play_pressed = false;
    	    mdata->rec_channels = rec_channels;
	    mdata->rec_reduced_freq = mdata->opt->rec_reduced_freq;
	    mdata->rec_16bit = mdata->opt->rec_16bit;
	    mdata->rec = true;
	    atomic_store( &mdata->rec_wp, (uint)0 );
	    mdata->rec_frames = 0;
	    if( mod_mutex_locked )
	    {
	    	smutex_unlock( psynth_get_mutex( mod_num, pnet ) );
	    	mod_mutex_locked = 0;
	    }
	    if( sthread_is_empty( &mdata->rec_thread ) )
	    {
		sundog_engine* sd = nullptr; GET_SD_FROM_PSYNTH_NET( pnet, sd );
		atomic_store( &mdata->rec_thread_stop_request, (int)0 );
		atomic_store( &mdata->rec_thread_state, (int)1 );
		ssemaphore_create( &mdata->rec_thread_sem, nullptr, 0, 0 );
		sthread_create( &mdata->rec_thread, sd, sampler_rec_thread, mod, 0 );
	    }
	    else
	    {
		atomic_store( &mdata->rec_thread_stop_request, (int)0 );
		atomic_store( &mdata->rec_thread_state, (int)1 );
		ssemaphore_release( &mdata->rec_thread_sem );
	    }
	}
	if( start == 0 )
	{
	    if( ( flags & SMP_REC_SS_FLAG_SYNTH_THREAD ) == 0 )
	    {
	    	if( smutex_lock( psynth_get_mutex( mod_num, pnet ) ) == 0 )
	    	    mod_mutex_locked = 1;
	    	else
	    	    break;
	    }
	    psynth_set_flags( mod_num, 0, PSYNTH_FLAG_GET_PLAY_COMMANDS | PSYNTH_FLAG_GET_STOP_COMMANDS, pnet );
	    mdata->rec_play_pressed = false;
	    mdata->rec = 0;
	    if( mod_mutex_locked )
	    {
	    	smutex_unlock( psynth_get_mutex( mod_num, pnet ) );
	    	mod_mutex_locked = 0;
	    }
	    int stop_req = 1;
	    if( flags & SMP_REC_SS_FLAG_DONT_LOAD_FILE )
		stop_req = 2;
	    atomic_store( &mdata->rec_thread_stop_request, (int)stop_req );
	    if( flags & SMP_REC_SS_FLAG_CLOSE_THREAD )
	    {
		while( atomic_load( &mdata->rec_thread_state ) ) stime_sleep(1);
		atomic_store( &mdata->rec_thread_stop_request, (int)3 );
		ssemaphore_release( &mdata->rec_thread_sem );
		sthread_destroy( &mdata->rec_thread, 1000 );
		ssemaphore_destroy( &mdata->rec_thread_sem );
	    }
	}
	break;
    }
    if( btn_mutex_locked ) smutex_unlock( &mdata->rec_btn_mutex );
    if( mod_mutex_locked ) smutex_unlock( psynth_get_mutex( mod_num, pnet ) );
    return 0;
}

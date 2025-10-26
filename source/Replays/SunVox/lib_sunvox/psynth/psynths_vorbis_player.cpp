/*
This file is part of the SunVox library.
Copyright (C) 2007 - 2025 Alexander Zolotov <nightradio@gmail.com>
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

#include "psynth_net.h"
#include "tremor/ivorbiscodec.h"
#include "tremor/ivorbisfile.h"
#include "psynths_vorbis_player.h"
#include "sunvox_engine.h"
#define MODULE_DATA	psynth_vplayer_data
#define MODULE_HANDLER	psynth_vplayer
#define MODULE_INPUTS	0
#define MODULE_OUTPUTS	2
#define MAX_CHANNELS	4
#define PCMBUF_SAMPLES 	256
#define PCMBUF_BYTES 	( PCMBUF_SAMPLES * sizeof( int16_t ) )
struct gen_channel
{
    bool    		playing;
    uint    		id;
    int	    		vel;
    int	    		ptr_h;
    int	    		ptr_l;
    uint    		delta_h;
    uint    		delta_l;
    OggVorbis_File  	vf;
    bool	    	vf_open;
    tremor_vorbis_info*    	vi;
    size_t    		src_offset;
    sfs_file 		f;
    int	    		loaded;
    int16_t    		pcmbuf[ PCMBUF_SAMPLES ]; 
};
struct MODULE_DATA
{
    PS_CTYPE  		ctl_volume;
    PS_CTYPE   		ctl_channels;
    PS_CTYPE   		ctl_static_freq;
    PS_CTYPE   		ctl_fine; 
    PS_CTYPE   		ctl_rel; 
    PS_CTYPE   		ctl_interp;
    PS_CTYPE   		ctl_loop;
    PS_CTYPE		ctl_ignore_noteoff;
    gen_channel   	channels[ MAX_CHANNELS + 1 ];
    bool    		no_active_channels;
    int	    		search_ptr;
    int	    		base_freq;
    int	    		base_pitch;
    uint*   		linear_freq_tab;
    int16_t    		pcmbuf[ PCMBUF_SAMPLES ]; 
    ov_callbacks	vc;
    void* 		src;
    char*	    	src_file;
    size_t 		src_size;
    uint64_t 		src_pcm_total; 
    int 		cur_chan;
    int	    		pause;
#ifdef SUNVOX_GUI
    window_manager* 	wm;
#endif
};
int vplayer_get_base_note( int mod_num, psynth_net* pnet )
{
    if( !pnet ) return 0;
    if( (unsigned)mod_num >= pnet->mods_num ) return 0;
    psynth_module* mod = &pnet->mods[ mod_num ];
    if( ( mod->flags & PSYNTH_FLAG_EXISTS ) == 0 ) return 0;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    if( !data->src && !data->src_file ) return 0;
    return ( 7680 - data->base_pitch ) / 64 - ( data->ctl_rel - 128 );
}
void vplayer_get_base_pitch( int mod_num, psynth_net* pnet )
{
    if( !pnet ) return;
    if( (unsigned)mod_num >= pnet->mods_num ) return;
    psynth_module* mod = &pnet->mods[ mod_num ];
    if( ( mod->flags & PSYNTH_FLAG_EXISTS ) == 0 ) return;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    if( !data->src && !data->src_file ) return;
    int base_freq = 1;
    OggVorbis_File vf;
    data->cur_chan = MAX_CHANNELS;
    data->channels[ MAX_CHANNELS ].src_offset = 0;
    int rv = tremor_ov_open_callbacks( (void*)data, &vf, 0, 0, data->vc );
    if( rv == 0 )
    {
	tremor_vorbis_info* vi = tremor_ov_info( &vf, -1 );
	base_freq = vi->rate;
	tremor_ov_clear( &vf );
    }
    int dist = 10000000;
    int pitch = 0;
    for( int p = 0; p < 7680; p++ )
    {
        int f;
        PSYNTH_GET_FREQ( data->linear_freq_tab, f, p );
        int d = f - base_freq;
        if( d < 0 ) d = -d;
        if( d <= dist )
        {
            dist = d;
            pitch = p;
        }
        else break;
    }
    data->base_freq = base_freq;
    data->base_pitch = pitch;
}
void vplayer_set_base_note( int base_note, int mod_num, psynth_net* pnet )
{
    if( !pnet ) return;
    if( (unsigned)mod_num >= pnet->mods_num ) return;
    psynth_module* mod = &pnet->mods[ mod_num ];
    if( ( mod->flags & PSYNTH_FLAG_EXISTS ) == 0 ) return;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    if( !data->src && !data->src_file ) return;
    vplayer_get_base_pitch( mod_num, pnet );
    int cur_base_note = ( 7680 - data->base_pitch ) / 64;
    int cur_base_finetune = ( 7680 - data->base_pitch ) & 63;
    data->ctl_fine = cur_base_finetune * 2;
    data->ctl_rel = cur_base_note - base_note + 128;
}
void vplayer_set_filename( int mod_num, char* filename, psynth_net* pnet )
{
    if( !pnet ) return;
    if( (unsigned)mod_num >= pnet->mods_num ) return;
    psynth_module* mod = &pnet->mods[ mod_num ];
    if( ( mod->flags & PSYNTH_FLAG_EXISTS ) == 0 ) return;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    smem_free( data->src_file );
    data->src_file = NULL;
    if( filename )
    {
	int len = smem_strlen( filename ) + 1;
	data->src_file = SMEM_ALLOC2( char, len );
	smem_copy( data->src_file, filename, len );
    }
}
uint64_t vplayer_get_pcm_time( int mod_num, psynth_net* pnet )
{
    if( !pnet ) return 0;
    if( (unsigned)mod_num >= pnet->mods_num ) return 0;
    psynth_module* mod = &pnet->mods[ mod_num ];
    if( ( mod->flags & PSYNTH_FLAG_EXISTS ) == 0 ) return 0;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    if( !data->src && !data->src_file ) return -1;
    for( int c = 0; c < data->ctl_channels; c++ ) 
    {
	if( data->channels[ c ].playing )
	{
	    return tremor_ov_pcm_tell( &data->channels[ c ].vf );
	}
    }
    return -1;
}
uint64_t vplayer_get_total_pcm_time( int mod_num, psynth_net* pnet )
{
    if( !pnet ) return 0;
    if( (unsigned)mod_num >= pnet->mods_num ) return 0;
    psynth_module* mod = &pnet->mods[ mod_num ];
    if( ( mod->flags & PSYNTH_FLAG_EXISTS ) == 0 ) return 0;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    if( !data->src && !data->src_file ) return 0;
    OggVorbis_File vf;
    data->cur_chan = MAX_CHANNELS;
    data->channels[ MAX_CHANNELS ].src_offset = 0;
    int rv = tremor_ov_open_callbacks( (void*)data, &vf, 0, 0, data->vc );
    if( rv == 0 )
    {
	uint64_t t = tremor_ov_pcm_total( &vf, -1 );
	tremor_ov_clear( &vf );
	return t;
    }
    return 0;
}
void vplayer_set_pcm_time( int mod_num, uint64_t t, psynth_net* pnet )
{
    if( !pnet ) return;
    if( (unsigned)mod_num >= pnet->mods_num ) return;
    psynth_module* mod = &pnet->mods[ mod_num ];
    if( ( mod->flags & PSYNTH_FLAG_EXISTS ) == 0 ) return;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    if( !data->src && !data->src_file ) return;
    for( int c = 0; c < data->ctl_channels; c++ ) 
    {
	if( data->channels[ c ].playing )
	{
	    tremor_ov_pcm_seek( &data->channels[ c ].vf, t );
	    break;
	}
    }
}
int vplayer_load_file( int mod_num, const char* filename, sfs_file f, psynth_net* pnet )
{
    if( !pnet ) return -1;
    if( (unsigned)mod_num >= pnet->mods_num ) return -1;
    psynth_module* mod = &pnet->mods[ mod_num ];
    if( ( mod->flags & PSYNTH_FLAG_EXISTS ) == 0 ) return -1;
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    int rv = -1;
    size_t fsize = 0;
    bool need_close = 0;
    void* src = NULL;
    bool locked = 0;
    while( 1 )
    {
	if( f == 0 )
	{
	    fsize = sfs_get_file_size( filename );
	    if( fsize == 0 ) break;
	    f = sfs_open( filename, "rb" );
	    if( f == 0 ) break;
	    need_close = 1;
	}
	else
	{
	    size_t file_begin = sfs_tell( f );
            sfs_seek( f, 0, SFS_SEEK_END );
            fsize = sfs_tell( f ) - file_begin;
    	    sfs_seek( f, file_begin, SFS_SEEK_SET );
	}
	int mutex_res = smutex_lock( psynth_get_mutex( mod_num, pnet ) );
        if( mutex_res != 0 )
	{
	    slog( "load ogg: mutex lock error %d\n", mutex_res );
    	    break;
	}
	locked = 1;
        for( int c = 0; c <= MAX_CHANNELS; c++ ) 
	{
	    if( data->channels[ c ].vf_open )
    	    {
		data->cur_chan = c;
		tremor_ov_clear( &data->channels[ c ].vf );
		data->channels[ c ].vf_open = 0;
		data->channels[ c ].playing = 0;
		data->channels[ c ].id = ~0;
	    }
	}
	data->no_active_channels = 1;
        psynth_new_chunk( mod_num, 0, fsize, 0, 0, pnet );
        src = psynth_get_chunk_data( mod_num, 0, pnet );
	if( !src ) break;
        data->src = src;
	data->src_size = fsize;
        sfs_read( src, 1, fsize, f );
        data->src_pcm_total = vplayer_get_total_pcm_time( mod_num, pnet );
	vplayer_set_base_note( 5 * 12, mod_num, pnet );
        mod->draw_request++;
	pnet->change_counter++;
	rv = 0;
	break;
    }
    if( locked )
	smutex_unlock( psynth_get_mutex( mod_num, pnet ) );
    if( f && need_close ) sfs_close( f );
    return rv;
}
#ifdef SUNVOX_GUI
#include "../../sunvox/main/sunvox_gui.h"
struct vplayer_visual_data
{
    MODULE_DATA*	module_data;
    int			mod_num;
    psynth_net*		pnet;
    WINDOWPTR		load_button;
};
int vplayer_load_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    vplayer_visual_data* data = (vplayer_visual_data*)user_data;
    char* name = NULL;
    name = fdialog( ps_get_string( STR_PS_LOAD_OGG_VORBIS_FILE ), "ogg", ".vplayer_f", 0, FDIALOG_FLAG_LOAD, wm );
    vplayer_load_file( data->mod_num, name, 0, data->pnet );
    return 0;
}
int vplayer_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    vplayer_visual_data* data = (vplayer_visual_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    return sizeof( vplayer_visual_data );
	    break;
	case EVT_AFTERCREATE:
	    data->pnet = (psynth_net*)win->host;
	    data->module_data = NULL;
	    data->mod_num = -1;
	    data->load_button = new_window( wm_get_string( STR_WM_LOAD ), 0, 0, 1, 1, wm->button_color, win, button_handler, wm );
	    set_handler( data->load_button, vplayer_load_handler, data, wm );
	    set_window_controller( data->load_button, 0, wm, (WCMD)0, CEND );
	    set_window_controller( data->load_button, 1, wm, (WCMD)0, CEND );
	    set_window_controller( data->load_button, 2, wm, (WCMD)wm->button_xsize, CEND );
	    set_window_controller( data->load_button, 3, wm, (WCMD)wm->text_ysize, CEND );
	    retval = 1;
	    break;
	case EVT_DRAW:
	    if( data->module_data->src )
	    {
		wbd_lock( win );
		draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
		char ts[ 512 ];
		char ts2[ 3 ];
		uint base_note = vplayer_get_base_note( data->mod_num, data->pnet ) - data->pnet->mods[ data->mod_num ].relative_note;
		int ft = data->pnet->mods[ data->mod_num ].finetune;
                if( ft >= 128 ) base_note--;
                if( ft < -128 ) base_note++;
		ts2[ 0 ] = g_n2c[ base_note % 12 ];
		ts2[ 1 ] = '?';
		int oct = base_note / 12;
		if( oct >= 0 && oct < 10 ) ts2[ 1 ] = '0' + oct;
		ts2[ 2 ] = 0;
		wm->cur_font_color = wm->color2;
		sprintf( ts, "%s: %s\n", ps_get_string( STR_PS_BASE_NOTE ), ts2 );
		draw_string( ts, 0, wm->text_ysize + wm->interelement_space, wm );
		sprintf( ts, "%d %s\n", data->module_data->base_freq, wm_get_string( STR_WM_HZ ) );
		draw_string( ts, 0, wm->text_ysize + wm->interelement_space + char_y_size( wm ), wm );
		sprintf( ts, "%d %s\n", (int)data->module_data->src_size, wm_get_string( STR_WM_BYTES ) );
		draw_string( ts, 0, wm->text_ysize + wm->interelement_space + char_y_size( wm ) * 2, wm );
		uint seconds = data->module_data->src_pcm_total / data->module_data->base_freq;
		sprintf( ts, "%02d:%02d\n", seconds / 60, seconds % 60 );
		draw_string( ts, 0, wm->text_ysize + wm->interelement_space + char_y_size( wm ) * 3, wm );
		wbd_draw( wm );
		wbd_unlock( wm );
		retval = 1;
	    }
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_MOUSEBUTTONUP:
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    retval = 1;
	    break;
    }
    return retval;
}
#endif
size_t vplayer_read( void* ptr, size_t s, size_t nmemb, void* datasource )
{
    MODULE_DATA* data = (MODULE_DATA*)datasource;
    gen_channel* chan = &data->channels[ data->cur_chan ];
    if( data->src )
    {
	if( chan->src_offset >= data->src_size ) return 0;
	size_t size = s * nmemb;
	size_t can_read = data->src_size - chan->src_offset;
	if( can_read < size ) size = can_read;
	smem_copy( ptr, (char*)data->src + chan->src_offset, size );
	chan->src_offset += size;
	return size;
    }
    if( data->src_file )
    {
	if( chan->f == 0 )
	{
	    chan->f = sfs_open( data->src_file, "rb" );
	    if( chan->f == 0 ) return 0;
	}
	return sfs_read( ptr, 1, s * nmemb, chan->f );
    }
    return 0;
}
int vplayer_seek( void* datasource, ogg_int64_t offset, int whence )
{
    MODULE_DATA* data = (MODULE_DATA*)datasource;
    gen_channel* chan = &data->channels[ data->cur_chan ];
    if( data->src )
    {
	switch( whence )
	{
	    case 0: chan->src_offset = offset; break;
	    case 1: chan->src_offset += offset; break;
	    case 2: chan->src_offset = data->src_size + offset; break;
	}
	if( chan->src_offset >= data->src_size ) return -1;
	return 0;
    }
    if( data->src_file )
    {
	if( chan->f )
	{
	    return sfs_seek( chan->f, offset, whence );
	}
    }
    return 0;
}
int vplayer_close( void* datasource )
{
    MODULE_DATA* data = (MODULE_DATA*)datasource;
    gen_channel* chan = &data->channels[ data->cur_chan ];
    if( data->src )
    {
	chan->src_offset = 0;
	return 0;
    }
    if( data->src_file )
    {
	if( chan->f )
	{
	    sfs_close( chan->f );
	    chan->f = 0;
	}
    }
    return 0;
}
long vplayer_tell( void* datasource )
{
    MODULE_DATA* data = (MODULE_DATA*)datasource;
    gen_channel* chan = &data->channels[ data->cur_chan ];
    if( data->src )
    {
	return chan->src_offset;
    }
    if( data->src_file )
    {
	if( chan->f )
	{
	    return sfs_tell( chan->f );
	}
    }
    return 0;
}
PS_RETTYPE MODULE_HANDLER( 
    PSYNTH_MODULE_HANDLER_PARAMETERS
    )
{
    psynth_module* mod;
    MODULE_DATA* data;
    if( mod_num >= 0 )
    {
	mod = &pnet->mods[ mod_num ];
	data = (MODULE_DATA*)mod->data_ptr;
    }
    PS_RETTYPE retval = 0;
    switch( event->command )
    {
	case PS_CMD_GET_DATA_SIZE:
	    retval = sizeof( MODULE_DATA );
	    break;
	case PS_CMD_GET_NAME:
	    retval = (PS_RETTYPE)"Vorbis player";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Vorbis player - проигрыватель файлов в формате OGG Vorbis (открытый аналог MP3). Файл сохраняется в памяти модуля, повторная загрузка с диска не производится.";
                        break;
                    }
		    retval = (PS_RETTYPE)"OGG Vorbis player. Vorbis is a patent-free and open-source audio coding format. Better than MP3.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#0082FF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR | PSYNTH_FLAG_USE_MUTEX; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 8, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 512, 256, 0, &data->ctl_volume, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ORIGINAL_SPEED ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 1, 1, &data->ctl_static_freq, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FINETUNE ), "", -128, 128, 0, 0, &data->ctl_fine, 0, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_TRANSPOSE ), "", 0, 256, 128, 1, &data->ctl_rel, 128, 0, pnet );
	    psynth_set_ctl_show_offset( mod_num, 3, -128, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_INTERPOLATION ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 1, 1, &data->ctl_interp, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_POLYPHONY ), ps_get_string( STR_PS_CH ), 1, MAX_CHANNELS, 1, 1, &data->ctl_channels, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_REPEAT ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_loop, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_IGNORE_NOTEOFF ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_ignore_noteoff, -1, 1, pnet );
	    for( int c = 0; c <= MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].vf_open = 0;
		data->channels[ c ].playing = 0;
		data->channels[ c ].id = ~0;
		data->channels[ c ].src_offset = 0;
		data->channels[ c ].loaded = 0;
		data->channels[ c ].f = 0;
	    }
	    data->no_active_channels = 1;
	    data->search_ptr = 0;
	    data->linear_freq_tab = g_linear_freq_tab;
	    data->vc.read_func = &vplayer_read;
	    data->vc.seek_func = &vplayer_seek;
	    data->vc.close_func = &vplayer_close;
	    data->vc.tell_func = &vplayer_tell;
	    data->src = 0;
	    data->src_file = 0;
	    data->src_pcm_total = 0;
	    data->pause = 0;
#ifdef SUNVOX_GUI
	    {
		data->wm = 0;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
		    mod->visual = new_window( "VPlayer GUI", 0, 0, 10, 10, wm->button_color, 0, pnet, vplayer_visual_handler, wm );
		    mod->visual_min_ysize = 0;
		    vplayer_visual_data* data1 = (vplayer_visual_data*)mod->visual->data;
		    data1->module_data = data;
		    data1->mod_num = mod_num;
		    data1->pnet = pnet;
		}
	    }
#endif
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    {
		data->src = psynth_get_chunk_data( mod_num, 0, pnet );
		size_t size = 0;
		psynth_get_chunk_info( mod_num, 0, pnet, &size, 0, 0 );
		data->src_size = size;
		data->src_pcm_total = vplayer_get_total_pcm_time( mod_num, pnet );
		vplayer_get_base_pitch( mod_num, pnet );
	    }
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** outputs = mod->channels_out;
    		if( data->pause ) break;
		int16_t* RESTRICT main_pcmbuf = data->pcmbuf;
		data->no_active_channels = 1;
		for( int c = 0; c < data->ctl_channels; c++ )
		{
		    gen_channel* chan = &data->channels[ c ];
		    if( !chan->playing ) continue;
		    data->cur_chan = c;
		    data->no_active_channels = 0;
		    int offset = mod->offset;
		    int frames = mod->frames;
#ifdef PS_STYPE_FLOATINGPOINT
		    PS_STYPE vol = ( (PS_STYPE)data->ctl_volume / 256.0 ) * ( (PS_STYPE)chan->vel / 256.0 );
		    PS_STYPE norm_vol = 1.0;
#else
		    int vol = ( data->ctl_volume * chan->vel ) / 256;
		    int norm_vol = 256;
#endif
		    uint delta_h;
		    uint delta_l;
		    if( data->ctl_static_freq )
		    {
			float delta = (float)chan->vi->rate / (float)pnet->sampling_freq;
			uint delta_i = (uint)( delta * (float)( 1 << PSYNTH_FP64_PREC ) );
			delta_h = delta_i >> PSYNTH_FP64_PREC;
			delta_l = delta_i & ( ( 1 << PSYNTH_FP64_PREC ) - 1 );
		    }
		    else
		    {
			delta_h = chan->delta_h;
			delta_l = chan->delta_l;
		    }
		    int16_t* RESTRICT chan_pcmbuf = chan->pcmbuf;
		    while( frames > 0 && chan->playing )
		    {
			int read_rv = 1; 
			while( chan->ptr_h + 1 >= chan->loaded )
			{
			    uint bytes_to_read = PCMBUF_BYTES - 8; 
			    int pcm_offset = ( chan->loaded * chan->vi->channels * sizeof( int16_t ) ) & ( PCMBUF_BYTES - 1 );
			    int current_section;
			    read_rv = tremor_ov_read( &chan->vf, main_pcmbuf, bytes_to_read, &current_section );
			    if( read_rv <= 0 )
			    {
				if( read_rv == 0 && data->ctl_loop )
				{
				    tremor_ov_time_seek( &chan->vf, 0 );
				}
				else
				{
				    chan->playing = 0;
				    chan->id = ~0;
				    break;
				}
			    }
			    else
			    {
				int16_t* pcmbuf_src = main_pcmbuf;
				for( uint i = pcm_offset / sizeof( int16_t ); i < ( pcm_offset + read_rv ) / sizeof( int16_t ); i++ )
				{
				    chan_pcmbuf[ i & ( PCMBUF_SAMPLES - 1 ) ] = *pcmbuf_src;
				    pcmbuf_src++;
				}
				int read_frames = read_rv / ( chan->vi->channels * sizeof( int16_t ) );
				chan->loaded += read_frames;
			    }
			}
			int ptr_h;
			int ptr_l;
			int frames_filled = 0;
			int loaded = chan->loaded;
			int outputs_num = psynth_get_number_of_outputs( mod );
			for( int ch = 0; ch < outputs_num; ch++ )
			{
			    PS_STYPE* out = outputs[ ch ] + offset;
			    ptr_h = chan->ptr_h;
			    ptr_l = chan->ptr_l;
			    if( read_rv > 0 )
			    {
				if( data->ctl_interp && !( delta_h == 1 && delta_l == 0 ) )
				{
				    if( chan->vi->channels == 1 )
				    {
					for( frames_filled = 0; frames_filled < frames; frames_filled++ )
					{
					    if( ptr_h + 1 >= loaded ) break;
					    int v1 = chan_pcmbuf[ ptr_h & ( PCMBUF_SAMPLES - 1 ) ];
					    int v2 = chan_pcmbuf[ ( ptr_h + 1 ) & ( PCMBUF_SAMPLES - 1 ) ];
					    v1 = v1 * ( ( ( 1 << PSYNTH_FP64_PREC ) - 1 ) - ptr_l );
					    v1 >>= PSYNTH_FP64_PREC;
					    v2 = v2 * ptr_l;
					    v2 >>= PSYNTH_FP64_PREC;
					    int v = v1 + v2;
					    PS_STYPE2 s;
					    PS_INT16_TO_STYPE( s, v );
#ifdef PS_STYPE_FLOATINGPOINT
					    if( retval == 0 ) *out = s * vol; else *out += s * vol;
#else
					    s *= vol;
					    s /= 256;
					    if( retval == 0 ) *out = s; else *out += s;
#endif
					    out++;
					    PSYNTH_FP64_ADD( ptr_h, ptr_l, delta_h, delta_l );
					}
				    }
				    if( chan->vi->channels == 2 )
				    {
					for( frames_filled = 0; frames_filled < frames; frames_filled++ )
					{
					    if( ptr_h + 1 >= loaded ) break;
					    int v1 = chan_pcmbuf[ ( ( ptr_h * 2 ) & ( PCMBUF_SAMPLES - 1 ) ) + ch ];
					    int v2 = chan_pcmbuf[ ( ( ( ptr_h + 1 ) * 2 ) & ( PCMBUF_SAMPLES - 1 ) ) + ch ];
					    v1 = v1 * ( ( ( 1 << PSYNTH_FP64_PREC ) - 1 ) - ptr_l );
					    v1 >>= PSYNTH_FP64_PREC;
					    v2 = v2 * ptr_l;
					    v2 >>= PSYNTH_FP64_PREC;
					    int v = v1 + v2;
					    PS_STYPE2 s;
					    PS_INT16_TO_STYPE( s, v );
#ifdef PS_STYPE_FLOATINGPOINT
					    if( retval == 0 ) *out = s * vol; else *out += s * vol;
#else
					    s *= vol;
					    s /= 256;
					    if( retval == 0 ) *out = s; else *out += s;
#endif
					    out++;
					    PSYNTH_FP64_ADD( ptr_h, ptr_l, delta_h, delta_l );
					}
				    }
				}
				else
				{
				    if( vol == norm_vol )
				    {
					if( chan->vi->channels == 1 )
					{
					    for( frames_filled = 0; frames_filled < frames; frames_filled++ )
					    {
						if( ptr_h >= loaded ) break;
						int16_t v = chan_pcmbuf[ ptr_h & ( PCMBUF_SAMPLES - 1 ) ];
						PS_STYPE s;
						PS_INT16_TO_STYPE( s, v );
						if( retval == 0 ) *out = s; else *out += s;
						out++;
						PSYNTH_FP64_ADD( ptr_h, ptr_l, delta_h, delta_l );
					    }
					}
					if( chan->vi->channels == 2 )
					{
					    for( frames_filled = 0; frames_filled < frames; frames_filled++ )
					    {
						if( ptr_h >= loaded ) break;
						int16_t v = chan_pcmbuf[ ( ( ( ptr_h * 2 ) & ( PCMBUF_SAMPLES - 1 ) ) ) + ch ];
						PS_STYPE s;
						PS_INT16_TO_STYPE( s, v );
						if( retval == 0 ) *out = s; else *out += s;
						out++;
						PSYNTH_FP64_ADD( ptr_h, ptr_l, delta_h, delta_l );
					    }
					}
				    }
				    else
				    {
					if( chan->vi->channels == 1 )
					{
					    for( frames_filled = 0; frames_filled < frames; frames_filled++ )
					    {
						if( ptr_h >= loaded ) break;
						int16_t v = chan_pcmbuf[ ptr_h & ( PCMBUF_SAMPLES - 1 ) ];
						PS_STYPE2 s;
						PS_INT16_TO_STYPE( s, v );
#ifdef PS_STYPE_FLOATINGPOINT
						if( retval == 0 ) *out = s * vol; else *out += s * vol;
#else
						s *= vol;
						s /= 256;
						if( retval == 0 ) *out = s; else *out += s;
#endif
						out++;
						PSYNTH_FP64_ADD( ptr_h, ptr_l, delta_h, delta_l );
					    }
					}
					if( chan->vi->channels == 2 )
					{
					    for( frames_filled = 0; frames_filled < frames; frames_filled++ )
					    {
						if( ptr_h >= loaded ) break;
						int16_t v = chan_pcmbuf[ ( ( ptr_h * 2 ) & ( PCMBUF_SAMPLES - 1 ) ) + ch ];
						PS_STYPE2 s;
						PS_INT16_TO_STYPE( s, v );
#ifdef PS_STYPE_FLOATINGPOINT
						if( retval == 0 ) *out = s * vol; else *out += s * vol;
#else
						s *= vol;
						s /= 256;
						if( retval == 0 ) *out = s; else *out += s;
#endif
						out++;
						PSYNTH_FP64_ADD( ptr_h, ptr_l, delta_h, delta_l );
					    }
					}
				    }
				}
			    }
			    else 
			    {
				if( retval == 0 )
				{
				    for( frames_filled = 0; frames_filled < frames; frames_filled++ )
				    {
					*out = 0;
					out++;
				    }
				}
			    }
			} 
			if( outputs_num > 0 )
			{
			    chan->ptr_h = ptr_h;
			    chan->ptr_l = ptr_l;
			}
			offset += frames_filled;
			frames -= frames_filled;
		    } 
		    retval = 1;
		} 
	    }
	    break;
	case PS_CMD_NOTE_ON:
	    {
		int c;
		if( !data->src && !data->src_file ) break;
		if( data->no_active_channels == 0 )
		{
		    for( c = 0; c < MAX_CHANNELS; c++ )
		    {
			if( data->channels[ c ].id == event->id && !data->ctl_ignore_noteoff )
			{
			    if( data->channels[ c ].playing )
			    {
				data->channels[ c ].playing = 0;
				data->channels[ c ].id = ~0;
			    }
			    else 
			    {
				data->channels[ c ].id = ~0;
			    }
			    break;
			}
		    }
		    for( c = 0; c < data->ctl_channels; c++ )
		    {
			if( data->channels[ data->search_ptr ].playing == 0 ) break;
			data->search_ptr++;
			if( data->search_ptr >= data->ctl_channels ) data->search_ptr = 0;
		    }
		    if( c == data->ctl_channels )
		    {
			data->search_ptr++;
			if( data->search_ptr >= data->ctl_channels ) data->search_ptr = 0;
		    }
		}
		else 
		{
		    data->search_ptr = 0;
		}
		c = data->search_ptr;
		data->cur_chan = c;
		gen_channel* chan = &data->channels[ c ];
		chan->src_offset = 0;
		int rv = 0;
		if( chan->vf_open == 0 )
		{
		    rv = tremor_ov_open_callbacks( (void*)data, &chan->vf, 0, 0, data->vc );
		}
		else 
		{
		    tremor_ov_time_seek( &chan->vf, 0 );
		}
		if( rv == 0 )
		{
		    chan->playing = 1;
		    chan->vel = event->note.velocity;
		    chan->id = event->id;
		    uint delta_h, delta_l;
		    int freq;
		    int pitch = event->note.pitch / 4 - data->ctl_fine / 2 - ( data->ctl_rel - 128 ) * 64;
		    PSYNTH_GET_FREQ( data->linear_freq_tab, freq, pitch );
		    PSYNTH_GET_DELTA( pnet->sampling_freq, freq, delta_h, delta_l );
		    chan->delta_h = delta_h;
		    chan->delta_l = delta_l;
		    chan->ptr_h = 0;
		    chan->ptr_l = 0;
		    if( chan->vf_open == 0 )
		    {
			chan->vi = tremor_ov_info( &chan->vf, -1 );
		    }
		    chan->loaded = 0;
		    chan->vf_open = 1;
		    data->no_active_channels = 0;
		}
		retval = 1;
	    }
	    break;
    	case PS_CMD_SET_FREQ:
	    if( data->no_active_channels ) break;
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
		if( data->channels[ c ].id == event->id )
		{
		    uint delta_h, delta_l;
		    int freq;
		    int pitch = event->note.pitch / 4 - data->ctl_fine / 2 - ( data->ctl_rel - 128 ) * 64;
		    PSYNTH_GET_FREQ( data->linear_freq_tab, freq, pitch );
		    PSYNTH_GET_DELTA( pnet->sampling_freq, freq, delta_h, delta_l );
		    data->channels[ c ].delta_h = delta_h;
		    data->channels[ c ].delta_l = delta_l;
		    data->channels[ c ].vel = event->note.velocity;
		    break;
		}
	    }
	    retval = 1;
	    break;
    	case PS_CMD_SET_VELOCITY:
	    if( data->no_active_channels ) break;
	    for( int c = 0; c < data->ctl_channels; c++ )
	    {
		if( data->channels[ c ].id == event->id )
		{
		    data->channels[ c ].vel = event->note.velocity;
		    break;
		}
	    }
	    retval = 1;
	    break;
        case PS_CMD_SET_SAMPLE_OFFSET:
        case PS_CMD_SET_SAMPLE_OFFSET2:
            if( data->no_active_channels ) break;
            for( int c = 0; c < data->ctl_channels; c++ )
            {
                if( data->channels[ c ].id == event->id )
                {
            	    gen_channel* chan = &data->channels[ c ];
            	    if( chan->vf_open == 0 ) break;
                    uint64_t new_offset;
                    if( event->command == PS_CMD_SET_SAMPLE_OFFSET )
                    {
                        new_offset = event->sample_offset.sample_offset;
                    }
                    else
                    {
                        uint64_t v = data->src_pcm_total * event->sample_offset.sample_offset;
                        v /= 0x8000;
                        new_offset = v;
                    }
                    if( new_offset >= data->src_pcm_total )
                        new_offset = data->src_pcm_total - 1;
            	    tremor_ov_pcm_seek( &chan->vf, new_offset );
                    retval = 1;
                    break;
                }
            }
            break;
	case PS_CMD_NOTE_OFF:
	    if( data->no_active_channels || data->ctl_ignore_noteoff ) break;
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		if( data->channels[ c ].id == event->id )
		{
		    if( data->channels[ c ].playing )
		    {
			data->channels[ c ].playing = 0;
		    }
		    break;
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	    break;
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    switch( event->controller.ctl_num )
	    {
		case 0x98:
		    data->pause = event->controller.ctl_val;
		    retval = 1;
		break;
	    }
	    break;
	case PS_CMD_CLEAN:
	case PS_CMD_ALL_NOTES_OFF:
	    for( int c = 0; c <= MAX_CHANNELS; c++ )
	    {
		if( data->channels[ c ].playing )
		{
		    data->channels[ c ].playing = 0;
		}
		data->channels[ c ].id = ~0;
	    }
	    data->no_active_channels = 1;
	    retval = 1;
	    break;
	case PS_CMD_CLOSE:
	    for( int c = 0; c <= MAX_CHANNELS; c++ )
	    {
		if( data->channels[ c ].vf_open )
		{
		    data->cur_chan = c;
		    tremor_ov_clear( &data->channels[ c ].vf );
		    data->channels[ c ].vf_open = 0;
		}
	    }
#ifdef SUNVOX_GUI
	    if( mod->visual && data->wm )
	    {
		remove_window( mod->visual, data->wm );
		mod->visual = 0;
	    }
#endif
	    smem_free( data->src_file );
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}

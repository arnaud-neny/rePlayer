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

#include "psynth_net.h"
#include "sunvox_engine.h"
#define MODULE_DATA	psynth_pitch_detector_data
#define MODULE_HANDLER	psynth_pitch_detector
#define MODULE_INPUTS	1
#define MODULE_OUTPUTS	0
static int g_rates[ 3 ] = { 12000, 24000, 44100 };
#define HIST_SIZE 	16
#define FILTERS 	4
#define FILTER_STATE_VARS 2
struct estimate
{
    float T;
    float v; 
    uint frame; 
};
struct MODULE_DATA
{
    PS_CTYPE		ctl_alg;
    PS_CTYPE		ctl_threshold;
    PS_CTYPE		ctl_vel_gain;
    PS_CTYPE		ctl_microtones;
    PS_CTYPE		ctl_detector_finetune;
    PS_CTYPE		ctl_lp_freq;
    PS_CTYPE		ctl_lp_rolloff;
    PS_CTYPE		ctl_srate;
    PS_CTYPE		ctl_buf_size;
    PS_CTYPE		ctl_buf_overlap;
    PS_CTYPE		ctl_abs_threshold;
    PS_CTYPE		ctl_rec;
    PS_STYPE		prev_smp;
    float		period_add;
    int 		period; 
    PS_STYPE2		period_amp;
    int			period_cnt;
    int			max_period;
    int			srate;
    int			buf_size; 
    int			buf_size_scaled; 
    int			buf_hop; 
    float*		buf;
    int			buf_ptr;
    float*		energy_terms;
    float*		fft_i1;
    float*		fft_i2;
    float*		fft_r1; 
    float*		fft_r2;
    float*		fft_win;
    int			fft_win_type; 
    estimate		hist[ HIST_SIZE ]; 
    int			hist_ptr;
    PS_STYPE2		lp_state[ FILTER_STATE_VARS * FILTERS ]; 
    psynth_event	out_evt;
    bool                empty; 
    bool		freq_detected;
    bool		playing;
    int			playing_frame_counter1;
    int			playing_frame_counter2;
    int			redraw_period1; 
    int			redraw_period2; 
    int			pitch;
    int			last_pitch;
    float		freq;
    float		last_freq;
    float		freq_min;
    float		freq_max;
    float		last_freq_min;
    float		last_freq_max;
    int			vel;
    uint		frame_cnt;
    char		str_note[ 8 ];
    char		str_freq[ 16 ];
    char		str_mod_name[ 32 ]; 
    psynth_resampler*   resamp; 
#ifdef SUNVOX_GUI
    int			rec_track;
    int			rec_session_id;
    window_manager* 	wm;
#endif
};
enum {
    cmd_note_on = 0,
    cmd_note_off,
    cmd_note_change,
    cmd_vel_change
};
#ifdef SUNVOX_GUI
#include "../../sunvox/main/sunvox_gui.h"
struct pitch_detector_visual_data
{
    MODULE_DATA* 	module_data;
    int         	mod_num;
    psynth_net* 	pnet;
    WINDOWPTR   	win;
    int			min_ysize;
    char		ts[ 512 ];
};
int pitch_detector_visual_handler( sundog_event* evt, window_manager* wm ) 
{
    int retval = 0;
    WINDOWPTR win = evt->win;
    pitch_detector_visual_data* data = (pitch_detector_visual_data*)win->data;
    switch( evt->type )
    {
        case EVT_GETDATASIZE:
            return sizeof( pitch_detector_visual_data );
            break;
        case EVT_AFTERCREATE:
    	    {
        	data->win = win;
        	data->pnet = (psynth_net*)win->host;
        	data->module_data = NULL;
        	data->mod_num = -1;
        	data->min_ysize = font_char_y_size( win->font, wm ) * 6;
    	    }
            retval = 1;
            break;
        case EVT_DRAW:
    	    {
    		MODULE_DATA* data2 = data->module_data;
		psynth_module* mod = &data->pnet->mods[ data->mod_num ];
    		wbd_lock( win );
                draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
                wm->cur_font_color = wm->color2;
                int y = 0;
                int line_ysize = char_y_size( wm );
                char ts[ 16 ];
                char ts2[ 16 ];
            	float_to_string( (float)data2->last_pitch / 256, ts, 2 );
            	sprintf( data->ts, "%s = %s (%s)", 
            	    sv_get_string( STR_SV_NOTE ),
            	    data2->str_note,
            	    ts );
                draw_string( data->ts, 0, y, wm );
                y += line_ysize;
		float freq_offset = (float)( data2->ctl_detector_finetune - 256 ) / 256;
		float freq, freq2;
            	float n = (float)data2->last_pitch / 256.0f; 
            	float_to_string( data2->last_freq, ts, 2 );
            	sprintf( data->ts, "%s = %s %s", ps_get_string( STR_PS_FREQUENCY ), ts, ps_get_string( STR_PS_HZ ) );
            	draw_string( data->ts, 0, y, wm );
                y += line_ysize;
		if( data2->last_freq_min && data2->last_freq_max )
		{
            	    float_to_string( data2->last_freq_min, ts, 2 );
            	    float_to_string( data2->last_freq_max, ts2, 2 );
            	    sprintf( data->ts, "%s: %s ... %s %s", ps_get_string( STR_PS_RANGE ), ts, ts2, ps_get_string( STR_PS_HZ ) );
        	    draw_string( data->ts, 0, y, wm );
            	    y += line_ysize;
        	}
        	freq = powf( 2.0f, ( n + freq_offset ) / 12.0f ) * (float)PS_NOTE0_FREQ;
                sprintf( data->ts, "%s = %.02f %s", ps_get_string( STR_PS_OUT_NOTE_FREQUENCY ), freq, ps_get_string( STR_PS_HZ ) );
            	draw_string( data->ts, 0, y, wm );
                y += line_ysize;
                wm->cur_font_color = blend( wm->color2, win->color, 64 );
                sprintf( data->ts, "%s: (%s)", ps_get_string( STR_PS_DETECTOR_BASE_FREQS ), ps_get_string( STR_PS_HZ ) );
            	draw_string( data->ts, 0, y, wm );
                y += line_ysize;
                freq = pow( 2, ( 0 + freq_offset ) / 12 ) * PS_NOTE0_FREQ;
                freq2 = pow( 2, ( 57 + freq_offset ) / 12 ) * PS_NOTE0_FREQ;
                sprintf( data->ts, "C0 = %.02f; A4 = %.02f;", freq, freq2 );
            	draw_string( data->ts, 0, y, wm );
                y += line_ysize;
                wbd_draw( wm );
                wbd_unlock( wm );
    	    }
    	    retval = 1;
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
static void pitch_detector_send( int mod_num, psynth_net* pnet, int offset, int cmd )
{
    psynth_module* mod = &pnet->mods[ mod_num ];
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    psynth_event* evt = &data->out_evt;
    evt->offset = offset;
    evt->id = ( SUNVOX_VIRTUAL_PATTERN_PITCH_DETECTOR << 16 ) | mod_num;
    evt->note.velocity = data->vel;
    switch( cmd )
    {
	case cmd_note_on:
	    data->playing = true;
	    evt->command = PS_CMD_NOTE_ON;
	    break;
	case cmd_note_off:
	    data->playing = false;
	    evt->command = PS_CMD_NOTE_OFF;
	    break;
	case cmd_note_change:
	    evt->command = PS_CMD_SET_FREQ;
	    break;
	case cmd_vel_change:
	    evt->command = PS_CMD_SET_VELOCITY;
	    evt->note.velocity = data->vel;
	    break;
    }
    if( cmd != cmd_vel_change )
    {
        psynth_multisend_pitch( mod, evt, pnet, PS_NOTE0_PITCH - data->pitch );
    }
    else
    {
        psynth_multisend( mod, evt, pnet );
    }
#ifdef SUNVOX_GUI
    sunvox_engine* sv = (sunvox_engine*)pnet->host;
    if( data->ctl_rec && sv->recording )
    {
        stime_ticks_t t = pnet->out_time + ( offset * stime_ticks_per_second() ) / pnet->sampling_freq;
        int line = sunvox_frames_get_value( SUNVOX_VF_CHAN_LINENUM, t, sv );
	if( pnet->th_num > 1 ) smutex_lock( &sv->rec_mutex );
	for( int i = 0, mt = 0; i < mod->output_links_num; i++ )
	{
    	    int l = mod->output_links[ i ];
    	    if( !psynth_get_module( l, pnet ) ) continue;
    	    int type = REC_PAT_PITCH_DETECTOR;
    	    if( data->rec_session_id != sv->rec_session_id )
    	    {
    		data->rec_session_id = sv->rec_session_id;
    		data->rec_track = sv->rec_tracks[ type ];
    		sv->rec_tracks[ type ]++;
    	    }
    	    else
    	    {
    		if( data->rec_track + mt >= sv->rec_tracks[ type ] )
    		    sv->rec_tracks[ type ] = data->rec_track + mt + 1;
    	    }
    	    if( sunvox_record_is_ready_for_event( sv ) )
    	    {
    		sunvox_record_write_time( type, line, 0, sv );
    		if( cmd == cmd_note_off )
    		{
    		    sunvox_record_write_byte( rec_evt_noteoff + ( type << REC_EVT_PAT_OFFSET ), sv );
            	    sunvox_record_write_int( data->rec_track + mt, sv );
    		}
    		else
    		{
    		    int e = rec_evt_pitchslide;
    		    if( cmd == cmd_note_on ) e = rec_evt_pitchon;
    		    sunvox_record_write_byte( e + ( type << REC_EVT_PAT_OFFSET ), sv );
    		    sunvox_record_write_int( PS_NOTE0_PITCH - data->pitch, sv );
            	    sunvox_record_write_byte( data->vel / 2, sv );
            	    sunvox_record_write_int( l, sv );
            	    sunvox_record_write_int( data->rec_track + mt, sv );
            	}
    	    }
    	    mt++;
	}
	if( pnet->th_num > 1 ) smutex_unlock( &sv->rec_mutex );
    }
#endif
}
static void pitch_detector_reinit_buffers( psynth_module* mod, int mod_num )
{
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    psynth_net* pnet = mod->pnet;
    data->srate = g_rates[ data->ctl_srate ];
    if( !data->resamp )
    {
        data->resamp = psynth_resampler_new( pnet, mod_num, pnet->sampling_freq, data->srate, 0, PSYNTH_RESAMP_FLAG_MODE2 );
        psynth_resampler_input_buf( data->resamp, 0 );
    }
    else
    {
	if( data->resamp->out_smprate != data->srate )
	{
	    psynth_resampler_change( data->resamp, pnet->sampling_freq, data->srate, 0, PSYNTH_RESAMP_FLAG_MODE2 );
    	    psynth_resampler_input_buf( data->resamp, 0 );
	}
    }
    int fft_win_type = 1; 
    if( data->ctl_alg == 3 ) fft_win_type = 2; 
    int buf_size = 256 << data->ctl_buf_size;
    if( data->srate <= 24000 ) buf_size /= 2;
    if( data->srate <= 12000 ) buf_size /= 2;
    int buf_hop = buf_size * ( 100 - data->ctl_buf_overlap ) / 100;
    if( buf_hop < 8 ) buf_hop = 8;
    data->buf_size_scaled = buf_size * data->resamp->ratio_fp / 65536;
    data->buf_hop = buf_hop;
    size_t new_size = buf_size * sizeof( float );
    size_t old_size = smem_get_size( data->buf );
    if( new_size > old_size )
    {
	data->buf = (float*)smem_resize( data->buf, new_size );
	data->energy_terms = (float*)smem_resize( data->energy_terms, new_size / 2 );
	data->fft_i1 = (float*)smem_resize( data->fft_i1, new_size );
	data->fft_r1 = (float*)smem_resize( data->fft_r1, new_size );
	data->fft_i2 = (float*)smem_resize( data->fft_i2, new_size );
	data->fft_r2 = (float*)smem_resize( data->fft_r2, new_size );
	data->fft_win = (float*)smem_resize( data->fft_win, new_size );
    }
    if( data->buf_size != buf_size || data->fft_win_type != fft_win_type )
    {
	data->buf_ptr = 0;
	data->buf_size = buf_size;
	data->fft_win_type = fft_win_type;
	if( fft_win_type == 1 )
	{
	    for( int i = 0; i < buf_size; i++ ) data->fft_win[ i ] = sin( M_PI * i / (buf_size-1) );
	}
	else
	{
	    float sigma = 1.0f / (8.0f/2);
	    float n1 = ( buf_size - 1 ) / 2.0f;
	    for( int i = 0; i < buf_size; i++ )
	    {
		float n2 = ( (float)i - n1 ) / ( sigma * n1 );
		data->fft_win[ i ] = exp( -0.5f * n2 * n2 );
	    }
	}
    }
}
static float parabolic_interpolation( float v0, float v1, float v2, int t )
{
    float correction = ( v2 - v0 ) / ( 2 * ( 2 * v1 - v2 - v0 ) );
    return t + correction;
}
static float gaussian_interpolation( float v0, float v1, float v2, int t )
{
    float correction = log( v2 / v0 ) / ( 2 * log( ( v1 * v1 ) / ( v2 * v0 ) ) );
    return t + correction;
}
static float get_T( MODULE_DATA* data, float* v, int t )
{
    int x0 = t - 1;
    int x1 = t;
    int x2 = t + 1;
    if( x0 < 0 ) x0 = 0;
    if( x2 >= data->buf_size / 2 ) x2 = x1;
    return parabolic_interpolation( v[ x0 ], v[ x1 ], v[ x2 ], t );
}
static float get_fft_bin( MODULE_DATA* data, float* v, int b )
{
    int x0 = b - 1;
    int x1 = b;
    int x2 = b + 1;
    return gaussian_interpolation( v[ x0 ], v[ x1 ], v[ x2 ], b );
}
static int get_pitch( psynth_module* mod, float freq )
{
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    int pitch = LOG2( freq / PS_NOTE0_FREQ ) * 256 * 12 - ( data->ctl_detector_finetune - 256 );
    if( !data->ctl_microtones )
        pitch = ( ( pitch + 128 ) / 256 ) * 256;
    pitch += mod->relative_note * 256 + mod->finetune;
    return pitch;
}
#ifdef OUTPUT_TO_FILE
static sfs_file g_fout = 0;
#endif
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
	    retval = (PS_RETTYPE)"Pitch Detector";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
			retval = (PS_RETTYPE)
"Pitch Detector пытается определить высоту тона входящего звукового сигнала. Частота и нота выводятся на экран. Ноты посылаются на выход модуля - "
"синтезаторы подключенные к выходу Pitch Detector будут играть в унисон с исходным звуком.";
                        break;
                    }
		    retval = (PS_RETTYPE)
"Pitch Detector tries to detect the pitch of the incoming audio signal. The frequency and note will be displayed. Notes will be sent to the module output - "
"synths connected to the Pitch Detector output will play in unison with the original sound.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FFC000";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS:
	    retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_NO_SCOPE_BUF | PSYNTH_FLAG_OUTPUT_IS_EMPTY | PSYNTH_FLAG_GET_STOP_COMMANDS;
	    break;
	case PS_CMD_GET_FLAGS2:
	    retval = PSYNTH_FLAG2_NOTE_SENDER | PSYNTH_FLAG2_GET_MUTED_COMMANDS;
	    break;
	case PS_CMD_INIT:
	    {
        	psynth_resize_ctls_storage( mod_num, 12, pnet );
        	int ctl;
#ifdef PS_STYPE_FLOATINGPOINT
    #define DEF_ALG 1
#else
    #define DEF_ALG 0
#endif
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_ALGORITHM ), ps_get_string( STR_PS_PD_ALGS ), 0, 3, DEF_ALG, 1, &data->ctl_alg, DEF_ALG, 0, pnet ); 
		ctl = psynth_register_ctl( mod_num, ps_get_string( STR_PS_THRESHOLD ), "", 0, 10000, 80, 0, &data->ctl_threshold, -1, 0, pnet ); 
        	    psynth_set_ctl_flags( mod_num, ctl, PSYNTH_CTL_FLAG_EXP3, pnet );
		ctl = psynth_register_ctl( mod_num, ps_get_string( STR_PS_GAIN ), "", 0, 256, 0, 0, &data->ctl_vel_gain, 0, 0, pnet );
        	    psynth_set_ctl_flags( mod_num, ctl, PSYNTH_CTL_FLAG_EXP3, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_MICROTONES ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 1, 1, &data->ctl_microtones, -1, 0, pnet );
		ctl = psynth_register_ctl( mod_num, ps_get_string( STR_PS_DETECTOR_FINETUNE ), "", 0, 512, 256, 0, &data->ctl_detector_finetune, 256, 0, pnet );
            	    psynth_set_ctl_show_offset( mod_num, ctl, -256, pnet );
                ctl = psynth_register_ctl( mod_num, ps_get_string( STR_PS_LP_FILTER_FREQ ), ps_get_string( STR_PS_HZ ), 0, 4000, 1000, 0, &data->ctl_lp_freq, 1000, 1, pnet );
            	    psynth_set_ctl_flags( mod_num, ctl, PSYNTH_CTL_FLAG_EXP3, pnet );
                psynth_register_ctl( mod_num, ps_get_string( STR_PS_LP_FILTER_ROLLOFF ), ps_get_string( STR_PS_FILTER_ROLLOFF_VALS ), 0, 3, 0, 1, &data->ctl_lp_rolloff, -1, 1, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_PD_ALG1_SAMPLE_RATE ), "12000;24000;44100", 0, 2, 0, 1, &data->ctl_srate, 0, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_PD_ALG1_BUF_SIZE ), "5;10;21;42;85", 0, 4, 2, 1, &data->ctl_buf_size, 2, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_PD_ALG1_BUF_OVERLAP ), "%", 0, 100, 50, 0, &data->ctl_buf_overlap, 50, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_PD_ALG1_ABS_THRESHOLD ), "", 0, 100, 10, 0, &data->ctl_abs_threshold, 10, 2, pnet );
		psynth_register_ctl( mod_num, ps_get_string( STR_PS_RECORD_NOTES ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_rec, -1, 3, pnet ); 
                smem_clear( &data->out_evt, sizeof( data->out_evt ) );
                data->max_period = pnet->sampling_freq / 25;
		data->redraw_period1 = pnet->sampling_freq / 20;
		data->redraw_period2 = pnet->sampling_freq / 1;
		data->freq_max = 0;
		data->freq_min = 1000000;
		data->empty = true;
                psynth_get_temp_buf( mod_num, pnet, 0 ); 
                if( pnet->sampling_freq < 44100 )
                {
            	    slog( "Pitch Detector: WRONG SAMPLING RATE!\n" );
                }
#ifdef OUTPUT_TO_FILE
                g_fout = sfs_open( "OUT.RAW", "wb" );
#endif
#ifdef SUNVOX_GUI
		data->rec_session_id = -1;
        	data->wm = NULL;
                sunvox_engine* sv = (sunvox_engine*)pnet->host;
                if( sv && sv->win )
                {
            	    mod->name2 = data->str_mod_name;
                    window_manager* wm = sv->win->wm;
                    data->wm = wm;
            	    mod->visual = new_window( "Pitch Detector GUI", 0, 0, 1, 1, wm->button_color, 0, pnet, pitch_detector_visual_handler, wm );
            	    pitch_detector_visual_data* data1 = (pitch_detector_visual_data*)mod->visual->data;
            	    mod->visual_min_ysize = data1->min_ysize;
            	    data1->module_data = data;
            	    data1->mod_num = mod_num;
            	    data1->pnet = pnet;
            	}
#endif
	    }
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    pitch_detector_reinit_buffers( mod, mod_num );
            retval = 1;
            break;
        case PS_CMD_RENDER_REPLACE:
    	{
	    if( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) break;
            PS_STYPE** inputs = mod->channels_in;
            int offset = mod->offset;
            int frames = mod->frames;
	    data->frame_cnt += frames;
	    data->playing_frame_counter1 += frames;
	    data->playing_frame_counter2 += frames;
            bool input_signal = false;
            for( int ch = 0; ch < MODULE_INPUTS; ch++ )
            {
                if( mod->in_empty[ ch ] < offset + frames )
                {
                    input_signal = true;
                    break;
        	}
            }
            if( !input_signal )
            {
        	if( !data->empty )
        	{
        	    if( data->playing )
        	    {
        		int cmd = cmd_note_off;
			pitch_detector_send( mod_num, pnet, mod->offset, cmd );
		    }
		    data->buf_ptr = 0;
		    for( int i = 0; i < HIST_SIZE; i++ ) data->hist[ i ].T = 0;
		    data->prev_smp = 0;
		    data->period = 0;
		    data->period_amp = 0;
		    for( int i = 0; i < FILTER_STATE_VARS * FILTERS; i++ ) data->lp_state[ i ] = 0;
        	    if( data->resamp->state ) psynth_resampler_reset( data->resamp );
        	    data->freq_detected = 0;
        	    data->empty = true;
        	}
		break;
            }
            data->empty = false;
            PS_STYPE* in = inputs[ 0 ] + offset;
            int in_frames = frames;
    	    PS_STYPE* temp_buf = psynth_get_temp_buf( mod_num, pnet, 0 );
    	    PS_STYPE* resamp_buf = NULL;
    	    PS_STYPE* buf = temp_buf; 
    	    if( data->ctl_alg )
    	    {
    		if( data->resamp->out_smprate != pnet->sampling_freq )
    		{
        	    resamp_buf = psynth_resampler_input_buf( data->resamp, 0 );
    	    	    buf = resamp_buf + psynth_resampler_input_buf_offset( data->resamp );
    	    	}
	    }
            if( data->ctl_lp_freq )
            {
        	PS_STYPE2 c, f, q;
#ifdef PS_STYPE_FLOATINGPOINT
                q = (PS_STYPE2)1536 / 1024; 
        	f = 2 * sin( M_PI * data->ctl_lp_freq / pnet->sampling_freq );
#else
                q = 1536; 
        	c = data->ctl_lp_freq * 32768 / pnet->sampling_freq; 
                f = 6433 * c / 32768; 
		if( f < 1 ) f = 1;
#endif
                for( int fnum = 0; fnum < data->ctl_lp_rolloff + 1; fnum++ )
                {
            	    PS_STYPE* fout = buf;
            	    PS_STYPE* fin = in; if( fnum > 0 ) fin = fout;
            	    PS_STYPE2* fstate = &data->lp_state[ fnum * FILTER_STATE_VARS ];
            	    for( int i = 0; i < frames; i++ )
                    {
                        PS_STYPE2 v = fin[ i ];
                        psynth_denorm_add_white_noise( v );
#ifdef PS_STYPE_FLOATINGPOINT
                        PS_STYPE2 low = fstate[ 1 ] + f * fstate[ 0 ];
                        PS_STYPE2 high = v - low - q * fstate[ 0 ];
                        PS_STYPE2 band = f * high + fstate[ 0 ];
                        v = low;
#else
                        v *= 32;
                        PS_STYPE2 low = fstate[ 1 ] + ( f * fstate[ 0 ] ) / 1024;
                        PS_STYPE2 high = v - low - ( q * fstate[ 0 ] ) / 1024;
                        PS_STYPE2 band = ( f * high ) / 1024 + fstate[ 0 ];
                        v = low / 32;
#endif
                        fstate[ 0 ] = band;
                        fstate[ 1 ] = low;
                        fout[ i ] = v;
                    }
            	}
        	in = buf;
            }
            else
            {
        	if( resamp_buf )
        	{
        	    for( int i = 0; i < frames; i++ ) buf[ i ] = in[ i ];
        	}
            }
	    if( resamp_buf )
	    {
        	psynth_resampler_begin( data->resamp, frames, 0 );
		psynth_resampler_end( data->resamp, input_signal?1:2, &resamp_buf, &temp_buf, 1, 0 );
		in = temp_buf;
		in_frames = data->resamp->output_frames;
	    }
#if OUTPUT_TO_FILE == 1
	    sfs_write( in, sizeof( PS_STYPE ), in_frames, g_fout );
#endif
            switch( data->ctl_alg )
            {
        	case 0:
        	{
        	    PS_STYPE2 amp_threshold = data->ctl_threshold * PS_STYPE_ONE / 10000;
        	    for( int i = 0; i < in_frames; i++ )
        	    {
        		PS_STYPE v = in[ i ];
        		data->period++;
        		if( data->period > data->max_period )
        		{
        		    data->period = 0;
			    data->period_cnt = 0;
        		    if( data->playing )
				pitch_detector_send( mod_num, pnet, mod->offset + i, cmd_note_off );
        		}
        		PS_STYPE2 amp = PS_STYPE_ABS( v ); if( amp > data->period_amp ) data->period_amp = amp;
        		if( data->prev_smp < 0 && v >= 0 )
        		{
        		    if( data->period_amp < amp_threshold )
        		    {
				data->period_cnt = 0;
        			if( data->playing )
				    pitch_detector_send( mod_num, pnet, mod->offset + i, cmd_note_off );
        		    }
        		    else
        		    {
        			float x = (float)data->prev_smp / ( data->prev_smp - v ); 
        			data->freq = (float)pnet->sampling_freq / ( (float)data->period + data->period_add - 1 + x );
				data->freq_detected = 1;
        			data->period_add = 1 - x;
        			while( data->period_cnt >= 1 ) 
        			{
        			    int prev_pitch = data->pitch;
        			    int prev_vel = data->vel;
        			    data->pitch = get_pitch( mod, data->freq );
        			    data->vel = data->period_amp * 256 / PS_STYPE_ONE * ( data->ctl_vel_gain + 1 );
        			    if( data->vel > 256 ) data->vel = 256;
        			    int cmd = cmd_note_on;
        			    if( data->playing )
        			    {
        				cmd = cmd_note_change;
        				if( ( data->pitch >> 2 ) == ( prev_pitch >> 2 ) )
        				{
        				    if( prev_vel == data->vel ) break;
        				    cmd = cmd_vel_change;
        				}
        			    }
				    pitch_detector_send( mod_num, pnet, mod->offset + i, cmd );
				    break;
				}
				data->period_cnt++;
			    }
        		    data->period = 0;
        		    data->period_amp = 0;
        		}
        		data->prev_smp = v;
		    }
        	    break;
        	}
        	case 1: case 2: case 3:
        	{
        	    int max_freq = data->ctl_alg == 1 ? 2000 : 1000; 
        	    if( pnet->base_host_version >= 0x02010200 )
        	    {
        		if( data->ctl_lp_freq == 0 )
        		{
        		    if( data->srate >= 44100 )
        		    {
        			max_freq *= 2;
        		    }
        		}
		    }
        	    int min_T = data->srate / max_freq;
        	    if( min_T < 2 ) min_T = 2;
        	    for( int i = 0; i < in_frames; i++ )
        	    {
        		PS_STYPE_TO_FLOAT( data->buf[ data->buf_ptr ], in[ i ] );
        		data->buf_ptr++;
        		if( data->buf_ptr >= data->buf_size )
        		{
        		    float amp = 0;
        		    if( data->ctl_alg == 1 )
        		    {
        			data->energy_terms[ 0 ] = 0;
        			for( int t = 0; t < data->buf_size / 2; t++ ) 
        			    data->energy_terms[ 0 ] += data->buf[ t ] * data->buf[ t ];
        			float amp2 = 0;
        			for( int t = data->buf_size / 2; t < data->buf_size; t += 2 )
        			    amp2 += data->buf[ t ] * data->buf[ t ];
        			amp = 
        			    ( sqrt( data->energy_terms[ 0 ] / ( data->buf_size / 2 ) ) + sqrt( amp2 / ( data->buf_size / 4 ) ) ) / 2;
        		    }
        		    if( data->ctl_alg >= 2 )
        		    {
        			for( int t = 0; t < data->buf_size; t += 2 )
        			    amp += data->buf[ t ] * data->buf[ t ];
        			amp = sqrt( amp / ( data->buf_size / 2 ) ); 
        		    }
        		    float amp_threshold = (float)data->ctl_threshold / 10000.0F;
        		    if( amp > 1.0F / ( 32768.0F * 2 ) || data->playing )
        		    {
        			int T = min_T; 
        			if( data->ctl_alg == 1 )
        			{
        			    for( int t = 1; t < data->buf_size / 2; t++ ) 
        				data->energy_terms[ t ] =
        				    data->energy_terms[ t - 1 ]
        				    - data->buf[ t - 1 ] * data->buf[ t - 1 ]
        				    + data->buf[ t + data->buf_size / 2 ] * data->buf[ t + data->buf_size / 2 ];
        			    memset( data->fft_i1, 0, data->buf_size * sizeof( float ) );
        			    for( int t = 0; t < data->buf_size; t++ ) data->fft_r1[ t ] = data->buf[ t ];
        			    fft( 0, data->fft_i1, data->fft_r1, data->buf_size );
        			    memset( data->fft_i2, 0, data->buf_size * sizeof( float ) );
        			    for( int t = 0; t <= data->buf_size / 2; t++ ) data->fft_r2[ t ] = 0;
        			    data->fft_r2[ 0 ] = data->buf[ 0 ]; for( int t = 1; t < data->buf_size / 2; t++ ) data->fft_r2[ data->buf_size - t ] = data->buf[ t ];
        			    fft( 0, data->fft_i2, data->fft_r2, data->buf_size );
        			    for( int t = 0; t < data->buf_size; t++ )
        			    {
        				float r1 = data->fft_r1[ t ];
    					data->fft_r1[ t ] = data->fft_r1[ t ] * data->fft_r2[ t ] - data->fft_i1[ t ] * data->fft_i2[ t ];
    					data->fft_i1[ t ] = data->fft_i1[ t ] * data->fft_r2[ t ] + r1                * data->fft_i2[ t ];
        			    }
        			    fft( FFT_FLAG_INVERSE, data->fft_i1, data->fft_r1, data->buf_size );
        			    for( int t = 0; t < data->buf_size / 2; t++ ) 
        				data->fft_r1[ t ] = data->energy_terms[ 0 ] + data->energy_terms[ t ] - 2 * data->fft_r1[ t ];
        			    float sum = 0;
        			    data->fft_r1[ 0 ] = 1;
        			    for( int t = 1; t < data->buf_size / 2; t++ )
        			    {
        				sum += data->fft_r1[ t ];
        				data->fft_r1[ t ] *= t / sum;
        			    }
        			    float threshold = (float)data->ctl_abs_threshold / 100.0F;
        			    for( ; T < data->buf_size / 2; T++ )
        			    {
        				if( data->fft_r1[ T ] < threshold )
        				{
        				    while( T + 1 < data->buf_size / 2 && data->fft_r1[ T + 1 ] < data->fft_r1[ T ] ) T++;
        				    break;
        				}
        			    }
        			}
        			if( data->ctl_alg == 2 )
        			{
        			    for( int t = 0; t < data->buf_size; t++ ) data->fft_r1[ t ] = data->buf[ t ] * data->fft_win[ t ];
        			    memset( data->fft_i1, 0, data->buf_size * sizeof( float ) );
        			    fft( 0, data->fft_i1, data->fft_r1, data->buf_size );
        			    for( int t = 0; t <= data->buf_size / 2; t++ )
        			    {
        				float rv = data->fft_r1[ t ];
        				float iv = data->fft_i1[ t ];
        				data->fft_r1[ t ] = log( 1 + sqrt( rv * rv + iv * iv ) );
        			    }
        			    for( int t = 0; t < data->buf_size / 2 - 1; t++ )
                            		data->fft_r1[ data->buf_size - 1 - t ] = data->fft_r1[ t + 1 ]; 
        			    memset( data->fft_i1, 0, data->buf_size * sizeof( float ) );
        			    fft( 1, data->fft_i1, data->fft_r1, data->buf_size );
        			    float max = -10000000;
        			    for( int t = min_T; t < data->buf_size / 2; t++ )
        			    {
        				float v = data->fft_r1[ t ];
        				if( v > max ) { T = t; max = v; }
        			    }
        			}
        			if( data->ctl_alg == 3 )
        			{
        			    for( int t = 0; t < data->buf_size; t++ ) data->fft_r1[ t ] = data->buf[ t ] * data->fft_win[ t ];
        			    memset( data->fft_i1, 0, data->buf_size * sizeof( float ) );
        			    fft( 0, data->fft_i1, data->fft_r1, data->buf_size );
        			    for( int b = 0; b <= data->buf_size / 2; b++ )
        			    {
        				float rv = data->fft_r1[ b ];
        				float iv = data->fft_i1[ b ];
        				data->fft_r1[ b ] = sqrt( rv * rv + iv * iv );
        			    }
        			    data->fft_r1[ 0 ] /= 2;
        			    data->fft_r1[ data->buf_size / 2 ] /= 2;
        			    float max = -10000000;
        			    int max_b = 0;
        			    for( int b = 1; b < data->buf_size / 2 - 1; b++ )
        			    {
        				float v = data->fft_r1[ b ];
        				if( v > max ) { max = v; max_b = b; }
        			    }
        			    T = max_b;
        			}
#if OUTPUT_TO_FILE == 2
				float vvvv = -1;
				sfs_write( &vvvv, sizeof( float ), 1, g_fout );
				sfs_write( data->fft_r1, sizeof( float ), data->buf_size / 2, g_fout );
#endif
        			int evt_offset = i * data->resamp->ratio_fp / 65536;
        			if( evt_offset >= frames ) evt_offset = frames - 1;
        			if( data->ctl_alg == 3 )
        			{
        			    if( T )
        			    {
					float b = get_fft_bin( data, data->fft_r1, T );
            				data->freq = b / ( data->buf_size / 2 ) * ( data->srate / 2 );
					data->freq_detected = 1;
    				    }
        			}
        			else
        			{
        			    if( T > min_T && T < data->buf_size / 2 )
        			    {
        				float T2;
        				if( data->ctl_alg == 1 )
        				{
        				    uint frame = data->frame_cnt - frames + evt_offset;
        				    estimate* e = &data->hist[ data->hist_ptr & (HIST_SIZE-1) ];
        				    e->T = get_T( data, data->fft_r1, T );
        				    e->v = data->fft_r1[ T ];
        				    e->frame = frame;
					    for( int h = 1; h < HIST_SIZE; h++ )
					    {
        					estimate* e2 = &data->hist[ ( data->hist_ptr - h ) & (HIST_SIZE-1) ];
        					if( e2->T == 0 || ( frame - e2->frame ) > (uint)(data->buf_size_scaled*0.51F) ) break;
        					if( e2->v < e->v ) e = e2;
					    }
					    data->hist_ptr++;
					    T2 = e->T;
					}
					else
					{
					    T2 = get_T( data, data->fft_r1, T );
					}
					if( T2 )
					{
					    data->freq = (float)data->srate / T2;
					    data->freq_detected = 1;
					}
				    }
				}
        			if( amp < amp_threshold )
        			{
        			    if( data->playing )
					pitch_detector_send( mod_num, pnet, mod->offset + evt_offset, cmd_note_off );
        			}
        			else
        			{
        			    if( data->freq_detected )
        			    {
        				int prev_pitch = data->pitch;
        				int prev_vel = data->vel;
					data->pitch = get_pitch( mod, data->freq );
        				data->vel = amp * 256 * ( data->ctl_vel_gain + 1 );
                            		if( data->vel > 256 ) data->vel = 256;
        				int cmd = cmd_note_on;
        				if( data->playing )
        				{
        				    cmd = cmd_note_change;
        				    if( ( data->pitch >> 2 ) == ( prev_pitch >> 2 ) )
        				    {
        					if( prev_vel == data->vel )
        					    cmd = -1;
        					else
        					    cmd = cmd_vel_change;
        				    }
        				}
					if( cmd != -1 ) pitch_detector_send( mod_num, pnet, mod->offset + evt_offset, cmd );
				    }
        			}
        		    } 
        		    data->buf_ptr = data->buf_size - data->buf_hop;
        		    for( int h = 0; h < data->buf_size - data->buf_hop; h++ )
        		    {
        			data->buf[ h ] = data->buf[ h + data->buf_hop ];
        		    }
        		}
        	    }
        	    break;
        	}
            }
            if( data->playing )
            {
        	if( data->freq < data->freq_min ) data->freq_min = data->freq;
        	if( data->freq > data->freq_max ) data->freq_max = data->freq;
        	int redraw = 0;
		if( data->playing_frame_counter1 > data->redraw_period1 )
		{
		    data->playing_frame_counter1 = 0;
		    redraw |= 1;
		}
		if( data->playing_frame_counter2 > data->redraw_period2 )
		{
		    data->playing_frame_counter2 = 0;
		    redraw |= 2;
		}
        	if( ( ( redraw & 1 ) && data->pitch != data->last_pitch ) ||
        	    ( redraw & 2 ) )
        	{
#ifdef SUNVOX_GUI
            	    int p_offset = 256 * 22 * 12 + 128; 
            	    int p = data->pitch;
            	    float n = (float)p / 256.0f; 
            	    p += p_offset;
            	    if( p >= 0 )
            	    {
            	        data->str_note[ 0 ] = g_n2c[ ( p / 256 ) % 12 ];
            		int_to_string( ( p / 256 ) / 12 - p_offset/256/12, &data->str_note[ 1 ] );
            	    }
            	    float_to_string( data->freq, data->str_freq, 2 );
            	    if( redraw & 2 )
            	    {
            		data->last_freq_max = data->freq_max;
            		data->last_freq_min = data->freq_min;
            		data->freq_max = 0;
            		data->freq_min = 100000;
            	    }
		    data->str_mod_name[ 0 ] = 0;
		    strcat( data->str_mod_name, data->str_note );
		    strcat( data->str_mod_name, " " );
		    strcat( data->str_mod_name, data->str_freq );
            	    COMPILER_MEMORY_BARRIER();
#endif
        	    data->last_pitch = data->pitch;
        	    data->last_freq = data->freq;
        	    mod->draw_request++;
        	}
    	    }
    	    break;
    	}
	case PS_CMD_CLEAN:
	    data->buf_ptr = 0;
	    data->freq_detected = 0;
	    for( int i = 0; i < HIST_SIZE; i++ ) data->hist[ i ].T = 0;
	    data->prev_smp = 0;
	    data->period = 0;
	    data->period_amp = 0;
	    data->playing = 0;
	    data->frame_cnt = 0;
	    data->str_mod_name[ 0 ] = 0;
	    for( int i = 0; i < FILTER_STATE_VARS * FILTERS; i++ ) data->lp_state[ i ] = 0;
	    psynth_resampler_reset( data->resamp );
	    data->empty = true;
	    retval = 1;
	    break;
	case PS_CMD_ALL_NOTES_OFF:
	case PS_CMD_STOP:
	    if( data->playing ) pitch_detector_send( mod_num, pnet, event->offset, cmd_note_off );
	    mod->draw_request++;
	    retval = 1;
	    break;
	case PS_CMD_MUTED:
	    if( data->playing ) pitch_detector_send( mod_num, pnet, event->offset, cmd_note_off );
	    retval = 1;
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
        case PS_CMD_SET_GLOBAL_CONTROLLER:
	    psynth_set_ctl2( mod, event );
            retval = 1;
            switch( event->controller.ctl_num )
            {
        	case 0: case 7: case 8: case 9: 
		    pitch_detector_reinit_buffers( mod, mod_num );
		    break;
            }
            break;
	case PS_CMD_CLOSE:
#ifdef OUTPUT_TO_FILE
            sfs_close( g_fout );
#endif
	    smem_free( data->buf );
	    smem_free( data->energy_terms );
	    smem_free( data->fft_i1 );
	    smem_free( data->fft_i2 );
	    smem_free( data->fft_r1 );
	    smem_free( data->fft_r2 );
	    smem_free( data->fft_win );
	    psynth_resampler_remove( data->resamp );
#ifdef SUNVOX_GUI
	    if( mod->visual && data->wm )
	    {
        	remove_window( mod->visual, data->wm );
        	mod->visual = NULL;
    	    }
#endif
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}

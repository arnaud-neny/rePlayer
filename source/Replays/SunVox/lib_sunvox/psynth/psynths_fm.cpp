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

#include "psynth.h"
#include "psynth_net.h"
#define MODULE_DATA	psynth_fm_data
#define MODULE_HANDLER	psynth_fm
#define MODULE_INPUTS	0
#define MODULE_OUTPUTS	2
#define MAX_CHANNELS	16
#if defined(PS_STYPE_FLOATINGPOINT) && ( CPUMARK >= 10 )
    #define FM_HQ32
    #define FM_TABLE_TYPE	float
#else
    #define FM_TABLE_TYPE	uint16_t
#endif
#define FM_SFREQ	44100
#define FM_ENV_STEP	256
#define FM_TABLE_SIZE	1550
#define FM_TABLE_AMP_SH	15
#define FM_TABLE_AMP	( 1 << FM_TABLE_AMP_SH )
#define FM_SINUS_SH	12
#define FM_SINUS_SIZE	( 1 << FM_SINUS_SH )
#define FM_MOD_DIV	( 32768 / ( FM_SINUS_SIZE * 8 ) )  
#define FM_PI		(float)M_PI
#define FM_2PI		( FM_PI * 2.0f )
struct gen_channel
{
    int	    	playing;
    uint    	id;
    int		vel;
    int    	cptr;
    int    	mptr;
    int	    	c_env_ptr;
    int	    	m_env_ptr;
    uint    	cdelta;
    int	    	sustain;
    int	    	note;
    psynth_renderbuf_pars       renderbuf_pars;
};
enum {
    MODE_HQ = 0,
    MODE_HQ_MONO,
    MODE_LQ,
    MODE_LQ_MONO,
    MODES
};
struct MODULE_DATA
{
    PS_CTYPE   		ctl_cvolume;
    PS_CTYPE   		ctl_mvolume;
    PS_CTYPE   		ctl_pan;
    PS_CTYPE   		ctl_cmul;
    PS_CTYPE   		ctl_mmul;
    PS_CTYPE   		ctl_mselfmod;
    PS_CTYPE   		ctl_ca;
    PS_CTYPE   		ctl_cd;
    PS_CTYPE   		ctl_cs;
    PS_CTYPE   		ctl_cr;
    PS_CTYPE   		ctl_ma;
    PS_CTYPE   		ctl_md;
    PS_CTYPE   		ctl_ms;
    PS_CTYPE   		ctl_mr;
    PS_CTYPE   		ctl_mscaling;
    PS_CTYPE   		ctl_channels;
    PS_CTYPE   		ctl_mode;
    gen_channel		channels[ MAX_CHANNELS ];
    bool    		no_active_channels;
    int	    		search_ptr;
    uint*   		linear_freq_tab;
#ifndef FM_HQ32
    int16_t*  		sin_tab;
#endif
    FM_TABLE_TYPE* 	cvolume_table;
    FM_TABLE_TYPE* 	mvolume_table;
    int	    		cvolume_table_size;
    int	    		mvolume_table_size;
    bool    		tables_render_request;
    psmoother_coefs     smoother_coefs;
#ifndef ONLY44100
    psynth_resampler*   resamp;
#endif
};
static void render_level( FM_TABLE_TYPE* table, int x1, int y1, int x2, int y2 )
{
#ifdef CHECK_TABLES
    if( x1 >= FM_TABLE_SIZE || x2 >= FM_TABLE_SIZE )
    {
	slog( "render_level() error. x1 = %d. x2 = %d.\n", x1, x2 );
    }
#endif
    int size = ( x2 - x1 );
    if( size == 0 ) size = 1;
#ifdef FM_HQ32
    float delta = (float)( y2 - y1 ) / (float)size;
    float cy = (float)y1;
    for( int cx = x1; cx <= x2; cx++ )
    {
	table[ cx ] = cy / (float)FM_TABLE_AMP;
	cy += delta;
    }
#else
    int delta = ( ( y2 - y1 ) << 15 ) / size;
    int cy = y1 << 15;
    for( int cx = x1; cx <= x2; cx++ )
    {
	table[ cx ] = (uint16_t)( cy >> 15 );
	cy += delta;
    }
#endif
}
static void render_volume_tables( MODULE_DATA* data )
{
    int x = data->ctl_ca;
    int y = FM_TABLE_AMP;
    render_level( data->cvolume_table, 0, 0, x, y );
    int x2 = x + data->ctl_cd;
    int y2 = data->ctl_cs * ( FM_TABLE_AMP / 256 );
    render_level( data->cvolume_table, x, y, x2, y2 );
    x = x2 + data->ctl_cr;
    y = 0;
    render_level( data->cvolume_table, x2, y2, x, y );
#ifdef CHECK_TABLES
    if( x + 1 >= FM_TABLE_SIZE )
    {
	slog( "render_volume_tables() error 1.\n" );
    }
#endif
    data->cvolume_table[ x + 1 ] = 0; 
    data->cvolume_table[ x + 2 ] = 0;
    data->cvolume_table_size = x;
    x = data->ctl_ma;
    y = data->ctl_mvolume * ( FM_TABLE_AMP / 256 );
    render_level( data->mvolume_table, 0, 0, x, y );
    x2 = x + data->ctl_md;
    y2 = ( ( data->ctl_mvolume * data->ctl_ms ) / 256 ) * ( FM_TABLE_AMP / 256 );
    render_level( data->mvolume_table, x, y, x2, y2 );
    x = x2 + data->ctl_mr;
    y = 0;
    render_level( data->mvolume_table, x2, y2, x, y );
#ifdef CHECK_TABLES
    if( x + 1 >= FM_TABLE_SIZE )
    {
	slog( "render_volume_tables() error 2.\n" );
    }
#endif
    data->mvolume_table[ x + 1 ] = 0; 
    data->mvolume_table[ x + 2 ] = 0;
    data->mvolume_table_size = x;
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
	    retval = (PS_RETTYPE)"FM";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)
"Синтезатор на основе алгоритма частотной модуляции (FM).\n"
"Каждый голос полифонии состоит из двух операторов с ADSR-огибающими:\n"
"1) Н (Несущий сигнал) - базовая синусоида;\n"
"2) М (Модулирующий сигнал) - синусоида, меняющая частоту первого оператора.\n"
"Звучит лучше на частоте дискретизации 44100Гц.";
                        break;
                    }
		    retval = (PS_RETTYPE)"Frequency Modulation (FM) Synthesizer.\n"
"Each voice of polyphony includes two operators with ADSR envelopes:\n"
"1) C (carrier) - base sine;\n"
"2) M (modulator) - sine wave that changes the frequency of the first operator.\n"
"Sounds better at a sample rate of 44100Hz";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#00E3FF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_GENERATOR; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 17, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_C_VOLUME ), "", 0, 256, 128, 0, &data->ctl_cvolume, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_M_VOLUME ), "", 0, 256, 48, 0, &data->ctl_mvolume, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_PANNING ), "", 0, 256, 128, 0, &data->ctl_pan, 128, 0, pnet );
	    psynth_set_ctl_show_offset( mod_num, 2, -128, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_C_FREQ_RATIO ), "", 0, 16, 1, 0, &data->ctl_cmul, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_M_FREQ_RATIO ), "", 0, 16, 1, 0, &data->ctl_mmul, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_M_SELF_MOD ), "", 0, 256, 0, 0, &data->ctl_mselfmod, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_C_ATTACK ), "", 0, 512, 32, 0, &data->ctl_ca, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_C_DECAY ), "", 0, 512, 32, 0, &data->ctl_cd, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_C_SUSTAIN ), "", 0, 256, 128, 0, &data->ctl_cs, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_C_RELEASE ), "", 0, 512, 64, 0, &data->ctl_cr, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_M_ATTACK ), "", 0, 512, 32, 0, &data->ctl_ma, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_M_DECAY ), "", 0, 512, 32, 0, &data->ctl_md, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_M_SUSTAIN ), "", 0, 256, 128, 0, &data->ctl_ms, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_M_RELEASE ), "", 0, 512, 64, 0, &data->ctl_mr, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_M_SCALING ), "", 0, 4, 0, 0, &data->ctl_mscaling, -1, 2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_POLYPHONY ), ps_get_string( STR_PS_CH ), 1, MAX_CHANNELS, 4, 1, &data->ctl_channels, -1, 3, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_MODE ), "HQ;HQmono;LQ;LQmono", 0, MODES - 1, MODE_HQ_MONO, 1, &data->ctl_mode, -1, 3, pnet );
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].playing = 0;
		data->channels[ c ].cptr = 0;
		data->channels[ c ].mptr = 0;
		data->channels[ c ].c_env_ptr = 0;
		data->channels[ c ].m_env_ptr = 0;
		data->channels[ c ].sustain = 0;
		data->channels[ c ].id = ~0;
	    }
	    data->no_active_channels = 1;
	    data->search_ptr = 0;
	    data->linear_freq_tab = g_linear_freq_tab;
	    psynth_get_temp_buf( mod_num, pnet, 0 ); 
#ifndef ONLY44100
	    data->resamp = NULL;
	    if( pnet->sampling_freq != FM_SFREQ )
	    {
                data->resamp = psynth_resampler_new( pnet, mod_num, FM_SFREQ, pnet->sampling_freq, 0, 0 );
                for( int i = 0; i < MODULE_OUTPUTS; i++ ) psynth_resampler_input_buf( data->resamp, i );
            }
#endif
	    data->cvolume_table = SMEM_ALLOC2( FM_TABLE_TYPE, FM_TABLE_SIZE );
	    data->mvolume_table = SMEM_ALLOC2( FM_TABLE_TYPE, FM_TABLE_SIZE );
	    data->cvolume_table_size = 0;
	    data->mvolume_table_size = 0;
	    data->tables_render_request = 1;
#ifndef FM_HQ32
	    data->sin_tab = (int16_t*)psynth_get_sine_table( 2, true, FM_SINUS_SH, 32767 );
#endif
	    psmoother_init( &data->smoother_coefs, 100, FM_SFREQ );
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].playing = 0;
		data->channels[ c ].id = ~0;
	    }
	    data->no_active_channels = 1;
#ifndef ONLY44100
            psynth_resampler_reset( data->resamp );
#endif
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
		if( data->ctl_mode == MODE_HQ_MONO || data->ctl_mode == MODE_LQ_MONO )
		{
		    if( psynth_get_number_of_outputs( mod ) != 1 )
		    {
			psynth_set_number_of_outputs( 1, mod_num, pnet );
		    }
		}
		else
		{
		    if( psynth_get_number_of_outputs( mod ) != MODULE_OUTPUTS )
		    {
			psynth_set_number_of_outputs( MODULE_OUTPUTS, mod_num, pnet );
		    }
		}
		int outputs_num = psynth_get_number_of_outputs( mod );
		if( data->tables_render_request )
		{
		    render_volume_tables( data );
		    data->tables_render_request = 0;
		}
		PS_STYPE** resamp_outputs = outputs;
                int resamp_offset = offset;
		int resamp_frames = frames;
#ifndef ONLY44100
		PS_STYPE* resamp_bufs[ MODULE_OUTPUTS ];
		PS_STYPE* resamp_outputs2[ MODULE_OUTPUTS ];
                if( data->resamp )
                {
                    for( int ch = 0; ch < outputs_num; ch++ )
                    {
                	resamp_bufs[ ch ] = psynth_resampler_input_buf( data->resamp, ch );
                        resamp_outputs2[ ch ] = resamp_bufs[ ch ] + psynth_resampler_input_buf_offset( data->resamp );
                    }
                    resamp_outputs = &resamp_outputs2[ 0 ];
                    resamp_offset = 0;
                    resamp_frames = psynth_resampler_begin( data->resamp, 0, frames );
                }
#endif
		int mselfmod = data->ctl_mselfmod;
#ifndef FM_HQ32
		int16_t* sin_tab = data->sin_tab;
#endif
		FM_TABLE_TYPE* cvolume_table = data->cvolume_table;
		FM_TABLE_TYPE* mvolume_table = data->mvolume_table;
		int cvolume_table_size = data->cvolume_table_size;
		int mvolume_table_size = data->mvolume_table_size;
		int ctl_mscaling = data->ctl_mscaling;
		data->no_active_channels = 1;
		for( int c = 0; c < data->ctl_channels; c++ )
		{
		    gen_channel* chan = &data->channels[ c ];
		    if( !chan->playing ) continue;
		    data->no_active_channels = 0;
		    uint cdelta = chan->cdelta;
		    if( data->ctl_cmul > 0 )
		        cdelta *= data->ctl_cmul;
		    else
		        cdelta /= 2;
		    uint mdelta;
		    if( data->ctl_mmul > 0 )
		        mdelta = chan->cdelta * data->ctl_mmul;
		    else
		        mdelta = chan->cdelta / 2;
		    int c_env_sustain_ptr = ( data->ctl_ca + data->ctl_cd ) * FM_ENV_STEP;
		    int m_env_sustain_ptr = ( data->ctl_ma + data->ctl_md ) * FM_ENV_STEP;
		    int m_env_max;
		    if( pnet->base_host_version >= 0x01070401 )
		        m_env_max = ( mvolume_table_size + 1 ) * FM_ENV_STEP;
		    else
		        m_env_max = mvolume_table_size * FM_ENV_STEP;
		    int mscaling;
		    for( int i = 0; i < ctl_mscaling; i++ )
		    {
		        if( i == 0 )
		    	    mscaling = 128 - chan->note;
			else
			    mscaling = ( mscaling * ( 128 - chan->note ) ) / 128;
		    }
		    PS_STYPE* render_buf = PSYNTH_GET_RENDERBUF( retval, resamp_outputs, outputs_num, resamp_offset );
		    int cptr = chan->cptr;
		    int mptr = chan->mptr;
		    int c_env_ptr = chan->c_env_ptr;
		    int m_env_ptr = chan->m_env_ptr;
		    int i;
		    for( i = 0; i < resamp_frames; i++ )
		    {
			PS_STYPE2 cval;
#ifdef FM_HQ32
			float mvol;
#else
			int mvol;
#endif
			if( data->ctl_mode < MODE_LQ )
			{
			    int coef = m_env_ptr & ( FM_ENV_STEP - 1 );
#ifdef CHECK_TABLES
			    if( ( m_env_ptr / FM_ENV_STEP ) + 1 >= FM_TABLE_SIZE ) slog( "FM render error 1.\n" );
#endif
#ifdef FM_HQ32
			    float coef_f = (float)coef / (float)FM_ENV_STEP;
			    mvol = 
			        coef_f * mvolume_table[ ( m_env_ptr / FM_ENV_STEP ) + 1 ] +
			        ( 1.0f - coef_f ) * mvolume_table[ m_env_ptr / FM_ENV_STEP ];
#else
			    mvol = 
			        ( coef * mvolume_table[ ( m_env_ptr / FM_ENV_STEP ) + 1 ] +
			        ( FM_ENV_STEP - coef ) * mvolume_table[ m_env_ptr / FM_ENV_STEP ] ) / FM_ENV_STEP;
#endif
			}
			else
			{
			    mvol = mvolume_table[ m_env_ptr / FM_ENV_STEP ];
			}
			if( ctl_mscaling ) mvol = ( mvol * mscaling ) / 128;
			if( mselfmod == 0 )
			{
#ifdef FM_HQ32
			    cval =
			        sinf(
			            sinf( FM_2PI * ( (float)(mptr&((1<<22)-1)) / (float)(1<<22) ) )
			    	    * mvol
				    * FM_2PI * 8.0f
				    + ( FM_2PI * ( (float)(cptr&((1<<22)-1)) / (float)(1<<22) ) )
				);
#else
			    cval = sin_tab[ ( ( sin_tab[ ( mptr >> (22-FM_SINUS_SH) ) & (FM_SINUS_SIZE-1) ] * mvol ) / (FM_TABLE_AMP*FM_MOD_DIV) + ( cptr >> (22-FM_SINUS_SH) ) ) & (FM_SINUS_SIZE-1) ];
#endif
			}
			else
			{
#ifdef FM_HQ32
			    const uint mask = ( 1 << 22 ) - 1;
			    float mptr_f = FM_2PI * ( (float)( mptr & mask ) / (float)( 1 << 22 ) );
			    float fb = ( ( sinf( mptr_f ) * mselfmod ) / 256.0f ) * FM_2PI * 8.0f;
			    float sin_val = sinf( fb + mptr_f );
			    sin_val *= mvol;
			    sin_val *= FM_2PI * 8.0f;
			    sin_val += ( FM_2PI * ( (float)( cptr & mask ) / (float)( 1 << 22 ) ) );
			    cval = sinf( sin_val );
#else
			    int fb = ( sin_tab[ ( mptr >> (22-FM_SINUS_SH) ) & (FM_SINUS_SIZE-1) ] * mselfmod ) / (256*FM_MOD_DIV);
			    int sin_ptr1 = ( ( mptr >> (22-FM_SINUS_SH) ) + fb ) & (FM_SINUS_SIZE-1);
			    int sin_val = sin_tab[ sin_ptr1 ];
			    int sin_ptr2 = ( ( sin_val * mvol ) / (FM_TABLE_AMP*FM_MOD_DIV) + ( cptr >> (22-FM_SINUS_SH) ) ) & (FM_SINUS_SIZE-1);
			    cval = sin_tab[ sin_ptr2 ];
#endif
			}
			if( data->ctl_mode < MODE_LQ )
			{
			    int coef = c_env_ptr & ( FM_ENV_STEP - 1 );
#ifdef CHECK_TABLES
			    if( ( c_env_ptr / FM_ENV_STEP ) + 1 >= FM_TABLE_SIZE ) slog( "FM render error 2.\n" );
#endif
#ifdef FM_HQ32
			    float coef_f = (float)coef / (float)FM_ENV_STEP;
			    cval *=
			        coef_f * cvolume_table[ ( c_env_ptr / FM_ENV_STEP ) + 1 ] +
			        ( 1.0f - coef_f ) * cvolume_table[ c_env_ptr / FM_ENV_STEP ];
#else
			    cval *=
			        ( coef * cvolume_table[ ( c_env_ptr / FM_ENV_STEP ) + 1 ] +
			        ( FM_ENV_STEP - coef ) * cvolume_table[ c_env_ptr / FM_ENV_STEP ] ) / FM_ENV_STEP;
			    cval /= FM_TABLE_AMP;
#endif
			}
			else
			{
#ifdef FM_HQ32
			    cval *= cvolume_table[ c_env_ptr / FM_ENV_STEP ];
#else
			    cval *= cvolume_table[ c_env_ptr / FM_ENV_STEP ];
			    cval /= FM_TABLE_AMP;
#endif
			}
			PS_STYPE2 out_val;
#ifdef FM_HQ32
			out_val = cval;
#else
			PS_INT16_TO_STYPE( out_val, cval );
#endif
			render_buf[ i ] = out_val;
			cptr += cdelta;
			mptr += mdelta;
			c_env_ptr++;
			m_env_ptr++;
			if( chan->sustain )
			{
			    if( c_env_ptr > c_env_sustain_ptr )
			        c_env_ptr = c_env_sustain_ptr;
			    if( m_env_ptr > m_env_sustain_ptr )
			        m_env_ptr = m_env_sustain_ptr;
			}
			else
			{
			    if( c_env_ptr >= cvolume_table_size * FM_ENV_STEP )
			    {
			        chan->playing = 0;
			        break;
			    }
			    if( m_env_ptr >= m_env_max )
			        m_env_ptr = m_env_max;
			}
		    }
		    chan->cptr = cptr;
		    chan->mptr = mptr;
		    chan->c_env_ptr = c_env_ptr;
		    chan->m_env_ptr = m_env_ptr;
		    if( !chan->playing ) chan->id = ~0;
		    retval = psynth_renderbuf2output(
    			retval,
		        resamp_outputs, outputs_num, resamp_offset, resamp_frames,
		        render_buf, NULL, i,
		        ( data->ctl_cvolume * chan->vel ) / 2, data->ctl_pan * 256,
		        &chan->renderbuf_pars, &data->smoother_coefs,
		        FM_SFREQ );
		} 
#ifndef ONLY44100
		if( data->resamp ) retval = psynth_resampler_end( data->resamp, retval, resamp_bufs, outputs, outputs_num, offset );
#endif
	    }
	    break;
	case PS_CMD_NOTE_ON:
	    {
		int c;
		if( data->no_active_channels == 0 )
		{
		    for( c = 0; c < MAX_CHANNELS; c++ )
		    {
			if( data->channels[ c ].id == event->id )
			{
			    if( data->channels[ c ].sustain )
			    {
				data->channels[ c ].sustain = 0;
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
			if( pnet->base_host_version >= 0x02010301 )
			{
			    for( c = 0; c < data->ctl_channels; c++ )
			    {
				gen_channel* ch = &data->channels[ data->search_ptr ];
				if( ch->sustain == 0 ) break;
				data->search_ptr++;
				if( data->search_ptr >= data->ctl_channels ) data->search_ptr = 0;
			    }
			}
		    }
		}
		else 
		{
		    data->search_ptr = 0;
		}
		uint delta_h, delta_l;
		int freq;
		PSYNTH_GET_FREQ( data->linear_freq_tab, freq, event->note.pitch / 4 );
		PSYNTH_GET_DELTA( FM_SFREQ, freq, delta_h, delta_l );
		c = data->search_ptr;
		gen_channel* chan = &data->channels[ c ];
                chan->renderbuf_pars.start = true;
                if( chan->playing && data->ctl_ca ) chan->renderbuf_pars.anticlick = true;
		chan->playing = 1;
		chan->vel = event->note.velocity;
		chan->cdelta = delta_l | ( delta_h << 16 );
		chan->cptr = 0;
		chan->mptr = 0;
		chan->c_env_ptr = 0;
		chan->m_env_ptr = 0;
		chan->id = event->id;
		if( pnet->base_host_version == 0x01070000 )
		    chan->note = event->note.root_note;
		else
		{
		    int n = PS_PITCH_TO_NOTE( event->note.pitch );
		    if( n < 0 ) n = 0;
		    if( n >= 10 * 12 ) n = 10 * 12 - 1;
		    chan->note = n;
		}
		chan->sustain = 1;
		data->no_active_channels = 0;
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
		    PSYNTH_GET_FREQ( data->linear_freq_tab, freq, event->note.pitch / 4 );
		    PSYNTH_GET_DELTA( FM_SFREQ, freq, delta_h, delta_l );
		    data->channels[ c ].cdelta = delta_l | ( delta_h << 16 );
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
	case PS_CMD_NOTE_OFF:
	    if( data->no_active_channels ) break;
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		if( data->channels[ c ].id == event->id )
		{
		    data->channels[ c ].sustain = 0;
		    break;
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_ALL_NOTES_OFF:
	    for( int c = 0; c < MAX_CHANNELS; c++ )
	    {
		data->channels[ c ].sustain = 0;
		data->channels[ c ].id = ~0;
	    }
	    retval = 1;
	    break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	case PS_CMD_SET_GLOBAL_CONTROLLER:
	    data->tables_render_request = 1;
	    break;
	case PS_CMD_CLOSE:
	    smem_free( data->cvolume_table );
	    smem_free( data->mvolume_table );
#ifndef ONLY44100
            psynth_resampler_remove( data->resamp );
#endif
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}

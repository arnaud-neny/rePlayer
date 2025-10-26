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
#include "psynths_metamodule.h"
#include "sunvox_engine.h"
#define MODULE_DATA	psynth_metamodule_data
#define MODULE_HANDLER	psynth_metamodule
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
#define USER_CTLS_OFFSET 	5
#define MAX_USER_CTLS		96
#if MAX_USER_CTLS + USER_CTLS_OFFSET + 8 > PSYNTH_MAX_CTLS
    #error WRONG MAX_USER_CTLS
#endif
#define MAX_CHANNELS    16
#define OPT_FLAG_GEN		( 1 << 0 ) 
#define OPT_FLAG_NOTGEN		( 1 << 1 ) 
#define OPT_FLAG_AUTO_BPM_TPL	( 1 << 2 ) 
#define OPT_FLAG_IGNORE_EFF31_AFTER_LAST_NOTEOFF	( 1 << 3 ) 
#define OPT_FLAG_JMP_TO_RL_PAT_AFTER_LAST_NOTEOFF	( 1 << 4 ) 
struct metamodule_options
{
    uint8_t		user_ctls_num;
    uint8_t		arpeggiator; 
    uint8_t		use_velocity; 
    uint8_t		no_evt_out; 
    uint32_t		flags; 
};
struct gen_channel
{
    bool                playing;
    uint                id;
    int                 vel;
};
struct MODULE_DATA
{
    PS_CTYPE		ctl_volume;
    PS_CTYPE		ctl_input;
    PS_CTYPE		ctl_play;	
    PS_CTYPE		ctl_bpm;
    PS_CTYPE		ctl_tpl;
    PS_CTYPE		ctl_user[ MAX_USER_CTLS ];
    gen_channel         channels[ MAX_CHANNELS ]; 
    int			active_channels;	  
    psynth_sunvox*	ps;
    size_t		proj_size;
    int			release_pattern; 
    uint32_t*		ctl_links; 
    char*		ctl_names[ MAX_USER_CTLS ]; 
    metamodule_options*	opt;
    metamodule_options	default_opt;
#ifdef SUNVOX_GUI
    window_manager*	wm;
#endif
};
static void metamodule_unpack_user_ctls( int mod_num, psynth_net* pnet );
static void metamodule_get_flags( psynth_module* mod, psynth_net* pnet );
static void metamodule_handle_ctl_play( MODULE_DATA* data, int prev_ctl_play );
int metamodule_load( const char* name, sfs_file f, int mod_num, psynth_net* pnet )
{
    psynth_module* mod;
    MODULE_DATA* data;
    if( mod_num >= 0 )
    {
	mod = &pnet->mods[ mod_num ];
	data = (MODULE_DATA*)mod->data_ptr;
    }
    else
    {
	return -1;
    }
    size_t fsize = 0;
    if( name && name[ 0 ] != 0 )
	fsize = sfs_get_file_size( name );
    int rv = -1;
    if( f == 0 )
        rv = sunvox_load_proj( name, SUNVOX_PROJ_LOAD_MAKE_TIMELINE_STATIC, data->ps->s[ 0 ] );
    else
        rv = sunvox_load_proj_from_fd( f, SUNVOX_PROJ_LOAD_MAKE_TIMELINE_STATIC, data->ps->s[ 0 ] );
    if( rv == 0 )
    {
        data->proj_size = fsize;
	data->release_pattern = -1;
        data->ctl_volume = data->ps->s[ 0 ]->net->global_volume;
        metamodule_unpack_user_ctls( mod_num, pnet );
        metamodule_get_flags( mod, pnet );
	metamodule_handle_ctl_play( data, data->ctl_play );
#ifdef SUNVOX_GUI
        metamodule_editor_reinit( mod_num, pnet );
        metamodule_visual_data* vdata = (metamodule_visual_data*)mod->visual->data;
	draw_window( vdata->editor_window, mod->visual->wm );
#endif
	pnet->change_counter++;
    }
    return rv;
}
static void metamodule_make_user_ctl_name( int mod_num, uint ctl_num, psynth_net* pnet )
{
    psynth_module* mod;
    MODULE_DATA* data;
    if( mod_num >= 0 )
    {
        mod = &pnet->mods[ mod_num ];
        data = (MODULE_DATA*)mod->data_ptr;
    }
    else return;
    if( ctl_num >= MAX_USER_CTLS ) return;
    smem_free( data->ctl_names[ ctl_num ] );
    data->ctl_names[ ctl_num ] = NULL;
    char* name = (char*)psynth_get_chunk_data( mod_num, 8 + ctl_num, pnet );
    while( 1 )
    {
	if( name )
	{
	    if( smem_strlen( name ) > 0 )
	    {
		data->ctl_names[ ctl_num ] = SMEM_STRDUP( name );
		break;
	    }
	}
	if( data->ps )
	{
	    sunvox_engine* s = data->ps->s[ 0 ];
	    int link_mod = data->ctl_links[ ctl_num ] & 0xFFFF;
	    int link_ctl = ( data->ctl_links[ ctl_num ] >> 16 ) & 0xFFFF;
	    if( (unsigned)link_mod < (unsigned)s->net->mods_num )
	    {
		psynth_module* mod = &s->net->mods[ link_mod ];
    		if( mod->flags & PSYNTH_FLAG_EXISTS )
    		{
    		    if( (unsigned)link_ctl < (unsigned)mod->ctls_num )
    		    {
    			psynth_ctl* ctl2 = &mod->ctls[ link_ctl ];
			data->ctl_names[ ctl_num ] = SMEM_STRDUP( ctl2->name );
    		    }
    		}
    	    }
	}
	break;
    }
    psynth_ctl* ctl = &mod->ctls[ ctl_num + USER_CTLS_OFFSET ];
    ctl->group = 2;
    if( data->ctl_names[ ctl_num ] )
    {
	char* name = data->ctl_names[ ctl_num ];
	if( name[ 0 ] == '@' )
	{
	    char c = name[ 1 ];
	    int g = -1;
	    if( c >= '0' && c <= '9' ) g = c - '0';
	    if( c >= 'a' && c <= 'f' ) g = 10 + c - 'a';
	    if( c >= 'A' && c <= 'F' ) g = 10 + c - 'A';
	    if( g != -1 )
	    {
		ctl->group = g;
		name += 2;
	    }
	}
	ctl->name = (const char*)name;
    }
    else
	ctl->name = "...";
}
static void metamodule_unpack_user_ctls( int mod_num, psynth_net* pnet )
{
    psynth_module* mod;
    MODULE_DATA* data;
    if( mod_num >= 0 )
    {
        mod = &pnet->mods[ mod_num ];
        data = (MODULE_DATA*)mod->data_ptr;
    }
    else return;
    mod->full_redraw_request++;
    for( int i = 0; i < MAX_USER_CTLS; i++ )
    {
	metamodule_make_user_ctl_name( mod_num, i, pnet );
	if( data->ps )
	{
	    sunvox_engine* s = data->ps->s[ 0 ];
	    psynth_ctl* ctl = &mod->ctls[ i + USER_CTLS_OFFSET ];
	    int link_mod = data->ctl_links[ i ] & 0xFFFF;
	    int link_ctl = ( data->ctl_links[ i ] >> 16 ) & 0xFFFF;
	    if( (unsigned)link_mod < (unsigned)s->net->mods_num )
	    {
		psynth_module* mod = &s->net->mods[ link_mod ];
    		if( mod->flags & PSYNTH_FLAG_EXISTS )
    		{
    		    if( (unsigned)link_ctl < (unsigned)mod->ctls_num )
    		    {
    			psynth_ctl* ctl2 = &mod->ctls[ link_ctl ];
    			ctl->label = ctl2->label;
    			ctl->min = ctl2->min;
    			ctl->max = ctl2->max;
    			ctl->def = ctl2->def;
    			*ctl->val = *ctl2->val;
    			ctl->normal_value = ctl2->normal_value;
    			ctl->show_offset = ctl2->show_offset;
    			ctl->type = ctl2->type;
    		    }
    		}
	    } 
	}
    }
}
static void metamodule_get_flags( psynth_module* mod, psynth_net* pnet )
{
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    psynth_module* input_mod = psynth_get_module( data->ctl_input, data->ps->s[ 0 ]->net );
    mod->flags &= ~( PSYNTH_FLAG_GENERATOR | PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_GET_SPEED_CHANGES );
    if( input_mod )
    {
        if( input_mod->flags & PSYNTH_FLAG_GENERATOR ) mod->flags |= PSYNTH_FLAG_GENERATOR;
        if( input_mod->flags & PSYNTH_FLAG_EFFECT ) mod->flags |= PSYNTH_FLAG_EFFECT;
    }
    if( data->ctl_play ) mod->flags |= PSYNTH_FLAG_GENERATOR;
    if( data->opt->flags & (OPT_FLAG_GEN|OPT_FLAG_NOTGEN) )
    {
	mod->flags &= ~PSYNTH_FLAG_GENERATOR;
	if( data->opt->flags & OPT_FLAG_GEN ) mod->flags |= PSYNTH_FLAG_GENERATOR;
    }
    if( data->opt->flags & OPT_FLAG_AUTO_BPM_TPL )
    {
	mod->flags |= PSYNTH_FLAG_GET_SPEED_CHANGES;
    }
}
static void metamodule_check_auto_bpm_tpl( psynth_module* mod, psynth_net* pnet, int bpm, int tpl )
{
    MODULE_DATA* data = (MODULE_DATA*)mod->data_ptr;
    if( data->opt->flags & OPT_FLAG_AUTO_BPM_TPL )
    {
	if( data->ps && data->ps->s[ 0 ]->initialized )
    	{
    	    sunvox_engine* s = data->ps->s[ 0 ];
    	    if( bpm == 0 )
    	    {
    		sunvox_engine* host_sv = (sunvox_engine*)pnet->host;
                if( host_sv )
                {
            	    s->bpm = host_sv->bpm;
            	    s->speed = host_sv->speed;
                }
    	    }
    	    else
    	    {
    		s->bpm = bpm;
    		s->speed = tpl;
    	    }
    	    data->ctl_bpm = s->bpm;
    	    data->ctl_tpl = s->speed;
    	    mod->draw_request++;
    	}
    }
}
inline void metamodule_handle_ctl_play_simple( MODULE_DATA* data )
{
    sunvox_engine* s = data->ps->s[ 0 ];
    if( data->ctl_play == 2 || data->ctl_play == 4 )
	s->stop_at_the_end_of_proj = true;
    else
	s->stop_at_the_end_of_proj = false;
}
static void metamodule_handle_ctl_play( MODULE_DATA* data, int prev_ctl_play )
{
    if( data->ps && data->ps->s[ 0 ]->initialized )
    {
	sunvox_engine* s = data->ps->s[ 0 ];
	bool stop = false;
	if( prev_ctl_play > 0 && data->ctl_play == 0 )
	{
	    stop = 1;
	}
	if( prev_ctl_play >= 3 && ( ( data->ctl_play == 1 || data->ctl_play == 2 ) && data->active_channels == 0 ) )
	{
	    stop = 1;
	}
	if( stop )
	{
    	    sunvox_user_cmd cmd;
    	    SMEM_CLEAR_STRUCT( cmd );
    	    cmd.n.note = NOTECMD_STOP;
    	    sunvox_send_user_command( &cmd, s );
	    if( data->active_channels > 0 )
	    {
        	for( int c = 0; c < MAX_CHANNELS; c++ )
        	{
            	    data->channels[ c ].playing = 0;
            	    data->channels[ c ].id = ~0;
        	}
        	data->active_channels = 0;
    	    }
        }
	metamodule_handle_ctl_play_simple( data );
    }
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
	    retval = (PS_RETTYPE)"MetaModule";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"Модуль, внутри которого находится отдельная независимая копия SunVox. Можно взять композицию в формате .sunvox, загрузить ее в MetaModule, после чего либо проиграть эту композицию (без изменений или с изменением тона в режиме арпеджиатора), либо играть отдельными ее модулями.";
                        break;
                    }
		    retval = (PS_RETTYPE)"MetaModule is a full-featured copy of SunVox. With this module you can include some .sunvox file to your project and then use this file as synth or effect.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#FF7800";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT | PSYNTH_FLAG_USE_MUTEX; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, PSYNTH_MAX_CTLS, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 1024, 256, 0, &data->ctl_volume, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_INPUT_MODULE ), "", 1, 256, 1, 1, &data->ctl_input, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_PLAY_PATTERNS ), ps_get_string( STR_PS_METAMODULE_PLAY_PAT_MODES ), 0, 4, 0, 1, &data->ctl_play, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_BPM ), "", 1, 1000, 125, 1, &data->ctl_bpm, 125, 1, pnet );
	    psynth_set_ctl_flags( mod_num, 3, PSYNTH_CTL_FLAG_EXP2, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_TPL ), "", 1, 31, 6, 1, &data->ctl_tpl, 6, 1, pnet );
	    for( int i = 0; i < MAX_USER_CTLS; i++ )
	    {
	        psynth_register_ctl( mod_num, ps_get_string( STR_PS_USER_DEFINED ), "", 0, 256, 0, 0, &data->ctl_user[ i ], 256, 2, pnet );
	    }
	    mod->ctls_num = USER_CTLS_OFFSET + 6;
	    psynth_change_ctls_num( mod_num, USER_CTLS_OFFSET + 6, pnet );
            for( int c = 0; c < MAX_CHANNELS; c++ )
            {
                data->channels[ c ].playing = 0;
                data->channels[ c ].id = ~0;
            }
            data->active_channels = 0;
	    data->ps = psynth_sunvox_new( pnet, mod_num, 1, 0 );
	    data->proj_size = 0;
	    data->release_pattern = -1;
	    for( uint i = 0; i < MAX_USER_CTLS; i++ )
		data->ctl_names[ i ] = NULL;
	    smem_clear( &data->default_opt, sizeof( metamodule_options ) );
	    data->default_opt.user_ctls_num = 3;
	    data->opt = &data->default_opt;
#ifdef SUNVOX_GUI
	    {
		data->wm = 0;
    		sunvox_engine* sv = (sunvox_engine*)pnet->host;
		if( sv && sv->win )
        	{
        	    window_manager* wm = sv->win->wm;
        	    data->wm = wm;
            	    mod->visual = new_window( "MetaModule GUI", 0, 0, 10, 10, wm->color1, 0, pnet, metamodule_visual_handler, wm );
            	    if( mod->visual && mod->visual->data )
            	    {
            		metamodule_visual_data* vdata = (metamodule_visual_data*)mod->visual->data;
            		mod->visual_min_ysize = vdata->min_ysize;
            		vdata->module_data = data;
            		vdata->mod_num = mod_num;
            		vdata->pnet = pnet;
            		if( vdata->editor_window && vdata->editor_window->childs[ 0 ]->data )
            		{
            	    	    metamodule_editor_data* wdata = (metamodule_editor_data*)vdata->editor_window->childs[ 0 ]->data;
            	    	    wdata->module_data = data;
            	    	    wdata->mod_num = mod_num;
            	    	    wdata->pnet = pnet;
            		    scrollbar_set_value( wdata->ctls_num, 6, wm );
            	    	}
            	    }
                }
            }
#endif
	    retval = 1;
	    break;
	case PS_CMD_SETUP_FINISHED:
	    {
		void* sunvox_file = psynth_get_chunk_data( mod_num, 0, pnet );
		if( sunvox_file )
		{
		    size_t size = 0;
		    psynth_get_chunk_info( mod_num, 0, pnet, &size, 0, 0 );
		    if( data->ps )
		    {
			sunvox_engine* s = data->ps->s[ 0 ];
			sfs_file f = sfs_open_in_memory( sunvox_file, size );
			if( f )
			{
			    data->proj_size = size;
			    sunvox_load_proj_from_fd( f, SUNVOX_PROJ_LOAD_MAKE_TIMELINE_STATIC, s );
			    sfs_close( f );
			}
		    }
		    psynth_remove_chunk( mod_num, 0, pnet );
		}
		data->ctl_links = (uint*)
		    psynth_get_chunk_data_autocreate( mod_num, 1, MAX_USER_CTLS * sizeof( uint32_t ), NULL, 0, pnet );
		bool opt_created = false;
                data->opt = (metamodule_options*)
            	    psynth_get_chunk_data_autocreate( mod_num, 2, sizeof( metamodule_options ), &opt_created, PSYNTH_GET_CHUNK_FLAG_CUT_UNUSED_SPACE, pnet );
                if( opt_created )
            	    smem_copy( data->opt, &data->default_opt, sizeof( metamodule_options ) );
#ifdef SUNVOX_GUI
                if( mod->visual && mod->visual->data && data->wm )
                {
            	    metamodule_visual_data* vdata = (metamodule_visual_data*)mod->visual->data;
            	    if( vdata->editor_window && vdata->editor_window->childs[ 0 ]->data )
            	    {
            	        metamodule_editor_data* wdata = (metamodule_editor_data*)vdata->editor_window->childs[ 0 ]->data;
            		scrollbar_set_value( wdata->ctls_num, data->opt->user_ctls_num, data->wm );
            	    }
            	}
#endif
		psynth_change_ctls_num( mod_num, USER_CTLS_OFFSET + data->opt->user_ctls_num, pnet );
                metamodule_unpack_user_ctls( mod_num, pnet );
    		metamodule_get_flags( mod, pnet );
    		metamodule_check_auto_bpm_tpl( mod, pnet, 0, 0 );
    		metamodule_handle_ctl_play( data, data->ctl_play );
	    }
	    retval = 1;
	    break;
	case PS_CMD_BEFORE_SAVE:
	    if( data->ps )
	    {
	        sunvox_engine* s = data->ps->s[ 0 ];
		sfs_file f = sfs_open_in_memory( SMEM_ALLOC( 4 ), 4 );
		if( f )
		{
		    int err = sunvox_save_proj_to_fd( f, SUNVOX_PROJ_SAVE_OPTIMIZE_PAT_SIZE, s );
    		    if( err != 0 ) 
    		    {
    			char ts[ 512 ];
    			sprintf( ts, "MetaModule %x save error %d", mod_num, err );
#ifdef SUNVOX_GUI
			if( data->wm ) edialog_open( ts, NULL, data->wm );
#endif
			smem_strcat( ts, sizeof( ts ), "\n" );
			slog( ts );
    		    }
    		    else
    		    {
			void* sunvox_file = sfs_get_data( f );
			sunvox_file = SMEM_RESIZE( sunvox_file, sfs_tell( f ) );
			psynth_new_chunk( mod_num, 0, 1, 0, 0, pnet ); 
			psynth_replace_chunk_data( mod_num, 0, sunvox_file, pnet );
		    }
		    sfs_close( f );
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_AFTER_SAVE:
	    psynth_remove_chunk( mod_num, 0, pnet );
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    if( data->ps && data->ps->s[ 0 ]->initialized )
	    {
		PS_STYPE** inputs = mod->channels_in;
                PS_STYPE** outputs = mod->channels_out;
                int frames = mod->frames;
                int offset = mod->offset;
                int outputs_num = psynth_get_number_of_outputs( mod );
                bool input_signal = false;
                for( int ch = 0; ch < outputs_num; ch++ )
                {
                    if( mod->in_empty[ ch ] < offset + frames )
                    {
                        input_signal = true;
                        break;
                    }
                }
    		sunvox_engine* s = data->ps->s[ 0 ];
        	s->net->global_volume = data->ctl_volume;
            	if( data->ctl_bpm != s->bpm ||
            	    data->ctl_tpl != s->speed )
            	{
            	    data->ctl_bpm = s->bpm;
            	    data->ctl_tpl = s->speed;
            	    mod->draw_request++;
            	}
		PS_STYPE* temp_channels_in[ PSYNTH_MAX_CHANNELS ];
		int temp_in_empty[ PSYNTH_MAX_CHANNELS ];
            	psynth_module* input_mod = NULL; 
            	while( (unsigned)data->ctl_input < (unsigned)s->net->mods_num )
            	{
            	    input_mod = &s->net->mods[ data->ctl_input ];
            	    if( !( input_mod->flags & PSYNTH_FLAG_EXISTS ) ) { input_mod = NULL; break; }
            	    if( ( input_mod->flags & PSYNTH_FLAG_EFFECT ) == 0 ||
            	        ( input_mod->flags & PSYNTH_FLAG_DONT_FILL_INPUT ) ) 
            	    { 
            		input_mod = NULL;
            		break; 
            	    }
            	    if( input_mod->input_links_num )
                    {
                        bool input_links = false;
                        for( int i = 0; i < input_mod->input_links_num; i++ )
                        {
                            if( input_mod->input_links[ i ] >= 0 )
                            {
                                input_links = true;
                                break;
                            }
                        }
                        if( input_links )
                        {
            		    input_mod = NULL;
            		    break;
                        }
                    }
            	    if( input_signal == false ) { input_mod = NULL; break; }
            	    break;
            	}
            	if( input_mod )
            	{
            	    input_mod->realtime_flags |= PSYNTH_RT_FLAG_DONT_CLEAN_INPUT;
            	    if( sizeof( input_mod->channels_in ) != sizeof( temp_channels_in ) ||
            		sizeof( temp_in_empty ) != sizeof( input_mod->in_empty ) )
            	    {
            		slog( "MetaModule error 1!\n" );
            		break;
            	    }
            	    smem_copy( &temp_channels_in, &input_mod->channels_in, sizeof( temp_channels_in ) );
            	    smem_copy( &temp_in_empty, &input_mod->in_empty, sizeof( temp_in_empty ) );
            	    for( int i = 0; i < psynth_get_number_of_inputs( input_mod ); i++ )
            	    {
            		int src_ch = i;
            		if( src_ch >= outputs_num ) src_ch = outputs_num - 1;
            		input_mod->channels_in[ i ] = inputs[ src_ch ] + offset;
            		int empty = mod->in_empty[ src_ch ] - offset;
            		if( empty < 0 ) empty = 0;
            		input_mod->in_empty[ i ] = empty;
            	    }
		}
    		sunvox_render_data rdata;
    		SMEM_CLEAR_STRUCT( rdata );
        	rdata.buffer_type = sound_buffer_int16;
    		rdata.buffer = 0;
    		rdata.frames = frames;
    		rdata.channels = outputs_num;
    		rdata.out_time = pnet->out_time;
    		if( sunvox_render_piece_of_sound( &rdata, s ) )
    		{
    		    psynth_module* output_mod = &s->net->mods[ 0 ];
    		    bool empty = true;
		    for( int ch = 0; ch < outputs_num; ch++ )
		    {
			if( output_mod->in_empty[ ch ] < frames ) { empty = false; break; }
		    }
		    if( empty == false )
		    {
			PS_STYPE* ch_data = 0;
			for( int ch = 0; ch < outputs_num; ch++ )
			{
		    	    PS_STYPE* out = outputs[ ch ] + offset;
		    	    if( ch < output_mod->output_channels ) ch_data = output_mod->channels_in[ ch ];
		    	    if( ch_data )
		    		smem_copy( out, ch_data, frames * sizeof( PS_STYPE ) );
		    	    else
		    		smem_clear( out, frames * sizeof( PS_STYPE ) );
			}
			retval = 1;
		    }
		    if( data->opt->no_evt_out == 0 && !( mod->realtime_flags & PSYNTH_RT_FLAG_MUTE ) )
		    {
			for( uint i = 0; i < output_mod->events_num; i++ )
                        {
                            psynth_event e = s->net->events_heap[ output_mod->events[ i ] ];
                            PSYNTH_EVT_ID_INC( e.id, mod_num ); 
                            e.id += ( 331 << 16 );
                    	    e.offset += offset;
                    	    int pitch = 0;
                            if( e.command == PS_CMD_NOTE_ON || e.command == PS_CMD_SET_FREQ )
                            {
                        	pitch = e.note.pitch;
                            }
                            for( int i = 0; i < mod->output_links_num; i++ )
                	    {
                    		int l = mod->output_links[ i ];
                    		if( (unsigned)l < (unsigned)pnet->mods_num )
                    		{
                        	    psynth_module* m = &pnet->mods[ l ];
                        	    if( m->flags & PSYNTH_FLAG_EXISTS )
                        	    {
                            		if( e.command == PS_CMD_NOTE_ON || e.command == PS_CMD_SET_FREQ )
                            		{
                            		    e.note.pitch = pitch - m->finetune - m->relative_note * 256;
                            		}
                            		psynth_add_event( l, &e, pnet );
                        	    }
                    		}
                	    }
                        }
		    }
		}
            	if( input_mod )
            	{
            	    smem_copy( &input_mod->channels_in, &temp_channels_in, sizeof( temp_channels_in ) );
            	    smem_copy( &input_mod->in_empty, &temp_in_empty, sizeof( temp_in_empty ) );
            	    input_mod->realtime_flags &= ~PSYNTH_RT_FLAG_DONT_CLEAN_INPUT;
		}
	    }
	    break;
	case PS_CMD_NOTE_ON:
	    if( data->ps && data->ps->s[ 0 ]->initialized )
            {
        	sunvox_engine* s = data->ps->s[ 0 ];
        	if( data->ctl_play )
        	{
        	    int c = 0;
        	    if( data->active_channels > 0 )
        	    {
        		for( c = 0; c < MAX_CHANNELS; c++ )
                	{
                	    gen_channel* chan = &data->channels[ c ];
                    	    if( chan->playing && chan->id == event->id ) break;
                        }
                        if( c >= MAX_CHANNELS )
                        {
        		    for( c = 0; c < MAX_CHANNELS; c++ )
                	    {
                		gen_channel* chan = &data->channels[ c ];
                    		if( chan->playing == 0 )
                    		{
                    		    chan->playing = 1;
                    		    chan->id = event->id;
                    		    data->active_channels++;
                    		    break;
                    		}
                	    }
                        }
		    }
		    else
		    {
                	gen_channel* chan = &data->channels[ c ];
                    	chan->playing = 1;
                    	chan->id = event->id;
                    	data->active_channels++;
                    	s->flags &= ~SUNVOX_FLAG_IGNORE_EFF31;
		    }
                    if( c < MAX_CHANNELS )
        	    {
        		sunvox_user_cmd cmd;
        		SMEM_CLEAR_STRUCT( cmd );
    			cmd.n.note = NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP;
			sunvox_send_user_command( &cmd, s );
			metamodule_handle_ctl_play_simple( data );
        		if( pnet->base_host_version >= 0x01090300 )
        		{
			    cmd.n.ctl = 0x001D; 
        		}
    			cmd.n.note = NOTECMD_PLAY;
			sunvox_send_user_command( &cmd, s );
        		if( data->opt->arpeggiator )
        		    s->pitch_offset = event->note.pitch - PS_NOTE_TO_PITCH( 5 * 12 );
        		else
        		    s->pitch_offset = 0;
        		if( data->opt->use_velocity )
        		    s->velocity = event->note.velocity;
        		else
        		    s->velocity = 256;
        	    }
        	}
        	else
        	{
        	    s->velocity = 256;
        	}
        	if( pnet->base_host_version >= 0x01090000 )
        	{
            	    psynth_module* input_mod = 0;
            	    if( (unsigned)data->ctl_input < (unsigned)s->net->mods_num )
            	    {
            		input_mod = &s->net->mods[ data->ctl_input ];
        		event->note.pitch -= input_mod->finetune + input_mod->relative_note * 256;
        	    }
        	}
        	sunvox_add_psynth_event_UNSAFE( data->ctl_input, event, s );
            }
	    retval = 1;
	    break;
	case PS_CMD_SET_FREQ:
	    if( data->ps && data->ps->s[ 0 ]->initialized )
            {
        	sunvox_engine* s = data->ps->s[ 0 ];
        	if( data->ctl_play )
        	{
                    if( data->active_channels > 0 )
                    {
        		if( data->opt->arpeggiator )
        		    s->pitch_offset = event->note.pitch - PS_NOTE_TO_PITCH( 5 * 12 );
        		else
        		    s->pitch_offset = 0;
        	    }
        	}
        	if( pnet->base_host_version >= 0x01090000 )
        	{
            	    psynth_module* input_mod = 0;
            	    if( (unsigned)data->ctl_input < (unsigned)s->net->mods_num )
            	    {
            		input_mod = &s->net->mods[ data->ctl_input ];
        		event->note.pitch -= input_mod->finetune + input_mod->relative_note * 256;
        	    }
        	}
        	sunvox_add_psynth_event_UNSAFE( data->ctl_input, event, s );
            }
	    retval = 1;
	    break;
	case PS_CMD_SET_VELOCITY:
	    if( data->ps && data->ps->s[ 0 ]->initialized )
            {
        	sunvox_engine* s = data->ps->s[ 0 ];
        	if( data->ctl_play )
        	{
                    if( data->active_channels > 0 )
                    {
        		if( data->opt->use_velocity )
        		    s->velocity = event->note.velocity;
        		else
        		    s->velocity = 256;
                    }
            	}
        	sunvox_add_psynth_event_UNSAFE( data->ctl_input, event, s );
            }
	    retval = 1;
	    break;
	case PS_CMD_NOTE_OFF:
	    if( data->ps && data->ps->s[ 0 ]->initialized )
            {
        	sunvox_engine* s = data->ps->s[ 0 ];
        	if( data->ctl_play )
        	{
                    if( data->active_channels > 0 )
                    {
        		for( int c = 0; c < MAX_CHANNELS; c++ )
                	{
                	    gen_channel* chan = &data->channels[ c ];
                    	    if( chan->id == event->id ) 
                    	    {
                    		chan->playing = 0;
                    		chan->id = ~0;
                    		data->active_channels--;
                    		break;
                    	    }
                        }
                        if( data->active_channels == 0 )
                        {
			    if( data->ctl_play < 3 )
			    {
				sunvox_user_cmd cmd;
                    		SMEM_CLEAR_STRUCT( cmd );
    				cmd.n.note = NOTECMD_STOP;
				sunvox_send_user_command( &cmd, s );
			    }
			    else
			    {
                    		if( data->opt->flags & OPT_FLAG_IGNORE_EFF31_AFTER_LAST_NOTEOFF )
                    		    s->flags |= SUNVOX_FLAG_IGNORE_EFF31;
                    		if( data->opt->flags & OPT_FLAG_JMP_TO_RL_PAT_AFTER_LAST_NOTEOFF )
                    		{
                    		    if( data->release_pattern < 0 )
                    			data->release_pattern = sunvox_get_pattern_num_by_name( "RL", s );
                    		    sunvox_pattern_info* pat_info = sunvox_get_pattern_info( data->release_pattern, s );
                    		    if( pat_info )
                    		    {
        				sunvox_user_cmd cmd;
        				SMEM_CLEAR_STRUCT( cmd );
					if( pat_info->x >= 0 )
					{
    					    cmd.n.note = NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP;
        				    cmd.n.ctl_val = pat_info->x;
        				}
        				else
        				{
    					    cmd.n.note = NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP2;
        				    cmd.n.ctl_val = -pat_info->x;
        				}
					sunvox_send_user_command( &cmd, s );
					cmd.n.ctl_val = 0;
		    			cmd.n.ctl = 0x001D; 
		        		cmd.n.note = NOTECMD_PLAY;
					sunvox_send_user_command( &cmd, s );
                    		    }
                    		}
                    	    }
			}
		    }
        	}
        	sunvox_add_psynth_event_UNSAFE( data->ctl_input, event, s );
            }
	    retval = 1;
	    break;
	case PS_CMD_ALL_NOTES_OFF:
	    if( data->ps && data->ps->s[ 0 ]->initialized )
            {
        	sunvox_engine* s = data->ps->s[ 0 ];
        	sunvox_user_cmd cmd;
        	SMEM_CLEAR_STRUCT( cmd );
    		cmd.n.note = NOTECMD_STOP;
		sunvox_send_user_command( &cmd, s );
        	sunvox_add_psynth_event_UNSAFE( data->ctl_input, event, s );
            }
            for( int c = 0; c < MAX_CHANNELS; c++ )
            {
                data->channels[ c ].playing = 0;
                data->channels[ c ].id = ~0;
            }
            data->active_channels = 0;
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    if( data->ps && data->ps->s[ 0 ]->initialized )
            {
        	sunvox_engine* s = data->ps->s[ 0 ];
        	sunvox_user_cmd cmd;
        	SMEM_CLEAR_STRUCT( cmd );
    		cmd.n.note = NOTECMD_STOP;
		sunvox_send_user_command( &cmd, s );
    		cmd.n.note = NOTECMD_CLEAN_MODULES;
		sunvox_send_user_command( &cmd, s );
            }
            for( int c = 0; c < MAX_CHANNELS; c++ )
            {
                data->channels[ c ].playing = 0;
            }
            data->active_channels = 0;
	    retval = 1;
	    break;
        case PS_CMD_SET_SAMPLE_OFFSET:
	case PS_CMD_SET_SAMPLE_OFFSET2:
            if( data->ps && data->ps->s[ 0 ]->initialized )
            {
        	sunvox_engine* s = data->ps->s[ 0 ];
                if( data->ctl_play )
        	{
        	    sunvox_user_cmd cmd;
        	    SMEM_CLEAR_STRUCT( cmd );
    		    cmd.n.note = NOTECMD_PREPARE_FOR_IMMEDIATE_JUMP;
                    if( event->command == PS_CMD_SET_SAMPLE_OFFSET )
        		cmd.n.ctl_val = event->sample_offset.sample_offset / 256;
        	    else
        		cmd.n.ctl_val = ( ( event->sample_offset.sample_offset & 32767 ) * s->proj_lines ) >> 15;
		    sunvox_send_user_command( &cmd, s );
		    cmd.n.ctl_val = 0;
        	    if( pnet->base_host_version >= 0x01090300 )
        	    {
		        cmd.n.ctl = 0x001D; 
        	    }
    		    cmd.n.note = NOTECMD_PLAY;
		    sunvox_send_user_command( &cmd, s );
        	}
        	sunvox_add_psynth_event_UNSAFE( data->ctl_input, event, s );
            }
            retval = 1;
            break;
	case PS_CMD_SET_LOCAL_CONTROLLER:
	case PS_CMD_SET_GLOBAL_CONTROLLER:
            {
        	metamodule_options* opt = data->opt;
                int v = event->controller.ctl_val;
                switch( event->controller.ctl_num + 1 )
                {
                    case 127: opt->arpeggiator = v!=0; retval = 1; break;
    		    case 126: opt->use_velocity = v!=0; retval = 1; break;
		    case 125: opt->no_evt_out = !v; retval = 1; break;
		    case 124: opt->flags &= ~(OPT_FLAG_GEN|OPT_FLAG_NOTGEN); if( v ) opt->flags |= OPT_FLAG_GEN; metamodule_get_flags( mod, pnet ); retval = 1; break;
		    case 123: opt->flags &= ~(OPT_FLAG_GEN|OPT_FLAG_NOTGEN); if( v ) opt->flags |= OPT_FLAG_NOTGEN; metamodule_get_flags( mod, pnet ); retval = 1; break;
		    case 122:
			opt->flags &= ~(OPT_FLAG_AUTO_BPM_TPL);
			if( v ) opt->flags |= OPT_FLAG_AUTO_BPM_TPL;
			metamodule_get_flags( mod, pnet );
			metamodule_check_auto_bpm_tpl( mod, pnet, 0, 0 );
			retval = 1; break;
		    case 121: opt->flags &= ~OPT_FLAG_IGNORE_EFF31_AFTER_LAST_NOTEOFF; if( v ) opt->flags |= OPT_FLAG_IGNORE_EFF31_AFTER_LAST_NOTEOFF; retval = 1; break;
		    case 120: opt->flags &= ~OPT_FLAG_JMP_TO_RL_PAT_AFTER_LAST_NOTEOFF; if( v ) opt->flags |= OPT_FLAG_JMP_TO_RL_PAT_AFTER_LAST_NOTEOFF; retval = 1; break;
                    default: break;
                }
                if( retval ) break;
            }
	    if( data->ps && data->ps->s[ 0 ]->initialized )
            {
        	sunvox_engine* s = data->ps->s[ 0 ];
        	int val;
        	switch( event->controller.ctl_num )
        	{
        	    case 1:
        		data->ctl_input = event->controller.ctl_val;
    			metamodule_get_flags( mod, pnet );
        		break;
        	    case 2:
        		{
        		    int prev_ctl_play = data->ctl_play;
        		    data->ctl_play = event->controller.ctl_val;
    			    metamodule_get_flags( mod, pnet );
        		    if( pnet->base_host_version >= 0x02010200 )
        		    {
    			        metamodule_handle_ctl_play( data, prev_ctl_play );
    			    }
    			}
        		break;
            	    case 3:
                	val = event->controller.ctl_val & 65535;
                	if( val ) s->bpm = val;
                	break;
            	    case 4:
                	val = event->controller.ctl_val & 255;
                	if( val ) s->speed = val;
                	break;
        	}
        	if( event->controller.ctl_num >= USER_CTLS_OFFSET )
        	{
        	    psynth_event evt = *event;
        	    int n = evt.controller.ctl_num - USER_CTLS_OFFSET;
        	    int link_mod = data->ctl_links[ n ] & 0xFFFF;
        	    if( link_mod )
        	    {
        		int link_ctl = ( data->ctl_links[ n ] >> 16 ) & 0xFFFF;
        		evt.controller.ctl_num = link_ctl;
        		sunvox_add_psynth_event_UNSAFE( link_mod, &evt, s );
        	    }
        	}
            }
            break;
        case PS_CMD_SPEED_CHANGED:
    	    metamodule_check_auto_bpm_tpl( mod, pnet, event->speed.bpm, event->speed.ticks_per_line );
            retval = 1;
            break;
	case PS_CMD_CLOSE:
#ifdef SUNVOX_GUI
	    if( mod->visual && data->wm )
	    {
        	remove_window( mod->visual, data->wm );
        	mod->visual = 0;
    	    }
#endif
	    psynth_sunvox_remove( data->ps );
	    data->ps = NULL;
	    for( uint i = 0; i < MAX_USER_CTLS; i++ )
		smem_free( data->ctl_names[ i ] );
	    retval = 1;
	    break;
        default:
            break;
    }
    return retval;
}

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
#include "../sunvox_engine.h"
void psynth_handle_midi_in_flags( uint mod_num, psynth_net* pnet )
{
    if( pnet->flags & PSYNTH_NET_FLAG_NO_MIDI ) return;
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( !mod ) return;
    if( pnet->midi_in_mods )
    {
	for( uint i = 0; i < pnet->midi_in_mods_num; i++ )
	{
	    if( pnet->midi_in_mods[ i ] == mod_num )
		pnet->midi_in_mods[ i ] = 0;
	}
    }
    bool add = false;
    if( mod->midi_in_flags & PSYNTH_MIDI_IN_FLAG_ALWAYS_ACTIVE )
    {
	add = true;
    }
    if( add )
    {
	int i = -1;
	if( !pnet->midi_in_mods )
	{
	    pnet->midi_in_mods = SMEM_ZALLOC2( uint, 4 );
	    i = 0;
	}
	else
	{
	    for( i = 0; i < (int)pnet->midi_in_mods_num; i++ )
	    {
		if( pnet->midi_in_mods[ i ] == 0 )
		    break;
	    }
	}
	if( i >= 0 )
	{
	    int old_size = (int)smem_get_size( pnet->midi_in_mods ) / sizeof( uint );
	    if( i >= old_size )
	    {
		size_t new_size = old_size + 4;
		pnet->midi_in_mods = SMEM_ZRESIZE2( pnet->midi_in_mods, uint, new_size );
	    }
	    pnet->midi_in_mods[ i ] = mod_num;
	}
    }
    pnet->midi_in_mods_num = 0;
    uint size = (uint)smem_get_size( pnet->midi_in_mods ) / sizeof( uint );
    for( uint i = 0; i < size; i++ )
    {
	if( pnet->midi_in_mods[ i ] != 0 )
	    pnet->midi_in_mods_num = i + 1;
    }
    if( pnet->midi_in_mods_num == 0 )
    {
	smem_free( pnet->midi_in_mods );
	pnet->midi_in_mods = NULL;
    }
}
static void psynth_make_ctl_midi_in_name( char* str, uint midi_pars1, uint midi_pars2 )
{
    int type = PSYNTH_MIDI_PARS1_GET_TYPE( midi_pars1 );
    int ch = PSYNTH_MIDI_PARS1_GET_CH( midi_pars1 );
    str[ 0 ] = int_to_hchar( ch );
    str[ 1 ] = int_to_hchar( type );
    str[ 2 ] = 0;
    if( type >= psynth_midi_note && type <= psynth_midi_prog )
    {
	int_to_string_hex( PSYNTH_MIDI_PARS2_GET_PAR( midi_pars2 ), &str[ 2 ] );
    }
}
int psynth_set_ctl_midi_in( uint mod_num, uint ctl_num, uint midi_pars1, uint midi_pars2, psynth_net* pnet )
{
    if( pnet->flags & PSYNTH_NET_FLAG_NO_MIDI ) return 0;
    psynth_module* mod = psynth_get_module( mod_num, pnet );
    if( mod == 0 ) return -1;
    if( ctl_num >= mod->ctls_num ) return -1;
    bool midi_in_active = false;
    psynth_ctl* ctl = &mod->ctls[ ctl_num ];
    char ts[ 64 ];
    SSYMTAB_VAL tv;
    SMEM_CLEAR_STRUCT( tv );
    int prev_type = PSYNTH_MIDI_PARS1_GET_TYPE( ctl->midi_pars1 );
    if( prev_type != psynth_midi_none )
    {
	psynth_make_ctl_midi_in_name( ts, ctl->midi_pars1, ctl->midi_pars2 );
	ssymtab_item* s = ssymtab_lookup( ts, -1, false, 0, tv, 0, pnet->midi_in_map );
	if( s )
	{
	    if( s->val.p )
	    {
		int* midi_links = (int*)s->val.p;
		size_t midi_links_num = smem_get_size( midi_links ) / sizeof( int );
		for( size_t i = 0; i < midi_links_num; i++ )
		{
		    if( midi_links[ i ] == (int)( mod_num | ( ctl_num << 16 ) ) )
		    {
			midi_links[ i ] = -1;
		    }
		}
	    }
	}
    }
    ctl->midi_pars1 = midi_pars1;
    ctl->midi_pars2 = midi_pars2;
    int new_type = PSYNTH_MIDI_PARS1_GET_TYPE( midi_pars1 );
    if( new_type != psynth_midi_none )
    {
	psynth_make_ctl_midi_in_name( ts, midi_pars1, midi_pars2 );
	ssymtab_item* s = ssymtab_lookup( ts, -1, true, 0, tv, 0, pnet->midi_in_map );
	if( s )
	{
	    if( s->val.p == 0 )
	    {
		s->val.p = SMEM_ALLOC( sizeof( int ) );
		((int*)s->val.p)[ 0 ] = -1;
	    }
	    int* midi_links = (int*)s->val.p;
	    size_t midi_links_num = smem_get_size( midi_links ) / sizeof( int );
	    size_t i = 0;
	    for( ; i < midi_links_num; i++ )
	    {
		if( midi_links[ i ] == -1 )
		{
		    break;
		}
	    }
	    if( i == midi_links_num )
	    {
		s->val.p = SMEM_RESIZE( s->val.p, ( midi_links_num + 1 ) * sizeof( int ) );
		midi_links = (int*)s->val.p;
	    }
	    midi_links[ i ] = mod_num | ( ctl_num << 16 );
	    midi_in_active = true;
	}
    }
    if( midi_in_active == false )
    {
	for( uint i = 0; i < mod->ctls_num; i++ )
	{
	    psynth_ctl* c = &mod->ctls[ i ];
	    if( PSYNTH_MIDI_PARS1_GET_TYPE( c->midi_pars1 ) != psynth_midi_none )
	    {
		midi_in_active = true;
		break;
	    }
	}
    }
    if( midi_in_active )
	mod->ui_flags |= PSYNTH_UI_FLAG_CTLS_WITH_MIDI_IN;
    else
	mod->ui_flags &= ~PSYNTH_UI_FLAG_CTLS_WITH_MIDI_IN;
    return 0;
}
void psynth_handle_ctl_midi_in( psynth_midi_evt* evt, int offset, psynth_net* pnet ) 
{
    if( pnet->flags & PSYNTH_NET_FLAG_NO_MIDI ) return;
    char ts[ 64 ];
    SSYMTAB_VAL tv;
    tv.i = 0;
    psynth_make_ctl_midi_in_name( ts, PSYNTH_CTL_MIDI_PARS1( evt->type, evt->ch, 0 ), PSYNTH_CTL_MIDI_PARS2( evt->val1, 0, 0 ) );
    ssymtab_item* s = ssymtab_lookup( ts, -1, false, 0, tv, 0, pnet->midi_in_map );
    if( s )
    {
	if( s->val.p )
	{
	    int* midi_links = (int*)s->val.p;
            size_t midi_links_num = smem_get_size( midi_links ) / sizeof( int );
            for( size_t i = 0; i < midi_links_num; i++ )
            {
        	if( midi_links[ i ] == -1 ) continue;
		uint mod_num = midi_links[ i ] & 0xFFFF;
		uint ctl_num = ( midi_links[ i ] >> 16 ) & 0xFFFF;
		psynth_module* mod = psynth_get_module( mod_num, pnet );
		if( mod == 0 ) continue;
		if( ctl_num >= mod->ctls_num ) continue;
		psynth_ctl* ctl = &mod->ctls[ ctl_num ];
		int val = 0;
		switch( evt->type )
		{
		    case psynth_midi_note:
		    case psynth_midi_key_pressure:
		    case psynth_midi_ctl:
	    	    case psynth_midi_nrpn:
		    case psynth_midi_rpn:
			val = evt->val2;
			break;
		    case psynth_midi_prog:
			val = 127;
			break;
		    case psynth_midi_channel_pressure:
		    case psynth_midi_pitch:
			val = evt->val1;
			break;
		    default:
			break;
		}
		if( val & psynth_midi_val_14bit_flag )
		    val = ( ( val & 16383 ) * 32768 ) / 16383;
		else
		    val = ( ( val & 127 ) * 32768 ) / 127;
		if( val > 32768 ) val = 32768;
		bool ignore = false;
		switch( PSYNTH_MIDI_PARS1_GET_MODE( ctl->midi_pars1 ) )
		{
		    case psynth_midi_lin:
			break;
		    case psynth_midi_exp1:
			val = dsp_curve( val, dsp_curve_type_quadratic1 );
			break;
		    case psynth_midi_exp2:
			val = dsp_curve( val, dsp_curve_type_quadratic2 );
			break;
		    case psynth_midi_spline:
			val = dsp_curve( val, dsp_curve_type_spline );
			break;
		    case psynth_midi_trig:
			if( val < 16384 ) val = 0; else val = 32768;
			break;
		    case psynth_midi_toggle:
			if( mod->render_counter != pnet->render_counter )
			{
			    mod->render_counter = pnet->render_counter;
			    for( uint c = 0; c < mod->ctls_num; c++ )
			    {
				psynth_ctl* ctl2 = &mod->ctls[ c ];
				int min2 = PSYNTH_MIDI_PARS2_GET_MIN( ctl2->midi_pars2 );
				if( ctl2->val[ 0 ] == ( min2 * ( ctl2->max - ctl2->min ) ) / 200 + ctl2->min )
				    ctl2->midi_val = 0;
				else
				    ctl2->midi_val = 1;
			    }
			}
			if( val >= 16384 )
			{
			    if( ctl->midi_val == 0 )
			    {
			        ctl->midi_val = 1;
			        val = 32768;
			    }
			    else
			    {
			        ctl->midi_val = 0;
			        val = 0;
			    }
			}
			else
			{
			    ignore = true;
			}
			break;
		}
		if( ignore == false )
		{
		    int min = PSYNTH_MIDI_PARS2_GET_MIN( ctl->midi_pars2 );
		    int max = PSYNTH_MIDI_PARS2_GET_MAX( ctl->midi_pars2 );
		    val = ( ( min * 32768 ) + val * ( max - min ) ) / 200;
		    if( ctl->type == 1 )
			val = ctl->min + ( ( val * ( ctl->max - ctl->min ) ) >> 15 );
		    psynth_event mod_evt;
		    mod_evt.offset = offset;
		    mod_evt.command = PS_CMD_SET_GLOBAL_CONTROLLER;
		    mod_evt.controller.ctl_num = ctl_num;
		    mod_evt.controller.ctl_val = val;
		    psynth_add_event( mod_num, &mod_evt, pnet );
		    mod->ui_flags |= PSYNTH_UI_FLAG_MIDI_IN_ACTIVITY;
    		}
    	    }
    	}
    }
}

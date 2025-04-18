#pragma once

//Various helpers that can be used in Pixilang, SunVox Library and some other SunVox-based projects.

#include "sunvox_engine.h"

#ifdef SVH_INLINES

inline int svh_send_event( sunvox_engine* s, stime_ticks_t t, int track, int note, int vel, int module, int ctl, int ctl_val )
{
    sunvox_user_cmd cmd;
    smem_clear_struct( cmd );
    cmd.ch = track;
    cmd.t = t;
    cmd.n.note = note;
    cmd.n.vel = vel;
    cmd.n.mod = module;
    cmd.n.ctl = ctl;
    cmd.n.ctl_val = ctl_val;
    sunvox_send_user_command( &cmd, s );
    return 0;
}

inline int svh_get_number_of_module_ctls( sunvox_engine* s, int mod_num )
{
    psynth_module* m = psynth_get_module( mod_num, s->net );
    if( m )
    {
        return m->ctls_num;
    }
    return 0;
}

inline const char* svh_get_module_ctl_name( sunvox_engine* s, int mod_num, int ctl_num )
{
    psynth_module* m = psynth_get_module( mod_num, s->net );
    if( m )
    {
        psynth_ctl* c = psynth_get_ctl( m, ctl_num, s->net );
        if( c )
        {
    	    return (const char*)m->ctls[ ctl_num ].name;
        }
    }
    return NULL;
}

//scaled: 0 - real ctl value; 1 - scaled for XXYY; 2 - displayed;
inline int svh_get_module_ctl_value( sunvox_engine* s, int mod_num, int ctl_num, int scaled )
{
    psynth_module* m = psynth_get_module( mod_num, s->net );
    if( m )
    {
        psynth_ctl* c = psynth_get_ctl( m, ctl_num, s->net );
        if( c )
        {
            int val = c->val[ 0 ];
            switch( scaled )
            {
                case 1:
                    if( c->type == 0 )
                        val = ( ( val - c->min ) << 15 ) / ( c->max - c->min );
                    break;
                case 2:
                    val += c->show_offset;
                    break;
            }
            return val;
        }
    }
    return 0;
}

//scaled: 0 - real ctl value; 1 - scaled for XXYY; 2 - displayed;
inline int svh_set_module_ctl_value( sunvox_engine* s, stime_ticks_t t, int mod_num, int ctl_num, int val, int scaled )
{
    psynth_module* m = psynth_get_module( mod_num, s->net );
    if( !m ) return -1;
    psynth_ctl* c = psynth_get_ctl( m, ctl_num, s->net );
    if( !c ) return -1;
    switch( scaled )
    {
        case 1:
            if( c->type == 0 )
                val = c->min + val * ( c->max - c->min ) / 0x8000;
            break;
        case 2:
            val -= c->show_offset;
            break;
    }
    if( val < c->min ) val = c->min;
    if( val > c->max ) val = c->max;
    if( c->type == 0 )
        val = ( val - c->min ) * 0x8000 / ( c->max - c->min );
    svh_send_event( s, t, 0, 0, 0, mod_num + 1, ( ctl_num + 1 ) << 8, val );
    return 0;
}

//scaled: 0 - real ctl value; 1 - scaled for XXYY; 2 - displayed;
//par: 0 - min; 1 - max; 2 - display offset; 3 - type; 4 - group;  DON'T CHANGE THIS ORDER! :)
inline int svh_get_module_ctl_par( sunvox_engine* s, int mod_num, int ctl_num, int scaled, int par )
{
    psynth_module* m = psynth_get_module( mod_num, s->net );
    if( !m ) return 0;
    psynth_ctl* c = psynth_get_ctl( m, ctl_num, s->net );
    if( !c ) return 0;
    int val = 0;
    switch( par )
    {
	case 0:
	    val = c->min;
	    switch( scaled )
	    {
    		case 1: if( c->type == 0 ) val = 0; break;
    		case 2: val += c->show_offset; break;
	    }
	    break;
	case 1:
	    val = c->max;
	    switch( scaled )
	    {
    		case 1: if( c->type == 0 ) val = 32768; break;
    		case 2: val += c->show_offset; break;
	    }
	    break;
	case 2:
	    val = c->show_offset;
	    break;
	case 3:
	    val = c->type;
	    break;
	case 4:
	    val = c->group;
	    break;
    }
    return val;
}

#endif //#ifdef SVH_INLINES

/*
    keymap.cpp - key redefinition
    This file is part of the SunDog engine.
    Copyright (C) 2014 - 2024 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

char g_ascii_names[ 128 * 2 ];

const char* get_key_name( int key )
{
    const char* rv = NULL;
    if( key > ' ' && key <= '~' )
    {
        if( g_ascii_names[ '0' * 2 ] != '0' )
        {
            for( int i = 0; i < 128; i++ )
            {
                int c = i;
                if( i >= 0x61 && i <= 0x7A ) c -= 0x20;
                g_ascii_names[ i * 2 ] = c;
                g_ascii_names[ i * 2 + 1 ] = 0;
            }
        }
        return (const char*)&g_ascii_names[ key * 2 ];
    }
    switch( key )
    {
        case KEY_BACKSPACE: rv = "Backspace"; break;
        case KEY_TAB: rv = "Tab"; break;
        case KEY_ENTER: rv = "Enter"; break;
        case KEY_ESCAPE: rv = "Escape"; break;
        case KEY_SPACE: rv = "Space"; break;
        case KEY_F1: rv = "F1"; break;
        case KEY_F2: rv = "F2"; break;
        case KEY_F3: rv = "F3"; break;
        case KEY_F4: rv = "F4"; break;
        case KEY_F5: rv = "F5"; break;
        case KEY_F6: rv = "F6"; break;
        case KEY_F7: rv = "F7"; break;
        case KEY_F8: rv = "F8"; break;
        case KEY_F9: rv = "F9"; break;
        case KEY_F10: rv = "F10"; break;
        case KEY_F11: rv = "F11"; break;
        case KEY_F12: rv = "F12"; break;
        case KEY_UP: rv = "Up"; break;
        case KEY_DOWN: rv = "Down"; break;
        case KEY_LEFT: rv = "Left"; break;
        case KEY_RIGHT: rv = "Right"; break;
        case KEY_INSERT: rv = "Insert"; break;
        case KEY_DELETE: rv = "Delete"; break;
        case KEY_HOME: rv = "Home"; break;
        case KEY_END: rv = "End"; break;
        case KEY_PAGEUP: rv = "PageUp"; break;
        case KEY_PAGEDOWN: rv = "PageDown"; break;
        case KEY_CAPS: rv = "CapsLock"; break;
        case KEY_SHIFT: rv = "Shift"; break;
        case KEY_CTRL: rv = "Ctrl"; break;
        case KEY_ALT: rv = "Alt"; break;
        case KEY_MENU: rv = "Menu"; break;
        case KEY_CMD: rv = "Cmd"; break;
        case KEY_FN: rv = "Fn"; break;
        case KEY_MIDI_NOTE:
        case KEY_MIDI_CTL:
        case KEY_MIDI_NRPN:
        case KEY_MIDI_RPN:
        case KEY_MIDI_PROG:
            rv = "MIDI: ";
            break;
        default:
            break;
    }
    return rv;
}

inline void keymap_make_codename( char* name, int key, uint flags, uint pars1, uint pars2 )
{
    if( pars1 || pars2 )
    {
	sprintf( name, "%x %x %x %x", key, flags, pars1, pars2 );
    }
    else
    {
	name[ 0 ] = int_to_hchar( flags >> 4 );
	name[ 1 ] = int_to_hchar( flags & 15 );
	hex_int_to_string( key, &name[ 2 ] );
    }
}

sundog_keymap* keymap_new( window_manager* wm )
{
    sundog_keymap* km = (sundog_keymap*)smem_new( sizeof( sundog_keymap ) );
    smem_zero( km );
    return km;
}

void keymap_remove( sundog_keymap* km, window_manager* wm )
{
    if( !km )
    {
	if( wm ) km = wm->km;
	if( !km ) return;
    }
    if( km->sections )
    {
	for( int i = 0; i < (int)( smem_get_size( km->sections ) / sizeof( sundog_keymap_section ) ); i++ )
	{
	    sundog_keymap_section* section = &km->sections[ i ];
	    for( int i2 = 0; i2 < (int)( smem_get_size( section->actions ) / sizeof( sundog_keymap_action ) ); i2++ )
	    {
		sundog_keymap_action* action = &section->actions[ i2 ];
	    }
	    smem_free( section->actions );
	    ssymtab_remove( section->bindings );
	}
	smem_free( km->sections );
    }
    smem_free( km );
}

void keymap_silent( sundog_keymap* km, bool silent, window_manager* wm )
{
    if( !km ) 
    {
	if( wm ) km = wm->km;
	if( !km ) return;
    }
    km->silent = silent;
}

int keymap_add_section( sundog_keymap* km, const char* section_name, const char* section_id, int section_num, int num_actions, window_manager* wm )
{
    if( !km ) 
    {
	if( wm ) km = wm->km;
	if( !km ) return -1;
    }
    int rv = -1;
    while( 1 )
    {
	if( km->sections == 0 )
	{
	    km->sections = (sundog_keymap_section*)smem_new( sizeof( sundog_keymap_section ) * ( section_num + 1 ) );
	    if( km->sections )
	    {
		smem_zero( km->sections );
		rv = 0;
	    }
	}
	else
	{
	    int count = (int)( smem_get_size( km->sections ) / sizeof( sundog_keymap_section ) );
	    if( (unsigned)section_num >= (unsigned)count )
	    {
		km->sections = (sundog_keymap_section*)smem_resize2( km->sections, ( section_num + 1 ) * sizeof( sundog_keymap_section ) );
		rv = 0;
	    }
	}
	if( rv != -1 )
	{
	    sundog_keymap_section* section = &km->sections[ section_num ];
	    section->last_added_action = -1;
	    section->name = section_name;
	    section->id = section_id;
	    section->bindings = ssymtab_new( 5 ); //1543
	    section->actions = (sundog_keymap_action*)smem_znew( sizeof( sundog_keymap_action ) * ( num_actions + 1 ) );
	}
	break;
    }
    return rv;
}

int keymap_add_action( sundog_keymap* km, int section_num, const char* action_name, const char* action_id, int action_num, window_manager* wm )
{
    if( !km )
    {
	if( wm ) km = wm->km;
	if( !km ) return -1;
    }
    int rv = -1;
    while( 1 )
    {
	if( section_num < 0 ) break;
	if( (unsigned)section_num >= smem_get_size( km->sections ) / sizeof( sundog_keymap_section ) ) break;
	sundog_keymap_section* section = &km->sections[ section_num ];
	if( section->actions == NULL )
	{
	    section->actions = (sundog_keymap_action*)smem_new( sizeof( sundog_keymap_action ) * ( action_num + 1 ) );
	    if( section->actions )
	    {
		smem_zero( section->actions );
		rv = 0;
	    }
	}
	else
	{
	    int count = (int)( smem_get_size( section->actions ) / sizeof( sundog_keymap_action ) );
	    if( (unsigned)action_num >= (unsigned)count )
	    {
		section->actions = (sundog_keymap_action*)smem_resize2( section->actions, ( action_num + 1 ) * sizeof( sundog_keymap_action ) );
	    }
	    rv = 0;
	}
	if( rv != -1 )
	{
	    sundog_keymap_action* action = &section->actions[ action_num ];
	    action->name = action_name;
	    action->id = action_id;
	    action->group = section->group;
	    if( section->last_added_action >= 0 && action_num - section->last_added_action != 1 )
		slog( "Wrong order for action %s\n", action_name );
	    section->last_added_action = action_num;
	}
	break;
    }
    return rv;
}

int keymap_set_group( sundog_keymap* km, int section_num, const char* group, window_manager* wm )
{
    if( !km )
    {
	if( wm ) km = wm->km;
	if( !km ) return -1;
    }
    int rv = -1;
    while( 1 )
    {
	if( section_num < 0 ) break;
	if( (unsigned)section_num >= smem_get_size( km->sections ) / sizeof( sundog_keymap_section ) ) break;
	sundog_keymap_section* section = &km->sections[ section_num ];
	section->group = group;
	rv = 0;
	break;
    }
    return rv;
}

const char* keymap_get_action_name( sundog_keymap* km, int section_num, int action_num, window_manager* wm )
{
    if( !km )
    {
	if( wm ) km = wm->km;
	if( !km ) return NULL;
    }
    const char* rv = NULL;
    while( 1 )
    {
	if( section_num < 0 ) break;
	if( (unsigned)section_num >= smem_get_size( km->sections ) / sizeof( sundog_keymap_section ) ) break;
	sundog_keymap_section* section = &km->sections[ section_num ];
	if( action_num < 0 ) break;
	if( (unsigned)action_num >= smem_get_size( section->actions ) / sizeof( sundog_keymap_action ) ) break;
	sundog_keymap_action* action = &section->actions[ action_num ];

	rv = action->name;

	break;
    }
    return rv;
}

const char* keymap_get_action_group_name( sundog_keymap* km, int section_num, int action_num, window_manager* wm )
{
    if( !km )
    {
	if( wm ) km = wm->km;
	if( !km ) return NULL;
    }
    const char* rv = NULL;
    while( 1 )
    {
	if( section_num < 0 ) break;
	if( (unsigned)section_num >= smem_get_size( km->sections ) / sizeof( sundog_keymap_section ) ) break;
	sundog_keymap_section* section = &km->sections[ section_num ];
	if( action_num < 0 ) break;
	if( (unsigned)action_num >= smem_get_size( section->actions ) / sizeof( sundog_keymap_action ) ) break;
	sundog_keymap_action* action = &section->actions[ action_num ];

	rv = action->group;

	break;
    }
    return rv;
}

int keymap_bind2( sundog_keymap* km, int section_num, int key, uint flags, uint pars1, uint pars2, int action_num, int bind_num, int bind_flags, window_manager* wm )
{
    if( !km )
    {
	if( wm ) km = wm->km;
	if( !km ) return -1;
    }
    int rv = -1;
    while( 1 )
    {
	if( section_num < 0 ) break;
	if( (unsigned)section_num >= smem_get_size( km->sections ) / sizeof( sundog_keymap_section ) ) break;
	sundog_keymap_section* section = &km->sections[ section_num ];
	int actions_count = (int)( smem_get_size( section->actions ) / sizeof( sundog_keymap_action ) );
	if( (unsigned)action_num >= (unsigned)actions_count ) break;
	sundog_keymap_action* action = &section->actions[ action_num ];
	if( (unsigned)bind_num >= KEYMAP_ACTION_KEYS ) break;

	flags &= EVT_FLAG_MODS;

	SSYMTAB_VAL val;
	val.i = 0;
	char name[ 128 ];

	if( bind_flags & ( KEYMAP_BIND_OVERWRITE | KEYMAP_BIND_RESET_TO_DEFAULT ) )
	{
	    uint32_t prev_key = action->keys[ bind_num ].key;
	    uint prev_flags = action->keys[ bind_num ].flags;
	    uint prev_pars1 = action->keys[ bind_num ].pars1;
	    uint prev_pars2 = action->keys[ bind_num ].pars2;
	    keymap_make_codename( name, prev_key, prev_flags, prev_pars1, prev_pars2 );
	    ssymtab_item* sym = ssymtab_lookup( (const char*)name, -1, 0, 0, val, 0, section->bindings );
	    if( sym )
	    {
		if( sym->val.i == action_num )
		{
		    sym->val.i = -1;
		    if( action->keys[ bind_num ].key == KEY_MIDI_NOTE )
		    {
			int midi_note = action->keys[ bind_num ].pars1 & 255;
			int midi_ch = ( action->keys[ bind_num ].pars2 >> 16 ) & 15;
			midi_note += 128 * midi_ch;
			km->midi_notes[ midi_note / 32 ] &= ~( 1 << ( midi_note & 31 ) );
		    }
		    if( prev_key && ( bind_flags & KEYMAP_BIND_FIX_CONFLICTS ) )
		    {
			//pass the binding to the conflicting action (where it is also needed):
			for( int a = 0; a < actions_count; a++ )
			{
			    if( a == action_num ) continue;
			    sundog_keymap_action* action2 = &section->actions[ a ];
			    for( int n = 0; n < KEYMAP_ACTION_KEYS; n++ )
			    {
				sundog_keymap_key* k = &action2->keys[ n ];
				if( k->key == prev_key &&
			    	    k->flags == prev_flags &&
			    	    k->pars1 == prev_pars1 &&
			    	    k->pars2 == prev_pars2 )
				{
				    sym->val.i = a;
				    if( !km->silent )
					slog( "Key %x;%x is now attached to action \"%s\"\n", prev_key, prev_flags, action2->name );
				}
			    }
			}
		    }
		}
	    }
	}

	if( bind_flags & KEYMAP_BIND_RESET_TO_DEFAULT )
	{
	    key = action->default_keys[ bind_num ].key;
	    flags = action->default_keys[ bind_num ].flags;
	    pars1 = action->default_keys[ bind_num ].pars1;
	    pars2 = action->default_keys[ bind_num ].pars2;
	}

	action->keys[ bind_num ].key = key;
	action->keys[ bind_num ].flags = flags;
	action->keys[ bind_num ].pars1 = pars1;
	action->keys[ bind_num ].pars2 = pars2;
	if( bind_flags & KEYMAP_BIND_DEFAULT )
	{
	    action->default_keys[ bind_num ].key = key;
	    action->default_keys[ bind_num ].flags = flags;
	    action->default_keys[ bind_num ].pars1 = pars1;
	    action->default_keys[ bind_num ].pars2 = pars2;
	}

	if( key || flags || pars1 || pars2 )
	{
	    val.i = action_num;
	    keymap_make_codename( name, key, flags, pars1, pars2 );
	    bool created = 0;
	    ssymtab_item* sym = ssymtab_lookup( (const char*)name, -1, 1, 0, val, &created, section->bindings );
	    if( sym )
	    {
		if( created == 0 )
		{
		    if( sym->val.i >= 0 && sym->val.i != val.i && !km->silent )
			slog( "Key %x;%x is now attached to action \"%s\" and detached from action \"%s\"\n", key, flags, section->actions[ action_num ].name, section->actions[ sym->val.i ].name );
		}
		sym->val = val;
		if( key == KEY_MIDI_NOTE )
		{
		    int midi_note = pars1 & 255;
		    int midi_ch = ( pars1 >> 16 ) & 15;
                    midi_note += 128 * midi_ch;
		    km->midi_notes[ midi_note / 32 ] |= 1 << ( midi_note & 31 );
		}
	    }
	}

	rv = 0;
	break;
    }
    return rv;
}

int keymap_bind( sundog_keymap* km, int section_num, int key, uint flags, int action_num, int bind_num, int bind_flags, window_manager* wm )
{
    return keymap_bind2( km, section_num, key, flags, 0, 0, action_num, bind_num, bind_flags, wm );
}

int keymap_get_action( sundog_keymap* km, int section_num, int key, uint flags, uint pars1, uint pars2, window_manager* wm )
{
    if( !km )
    {
	if( wm ) km = wm->km;
	if( !km ) return -1;
    }
    int rv = -1;
    while( 1 )
    {
	if( section_num < 0 ) break;
	if( (unsigned)section_num >= smem_get_size( km->sections ) / sizeof( sundog_keymap_section ) ) break;
	sundog_keymap_section* section = &km->sections[ section_num ];

	flags &= EVT_FLAG_MODS;

	if( key < KEY_MIDI_NOTE || key > KEY_MIDI_PROG )
	{
	    pars1 = 0;
	    pars2 = 0;
	}

	SSYMTAB_VAL val;
	val.i = 0;
	char name[ 128 ];
	keymap_make_codename( name, key, flags, pars1, pars2 );

	bool created = 0;
	ssymtab_item* sym = ssymtab_lookup( name, -1, 0, 0, val, 0, section->bindings );
	if( sym )
	{
	    rv = (int)sym->val.i;
	}

	break;
    }

    return rv;
}

sundog_keymap_key* keymap_get_key( sundog_keymap* km, int section_num, int action_num, int key_num, window_manager* wm )
{
    if( !km )
    {
	if( wm ) km = wm->km;
	if( !km ) return 0;
    }
    sundog_keymap_key* rv = 0;
    while( 1 )
    {
	if( section_num < 0 ) break;
	if( (unsigned)section_num >= smem_get_size( km->sections ) / sizeof( sundog_keymap_section ) ) break;
	sundog_keymap_section* section = &km->sections[ section_num ];
	if( action_num < 0 ) break;
	if( (unsigned)action_num >= smem_get_size( section->actions ) / sizeof( sundog_keymap_action ) ) break;
	sundog_keymap_action* action = &section->actions[ action_num ];
	if( key_num < 0 ) break;
	if( key_num >= KEYMAP_ACTION_KEYS ) break;

	if( action->keys[ key_num ].key || action->keys[ key_num ].flags )
	{
	    rv = &action->keys[ key_num ];
	}

	break;
    }
    return rv;
}

bool keymap_midi_note_assigned( sundog_keymap* km, int note, int channel, window_manager* wm )
{
    if( !km )
    {
	if( wm ) km = wm->km;
	if( !km ) return false;
    }
    int note2 = note + 128 * channel;
    if( km->midi_notes[ note2 / 32 ] & ( 1 << ( note2 & 31 ) ) )
	return true;
    return false;
}

int keymap_save( sundog_keymap* km, sconfig_data* config, window_manager* wm )
{
    if( !km )
    {
	if( wm ) km = wm->km;
	if( !km ) return -1;
    }
    int rv = -1;
    while( 1 )
    {
	int sections_count = (int)( smem_get_size( km->sections ) / sizeof( sundog_keymap_section ) );
	char* keys = (char*)smem_new( 8 );
	for( int snum = 0; snum < sections_count; snum++ )
	{
	    sundog_keymap_section* section = &km->sections[ snum ];
	    int actions_count = (int)( smem_get_size( section->actions ) / sizeof( sundog_keymap_action ) );
	    for( int anum = 0; anum < actions_count; anum++ )
	    {
		sundog_keymap_action* action = &section->actions[ anum ];
		char action_name[ 128 ];
		sprintf( action_name, "%s_%s", section->id, action->id );
		char key[ 10 * 5 ];
		keys[ 0 ] = 0;
		for( int i = 0; i < KEYMAP_ACTION_KEYS; i++ )
		{
		    if( action->keys[ i ].key != action->default_keys[ i ].key ||
			action->keys[ i ].flags != action->default_keys[ i ].flags ||
			action->keys[ i ].pars1 != action->default_keys[ i ].pars1 ||
			action->keys[ i ].pars2 != action->default_keys[ i ].pars2 )
		    {
			if( action->keys[ i ].pars1 || action->keys[ i ].pars2 )
			{
			    sprintf( key, "%x %x %x %x %x ", 100 + i, action->keys[ i ].key, action->keys[ i ].flags, action->keys[ i ].pars1, action->keys[ i ].pars2 );
			}
			else
			{
			    sprintf( key, "%x %x %x ", i, action->keys[ i ].key, action->keys[ i ].flags );
			}
			smem_strcat_resize( keys, key );
		    }
		}
		if( keys[ 0 ] == 0 )
		{
		    sconfig_remove_key( action_name, config );
		}
		else
		{
		    sconfig_set_str_value( action_name, keys, config );
		}
	    }
	}
	smem_free( keys );
	rv = 0;
	break;
    }
    if( rv == 0 )
    {
	sconfig_save( config );
    }
    return rv;
}

int keymap_load( sundog_keymap* km, sconfig_data* config, window_manager* wm )
{
    if( !km )
    {
	if( wm ) km = wm->km;
	if( !km ) return -1;
    }
    int rv = -1;
    while( 1 )
    {
	int sections_count = (int)( smem_get_size( km->sections ) / sizeof( sundog_keymap_section ) );
	for( int snum = 0; snum < sections_count; snum++ )
	{
	    sundog_keymap_section* section = &km->sections[ snum ];
	    int actions_count = (int)( smem_get_size( section->actions ) / sizeof( sundog_keymap_action ) );
	    for( int anum = 0; anum < actions_count; anum++ )
	    {
		sundog_keymap_action* action = &section->actions[ anum ];
		char action_name[ 128 ];
		sprintf( action_name, "%s_%s", section->id, action->id );
		char* keys = sconfig_get_str_value( (const char*)action_name, 0, config );
		if( keys )
		{
		    keys = smem_strdup( keys );
		    int nums[ 5 * KEYMAP_ACTION_KEYS ];
		    int nums_count = 0;
		    smem_clear( nums, sizeof( nums ) );
		    int len = (int)smem_strlen( keys );
		    char* num_ptr = &keys[ 0 ];
		    for( int i = 0; i < len; i++ )
		    {
			if( keys[ i ] == ' ' )
			{
			    keys[ i ] = 0;
			    nums[ nums_count ] = hex_string_to_int( num_ptr );
			    num_ptr = &keys[ i + 1 ];
			    nums_count++;
			    if( nums_count >= 5 * KEYMAP_ACTION_KEYS ) break;
			}
		    }
		    smem_free( keys );
		    for( int i = 0; i < nums_count; )
		    {
			int bind_num = nums[ i ]; i++;
			int key = nums[ i ]; i++;
			uint flags = nums[ i ]; i++;
			uint pars1 = 0;
			uint pars2 = 0;
			if( bind_num >= 100 )
			{
			    bind_num -= 100;
			    pars1 = nums[ i ]; i++;
			    pars2 = nums[ i ]; i++;
			}
			keymap_bind2( km, snum, key, flags, pars1, pars2, anum, bind_num, KEYMAP_BIND_OVERWRITE, wm );
		    }
		}
	    }
	}
	rv = 0;
	break;
    }
    return rv;
}

int keymap_print_available_keys( sundog_keymap* km, int section_num, int key_flags, window_manager* wm )
{
    if( !km )
    {
	if( wm ) km = wm->km;
	if( !km ) return -1;
    }
    int rv = -1;
    bool* keys = NULL;
    while( 1 )
    {
	if( section_num < 0 ) break;
	if( (unsigned)section_num >= smem_get_size( km->sections ) / sizeof( sundog_keymap_section ) ) break;
	sundog_keymap_section* section = &km->sections[ section_num ];
        int actions_count = (int)( smem_get_size( section->actions ) / sizeof( sundog_keymap_action ) );
	keys = (bool*)smem_znew( sizeof( bool ) * 256 );
        for( int anum = 0; anum < actions_count; anum++ )
        {
    	    sundog_keymap_action* action = &section->actions[ anum ];
    	    for( int i = 0; i < KEYMAP_ACTION_KEYS; i++ )
	    {
		sundog_keymap_key* key = &action->keys[ i ];
		if( key->key > 0 && key->key < KEY_UNKNOWN )
		{
		    if( key->flags == key_flags )
		    {
			keys[ key->key ] = 1;
		    }
		}
	    }
	}
	slog( "Available keys for the section \"%s\" (flags %x):\n", section->name, key_flags );
	for( int i = 0x20; i < KEY_UNKNOWN; i++ )
	{
	    if( i >= 'A' && i <= 'Z' ) continue; //skip capital chars
	    if( keys[ i ] == 0 )
	    {
		slog( "  %s (%x)\n", get_key_name( i ), i );
	    }
	}
	rv = 0;
	break;
    }
    smem_free( keys );
    return rv;
}

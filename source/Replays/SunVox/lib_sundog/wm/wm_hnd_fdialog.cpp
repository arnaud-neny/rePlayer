/*
    wm_hnd_fdialog.cpp - file dialog window (file browser)
    This file is part of the SunDog engine.
    Copyright (C) 2004 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"

#ifdef OS_UNIX
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

#ifdef OS_IOS
    #define ONLY_WORKING_DIR
#endif

#ifdef OS_ANDROID
    #include "main/android/sundog_bridge.h"
#endif

struct fdialog_data
{
    WINDOWPTR win;
    uint flags; //FDIALOG_FLAG_*
    bool show_hidden;
    int panel_recent_def_y;
    int panel_recent_def_xsize;
    WINDOWPTR panel_recent; //panel with recent files and dirs
    WINDOWPTR panel_main; //main panel with file name, file selector, buttons, etc.
    slist_data recent_files_data;
    slist_data recent_dirs_data;
    WINDOWPTR recent_div;
    WINDOWPTR recent_files;
    WINDOWPTR recent_dirs;
    bool recent_size_init_request;
    WINDOWPTR go_up_button;
    WINDOWPTR go_home_button;
    WINDOWPTR addvd_buttons[ 9 - ADD_VIRT_DISK ]; //additional virtual disks (system dependent)
    WINDOWPTR disk_buttons[ MAX_DISKS ];
    WINDOWPTR path;
    WINDOWPTR name;
    WINDOWPTR files;
    WINDOWPTR ok_button;
    WINDOWPTR cancel_button;
    WINDOWPTR edit_button;
    WINDOWPTR preview_button;
    int (*preview_handler)( fdialog_preview_data*, window_manager* );
    fdialog_preview_data preview_data;
    char* mask;
    char* props_file; //2:/dialog_id - properties of the current file dialog window
    char* props_file_recent_dirs; //2:/dialog_id_recent_dirs
    char* props_file_recent_files; //2:/dialog_id_recent_files
    char* props_file_dir_pos; //2:/dialog_id_dir_pos
    char* preview;
    char* prop_path[ MAX_DISKS ]; //current path for the specified disk: dir/
    int prop_cur_file[ MAX_DISKS ]; //current file number for the specified disk
    int	prop_cur_disk;
    ssymtab* dir_pos;
    char* final_path; //X:/dir/
    char* final_filename; //X:/dir/file
    bool first_time;
    bool fdialog_preview_flag;
};

static void fdialog_refresh_list( fdialog_data* data );
int fdialog_disk_button_handler( void* user_data, WINDOWPTR win, window_manager* wm );
int fdialog_ok_button_handler( void* user_data, WINDOWPTR win, window_manager* wm );
int fdialog_cancel_button_handler( void* user_data, WINDOWPTR win, window_manager* wm );
int fdialog_name_handler( void* user_data, WINDOWPTR name_win, window_manager* wm );

//
// Main static functions
//

static bool is_fdialog_win_valid( WINDOWPTR win )
{
    if( !win ) return false;
    if( !win->data ) return false;
    if( smem_get_size( win->data ) != sizeof( fdialog_data ) ) return false;
    return true;
}

static int fdialog_get_disk_count()
{
#ifdef ONLY_WORKING_DIR
    return 1;
#else
    return sfs_get_disk_count();
#endif
}

static void fdialog_refresh_disks()
{
    sfs_refresh_disks();
}

static const char* fdialog_get_disk_name( uint n )
{
#ifdef ONLY_WORKING_DIR
    return "1:/";
#else
    return sfs_get_disk_name( n );
#endif
}

static int fdialog_get_disk_num( const char* path )
{
#ifdef ONLY_WORKING_DIR
    return 0;
#else
    return sfs_get_disk_num( path );
#endif
}

static const char* fdialog_get_work_path()
{
#ifdef ONLY_WORKING_DIR
    return "1:/";
#else
    return sfs_get_work_path();
#endif
}

static char* fdialog_make_final_path( fdialog_data* data )
{
    window_manager* wm = data->win->wm;
    data->final_path[ 0 ] = 0;
    SMEM_STRCAT_D( data->final_path, fdialog_get_disk_name( data->prop_cur_disk ) );
    if( data->prop_path[ data->prop_cur_disk ] )
    {
        SMEM_STRCAT_D( data->final_path, data->prop_path[ data->prop_cur_disk ] );
    }
    return data->final_path;
}

static char* fdialog_make_final_filename( fdialog_data* data )
{
    window_manager* wm = data->win->wm;
    data->final_filename[ 0 ] = 0;
    char* fname = text_get_text( data->name, wm );
    if( fname )
    {
	SMEM_STRCAT_D( data->final_filename, fdialog_get_disk_name( data->prop_cur_disk ) );
	if( data->prop_path[ data->prop_cur_disk ] )
	{
	    SMEM_STRCAT_D( data->final_filename, data->prop_path[ data->prop_cur_disk ] );
	}
	SMEM_STRCAT_D( data->final_filename, fname );
    }
    return data->final_filename;
}

static char* fdialog_fix_list_item( const char* item_name )
{
    if( !item_name ) return NULL;
    size_t len = smem_strlen( item_name );
    char* new_item = SMEM_STRDUP( item_name );
    if( new_item )
    {
	for( size_t i = 0; i < len; i++ )
	{
	    if( new_item[ i ] == '\t' )
	    {
		new_item[ i ] = 0;
		break;
	    }
        }
    }
    return new_item;
}

static char* fdialog_fix_name( char* in_name, const char* mask )
{
    char* out_name = NULL;
    int mask_extensions = 0;
    int mask_cur_ext = 0;
    if( mask )
    {
	mask_extensions = 1;
	for( size_t i = 0; i < smem_strlen( mask ); i++ )
	{
	    if( mask[ i ] == '/' ) mask_extensions++;
	}
    }
    if( in_name && mask )
    {
	int a;
	bool ext_eq = 0;
	for( a = smem_strlen( in_name ) - 1; a >= 0; a-- )
	    if( in_name[ a ] == '.' ) break;
	if( a >= 0 )
	{
	    //Dot found:
	    while( 1 )
	    {
		size_t p;
		int extnum = 0;
		for( p = 0; p < smem_strlen( mask ); p++ )
		{
		    if( extnum == mask_cur_ext ) break;
		    if( mask[ p ] == '/' ) 
		    {
			extnum++;
		    }
		}
		ext_eq = 1;
		for( int i = a + 1 ; ; i++, p++ )
		{
		    int c = in_name[ i ];
		    if( c >= 65 && c <= 90 ) c += 0x20; //Make small leters
		    int m = mask[ p ];
		    if( m == '/' ) m = 0;
		    if( c != m )
		    {
			ext_eq = 0;
			break;
		    }
		    if( c == 0 ) break;
		    if( m == 0 ) break;
		}
		if( ext_eq == 0 )
		{
		    //File extension is not equal to extension from mask:
		    if( mask_cur_ext < mask_extensions - 1 )
		    {
			//But there are another extensions in mask. Lets try them too:
			mask_cur_ext++;
			continue;
		    }
		}
		break;
	    }
	}
	if( ext_eq == 0 ) 
	{
	    //Add extension:
	    out_name = SMEM_ALLOC2( char, smem_strlen( in_name ) + 32 );
	    out_name[ 0 ] = 0;
	    SMEM_STRCAT_D( out_name, in_name );
	    a = smem_strlen( in_name );
	    out_name[ a ] = '.';
	    a++;
	    int p = 0;
	    for(;;)
	    {
		out_name[ a ] = mask[ p ];
		if( out_name[ a ] == '/' ||
		    out_name[ a ] == 0 )
		{
		    out_name[ a ] = 0;
		    break;
		}
		p++;
		a++;
	    }
	}
    }
    if( out_name == NULL )
	out_name = SMEM_STRDUP( in_name );
    return out_name;
}

static uint32_t fdialog_pos_time()
{
    return stime_time() / 60; //seconds -> minutes
}

static void fdialog_pos_save_table( fdialog_data* data )
{
    int err = 0;
    sfs_file f = sfs_open( data->props_file_dir_pos, "wb" );
    if( f == 0 ) return;
    ssymtab_iterator si;
    ssymtab_iterator_init( &si, data->dir_pos );
    while( 1 )
    {
	ssymtab_item* item = ssymtab_iterator_next( &si );
	if( !item ) break;
	if( !item->name ) continue;
	if( item->type == 0 ) continue; //removed
	if( item->val.i <= 0 ) continue; //wrong position
	int path_size = smem_get_size( item->name );
	if( path_size == 0 ) continue;
	if( path_size >= 32768 ) { err = 1; break; } //path size is too large?
	int path_len = path_size - 2; //without last "/" and null terminator

	//Modification date+time:
	int time = item->type;
	int w = sfs_write( &time, 1, sizeof( time ), f );
	if( w != sizeof( time ) ) { err = 2; break; }

	//Position:
	w = sfs_write_varlen_uint32( item->val.i, f );
	if( w <= 0 ) { err = 3; break; }

	//Path length:
	w = sfs_write_varlen_uint32( path_len, f );
	if( w <= 0 ) { err = 4; break; }

	//Path:
	w = sfs_write( item->name, 1, path_len, f );
	if( w != path_len ) { err = 5; break; }
    }
    ssymtab_iterator_deinit( &si );
    sfs_close( f );
    if( err )
    {
	slog( "fdialog_pos_save_table() error %d\n", err );
	sfs_remove_file( data->props_file_dir_pos );
    }
}

static void fdialog_pos_load_table( fdialog_data* data )
{
    int err = 0;
    sfs_file f = sfs_open( data->props_file_dir_pos, "rb" );
    if( f == 0 ) return;
    int r = 0;
    char* path_buf = nullptr;
    while( 1 )
    {
	//Modification date+time:
	int time = 0;
	r = sfs_read( &time, 1, sizeof( time ), f );
	if( r == 0 ) break; //EOF
	if( r != sizeof( time ) ) { err = 1; break; }

	//Position:
	int pos = sfs_read_varlen_uint32( &r, f );
	if( r == 0 ) { err = 2; break; }

	//Path length:
	int path_len = sfs_read_varlen_uint32( &r, f );
	if( r == 0 ) { err = 3; break; }
	if( path_len >= 32768 ) { err = 4; break; } //path size is too large?
	int path_size = path_len + 2; //plus "/" and null terminator
	path_buf = SMEM_SMART_ZRESIZE2( path_buf, char, path_size, 32 );
	path_buf[ path_size - 1 ] = 0;
	path_buf[ path_size - 2 ] = '/';

	//Path:
	r = sfs_read( path_buf, 1, path_len, f );
	if( r != path_len ) { err = 5; break; }

	//Save to the hash table:
	SSYMTAB_VAL v;
	v.i = 0;
	bool created = false;
	ssymtab_item* sym = ssymtab_lookup( path_buf, -1, 1, 0, v, &created, data->dir_pos );
	if( sym )
	{
	    sym->type = time;
	    sym->val.i = pos;
	}

	//printf( "%s: %d\n", path_buf, pos );
    }
    int file_size = (int)sfs_tell( f );
    sfs_close( f );
    smem_free( path_buf );
    if( err )
    {
	slog( "fdialog_pos_load_table() error %d\n", err );
	sfs_remove_file( data->props_file_dir_pos );
    }

    //Cleanup:
    int file_size_limit = sconfig_get_int_value( APP_CFG_FDIALOG_DIR_POS_FILESIZE_LIMIT, 1024 * 128, 0 );
    if( file_size > file_size_limit ) file_size_limit /= 2;
    while( file_size > file_size_limit )
    {
	uint32_t oldest_time = 0xFFFFFFFF;
	ssymtab_item* oldest_dir = nullptr;

        ssymtab_iterator si;
        ssymtab_iterator_init( &si, data->dir_pos );
        while( 1 )
	{
	    ssymtab_item* item = ssymtab_iterator_next( &si );
	    if( !item ) break;
	    if( item->type == 0 ) continue; //removed
	    if( (uint32_t)item->type < oldest_time )
	    {
		oldest_time = item->type;
		oldest_dir = item;
	    }
	}
	ssymtab_iterator_deinit( &si );
	if( oldest_dir )
	{
	    int path_len = smem_strlen( oldest_dir->name ) - 1;
	    file_size -= path_len + varlen_uint32_size( path_len ) + varlen_uint32_size( oldest_dir->val.i ) + 4;
	    oldest_dir->type = 0; //removed
	    slog( " - %s; %d\n", oldest_dir->name, file_size );
	}
	else break;
    }
}

static void fdialog_pos_init( fdialog_data* data )
{
    data->dir_pos = ssymtab_new( 389 );

    fdialog_pos_load_table( data );
}

static void fdialog_pos_deinit( fdialog_data* data )
{
    fdialog_pos_save_table( data );

    ssymtab_delete( data->dir_pos );
}

//Associate the file position for the current directory:
static void fdialog_pos_store( fdialog_data* data )
{
    window_manager* wm = data->win->wm;

    char* final_path = fdialog_make_final_path( data );
    char* short_path = nullptr;
    char* ts = nullptr;
    while( 1 )
    {
	if( !final_path ) break;
	if( final_path[ 0 ] == 0 ) break;
	short_path = sfs_make_filename( wm->sd, final_path, false ); //X:/dir/

	//printf( "STORE %s %d\n", short_path, data->prop_cur_file[ data->prop_cur_disk ] );

	SSYMTAB_VAL v;
	v.i = 0;
	bool created = false;
	ssymtab_item* sym = ssymtab_lookup( short_path, -1, 1, 0, v, &created, data->dir_pos );
	if( sym )
	{
	    sym->type = fdialog_pos_time();
	    sym->val.i = data->prop_cur_file[ data->prop_cur_disk ];
	}

	break;
    }
    smem_free( short_path );
    smem_free( ts );
}

//Restore the file position for the current directory:
static void fdialog_pos_restore( fdialog_data* data )
{
    data->prop_cur_file[ data->prop_cur_disk ] = -1;

    window_manager* wm = data->win->wm;

    char* final_path = fdialog_make_final_path( data );
    char* short_path = nullptr;
    while( 1 )
    {
	if( !final_path ) break;
	if( final_path[ 0 ] == 0 ) break;
	short_path = sfs_make_filename( wm->sd, final_path, false ); //X:/dir/

	//printf( "RESTORE %s\n", short_path );

	int pos = -1;
	SSYMTAB_VAL v;
	v.i = 0;
	bool created = false;
	ssymtab_item* sym = ssymtab_lookup( short_path, -1, 0, 0, v, &created, data->dir_pos );
	if( sym )
	{
	    data->prop_cur_file[ data->prop_cur_disk ] = sym->val.i;
	}

	break;
    }
    smem_free( short_path );
}

static char* fdialog_edit_get_menu( fdialog_data* data )
{
    char* ts = SMEM_ALLOC2( char, 4096 );
    snprintf( ts, 4096, "%s\n%s\n%s\n%s\n%s\n%s\n%s", 
        wm_get_string( STR_WM_DELETE ),
        wm_get_string( STR_WM_RENAME ),
        wm_get_string( STR_WM_CUT ),
        wm_get_string( STR_WM_COPY ),
        wm_get_string( STR_WM_PASTE ),
        wm_get_string( STR_WM_CREATE_DIR ),
        wm_get_string( STR_WM_DELETE_CUR_DIR ) );
    return ts;
}

//Delete file:
static int del_file_dialog_action_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    WINDOWPTR fdialog_win = (WINDOWPTR)user_data;
    if( !is_fdialog_win_valid( fdialog_win ) ) return 1; //close
    fdialog_data* data = (fdialog_data*)fdialog_win->data;
    char* fname = (char*)get_data_container2( win, "filename" );
    if( win->action_result == 0 && fname && fname[ 0 ] != 0 )
    {
	if( !wm->fdialog_delfile_confirm && !strstr( win->name, wm_get_string( STR_WM_ARE_YOU_SURE ) ) )
	{
    	    char* ts = SMEM_ALLOC2( char, 256 );
	    ts[ 0 ] = 0;
	    SMEM_STRCAT_D( ts, "!" );
	    SMEM_STRCAT_D( ts, wm_get_string( STR_WM_ARE_YOU_SURE ) );
	    SMEM_STRCAT_D( ts, "\n" );
	    SMEM_STRCAT_D( ts, win->name + 1 );

	    WINDOWPTR d = dialog_open( NULL, ts, wm_get_string( STR_WM_YESNO ), 0, wm );
	    if( d )
	    {
		add_data_container( d->childs[ 0 ], "filename", SMEM_STRDUP( fname ) );
		set_handler( d->childs[ 0 ], del_file_dialog_action_handler, fdialog_win, wm );
	    }

	    smem_free( ts );
	}
	else
	{
	    wm->fdialog_delfile_confirm = true;
	    if( !strstr( win->name, wm_get_string( STR_WM_DELETE_DIR2 ) ) )
	    {
		slog( "Deleting file: %s\n", fname );
		slog( "Ret.val = %d\n", sfs_remove_file( fname ) );
	    }
	    else
	    {
    		slog( "Deleting directory: %s\n", fname );
		slog( "Ret.val = %d\n", sfs_remove( fname ) );
	    }
	    fdialog_refresh_list( data );
	}
    }
    return 1; //close
}

//Overwrite:
static int rename_file_dialog_action_handler2( void* user_data, WINDOWPTR win, window_manager* wm )
{
    WINDOWPTR fdialog_win = (WINDOWPTR)user_data;
    if( !is_fdialog_win_valid( fdialog_win ) ) return 1; //close
    fdialog_data* data = (fdialog_data*)fdialog_win->data;
    if( win->action_result == 0 )
    {
	char* new_name = (char*)get_data_container2( win, "filename" );
	char* prev_name = (char*)get_data_container2( win, "prevname" );
	if( new_name && prev_name )
	{
    	    sfs_rename( prev_name, new_name );
	    fdialog_refresh_list( data );
	}
    }
    return 1; //close
}

//Rename file:
static int rename_file_dialog_action_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    WINDOWPTR fdialog_win = (WINDOWPTR)user_data;
    if( !is_fdialog_win_valid( fdialog_win ) ) return 1; //close
    fdialog_data* data = (fdialog_data*)fdialog_win->data;
    dialog_item* ditems = dialog_get_items( win );
    dialog_item* new_name_item = dialog_get_item( ditems, 'name' );
    if( !new_name_item ) return 1;
    char* new_name = new_name_item->str_val;
    char* prev_name = (char*)get_data_container2( win, "filename" );
    if( win->action_result == 0 && new_name )
    {
        if( smem_strcmp( new_name, prev_name ) != 0 )
        {
            if( new_name[ 0 ] != 0 )
            {
		char* fixed_name = fdialog_fix_name( new_name, data->mask );
                char* new_name2 = SMEM_ALLOC2( char, smem_strlen( fixed_name ) + 8192 );
                new_name2[ 0 ] = 0;
                SMEM_STRCAT_D( new_name2, fdialog_get_disk_name( data->prop_cur_disk ) );
		if( data->prop_path[ data->prop_cur_disk ] )
		{
		    SMEM_STRCAT_D( new_name2, data->prop_path[ data->prop_cur_disk ] );
		}
		SMEM_STRCAT_D( new_name2, fixed_name );
		if( sfs_get_file_size( new_name2 ) )
		{
		    //Already exists:
		    WINDOWPTR d = fdialog_overwrite_open( fixed_name, wm );
		    if( d )
		    {
			add_data_container( d->childs[ 0 ], "filename", SMEM_STRDUP( new_name2 ) );
			add_data_container( d->childs[ 0 ], "prevname", SMEM_STRDUP( prev_name ) );
			set_handler( d->childs[ 0 ], rename_file_dialog_action_handler2, fdialog_win, wm );
		    }
		}
		else
		{
                    sfs_rename( prev_name, new_name2 );
                }
                smem_free( new_name2 );
        	smem_free( fixed_name );
		fdialog_refresh_list( data );
    	    }
        }
    }
    smem_free( new_name_item->str_val );
    new_name_item->str_val = NULL;
    return 1; //close
}

//Create directory:
static int mkdir_dialog_action_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    WINDOWPTR fdialog_win = (WINDOWPTR)user_data;
    if( !is_fdialog_win_valid( fdialog_win ) ) return 1; //close
    fdialog_data* data = (fdialog_data*)fdialog_win->data;
    dialog_item* ditems = dialog_get_items( win );
    dialog_item* dir_name_item = dialog_get_item( ditems, 'name' );
    if( !dir_name_item ) return 1;
    char* dir_name = dir_name_item->str_val;
    if( win->action_result == 0 && dir_name )
    {
        if( dir_name[ 0 ] != 0 )
        {
            char* new_name = SMEM_ALLOC2( char, smem_strlen( dir_name ) + 8192 );
            new_name[ 0 ] = 0;
            SMEM_STRCAT_D( new_name, fdialog_get_disk_name( data->prop_cur_disk ) );
	    if( data->prop_path[ data->prop_cur_disk ] )
	    {
	        SMEM_STRCAT_D( new_name, data->prop_path[ data->prop_cur_disk ] );
	    }
	    SMEM_STRCAT_D( new_name, dir_name );
#ifdef OS_UNIX
	    sfs_mkdir( new_name, S_IRWXU | S_IRWXG | S_IRWXO );
#else
	    sfs_mkdir( new_name, 0 );
#endif
            smem_free( new_name );
	    fdialog_refresh_list( data );
        }
    }
    smem_free( dir_name_item->str_val );
    dir_name_item->str_val = NULL;
    return 1; //close
}

//Delete directory:
static int deldir_dialog_action_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    WINDOWPTR fdialog_win = (WINDOWPTR)user_data;
    if( !is_fdialog_win_valid( fdialog_win ) ) return 1; //close
    fdialog_data* data = (fdialog_data*)fdialog_win->data;
    char* dir_name = (char*)get_data_container2( win, "dirname" );
    if( win->action_result == 0 && dir_name )
    {
	if( !wm->fdialog_deldir_confirm && !strstr( win->name, wm_get_string( STR_WM_ARE_YOU_SURE ) ) )
	{
    	    char* ts = SMEM_ALLOC2( char, 256 );
	    ts[ 0 ] = 0;
	    SMEM_STRCAT_D( ts, "!" );
	    SMEM_STRCAT_D( ts, wm_get_string( STR_WM_ARE_YOU_SURE ) );
	    SMEM_STRCAT_D( ts, "\n" );
	    SMEM_STRCAT_D( ts, win->name + 1 );

	    WINDOWPTR d = dialog_open( NULL, ts, wm_get_string( STR_WM_YESNO ), 0, wm );
	    if( d )
	    {
		add_data_container( d->childs[ 0 ], "dirname", SMEM_STRDUP( dir_name ) );
		set_handler( d->childs[ 0 ], deldir_dialog_action_handler, fdialog_win, wm );
	    }

	    smem_free( ts );
	}
	else
	{
	    wm->fdialog_deldir_confirm = true;
    	    slog( "Deleting directory: %s\n", dir_name );
	    int rv = sfs_remove( dir_name );
	    slog( "Ret.val = %d\n", rv );
	    if( rv == 0 )
	    {
		fdialog_disk_button_handler( data, data->go_up_button, wm );
	    }
	    fdialog_refresh_list( data );
	}
    }
    return 1; //close
}

static void fdialog_edit( fdialog_data* data, int op )
{
    window_manager* wm = data->win->wm;
    WINDOWPTR fdialog_win = data->win;
    bool dir_selected = false;
    char* dir_name = NULL; //dir
    char* dir_name2 = NULL; //full/path/dir
    slist_data* ldata = list_get_data( data->files, wm );
    if( slist_get_attr( ldata->selected_item, ldata ) == 1 ) dir_selected = true;
    char* res = NULL; //full path (static link)
    char* name = text_get_text( data->name, wm ); //file/dir name only (static link)
    switch( op )
    {
        case 0:
    	    //Delete file/dir:
	case 1:
	    //Rename file:
	case 2:
	    //Cut file:
	case 3:
	    //Copy file:
	    {
		if( ( op == 0 /*|| op == 1*/ ) && dir_selected )
		{
		    //Dir:
		    dir_name = fdialog_fix_list_item( slist_get_item( ldata->selected_item, ldata ) );
	    	    if( dir_name )
	    	    {
			dir_name2 = SMEM_ALLOC2( char, 32 ); dir_name2[ 0 ] = 0;
			if( dir_name2 )
			{
			    SMEM_STRCAT_D( dir_name2, fdialog_get_disk_name( data->prop_cur_disk ) );
	    		    if( data->prop_path[ data->prop_cur_disk ] )
	    		    {
				SMEM_STRCAT_D( dir_name2, data->prop_path[ data->prop_cur_disk ] );
	    		    }
	    		    SMEM_STRCAT_D( dir_name2, dir_name );
	    		    name = dir_name;
	    		    res = dir_name2;
			}
		    }
		}
		else
		{
		    //File:
	    	    if( name == NULL || name[ 0 ] == 0 )
	    	    {
	    	        dialog_open( NULL, wm_get_string( STR_WM_FILE_MSG_NONAME ), wm_get_string( STR_WM_OK ), 0, wm );
	    		return;
	    	    }
		    fdialog_name_handler( data, data->name, wm ); //Add file extension
		    res = fdialog_make_final_filename( data );
		    name = text_get_text( data->name, wm );
	    	}
	    }
	    break;
	default:
	    break;
    }
    switch( op )
    {
        case 0:
	    //Delete file/dir:
	    if( res && name )
	    {
	        char* ts = SMEM_ALLOC2( char, smem_strlen( name ) + 256 );
		ts[ 0 ] = 0;
		SMEM_STRCAT_D( ts, "!" );
		if( dir_selected )
		    SMEM_STRCAT_D( ts, wm_get_string( STR_WM_DELETE_DIR2 ) );
		else
		    SMEM_STRCAT_D( ts, wm_get_string( STR_WM_DELETE2 ) );
		SMEM_STRCAT_D( ts, " " );
		SMEM_STRCAT_D( ts, name );
		SMEM_STRCAT_D( ts, "?" );
		if( res[ 0 ] != 0 )
		{
		    WINDOWPTR d = dialog_open( NULL, ts, wm_get_string( STR_WM_YESNO ), 0, wm );
		    if( d )
		    {
			add_data_container( d->childs[ 0 ], "filename", SMEM_STRDUP( res ) );
			set_handler( d->childs[ 0 ], del_file_dialog_action_handler, fdialog_win, wm );
		    }
		}
		smem_free( ts );
	    }
	    break;
	case 1:
	    //Rename file:
	    if( res && name )
	    {
		dialog_item* dlist = NULL;
		while( 1 )
		{
		    dialog_item* di = NULL;
		    
		    di = dialog_new_item( &dlist ); if( !di ) break;
            	    di->type = DIALOG_ITEM_TEXT;
            	    di->str_val = name;
            	    di->id = 'name';
		    
		    wm->opt_dialog_items = dlist;
		    const char* dname = wm_get_string( STR_WM_RENAME_FILE2 );
		    //if( dir_selected ) dname = wm_get_string( STR_WM_RENAME_DIR2 );
            	    WINDOWPTR d = dialog_open( dname, NULL, wm_get_string( STR_WM_OKCANCEL ), 0, wm ); //retval = decorator
    		    if( !d ) break;
    		    
		    add_data_container( d->childs[ 0 ], "filename", SMEM_STRDUP( res ) );
		    add_data_container( d->childs[ 0 ], "prevname", SMEM_STRDUP( name ) );
    		    set_handler( d->childs[ 0 ], rename_file_dialog_action_handler, fdialog_win, wm );
	            dialog_set_flags( d, DIALOG_FLAG_AUTOREMOVE_ITEMS );
	            dlist = NULL; //because we use DIALOG_FLAG_AUTOREMOVE_ITEMS
        
		    break;
		}
		smem_free( dlist );
	    }
	    break;
	case 2:
	    //Cut file:
	case 3:
	    //Copy file:
	    if( res && name )
	    {
	        smem_free( wm->fdialog_copy_file_name );
	        smem_free( wm->fdialog_copy_file_name2 );
	        wm->fdialog_copy_file_name = (char*)SMEM_STRDUP( res );
	        wm->fdialog_copy_file_name2 = (char*)SMEM_STRDUP( name );
	        if( op == 2 )
	    	    wm->fdialog_cut_file_flag = true;
		else
		    wm->fdialog_cut_file_flag = false;
	    }
	    break;
	case 4:
	    //Paste file:
	    if( wm->fdialog_copy_file_name )
	    {
        	char* new_name = SMEM_ALLOC2( char, smem_strlen( wm->fdialog_copy_file_name ) + 8192 );
        	new_name[ 0 ] = 0;
            	SMEM_STRCAT_D( new_name, fdialog_get_disk_name( data->prop_cur_disk ) );
                if( data->prop_path[ data->prop_cur_disk ] )
                {
                    SMEM_STRCAT_D( new_name, data->prop_path[ data->prop_cur_disk ] );
	        }
	        SMEM_STRCAT_D( new_name, wm->fdialog_copy_file_name2 );
	        while( sfs_get_file_size( new_name ) )
	        {
	    	    //Already exists:
	    	    int i = (int)smem_strlen( new_name );
	    	    bool dot_found = 0;
	    	    for( ; i >= 0; i-- )
	    	    {
	    	        if( new_name[ i ] == '.' ) { dot_found = 1; break; }
	        	if( new_name[ i ] == '/' ) break;
	    	    }
	    	    if( dot_found )
	    	    {
	    	        int i2 = (int)smem_strlen( new_name ) + 1;
	    	        for( ; i2 > i; i2-- )
	    	        {
	    		    new_name[ i2 ] = new_name[ i2 - 1 ];
	        	}
	        	new_name[ i ] = '_';
	    	    }
	    	    else
	    	    {
	        	SMEM_STRCAT_D( new_name, "_" );
	    	    }
	        }
    		sfs_copy_file( new_name, wm->fdialog_copy_file_name );
		if( wm->fdialog_cut_file_flag ) sfs_remove_file( wm->fdialog_copy_file_name );
            	smem_free( new_name );

		fdialog_refresh_list( data );

		smem_free( wm->fdialog_copy_file_name );
		smem_free( wm->fdialog_copy_file_name2 );
		wm->fdialog_copy_file_name = 0;
		wm->fdialog_copy_file_name2 = 0;
	    }
	    break;
	case 5:
	    //Create directory:
	    {
		dialog_item* dlist = NULL;
		while( 1 )
		{
		    dialog_item* di = NULL;
		    
		    di = dialog_new_item( &dlist ); if( !di ) break;
            	    di->type = DIALOG_ITEM_TEXT;
            	    di->str_val = NULL;
            	    di->id = 'name';
		    
		    wm->opt_dialog_items = dlist;
            	    WINDOWPTR d = dialog_open( wm_get_string( STR_WM_CREATE_DIR ), NULL, wm_get_string( STR_WM_OKCANCEL ), 0, wm ); //retval = decorator
    		    if( !d ) break;
    		    
    		    set_handler( d->childs[ 0 ], mkdir_dialog_action_handler, fdialog_win, wm );
	            dialog_set_flags( d, DIALOG_FLAG_AUTOREMOVE_ITEMS );
	            dlist = NULL; //because we use DIALOG_FLAG_AUTOREMOVE_ITEMS
        
		    break;
		}
		smem_free( dlist );
	    }
	    break;
	case 6:
	    //Delete current directory:
	    {
	        char* p = data->prop_path[ data->prop_cur_disk ];
		if( p && p[ 0 ] != 0 )
		{
		    char* path = SMEM_STRDUP( p );
		    int size = strlen( path );
		    if( path[ size - 1 ] == '/' ) { path[ size - 1 ] = 0; size--; }
		    if( size > 0 )
		    {
		        int i = (int)size - 2;
		        for( ; i > 0; i-- )
		        {
		            if( path[ i ] == '/' ) { i++; break; }
		        }
		        char* dir_name = path + i;

		        char* ts = SMEM_ALLOC2( char, smem_strlen( dir_name ) + 256 );
	    		ts[ 0 ] = 0;
			SMEM_STRCAT_D( ts, "!" );
			SMEM_STRCAT_D( ts, wm_get_string( STR_WM_DELETE_CUR_DIR2 ) );
			SMEM_STRCAT_D( ts, " (" );
			SMEM_STRCAT_D( ts, dir_name );
			SMEM_STRCAT_D( ts, ") " );
			SMEM_STRCAT_D( ts, wm_get_string( STR_WM_RECURS ) );
			SMEM_STRCAT_D( ts, "?" );

			WINDOWPTR d = dialog_open( NULL, ts, wm_get_string( STR_WM_YESNO ), 0, wm );
			if( d )
			{
			    char* full_path = SMEM_ALLOC2( char, size + 1024 );
			    full_path[ 0 ] = 0;
			    SMEM_STRCAT_D( full_path, fdialog_get_disk_name( data->prop_cur_disk ) );
			    SMEM_STRCAT_D( full_path, path );
			    add_data_container( d->childs[ 0 ], "dirname", SMEM_STRDUP( full_path ) );
			    set_handler( d->childs[ 0 ], deldir_dialog_action_handler, fdialog_win, wm );
		    	    smem_free( full_path );
			}

			smem_free( ts );
		    }
		    smem_free( path );
		}
	    }
	    break;
	default:
	    break;
    }
    data->final_filename[ 0 ] = 0;
    smem_free( dir_name );
    smem_free( dir_name2 );
}

static void fdialog_refresh_list( fdialog_data* data )
{
    window_manager* wm = data->win->wm;
    if( !data->files ) return;
    slist_data* ldata = list_get_data( data->files, wm );
    slist_deinit( ldata );
    slist_init( ldata );
    sfs_find_struct fs;
    SMEM_CLEAR_STRUCT( fs );
    const char* disk_name = fdialog_get_disk_name( data->prop_cur_disk );
    char* path = data->prop_path[ data->prop_cur_disk ];
    char* res = SMEM_ALLOC2( char, smem_strlen( disk_name ) + smem_strlen( path ) + 1 );
    res[ 0 ] = 0;
    SMEM_STRCAT_D( res, disk_name );
    SMEM_STRCAT_D( res, path );
    fs.opt |= SFS_FIND_OPT_FILESIZE;
    fs.start_dir = res;
    fs.mask = data->mask;
    if( sfs_find_first( &fs ) )
    {
	char* temp_name = SMEM_ALLOC2( char, MAX_DIR_LEN );
	while( 1 )
	{
	    bool show = true;
	    if( fs.name[ 0 ] == '.' && !data->show_hidden ) show = false;
	    if( show )
	    {
		if( fs.type == SFS_FILE )
		{
	    	    size_t name_len = smem_strlen( fs.name );
	    	    size_t file_size = fs.size;

	    	    while( 1 )
	    	    {
	    	        if( file_size > 1024 * 1024 * 1024 )
			{
		    	    sprintf( temp_name, "%s\t%dG", fs.name, (int)( file_size / ( 1024 * 1024 * 1024 ) ) );
		    	    break;
			}
			if( file_size > 1024 * 1024 )
			{
		    	    sprintf( temp_name, "%s\t%dM", fs.name, (int)( file_size / ( 1024 * 1024 ) ) );
		    	    break;
			}
			if( file_size > 1024 )
			{
		    	    sprintf( temp_name, "%s\t%dK", fs.name, (int)( file_size / 1024 ) );
		    	    break;
			}
			sprintf( temp_name, "%s\t%dB", fs.name, (int)file_size );
			break;
		    }
		    slist_add_item( temp_name, 0, ldata );
		}
		else
		{
		    if( fs.name[ 0 ] == '.' && fs.name[ 1 ] == 0 ) show = false;
		    if( fs.name[ 0 ] == '.' && fs.name[ 1 ] == '.' ) show = false;
		    if( show ) slist_add_item( fs.name, 1, ldata );
		}
	    }
	    if( sfs_find_next( &fs ) == 0 ) break;
	}
	smem_free( temp_name );
    }
    sfs_find_close( &fs );
    smem_free( res );
    slist_sort( ldata );

    //Set file num:
    list_select_item( data->files, data->prop_cur_file[ data->prop_cur_disk ], true );
}

static void fdialog_set_path( fdialog_data* data, const char* str )
{
    window_manager* wm = data->win->wm;
/*#ifdef ONLY_WORKING_DIR
    const char* work = fdialog_get_work_path();
    printf( "%s\n%s\n", work, str );
    if( smem_strstr( str, work ) == str )
	str += smem_strlen( work );
#endif*/
    text_set_text( data->path, str, wm );
}

static void fdialog_refresh_ui( fdialog_data* data )
{
    window_manager* wm = data->win->wm;

    //Selected disk:
    for( int i = 0; i < fdialog_get_disk_count(); i++ )
        if( data->disk_buttons[ i ] )
    	    data->disk_buttons[ i ]->color = wm->button_color;
    if( data->disk_buttons[ data->prop_cur_disk ] )
        data->disk_buttons[ data->prop_cur_disk ]->color = BUTTON_HIGHLIGHT_COLOR;

    //Path:
    if( data->prop_path[ data->prop_cur_disk ] )
	fdialog_set_path( data, data->prop_path[ data->prop_cur_disk ] );
    else
	fdialog_set_path( data, "" );

    //Filelist:
    fdialog_refresh_list( data );
}

static void fdialog_jump_to_path( fdialog_data* data, const char* path )
{
    fdialog_pos_store( data );
#ifndef ONLY_WORKING_DIR
    window_manager* wm = data->win->wm;
    char* path2 = sfs_make_filename( wm->sd, path, true );
    if( !path2 ) return;
#else
    const char* path2 = path;
#endif
    while( 1 )
    {
        int d = fdialog_get_disk_num( path2 );
	if( d < 0 ) 
	{
	    slog( "Can't get disk number from: %s\n", path2 );
	    break;
	}
    
        //Get current directory:
	const char* cdir = path2;
        if( cdir && cdir[ 0 ] )
	{
    	    size_t s = smem_strlen( cdir );
    	    if( s > 5 ) s = 5;
	    for( size_t i = 0; i < s; i++ )
    	    {
    		if( cdir[ i ] == ':' )
    		{
    	    	    cdir += i + 1;
    	    	    break;
    		}
	    }
	    if( smem_strlen( cdir ) > 0 )
    	    {
    		if( cdir[ 0 ] == '/' )
    		{
        	    cdir++;
        	}
    	    }
	}
	else
	{
	    cdir = (char*)"";
	}

	//Set current disk and directory:
	data->prop_cur_disk = d;
	if( data->prop_path[ data->prop_cur_disk ] )
	    smem_free( data->prop_path[ data->prop_cur_disk ] );
	data->prop_path[ data->prop_cur_disk ] = SMEM_STRDUP( cdir );
    
	break;
    }
#ifndef ONLY_WORKING_DIR
    smem_free( path2 );
#endif
}

//
// Remove / Load / Save file dialog properties
//

static char* fdialog_make_props_filename( const char* id )
{
    const char* conf_path = sfs_get_conf_path();
    size_t conf_path_len = smem_strlen( conf_path );
    size_t id_len = smem_strlen( id );
    char* rv = SMEM_ALLOC2( char, conf_path_len + id_len + 1 );
    rv[ 0 ] = 0;
    SMEM_STRCAT_D( rv, conf_path );
    SMEM_STRCAT_D( rv, id );
    return rv;
}

static void fdialog_remove_props( fdialog_data* data )
{
    for( int i = 0; i < MAX_DISKS; i++ )
    {
	smem_free( data->prop_path[ i ] );
        data->prop_path[ i ] = nullptr;
        data->prop_cur_file[ i ] = 0;
    }
}

static bool fdialog_load_string( sfs_file f, char* str, uint size )
{
    bool eof = false;
    str[ size - 1 ] = 0;
    uint p = 0;
    while( 1 )
    {
	int c = sfs_getc( f );
	if( sfs_eof( f ) ) { c = 0; eof = true; }
	if( p < size - 1 )
	{
	    str[ p ] = c;
	    p++;
	}
	if( c == 0 ) break;
    }
    return eof;
}

static void fdialog_load_props( fdialog_data* data )
{
    fdialog_remove_props( data );
    fdialog_jump_to_path( data, fdialog_get_work_path() );
    
    if( data->props_file == 0 ) return;

    sfs_file f = sfs_open( data->props_file, "rb" );
    if( f == 0 ) return;
    
    uint temp_str_size = 4096;
    char* temp_str = SMEM_ALLOC2( char, temp_str_size );

    //Current disk:
    fdialog_load_string( f, temp_str, temp_str_size );
    int d = fdialog_get_disk_num( temp_str );
    if( d >= 0 ) 
        data->prop_cur_disk = d;

    //Disks info:
    for( int i = 0; i < MAX_DISKS; i++ )
    {
	if( fdialog_load_string( f, temp_str, temp_str_size ) ) break; //disk name
        d = fdialog_get_disk_num( temp_str ); //disk num
	fdialog_load_string( f, temp_str, temp_str_size ); //path on this disk
	int file_num; sfs_read( &file_num, 4, 1, f ); //file number on this disk
	if( d >= 0 )
	{
	    if( data->prop_path[ d ] )
	    {
	        //Remove old path:
	        smem_free( data->prop_path[ d ] );
	        data->prop_path[ d ] = 0;
	    }
	    if( temp_str[ 0 ] )
	    {
	        //Set new path:
	        data->prop_path[ d ] = SMEM_STRDUP( temp_str );
	    }
	    data->prop_cur_file[ d ] = file_num;
	}
    }

    smem_free( temp_str );
    sfs_close( f );
}

static void fdialog_save_props( fdialog_data* data )
{
    if( data->props_file == 0 ) return;
    
    sfs_file f = sfs_open( data->props_file, "wb" );
    if( f == 0 ) return;

    //Save current disk:
    char* path;
    const char* disk_name = fdialog_get_disk_name( data->prop_cur_disk );
    sfs_write( disk_name, 1, smem_strlen( disk_name ) + 1, f );

    //Save other properties:
    for( int a = 0; a < fdialog_get_disk_count(); a++ )
    {
        //Disk name:
        disk_name = fdialog_get_disk_name( a );
        sfs_write( disk_name, 1, smem_strlen( disk_name ) + 1, f );
        //Current path:
        if( data->prop_path[ a ] == 0 )
	    sfs_putc( 0, f );
	else
	{
	    path = data->prop_path[ a ];
	    sfs_write( path, 1, smem_strlen( path ) + 1, f );
	}
	//Selected file num:
	sfs_write( &data->prop_cur_file[ a ], 4, 1, f );
    }

    sfs_close( f );
}

//
// Recent files/dirs
//

static int fdialog_load_recent_list( fdialog_data* data, slist_data* l, const char* filename )
{
    int rv = -1;
    slist_deinit( l );
    slist_init( l );
    if( slist_load( filename, l ) != 0 )
    {
	slist_deinit( l );
	slist_init( l );
    }
    else
    {
	rv = 0;
    }
    return rv;
}

static void fdialog_fill_recent_window( fdialog_data* data, WINDOWPTR dest, slist_data* src, uint8_t attr )
{
    window_manager* wm = data->win->wm;
    slist_data* ldest = list_get_data( dest, wm );
    slist_deinit( ldest );
    slist_init( ldest );
    uint num = slist_get_items_num( src );
    if( attr == 0 )
    {
	//Files:
	for( uint i = 0; i < num; i++ )
	{
	    const char* item = slist_get_item( i, src );
	    item = sfs_get_filename_without_dir( item );
	    slist_add_item( item, attr, ldest );
	}
    }
    else
    {
	//Dirs:
	size_t tslen = MAX_DIR_LEN;
	char* ts = SMEM_ALLOC2( char, tslen );
	if( ts == 0 ) return;
	for( uint i = 0; i < num; i++ )
	{
	    const char* item = slist_get_item( i, src );
	    ts[ 0 ] = 0;
	    smem_strcat( ts, tslen, item );
	    size_t len = smem_strlen( ts );
	    int offset = 0;
	    if( len > 0 )
	    {
		if( ts[ len - 1 ] == '/' )
		    ts[ len - 1 ] = 0;
	    }
	    if( len >= 3 )
	    {
		if( ts[ 0 ] >= '0' && ts[ 0 ] <= '9' && ts[ 1 ] == ':' && ts[ 2 ] == '/' )
		{
		    offset = 3;
		}
	    }
	    slist_add_item( ts + offset, attr, ldest );
	}
	smem_free( ts );
    }
}

//
// Window handlers
//

static void fdialog_preview_changed( fdialog_data* data )
{
    window_manager* wm = data->win->wm;
    if( data->preview && data->fdialog_preview_flag )
    {
	data->preview_data.name = fdialog_make_final_filename( data );
	if( data->preview_data.name )
	{
	    data->preview_data.status = FPREVIEW_FILE_SELECTED;
	    data->preview_handler( &data->preview_data, wm );
	    data->preview_data.name[ 0 ] = 0;
	}
    }
}

int fdialog_recent_files_list_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    fdialog_data* data = (fdialog_data*)user_data;
    slist_data* ldata = list_get_data( win, wm );
    int last_action = list_get_last_action( win, wm );
    if( ldata == 0 || last_action == 0 ) return 0;
    if( last_action == LIST_ACTION_ESCAPE )
    {
        //ESCAPE KEY:
        send_event( data->win, EVT_CLOSEREQUEST, wm );
        return 0;
    }
    uint num = slist_get_items_num( ldata );
    int sel = ldata->selected_item;
    if( sel < 0 || sel >= (int)num ) return 0;
    if( last_action == LIST_ACTION_CLICK )
    {
	const char* item = slist_get_item( sel, &data->recent_files_data );
	if( item )
	{
	    char* path = sfs_get_filename_path( item ); //X:/dir/
	    if( path )
	    {
		fdialog_jump_to_path( data, path );
		fdialog_pos_restore( data );
		fdialog_refresh_ui( data );

		const char* filename = sfs_get_filename_without_dir( item );
		if( filename )
		{
		    slist_data* fl = list_get_data( data->files, wm );
		    uint fl_num = slist_get_items_num( fl );
		    for( uint i = 0; i < fl_num; i++ )
		    {
			bool found = false;
			if( slist_get_attr( i, fl ) == 0 )
			{
			    //File:
			    const char* fl_name = slist_get_item( i, fl );
			    if( fl_name )
			    {
		    		char* fl_name2 = fdialog_fix_list_item( fl_name );
		    		if( fl_name2 )
		    		{
		    		    if( smem_strcmp( fl_name2, filename ) == 0 )
		    		    {
		    			found = true;
		    		    }
		    		    smem_free( fl_name2 );
		    		}
		    	    }
		    	}
		    	if( found )
		    	{
			    data->prop_cur_file[ data->prop_cur_disk ] = i;
			    list_select_item( data->files, i, true );
			    text_set_text( data->name, filename, wm );
			    fdialog_preview_changed( data );
		    	    break;
		    	}
		    }
		}

		fdialog_save_props( data );
		draw_window( data->win, wm );
	    }
	    smem_free( path );
	}
    }
    if( last_action == LIST_ACTION_ENTER || 
	last_action == LIST_ACTION_DOUBLECLICK )
    {
	fdialog_ok_button_handler( data, data->ok_button, wm );
    }
    return 0;
}

int fdialog_recent_dirs_list_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    fdialog_data* data = (fdialog_data*)user_data;
    slist_data* ldata = list_get_data( win, wm );
    int last_action = list_get_last_action( win, wm );
    if( ldata == 0 || last_action == 0 ) return 0;
    if( last_action == LIST_ACTION_ESCAPE )
    {
        //ESCAPE KEY:
        send_event( data->win, EVT_CLOSEREQUEST, wm );
        return 0;
    }
    uint num = slist_get_items_num( ldata );
    int sel = ldata->selected_item;
    if( sel < 0 || sel >= (int)num ) return 0;
    if( last_action == LIST_ACTION_ENTER || 
	last_action == LIST_ACTION_CLICK ||
	last_action == LIST_ACTION_DOUBLECLICK )
    {
	const char* item = slist_get_item( sel, &data->recent_dirs_data );
	if( item )
	{
	    fdialog_jump_to_path( data, item );
	    fdialog_pos_restore( data );
	    fdialog_refresh_ui( data );
	    fdialog_save_props( data );
	    draw_window( data->win, wm );
	}
    }
    return 0;
}

static int fdialog_menu_action_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    WINDOWPTR fdialog_win = (WINDOWPTR)user_data;
    if( !is_fdialog_win_valid( fdialog_win ) ) return 1; //close
    fdialog_data* data = (fdialog_data*)fdialog_win->data;
    fdialog_edit( data, win->action_result );
    return 1;
}

int fdialog_list_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    fdialog_data* data = (fdialog_data*)user_data;
    slist_data* ldata = list_get_data( data->files, wm );
    int last_action = list_get_last_action( win, wm );
    if( ldata && last_action )
    {
	if( last_action == LIST_ACTION_ESCAPE )
	{
	    //ESCAPE KEY:
	    send_event( data->win, EVT_CLOSEREQUEST, wm );
	    return 0;
	}
	if( last_action == LIST_ACTION_BACK )
	{
	    //BACKSPACE/LEFT KEY (go to the parent directory):
	    fdialog_disk_button_handler( data, data->go_up_button, wm );
	    return 0;
	}
	bool subdir = false;
	bool preview_name = false;
	char* item = fdialog_fix_list_item( slist_get_item( ldata->selected_item, ldata ) );
	if( item )
	{
	    uint8_t attr = slist_get_attr( ldata->selected_item, ldata );
	    if( last_action == LIST_ACTION_UPDOWN )
	    {
		//UP/DOWN KEY:
		if( attr == 0 )
		{
		    preview_name = true;
		    text_set_text( data->name, item, wm );
		}
	    }
	    if( last_action == LIST_ACTION_ENTER ||
		last_action == LIST_ACTION_DOUBLECLICK )
	    {
		//ENTER/SPACE KEY:
		if( attr == 1 )
		{
		    //It's a dir:
		    subdir = true;
		}
		else
		{
		    //Select file:
		    if( last_action == LIST_ACTION_DOUBLECLICK )
			text_set_text( data->name, item, wm );
		    fdialog_ok_button_handler( data, data->ok_button, wm );
		    smem_free( item );
		    return 0;
		}
	    }
	    if( last_action == LIST_ACTION_CLICK ||
		last_action == LIST_ACTION_RCLICK )
	    {
		//MOUSE CLICK:
		if( attr == 1 )
		{
		    if( last_action == LIST_ACTION_CLICK )
		    {
			//Enter subdirectory:
			subdir = true;
		    }
    		}
		else
		{
		    preview_name = true;
		    text_set_text( data->name, item, wm );
		}
	    }
	    if( subdir )
	    {
		//Go to the subdir:
		data->prop_cur_file[ data->prop_cur_disk ] = ldata->selected_item;
		fdialog_pos_store( data );
		char* path = data->prop_path[ data->prop_cur_disk ];
		if( path )
		    path = SMEM_RESIZE2( path, char, smem_strlen( path ) + smem_strlen( item ) + 2 );
		else
		{
		    path = SMEM_ALLOC2( char, smem_strlen( item ) + 2 );
		    path[ 0 ] = 0;
		}
		SMEM_STRCAT_D( path, item );
		SMEM_STRCAT_D( path, "/" );
		data->prop_path[ data->prop_cur_disk ] = path;
		fdialog_pos_restore( data );
		fdialog_set_path( data, path );
		fdialog_refresh_list( data );
		draw_window( data->win, wm );
	    }
	    if( preview_name )
	    {
		fdialog_preview_changed( data );
	    }
	    smem_free( item );
	}
	if( !subdir )
	{
	    if( last_action == LIST_ACTION_RCLICK )
	    {
		char* ts = fdialog_edit_get_menu( data );
		WINDOWPTR pp = popup_menu_open( wm_get_string( STR_WM_EDIT ), ts, POPUP_MENU_POS_AUTO, POPUP_MENU_POS_AUTO, wm->menu_color, -1, wm );
		if( pp )
		    set_handler( pp, fdialog_menu_action_handler, data->win, wm );
		smem_free( ts );
	    }
	    data->prop_cur_file[ data->prop_cur_disk ] = ldata->selected_item;
	}
    }
    return 0;
}

int fdialog_disk_button_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    fdialog_data* data = (fdialog_data*)user_data;

    if( win == data->go_up_button )
    {
        //Go to the parent dir:
        fdialog_pos_store( data );
        char* path = data->prop_path[ data->prop_cur_disk ];
        if( path )
        {
    	    int p;
	    for( p = smem_strlen( path ) - 2; p >= 0; p-- )
	    {
	        if( path[ p ] == '/' ) { path[ p + 1 ] = 0; break; }
	    }
	    if( p < 0 ) path[ 0 ] = 0;
	}
	fdialog_pos_restore( data );
    }

    if( win == data->go_home_button )
    {
        //Go to home dir:
        fdialog_jump_to_path( data, fdialog_get_work_path() );
	fdialog_pos_restore( data );
    }

    for( int i = 0; i < 9 - ADD_VIRT_DISK; i++ )
    {
	WINDOWPTR b = data->addvd_buttons[ i ];
	if( !b ) break;
	if( b == win )
	{
    	    //Go to external storage:
    	    char ts[ 8 ];
    	    sprintf( ts, "%d:/", ADD_VIRT_DISK + i );
    	    char* path = sfs_make_filename( wm->sd, ts, true );
    	    if( path )
    	    {
    		fdialog_jump_to_path( data, path );
    		smem_free( path );
		fdialog_pos_restore( data );
	    }
	    break;
	}
    }

    for( int a = 0; a < fdialog_get_disk_count(); a++ )
    {
        if( data->disk_buttons[ a ] )
        {
    	    //data->disk_buttons[ a ]->color = wm->button_color;
	    if( data->disk_buttons[ a ] == win )
	        data->prop_cur_disk = a;
	}
    }

    fdialog_refresh_ui( data );

    //Save props:
    fdialog_save_props( data );
    draw_window( data->win, wm );

    return 0;
}

static bool fdialog_preview_onoff( fdialog_data* data, bool enable, bool save_status, bool save_config )
{
    window_manager* wm = data->win->wm;
    if( data->fdialog_preview_flag == enable ) return false;
    bool sconfig_changed = false;
    if( enable )
    {
	//Open preview:
	data->fdialog_preview_flag = true;
	if( save_status )
	{
    	    sconfig_set_int_value( APP_CFG_FDIALOG_PREVIEW, 1, 0 );
    	    sconfig_changed = true;
    	}
        int ysize = sconfig_get_int_value( APP_CFG_FDIALOG_PREVIEW_YSIZE, 0, 0 );
        ysize = scale_coord( ysize, false, wm );
        if( ysize > data->win->ysize * 0.9 )
    	    ysize = data->win->ysize * 0.9;
        data->preview_data.win->ysize = ysize;
	data->preview_button->color = BUTTON_HIGHLIGHT_COLOR;
	data->preview_data.status = FPREVIEW_OPEN;
    }
    else
    {
	//Close preview:
	data->fdialog_preview_flag = false;
	if( save_status )
	{
    	    sconfig_remove_key( APP_CFG_FDIALOG_PREVIEW, 0 );
    	}
        int ysize = scale_coord( data->preview_data.win->ysize, true, wm );
        sconfig_set_int_value( APP_CFG_FDIALOG_PREVIEW_YSIZE, ysize, 0 );
	data->preview_button->color = wm->button_color;
	data->preview_data.status = FPREVIEW_CLOSE;
	sconfig_changed = true;
    }
    if( sconfig_changed && save_config )
    {
	sconfig_save( 0 );
    }
    data->preview_handler( &data->preview_data, wm );
    return sconfig_changed;
}

int fdialog_preview_button_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    fdialog_data* data = (fdialog_data*)user_data;
    
    if( data->fdialog_preview_flag )
    {
	//Close preview:
	fdialog_preview_onoff( data, false, true, true );
    }
    else
    {
	//Open preview:
	fdialog_preview_onoff( data, true, true, true );
    }

    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    
    return 0;
}

static void fdialog_add_recent( slist_data* l, const char* props, const char* name, uint max )
{
    uint items_num = slist_get_items_num( l );
    bool item_found = false;
    for( uint i = 0; i < items_num; i++ )
    {
	char* item = slist_get_item( i, l );
	if( smem_strcmp( name, item ) == 0 )
	{
	    slist_move_item( i, 0, l );
	    item_found = true;
	    break;
	}
    }
    if( item_found == false )
    {
	int new_item = slist_add_item( name, 0, l );
	if( new_item >= 0 )
	{
	    slist_move_item( new_item, 0, l );
	    items_num++;
	    while( items_num > max )
	    {
		slist_delete_item( items_num - 1, l );
		items_num--;
	    }
	}
    }
    slist_save( props, l );
}

static void save_recent( fdialog_data* data )
{
    window_manager* wm = data->win->wm;
    char* short_name = sfs_make_filename( wm->sd, data->final_filename, false ); //X:/dir/file
    char* short_path = sfs_get_filename_path( short_name ); //X:/dir/
    if( short_name && short_path )
    {
        fdialog_add_recent( &data->recent_files_data, data->props_file_recent_files, short_name, sconfig_get_int_value( APP_CFG_FDIALOG_RECENT_MAXFILES, 32, 0 ) );
        int short_path_len = smem_strlen( short_path );
        if( short_path_len == 3 && short_path[ 0 ] >= '0' && short_path[ 0 ] <= '9' && short_path[ 1 ] == ':' && short_path[ 2 ] == '/' )
        {
    	    //Ignore "X:/"
        }
	else
	{
	    fdialog_add_recent( &data->recent_dirs_data, data->props_file_recent_dirs, short_path, sconfig_get_int_value( APP_CFG_FDIALOG_RECENT_MAXDIRS, 32, 0 ) );
	}
    }
    smem_free( short_name );
    smem_free( short_path );
}

static int fdialog_overwrite_action_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    WINDOWPTR fdialog_win = (WINDOWPTR)user_data;
    if( !is_fdialog_win_valid( fdialog_win ) ) return 1; //close
    fdialog_data* data = (fdialog_data*)fdialog_win->data;
    if( win->action_result == 0 )
    {
	char* fname = (char*)get_data_container2( win, "filename" );
	if( fname )
	{
	    data->final_filename[ 0 ] = 0;
	    SMEM_STRCAT_D( data->final_filename, fname );

	    save_recent( data );

	    fdialog_win->action_handler( fdialog_win->handler_data, fdialog_win, wm );

	    remove_window( fdialog_win, wm );
	    recalc_regions( wm );
	    draw_window( wm->root_win, wm );
	}
    }
    return 1; //close
}

int fdialog_ok_button_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    fdialog_data* data = (fdialog_data*)user_data;

    char* name = text_get_text( data->name, wm );

    if( win != data->edit_button )
    {
	//Just OK:

	if( name == NULL || name[ 0 ] == 0 )
	{
    	    dialog_open( NULL, wm_get_string( STR_WM_FILE_MSG_NONAME ), wm_get_string( STR_WM_OK ), 0, wm );
    	    return 0;
    	}

	fdialog_name_handler( data, data->name, wm ); //Add file extension
	fdialog_make_final_filename( data );
	name = NULL; //name is not valid here

    	if( data->flags & FDIALOG_FLAG_LOAD )
    	{
    	    if( sfs_get_file_size( data->final_filename ) == 0 )
    	    {
    		dialog_open( NULL, wm_get_string( STR_WM_FILE_MSG_NOFILE ), wm_get_string( STR_WM_OK ), 0, wm );
    		return 0;
	    }
    	}

	while( data->win->action_handler )
	{
	    if( data->flags & FDIALOG_FLAG_WITH_OVERWRITE_DIALOG )
	    {
    		if( ( data->flags & FDIALOG_FLAG_LOAD ) == 0 )
    		{
    		    //Save:
    		    if( sfs_get_file_size( data->final_filename ) )
    		    {
    			//Overwrite:
    			WINDOWPTR w = fdialog_overwrite_open( data->final_filename, wm );
    			if( w )
    			{
			    add_data_container( w->childs[ 0 ], "filename", SMEM_STRDUP( data->final_filename ) );
    			    set_handler( w->childs[ 0 ], fdialog_overwrite_action_handler, data->win, wm );
    			}
    			break;
		    }
		}
	    }

	    save_recent( data );

    	    data->win->action_handler( data->win->handler_data, data->win, wm );

	    remove_window( data->win, wm );
	    recalc_regions( wm );
	    draw_window( wm->root_win, wm );

    	    break;
	}
    }
    else
    {
	//Edit:

	fdialog_edit( data, win->action_result );
	draw_window( wm->root_win, wm );
    }

    return 0;
}

int fdialog_cancel_button_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    fdialog_data* data = (fdialog_data*)user_data;
    send_event( data->win, EVT_CLOSEREQUEST, wm );
    return 0;
}

int fdialog_wifi_button_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    if( g_webserver_available )
        webserver_open( wm );
    return 0;
}

int fdialog_name_handler( void* user_data, WINDOWPTR name_win, window_manager* wm )
{
    fdialog_data* data = (fdialog_data*)user_data;
    char* new_name = fdialog_fix_name( text_get_text( name_win, wm ), data->mask );
    if( new_name )
    {
	text_set_text( name_win, new_name, wm );
	smem_free( new_name );
	draw_window( name_win, wm );
    }
    return 0;
}

int fdialog_preview_resize_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    fdialog_data* data = (fdialog_data*)user_data;
    
    int new_ysize = 0;
    resizer_get_vals( win, 0, &new_ysize );
    data->preview_data.win->ysize = new_ysize;
    recalc_regions( wm );
    draw_window( data->win, wm );
    
    return 0;
}

#ifdef SUNVOX_GUI
int fdialog_preview_resize_opt_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    fdialog_data* data = (fdialog_data*)user_data;
    
    send_event( data->preview_data.win, EVT_OPT, wm );
    
    return 0;
}
#endif

static int get_disk_button_xsize( const char* name, WINDOWPTR win )
{
    window_manager* wm = win->wm;
    int xsize = font_string_x_size( name, win->font, wm ) + wm->interelement_space * 2;
    if( xsize < wm->scrollbar_size ) 
	xsize = wm->scrollbar_size;
    //if( xsize < wm->small_button_xsize ) 
	//xsize = wm->small_button_xsize;
    return xsize;
}

void clear_fdialog_constructor_options( window_manager* wm )
{
    wm->opt_fdialog_id = NULL;
    wm->opt_fdialog_mask = NULL;
    wm->opt_fdialog_preview = NULL;
    wm->opt_fdialog_preview_handler = 0;
    wm->opt_fdialog_preview_user_data = NULL;
    for( int i = 0; i < 4; i++ )
    {
        wm->opt_fdialog_user_button[ i ] = NULL;
        wm->opt_fdialog_user_button_handler[ i ] = 0;
        wm->opt_fdialog_user_button_data[ i ] = NULL;
    }
    wm->opt_fdialog_def_filename = NULL;
    wm->opt_fdialog_flags = 0;
}

int fdialog_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    fdialog_data* data = (fdialog_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( fdialog_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		WINDOWPTR w;
		const char* bname;
		int but_xsize = wm->button_xsize;
		int but_ysize = wm->button_ysize;
		int x = 0;
		int y = 0;
		int label_xsize = 0;

		data->final_path = SMEM_ALLOC2( char, 256 );
		data->final_path[ 0 ] = 0;

		data->final_filename = SMEM_ALLOC2( char, 256 );
		data->final_filename[ 0 ] = 0;

		data->show_hidden = ( sconfig_get_int_value( APP_CFG_FDIALOG_SHOWHIDDEN, -1, 0 ) != -1 );

		fdialog_refresh_disks();
		for( int i = 0; i < MAX_DISKS; i++ )
		{
		    data->prop_path[ i ] = 0;
		    data->prop_cur_file[ i ] = 0;
		    data->disk_buttons[ i ] = 0;
    	        }
		data->prop_cur_disk = 0;
		if( wm->opt_fdialog_id )
		{
		    data->props_file = fdialog_make_props_filename( wm->opt_fdialog_id );
		    size_t plen = smem_strlen( data->props_file );
		    data->props_file_recent_dirs = SMEM_ALLOC2( char, plen + 32 );
		    data->props_file_recent_files = SMEM_ALLOC2( char, plen + 32 );
		    data->props_file_dir_pos = SMEM_ALLOC2( char, plen + 32 );
		    sprintf( data->props_file_recent_dirs, "%s_recent_dirs", data->props_file );
		    sprintf( data->props_file_recent_files, "%s_recent_files", data->props_file );
		    sprintf( data->props_file_dir_pos, "%s_dir_pos", data->props_file );
    	        }
		else
		{
		    data->props_file = NULL;
		    data->props_file_recent_dirs = NULL;
		    data->props_file_recent_files = NULL;
		    data->props_file_dir_pos = NULL;
		}
		data->mask = SMEM_STRDUP( wm->opt_fdialog_mask );
		data->preview = SMEM_STRDUP( wm->opt_fdialog_preview );
		data->preview_handler = wm->opt_fdialog_preview_handler;
		data->preview_data.user_data = wm->opt_fdialog_preview_user_data;
		data->flags = wm->opt_fdialog_flags;
		data->win = win;
		data->first_time = true;
		data->fdialog_preview_flag = false;

		data->preview_data.win = new_window( "fdialog preview", 0, wm->interelement_space, 0, 0, win->color, win, null_handler, wm );
		set_window_controller( data->preview_data.win, 0, wm, (WCMD)wm->interelement_space + wm->scrollbar_size, CEND );
		set_window_controller( data->preview_data.win, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );

		w = new_window( "fdialog preview resizer", 0, 0, 1, 1, wm->button_color, win, resizer_handler, wm );
                resizer_set_parameters( w, 0, wm->scrollbar_size );
                resizer_set_flags( w, resizer_get_flags( w ) | RESIZER_FLAG_TOPDOWN );
                set_handler( w, fdialog_preview_resize_handler, data, wm );
#ifdef SUNVOX_GUI
                resizer_set_flags( w, resizer_get_flags( w ) | RESIZER_FLAG_OPTBTN );
                resizer_set_opt_handler( w, fdialog_preview_resize_opt_handler, data );
#endif
		set_window_controller( w, 0, wm, (WCMD)wm->interelement_space, CEND );
		set_window_controller( w, 1, wm, (WCMD)wm->interelement_space, CEND );
		set_window_controller( w, 2, wm, (WCMD)wm->interelement_space + wm->scrollbar_size, CEND );
		set_window_controller( w, 3, wm, CWIN, (WCMD)data->preview_data.win, CY2, CSUB, (WCMD)wm->interelement_space, CMINVAL, (WCMD)wm->interelement_space, CEND );

		data->panel_recent = new_window( "fdialog panel: recent", 0, 0, win->xsize, win->ysize, win->color, win, null_handler, wm );

        	data->recent_div = new_window( "fdialog div", 0, 0, 0, wm->scrollbar_size, wm->button_color, data->panel_recent, divider_handler, wm );
        	divider_set_flags( data->recent_div, divider_get_flags( data->recent_div ) | DIVIDER_FLAG_DYNAMIC_XSIZE );
		set_window_controller( data->recent_div, 1, wm, CMINVAL, (WCMD)wm->text_ysize, CBACKVAL1, CMAXVAL, (WCMD)wm->scrollbar_size, CEND );
		set_window_controller( data->recent_div, 2, wm, CMINVAL, (WCMD)wm->scrollbar_size, CEND );
		set_window_controller( data->recent_div, 3, wm, CMINVAL, (WCMD)wm->text_ysize + wm->scrollbar_size, CBACKVAL1, CMAXVAL, (WCMD)0, CEND );

		set_window_controller( data->panel_recent, 1, wm, CWIN, (WCMD)data->preview_data.win, CY2, CEND );
		set_window_controller( data->panel_recent, 3, wm, CPERC, (WCMD)100, CEND );

		data->panel_main = new_window( "fdialog panel: main", 0, 0, 0, 0, win->color, win, null_handler, wm );
		set_window_controller( data->panel_main, 0, wm, CWIN, (WCMD)data->panel_recent, CX2, CEND );
		set_window_controller( data->panel_main, 1, wm, CWIN, (WCMD)data->preview_data.win, CY2, CEND );
		set_window_controller( data->panel_main, 2, wm, CPERC, (WCMD)100, CEND );
		set_window_controller( data->panel_main, 3, wm, CPERC, (WCMD)100, CEND );

		//RECENT:

		label_xsize = font_string_x_size( wm_get_string( STR_WM_FILE_RECENT ), win->font, wm ) + font_string_x_size( " ", win->font, wm );
		new_window( wm_get_string( STR_WM_FILE_RECENT ), 0, 0, label_xsize, wm->text_ysize, 0, data->panel_recent, label_handler, wm );

		data->recent_files = new_window( "recent files", 0, 0, 1, 1, wm->list_background, data->panel_recent, list_handler, wm );
		list_set_flags( data->recent_files, LIST_FLAG_SHADE_FILENAME_EXT );
		set_handler( data->recent_files, fdialog_recent_files_list_handler, data, wm );
		set_window_controller( data->recent_files, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->recent_files, 1, wm, (WCMD)wm->text_ysize, CEND );
		set_window_controller( data->recent_files, 2, wm, CPERC, (WCMD)100, CEND );
		set_window_controller( data->recent_files, 3, wm, CWIN, (WCMD)data->recent_div, CY1, CEND );

		data->recent_dirs = new_window( "recent dirs", 0, 0, 1, 1, wm->list_background, data->panel_recent, list_handler, wm );
		list_set_flags( data->recent_dirs, LIST_FLAG_RIGHT_ALIGNMENT_ON_OVERFLOW | LIST_FLAG_SHADE_LEFT_FILENAME_PART );
		set_handler( data->recent_dirs, fdialog_recent_dirs_list_handler, data, wm );
		set_window_controller( data->recent_dirs, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->recent_dirs, 1, wm, CWIN, (WCMD)data->recent_div, CY2, CEND );
		set_window_controller( data->recent_dirs, 2, wm, CPERC, (WCMD)100, CEND );
		set_window_controller( data->recent_dirs, 3, wm, CPERC, (WCMD)100, CEND );

		fdialog_load_recent_list( data, &data->recent_files_data, data->props_file_recent_files );
		fdialog_load_recent_list( data, &data->recent_dirs_data, data->props_file_recent_dirs );
		uint fnum = slist_get_items_num( &data->recent_files_data );
		uint rnum = slist_get_items_num( &data->recent_dirs_data );
		if( ( fnum != 0 || rnum != 0 ) && sconfig_get_int_value( APP_CFG_FDIALOG_NORECENT, -1, 0 ) == -1 )
		{
		    //Show recent:
		    data->recent_size_init_request = true;
		    data->recent_div->y = 0;
		    data->recent_div->xsize = 1;
		    set_window_controller( data->panel_recent, 0, wm, (WCMD)wm->interelement_space, CEND );
		    set_window_controller( data->panel_recent, 2, wm, CWIN, (WCMD)data->recent_div, CX2, CADD, (WCMD)wm->interelement_space, CEND );

		    //Fill the lists:
		    fdialog_fill_recent_window( data, data->recent_files, &data->recent_files_data, 0 );
		    fdialog_fill_recent_window( data, data->recent_dirs, &data->recent_dirs_data, 1 );
		}
		else
		{
		    //Hide recent:
		    data->recent_size_init_request = false;
		    data->panel_recent->x = 0;
		    data->panel_recent->xsize = 0;
		}

		//DISKS:
		int disk_button_xsize;
		int disks_x = wm->interelement_space;
		int disks_y = 0;
		int disks_buttons = 1;
		disk_button_xsize = get_disk_button_xsize( "..", win );
		data->go_up_button = new_window( 
		    "..", 
		    disks_x, disks_y, disk_button_xsize, wm->scrollbar_size, 
		    wm->button_color,
		    data->panel_main,
		    button_handler, 
		    wm );
		disks_x += disk_button_xsize + wm->interelement_space2;
		set_handler( data->go_up_button, fdialog_disk_button_handler, data, wm );
		disk_button_xsize = get_disk_button_xsize( STR_HOME, win );
		data->go_home_button = new_window( 
		    STR_HOME, 
		    disks_x, disks_y, disk_button_xsize, wm->scrollbar_size, 
		    wm->button_color,
		    data->panel_main,
		    button_handler, 
		    wm );
		disks_x += disk_button_xsize + wm->interelement_space2;
		set_handler( data->go_home_button, fdialog_disk_button_handler, data, wm );
#ifdef OS_ANDROID
		for( int i = 1; i < ANDROID_NUM_EXT_STORAGE_DEVS; i++ )
		{
		    char* path = android_sundog_get_dir( wm->sd, "external_files", i );
		    if( !path ) break;
		    free( path );
		    switch( i )
		    {
			case 1: bname = STR_MEMORY_CARD "2"; break;
			case 2: bname = STR_MEMORY_CARD "3"; break;
			case 3: bname = STR_MEMORY_CARD "4"; break;
			case 4: bname = STR_MEMORY_CARD "5"; break;
			case 5: bname = STR_MEMORY_CARD "6"; break;
			case 6: bname = STR_MEMORY_CARD "7"; break;
		    }
		    disk_button_xsize = get_disk_button_xsize( bname, win );
		    data->addvd_buttons[ i - 1 ] = new_window( 
			bname,
			disks_x, disks_y, disk_button_xsize, wm->scrollbar_size, 
			wm->button_color,
			data->panel_main,
			button_handler, 
			wm );
		    disks_x += disk_button_xsize + wm->interelement_space2;
		    set_handler( data->addvd_buttons[ i - 1 ], fdialog_disk_button_handler, data, wm );
		}
#endif
		if( fdialog_get_disk_count() > 1 || ( fdialog_get_disk_name( 0 )[ 0 ] != '/' && fdialog_get_disk_name( 0 )[ 0 ] != '1' ) )
		{
		    for( int i = 0; i < fdialog_get_disk_count(); i++ )
		    {
			disk_button_xsize = get_disk_button_xsize( fdialog_get_disk_name( i ), win );
			data->disk_buttons[ i ] = new_window( 
			    fdialog_get_disk_name( i ), 
			    disks_x, 
			    disks_y, 
			    disk_button_xsize, 
			    wm->scrollbar_size, 
			    wm->button_color,
			    data->panel_main,
			    button_handler, 
			    wm );
			set_handler( data->disk_buttons[ i ], fdialog_disk_button_handler, data, wm );
			disks_x += disk_button_xsize + wm->interelement_space2;
			disks_buttons++;
		    }
		}
		y = disks_y + wm->scrollbar_size + wm->interelement_space;

		//DIRECTORY:
		x = wm->interelement_space;
		bool path_on_new_line = false;
		if( disks_x > win->xsize / 2 )
		    path_on_new_line = true;
		label_xsize = font_string_x_size( wm_get_string( STR_WM_FILE_PATH ), win->font, wm ) + font_string_x_size( " ", win->font, wm );
		wm->opt_text_ro = true;
		data->path = new_window( "pathn", 0, disks_y, 1, wm->scrollbar_size, wm->text_background, data->panel_main, text_handler, wm );
		if( path_on_new_line )
		{
		    new_window( wm_get_string( STR_WM_FILE_PATH ), x, y, label_xsize, wm->text_ysize, 0, data->panel_main, label_handler, wm );
		    set_window_controller( data->path, 0, wm, (WCMD)x + label_xsize, CEND );
		    set_window_controller( data->path, 1, wm, (WCMD)y, CEND );
		    set_window_controller( data->path, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		    set_window_controller( data->path, 3, wm, (WCMD)y + wm->text_ysize, CEND );
        	    y += wm->text_ysize + wm->interelement_space;
    		}
    		else
    		{
		    set_window_controller( data->path, 0, wm, (WCMD)disks_x + wm->interelement_space, CEND );
		    set_window_controller( data->path, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
    		}
    		text_set_color( data->path, wm->color2 );

		//FILENAME:
		label_xsize = font_string_x_size( wm_get_string( STR_WM_FILE_NAME ), win->font, wm ) + font_string_x_size( " ", win->font, wm );
		new_window( wm_get_string( STR_WM_FILE_NAME ), x, y, label_xsize, wm->text_ysize, 0, data->panel_main, label_handler, wm );
		data->name = new_window( "namen", 0, y, 1, wm->text_ysize, wm->text_background, data->panel_main, text_handler, wm );
		set_handler( data->name, fdialog_name_handler, data, wm );
	        set_window_controller( data->name, 0, wm, (WCMD)x + label_xsize, CEND );
		set_window_controller( data->name, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		y += wm->text_ysize + wm->interelement_space;

		//FILES:
		data->files = new_window( "files", 0, y, 10, 10, wm->list_background, data->panel_main, list_handler, wm );
		list_set_flags( data->files, LIST_FLAG_SHADE_FILENAME_EXT );
		set_handler( data->files, fdialog_list_handler, data, wm );
		set_window_controller( data->files, 0, wm, CPERC, (WCMD)0, CADD, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->files, 1, wm, (WCMD)y, CEND );
		set_window_controller( data->files, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->files, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)but_ysize + wm->interelement_space * 2, CEND );

		//BUTTONS:

		bname = wm_get_string( STR_WM_OK ); but_xsize = button_get_optimal_xsize( bname, win->font, false, wm );
		data->ok_button = new_window( bname, x, 0, but_xsize, 1, wm->button_color, data->panel_main, button_handler, wm );
		set_handler( data->ok_button, fdialog_ok_button_handler, data, wm );
		set_window_controller( data->ok_button, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->ok_button, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)but_ysize + wm->interelement_space, CEND );
		x += but_xsize + wm->interelement_space2;
    
		bname = wm_get_string( STR_WM_CANCEL ); but_xsize = button_get_optimal_xsize( bname, win->font, false, wm );
		data->cancel_button = new_window( bname, x, 0, but_xsize, 1, wm->button_color, data->panel_main, button_handler, wm );
		set_handler( data->cancel_button, fdialog_cancel_button_handler, data, wm );
		set_window_controller( data->cancel_button, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->cancel_button, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)but_ysize + wm->interelement_space, CEND );
		x += but_xsize + wm->interelement_space2;

		bname = wm_get_string( STR_WM_EDIT ); but_xsize = button_get_optimal_xsize( bname, win->font, false, wm );
		data->edit_button = new_window( bname, x, 0, but_xsize, 1, wm->button_color, data->panel_main, button_handler, wm );
		char* ts = fdialog_edit_get_menu( data );
		button_set_menu( data->edit_button, ts );
		smem_free( ts );
		set_handler( data->edit_button, fdialog_ok_button_handler, data, wm );
		set_window_controller( data->edit_button, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->edit_button, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)but_ysize + wm->interelement_space, CEND );
		x += but_xsize + wm->interelement_space2;

		if( data->preview )
		{
		    but_xsize = button_get_optimal_xsize( data->preview, win->font, false, wm );
		    data->preview_button = new_window( data->preview, x, 0, but_xsize, 1, wm->button_color, data->panel_main, button_handler, wm );
		    set_handler( data->preview_button, fdialog_preview_button_handler, data, wm );
		    set_window_controller( data->preview_button, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		    set_window_controller( data->preview_button, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)but_ysize + wm->interelement_space, CEND );
		    x += but_xsize + wm->interelement_space2;

		    bool enable = sconfig_get_int_value( APP_CFG_FDIALOG_PREVIEW, 0, 0 ) != 0;
		    fdialog_preview_onoff( data, enable, false, false );
		}
		else 
		{
		    data->preview_button = 0;
		}
                if( g_webserver_available )
                {
                    bname = "Wi-Fi"; but_xsize = button_get_optimal_xsize( bname, win->font, false, wm );
                    WINDOWPTR wifi_button = new_window( bname, x, 0, but_xsize, 1, wm->button_color, data->panel_main, button_handler, wm );
                    set_handler( wifi_button, fdialog_wifi_button_handler, data, wm );
                    set_window_controller( wifi_button, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
                    set_window_controller( wifi_button, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)but_ysize + wm->interelement_space, CEND );
                    x += but_xsize + wm->interelement_space2;
                }
		for( int i = 0; i < 4; i++ )
		{
		    if( wm->opt_fdialog_user_button[ i ] )
		    {
			but_xsize = button_get_optimal_xsize( wm->opt_fdialog_user_button[ i ], win->font, false, wm );
			WINDOWPTR user_button = new_window( wm->opt_fdialog_user_button[ i ], x, 0, but_xsize, 1, wm->button_color, data->panel_main, button_handler, wm );
			set_handler( user_button, wm->opt_fdialog_user_button_handler[ i ], wm->opt_fdialog_user_button_data[ i ], wm );
			set_window_controller( user_button, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
			set_window_controller( user_button, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)but_ysize + wm->interelement_space, CEND );
			x += but_xsize + wm->interelement_space2;
		    }
		}

		//LOAD LAST PROPERTIES:
		recalc_regions( wm ); //we need to know the final size of the list with files
    		fdialog_load_props( data );
		fdialog_refresh_list( data );
		if( data->prop_path[ data->prop_cur_disk ] )
		    fdialog_set_path( data, data->prop_path[ data->prop_cur_disk ] );
		if( wm->opt_fdialog_def_filename )
		    text_set_text( data->name, wm->opt_fdialog_def_filename, wm );
		if( data->prop_cur_file[ data->prop_cur_disk ] >= 0 )
		{
		    slist_data* ldata = list_get_data( data->files, wm );
		    int snum = ldata->selected_item;
		    char* item = fdialog_fix_list_item( slist_get_item( snum, ldata ) );
		    //Set file num:
		    list_select_item( data->files, data->prop_cur_file[ data->prop_cur_disk ], true );
		    //Set file name:
		    if( item && wm->opt_fdialog_def_filename == NULL )
		    {
		        if( slist_get_attr( snum, ldata ) == 0 )
			    text_set_text( data->name, item, wm );
		    }
		    smem_free( item );
		}

		//SHOW CURRENT DISK:
		if( data->disk_buttons[ data->prop_cur_disk ] )
		{
		    data->disk_buttons[ data->prop_cur_disk ]->color = BUTTON_HIGHLIGHT_COLOR;
		}

		//DIR POSITION INIT:
		fdialog_pos_init( data );

		//SET FOCUS:
		dialog_stack_add( win );
		set_focus_win( data->files, wm );

		clear_fdialog_constructor_options( wm );
	    }
	    retval = 1;
	    break;
	case EVT_BEFORESHOW:
	    {
		if( data->recent_size_init_request )
		{
		    //Do it here, because the window size may be changed between EVT_AFTERCREATE and EVT_BEFORESHOW
		    data->panel_recent_def_y = win->ysize / 2;
		    data->panel_recent_def_xsize = win->xsize * 0.3;

		    int y = scale_coord( sconfig_get_int_value( APP_CFG_FDIALOG_RECENT_Y, scale_coord( data->panel_recent_def_y, true, wm ), 0 ), false, wm );
		    int xsize = scale_coord( sconfig_get_int_value( APP_CFG_FDIALOG_RECENT_XSIZE, scale_coord( data->panel_recent_def_xsize, true, wm ), 0 ), false, wm );
		    if( y > win->ysize - wm->scrollbar_size ) y = win->ysize - wm->scrollbar_size;
		    if( xsize > win->xsize * 0.9 ) xsize = win->xsize * 0.9;
		    data->recent_div->y = y;
		    data->recent_div->xsize = xsize;
		    data->recent_size_init_request = false;
		}
	    }
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    {
		bool save_config = false;
		if( data->panel_recent->xsize > 0 )
		{
		    if( data->recent_div->y != data->panel_recent_def_y ||
			data->recent_div->xsize != data->panel_recent_def_xsize )
		    {
			save_config = true;
			sconfig_set_int_value( APP_CFG_FDIALOG_RECENT_Y, scale_coord( data->recent_div->y, true, wm ), 0 );
			sconfig_set_int_value( APP_CFG_FDIALOG_RECENT_XSIZE, scale_coord( data->recent_div->xsize, true, wm ), 0 );
		    }
		}
		if( data->preview )
		{
		    if( fdialog_preview_onoff( data, false, false, false ) )
			save_config = true;
		}
		if( save_config )
		{
		    sconfig_save( 0 );
		}
		fdialog_save_props( data );
		fdialog_remove_props( data );
		fdialog_pos_deinit( data );
		smem_free( data->props_file );
		smem_free( data->props_file_recent_dirs );
		smem_free( data->props_file_recent_files );
		smem_free( data->props_file_dir_pos );
		smem_free( data->mask );
		smem_free( data->preview );
		smem_free( data->final_path );
		smem_free( data->final_filename );
		slist_deinit( &data->recent_files_data );
		slist_deinit( &data->recent_dirs_data );
		dialog_stack_del( win );
	    }
	    retval = 1;
	    break;
	case EVT_CLOSEREQUEST:
	    {
	        remove_window( win, wm );
		recalc_regions( wm );
	        draw_window( wm->root_win, wm );
	    }
	    retval = 1;
	    break;
	case EVT_DRAW:
	    {
		win_draw_frect( win, 0, 0, win->xsize, win->ysize, win->color, wm );
		if( data->first_time && data->files )
		{
		    //Set file num:
		    list_select_item( data->files, data->prop_cur_file[ data->prop_cur_disk ], true );
		    data->first_time = false;
		}
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
    }
    return retval;
}

char* fdialog_get_filename( WINDOWPTR win )
{
    if( !is_fdialog_win_valid( win ) ) return NULL;
    fdialog_data* data = (fdialog_data*)win->data;
    return data->final_filename;
}

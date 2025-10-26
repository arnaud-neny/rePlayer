/*
    wm_hnd_preferences.cpp
    This file is part of the SunDog engine.
    Copyright (C) 2011 - 2025 Alexander Zolotov <nightradio@gmail.com>
    WarmPlace.ru
*/

#include "sundog.h"
#include "video/video.h"

#if defined(OS_ANDROID) && !defined(NOFILEUTILS)
    #include "main/android/sundog_bridge.h"
#endif

#ifdef SUNVOX_GUI
    #include "../../sunvox/main/sunvox_gui.h"
#endif

bool g_clear_settings = false;

struct prefs_data
{
    WINDOWPTR win;
    WINDOWPTR close;
    WINDOWPTR sections;
    int list_xsize;
    int cur_section;
    WINDOWPTR cur_section_window;
    
    int correct_ysize;
};

static int prefs_close_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_data* data = (prefs_data*)user_data;

    remove_window( data->win, wm );
    recalc_regions( wm );
    draw_window( wm->root_win, wm );
    
    if( wm->prefs_restart_request )
    {
	if( dialog( 0, wm_get_string( STR_WM_PREFS_CHANGED ), wm_get_string( STR_WM_YESNO ), wm ) == 0 )
	{
	    wm->exit_request = 1;
	    wm->restart_request = 1;
	}
    }
    
    return 0;
}

static int prefs_sections_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_data* data = (prefs_data*)user_data;
    slist_data* ldata = list_get_data( data->sections, wm );
    
    if( ldata )
    {
	if( list_get_last_action( win, wm ) == LIST_ACTION_ESCAPE )
	{
	    //ESCAPE KEY:
	    prefs_close_handler( data, 0, wm );
	    return 0;
	}
	int sel = ldata->selected_item;
	if( (unsigned)sel < (unsigned)wm->prefs_sections )
	{
	    if( sel != data->cur_section )
	    {
		data->cur_section = sel;
		//Close old section:
		remove_window( data->cur_section_window, wm );
		//Open new section:
		data->cur_section_window = new_window( "Section", 0, 0, 1, 1, data->win->color, data->win, (int(*)(sundog_event*,window_manager*)) wm->prefs_section_handlers[ data->cur_section ], wm );
		set_window_controller( data->cur_section_window, 0, wm, (WCMD)wm->interelement_space * 2 + data->list_xsize, CEND );
		set_window_controller( data->cur_section_window, 1, wm, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->cur_section_window, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->cur_section_window, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		show_window( data->cur_section_window );
		bring_to_front( data->close, wm );
		//Resize prefs window:
		int new_ysize = wm->prefs_section_ysize + wm->interelement_space * 3 + wm->button_ysize;
		if( new_ysize < wm->large_window_ysize )
		    new_ysize = wm->large_window_ysize;
		resize_window_with_decorator( data->win, 0, new_ysize, wm );
		//Show it:
		recalc_regions( wm );
		draw_window( wm->root_win, wm );
	    }
	}
    }
    
    return 0;
}

int prefs_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    prefs_data* data = (prefs_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( prefs_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;
		
		wm->prefs_restart_request = false;

		data->list_xsize = 16;
		wm->prefs_sections = 0;
		while( 1 )
		{
		    const char* section_name = wm->prefs_section_names[ wm->prefs_sections ];
		    if( section_name == 0 ) break;
		    int x = font_string_x_size( section_name, win->font, wm ) + wm->interelement_space2;
		    if( x > data->list_xsize ) data->list_xsize = x;
		    wm->prefs_sections++;
		}
		
		wm->opt_list_without_scrollbar = true;
		data->sections = new_window( "Sections", 0, 0, 1, 1, wm->list_background, win, list_handler, wm );
		set_window_controller( data->sections, 0, wm, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->sections, 1, wm, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->sections, 2, wm, (WCMD)wm->interelement_space + data->list_xsize, CEND );
		set_window_controller( data->sections, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->button_ysize + wm->interelement_space * 2, CEND );
		set_handler( data->sections, prefs_sections_handler, data, wm );
		slist_data* l = list_get_data( data->sections, wm );
		for( int i = 0; i < wm->prefs_sections; i++ )
		{
		    const char* section_name = wm->prefs_section_names[ i ];
		    slist_add_item( section_name, 0, l );
		}
		l->selected_item = 0;

		data->cur_section = 0;
		data->cur_section_window = new_window( "Section", 0, 0, 1, 1, win->color, win, (int(*)(sundog_event*,window_manager*)) wm->prefs_section_handlers[ 0 ], wm );
		set_window_controller( data->cur_section_window, 0, wm, (WCMD)wm->interelement_space * 2 + data->list_xsize, CEND );
		set_window_controller( data->cur_section_window, 1, wm, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->cur_section_window, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->cur_section_window, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		
		int x = wm->interelement_space;
		const char* bname;
		int bxsize;
		
		bname = wm_get_string( STR_WM_CLOSE ); bxsize = button_get_optimal_xsize( bname, win->font, false, wm );
		data->close = new_window( bname, x, 0, bxsize, 1, wm->button_color, win, button_handler, wm );
		set_handler( data->close, prefs_close_handler, data, wm );
		set_window_controller( data->close, 1, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space, CEND );
		set_window_controller( data->close, 3, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->button_ysize + wm->interelement_space, CEND );
		
		//data->correct_ysize = win->ysize;
	    }
	    retval = 1;
	    break;
	case EVT_DRAW:
	    break;
	case EVT_CLOSEREQUEST:
            {
		prefs_close_handler( data, NULL, wm );
            }
            retval = 1;
            break;
	case EVT_BUTTONDOWN:
	    if( evt->key == KEY_ESCAPE )
	    {
		send_event( win, EVT_CLOSEREQUEST, wm );
		retval = 1;
	    }
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
	case EVT_TOUCHBEGIN:
	case EVT_TOUCHEND:
	case EVT_TOUCHMOVE:
	    retval = 1;
	    break;
	case EVT_BEFORECLOSE:
	    retval = 1;
	    wm->prefs_win = 0;
	    break;
	    
	case EVT_SCREENRESIZE:
	    if( !win->visible ) break; //recalc_controllers() will not be performed, so we will get the wrong window parameters
	case EVT_BEFORESHOW:
	    //resize_window_with_decorator( win, 0, data->correct_ysize, wm );
	    break;
    }
    return retval;
}

//
//
//

struct prefs_main_data
{
    WINDOWPTR win;

    WINDOWPTR reset_all;
    WINDOWPTR log;
    WINDOWPTR broad_fs; //Android MANAGE_EXTERNAL_STORAGE
};

int prefs_main_reset_all_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    if( dialog( 0, wm_get_string( STR_WM_CLEAR_SETTINGS_MSG ), wm_get_string( STR_WM_YESNO ), wm ) == 0 )
    {
	wm->exit_request = 1;
        wm->restart_request = 1;
        g_clear_settings = true;
    }
    return 0;
}

int prefs_main_log_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    edialog_open( NULL, NULL, wm );
    return 0;
}

int prefs_main_broad_fs_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
#ifdef OS_ANDROID
    if( android_sundog_allfiles_access_supported( wm->sd ) )
    {
	android_sundog_allfiles_access( wm->sd ); //show screen for controlling if the app can manage external storage (broad access)
    }
#endif
    return 0;
}

int prefs_main_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    prefs_main_data* data = (prefs_main_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( prefs_main_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;

		int y = 0;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->reset_all = new_window( wm_get_string( STR_WM_CLEAR_SETTINGS ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->reset_all, prefs_main_reset_all_handler, data, wm );
		set_window_controller( data->reset_all, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->reset_all, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->log = new_window( wm_get_string( STR_WM_LOG ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->log, prefs_main_log_handler, data, wm );
		set_window_controller( data->log, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->log, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

#ifdef OS_ANDROID
		if( android_sundog_allfiles_access_supported( wm->sd ) )
		{
		    wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		    data->broad_fs = new_window( wm_get_string( STR_WM_BROAD_FS_ACCESS ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		    set_handler( data->broad_fs, prefs_main_broad_fs_handler, data, wm );
		    set_window_controller( data->broad_fs, 0, wm, (WCMD)0, CEND );
		    set_window_controller( data->broad_fs, 2, wm, CPERC, (WCMD)100, CEND );
		    y += wm->text_ysize + wm->interelement_space;
		}
#endif

		wm->prefs_section_ysize = y;
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
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

//
//
//

struct prefs_ui_data
{
    WINDOWPTR win;
    WINDOWPTR window_pars; //for some desktop systems
    WINDOWPTR sysbars; //Android only
    WINDOWPTR lowres; //iOS only
    bool lowres_notice;
    WINDOWPTR maxfps;
    WINDOWPTR angle;
    WINDOWPTR control;
    WINDOWPTR zoom_btns;
    WINDOWPTR dclick;
    WINDOWPTR color;
    WINDOWPTR fonts;
    WINDOWPTR scale;
    WINDOWPTR virt_kbd;
    WINDOWPTR keymap;
    WINDOWPTR lang;
    WINDOWPTR hide_recent;
    WINDOWPTR show_hidden;
};

const int g_fps_vals[] = { 10,20,30,40,50,60,120,250,500,1000 };
const int g_fps_vals_num = 10;

#ifdef OS_ANDROID
    #define UI_PREFS_SYSBARS 1
#endif
#ifdef OS_IOS
    #define UI_PREFS_LOWRES 1
#endif
#if !defined(OS_APPLE) && !defined(OS_ANDROID) && !defined(OS_WINCE)
    #define UI_PREFS_WIN_PARS 1
#endif

static void prefs_ui_reinit( WINDOWPTR win )
{
    prefs_ui_data* data = (prefs_ui_data*)win->data;
    window_manager* wm = data->win->wm;

    char ts[ 512 ];
    const char* v = "";

    if( data->sysbars )
    {
	if( sconfig_get_int_value( APP_CFG_NOSYSBARS, 0, 0 ) != 0 )
	    data->sysbars->color = BUTTON_HIGHLIGHT_COLOR;
	else
	    data->sysbars->color = wm->button_color;
    }

    if( data->lowres )
    {
	if( sfs_get_file_size( "2:/opt_resolution" ) )
	    data->lowres->color = BUTTON_HIGHLIGHT_COLOR;
	else
	    data->lowres->color = wm->button_color;
    }

    if( data->maxfps )
    {
	int m = -1;
	int fps = sconfig_get_int_value( "maxfps", -1, 0 );
	int fps2 = fps;
	if( fps <= 0 )
	{
	    //Auto:
	    fps2 = wm->max_fps;
	    m = 0;
	}
	else
	{
	    for( int i = 0; i < g_fps_vals_num; i++ )
		if( g_fps_vals[ i ] == fps )
		    { m = i + 1; break; }
	}
	sprintf( ts, "%s = %d", wm_get_string( STR_WM_MAXFPS ), fps2 );
	button_set_text( data->maxfps, ts );
	button_set_menu_val( data->maxfps, m );
    }

    if( data->angle )
    {
	int a = sconfig_get_int_value( APP_CFG_ROTATE, 0, 0 );
	sprintf( ts, "%s = %d", wm_get_string( STR_WM_UI_ROTATION ), a );
	button_set_text( data->angle, ts );
	button_set_menu_val( data->angle, a / 90 );
    }

    if( data->control )
    {
	int m = 0;
	int tc = sconfig_get_int_value( APP_CFG_TOUCHCONTROL, -1, 0 );
	int pc = sconfig_get_int_value( APP_CFG_PENCONTROL, -1, 0 );
	if( tc == -1 && pc == -1 ) v = wm_get_string( STR_WM_AUTO );
	if( tc >= 0 ) { m = 1; v = wm_get_string( STR_WM_CTL_FINGERS ); }
	if( pc >= 0 ) { m = 2; v = wm_get_string( STR_WM_CTL_PEN ); }
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_CTL_TYPE ), v );
	button_set_text( data->control, ts );
	button_set_menu_val( data->control, m );
    }

    if( data->zoom_btns )
    {
	int m = 0;
	int vv = sconfig_get_int_value( APP_CFG_SHOW_ZOOM_BUTTONS, -1, 0 );
	v = wm_get_string( STR_WM_AUTO );
	if( vv == 0 ) { m = 2; v = wm_get_string( STR_WM_NO_CAP ); }
	if( vv == 1 ) { m = 1; v = wm_get_string( STR_WM_YES_CAP ); }
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_SHOW_ZOOM_BTNS ), v );
	button_set_text( data->zoom_btns, ts );
	button_set_menu_val( data->zoom_btns, m );
    }
    
    if( data->dclick )
    {
	int m = -1;
	int t = sconfig_get_int_value( APP_CFG_DOUBLECLICK, -1, 0 );
	if( t == -1 )
	{
	    m = 0;
	    sprintf( ts, "%s = %s (%d%s)", wm_get_string( STR_WM_DOUBLE_CLICK_TIME ), wm_get_string( STR_WM_AUTO ), wm->double_click_time, wm_get_string( STR_WM_MS ) );
	}
	else
	{
	    if( t >= 100 ) m = ( t - 100 ) / 50 + 1;
	    sprintf( ts, "%s = %d%s", wm_get_string( STR_WM_DOUBLE_CLICK_TIME ), t, wm_get_string( STR_WM_MS ) );
	}
	button_set_text( data->dclick, ts );
	button_set_menu_val( data->dclick, m );
    }
    
    if( data->virt_kbd )
    {
	int m = 0;
	int vk = -1;
	if( sconfig_get_int_value( APP_CFG_SHOW_VIRT_KBD, -1, 0 ) != -1 ) vk = 1;
	if( sconfig_get_int_value( APP_CFG_HIDE_VIRT_KBD, -1, 0 ) != -1 ) vk = 0;
	if( vk == -1 ) v = wm_get_string( STR_WM_AUTO );
	if( vk == 0 ) { m = 2; v = wm_get_string( STR_WM_OFF_CAP ); }
	if( vk == 1 ) { m = 1; v = wm_get_string( STR_WM_ON_CAP ); }
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_SHOW_KBD ), v );
	button_set_text( data->virt_kbd, ts );
	button_set_menu_val( data->virt_kbd, m );
    }
    
    if( data->lang )
    {
	int m = 0;
	char* lang = sconfig_get_str_value( APP_CFG_LANG, 0, 0 );
	v = wm_get_string( STR_WM_AUTO );
	if( smem_strstr( lang, "en_" ) ) { m = 1; v = "English"; }
	if( smem_strstr( lang, "ru_" ) ) { m = 2; v = "Русский"; }
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_LANG ), v );
	button_set_text( data->lang, ts );
	button_set_menu_val( data->lang, m );
    }

    if( data->hide_recent )
    {
	if( sconfig_get_int_value( APP_CFG_FDIALOG_NORECENT, -1, 0 ) != -1 )
    	    data->hide_recent->color = BUTTON_HIGHLIGHT_COLOR;
        else
	    data->hide_recent->color = wm->button_color;
    }

    if( data->show_hidden )
    {
	if( sconfig_get_int_value( APP_CFG_FDIALOG_SHOWHIDDEN, -1, 0 ) != -1 )
    	    data->show_hidden->color = BUTTON_HIGHLIGHT_COLOR;
        else
	    data->show_hidden->color = wm->button_color;
    }
}

static int prefs_ui_fps_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;
    
    int new_fps = -1;
    int v = win->action_result;
    if( v == 0 ) new_fps = 0;
    if( v >= 1 && v < g_fps_vals_num + 1 ) new_fps = g_fps_vals[ v - 1 ];
    if( new_fps == 0 )
	sconfig_remove_key( "maxfps", 0 );
    if( new_fps > 0 )
	sconfig_set_int_value( "maxfps", new_fps, 0 );
    if( new_fps != -1 )
    {
	sconfig_save( 0 );
	wm->prefs_restart_request = true;
    }
    
    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

static int prefs_ui_angle_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int prev_angle = sconfig_get_int_value( APP_CFG_ROTATE, 0, 0 );
    int angle = win->action_result * 90;
    if( angle >= 0 && angle <= 270 )
    {
	if( prev_angle != angle )
	{
	    if( angle )
		sconfig_set_int_value( APP_CFG_ROTATE, angle, 0 );
	    else
		sconfig_remove_key( APP_CFG_ROTATE, 0 );
	    sconfig_save( 0 );
	    wm->prefs_restart_request = true;
	}
    }
    
    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

static int prefs_ui_control_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int v = win->action_result;
    if( v == button_get_prev_menu_val( win ) ) return 0;
    if( v < 0 || v >= 3 ) return 0;

    switch( v )
    {
	case 0:
	    //Auto:
	    sconfig_remove_key( APP_CFG_TOUCHCONTROL, 0 );
	    sconfig_remove_key( APP_CFG_PENCONTROL, 0 );
	    break;
	case 1:
	    //Fingers:
	    sconfig_set_int_value( APP_CFG_TOUCHCONTROL, 1, 0 );
	    sconfig_remove_key( APP_CFG_PENCONTROL, 0 );
	    break;
	case 2:
	    //Pen/Mouse:
	    sconfig_set_int_value( APP_CFG_PENCONTROL, 1, 0 );
	    sconfig_remove_key( APP_CFG_TOUCHCONTROL, 0 );
	    break;
	default:
	    return 0;
	    break;
    }
    //To prevent stuck "navigation" mode (because the "selection" button may be invisible for some control types):
#ifdef SUNVOX_GUI
    sconfig_remove_key( APP_CFG_SUNVOX_EDIT_MODE_P, 0 );
    sconfig_remove_key( APP_CFG_SUNVOX_EDIT_MODE_T, 0 );
#endif
    //
    sconfig_save( 0 );
    wm->prefs_restart_request = true;

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

static int prefs_ui_zoom_btns_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int v = win->action_result;
    switch( v )
    {
	case 0:
	    //Auto:
	    sconfig_remove_key( APP_CFG_SHOW_ZOOM_BUTTONS, 0 );
	    break;
	case 1:
	    //Yes:
	    sconfig_set_int_value( APP_CFG_SHOW_ZOOM_BUTTONS, 1, 0 );
	    break;
	case 2:
	    //No:
	    sconfig_set_int_value( APP_CFG_SHOW_ZOOM_BUTTONS, 0, 0 );
	    break;
	default:
	    return 0;
	    break;
    }
    sconfig_save( 0 );
    wm->prefs_restart_request = true;

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

static int prefs_ui_dclick_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int v = win->action_result;
    if( v == 0 )
    {
	//Auto:
	sconfig_remove_key( APP_CFG_DOUBLECLICK, 0 );
	wm->double_click_time = DEFAULT_DOUBLE_CLICK_TIME;
    }
    else
    {
	if( v >= 1 )
	{
	    v++;
	    sconfig_set_int_value( APP_CFG_DOUBLECLICK, v * 50, 0 );
	    wm->double_click_time = v * 50;
	}
    }
    sconfig_save( 0 );

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

static int prefs_ui_color_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    colortheme_open( wm );
    return 0;
}

static int prefs_ui_font_dialog_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    dialog_item* dlist = dialog_get_items( win );
    if( win->action_result == 0 )
    {
	//OK:

	bool changed = false;

#ifdef OPENGL
	int font_upscaling = sconfig_get_int_value( APP_CFG_NO_FONT_UPSCALE, -1, 0 ) == -1;
	int font_fscaling = sconfig_get_int_value( "int_font_scaling", 0, 0 ) == 0;

	int font_upscaling2 = dialog_get_item( dlist, 'upsc' )->int_val - 1;
	int font_fscaling2 = dialog_get_item( dlist, 'frsc' )->int_val - 1;

	if( font_upscaling != font_upscaling2 )
	{
	    if( font_upscaling2 < 0 || font_upscaling2 == 1 )
		sconfig_remove_key( APP_CFG_NO_FONT_UPSCALE, 0 );
	    if( font_upscaling2 == 0 )
		sconfig_set_int_value( APP_CFG_NO_FONT_UPSCALE, 1, 0 );
	    changed = true;
	}
	if( font_fscaling != font_fscaling2 )
	{
	    if( font_fscaling2 < 0 || font_fscaling2 == 1 )
		sconfig_remove_key( "int_font_scaling", 0 );
	    if( font_fscaling2 == 0 )
		sconfig_set_int_value( "int_font_scaling", 1, 0 );
	    changed = true;
	}
#endif

	int font_big = sconfig_get_int_value( APP_CFG_FONT_BIG, DEFAULT_FONT_BIG, 0 );
	int font_med_mono = sconfig_get_int_value( APP_CFG_FONT_MEDIUM_MONO, DEFAULT_FONT_MEDIUM_MONO, 0 );
	int font_small = sconfig_get_int_value( APP_CFG_FONT_SMALL, DEFAULT_FONT_SMALL, 0 );

	int font_big2 = dialog_get_item( dlist, 'fbig' )->int_val - 1;
	int font_med_mono2 = dialog_get_item( dlist, 'fmed' )->int_val - 1;
	int font_small2 = dialog_get_item( dlist, 'fsml' )->int_val - 1;
	if( font_med_mono != font_med_mono2 )
	{
	    if( font_med_mono2 == DEFAULT_FONT_MEDIUM_MONO || font_med_mono2 < 0 )
		sconfig_remove_key( APP_CFG_FONT_MEDIUM_MONO, 0 );
	    else
		sconfig_set_int_value( APP_CFG_FONT_MEDIUM_MONO, font_med_mono2, 0 );
	    changed = true;
	}
	if( font_big != font_big2 )
	{
	    if( font_big2 == DEFAULT_FONT_BIG || font_big2 < 0 )
	        sconfig_remove_key( APP_CFG_FONT_BIG, 0 );
	    else
		sconfig_set_int_value( APP_CFG_FONT_BIG, font_big2, 0 );
	    changed = true;
	}
	if( font_small != font_small2 )
	{
	    if( font_small2 == DEFAULT_FONT_SMALL || font_small2 < 0 )
		sconfig_remove_key( APP_CFG_FONT_SMALL, 0 );
	    else
		sconfig_set_int_value( APP_CFG_FONT_SMALL, font_small2, 0 );
	    changed = true;
	}
	if( changed )
	{
    	    wm->prefs_restart_request = true;
    	    sconfig_save( 0 );
    	}
    }
    if( win->action_result == 2 )
    {
	//Reset:
	dialog_get_item( dlist, 'fbig' )->int_val = DEFAULT_FONT_BIG + 1;
	dialog_get_item( dlist, 'fmed' )->int_val = DEFAULT_FONT_MEDIUM_MONO + 1;
	dialog_get_item( dlist, 'fsml' )->int_val = DEFAULT_FONT_SMALL + 1;
	dialog_reinit_items( win, false );
        draw_window( win, wm );
	return 0;
    }
    return 1;
}

static int prefs_ui_fonts_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    char* fonts_menu = SMEM_ALLOC2( char, 128 );
    fonts_menu[ 0 ] = 0;
    SMEM_STRCAT_D( fonts_menu, wm_get_string( STR_WM_AUTO ) );
    for( int i = 0; i < g_fonts_count; i++ )
    {
	SMEM_STRCAT_D( fonts_menu, "\n" );
	SMEM_STRCAT_D( fonts_menu, g_font_names[ i ] );
    }

#ifdef OPENGL
    char* upscale_menu = SMEM_ALLOC2( char, 128 );
    upscale_menu[ 0 ] = 0;
    SMEM_STRCAT_D( upscale_menu, wm_get_string( STR_WM_AUTO ) );
    SMEM_STRCAT_D( upscale_menu, "\n" );
    SMEM_STRCAT_D( upscale_menu, wm_get_string( STR_WM_OFF_CAP ) );
    SMEM_STRCAT_D( upscale_menu, "\n" );
    SMEM_STRCAT_D( upscale_menu, wm_get_string( STR_WM_ON_CAP ) );
    int font_upscaling = sconfig_get_int_value( APP_CFG_NO_FONT_UPSCALE, -1, 0 ) == -1;
    int font_fscaling = sconfig_get_int_value( "int_font_scaling", 0, 0 ) == 0;
#endif

    int font_big = sconfig_get_int_value( APP_CFG_FONT_BIG, DEFAULT_FONT_BIG, 0 );
    int font_med_mono = sconfig_get_int_value( APP_CFG_FONT_MEDIUM_MONO, DEFAULT_FONT_MEDIUM_MONO, 0 );
    int font_small = sconfig_get_int_value( APP_CFG_FONT_SMALL, DEFAULT_FONT_SMALL, 0 );

    char* dialog_buttons = SMEM_ALLOC2( char, 512 );
    dialog_item* dlist = NULL;
    WINDOWPTR dwin = NULL;
    while( 1 )
    {
        dialog_item* di = NULL;

        snprintf( dialog_buttons, smem_get_size( dialog_buttons ),
            "OK;%s;%s",
            wm_get_string( STR_WM_CANCEL ),
            wm_get_string( STR_WM_RESET ) );

        di = dialog_new_item( &dlist ); if( !di ) break;
	di->type = DIALOG_ITEM_POPUP;
	di->str_val = (char*)wm_get_string( STR_WM_FONT_BIG );
	di->int_val = font_big + 1;
	di->menu = fonts_menu;
	di->id = 'fbig';

        di = dialog_new_item( &dlist ); if( !di ) break;
	di->type = DIALOG_ITEM_POPUP;
	di->str_val = (char*)wm_get_string( STR_WM_FONT_MEDIUM_MONO );
	di->int_val = font_med_mono + 1;
	di->menu = fonts_menu;
	di->id = 'fmed';

        di = dialog_new_item( &dlist ); if( !di ) break;
	di->type = DIALOG_ITEM_POPUP;
	di->str_val = (char*)wm_get_string( STR_WM_FONT_SMALL );
	di->int_val = font_small + 1;
	di->menu = fonts_menu;
	di->id = 'fsml';

#ifdef OPENGL
        di = dialog_new_item( &dlist ); if( !di ) break;
	di->type = DIALOG_ITEM_EMPTY_LINE;

        di = dialog_new_item( &dlist ); if( !di ) break;
	di->type = DIALOG_ITEM_POPUP;
	di->str_val = (char*)wm_get_string( STR_WM_FONT_UPSCALING );
	di->int_val = font_upscaling + 1;
	di->menu = upscale_menu;
	di->id = 'upsc';

        di = dialog_new_item( &dlist ); if( !di ) break;
	di->type = DIALOG_ITEM_POPUP;
	di->str_val = (char*)wm_get_string( STR_WM_FONT_FSCALING );
	di->int_val = font_fscaling + 1;
	di->menu = upscale_menu;
	di->id = 'frsc';
#endif

        wm->opt_dialog_items = dlist;
	dwin = dialog_open( wm_get_string( STR_WM_FONTS ), NULL, dialog_buttons, DIALOG_FLAG_SINGLE, wm ); //retval = decorator
        if( !dwin ) break;

	set_handler( dwin->childs[ 0 ], prefs_ui_font_dialog_handler, user_data, wm );
        dialog_set_flags( dwin, DIALOG_FLAG_AUTOREMOVE_ITEMS );
        dlist = NULL; //because we use DIALOG_FLAG_AUTOREMOVE_ITEMS

        break;
    }
    smem_free( dlist );
    smem_free( dialog_buttons );

    smem_free( fonts_menu );
#ifdef OPENGL
    smem_free( upscale_menu );
#endif
    return 0;
}

static int prefs_ui_scale_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    ui_scale_open( wm );
    return 0;
}

static int prefs_ui_virt_kbd_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int v = win->action_result;
    switch( v )
    {
	case 0:
	    //Auto:
	    sconfig_remove_key( APP_CFG_SHOW_VIRT_KBD, 0 );
	    sconfig_remove_key( APP_CFG_HIDE_VIRT_KBD, 0 );
	    break;
	case 1:
	    //Yes:
	    sconfig_set_int_value( APP_CFG_SHOW_VIRT_KBD, 1, 0 );
	    sconfig_remove_key( APP_CFG_HIDE_VIRT_KBD, 0 );
	    break;
	case 2:
	    //No:
	    sconfig_set_int_value( APP_CFG_HIDE_VIRT_KBD, 1, 0 );
	    sconfig_remove_key( APP_CFG_SHOW_VIRT_KBD, 0 );
	    break;
	default:
	    return 0;
	    break;
    }
    sconfig_save( 0 );
    wm->prefs_restart_request = true;

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

static int prefs_ui_keymap_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    keymap_open( wm );
    return 0;
}

static int prefs_ui_lang_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int v = win->action_result;
    switch( v )
    {
	case 0:
	    //Auto:
	    sconfig_remove_key( APP_CFG_LANG, 0 );
	    break;
	case 1:
	    //English:
	    sconfig_set_str_value( APP_CFG_LANG, "en_US", 0 );
	    break;
	case 2:
	    //Russian:
	    sconfig_set_str_value( APP_CFG_LANG, "ru_RU", 0 );
	    break;
	default:
	    return 0;
	    break;
    }
    sconfig_save( 0 );
    wm->prefs_restart_request = true;

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

static int prefs_ui_hide_recent_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    if( sconfig_get_int_value( APP_CFG_FDIALOG_NORECENT, -1, 0 ) != -1 )
        sconfig_remove_key( APP_CFG_FDIALOG_NORECENT, 0 );
    else
        sconfig_set_int_value( APP_CFG_FDIALOG_NORECENT, 1, 0 );
    sconfig_save( 0 );

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

static int prefs_ui_show_hidden_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    if( sconfig_get_int_value( APP_CFG_FDIALOG_SHOWHIDDEN, -1, 0 ) != -1 )
        sconfig_remove_key( APP_CFG_FDIALOG_SHOWHIDDEN, 0 );
    else
        sconfig_set_int_value( APP_CFG_FDIALOG_SHOWHIDDEN, 1, 0 );
    sconfig_save( 0 );

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

#ifdef UI_PREFS_WIN_PARS
static int prefs_ui_window_pars_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    int screen_xsize = sconfig_get_int_value( APP_CFG_WIN_XSIZE, wm->real_window_width, 0 );
    int screen_ysize = sconfig_get_int_value( APP_CFG_WIN_YSIZE, wm->real_window_height, 0 );
    int fullscreen = 0;
    if( sconfig_get_int_value( APP_CFG_FULLSCREEN, -1, 0 ) != -1 ) fullscreen = 1;
    dialog_item di[ 6 ];
    smem_clear( &di, sizeof( di ) );
    di[ 0 ].type = DIALOG_ITEM_LABEL;
    di[ 0 ].str_val = (char*)wm_get_string( STR_WM_WINDOW_WIDTH );
    di[ 1 ].type = DIALOG_ITEM_NUMBER;
    di[ 1 ].min = 0;
    di[ 1 ].max = 8000;
    di[ 1 ].int_val = screen_xsize;
    di[ 2 ].type = DIALOG_ITEM_LABEL;
    di[ 2 ].str_val = (char*)wm_get_string( STR_WM_WINDOW_HEIGHT );
    di[ 3 ].type = DIALOG_ITEM_NUMBER;
    di[ 3 ].min = 0;
    di[ 3 ].max = 8000;
    di[ 3 ].int_val = screen_ysize;
    di[ 4 ].type = DIALOG_ITEM_POPUP;
    di[ 4 ].str_val = (char*)wm_get_string( STR_WM_WINDOW_FULLSCREEN );
    di[ 4 ].int_val = fullscreen;
    di[ 4 ].menu = wm_get_string( STR_WM_OFF_ON_MENU );
    di[ 5 ].type = DIALOG_ITEM_NONE;
    wm->opt_dialog_items = di;
    int d = dialog( wm_get_string( STR_WM_WINDOW_PARS ), 0, wm_get_string( STR_WM_OKCANCEL ), wm );
    if( d == 0 )
    {
        bool changed = false;
    	if( di[ 1 ].int_val != screen_xsize || di[ 3 ].int_val != screen_ysize )
    	{
    	    sconfig_set_int_value( APP_CFG_WIN_XSIZE, di[ 1 ].int_val, 0 );
    	    sconfig_set_int_value( APP_CFG_WIN_YSIZE, di[ 3 ].int_val, 0 );
    	    changed = true;
    	}
    	if( di[ 4 ].int_val != fullscreen )
    	{
    	    if( di[ 4 ].int_val )
    		sconfig_set_int_value( APP_CFG_FULLSCREEN, 1, 0 );
    	    else
    		sconfig_remove_key( APP_CFG_FULLSCREEN, 0 );
    	    changed = true;
    	}
        if( changed )
        {
    	    wm->prefs_restart_request = true;
    	    wm->flags |= WIN_INIT_DONT_SAVE_WINSIZE;
    	    sconfig_save( 0 );
    	}
    }
    return 0;
}
#endif

#ifdef UI_PREFS_SYSBARS
static int prefs_ui_sysbars_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    int val = sconfig_get_int_value( APP_CFG_NOSYSBARS, 0, 0 );
    if( val == 0 )
	val = 1;
    else
	val = 0;
    if( val == 0 )
	sconfig_remove_key( APP_CFG_NOSYSBARS, 0 );
    else
	sconfig_set_int_value( APP_CFG_NOSYSBARS, val, 0 );
    sconfig_save( 0 );
    
    wm->prefs_restart_request = true;

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}
#endif

#ifdef UI_PREFS_LOWRES
static int prefs_ui_lowres_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_ui_data* data = (prefs_ui_data*)user_data;

    if( sfs_get_file_size( "2:/opt_resolution" ) )
	sfs_remove_file( "2:/opt_resolution" );
    else
    {
	sfs_file f = sfs_open( "2:/opt_resolution", "wb" );
	if( f )
	{
	    sfs_write( "low", 1, 5, f );
	    sfs_close( f );
	}
    }

    if( data->lowres_notice == 0 )
    {
	dialog_open( NULL, wm_get_string( STR_WM_LOWRES_IOS_NOTICE ), wm_get_string( STR_WM_OK ), DIALOG_FLAG_SINGLE, wm );
	data->lowres_notice = 1;
    }

    prefs_ui_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}
#endif

int prefs_ui_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    prefs_ui_data* data = (prefs_ui_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( prefs_ui_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;

		char ts[ 256 ];

		btn_autoalign_data aa;
		btn_autoalign_init( &aa, wm, 0 );

#ifdef UI_PREFS_WIN_PARS
		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->window_pars = new_window( wm_get_string( STR_WM_WINDOW_PARS ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->window_pars, prefs_ui_window_pars_handler, data, wm );
		btn_autoalign_add( &aa, data->window_pars, 0 );
#endif
#ifdef UI_PREFS_SYSBARS
    #ifdef OS_ANDROID
		if( g_android_version_nums[ 0 ] >= 4 )
    #endif
		{
		    wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		    data->sysbars = new_window( wm_get_string( STR_WM_HIDE_SYSTEM_BARS ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		    set_handler( data->sysbars, prefs_ui_sysbars_handler, data, wm );
		    btn_autoalign_add( &aa, data->sysbars, 0 );
		}
#endif
#ifdef UI_PREFS_LOWRES
		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->lowres = new_window( wm_get_string( STR_WM_LOWRES ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->lowres, prefs_ui_lowres_handler, data, wm );
		btn_autoalign_add( &aa, data->lowres, 0 );
#endif

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->scale = new_window( wm_get_string( STR_WM_UI_SCALE ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->scale, prefs_ui_scale_handler, data, wm );
		btn_autoalign_add( &aa, data->scale, 0 );

		btn_autoalign_next_line( &aa, BTN_AUTOALIGN_LINE_EVENLY );

#ifdef SCREEN_ROTATE_SUPPORTED
		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->angle = new_window( wm_get_string( STR_WM_UI_ROTATION ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->angle, prefs_ui_angle_handler, data, wm );
		button_set_menu( data->angle, "0\n90\n180\n270" );
		btn_autoalign_add( &aa, data->angle, 0 );
#endif

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->maxfps = new_window( wm_get_string( STR_WM_MAXFPS ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->maxfps, prefs_ui_fps_handler, data, wm );
		ts[ 0 ] = 0;
		smem_strcat( ts, sizeof( ts ), wm_get_string( STR_WM_AUTO ) );
		for( int i = 0; i < g_fps_vals_num; i++ )
		{
		    char ts2[ 32 ];
		    ts2[ 0 ] = 0xA;
		    int_to_string( g_fps_vals[ i ], ts2 + 1 );
		    smem_strcat( ts, sizeof( ts ), ts2 );
		}
		button_set_menu( data->maxfps, ts );
		btn_autoalign_add( &aa, data->maxfps, 0 );

		btn_autoalign_next_line( &aa, BTN_AUTOALIGN_LINE_EVENLY );

		if( ( wm->prefs_flags & PREFS_FLAG_NO_COLOR_THEME ) == 0 )
		{
		    wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		    data->color = new_window( wm_get_string( STR_WM_COLOR_THEME ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		    set_handler( data->color, prefs_ui_color_handler, data, wm );
		    btn_autoalign_add( &aa, data->color, 0 );
		}

		if( ( wm->prefs_flags & PREFS_FLAG_NO_FONTS ) == 0 )
		{
		    wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		    data->fonts = new_window( wm_get_string( STR_WM_FONTS ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		    set_handler( data->fonts, prefs_ui_fonts_handler, data, wm );
		    btn_autoalign_add( &aa, data->fonts, 0 );
		}

		btn_autoalign_next_line( &aa, BTN_AUTOALIGN_LINE_EVENLY );

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->control = new_window( wm_get_string( STR_WM_CTL_TYPE ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->control, prefs_ui_control_handler, data, wm );
		button_set_menu( data->control, wm_get_string( STR_WM_CTL_TYPE_MENU ) );
		btn_autoalign_add( &aa, data->control, 0 );

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->zoom_btns = new_window( wm_get_string( STR_WM_SHOW_ZOOM_BUTTONS ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->zoom_btns, prefs_ui_zoom_btns_handler, data, wm );
		button_set_menu( data->zoom_btns, wm_get_string( STR_WM_AUTO_YES_NO_MENU ) );
		btn_autoalign_add( &aa, data->zoom_btns, 0 );

		if( wm->prefs_flags & PREFS_FLAG_NO_CONTROL_TYPE )
		{
		    data->control->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
		    data->zoom_btns->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
		}

		btn_autoalign_next_line( &aa, BTN_AUTOALIGN_LINE_EVENLY );

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->dclick = new_window( wm_get_string( STR_WM_DOUBLE_CLICK_TIME ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->dclick, prefs_ui_dclick_handler, data, wm );
		ts[ 0 ] = 0;
		smem_strcat( ts, sizeof( ts ), wm_get_string( STR_WM_AUTO ) );
		smem_strcat( ts, sizeof( ts ), "\n100\n150\n200\n250\n300\n350\n400\n450\n500" );
		button_set_menu( data->dclick, ts );
		btn_autoalign_add( &aa, data->dclick, 0 );

		btn_autoalign_next_line( &aa, BTN_AUTOALIGN_LINE_EVENLY );

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->virt_kbd = new_window( wm_get_string( STR_WM_SHOW_KBD ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->virt_kbd, prefs_ui_virt_kbd_handler, data, wm );
		button_set_menu( data->virt_kbd, wm_get_string( STR_WM_AUTO_ON_OFF_MENU ) );
		btn_autoalign_add( &aa, data->virt_kbd, 0 );

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->keymap = new_window( wm_get_string( STR_WM_SHORTCUTS_SHORT ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->keymap, prefs_ui_keymap_handler, data, wm );
		btn_autoalign_add( &aa, data->keymap, 0 );

		if( wm->prefs_flags & PREFS_FLAG_NO_KEYMAP )
		{
		    data->keymap->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
		}

		btn_autoalign_next_line( &aa, BTN_AUTOALIGN_LINE_EVENLY );

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW | BUTTON_FLAG_SHOW_PREV_VALUE;
		data->lang = new_window( wm_get_string( STR_WM_LANG ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->lang, prefs_ui_lang_handler, data, wm );
		ts[ 0 ] = 0;
		smem_strcat( ts, sizeof( ts ), wm_get_string( STR_WM_AUTO ) );
		smem_strcat( ts, sizeof( ts ), "\nEnglish\nРусский" );
		button_set_menu( data->lang, ts );
		btn_autoalign_add( &aa, data->lang, 0 );

		btn_autoalign_next_line( &aa, BTN_AUTOALIGN_LINE_EVENLY );

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->hide_recent = new_window( wm_get_string( STR_WM_HIDE_RECENT_FILES ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->hide_recent, prefs_ui_hide_recent_handler, data, wm );
		btn_autoalign_add( &aa, data->hide_recent, 0 );

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->show_hidden = new_window( wm_get_string( STR_WM_SHOW_HIDDEN_FILES ), 0, 0, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->show_hidden, prefs_ui_show_hidden_handler, data, wm );
		btn_autoalign_add( &aa, data->show_hidden, 0 );

		btn_autoalign_next_line( &aa, BTN_AUTOALIGN_LINE_EVENLY );

		prefs_ui_reinit( win );

		wm->prefs_section_ysize = aa.y;

		btn_autoalign_deinit( &aa );
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
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

//
//
//

struct prefs_svideo_data
{
    WINDOWPTR win;
    WINDOWPTR cam;
    WINDOWPTR cam_rotate;
};

static void prefs_svideo_reinit( WINDOWPTR win )
{
    prefs_svideo_data* data = (prefs_svideo_data*)win->data;
    window_manager* wm = data->win->wm;

    char ts[ 512 ];

    int cam = sconfig_get_int_value( APP_CFG_CAMERA, -1111, 0 );
    if( cam == -1111 )
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_CAMERA ), wm_get_string( STR_WM_AUTO ) );
    else
    {
#if defined(OS_IOS) || defined(OS_ANDROID)
	if( cam >= 0 && cam <= 1 )
	{
	    const char* n;
	    if( cam == 0 ) n = wm_get_string( STR_WM_BACK_CAM );
	    if( cam == 1 ) n = wm_get_string( STR_WM_FRONT_CAM );
	    sprintf( ts, "%s = %d (%s)", wm_get_string( STR_WM_CAMERA ), cam, n );
	}
	else
#endif
	{
	    sprintf( ts, "%s = %d", wm_get_string( STR_WM_CAMERA ), cam );
	}
    }
    button_set_text( data->cam, ts );

    if( data->cam_rotate )
    {
	sprintf( ts, "%s = %d", wm_get_string( STR_WM_CAM_ROTATION ), sconfig_get_int_value( APP_CFG_CAM_ROTATE, 0, 0 ) );
	button_set_text( data->cam_rotate, ts );
    }
}

static int prefs_svideo_cam_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_svideo_data* data = (prefs_svideo_data*)user_data;

    int cam = win->action_result;
    if( cam < 0 || cam > 10 ) return 0;
    
    if( cam == 0 )
    {
	if( sconfig_get_int_value( APP_CFG_CAMERA, -1, 0 ) != -1 )
	{
	    sconfig_remove_key( APP_CFG_CAMERA, 0 );
	    wm->prefs_restart_request = true;
	    sconfig_save( 0 );
	}
    }
    if( cam > 0 )
    {
	cam--;
	if( sconfig_get_int_value( APP_CFG_CAMERA, -1, 0 ) != cam )
	{
	    sconfig_set_int_value( APP_CFG_CAMERA, cam, 0 );
	    wm->prefs_restart_request = true;
	    sconfig_save( 0 );
	}
    }

    prefs_svideo_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

static int prefs_svideo_cam_rotate_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_svideo_data* data = (prefs_svideo_data*)user_data;

    int prev_angle = sconfig_get_int_value( APP_CFG_CAM_ROTATE, 0, 0 );
    int angle = win->action_result * 90;
    if( angle >= 0 && angle <= 270 )
    {
	if( prev_angle != angle )
	{
	    if( angle )
		sconfig_set_int_value( APP_CFG_CAM_ROTATE, angle, 0 );
	    else
		sconfig_remove_key( APP_CFG_CAM_ROTATE, 0 );
	    sconfig_save( 0 );
	    wm->prefs_restart_request = true;
	}
    }
    
    prefs_svideo_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

int prefs_svideo_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    prefs_svideo_data* data = (prefs_svideo_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( prefs_svideo_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;
		
		char ts[ 512 ];
		int y = 0;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->cam = new_window( wm_get_string( STR_WM_CAMERA ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->cam, prefs_svideo_cam_handler, data, wm );
#if defined(OS_IOS) || defined(OS_ANDROID)
		sprintf( ts, "%s\n0 (%s)\n1 (%s)\n2\n3\n4\n5\n6\n7", wm_get_string( STR_WM_AUTO ), wm_get_string( STR_WM_BACK_CAM ), wm_get_string( STR_WM_FRONT_CAM ) );
#else
		sprintf( ts, "%s\n0\n1\n2\n3\n4\n5\n6\n7", wm_get_string( STR_WM_AUTO ) );
#endif
		button_set_menu( data->cam, ts );
		set_window_controller( data->cam, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->cam, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->cam_rotate = new_window( wm_get_string( STR_WM_CAM_ROTATION ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->cam_rotate, prefs_svideo_cam_rotate_handler, data, wm );
		button_set_menu( data->cam_rotate, "0\n90\n180\n270" );
		set_window_controller( data->cam_rotate, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->cam_rotate, 2, wm, CPERC, (WCMD)100, CSUB, (WCMD)wm->interelement_space2, CEND );
		y += wm->text_ysize + wm->interelement_space;

		prefs_svideo_reinit( win );

		wm->prefs_section_ysize = y;
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
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

//
//
//

struct prefs_audio_data
{
    WINDOWPTR win;
    WINDOWPTR buf;
    WINDOWPTR freq;
    WINDOWPTR driver;
    WINDOWPTR device;
    WINDOWPTR device_in;
    WINDOWPTR opt;
    int opt_type; //1 - ASIO; 2 - iOS Audio Unit;
};

static const char* prefs_audio_get_driver( window_manager* wm ) //with the driver name instead of AUTO
{
    sundog_sound* snd = wm->sd->ss;

    const char* drv = sconfig_get_str_value( APP_CFG_SND_DRIVER, 0, 0 );
    if( !drv )
    {
	//Auto:
        if( snd && snd->initialized )
        {
    	    //Already selected by app:
    	    drv = sundog_sound_get_driver_name( snd );
	}
	else
	{
	    //Global default:
	    drv = sundog_sound_get_default_driver();
	}
    }

    return drv;
}

static void prefs_audio_reinit( WINDOWPTR win )
{
    prefs_audio_data* data = (prefs_audio_data*)win->data;
    window_manager* wm = data->win->wm;
    sundog_sound* snd = wm->sd->ss;

    char ts[ 512 ];

    const char* cur_drv = prefs_audio_get_driver( wm );

    char* drv = sconfig_get_str_value( APP_CFG_SND_DRIVER, NULL, 0 );
    while( 1 )
    {
	data->opt_type = 0;
#ifdef OS_WIN
	if( smem_strstr( drv, "asio" ) )
	{
	    data->opt_type = 1;
	    rename_window( data->opt, wm_get_string( STR_WM_ADD_OPTIONS_ASIO ) );
	    if( show_window2( data->opt ) )
		recalc_regions( wm );
	    break;
	}
#endif
#ifdef OS_IOS
    #ifndef AUDIOUNIT_EXTENSION
	if( !drv || smem_strstr( drv, "audiounit" ) )
	{
	    data->opt_type = 2;
	    if( show_window2( data->opt ) )
		recalc_regions( wm );
	    break;
	}
    #endif
#endif
	if( hide_window2( data->opt ) )
	    recalc_regions( wm );
	break;
    }
    if( !drv )
    {
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_DRIVER ), wm_get_string( STR_WM_AUTO ) );
    }
    else
    {
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_DRIVER ), drv );
	char** names = NULL;
	char** infos = NULL;
	int drivers = sundog_sound_get_drivers( &names, &infos );
	if( ( drivers > 0 ) && names && infos )
	{
	    for( int d = 0; d < drivers; d++ )
	    {
		if( smem_strcmp( names[ d ], drv ) == 0 )
		{
		    sprintf( ts, "%s = %s", wm_get_string( STR_WM_DRIVER ), infos[ d ] );
		    break;
		}
	    }
	    for( int d = 0; d < drivers; d++ )
	    {
		smem_free( names[ d ] );
		smem_free( infos[ d ] );
	    }
	    smem_free( names );
	    smem_free( infos );
	}
    }
    button_set_text( data->driver, ts );

    char* dev = sconfig_get_str_value( APP_CFG_SND_DEV, 0, 0 );
    if( !dev )
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_OUTPUT ), wm_get_string( STR_WM_AUTO ) );
    else
    {
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_OUTPUT ), dev );
	char** names = NULL;
	char** infos = NULL;
	int devices = sundog_sound_get_devices( cur_drv, &names, &infos, 0 );
	if( ( devices > 0 ) && names && infos )
	{
	    for( int d = 0; d < devices; d++ )
	    {
		if( smem_strcmp( names[ d ], dev ) == 0 )
		{
		    ts[ 0 ] = 0;
		    smem_strcat( ts, sizeof( ts ), wm_get_string( STR_WM_OUTPUT ) );
		    smem_strcat( ts, sizeof( ts ), " = " );
		    smem_strcat( ts, sizeof( ts ), infos[ d ] );
		    break;
		}
	    }
	    for( int d = 0; d < devices; d++ )
	    {
		smem_free( names[ d ] );
		smem_free( infos[ d ] );
	    }
	    smem_free( names );
	    smem_free( infos );
	}
    }
    button_set_text( data->device, ts );

    dev = sconfig_get_str_value( APP_CFG_SND_DEV_IN, 0, 0 );
    if( !dev )
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_INPUT ), wm_get_string( STR_WM_AUTO ) );
    else
    {
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_INPUT ), dev );
	char** names = NULL;
	char** infos = NULL;
	int devices = sundog_sound_get_devices( cur_drv, &names, &infos, 1 );
	if( ( devices > 0 ) && names && infos )
	{
	    for( int d = 0; d < devices; d++ )
	    {
		if( smem_strcmp( names[ d ], dev ) == 0 )
		{
		    ts[ 0 ] = 0;
		    smem_strcat( ts, sizeof( ts ), wm_get_string( STR_WM_INPUT ) );
		    smem_strcat( ts, sizeof( ts ), " = " );
		    smem_strcat( ts, sizeof( ts ), infos[ d ] );
		    break;
		}
	    }
	    for( int d = 0; d < devices; d++ )
	    {
		smem_free( names[ d ] );
		smem_free( infos[ d ] );
	    }
	    smem_free( names );
	    smem_free( infos );
	}
    }
    button_set_text( data->device_in, ts );

    int freq = 44100;
    if( snd && snd->initialized )
        freq = snd->freq;
    int size = sconfig_get_int_value( APP_CFG_SND_BUF, 0, 0 );
    if( size == 0 )
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_BUFFER ), wm_get_string( STR_WM_AUTO ) );
    else
	sprintf( ts, "%s = %d; %d %s", wm_get_string( STR_WM_BUFFER ), size, ( size * 1000 ) / freq, wm_get_string( STR_WM_MS ) );
    button_set_text( data->buf, ts );

    freq = sconfig_get_int_value( APP_CFG_SND_FREQ, 0, 0 );
    if( freq == 0 )
	sprintf( ts, "%s = %s", wm_get_string( STR_WM_SAMPLE_RATE ), wm_get_string( STR_WM_AUTO ) );
    else
	sprintf( ts, "%s = %d %s", wm_get_string( STR_WM_SAMPLE_RATE ), freq, wm_get_string( STR_WM_HZ ) );
    button_set_text( data->freq, ts );
}

static int prefs_audio_driver_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_audio_data* data = (prefs_audio_data*)user_data;
    
    size_t menu_size = 8192;
    char* menu = SMEM_ALLOC2( char, menu_size );
    menu[ 0 ] = 0;
    
    smem_strcat( menu, menu_size, wm_get_string( STR_WM_AUTO ) );
    
    char** names = 0;
    char** infos = 0;
    int drivers = sundog_sound_get_drivers( &names, &infos );
    if( ( drivers > 0 ) && names && infos )
    {
	smem_strcat( menu, menu_size, "\n" );
	
	for( int d = 0; d < drivers; d++ )
	{
	    smem_strcat( menu, menu_size, infos[ d ] );
	    if( d != drivers - 1 )
		smem_strcat( menu, menu_size, "\n" );
	}

	int sel = popup_menu( wm_get_string( STR_WM_DRIVER ), menu, win->screen_x, win->screen_y, wm->menu_color, wm );
	if( (unsigned)sel < (unsigned)drivers + 1 )
	{
	    if( sel == 0 )
	    {
		//Auto:
		if( sconfig_get_str_value( APP_CFG_SND_DRIVER, 0, 0 ) != 0 )
		{
		    sconfig_remove_key( APP_CFG_SND_DRIVER, 0 );
		    sconfig_remove_key( APP_CFG_SND_DEV, 0 );
		    sconfig_remove_key( APP_CFG_SND_DEV_IN, 0 );
            	    wm->prefs_restart_request = true;
        	    sconfig_save( 0 );
		}
	    }
	    else
	    {
		if( smem_strcmp( sconfig_get_str_value( APP_CFG_SND_DRIVER, "", 0 ), names[ sel - 1 ] ) )
		{
		    sconfig_set_str_value( APP_CFG_SND_DRIVER, names[ sel - 1 ], 0 );
		    sconfig_remove_key( APP_CFG_SND_DEV, 0 );
		    sconfig_remove_key( APP_CFG_SND_DEV_IN, 0 );
		    wm->prefs_restart_request = true;
            	    sconfig_save( 0 );
		}
	    }
	}

	for( int d = 0; d < drivers; d++ )
	{
	    smem_free( names[ d ] );
	    smem_free( infos[ d ] );
	}
	smem_free( names );
	smem_free( infos );
    }
    
    smem_free( menu );

    prefs_audio_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

static int prefs_audio_device_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_audio_data* data = (prefs_audio_data*)user_data;
    sundog_sound* snd = wm->sd->ss;
    
    bool input = 0;
    const char* key = APP_CFG_SND_DEV;
    if( win == data->device_in )
    {
	input = 1;
	key = APP_CFG_SND_DEV_IN;
    }

    const char* cur_drv = prefs_audio_get_driver( wm );
    
    size_t menu_size = 8192;
    char* menu = SMEM_ALLOC2( char, menu_size );
    menu[ 0 ] = 0;
    
    smem_strcat( menu, menu_size, wm_get_string( STR_WM_AUTO ) );
    
    char** names = 0;
    char** infos = 0;
    int devices = sundog_sound_get_devices( cur_drv, &names, &infos, input );
    if( ( devices > 0 ) && names && infos )
    {
	smem_strcat( menu, menu_size, "\n" );
	
	for( int d = 0; d < devices; d++ )
	{
	    smem_strcat( menu, menu_size, infos[ d ] );
	    if( d != devices - 1 )
		smem_strcat( menu, menu_size, "\n" );
	}

	int sel;
	if( input )
	    sel = popup_menu( wm_get_string( STR_WM_INPUT_DEVICE ), menu, win->screen_x, win->screen_y, wm->menu_color, wm );
	else
	    sel = popup_menu( wm_get_string( STR_WM_DEVICE ), menu, win->screen_x, win->screen_y, wm->menu_color, wm );
	if( (unsigned)sel < (unsigned)devices + 1 )
	{
	    if( sel == 0 )
	    {
		//Auto:
		if( sconfig_get_str_value( key, 0, 0 ) != 0 )
		{
		    sconfig_remove_key( key, 0 );
            	    wm->prefs_restart_request = true;
        	    sconfig_save( 0 );
		}
	    }
	    else
	    {
		if( smem_strcmp( sconfig_get_str_value( key, "", 0 ), names[ sel - 1 ] ) )
		{
		    sconfig_set_str_value( key, names[ sel - 1 ], 0 );
		    wm->prefs_restart_request = true;
            	    sconfig_save( 0 );
		}
	    }
	}

	for( int d = 0; d < devices; d++ )
	{
	    smem_free( names[ d ] );
	    smem_free( infos[ d ] );
	}
	smem_free( names );
	smem_free( infos );
    }
    
    smem_free( menu );    

    prefs_audio_reinit( data->win );
    draw_window( data->win, wm );

    return 0;
}

static const int g_sound_buf_size_table[] = 
{
    128,
    256, 
    512,
    768,
    1024,
    1280,
    1536,
    1792,
    2048,
    2560,
    3072,
    4096,
    -1
};

static int prefs_audio_buf_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_audio_data* data = (prefs_audio_data*)user_data;
    sundog_sound* snd = wm->sd->ss;
    
    size_t menu_size = 8192;
    char* menu = SMEM_ALLOC2( char, menu_size );
    menu[ 0 ] = 0;
    smem_strcat( menu, menu_size, wm_get_string( STR_WM_AUTO ) );
    
    int i = 0;
    int size = 0;

#if defined(OS_MACOS)
#else
    int freq = 44100;
    if( snd && snd->initialized )
        freq = snd->freq;
    char ts[ 512 ];
    while( 1 )
    {
	size = g_sound_buf_size_table[ i ];
	if( size == -1 ) break;
	sprintf( ts, "\n%d; %d %s", size, ( size * 1000 ) / freq, wm_get_string( STR_WM_MS ) ); 
	smem_strcat( menu, menu_size, ts );
	i++;
    }
#endif
    
    size = 0;
    int v = popup_menu( wm_get_string( STR_WM_BUFFER_SIZE ), menu, win->screen_x, win->screen_y, wm->menu_color, wm );
    if( v > 0 && v < i + 1 )
	size = g_sound_buf_size_table[ v - 1 ];
    
    smem_free( menu );
    
    if( v == 0 || size > 0 )
    {
	if( v == 0 )
	{
	    if( sconfig_get_int_value( APP_CFG_SND_BUF, -1, 0 ) != -1 )
	    {
		sconfig_remove_key( APP_CFG_SND_BUF, 0 );
		wm->prefs_restart_request = true;
		sconfig_save( 0 );
	    }
	}
	if( size > 0 )
	{
	    if( sconfig_get_int_value( APP_CFG_SND_BUF, -1, 0 ) != size )
	    {
		sconfig_set_int_value( APP_CFG_SND_BUF, size, 0 );
		wm->prefs_restart_request = true;
		sconfig_save( 0 );
	    }
	}
    }

    prefs_audio_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

static int prefs_audio_freq_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_audio_data* data = (prefs_audio_data*)user_data;
    
    int freq = 0;
    int v = win->action_result;
    switch( v )
    {
	case 1: freq = 44100; break;
	case 2: freq = 48000; break;
	case 3: freq = 88200; break;
	case 4: freq = 96000; break;
	case 5: freq = 192000; break;
    }
    
    if( v == 0 || freq > 0 )
    {
	if( v == 0 )
	{
	    if( sconfig_get_int_value( APP_CFG_SND_FREQ, -1, 0 ) != -1 )
	    {
		sconfig_remove_key( APP_CFG_SND_FREQ, 0 );
		wm->prefs_restart_request = true;
		sconfig_save( 0 );
	    }
	}
	if( freq > 0 )
	{
	    if( sconfig_get_int_value( APP_CFG_SND_FREQ, -1, 0 ) != freq )
	    {
		sconfig_set_int_value( APP_CFG_SND_FREQ, freq, 0 );
		wm->prefs_restart_request = true;
		sconfig_save( 0 );
	    }
	}
    }

    prefs_audio_reinit( data->win );
    draw_window( data->win, wm );
    
    return 0;
}

#ifdef OS_WIN
static int prefs_audio_opt_asio_dialog_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    dialog_item* dlist = dialog_get_items( win );
    if( win->action_result == 0 )
    {
        bool changed = 0;
        int out_ch = dialog_get_item( dlist, 'outc' )->int_val;
    	if( sconfig_get_int_value( "audio_ch", 0, 0 ) != out_ch )
	{
    	    sconfig_set_int_value( "audio_ch", out_ch, 0 );
    	    changed = 1;
    	}
        int in_ch = dialog_get_item( dlist, 'inpc' )->int_val;
	if( sconfig_get_int_value( "audio_ch_in", 0, 0 ) != in_ch )
    	{
    	    sconfig_set_int_value( "audio_ch_in", in_ch, 0 );
    	    changed = 1;
    	}
    	if( changed )
    	{
    	    wm->prefs_restart_request = true;
    	    sconfig_save( 0 );
    	}
    }
    return 1;
}
#endif

#ifdef OS_IOS
static int prefs_audio_opt_au_dialog_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    dialog_item* dlist = dialog_get_items( win );
    if( win->action_result == 0 )
    {
        bool changed = 0;
        int m = dialog_get_item( dlist, 'meas' )->int_val;
    	if( sconfig_get_int_value( "audio_mode_measurement", 0, NULL ) != m )
	{
	    if( m == 0 )
		sconfig_remove_key( "audio_mode_measurement", NULL );
	    else
    		sconfig_set_int_value( "audio_mode_measurement", m, NULL );
    	    changed = 1;
    	}
    	if( changed )
    	{
    	    wm->prefs_restart_request = true;
    	    sconfig_save( 0 );
    	}
    }
    return 1;
}
#endif

static int prefs_audio_opt_handler( void* user_data, WINDOWPTR win, window_manager* wm )
{
    prefs_audio_data* data = (prefs_audio_data*)user_data;

    dialog_item* dlist = NULL;
    WINDOWPTR dwin = NULL;

#ifdef OS_WIN
    if( data->opt_type == 1 )
    {
	//ASIO
	while( 1 )
	{
    	    dialog_item* di = NULL;

    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_LABEL;
    	    di->str_val = (char*)wm_get_string( STR_WM_FIRST_OUT_CH );

    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_NUMBER;
    	    di->min = 0;
    	    di->max = 64;
    	    di->int_val = sconfig_get_int_value( "audio_ch", 0, 0 );
    	    di->id = 'outc';

    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_LABEL;
    	    di->str_val = (char*)wm_get_string( STR_WM_FIRST_IN_CH );

    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_NUMBER;
    	    di->min = 0;
    	    di->max = 64;
    	    di->int_val = sconfig_get_int_value( "audio_ch_in", 0, 0 );
    	    di->id = 'inpc';

    	    wm->opt_dialog_items = dlist;
    	    dwin = dialog_open( wm_get_string( STR_WM_ASIO_OPTIONS ), NULL, wm_get_string( STR_WM_OKCANCEL ), DIALOG_FLAG_SINGLE, wm ); //retval = decorator
    	    if( !dwin ) break;

    	    set_handler( dwin->childs[ 0 ], prefs_audio_opt_asio_dialog_handler, user_data, wm );
    	    dialog_set_flags( dwin, DIALOG_FLAG_AUTOREMOVE_ITEMS );
    	    dlist = NULL; //because we use DIALOG_FLAG_AUTOREMOVE_ITEMS

    	    break;
    	}
    }
#endif
#ifdef OS_IOS
    if( data->opt_type == 2 )
    {
	//Audio Unit
	while( 1 )
	{
    	    dialog_item* di = NULL;

    	    di = dialog_new_item( &dlist ); if( !di ) break;
    	    di->type = DIALOG_ITEM_CHECKBOX;
    	    di->str_val = (char*)wm_get_string( STR_WM_MEASUREMENT_MODE );
    	    di->int_val = sconfig_get_int_value( "audio_mode_measurement", 0, 0 );
    	    di->id = 'meas';

    	    wm->opt_dialog_items = dlist;
    	    dwin = dialog_open( wm_get_string( STR_WM_ADD_OPTIONS ), NULL, wm_get_string( STR_WM_OKCANCEL ), DIALOG_FLAG_SINGLE, wm ); //retval = decorator
    	    if( !dwin ) break;

    	    set_handler( dwin->childs[ 0 ], prefs_audio_opt_au_dialog_handler, user_data, wm );
    	    dialog_set_flags( dwin, DIALOG_FLAG_AUTOREMOVE_ITEMS );
    	    dlist = NULL; //because we use DIALOG_FLAG_AUTOREMOVE_ITEMS

    	    break;
    	}
    }
#endif

    smem_free( dlist );

    return 0;
}

int prefs_audio_handler( sundog_event* evt, window_manager* wm )
{
    if( window_handler_check_data( evt, wm ) ) return 0;
    int retval = 0;
    WINDOWPTR win = evt->win;
    prefs_audio_data* data = (prefs_audio_data*)win->data;
    switch( evt->type )
    {
	case EVT_GETDATASIZE:
	    retval = sizeof( prefs_audio_data );
	    break;
	case EVT_AFTERCREATE:
	    {
		data->win = win;

		char ts[ 512 ];
		int y = 0;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->driver = new_window( "Driver", 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->driver, prefs_audio_driver_handler, data, wm );
		set_window_controller( data->driver, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->driver, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->device = new_window( "Output device", 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->device, prefs_audio_device_handler, data, wm );
		set_window_controller( data->device, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->device, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->device_in = new_window( "Input device", 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->device_in, prefs_audio_device_handler, data, wm );
		set_window_controller( data->device_in, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->device_in, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->buf = new_window( "Audio buffer size", 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->buf, prefs_audio_buf_handler, data, wm );
		set_window_controller( data->buf, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->buf, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->freq = new_window( wm_get_string( STR_WM_SAMPLE_RATE ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->freq, prefs_audio_freq_handler, data, wm );
#if defined(ONLY44100)
		sprintf( ts, "%s", wm_get_string( STR_WM_AUTO ) );
#else
		sprintf( ts, "%s\n44100\n48000\n88200\n96000\n192000", wm_get_string( STR_WM_AUTO ) );
#endif
		button_set_menu( data->freq, ts );
		set_window_controller( data->freq, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->freq, 2, wm, CPERC, (WCMD)100, CEND );
		y += wm->text_ysize + wm->interelement_space;

		wm->opt_button_flags = BUTTON_FLAG_LEFT_ALIGNMENT_ON_OVERFLOW;
		data->opt = new_window( wm_get_string( STR_WM_ADD_OPTIONS ), 0, y, 1, wm->text_ysize, wm->button_color, win, button_handler, wm );
		set_handler( data->opt, prefs_audio_opt_handler, data, wm );
		set_window_controller( data->opt, 0, wm, (WCMD)0, CEND );
		set_window_controller( data->opt, 2, wm, CPERC, (WCMD)100, CEND );
		data->opt->flags |= WIN_FLAG_ALWAYS_INVISIBLE;
		y += wm->text_ysize + wm->interelement_space;

		prefs_audio_reinit( win );

		wm->prefs_section_ysize = y;
	    }
	    retval = 1;
	    break;
	case EVT_DRAW:
	    {
		wbd_lock( win );
		wm->cur_font_color = wm->color2;
		draw_frect( 0, 0, win->xsize, win->ysize, win->color, wm );
		sundog_sound* snd = wm->sd->ss;
                if( snd )
                {
                    char ts[ 512 ];
                    int y = data->freq->y + data->freq->ysize + wm->interelement_space;
                    if( data->opt->visible )
                        y = data->opt->y + data->opt->ysize + wm->interelement_space;
                    ts[ 0 ] = 0;
                    smem_strcat( ts, sizeof( ts ), wm_get_string( STR_WM_CUR_DRIVER ) );
                    smem_strcat( ts, sizeof( ts ), ": " );
                    smem_strcat( ts, sizeof( ts ), sundog_sound_get_driver_info( snd ) );
                    draw_string( ts, 0, y, wm );
                    sprintf( ts, "%s: %d %s", wm_get_string( STR_WM_CUR_SAMPLE_RATE ), snd->freq, wm_get_string( STR_WM_HZ ) );
                    draw_string( ts, 0, y + char_y_size( wm ), wm );
                    sprintf( ts, "%s: %d; %d %s", wm_get_string( STR_WM_CUR_LATENCY ), snd->out_latency, ( snd->out_latency * 1000 ) / snd->freq, wm_get_string( STR_WM_MS ) );
                    draw_string( ts, 0, y + char_y_size( wm ) * 2, wm );
                }
                wbd_draw( wm );
                wbd_unlock( wm );
	    }
	    retval = 1;
	    break;
	case EVT_MOUSEBUTTONDOWN:
	case EVT_MOUSEMOVE:
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

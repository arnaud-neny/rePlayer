#pragma once

#define DRAW_CTL_INFO_FLAG_RANGEONLY 	1

#define MAX_CURVE_YSIZE			( wm->scrollbar_size * 6 )

int psynth_get_curve_ysize( int y, int ysize_max, WINDOWPTR win );

void psynth_draw_simple_grid( int x, int y, int xsize, int ysize, WINDOWPTR win );
void psynth_draw_grid( int x, int y, int xsize, int ysize, WINDOWPTR win );
void psynth_draw_ctl_info( 
    int mod_num, 
    psynth_net* pnet, 
    int ctl_num, 
    int ctl_min, 
    int ctl_max, 
    int ctl_offset, 
    int x, 
    int y, 
    uint flags,
    WINDOWPTR win );

//Module options:
struct psynth_opt_menu
{
    char*		str;
    WINDOWPTR		btn;
};
psynth_opt_menu* opt_menu_new( WINDOWPTR btn );
void opt_menu_remove( psynth_opt_menu* m );
void opt_menu_add_cstr( psynth_opt_menu* m, const char* opt_name, uint8_t val, int ctl );
void opt_menu_add( psynth_opt_menu* m, ps_string opt_name, uint8_t val, int ctl );
int opt_menu_show( psynth_opt_menu* m );

//One module (with separate SunVox engine) inside another module:
//(based on psynth_sunvox object from sunvox_engine.cpp)
#define PSYNTH_SUBMOD_YSIZE		( wm->controller_ysize * 3 + wm->interelement_space * 2 )
struct psynth_sunvox;
struct psynth_submod_data
{
    WINDOWPTR           win; //Window (itself) with submodule parameters
    psynth_net*         pnet;
    int			mod_num;
    uint		max_channels;
    uint		sunvox_flags;
    psynth_sunvox**	ps; //SunVox engine with the submodule
    
    WINDOWPTR		submod;
    WINDOWPTR		submod_selector; //Parent window! Not the selector itself
    WINDOWPTR		ctl;
    WINDOWPTR		val;
};
int psynth_submod_handler( sundog_event* evt, window_manager* wm );
//parent_mod_num - module which will use the submodule;
//max_channels - polyphony;
//ps - psynth_sunvox object;
void psynth_submod_change( WINDOWPTR win, int parent_mod_num, uint max_channels, uint sunvox_flags, psynth_sunvox** ps ); //set new parameters
#define PSYNTH_SUBMOD_REINIT_MOD	( 1 << 0 )
#define PSYNTH_SUBMOD_REINIT_CTL	( 1 << 1 )
#define PSYNTH_SUBMOD_REINIT_VAL	( 1 << 2 )
int psynth_submod_reinit( WINDOWPTR win, uint reinit_flags, uint mod_num ); //reinit some UI parts
int psynth_submod_get_ctl_num( WINDOWPTR win );
int psynth_submod_set_ctl_num( WINDOWPTR win, int ctl_num );

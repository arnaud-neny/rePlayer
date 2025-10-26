#pragma once

//PSynth (PsyTexxSynth) - SunVox audio core
//Use this header in the host code

#include "psynth.h"

//How to use:
//1) psynth_render_begin();
//2) while( ... )
//   psynth_reset_events();
//   add events via psynth_add_event();
//   psynth_render_setup() - set rendering parameters;
//   psynth_render_all() - full net rendering (buffer size = user defined; not more then max_buf_size);
//3) psynth_render_end().

PS_RETTYPE psynth_empty( PSYNTH_MODULE_HANDLER_PARAMETERS );

//Main functions:
int psynth_global_init();
int psynth_global_deinit();
void psynth_init( uint flags, int freq, int bpm, int tpl, void* host, uint base_host_version, psynth_net* pnet );
void psynth_close( psynth_net* pnet );
void psynth_clear( psynth_net* pnet );
int psynth_add_module(  
    int i,
    psynth_handler_t handler,
    const char* name,
    uint flags, 
    int x, 
    int y,
    int z, 
    int bpm, //beats per minute
    int tpl, //ticks per line
    psynth_net* pnet );
void psynth_remove_module( uint mod_num, psynth_net* pnet );
void psynth_remove_empty_modules_at_the_end( psynth_net* pnet );
smutex* psynth_get_mutex( uint mod_num, psynth_net* pnet );
int psynth_get_module_by_name( const char* name, psynth_net* pnet );
inline psynth_module* psynth_get_module( uint mod_num, psynth_net* pnet )
{
    if( (unsigned)mod_num >= (unsigned)pnet->mods_num ) return NULL;
    psynth_module* mod = &pnet->mods[ mod_num ];
    if( !( mod->flags & PSYNTH_FLAG_EXISTS ) ) return NULL;
    return mod;
}
void psynth_rename( uint mod_num, const char* name, psynth_net* pnet );
void psynth_set_flags( uint mod_num, uint32_t set, uint32_t reset, psynth_net* pnet );
void psynth_set_flags2( uint mod_num, uint32_t set, uint32_t reset, psynth_net* pnet );
void psynth_add_link( bool input, uint mod_num, int link, int link_slot, psynth_net* pnet );
void psynth_make_link( int out, int in, psynth_net* pnet ); //out.link = in; Example: out = 0.OUTPUT; in = SOME SYNTH
void psynth_make_link( int out, int in, int out_slot, int in_slot, psynth_net* pnet );
int psynth_remove_link( int out, int in, psynth_net* pnet ); //Remove connection between the modules (in any direction)
int psynth_check_link( int out, int in, psynth_net* pnet ); //Check connection between the modules; return values: 0 - no connection; 1 - in->out; 2 - out->in;
int psynth_open_midi_out( uint mod_num, char* dev_name, int channel, psynth_net* pnet );
int psynth_set_midi_prog( uint mod_num, int bank, int prog, psynth_net* pnet );
void psynth_all_midi_notes_off( uint mod_num, stime_ticks_t t, psynth_net* pnet );
void psynth_reset_events( psynth_net* pnet );
void psynth_add_event( uint mod_num, psynth_event* evt, psynth_net* pnet ); //Can change events_heap and break your links to the events! (RISK OF EVENT DAMAGE)
void psynth_multisend( psynth_module* mod, psynth_event* evt, psynth_net* pnet );
void psynth_multisend_pitch( psynth_module* mod, psynth_event* evt, psynth_net* pnet, int pitch );
PS_STYPE* psynth_get_scope_buffer( int ch, int* offset, int* size, uint mod_num, stime_ticks_t t, psynth_net* pnet );
void psynth_set_ctl2( psynth_module* mod, psynth_event* evt );
void psynth_render_begin( stime_ticks_t out_time, psynth_net* pnet );
void psynth_render_end( int frames, psynth_net* pnet );
void psynth_render_setup( int buf_size, stime_ticks_t out_time, void* in_buf, sound_buffer_type in_buf_type, int in_buf_channels, psynth_net* pnet );
void psynth_render_all( psynth_net* pnet );

//Event handling:
PS_RETTYPE psynth_handle_event( uint mod_num, psynth_event* evt, psynth_net* pnet ); //Manual event handling (without psynth_render()); for psynth_sunvox_apply_module(), etc.
PS_RETTYPE psynth_handle_ctl_event( uint mod_num, int ctl_num, int ctl_val, psynth_net* pnet );
PS_RETTYPE psynth_do_command( uint mod_num, psynth_command cmd, psynth_net* pnet ); //Do simple command (without event parameters)
int psynth_curve( uint mod_num, int curve_num, float* data, int len, bool w, psynth_net* pnet ); //retval: number of items processed successfully
//Available curves (Y=CURVE[X]):
//  MultiSynth:
//    0 - X = note (0..127); Y = velocity (0..1); 128 items;
//    1 - X = velocity (0..256); Y = velocity (0..1); 257 items;
//    2 - X = note (0..127); Y = pitch (0..1); 128 items;
//        pitch range: 0 ... 16384/65535 (note0) ... 49152/65535 (note128) ... 1; semitone = 256/65535;
//  WaveShaper:
//    0 - X = input (0..255); Y = output (0..1); 256 items;
//  MultiCtl:
//    0 - X = input (0..256); Y = output (0..1); 257 items;
//  Analog Generator, Generator:
//    0 - X = drawn waveform sample number (0..31); Y = volume (-1..1); 32 items;
//  FM2:
//    0 - X = custom waveform sample number (0..255); Y = volume (-1..1); 256 items;

//MIDI IN for notes:
void psynth_handle_midi_in_flags( uint mod_num, psynth_net* pnet );

//MIDI IN for controllers:
int psynth_set_ctl_midi_in( uint mod_num, uint ctl_num, uint midi_pars1, uint midi_pars2, psynth_net* pnet );
void psynth_handle_ctl_midi_in( psynth_midi_evt* evt, int offset, psynth_net* pnet ); //Call it from SunVox engine (audio callback -> MIDI events handling) only!

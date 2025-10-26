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
#include "sunvox_engine.h"
#define MODULE_DATA	psynth_gpio_data
#define MODULE_HANDLER	psynth_gpio
#define MODULE_INPUTS	1
#define MODULE_OUTPUTS	1
#define MAX_GPIO_PINS	256
#ifdef OS_LINUX
volatile int16_t g_gpio_pins_usage[ MAX_GPIO_PINS ];
volatile int8_t g_gpio_pins_direction[ MAX_GPIO_PINS ]; 
volatile int g_gpio_users_counter = 0;
#endif
static void gpio_init()
{
#ifdef OS_LINUX
    if( g_gpio_users_counter == 0 )
    {
	for( int i = 0; i < MAX_GPIO_PINS; i++ )
	{
	    g_gpio_pins_usage[ i ] = 0;
	    g_gpio_pins_direction[ i ] = 0;
	}
    }
    g_gpio_users_counter++;
#endif
}
static void gpio_deinit()
{
#ifdef OS_LINUX
    g_gpio_users_counter--;
    if( g_gpio_users_counter == 0 )
    {
    }
#endif
}
static void gpio_pin_lock( int p, bool out )
{
#ifdef OS_LINUX
    if( (unsigned)p >= (unsigned)MAX_GPIO_PINS ) return; 
    if( g_gpio_pins_usage[ p ] == 0 )
    {
	slog( "GPIO %d (dir:%d) locked\n", p, out );
	FILE* f = fopen( "/sys/class/gpio/export", "wb" );
	if( f )
	{
	    fprintf( f, "%d\n", p );
	    fclose( f );
	}
	if( out )
	    g_gpio_pins_direction[ p ] = -1;
	else
	    g_gpio_pins_direction[ p ] = -2;
    }
    g_gpio_pins_usage[ p ]++;
#endif
}
static void gpio_pin_unlock( int p )
{
#ifdef OS_LINUX
    if( (unsigned)p >= (unsigned)MAX_GPIO_PINS ) return; 
    g_gpio_pins_usage[ p ]--;
    if( g_gpio_pins_usage[ p ] == 0 )
    {
	slog( "GPIO %d unlocked\n", p );
	FILE* f = fopen( "/sys/class/gpio/unexport", "wb" );
	if( f )
	{
	    fprintf( f, "%d\n", p );
	    fclose( f );
	}
	g_gpio_pins_direction[ p ] = 0;
    }
#endif
}
static void gpio_set_direction( int p )
{
#ifdef OS_LINUX
    char ts[ 512 ];
    if( (unsigned)p >= (unsigned)MAX_GPIO_PINS ) return; 
    if( g_gpio_pins_direction[ p ] < 0 )
    {
	sprintf( ts, "/sys/class/gpio/gpio%d/direction", p );
	FILE* f = fopen( ts, "wb" );
	if( f )
	{
	    if( g_gpio_pins_direction[ p ] == -1 )
	    {
		fprintf( f, "out\n" );
		g_gpio_pins_direction[ p ] = 1;
	    }
	    else
	    {
		fprintf( f, "in\n" );
		g_gpio_pins_direction[ p ] = 2;
	    }
	    fclose( f );
	}
    }
#endif
}
static void gpio_pin_write( int p, bool v )
{
#ifdef OS_LINUX
    char ts[ 512 ];
    if( (unsigned)p >= (unsigned)MAX_GPIO_PINS ) return; 
    gpio_set_direction( p );
    sprintf( ts, "/sys/class/gpio/gpio%d/value", p );
    FILE* f = fopen( ts, "wb" );
    if( f )
    {
	fprintf( f, "%d\n", v );
        fclose( f );
    }
#endif
}
static bool gpio_pin_read( int p )
{
#ifdef OS_LINUX
    bool rv = 0;
    char ts[ 512 ];
    if( (unsigned)p >= (unsigned)MAX_GPIO_PINS ) return 0; 
    gpio_set_direction( p );
    sprintf( ts, "/sys/class/gpio/gpio%d/value", p );
    FILE* f = fopen( ts, "rb" );
    if( f )
    {
	if( fgetc( f ) == '1' )
	    rv = 1;
        fclose( f );
    }
    return rv;
#else
    return 0;
#endif
}
struct MODULE_DATA
{
    PS_CTYPE   		ctl_out;
    PS_CTYPE   		ctl_out_pin;
    PS_CTYPE		ctl_out_threshold;
    PS_CTYPE   		ctl_in;
    PS_CTYPE   		ctl_in_pin;
    PS_CTYPE		ctl_in_note;
    PS_CTYPE		ctl_in_amp;
    int			cur_out;
    int			cur_in;
    bool		prev_in_level;
    psynth_event	note_evt;
};
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
	    retval = (PS_RETTYPE)"GPIO";
	    break;
	case PS_CMD_GET_INFO:
	    {
                const char* lang = slocale_get_lang();
                while( 1 )
                {
                    if( smem_strstr( lang, "ru_" ) )
                    {
                        retval = (PS_RETTYPE)"При помощи этого модуля можно, например, посылать сигналы на внешние светодиоды, или опрашивать кнопки, подключенные к \"ногам\" GPIO на системной плате.\nДля работы модуля нужен Linux со включенным интерфейсом GPIO.";
                        break;
                    }
		    retval = (PS_RETTYPE)"With this module you can use the General-Purpose Input/Output pins of the device board. For example, you can send some signals to the LEDs, or receive the ON/OFF messages from the buttons.\nRequirements: Linux with GPIO Sysfs Interface enabled.";
                    break;
                }
            }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#847FFF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 7, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_OUT ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_out, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_OUT_PIN ), "", 0, MAX_GPIO_PINS, 0, 1, &data->ctl_out_pin, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_OUT_THRESHOLD ), "%", 0, 100, 50, 0, &data->ctl_out_threshold, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_IN ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_in, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_IN_PIN ), "", 0, MAX_GPIO_PINS, 0, 1, &data->ctl_in_pin, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_IN_NOTE ), "", 0, 128, 0, 1, &data->ctl_in_note, -1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_IN_AMP ), "%", 0, 100, 100, 0, &data->ctl_in_amp, -1, 1, pnet );
	    data->cur_out = -1;
	    data->cur_in = -1;
	    data->prev_in_level = 0;
	    smem_clear( &data->note_evt, sizeof( data->note_evt ) );
	    gpio_init();
	    retval = 1;
	    break;
	case PS_CMD_CLEAN:
	    retval = 1;
	    break;
	case PS_CMD_RENDER_REPLACE:
	    {
		PS_STYPE** inputs = mod->channels_in;
		PS_STYPE** outputs = mod->channels_out;
		int offset = mod->offset;
		int frames = mod->frames;
		if( data->ctl_out )
		{
		    if( data->ctl_out_pin != data->cur_out )
		    {
			gpio_pin_unlock( data->cur_out );
			gpio_pin_lock( data->ctl_out_pin, 1 );
			data->cur_out = data->ctl_out_pin;
		    }
		}
		else
		{
		    gpio_pin_unlock( data->cur_out );
		    data->cur_out = -1;
		}
		if( data->ctl_in )
		{
		    if( data->ctl_in_pin != data->cur_in )
		    {
			gpio_pin_unlock( data->cur_in );
			gpio_pin_lock( data->ctl_in_pin, 0 );
			data->cur_in = data->ctl_in_pin;
		    }
		}
		else
		{
		    gpio_pin_unlock( data->cur_in );
		    data->cur_in = -1;
		}
		if( data->ctl_out )
		{
		    PS_STYPE* in = inputs[ 0 ] + offset;
		    PS_STYPE2 t = ( PS_STYPE_ONE * data->ctl_out_threshold ) / (PS_STYPE2)100;
		    bool v = 0;
		    for( int i = 0; i < frames; i++ )
		    {
			if( ( in[ i ] > t ) || ( -in[ i ] > t ) ) 
			{
			    v = 1;
			    break;
			}
		    }
		    gpio_pin_write( data->ctl_out_pin, v );
		}
		if( data->ctl_in )
		{
		    PS_STYPE* out = outputs[ 0 ] + offset;
		    int note_onoff = 0;
		    if( gpio_pin_read( data->ctl_in_pin ) )
		    {
			if( data->ctl_in_note > 0 )
			    if( data->prev_in_level != 1 ) note_onoff = 1;
			PS_STYPE2 v = ( PS_STYPE_ONE * data->ctl_in_amp ) / (PS_STYPE2)100;
			if( v )
			{
			    for( int i = 0; i < frames; i++ )
			    {
				out[ i ] = v;
			    }
			    retval = 1;
			}
			data->prev_in_level = 1;
		    }
		    else
		    {
			if( data->ctl_in_note > 0 )
			    if( data->prev_in_level != 0 ) note_onoff = -1;
			data->prev_in_level = 0;
		    }
		    if( note_onoff != 0 )
		    {
	    		data->note_evt.offset = offset;
			data->note_evt.id = ( SUNVOX_VIRTUAL_PATTERN_GPIO << 16 ) | ( mod_num & 65535 );
			if( note_onoff == 1 )
			    data->note_evt.command = PS_CMD_NOTE_ON;
			else
			    data->note_evt.command = PS_CMD_NOTE_OFF;
			data->note_evt.note.velocity = 256;
        		data->note_evt.note.pitch = PS_NOTE_TO_PITCH( data->ctl_in_note - 1 );
        		if( pnet->base_host_version >= 0x01090300 )
        		{
        		    data->note_evt.note.pitch -= mod->finetune + mod->relative_note * 256;
        		}
			for( int i = 0; i < mod->output_links_num; i++ )
                	{
                    	    int l = mod->output_links[ i ];
                    	    if( (unsigned)l < (unsigned)pnet->mods_num )
                    	    {
                        	psynth_module* m = &pnet->mods[ l ];
                        	if( m->flags & PSYNTH_FLAG_EXISTS )
                        	{
                            	    psynth_add_event( l, &data->note_evt, pnet );
                        	}
                    	    }
                	}
		    }
		}
	    }
	    break;
	case PS_CMD_CLOSE:
	    gpio_pin_unlock( data->cur_out );
	    gpio_pin_unlock( data->cur_in );
	    gpio_deinit();
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}

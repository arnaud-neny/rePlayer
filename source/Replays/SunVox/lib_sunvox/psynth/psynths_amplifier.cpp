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
#define MODULE_DATA	psynth_amplifier_data
#define MODULE_HANDLER	psynth_amplifier
#define MODULE_INPUTS	2
#define MODULE_OUTPUTS	2
struct MODULE_DATA
{
    PS_CTYPE   	ctl_volume;
    PS_CTYPE   	ctl_pan;
    PS_CTYPE   	ctl_dc;
    PS_CTYPE	ctl_inverse;
    PS_CTYPE	ctl_width;
    PS_CTYPE	ctl_abs;
    PS_CTYPE	ctl_fine_volume;
    PS_CTYPE	ctl_gain;
    PS_CTYPE	ctl_bipolar_dc;
};
#define FORMULA_DESC "out=width(inverse(abs(pan(in*vol)+dc)))"
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
    int ch;
    switch( event->command )
    {
	case PS_CMD_GET_DATA_SIZE:
	    retval = sizeof( MODULE_DATA );
	    break;
	case PS_CMD_GET_NAME:
	    retval = (PS_RETTYPE)"Amplifier";
	    break;
	case PS_CMD_GET_INFO:
	    {
		const char* lang = slocale_get_lang();
	        while( 1 )
	        {
	            if( smem_strstr( lang, "ru_" ) )
	            {
			retval = 
			    (PS_RETTYPE)"Усилитель сигнала с дополнительными возможностями:\n * установка DC смещения;\n * инверсия сигнала;\n * установка ширины стерео-панорамы.\n"
			    "Формула: " FORMULA_DESC;
			break;
		    }
		    retval = 
			(PS_RETTYPE)"Amplifier\n" 
			"Formula: " FORMULA_DESC;
		    break;
		}
	    }
	    break;
	case PS_CMD_GET_COLOR:
	    retval = (PS_RETTYPE)"#E47FFF";
	    break;
	case PS_CMD_GET_INPUTS_NUM: retval = MODULE_INPUTS; break;
	case PS_CMD_GET_OUTPUTS_NUM: retval = MODULE_OUTPUTS; break;
	case PS_CMD_GET_FLAGS: retval = PSYNTH_FLAG_EFFECT; break;
	case PS_CMD_INIT:
	    psynth_resize_ctls_storage( mod_num, 9, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_VOLUME ), "", 0, 1024, 256, 0, &data->ctl_volume, 256, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_BALANCE ), "", 0, 256, 128, 0, &data->ctl_pan, 128, 0, pnet );
	    psynth_set_ctl_show_offset( mod_num, 1, -128, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_DC_OFFSET ), "", 0, 256, 128, 0, &data->ctl_dc, 128, 1, pnet );
	    psynth_set_ctl_show_offset( mod_num, 2, -128, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_INVERSE ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_inverse, 1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_STEREO_WIDTH ), "", 0, 256, 128, 0, &data->ctl_width, 128, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_ABSOLUTE ), ps_get_string( STR_PS_OFF_ON ), 0, 1, 0, 1, &data->ctl_abs, 1, 1, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_FINE_VOLUME ), "", 0, 32768, 32768, 0, &data->ctl_fine_volume, -1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_GAIN ), "", 0, 5000, 1, 0, &data->ctl_gain, 1, 0, pnet );
	    psynth_register_ctl( mod_num, ps_get_string( STR_PS_BIPOLAR_DC_OFFSET ), "", 0, 32768, 32768/2, 0, &data->ctl_bipolar_dc, 32768/2, 1, pnet );
	    psynth_set_ctl_show_offset( mod_num, 8, -32768/2, pnet );
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
		bool no_input_signal = true;
		for( ch = 0; ch < MODULE_OUTPUTS; ch++ )
		{
		    if( mod->in_empty[ ch ] < offset + frames )
		    {
			no_input_signal = false;
			break;
		    }
		}
		if( data->ctl_volume == 0 || data->ctl_fine_volume == 0 || data->ctl_gain == 0 ) no_input_signal = true;
		if( data->ctl_dc != 128 || data->ctl_bipolar_dc > 32768/2 ) no_input_signal = false;
		if( no_input_signal ) 
		{
		    break;
		}
		int ctl_volume = data->ctl_volume;
		int ctl_pan = data->ctl_pan;
		int ctl_dc = data->ctl_dc - 128;
		int ctl_bipolar_dc = data->ctl_bipolar_dc - 32768/2;
		PS_STYPE2 dc = ( ctl_dc * PS_STYPE_ONE ) / (PS_STYPE2)128;
		PS_STYPE2 bipolar_dc = ( ctl_bipolar_dc * PS_STYPE_ONE ) / (PS_STYPE2)(32768/2);
		for( ch = 0; ch < MODULE_OUTPUTS; ch++ )
		{
		    PS_STYPE* in = inputs[ ch ] + offset;
		    PS_STYPE* out = outputs[ ch ] + offset;
		    int vol = ctl_volume;
		    if( ctl_pan < 128 )
                    {
                        if( ch == 1 ) { vol *= ctl_pan; vol /= 128; }
                    }
                    else
                    {
                        if( ch == 0 ) { vol *= 128 - ( ctl_pan - 128 ); vol /= 128; }
                    }
                    if( vol == 0 || data->ctl_fine_volume == 0 || data->ctl_gain == 0 )
                    {
			for( int i = 0; i < frames; i++ )
			{
			    out[ i ] = 0;
			}
                    }
            	    else
            	    {
#ifdef PS_STYPE_FLOATINGPOINT
			PS_STYPE2 fvol = ( (PS_STYPE2)vol / 256.0F ) * ( (PS_STYPE2)data->ctl_fine_volume / 32768.0F ) * data->ctl_gain;
			if( fvol != 1.0F )
			{
			    for( int i = 0; i < frames; i++ )
			    {
				out[ i ] = in[ i ] * fvol;
			    }
			}
			else
			{
			    for( int i = 0; i < frames; i++ )
			    {
				out[ i ] = in[ i ];
			    }
			}
#else
			int32_t ivol = ( vol * data->ctl_fine_volume ) >> 8;
			if( ivol == 32768 )
			{
			    for( int i = 0; i < frames; i++ ) out[ i ] = in[ i ];
			}
			else
			{
			    if( ivol < 32768 )
			    {
				if( ivol == 0 )
				{
				    for( int i = 0; i < frames; i++ ) out[ i ] = 0;
				}
				else
				{
				    for( int i = 0; i < frames; i++ )
	    			    {
					PS_STYPE2 v = in[ i ];
					v *= ivol;
					v /= 32768;
					out[ i ] = v;
				    }
				}
			    }
			    else
			    {
				uint shift = 0;
				while( ( ivol >> shift ) > 32768 ) shift++;
				int32_t ivol2 = ivol >> shift;
				for( int i = 0; i < frames; i++ )
	    			{
				    PS_STYPE2 v = in[ i ];
				    v *= ivol2;
				    v /= 32768 >> shift;
				    out[ i ] = v;
				}
			    }
			}
			if( data->ctl_gain != 1 )
			{
			    for( int i = 0; i < frames; i++ )
	    		    {
			        PS_STYPE2 v = out[ i ];
			        v *= data->ctl_gain;
			        LIMIT_NUM( v, -( 1 << ( sizeof( PS_STYPE ) * 8 - 1 ) ), ( 1 << ( sizeof( PS_STYPE ) * 8 - 1 ) ) - 1 );
			        out[ i ] = v;
			    }
			}
#endif
		    }
		    if( ctl_dc )
		    {
			for( int i = 0; i < frames; i++ )
			{
			    out[ i ] = out[ i ] + dc;
			}
		    }
		    if( ctl_bipolar_dc )
		    {
		        for( int i = 0; i < frames; i++ )
		        {
		    	    PS_STYPE v = out[ i ];
			    if( v >= 0 )
			    {
			        v += bipolar_dc;
			        if( v < 0 ) v = 0;
			    }
			    else
			    {
			        v -= bipolar_dc;
			        if( v > 0 ) v = 0;
			    }
			    out[ i ] = v;
			}
		    }
		    if( data->ctl_abs )
		    {
			for( int i = 0; i < frames; i++ )
			{
			    if( out[ i ] < 0 ) out[ i ] = -out[ i ];
			}
		    }
		    if( data->ctl_inverse )
		    {
			for( int i = 0; i < frames; i++ )
			{
			    out[ i ] = -out[ i ];
			}
		    }
		}
		if( data->ctl_width != 128 )
		{
		    int ctl_width = data->ctl_width;
		    int ctl_width2 = 128 - data->ctl_width;
		    PS_STYPE* out1 = outputs[ 0 ] + offset;
		    PS_STYPE* out2 = outputs[ 1 ] + offset;
		    for( int i = 0; i < frames; i++ )
		    {
			PS_STYPE2 v1 = out1[ i ];
			PS_STYPE2 v2 = out2[ i ];
			PS_STYPE2 v3 = ( ( ( v1 + v2 ) / 2 ) * ctl_width2 ) / 128;
			v1 = ( v1 * ctl_width ) / 128;
			v2 = ( v2 * ctl_width ) / 128;
			out1[ i ] = v3 + v1;
			out2[ i ] = v3 + v2;
		    }
		}
	    }
	    retval = 1;
	    break;
	case PS_CMD_CLOSE:
	    retval = 1;
	    break;
	default: break;
    }
    return retval;
}

#pragma once

#define DC_FILTER_CHANNELS 2

struct dc_filter
{
    PS_STYPE2	dc_block_pole;
    PS_STYPE2  	dc_block_acc[ DC_FILTER_CHANNELS ];
    PS_STYPE2  	dc_block_prev_x[ DC_FILTER_CHANNELS ];
    PS_STYPE2  	dc_block_prev_y[ DC_FILTER_CHANNELS ];
    int		empty_samples_counter[ DC_FILTER_CHANNELS ];
    int		empty_frames_counter_max; //filter will be stopped after this number of frames
};

void dc_filter_init( dc_filter* f, int sample_rate );
void dc_filter_stop( dc_filter* f );
bool dc_filter_run( dc_filter* f, int ch, PS_STYPE* in, PS_STYPE* out, int frames ); //retval: true = finished (no output data); false = output filled

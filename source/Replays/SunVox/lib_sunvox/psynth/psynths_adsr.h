#pragma once

#define ADSR_CURVE_TYPES 	12
#define ADSR_SUSTAIN_MODES	3
#define ADSR_SMOOTH_MODES	4

struct adsr_env
{
    int			ctl_volume; //0...32768(norm)...32768*8
    int			ctl_a; //ms
    int			ctl_d; //ms
    int			ctl_s; //0...32768
    int			ctl_r; //ms
    int8_t		ctl_acurve;
    int8_t		ctl_dcurve;
    int8_t		ctl_rcurve;
    int8_t		ctl_sustain; //Envelope will be held at point "sustain" until the note is released
				     //0 - off; 1 - on; 2 - repeat;
    int8_t		ctl_sustain_pedal; //Hold the note (release is only possible when this pedal is OFF)
    int8_t		ctl_smooth; //0 - off; 1 - restart & volume change; 2 - restart (smoother) & volume change; 3 - volume change;

    bool                delta_recalc_request; //set it if one of the following parameters has changed: srate, ctl_a, ctl_d, ctl_r;
    int			srate; //Sampling rate (Hz)
    int                 ad; //Attack delta .15
    int                 dd; //Decay delta .15
    int                 rd; //Release delta .15
    int                 v; //Current attack/decay/sustain value .15
    int                 v2; //Current release value .15
    //int                 v3; //Final value (with envelope curve) 0..32768
    int                 v4; //Smooth transition (decreasing) .15
    PS_STYPE            v5; //Last envelope sample (before applying the volume)
    psmoother_coefs	smoother_coefs;
    psmoother		vol; //Current volume
    uint32_t            state_flags;
    bool		pressed;
    bool		pressed2;
    bool                playing;
};

void adsr_env_start( adsr_env* a );
void adsr_env_stop( adsr_env* a );
void adsr_env_reset( adsr_env* a );
void adsr_env_init( adsr_env* a, bool filled_with_zeros, int srate );
void adsr_env_change_srate( adsr_env* a, int srate );
void adsr_env_change_adr( adsr_env* a, int alen, int dlen, int rlen );
void adsr_env_deinit( adsr_env* a );
psynth_buf_state adsr_env_run( adsr_env* a, PS_STYPE* out, int frames, int* out_frames );

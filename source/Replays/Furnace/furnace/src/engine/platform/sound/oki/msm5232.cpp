// license:GPL-2.0+
// copyright-holders:Jarek Burczynski, Hiromitsu Shioya
// additional modifications for Furnace by tildearrow
#include "msm5232.h"
#include <stdlib.h>
#include <string.h>

#define CLOCK_RATE_DIVIDER 16

/*
    OKI MSM5232RS
    8 channel tone generator
*/

msm5232_device::msm5232_device(uint32_t clock)
	: m_noise_cnt(0), m_noise_step(0), m_noise_rng(0), m_noise_clocks(0), m_UpdateStep(0), m_control1(0), m_control2(0), m_gate(0), m_chip_clock(0), m_rate(0), m_clock(clock)
	, m_gate_handler_cb(NULL)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void msm5232_device::device_start()
{
	int rate = m_clock/CLOCK_RATE_DIVIDER;

	init(m_clock, rate);
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void msm5232_device::device_reset()
{
	for (int i=0; i<8; i++)
	{
		write(i, 0x80);
		write(i, 0x00);
	}
	m_noise_cnt     = 0;
	m_noise_rng     = 1;
	m_noise_clocks  = 0;

	m_control1      = 0;
	m_EN_out16[0]   = 0;
	m_EN_out8[0]    = 0;
	m_EN_out4[0]    = 0;
	m_EN_out2[0]    = 0;

	m_control2      = 0;
	m_EN_out16[1]   = 0;
	m_EN_out8[1]    = 0;
	m_EN_out4[1]    = 0;
	m_EN_out2[1]    = 0;

	gate_update();
}

//-------------------------------------------------
//  device_stop - device-specific stop
//-------------------------------------------------

void msm5232_device::device_stop()
{
}

void msm5232_device::set_capacitors(double cap1, double cap2, double cap3, double cap4, double cap5, double cap6, double cap7, double cap8)
{
	m_external_capacity[0] = cap1;
	m_external_capacity[1] = cap2;
	m_external_capacity[2] = cap3;
	m_external_capacity[3] = cap4;
	m_external_capacity[4] = cap5;
	m_external_capacity[5] = cap6;
	m_external_capacity[6] = cap7;
	m_external_capacity[7] = cap8;
}

void msm5232_device::set_vol_input(double v1, double v2, double v3, double v4, double v5, double v6, double v7, double v8)
{
	m_external_input[0] = v1;
	m_external_input[1] = v2;
	m_external_input[2] = v3;
	m_external_input[3] = v4;
	m_external_input[4] = v5;
	m_external_input[5] = v6;
	m_external_input[6] = v7;
	m_external_input[7] = v8;
}

/* Default chip clock is 2119040 Hz */
/* At this clock chip generates exactly 440.0 Hz signal on 8' output when pitch data=0x21 */


/* ROM table to convert from pitch data into data for programmable counter and binary counter */
/* Chip has 88x12bits ROM   (addressing (in hex) from 0x00 to 0x57) */
#define ROM(counter,bindiv) (counter|(bindiv<<9))

static const uint16_t MSM5232_ROM[88]={
/* higher values are Programmable Counter data (9 bits) */
/* lesser values are Binary Counter shift data (3 bits) */

/* 0 */ ROM (506, 7),

/* 1 */ ROM (478, 7),/* 2 */ ROM (451, 7),/* 3 */ ROM (426, 7),/* 4 */ ROM (402, 7),
/* 5 */ ROM (379, 7),/* 6 */ ROM (358, 7),/* 7 */ ROM (338, 7),/* 8 */ ROM (319, 7),
/* 9 */ ROM (301, 7),/* A */ ROM (284, 7),/* B */ ROM (268, 7),/* C */ ROM (253, 7),

/* D */ ROM (478, 6),/* E */ ROM (451, 6),/* F */ ROM (426, 6),/*10 */ ROM (402, 6),
/*11 */ ROM (379, 6),/*12 */ ROM (358, 6),/*13 */ ROM (338, 6),/*14 */ ROM (319, 6),
/*15 */ ROM (301, 6),/*16 */ ROM (284, 6),/*17 */ ROM (268, 6),/*18 */ ROM (253, 6),

/*19 */ ROM (478, 5),/*1A */ ROM (451, 5),/*1B */ ROM (426, 5),/*1C */ ROM (402, 5),
/*1D */ ROM (379, 5),/*1E */ ROM (358, 5),/*1F */ ROM (338, 5),/*20 */ ROM (319, 5),
/*21 */ ROM (301, 5),/*22 */ ROM (284, 5),/*23 */ ROM (268, 5),/*24 */ ROM (253, 5),

/*25 */ ROM (478, 4),/*26 */ ROM (451, 4),/*27 */ ROM (426, 4),/*28 */ ROM (402, 4),
/*29 */ ROM (379, 4),/*2A */ ROM (358, 4),/*2B */ ROM (338, 4),/*2C */ ROM (319, 4),
/*2D */ ROM (301, 4),/*2E */ ROM (284, 4),/*2F */ ROM (268, 4),/*30 */ ROM (253, 4),

/*31 */ ROM (478, 3),/*32 */ ROM (451, 3),/*33 */ ROM (426, 3),/*34 */ ROM (402, 3),
/*35 */ ROM (379, 3),/*36 */ ROM (358, 3),/*37 */ ROM (338, 3),/*38 */ ROM (319, 3),
/*39 */ ROM (301, 3),/*3A */ ROM (284, 3),/*3B */ ROM (268, 3),/*3C */ ROM (253, 3),

/*3D */ ROM (478, 2),/*3E */ ROM (451, 2),/*3F */ ROM (426, 2),/*40 */ ROM (402, 2),
/*41 */ ROM (379, 2),/*42 */ ROM (358, 2),/*43 */ ROM (338, 2),/*44 */ ROM (319, 2),
/*45 */ ROM (301, 2),/*46 */ ROM (284, 2),/*47 */ ROM (268, 2),/*48 */ ROM (253, 2),

/*49 */ ROM (478, 1),/*4A */ ROM (451, 1),/*4B */ ROM (426, 1),/*4C */ ROM (402, 1),
/*4D */ ROM (379, 1),/*4E */ ROM (358, 1),/*4F */ ROM (338, 1),/*50 */ ROM (319, 1),
/*51 */ ROM (301, 1),/*52 */ ROM (284, 1),/*53 */ ROM (268, 1),/*54 */ ROM (253, 1),

/*55 */ ROM (253, 1),/*56 */ ROM (253, 1),

/*57 */ ROM (13, 7)
};
#undef ROM


#define STEP_SH (16)    /* step calculations accuracy */

/*
 * resistance values are guesswork, default capacity is mentioned in the datasheets
 *
 * charges external capacitor (default is 0.39uF) via R51
 * in approx. 5*1400 * 0.39e-6
 *
 * external capacitor is discharged through R52
 * in approx. 5*28750 * 0.39e-6
 */


#define R51 1400    /* charge resistance */
#define R52 28750   /* discharge resistance */

#if 0
/*
    C24 = external capacity

    osd_printf_debug("Time constant T=R*C =%f sec.\n",R51*C24);
    osd_printf_debug("Cap fully charged after 5T=%f sec (sample=%f). Level=%f\n",(R51*C24)*5,(R51*C24)*5*sample_rate , VMAX*0.99326 );
    osd_printf_debug("Cap charged after 5T=%f sec (sample=%f). Level=%20.16f\n",(R51*C24)*5,(R51*C24)*5*sample_rate ,
           VMAX*(1.0-pow(2.718,-0.0748/(R51*C24))) );
*/
#endif




void msm5232_device::init_tables()
{
	int i;
	double scale;

	/* sample rate = chip clock !!!  But : */
	/* highest possible frequency is chipclock/13/16 (pitch data=0x57) */
	/* at 2MHz : 2000000/13/16 = 9615 Hz */

	i = ((double)(1<<STEP_SH) * (double)m_rate) / (double)m_chip_clock;
	m_UpdateStep = i;
	/* printf("clock=%d Hz rate=%d Hz, UpdateStep=%d\n",
	        m_chip_clock, m_rate, m_UpdateStep); */

	scale = ((double)m_chip_clock) / (double)m_rate;
	m_noise_step = ((1<<STEP_SH)/128.0) * scale; /* step of the rng reg in 16.16 format */
	/* logerror("noise step=%8x\n", m_noise_step); */

#if 0
{
	/* rate tables (in milliseconds) */
	static const int ATBL[8] = { 2,4,8,16, 32,64, 32,64};
	static const int DTBL[16]= { 40,80,160,320, 640,1280, 640,1280,
							333,500,1000,2000, 4000,8000, 4000,8000};
	for (i=0; i<8; i++)
	{
		double clockscale = (double)m_chip_clock / 2119040.0;
		double time = (ATBL[i] / 1000.0) / clockscale;  /* attack time in seconds */
		m_ar_tbl[i] = 0.50 * ( (1.0/time) / (double)m_rate );
		/* printf("ATBL[%d] = %20.16f time = %f s\n",i, m_ar_tbl[i], time); */
	}

	for (i=0; i<16; i++)
	{
		double clockscale = (double)m_chip_clock / 2119040.0;
		double time = (DTBL[i] / 1000.0) / clockscale;  /* decay time in seconds */
		m_dr_tbl[i] = 0.50 * ( (1.0/time) / (double)m_rate );
		/* printf("DTBL[%d] = %20.16f time = %f s\n",i, m_dr_tbl[i], time); */
	}
}
#endif


	for (i=0; i<8; i++)
	{
		double clockscale = (double)m_chip_clock / 2119040.0;
		m_ar_tbl[i]   = ((1<<i) / clockscale) * (double)R51;
	}

	for (i=0; i<8; i++)
	{
		double clockscale = (double)m_chip_clock / 2119040.0;
		m_dr_tbl[i]   = (     (1<<i) / clockscale) * (double)R52;
		m_dr_tbl[i+8] = (6.25*(1<<i) / clockscale) * (double)R52;
	}
}


void msm5232_device::init_voice(int i)
{
	m_voi[i].ar_rate= m_ar_tbl[0] * m_external_capacity[i];
	m_voi[i].dr_rate= m_dr_tbl[0] * m_external_capacity[i];
	m_voi[i].rr_rate= m_dr_tbl[0] * m_external_capacity[i]; /* this is constant value */
	m_voi[i].eg_sect= -1;
	m_voi[i].eg     = 0.0;
	m_voi[i].eg_arm = 0;
  m_voi[i].eg_ext = 0;
	m_voi[i].pitch  = -1.0;
  m_voi[i].mute = false;
}

void msm5232_device::mute(int voice, bool mute)
{
  m_voi[voice].mute = mute;
}

void msm5232_device::gate_update()
{
	int new_state = (m_control2 & 0x20) ? m_voi[7].GF : 0;

	if (m_gate != new_state && m_gate_handler_cb!=NULL)
	{
		m_gate = new_state;
		m_gate_handler_cb(new_state);
	}
}

int msm5232_device::get_rate() {
  return m_rate;
}

void msm5232_device::init(int clock, int rate)
{
	int j;

	m_chip_clock = clock;
	m_rate  = rate ? rate : 44100;  /* avoid division by 0 */

	init_tables();

	for (j=0; j<8; j++)
	{
		memset(&m_voi[j],0,sizeof(VOICE));
		init_voice(j);
	}
}


void msm5232_device::write(unsigned int offset, uint8_t data)
{
	if (offset > 0x0d)
		return;

	if (offset < 0x08) /* pitch */
	{
		int ch = offset&7;

		m_voi[ch].GF = ((data&0x80)>>7);
		if (ch == 7)
			gate_update();

		if(data&0x80)
		{
			if(data >= 0xd8)
			{
				/*if ((data&0x7f) != 0x5f) logerror("MSM5232: WRONG PITCH CODE = %2x\n",data&0x7f);*/
				m_voi[ch].mode = 1;     /* noise mode */
				m_voi[ch].eg_sect = 0;  /* Key On */
			}
			else
			{
				if ( m_voi[ch].pitch != (data&0x7f) )
				{
					int n;
					uint16_t pg;

					m_voi[ch].pitch = data&0x7f;

					pg = MSM5232_ROM[ data&0x7f ];

					m_voi[ch].TG_count_period = (pg & 0x1ff) * m_UpdateStep / 2;

					n = (pg>>9) & 7;    /* n = bit number for 16' output */
					m_voi[ch].TG_out16 = 1<<n;
										/* for 8' it is bit n-1 (bit 0 if n-1<0) */
										/* for 4' it is bit n-2 (bit 0 if n-2<0) */
										/* for 2' it is bit n-3 (bit 0 if n-3<0) */
					n = (n>0)? n-1: 0;
					m_voi[ch].TG_out8  = 1<<n;

					n = (n>0)? n-1: 0;
					m_voi[ch].TG_out4  = 1<<n;

					n = (n>0)? n-1: 0;
					m_voi[ch].TG_out2  = 1<<n;
				}
				m_voi[ch].mode = 0;     /* tone mode */
				m_voi[ch].eg_sect = 0;  /* Key On */
			}
		}
		else
		{
			if ( !m_voi[ch].eg_arm )    /* arm = 0 */
				m_voi[ch].eg_sect = 2;  /* Key Off -> go to release */
			else                            /* arm = 1 */
				m_voi[ch].eg_sect = 1;  /* Key Off -> go to decay */
		}
	}
	else
	{
		int i;
		switch(offset)
		{
		case 0x08:  /* group1 attack */
			for (i=0; i<4; i++)
				m_voi[i].ar_rate   = m_ar_tbl[data&0x7] * m_external_capacity[i];
			break;

		case 0x09:  /* group2 attack */
			for (i=0; i<4; i++)
				m_voi[i+4].ar_rate = m_ar_tbl[data&0x7] * m_external_capacity[i+4];
			break;

		case 0x0a:  /* group1 decay */
			for (i=0; i<4; i++) {
				m_voi[i].dr_rate   = m_dr_tbl[data&0xf] * m_external_capacity[i];
      }
			break;

		case 0x0b:  /* group2 decay */
			for (i=0; i<4; i++)
				m_voi[i+4].dr_rate = m_dr_tbl[data&0xf] * m_external_capacity[i+4];
			break;

		case 0x0c:  /* group1 control */

			/*if (m_control1 != data)
			    logerror("msm5232: control1 ctrl=%x OE=%x\n", data&0xf0, data&0x0f);*/

			/*if (data & 0x10)
			    popmessage("msm5232: control1 ctrl=%2x\n", data);*/

			m_control1 = data;

			for (i=0; i<4; i++)
			{
				if ( (data&0x10) && (m_voi[i].eg_sect == 1) )
					m_voi[i].eg_sect = 0;
				m_voi[i].eg_arm = data&0x10;
        m_voi[i].eg_ext = !(data&0x20);
			}

			m_EN_out16[0] = (data&1) ? ~0:0;
			m_EN_out8[0]  = (data&2) ? ~0:0;
			m_EN_out4[0]  = (data&4) ? ~0:0;
			m_EN_out2[0]  = (data&8) ? ~0:0;

			break;

		case 0x0d:  /* group2 control */

			/*if (m_control2 != data)
			    logerror("msm5232: control2 ctrl=%x OE=%x\n", data&0xf0, data&0x0f);*/

			/*if (data & 0x10)
			    popmessage("msm5232: control2 ctrl=%2x\n", data);*/

			m_control2 = data;
			gate_update();

			for (i=0; i<4; i++)
			{
				if ( (data&0x10) && (m_voi[i+4].eg_sect == 1) )
					m_voi[i+4].eg_sect = 0;
				m_voi[i+4].eg_arm = data&0x10;
        m_voi[i+4].eg_ext = !(data&0x20);
			}

			m_EN_out16[1] = (data&1) ? ~0:0;
			m_EN_out8[1]  = (data&2) ? ~0:0;
			m_EN_out4[1]  = (data&4) ? ~0:0;
			m_EN_out2[1]  = (data&8) ? ~0:0;

			break;
		}
	}
}



#define VMIN    0
#define VMAX    32768


void msm5232_device::EG_voices_advance()
{
	VOICE *voi = &m_voi[0];
	int samplerate = m_rate;
	int i;

	i = 8;
	do
	{
    if (voi->eg_ext) {
      voi->egvol=m_external_input[8-i]*2048.0;
    } else {
      switch(voi->eg_sect)
      {
      case 0: /* attack */

        /* capacitor charge */
        if (voi->eg < VMAX)
        {
          voi->counter -= (int)((VMAX - voi->eg) / voi->ar_rate);
          if ( voi->counter <= 0 )
          {
            int n = -voi->counter / samplerate + 1;
            voi->counter += n * samplerate;
            if ( (voi->eg += n) > VMAX )
              voi->eg = VMAX;
          }
        }

        /* when ARM=0, EG switches to decay as soon as cap is charged to VT (EG inversion voltage; about 80% of MAX) */
        if (!voi->eg_arm)
        {
          if(voi->eg >= VMAX * 80/100 )
          {
            voi->eg_sect = 1;
          }
        }
        else
        /* ARM=1 */
        {
          /* when ARM=1, EG stays at maximum until key off */
        }

        voi->egvol = voi->eg / 16; /*32768/16 = 2048 max*/

        break;

      case 1: /* decay */

        /* capacitor discharge */
        if (voi->eg > VMIN)
        {
          voi->counter -= (int)((voi->eg - VMIN) / voi->dr_rate);
          if ( voi->counter <= 0 )
          {
            int n = -voi->counter / samplerate + 1;
            voi->counter += n * samplerate;
            if ( (voi->eg -= n) < VMIN )
              voi->eg = VMIN;
          }
        }
        else /* voi->eg <= VMIN */
        {
          voi->eg_sect =-1;
        }

        voi->egvol = voi->eg / 16; /*32768/16 = 2048 max*/

        break;

      case 2: /* release */

        /* capacitor discharge */
        if (voi->eg > VMIN)
        {
          voi->counter -= (int)((voi->eg - VMIN) / voi->rr_rate);
          if ( voi->counter <= 0 )
          {
            int n = -voi->counter / samplerate + 1;
            voi->counter += n * samplerate;
            if ( (voi->eg -= n) < VMIN )
              voi->eg = VMIN;
          }
        }
        else /* voi->eg <= VMIN */
        {
          voi->eg_sect =-1;
        }

        voi->egvol = voi->eg / 16; /*32768/16 = 2048 max*/

        break;

      default:
        break;
      }
    }

		voi++;
		i--;
	} while (i>0);

}

static int o2,o4,o8,o16,solo8,solo16;

void msm5232_device::TG_group_advance(int groupidx)
{
	VOICE *voi = &m_voi[groupidx*4];
	int i;

	o2 = o4 = o8 = o16 = solo8 = solo16 = 0;

	i=4;
	do
	{
		int out2, out4, out8, out16;

		out2 = out4 = out8 = out16 = 0;

		if (voi->mode==0)   /* generate square tone */
		{
			int left = 1<<STEP_SH;
			do
			{
				int nextevent = left;

				if (voi->TG_cnt&voi->TG_out16)  out16+=voi->TG_count;
				if (voi->TG_cnt&voi->TG_out8)   out8 +=voi->TG_count;
				if (voi->TG_cnt&voi->TG_out4)   out4 +=voi->TG_count;
				if (voi->TG_cnt&voi->TG_out2)   out2 +=voi->TG_count;

				voi->TG_count -= nextevent;

				while (voi->TG_count <= 0)
				{
					voi->TG_count += voi->TG_count_period;
					voi->TG_cnt++;
					if (voi->TG_cnt&voi->TG_out16)  out16+=voi->TG_count_period;
					if (voi->TG_cnt&voi->TG_out8 )  out8 +=voi->TG_count_period;
					if (voi->TG_cnt&voi->TG_out4 )  out4 +=voi->TG_count_period;
					if (voi->TG_cnt&voi->TG_out2 )  out2 +=voi->TG_count_period;

					if (voi->TG_count > 0)
						break;

					voi->TG_count += voi->TG_count_period;
					voi->TG_cnt++;
					if (voi->TG_cnt&voi->TG_out16)  out16+=voi->TG_count_period;
					if (voi->TG_cnt&voi->TG_out8 )  out8 +=voi->TG_count_period;
					if (voi->TG_cnt&voi->TG_out4 )  out4 +=voi->TG_count_period;
					if (voi->TG_cnt&voi->TG_out2 )  out2 +=voi->TG_count_period;
				}
				if (voi->TG_cnt&voi->TG_out16)  out16-=voi->TG_count;
				if (voi->TG_cnt&voi->TG_out8 )  out8 -=voi->TG_count;
				if (voi->TG_cnt&voi->TG_out4 )  out4 -=voi->TG_count;
				if (voi->TG_cnt&voi->TG_out2 )  out2 -=voi->TG_count;

				left -=nextevent;

			}while (left>0);
		}
		else    /* generate noise */
		{
			if (m_noise_clocks&8)   out16+=(1<<STEP_SH);
			if (m_noise_clocks&4)   out8 +=(1<<STEP_SH);
			if (m_noise_clocks&2)   out4 +=(1<<STEP_SH);
			if (m_noise_clocks&1)   out2 +=(1<<STEP_SH);
		}

		/* calculate signed output */
    if (!voi->mute) {
      o16 += vo16[groupidx*4+(4-i)] = ( (out16-(1<<(STEP_SH-1))) * voi->egvol) >> STEP_SH;
      o8  += vo8 [groupidx*4+(4-i)] = ( (out8 -(1<<(STEP_SH-1))) * voi->egvol) >> STEP_SH;
      o4  += vo4 [groupidx*4+(4-i)] = ( (out4 -(1<<(STEP_SH-1))) * voi->egvol) >> STEP_SH;
      o2  += vo2 [groupidx*4+(4-i)] = ( (out2 -(1<<(STEP_SH-1))) * voi->egvol) >> STEP_SH;

      if (i == 1 && groupidx == 1)
      {
        solo16 += ( (out16-(1<<(STEP_SH-1))) << 11) >> STEP_SH;
        solo8  += ( (out8 -(1<<(STEP_SH-1))) << 11) >> STEP_SH;
      }
    }

		voi++;
		i--;
	}while (i>0);

	/* cut off disabled output lines */
	o16 &= m_EN_out16[groupidx];
	o8  &= m_EN_out8 [groupidx];
	o4  &= m_EN_out4 [groupidx];
	o2  &= m_EN_out2 [groupidx];
}


/* macro saves feet data to mono file */
#ifdef SAVE_SEPARATE_CHANNELS
	#define SAVE_SINGLE_CHANNEL(j,val) \
	{   signed int pom= val; \
	if (pom > 32767) pom = 32767; else if (pom < -32768) pom = -32768; \
	fputc((unsigned short)pom&0xff,sample[j]); \
	fputc(((unsigned short)pom>>8)&0xff,sample[j]);  }
#else
	#define SAVE_SINGLE_CHANNEL(j,val)
#endif

/* first macro saves all 8 feet outputs to mixed (mono) file */
/* second macro saves one group into left and the other in right channel */
#if 1   /*MONO*/
	#ifdef SAVE_SAMPLE
		#define SAVE_ALL_CHANNELS \
		{   signed int pom = buf1[i] + buf2[i]; \
		fputc((unsigned short)pom&0xff,sample[8]); \
		fputc(((unsigned short)pom>>8)&0xff,sample[8]); \
		}
	#else
		#define SAVE_ALL_CHANNELS
	#endif
#else   /*STEREO*/
	#ifdef SAVE_SAMPLE
		#define SAVE_ALL_CHANNELS \
		{   signed int pom = buf1[i]; \
		fputc((unsigned short)pom&0xff,sample[8]); \
		fputc(((unsigned short)pom>>8)&0xff,sample[8]); \
		pom = buf2[i]; \
		fputc((unsigned short)pom&0xff,sample[8]); \
		fputc(((unsigned short)pom>>8)&0xff,sample[8]); \
		}
	#else
		#define SAVE_ALL_CHANNELS
	#endif
#endif


/* MAME Interface */
void msm5232_device::device_post_load()
{
	init_tables();
}

void msm5232_device::set_clock(int clock)
{
	if (m_chip_clock != clock)
	{
		m_chip_clock = clock;
		m_rate = clock/CLOCK_RATE_DIVIDER;
		init_tables();
	}
}


//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

void msm5232_device::sound_stream_update(short* outputs)
{
	auto &buf1 = outputs[0];
	auto &buf2 = outputs[1];
	auto &buf3 = outputs[2];
	auto &buf4 = outputs[3];
	auto &buf5 = outputs[4];
	auto &buf6 = outputs[5];
	auto &buf7 = outputs[6];
	auto &buf8 = outputs[7];
	auto &bufsolo1 = outputs[8];
	auto &bufsolo2 = outputs[9];
	auto &bufnoise = outputs[10];

  /* calculate all voices' envelopes */
  EG_voices_advance();

  TG_group_advance(0);   /* calculate tones group 1 */
  buf1=o2;
  buf2=o4;
  buf3=o8;
  buf4=o16;

  TG_group_advance(1);   /* calculate tones group 2 */
  buf5=o2;
  buf6=o4;
  buf7=o8;
  buf8=o16;

  bufsolo1=solo8;
  bufsolo2=solo16;

  /* update noise generator */
  {
    int cnt = (m_noise_cnt+=m_noise_step) >> STEP_SH;
    m_noise_cnt &= ((1<<STEP_SH)-1);
    while (cnt > 0)
    {
      int tmp = m_noise_rng & (1<<16);        /* store current level */

      if (m_noise_rng&1)
        m_noise_rng ^= 0x24000;
      m_noise_rng>>=1;

      if ( (m_noise_rng & (1<<16)) != tmp )   /* level change detect */
        m_noise_clocks++;

      cnt--;
    }
  }

  bufnoise=(m_noise_rng & (1<<16)) ? 32767 : 0;
}

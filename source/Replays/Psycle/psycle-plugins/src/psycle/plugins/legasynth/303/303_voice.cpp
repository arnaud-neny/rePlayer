/////////////////////////////////////////////////////////////////////
// Dmitry "Sartorius" Kulikov LegaSynth plugins for PSYCLE

/***************************************************************************
                             -------------------
    begin                : Wed Jul 10 2002
    copyright            : (C) 2002 by red
    email                : red@server
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "303_voice.h"
#include <psycle/helpers/math/erase_all_nans_infinities_and_denormals.hpp>
#include <cmath>
namespace psycle::plugins::legasynth {

TB303_Voice::TB303_Voice():Voice(){
	used_data=NULL;
	base_freq=44100.f;

	vco_inc=vco_k=vcf_rescoeff=0;
	
	vcf_cutoff = 0; vcf_envmod = 0;
	vcf_reso = 0; vcf_envdecay = 0;
	vcf_envpos = ENV_UPDATE_INTERVAL;
	vcf_a = vcf_b = vcf_d1 = vcf_d2 = 0;
	vcf_c0 = 0; vcf_e0 = 0; vcf_e1 = 0;

	vca_mode = 0;  vca_a = vcf_b = vcf_c =0;
	vca_attack = 1.0f - 0.94406088f;
	vca_decay = 0.99897516f;	// 50.92ms at 44.1Khz
	vca_a0 = 0.5;
}

TB303_Voice::~TB303_Voice(){

}

void TB303_Voice::set_data(Default_Data& p_data) {
	set_default_data(p_data);
	used_data=reinterpret_cast<Data*>(&p_data);
}

bool TB303_Voice::set_controller(char p_ctrl,char p_param) {

	switch (p_ctrl) {
		case 80: {
			used_data->envelope_cutoff=(int)p_param << 9;
		} break;
		case 81: {
			used_data->resonance=(int)p_param << 9;
		} break;
		case 82: {
			used_data->envelope_mod=(int)p_param << 9;
		} break;	                
		default: {
			return false; //not found!
		}								
	}
	return true; //recognized
}
void TB303_Voice::set_velocity(int velocity) {
	this->velocity = velocity;
}
void TB303_Voice::mix_modifier_call() {
	//remember, this function is called UPDATES_PER_SECOND times per second

	//todo: this is a destructive change over the original envelope_cutoff. Not sure
	// if it is desiderable that a new note has the original or the changed value.
	used_data->envelope_cutoff+=used_data->cutoff_sweep<<4;
	
	if (used_data->envelope_cutoff<0)
		used_data->envelope_cutoff=0;
	if (used_data->envelope_cutoff>0xFFFF)
		used_data->envelope_cutoff=0xFFFF;

    params.cutoff=used_data->envelope_cutoff;
	//todo: simplify the formula?
    params.cutoff+=used_data->cutoff_lfo_depth*8
				*std::sin( (float)get_update_count()*2.f*M_PI
						*(float)(used_data->cutoff_lfo_speed+1)*(0.1f/256.f));

	if (params.cutoff<0) params.cutoff=0;
	if (params.cutoff>0xFFFF) params.cutoff=0xFFFF;


	params.resonance=used_data->resonance;
	recalculate_filters();        
	
}

void TB303_Voice::mix_internal(int p_amount,float *p_where_l,float *p_where_r) {
    float mix_volume_left;
    float mix_volume_right;
    float mix_volume;
    float val;
    
    mix_volume=get_total_volume() * REQUESTED_MAX_SAMPLE_VALUE;
	mix_volume_right=get_total_pan() * mix_volume;
	mix_volume_left=(1.0f - get_total_pan()) * mix_volume;

   	while (p_amount--) { //ok, the amount of SAMPLES to mix
		float w,k;

		// update vcf
   		if(vcf_envpos >= ENV_UPDATE_INTERVAL) {
   			w = vcf_e0 + vcf_c0;
			k = std::exp(-w/vcf_rescoeff);
   			vcf_c0 *= vcf_envdecay;
			psycle::helpers::math::erase_all_nans_infinities_and_denormals(vcf_c0);
   			vcf_a = 2.0*std::cos(2.0*w) * k;
   			vcf_b = -k*k;
   			vcf_c = 1.0 - vcf_a - vcf_b;
   			vcf_envpos = 0;
   		}

   		// compute sample
   		val=vcf_a*vcf_d1 + vcf_b*vcf_d2 + vcf_c*vco_k*vca_a;
		psycle::helpers::math::erase_all_nans_infinities_and_denormals(val);
		
		*p_where_l += val * mix_volume_left; //mix to channel buffer, left sample
  		p_where_l++;
		*p_where_r +=  val * mix_volume_right; //mix to channel buffer, right sample
  		p_where_r++;

   		vcf_d2=vcf_d1;
   		vcf_envpos++;
   		vcf_d1=val;
   		// update vco
   		vco_k += vco_inc;
   		if(vco_k > 0.5) vco_k -= 1.0;

   		// update vca
   		if(!vca_mode) vca_a+=(vca_a0-vca_a)*vca_attack;
   		else if(vca_mode == 1) {
   			vca_a *= vca_decay;
   			if(vca_a < (1.0/65536.0)) { vca_a = 0; vca_mode = 2; }
   		}
	}
}

void TB303_Voice::set_note_internal(char p_note,char p_velocity) {
	//there is not really need to take the parameters unless you _really_ want them
    //the best is to just use get_fnote() and get_total_volume() later on
	vca_mode = 0;
	recalculate_filters();
	vcf_c0 = vcf_e1;
	vcf_envpos = ENV_UPDATE_INTERVAL;
}

void TB303_Voice::set_note_off_internal(char p_velocity){
	vca_mode=1;
}

void TB303_Voice::set_mix_frequency_internal(int p_mixfreq){ //stuff we need to recalculate when having the frequency

	base_freq=p_mixfreq;
	
	recalculate_filters();
	recalculate_pitch_internal();

}

Voice::Status TB303_Voice::get_status(){ //obvious function

	return (vca_mode==2) ? DEAD : ATTACKING; //i think i should change this to SUSTAINING.. ohwell..
	//dont return DEAD unless the voice finished, when you return DEAD, the voice is _killed_.	
}

void TB303_Voice::recalculate_pitch_internal() {

	float basenote;
	float frequency;

    basenote=get_fnote(); //fnote is the note, but given as FLOAT (0-127 with decimals), so it allows perfect control for vibratos/sweeps/etc
	basenote+=used_data->coarse; //apply coarse tunning
	basenote+=(float)used_data->fine*0.01;

	frequency=8.1757989156*std::pow(2.0,basenote*(1.0/12.0)); //note -> frequency conversion  8.1757989156 is the midi C-0
	vco_inc=frequency/base_freq;
}

void TB303_Voice::recalculate_filters() {

	vcf_cutoff = (float)params.cutoff*0.000015259021896696421759365224689097f; /*/65535.0;*/

	vcf_reso = (float)params.resonance*0.000015259021896696421759365224689097f; /*/65535.0;*/
	vcf_rescoeff = std::exp(-1.20 + 3.455*vcf_reso);

	vcf_envmod = (float)used_data->envelope_mod*0.000015259021896696421759365224689097f; /*/65535.0;*/

	float d = (float)used_data->envelope_decay*0.000015259021896696421759365224689097f; /*/65535.0;*/
	d = 0.05 + (2.45*d);
	d*=base_freq;
	vcf_envdecay = std::pow(0.1, 1.0/d * ENV_UPDATE_INTERVAL);
	float elapsed = 1;
	if (update_count > 0) elapsed = std::pow(vcf_envdecay, update_count);

	vcf_e1 = std::exp(6.109 + 1.5876*vcf_envmod + 2.1553*vcf_cutoff - 1.2*(1.0-vcf_reso));
	vcf_e0 = std::exp(5.613 - 0.8*vcf_envmod + 2.1553*vcf_cutoff - 0.7696*(1.0-vcf_reso));
	vcf_e0*=M_PI/base_freq;
	vcf_e1*=M_PI/base_freq;
	vcf_e1 -= vcf_e0;
	vcf_e1 *= elapsed;
	psycle::helpers::math::erase_all_nans_infinities_and_denormals(vcf_e1);
	vcf_envpos = ENV_UPDATE_INTERVAL;
}

}
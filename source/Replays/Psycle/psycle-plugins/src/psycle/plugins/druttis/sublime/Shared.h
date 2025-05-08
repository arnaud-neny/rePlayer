//////////////////////////////////////////////////////////////////////
//
//				Shared.h
//
//				druttis@darkface.pp.se
//
//////////////////////////////////////////////////////////////////////
#pragma once
#include <psycle/plugin_interface.hpp>
namespace psycle::plugins::druttis::sublime {
using psycle::plugin_interface::MAX_BUFFER_LENGTH;

//////////////////////////////////////////////////////////////////////
//
//				Various buffers
//	The purpose of this class is to keep a reserved amount of memory
// for buffer operations during the Voice work call. They can be
// shared for the same Machine instance.
// 
//////////////////////////////////////////////////////////////////////
class SharedBuffers {
public:
//				Monophonics
	float				m_out[MAX_BUFFER_LENGTH];
	float				m_mod_out[MAX_BUFFER_LENGTH];
//				Polyphonics
	float				m_osc1_fm_out[MAX_BUFFER_LENGTH];
	float				m_osc2_fm_out[MAX_BUFFER_LENGTH];
	float				m_lfo1_out[MAX_BUFFER_LENGTH];
	float				m_lfo2_out[MAX_BUFFER_LENGTH];
	float				m_osc1_phase_out[MAX_BUFFER_LENGTH];
	float				m_osc2_phase_out[MAX_BUFFER_LENGTH];
	float				m_out1[MAX_BUFFER_LENGTH];
	float				m_out2[MAX_BUFFER_LENGTH];
};
}
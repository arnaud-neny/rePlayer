//============================================================================
//
//				CEnvelope.cpp
//
//============================================================================
#include "CEnvelope.h"

namespace psycle::plugins::druttis {

//============================================================================
//
//				CEnvelope
//
//============================================================================
CEnvelope::CEnvelope()
: m_attack(0)
, m_decay(0)
, m_sustain(1.0f)
, m_release(0)
, m_stage(ENVELOPE_DONE)
, m_value(0.0f)
, m_coeff(0.0f)
, m_samples(0)
{
}

CEnvelope::~CEnvelope()
{
}

bool CEnvelope::IsFinished()
{
	return (m_stage == ENVELOPE_DONE);
}

void CEnvelope::Begin()
{
	if (m_attack > 0) {
		m_stage = ENVELOPE_ATTACK;
		m_value = 0.0f;
		m_coeff = 1.0f / m_attack;
		m_samples = m_attack;
	} else if (m_decay > 0) {
		m_stage = ENVELOPE_DECAY;
		m_value = 1.0f;
		m_coeff = (m_sustain - 1.0f) / m_decay;
		m_samples = m_decay;
	} else if (m_sustain > 0.0f) {
		m_stage = ENVELOPE_SUSTAIN;
		m_value = m_sustain;
		m_coeff = 0.0f;
		m_samples = 64;
	} else {
		m_stage = ENVELOPE_DONE;
		m_value = 0.0f;
		m_coeff = 0.0f;
		m_samples = 64;
	}
}

void CEnvelope::Stop()
{
	if (m_stage != ENVELOPE_DONE) {
		if (m_release > 0) {
			m_stage = ENVELOPE_RELEASE;
			m_coeff = - m_value / m_release;
			m_samples = m_release;
		} else {
			m_stage = ENVELOPE_DONE;
			m_value = 0.0f;
			m_coeff = 0.0f;
			m_samples = 64;
		}
	}
}

void CEnvelope::Kill()
{
	m_stage = ENVELOPE_DONE;
	m_value = 0.0f;
	m_coeff = 0.0f;
	m_samples = 64;
}

int CEnvelope::GetAttack()
{
	return m_attack;
}

bool CEnvelope::SetAttack(int attack)
{
	if (attack < 0.0f)
		return false;
	m_attack = attack;
	return true;
}

int CEnvelope::GetDecay()
{
	return m_decay;
}

bool CEnvelope::SetDecay(int decay)
{
	if (decay < 0.0f)
		return false;
	m_decay = decay;
	return true;
}

float CEnvelope::GetSustain()
{
	return m_sustain;
}

bool CEnvelope::SetSustain(float sustain)
{
	if ((sustain < 0.0f) || (sustain > 1.0f))
		return false;
	m_sustain = sustain;
	return true;
}

int CEnvelope::GetRelease()
{
	return m_release;
}

bool CEnvelope::SetRelease(int release)
{
	if (m_release < 0.0f)
		return false;
	m_release = release;
	return true;
}
}
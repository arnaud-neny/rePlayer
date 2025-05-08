//============================================================================
//
//				CEnvelope.h
//
//============================================================================
#pragma once

namespace psycle::plugins::druttis {

//============================================================================
//				Defines
//============================================================================
typedef enum {
	ENVELOPE_DONE = 0,
	ENVELOPE_ATTACK,
	ENVELOPE_DECAY,
	ENVELOPE_SUSTAIN,
	ENVELOPE_RELEASE
} env_t;
//============================================================================
//				Class
//============================================================================
class CEnvelope
{
private:
	int				m_attack;
	int				m_decay;
	float			m_sustain;
	int				m_release;
	env_t			m_stage;
	float			m_value;
	float			m_coeff;
	int				m_samples;
public:
	CEnvelope();
	~CEnvelope();
	virtual bool IsFinished();
	void Begin();
	void Stop();
	void Kill();
	int GetAttack();
	bool SetAttack(int attack);
	int GetDecay();
	bool SetDecay(int decay);
	float GetSustain();
	bool SetSustain(float sustain);
	int GetRelease();
	bool SetRelease(int release);
	inline float Next()
	{
		if(m_samples-- > 0) {
			m_value += m_coeff;
		}
		else {
			switch (m_stage)
			{
			case ENVELOPE_ATTACK:
				if (m_decay > 0.0f) {
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
				break;

			case ENVELOPE_DECAY:
				if (m_sustain > 0.0f) {
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
				break;

			case ENVELOPE_SUSTAIN:
				m_value = m_sustain;
				m_samples = 64;
				break;

			case ENVELOPE_RELEASE:
				m_stage = ENVELOPE_DONE;
				m_value = 0.0f;
				m_coeff = 0.0f;
				m_samples = 64;
				break;

			default:
				m_stage = ENVELOPE_DONE;
				m_value = 0.0f;
				m_coeff = 0.0f;
				m_samples = 64;
				break;
			}
		}
		return m_value;
	}
};
}
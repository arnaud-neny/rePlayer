// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#ifndef PSYCLE__CORE__PLAYER_TIME_INFO__INCLUDED
#define PSYCLE__CORE__PLAYER_TIME_INFO__INCLUDED
#pragma once

#include <psycle/core/detail/project.hpp>

namespace psycle { namespace core {

/// class for play time information
class PSYCLE__CORE__DECL PlayerTimeInfo {
	public:
			PlayerTimeInfo();
			~PlayerTimeInfo();

			/// the sequence position currently being played in beats
			void setPlayBeatPos( double pos );
			double playBeatPos() const { return playBeatPos_; }

			/// Start and end cycle position.
			void setCycleStartPos( double pos );
			double cycleStartPos() const { return cycleStartPos_; }

			void setCycleEndPos( double pos );
			double cycleEndPos() const { return cycleEndPos_; }

			/// Returns if cycle is enabled. Not checking with equal for rounding errors.
			bool cycleEnabled() const { return cycleEndPos_ - cycleStartPos_ > 0.1f; }

			/// the current master sample position
			void setSamplePos( int pos );
			int samplePos() const { return samplePos_; }

			/// for legacy sincronization and duration of some commands.
			void setTicksSpeed( int ticks , bool isticks);
			int ticksSpeed() const { return ticks_; }
			bool isTicks() const { return isTicks_; }

			/// the current beats per minute at which to play the song.
			/// can be changed from the song itself using commands.
			void setBpm( double bpm );
			double bpm() const { return bpm_; }

			void setSampleRate( int rate );
			int sampleRate( ) const { return sampleRate_; }

			void setInputLatency( int lat );
			int inputLatency( ) const { return inputLatency_; }
			void setOutputLatency( int lat );
			int outputLatency( ) const { return outputLatency_; }


			float samplesPerBeat() const { return samplesPerBeat_; }
			float samplesPerTick() const { return samplesPerTick_; }
			
	private:
			double playBeatPos_;
			int samplePos_;
			int ticks_;
			bool isTicks_;
			double bpm_;
			int sampleRate_;
			float samplesPerBeat_;
			float samplesPerTick_;
			int inputLatency_;
			int outputLatency_;
			double cycleStartPos_;
			double cycleEndPos_;

			void recalcSPB();
			void recalcSPT();
};

}}

#endif

// This source is free software ; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation ; either version 2, or (at your option) any later version.
// copyright 2007-2009 members of the psycle project http://psycle.sourceforge.net

#include <psycle/core/detail/project.private.hpp>
#include "playertimeinfo.h"

#include <cassert>

namespace psycle { namespace core {

PlayerTimeInfo::PlayerTimeInfo( )
:
	playBeatPos_(0.0),
	samplePos_(0),
	ticks_(4),
	isTicks_(true),
	bpm_(125.0),
	sampleRate_(44100)
{
	recalcSPB();
	recalcSPT();
}

PlayerTimeInfo::~ PlayerTimeInfo( )
{
}

void PlayerTimeInfo::setCycleStartPos( double pos )
{
	assert(pos >= 0);
	cycleStartPos_ = pos;
}

void PlayerTimeInfo::setCycleEndPos( double pos )
{
	assert(pos >= 0);
	cycleEndPos_ = pos;
}

void PlayerTimeInfo::setPlayBeatPos( double pos )
{
	assert(pos >= 0);
	playBeatPos_ = pos;
}

void PlayerTimeInfo::setSamplePos( int pos )
{
	assert(pos >= 0);
	samplePos_ = pos;
}

void PlayerTimeInfo::setTicksSpeed( int ticks, bool isticks )
{
	assert(ticks > 0);
	ticks_ = ticks;
	isTicks_ = isticks;
	recalcSPT();
}

void PlayerTimeInfo::setBpm( double bpm )
{
	assert(bpm > 0);
	bpm_ = bpm;
	recalcSPB();
	recalcSPT();
}

void PlayerTimeInfo::setSampleRate( int rate )
{
	assert(rate > 0);
	sampleRate_ = rate;
	recalcSPB();
	recalcSPT();
}

void PlayerTimeInfo::recalcSPB( )
{
	samplesPerBeat_ = (sampleRate_*60) / bpm_;
	assert(samplesPerBeat_ > 0);
}

void PlayerTimeInfo::recalcSPT( )
{
	if ( isTicks_ )
	{
		samplesPerTick_ = samplesPerBeat() / ticks_;
	}
	else
	{
		samplesPerTick_ =  samplesPerBeat() * ticks_ / 24.0f;
	}
	assert(samplesPerTick_ > 0);
}

}}
